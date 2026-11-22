/* The stroke font: metrics, that every letter draws inside its advance, case folding, blanks, the halo and
 * the outline ring. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_font.h"

#define W 400
#define H 200

static SwcCanvas *fresh(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    swc_cv_fill_rgb(cv, 0xff0000);
    return cv;
}

static int red(const SwcCanvas *cv, int x, int y) {
    return swc_cv_pixels(cv)[((size_t)y * (size_t)swc_cv_width(cv) + (size_t)x) * 4];
}

static double ink(const SwcCanvas *cv) {
    double sum = 0;
    for (int y = 0; y < swc_cv_height(cv); y++)
        for (int x = 0; x < swc_cv_width(cv); x++)
            sum += red(cv, x, y) / 255.0;
    return sum;
}

/* Bounding box of the pixels with some ink; returns false when the canvas is empty. */
static bool bounds(const SwcCanvas *cv, int *x0, int *y0, int *x1, int *y1) {
    bool any = false;
    *x0 = W;
    *y0 = H;
    *x1 = -1;
    *y1 = -1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (red(cv, x, y) > 8) {
                any = true;
                if (x < *x0)
                    *x0 = x;
                if (x > *x1)
                    *x1 = x;
                if (y < *y0)
                    *y0 = y;
                if (y > *y1)
                    *y1 = y;
            }
    return any;
}

static void test_metrics_follow_the_size_and_the_spacing(void) {
    SwcFontStyle st = {100, 900, false, false, 0};
    CHECK_NEAR(swc_font_advance(&st, 'I'), 30.0, 1e-9);
    CHECK_NEAR(swc_font_advance(&st, ' '), 28.0, 1e-9);
    CHECK_NEAR(swc_font_advance(&st, 'W'), 100.0, 1e-9);
    st.size = 50;
    CHECK_NEAR(swc_font_advance(&st, 'W'), 50.0, 1e-9);
    st.spacing = 3;
    CHECK_NEAR(swc_font_measure(&st, "IW"), 15.0 + 50.0 + 6.0, 1e-9);
    CHECK_NEAR(swc_font_measure(&st, ""), 0.0, 1e-9);
}

static void test_a_monospaced_face_has_one_advance_and_lighter_stems(void) {
    SwcFontStyle st = {40, 700, false, true, 0};
    CHECK_NEAR(swc_font_advance(&st, 'I'), 24.0, 1e-9);
    CHECK_NEAR(swc_font_advance(&st, 'W'), 24.0, 1e-9);
    CHECK_NEAR(swc_font_advance(&st, '7'), 24.0, 1e-9);
    SwcFontStyle prop = {40, 700, false, false, 0};
    CHECK(swc_font_stem(&st) < swc_font_stem(&prop));
}

static void test_heavier_weights_have_wider_stems(void) {
    SwcFontStyle a = {100, 700, false, false, 0}, b = {100, 800, false, false, 0},
                 c = {100, 900, false, false, 0};
    CHECK(swc_font_stem(&a) < swc_font_stem(&b));
    CHECK(swc_font_stem(&b) < swc_font_stem(&c));
    CHECK_NEAR(swc_font_stem(&c), 19.5, 1e-9);
}

static void test_every_character_draws_ink_and_stays_near_its_advance(void) {
    static const char SET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%/.:-";
    SwcFontStyle st = {80, 900, false, false, 0};
    for (const char *p = SET; *p; p++) {
        SwcCanvas *cv = fresh();
        char s[2] = {*p, 0};
        swc_font_draw(cv, &st, s, 100, 120, 0, false);
        int x0, y0, x1, y1;
        CHECK(bounds(cv, &x0, &y0, &x1, &y1));
        double adv = swc_font_advance(&st, *p);
        /* a letter never strays more than a little past its own advance, nor above or far below the line */
        CHECK(x0 >= 100 - 0.08 * st.size);
        CHECK(x1 <= 100 + adv + 0.08 * st.size);
        CHECK(y0 >= 120 - SWC_FONT_CAP_HEIGHT * st.size - 3);
        CHECK(y1 <= 120 + 0.2 * st.size);
        swc_cv_destroy(cv);
    }
}

static void test_capitals_stand_on_the_baseline_with_the_cap_height(void) {
    SwcFontStyle st = {100, 900, false, false, 0};
    SwcCanvas *cv = fresh();
    swc_font_draw(cv, &st, "H", 50, 150, 0, false);
    int x0, y0, x1, y1;
    CHECK(bounds(cv, &x0, &y0, &x1, &y1));
    CHECK(abs(y1 - 149) <= 1);
    CHECK(abs((150 - y0) - 72) <= 2);
    swc_cv_destroy(cv);
}

