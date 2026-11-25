/* The hologram ad atlas: every cell draws, cells are independent of the order they are drawn in, the padding
 * stays black so mip levels do not bleed between ads, and the whole atlas is reproducible. */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arcade_test.h"
#include "swc_atlas.h"

#define SEED 2086230173u /* swc_layer_seed(1984, "holograms") */

static size_t bytes(const SwcCanvas *cv) {
    return (size_t)swc_cv_width(cv) * (size_t)swc_cv_height(cv) * 4;
}

static const uint8_t *px(const SwcCanvas *cv, int x, int y) {
    return swc_cv_pixels(cv) + ((size_t)y * (size_t)swc_cv_width(cv) + (size_t)x) * 4;
}

/* Fraction of the cell's pixels that carry any light. */
static double cover(const SwcCanvas *cv, const SwcAtlasCell *c) {
    long lit = 0;
    for (int y = c->y; y < c->y + c->h; y++)
        for (int x = c->x; x < c->x + c->w; x++) {
            const uint8_t *p = px(cv, x, y);
            if (p[0] + p[1] + p[2] > 24)
                lit++;
        }
    return (double)lit / ((double)c->w * c->h);
}

static void test_a_new_atlas_is_black_and_2048_square(void) {
    SwcCanvas *cv = swc_atlas_create();
    CHECK(cv != NULL);
    CHECK_EQ_INT(swc_cv_width(cv), SWC_ATLAS_SIZE);
    CHECK_EQ_INT(swc_cv_height(cv), SWC_ATLAS_SIZE);
    long lit = 0;
    for (size_t i = 0; i < bytes(cv); i += 4)
        lit += swc_cv_pixels(cv)[i] | swc_cv_pixels(cv)[i + 1] | swc_cv_pixels(cv)[i + 2];
    CHECK_EQ_INT((int)lit, 0);
    swc_cv_destroy(cv);
}

static void test_every_cell_draws_a_plausible_amount_of_ink(void) {
    SwcCanvas *cv = swc_atlas_draw_all(SEED);
    CHECK(cv != NULL);
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int n = swc_atlas_layout(cells);
    CHECK_EQ_INT(n, 28);
    for (int i = 0; i < n; i++) {
        double c = cover(cv, &cells[i]);
        CHECK(c > 0.08);
        CHECK(c < 0.50);
    }
    swc_cv_destroy(cv);
}

static void test_the_padding_of_every_cell_stays_black(void) {
    SwcCanvas *cv = swc_atlas_draw_all(SEED);
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int n = swc_atlas_layout(cells);
    for (int i = 0; i < n; i++) {
        const SwcAtlasCell *c = &cells[i];
        int pad_x = c->kind == SWC_CELL_STRIP ? 0 : SWC_ATLAS_CELL_PAD;
        long lit = 0;
        for (int y = c->y; y < c->y + c->h; y++)
            for (int x = c->x; x < c->x + c->w; x++) {
                bool inside = x >= c->x + pad_x && x < c->x + c->w - pad_x &&
                              y >= c->y + SWC_ATLAS_CELL_PAD && y < c->y + c->h - SWC_ATLAS_CELL_PAD;
                if (!inside) {
                    const uint8_t *p = px(cv, x, y);
                    lit += p[0] | p[1] | p[2];
                }
            }
        CHECK_EQ_INT((int)lit, 0);
    }
    swc_cv_destroy(cv);
}

static void test_cells_are_independent_of_the_order_they_are_drawn_in(void) {
    SwcCanvas *whole = swc_atlas_draw_all(SEED);
    SwcCanvas *back = swc_atlas_create();
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int n = swc_atlas_layout(cells);
    for (int i = n - 1; i >= 0; i--)
        swc_atlas_draw_cell(back, &cells[i], SEED);
    CHECK(memcmp(swc_cv_pixels(whole), swc_cv_pixels(back), bytes(whole)) == 0);
    swc_cv_destroy(whole);
    swc_cv_destroy(back);
}

static void test_one_cell_touches_only_its_own_pixels(void) {
    SwcCanvas *cv = swc_atlas_create();
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    swc_atlas_layout(cells);
    const SwcAtlasCell *c = &cells[17]; /* a tall ad */
    swc_atlas_draw_cell(cv, c, SEED);
    long outside = 0, inside = 0;
    for (int y = 0; y < SWC_ATLAS_SIZE; y++)
        for (int x = 0; x < SWC_ATLAS_SIZE; x++) {
            const uint8_t *p = px(cv, x, y);
            int v = p[0] | p[1] | p[2];
            bool in = x >= c->x && x < c->x + c->w && y >= c->y && y < c->y + c->h;
            if (in)
                inside += v != 0;
            else
                outside += v != 0;
        }
    CHECK(inside > 1000);
    CHECK_EQ_INT((int)outside, 0);
    swc_cv_destroy(cv);
}

