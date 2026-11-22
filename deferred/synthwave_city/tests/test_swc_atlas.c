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

int main(void) {
    RUN(test_a_new_atlas_is_black_and_2048_square);
    RUN(test_every_cell_draws_a_plausible_amount_of_ink);
    RUN(test_the_padding_of_every_cell_stays_black);
    RUN(test_cells_are_independent_of_the_order_they_are_drawn_in);
    RUN(test_one_cell_touches_only_its_own_pixels);
    RUN(test_the_seed_changes_the_glyph_runs_but_the_ads_keep_their_layout);
    RUN(test_the_whole_atlas_draws_quickly);
    return TEST_SUMMARY();
}