static void test_lower_case_draws_as_upper_case_and_unknown_characters_are_blank(void) {
    SwcFontStyle st = {60, 800, false, false, 2};
    SwcCanvas *a = fresh(), *b = fresh(), *c = fresh();
    swc_font_draw(a, &st, "Kiro 88", 10, 100, 0, false);
    swc_font_draw(b, &st, "KIRO 88", 10, 100, 0, false);
    CHECK(memcmp(swc_cv_pixels(a), swc_cv_pixels(b), (size_t)W * H * 4) == 0);
    swc_font_draw(c, &st, "@", 10, 100, 0, false);
    CHECK(ink(c) == 0.0);
    CHECK_NEAR(swc_font_advance(&st, '@'), 36.0, 1e-9);
    swc_cv_destroy(a);
    swc_cv_destroy(b);
    swc_cv_destroy(c);
}

static void test_italic_leans_the_top_to_the_right(void) {
    SwcFontStyle st = {100, 900, false, false, 0};
    SwcFontStyle it = {100, 900, true, false, 0};
    SwcCanvas *a = fresh(), *b = fresh();
    swc_font_draw(a, &st, "I", 100, 150, 0, false);
    swc_font_draw(b, &it, "I", 100, 150, 0, false);
    int n = 0;
    double sa = 0, sb = 0;
    for (int y = 80; y < 90; y++)
        for (int x = 0; x < W; x++) {
            sa += red(a, x, y) * x;
            sb += red(b, x, y) * x;
            n += red(a, x, y);
        }
    CHECK(n > 0);
    /* near the cap line the italic stem sits to the right of the upright one */
    CHECK(sb / (double)n > sa / (double)n + 8.0);
    CHECK(ink(a) > 0 && fabs(ink(a) - ink(b)) / ink(a) < 0.03);
    swc_cv_destroy(a);
    swc_cv_destroy(b);
}

static void test_a_halo_is_wider_and_a_ring_leaves_the_letter_alone(void) {
    SwcFontStyle st = {100, 900, false, false, 0};
    SwcCanvas *plain = fresh(), *halo = fresh(), *ring = fresh();
    swc_font_draw(plain, &st, "O", 100, 150, 0, false);
    swc_font_draw(halo, &st, "O", 100, 150, 8, false);
    CHECK(ink(halo) > ink(plain) * 1.15);
    /* the ring is painted first onto black, the letter after: nothing of the letter's inside changes */
    swc_font_draw(ring, &st, "O", 100, 150, 8, true);
    int hx = 100 + (int)(0.39 * 100), hy = 150 - 36; /* the middle of the O's counter */
    CHECK_EQ_INT(red(ring, hx, hy), 0);
    CHECK(ink(ring) > 0);
    CHECK(ink(ring) < ink(halo));
    swc_cv_destroy(plain);
    swc_cv_destroy(halo);
    swc_cv_destroy(ring);
}

static void test_the_same_text_gives_the_same_pixels(void) {
    SwcFontStyle st = {70, 900, true, false, 1.5};
    SwcCanvas *a = fresh(), *b = fresh();
    swc_font_draw(a, &st, "NOVA COLA 88.8", 5, 90, 0, false);
    swc_font_draw(b, &st, "NOVA COLA 88.8", 5, 90, 0, false);
    CHECK(memcmp(swc_cv_pixels(a), swc_cv_pixels(b), (size_t)W * H * 4) == 0);
    swc_cv_destroy(a);
    swc_cv_destroy(b);
}

int main(void) {
    RUN(test_metrics_follow_the_size_and_the_spacing);
    RUN(test_a_monospaced_face_has_one_advance_and_lighter_stems);
    RUN(test_heavier_weights_have_wider_stems);
    RUN(test_every_character_draws_ink_and_stays_near_its_advance);
    RUN(test_capitals_stand_on_the_baseline_with_the_cap_height);
    RUN(test_lower_case_draws_as_upper_case_and_unknown_characters_are_blank);
    RUN(test_italic_leans_the_top_to_the_right);
    RUN(test_a_halo_is_wider_and_a_ring_leaves_the_letter_alone);
    RUN(test_the_same_text_gives_the_same_pixels);
    return TEST_SUMMARY();
}