static void test_the_seed_changes_the_glyph_runs_but_the_ads_keep_their_layout(void) {
    SwcCanvas *a = swc_atlas_draw_all(SEED);
    SwcCanvas *b = swc_atlas_draw_all(SEED + 1);
    SwcCanvas *a2 = swc_atlas_draw_all(SEED);
    CHECK(memcmp(swc_cv_pixels(a), swc_cv_pixels(a2), bytes(a)) == 0);
    CHECK(memcmp(swc_cv_pixels(a), swc_cv_pixels(b), bytes(a)) != 0);
    /* but only a little differs: the ads are fixed art, only their alien script and bars are random */
    long diff = 0;
    for (size_t i = 0; i < bytes(a); i += 4)
        diff +=
            swc_cv_pixels(a)[i] != swc_cv_pixels(b)[i] || swc_cv_pixels(a)[i + 1] != swc_cv_pixels(b)[i + 1];
    CHECK(diff > 0);
    CHECK((double)diff < 0.06 * SWC_ATLAS_SIZE * SWC_ATLAS_SIZE);
    swc_cv_destroy(a);
    swc_cv_destroy(a2);
    swc_cv_destroy(b);
}

static void test_the_whole_atlas_draws_quickly(void) {
    clock_t t0 = clock();
    SwcCanvas *cv = swc_atlas_draw_all(SEED);
    double s = (double)(clock() - t0) / CLOCKS_PER_SEC;
    CHECK(cv != NULL);
    CHECK(s < 5.0); /* about 0.14 s on a desktop; generous for a slow CI machine or a sanitiser build */
    swc_cv_destroy(cv);
}

/* ---- the work in pieces ---- */

/* Sum of each colour channel over a cell (the three inks of the ads are mostly one channel each). */
static void channel_sums(const SwcCanvas *cv, const SwcAtlasCell *c, long sum[3]) {
    sum[0] = sum[1] = sum[2] = 0;
    for (int y = c->y; y < c->y + c->h; y++)
        for (int x = c->x; x < c->x + c->w; x++) {
            const uint8_t *p = px(cv, x, y);
            sum[0] += p[0];
            sum[1] += p[1];
            sum[2] += p[2];
        }
}

/* Channel sums of the four station ads as the web version draws them at seed 1984 (read from its atlas):
 * the port draws with its own letter shapes, so it has to come within 12 % (or 25000 of a faint channel) of
 * each. */
static const struct {
    int index;
    long sum[3];
} STATION_ADS[4] = {
    {0, {6392632, 98406, 1710129}},   /* OBSIDIAN */
    {2, {2689612, 1739940, 1298590}}, /* VELVET */
    {5, {3884396, 2887216, 2308553}}, /* STARPORT */
    {6, {3990615, 2674757, 1485404}}, /* PLAZA */
};

static void test_the_four_station_ads_carry_the_ink_of_the_web_versions_ads(void) {
    SwcCanvas *cv = swc_atlas_draw_all(SEED);
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int n = swc_atlas_layout(cells);
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (cells[i].kind != SWC_CELL_WIDE)
            continue;
        for (int k = 0; k < 4; k++) {
            if (cells[i].index != STATION_ADS[k].index)
                continue;
            long sum[3];
            channel_sums(cv, &cells[i], sum);
            found++;
            for (int ch = 0; ch < 3; ch++) {
                double want = (double)STATION_ADS[k].sum[ch];
                double room = want * 0.12 > 25000.0 ? want * 0.12 : 25000.0;
                CHECK_NEAR((double)sum[ch], want, room);
            }
        }
    }
    CHECK_EQ_INT(found, 4);
    swc_cv_destroy(cv);
}

static void test_a_job_drawn_cell_by_cell_makes_the_same_atlas(void) {
    SwcAtlasJob job;
    CHECK(swc_atlas_job_start(&job, SEED));
    CHECK_EQ_INT(job.cell_count, SWC_ATLAS_CELL_COUNT);
    int drawn = 0;
    while (swc_atlas_job_next(&job))
        drawn++;
    CHECK_EQ_INT(drawn + 1, SWC_ATLAS_CELL_COUNT); /* the last call draws the last cell and says "done" */
    CHECK(!swc_atlas_job_next(&job));              /* nothing left: no more drawing */
    SwcCanvas *whole = swc_atlas_draw_all(SEED);
    SwcCanvas *piecewise = swc_atlas_job_take(&job);
    CHECK(whole != NULL && piecewise != NULL);
    CHECK(job.cv == NULL);
    CHECK(memcmp(swc_cv_pixels(whole), swc_cv_pixels(piecewise), bytes(whole)) == 0);
    swc_cv_destroy(whole);
    swc_cv_destroy(piecewise);
}

static void test_an_unfinished_job_is_freed_without_a_leak(void) {
    SwcAtlasJob job;
    CHECK(swc_atlas_job_start(&job, SEED));
    swc_atlas_job_next(&job);
    swc_atlas_job_free(&job); /* the sanitiser build would report the canvas */
    CHECK(job.cv == NULL);
    swc_atlas_job_free(&job); /* and twice is fine */
}

static void test_half_floats_have_the_right_bits(void) {
    CHECK_EQ_INT(swc_half_bits(0.0f), 0x0000);
    CHECK_EQ_INT(swc_half_bits(1.0f), 0x3c00);
    CHECK_EQ_INT(swc_half_bits(0.5f), 0x3800);
    CHECK_EQ_INT(swc_half_bits(2.0f), 0x4000);
    CHECK_EQ_INT(swc_half_bits(-2.0f), 0xc000);
    CHECK_EQ_INT(swc_half_bits(65504.0f), 0x7bff);      /* the largest finite half */
    CHECK_EQ_INT(swc_half_bits(1.0e9f), 0x7c00);        /* too big: infinity */
    CHECK_EQ_INT(swc_half_bits(5.9604645e-8f), 0x0001); /* the smallest denormal */
    CHECK_EQ_INT(swc_half_bits(1.0e-12f), 0x0000);      /* too small: zero */
    CHECK_EQ_INT(swc_half_bits(0.333333343f), 0x3555);  /* rounds to nearest */
}

static void test_the_srgb_table_is_monotone_and_hits_its_ends(void) {
    uint16_t lut[256];
    swc_atlas_srgb_lut(lut);
    CHECK_EQ_INT(lut[0], 0x0000);
    CHECK_EQ_INT(lut[255], 0x3c00); /* white is 1.0 */
    for (int i = 1; i < 256; i++)
        CHECK(lut[i] > lut[i - 1]); /* non-negative halves order like their bits */
    CHECK_EQ_INT(lut[1], swc_half_bits((float)(1.0 / 255.0 / 12.92)));
}

static void test_the_texture_rows_are_flipped_and_done_in_any_chunks(void) {
    SwcAtlasJob job;
    CHECK(swc_atlas_job_start(&job, SEED));
    while (swc_atlas_job_next(&job)) {
    }
    SwcCanvas *cv = swc_atlas_job_take(&job);
    int w = swc_cv_width(cv), h = swc_cv_height(cv);
    size_t n = (size_t)w * (size_t)h * 4u;
    uint16_t *whole = malloc(n * sizeof(uint16_t)), *chunked = malloc(n * sizeof(uint16_t));
    CHECK(whole != NULL && chunked != NULL);
    uint16_t lut[256];
    swc_atlas_srgb_lut(lut);
    swc_atlas_half_rows(cv, lut, 0, h, whole);
    memset(chunked, 0xff, n * sizeof(uint16_t));
    for (int y = 0; y < h; y += 192) /* chunks that do not divide the height */
        swc_atlas_half_rows(cv, lut, y, 192, chunked);
    CHECK(memcmp(whole, chunked, n * sizeof(uint16_t)) == 0);
    /* texture row 0 is the canvas's last row: find a lit texel in the atlas and look at its mirror image */
    int found = 0;
    for (int y = 0; y < h && !found; y += 7)
        for (int x = 0; x < w && !found; x += 5) {
            const uint8_t *p = px(cv, x, y);
            if (p[0] > 200) {
                const uint16_t *t = whole + ((size_t)(h - 1 - y) * (size_t)w + (size_t)x) * 4u;
                CHECK_EQ_INT(t[0], lut[p[0]]);
                CHECK_EQ_INT(t[1], lut[p[1]]);
                CHECK_EQ_INT(t[2], lut[p[2]]);
                CHECK_EQ_INT(t[3], 0x3c00);
                found = 1;
            }
        }
    CHECK(found);
    free(whole);
    free(chunked);
    swc_cv_destroy(cv);
}

int main(void) {
    RUN(test_the_four_station_ads_carry_the_ink_of_the_web_versions_ads);
    RUN(test_a_job_drawn_cell_by_cell_makes_the_same_atlas);
    RUN(test_an_unfinished_job_is_freed_without_a_leak);
    RUN(test_half_floats_have_the_right_bits);
    RUN(test_the_srgb_table_is_monotone_and_hits_its_ends);
    RUN(test_the_texture_rows_are_flipped_and_done_in_any_chunks);
    RUN(test_a_new_atlas_is_black_and_2048_square);
    RUN(test_every_cell_draws_a_plausible_amount_of_ink);
    RUN(test_the_padding_of_every_cell_stays_black);
    RUN(test_cells_are_independent_of_the_order_they_are_drawn_in);
    RUN(test_one_cell_touches_only_its_own_pixels);
    RUN(test_the_seed_changes_the_glyph_runs_but_the_ads_keep_their_layout);
    RUN(test_the_whole_atlas_draws_quickly);
    return TEST_SUMMARY();
}
