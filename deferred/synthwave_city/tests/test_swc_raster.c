/* The software rasteriser: exact coverage of simple shapes, strokes with caps and joins, gradients, clips,
 * the fill rules, transforms and the text helpers. Areas are checked as the sum of one channel over the
 * canvas. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_raster.h"

#define W 100
#define H 100

static int red(const SwcCanvas *cv, int x, int y) {
    return swc_cv_pixels(cv)[((size_t)y * (size_t)swc_cv_width(cv) + (size_t)x) * 4];
}

static double red_area(const SwcCanvas *cv) {
    double sum = 0;
    for (int y = 0; y < swc_cv_height(cv); y++)
        for (int x = 0; x < swc_cv_width(cv); x++)
            sum += (double)red(cv, x, y) / 255.0;
    return sum;
}

static SwcCanvas *fresh(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    swc_cv_fill_rgb(cv, 0xff0000);
    swc_cv_stroke_rgb(cv, 0xff0000);
    return cv;
}

static void test_a_new_canvas_is_opaque_black(void) {
    SwcCanvas *cv = swc_cv_create(8, 4);
    CHECK(cv != NULL);
    CHECK_EQ_INT(swc_cv_width(cv), 8);
    CHECK_EQ_INT(swc_cv_height(cv), 4);
    const uint8_t *p = swc_cv_pixels(cv);
    for (int i = 0; i < 8 * 4; i++) {
        CHECK_EQ_INT(p[i * 4], 0);
        CHECK_EQ_INT(p[i * 4 + 1], 0);
        CHECK_EQ_INT(p[i * 4 + 2], 0);
        CHECK_EQ_INT(p[i * 4 + 3], 255);
    }
    swc_cv_destroy(cv);
    CHECK(swc_cv_create(0, 5) == NULL);
}

static void test_a_rectangle_on_whole_pixels_is_exact(void) {
    SwcCanvas *cv = fresh();
    swc_cv_fill_rect(cv, 10, 20, 30, 15);
    CHECK_NEAR(red_area(cv), 450.0, 1e-6);
    CHECK_EQ_INT(red(cv, 10, 20), 255);
    CHECK_EQ_INT(red(cv, 39, 34), 255);
    CHECK_EQ_INT(red(cv, 9, 20), 0);
    CHECK_EQ_INT(red(cv, 40, 20), 0);
    CHECK_EQ_INT(red(cv, 10, 35), 0);
    swc_cv_destroy(cv);
}

static void test_a_rectangle_on_half_pixels_covers_half(void) {
    SwcCanvas *cv = fresh();
    swc_cv_fill_rect(cv, 10.5, 20.25, 10, 10);
    CHECK_NEAR(red_area(cv), 100.0, 0.05);
    CHECK_NEAR(red(cv, 10, 25), 128, 1.5);
    CHECK_NEAR(red(cv, 20, 25), 128, 1.5);
    CHECK_NEAR(red(cv, 15, 20), 255 * 0.75, 2.5);
    swc_cv_destroy(cv);
}

static void test_a_circle_has_the_right_area(void) {
    SwcCanvas *cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 50, 50, 30, 0, 2 * 3.14159265358979323846, false);
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_NEAR(red_area(cv), 3.14159265358979323846 * 900.0, 3.14159265358979323846 * 900.0 * 0.004);
    CHECK_EQ_INT(red(cv, 50, 50), 255);
    CHECK_EQ_INT(red(cv, 50, 10), 0);
    swc_cv_destroy(cv);
}

static void test_a_stroke_is_as_wide_as_the_line_width(void) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 3);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 40.5);
    swc_cv_line_to(cv, 60, 40.5);
    swc_cv_stroke(cv);
    CHECK_NEAR(red_area(cv), 150.0, 0.2);
    CHECK_EQ_INT(red(cv, 30, 39), 255);
    CHECK_EQ_INT(red(cv, 30, 40), 255);
    CHECK_EQ_INT(red(cv, 30, 41), 255);
    CHECK_EQ_INT(red(cv, 30, 38), 0);
    CHECK_EQ_INT(red(cv, 30, 42), 0);
    swc_cv_destroy(cv);
}

static double line_area(SwcLineCap cap) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 10);
    swc_cv_line_cap(cv, cap);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 20, 50);
    swc_cv_line_to(cv, 70, 50);
    swc_cv_stroke(cv);
    double a = red_area(cv);
    swc_cv_destroy(cv);
    return a;
}

static void test_caps_add_their_own_area(void) {
    double pi = 3.14159265358979323846;
    CHECK_NEAR(line_area(SWC_CAP_BUTT), 500.0, 0.3);
    CHECK_NEAR(line_area(SWC_CAP_SQUARE), 600.0, 0.3);
    CHECK_NEAR(line_area(SWC_CAP_ROUND), 500.0 + pi * 25.0, 1.5);
}

static double corner_area(SwcLineJoin join) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 10);
    swc_cv_line_join(cv, join);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 20, 70);
    swc_cv_line_to(cv, 20, 30);
    swc_cv_line_to(cv, 60, 30);
    swc_cv_stroke(cv);
    double a = red_area(cv);
    swc_cv_destroy(cv);
    return a;
}

static void test_joins_fill_the_outside_of_a_corner_as_asked(void) {
    double pi = 3.14159265358979323846;
    double bevel = corner_area(SWC_JOIN_BEVEL);
    double miter = corner_area(SWC_JOIN_MITER);
    double round = corner_area(SWC_JOIN_ROUND);
    CHECK_NEAR(miter, 40 * 10 + 40 * 10 - 25 + 25, 1.0); /* two arms and a full corner square */
    CHECK_NEAR(miter - bevel, 12.5, 0.6);                /* the miter wedge is half the 5 x 5 corner square */
    CHECK_NEAR(round - bevel, pi * 25.0 / 4.0 - 12.5, 0.8);
}

static void test_a_stroke_closed_by_a_path_has_no_cap_at_the_seam(void) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 6);
    swc_cv_begin_path(cv);
    swc_cv_rect(cv, 20, 20, 50, 40);
    swc_cv_stroke(cv);
    /* a frame 6 wide: outer 56 x 46 minus inner 44 x 34 */
    CHECK_NEAR(red_area(cv), 56.0 * 46.0 - 44.0 * 34.0, 1.0);
    CHECK_EQ_INT(red(cv, 20, 20), 255);
    CHECK_EQ_INT(red(cv, 45, 40), 0);
    swc_cv_destroy(cv);
}

static void test_a_gradient_runs_between_its_points_and_clamps(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    SwcGradient g = swc_gradient2(0, 20, 0, 80, 0x000000, 0xff0000);
    swc_cv_fill_gradient(cv, &g);
    swc_cv_fill_rect(cv, 0, 0, W, H);
    CHECK_EQ_INT(red(cv, 50, 5), 0);
    CHECK_NEAR(red(cv, 50, 50), 127.5, 4.5); /* pixel centre at 50.5: 0.508 of the way */
    CHECK_NEAR(red(cv, 50, 35), 0.25 * 255.0, 4.5);
    CHECK_EQ_INT(red(cv, 50, 95), 255);
    swc_cv_destroy(cv);
}

static void test_a_gradient_follows_the_transform(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    swc_cv_translate(cv, 0, 50);
    SwcGradient g = swc_gradient2(0, -30, 0, 30, 0x000000, 0xff0000);
    swc_cv_fill_gradient(cv, &g);
    swc_cv_fill_rect(cv, 0, -50, W, 100);
    CHECK_NEAR(red(cv, 10, 20), 0, 3);
    CHECK_NEAR(red(cv, 10, 50), 127.5, 4.5);
    CHECK_NEAR(red(cv, 10, 80), 255, 3);
    swc_cv_destroy(cv);
}

static void test_a_gradient_with_several_stops(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    SwcGradient g;
    memset(&g, 0, sizeof(g));
    g.x0 = 0;
    g.x1 = 100;
    g.stop_count = 3;
    g.stop_at[0] = 0.0;
    g.stop_at[1] = 0.5;
    g.stop_at[2] = 1.0;
    g.stop_rgb[0] = 0x000000;
    g.stop_rgb[1] = 0xff0000;
    g.stop_rgb[2] = 0x000000;
    swc_cv_fill_gradient(cv, &g);
    swc_cv_fill_rect(cv, 0, 0, W, H);
    CHECK_NEAR(red(cv, 50, 5), 255, 4);
    CHECK_NEAR(red(cv, 25, 5), 127, 4);
    CHECK_NEAR(red(cv, 75, 5), 127, 4);
    swc_cv_destroy(cv);
}

static void test_a_rectangle_clip_limits_drawing_and_restore_lifts_it(void) {
    SwcCanvas *cv = fresh();
    swc_cv_save(cv);
    swc_cv_begin_path(cv);
    swc_cv_rect(cv, 20, 20, 30, 30);
    swc_cv_clip(cv, SWC_NONZERO);
    swc_cv_fill_rect(cv, 0, 0, W, H);
    CHECK_NEAR(red_area(cv), 900.0, 1e-6);
    CHECK_EQ_INT(red(cv, 19, 30), 0);
    CHECK_EQ_INT(red(cv, 20, 30), 255);
    swc_cv_restore(cv);
    swc_cv_fill_rect(cv, 60, 60, 10, 10);
    CHECK_NEAR(red_area(cv), 1000.0, 1e-6);
    swc_cv_release_clips(cv);
    swc_cv_destroy(cv);
}

static void test_a_circle_clip_and_a_nested_clip_intersect(void) {
    double pi = 3.14159265358979323846;
    SwcCanvas *cv = fresh();
    swc_cv_save(cv);
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 50, 50, 30, 0, 2 * pi, false);
    swc_cv_clip(cv, SWC_NONZERO);
    swc_cv_fill_rect(cv, 0, 0, W, H);
    CHECK_NEAR(red_area(cv), pi * 900.0, pi * 900.0 * 0.004);
    /* a second clip: the left half of the box */
    swc_cv_begin_path(cv);
    swc_cv_rect(cv, 0, 0, 50, H);
    swc_cv_clip(cv, SWC_NONZERO);
    swc_cv_fill_rgb(cv, 0x00ff00);
    swc_cv_fill_rect(cv, 0, 0, W, H);
    const uint8_t *p = swc_cv_pixels(cv);
    CHECK_EQ_INT(p[((size_t)50 * W + 40) * 4 + 1], 255); /* green on the left */
    CHECK_EQ_INT(p[((size_t)50 * W + 60) * 4 + 1], 0);   /* untouched on the right */
    CHECK_EQ_INT(p[((size_t)50 * W + 60) * 4], 255);
    CHECK_EQ_INT(p[((size_t)5 * W + 5) * 4 + 1], 0); /* outside the circle */
    swc_cv_restore(cv);
    swc_cv_release_clips(cv);
    swc_cv_destroy(cv);
}

static void test_arcs_turn_the_way_a_browser_turns_them(void) {
    double pi = 3.14159265358979323846;
    /* a counter-clockwise arc from 0 to a full turn is a whole circle, as in a browser */
    SwcCanvas *cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 50, 50, 20, 0, 2 * pi, true);
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_NEAR(red_area(cv), pi * 400.0, pi * 400.0 * 0.004);
    swc_cv_destroy(cv);
    /* clockwise from the left round the top to the right is the upper half */
    cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 50, 50, 20, pi, 2 * pi, false);
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_NEAR(red_area(cv), pi * 400.0 / 2.0, pi * 400.0 * 0.004);
    CHECK_EQ_INT(red(cv, 50, 40), 255);
    CHECK_EQ_INT(red(cv, 50, 60), 0);
    swc_cv_destroy(cv);
    /* clockwise from the right to the left (pi back to 0) goes through the bottom */
    cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 50, 50, 20, 0, pi, false);
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_EQ_INT(red(cv, 50, 60), 255);
    CHECK_EQ_INT(red(cv, 50, 40), 0);
    swc_cv_destroy(cv);
}

static void test_the_even_odd_rule_leaves_a_hole(void) {
    SwcCanvas *cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_rect(cv, 10, 10, 60, 60);
    swc_cv_rect(cv, 30, 30, 20, 20);
    swc_cv_fill(cv, SWC_EVENODD);
    CHECK_NEAR(red_area(cv), 3600.0 - 400.0, 1e-6);
    CHECK_EQ_INT(red(cv, 40, 40), 0);
    swc_cv_destroy(cv);
    cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_rect(cv, 10, 10, 60, 60);
    swc_cv_rect(cv, 30, 30, 20, 20); /* same winding: the non-zero rule fills it */
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_NEAR(red_area(cv), 3600.0, 1e-6);
    swc_cv_destroy(cv);
}

static void test_transforms_apply_in_order(void) {
    SwcCanvas *cv = fresh();
    swc_cv_translate(cv, 10, 20);
    swc_cv_scale(cv, 2, 3);
    swc_cv_fill_rect(cv, 1, 1, 5, 4);
    /* x from 12 to 22, y from 23 to 35 */
    CHECK_NEAR(red_area(cv), 120.0, 1e-6);
    CHECK_EQ_INT(red(cv, 12, 23), 255);
    CHECK_EQ_INT(red(cv, 21, 34), 255);
    CHECK_EQ_INT(red(cv, 22, 34), 0);
    swc_cv_destroy(cv);
    cv = fresh();
    swc_cv_translate(cv, 50, 50);
    swc_cv_rotate(cv, 3.14159265358979323846 / 2.0);
    swc_cv_fill_rect(cv, 0, 0, 20, 10); /* turned a quarter: 10 wide and 20 tall, to the lower left */
    CHECK_NEAR(red_area(cv), 200.0, 1e-3);
    CHECK_EQ_INT(red(cv, 45, 60), 255);
    CHECK_EQ_INT(red(cv, 55, 60), 0);
    swc_cv_destroy(cv);
}

static void test_rounded_rectangles_via_arc_to(void) {
    double pi = 3.14159265358979323846;
    SwcCanvas *cv = fresh();
    double x = 20, y = 20, w = 60, h = 40, r = 10;
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, x + r, y);
    swc_cv_arc_to(cv, x + w, y, x + w, y + h, r);
    swc_cv_arc_to(cv, x + w, y + h, x, y + h, r);
    swc_cv_arc_to(cv, x, y + h, x, y, r);
    swc_cv_arc_to(cv, x, y, x + w, y, r);
    swc_cv_close_path(cv);
    swc_cv_fill(cv, SWC_NONZERO);
    CHECK_NEAR(red_area(cv), w * h - (4.0 - pi) * r * r, 1.0);
    CHECK_EQ_INT(red(cv, 20, 20), 0); /* the corner is cut */
    CHECK_EQ_INT(red(cv, 50, 20), 255);
    swc_cv_destroy(cv);
}

static void test_curves_are_flattened_close_to_the_true_curve(void) {
    SwcCanvas *cv = fresh();
    /* a quadratic from (10,80) to (90,80) with control (50,0): the apex is at y = 40 */
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 80);
    swc_cv_quad_to(cv, 50, 0, 90, 80);
    swc_cv_close_path(cv);
    swc_cv_fill(cv, SWC_NONZERO);
    /* the area between a parabola and its chord is 2/3 of the bounding triangle-rectangle: 2/3 * 80 * 40 */
    CHECK_NEAR(red_area(cv), 2.0 / 3.0 * 80.0 * 40.0, 2.0);
    swc_cv_destroy(cv);
    cv = fresh();
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 50);
    swc_cv_bezier_to(cv, 10, 10, 90, 10, 90, 50);
    swc_cv_close_path(cv);
    swc_cv_fill(cv, SWC_NONZERO);
    /* the cubic's apex is at t = 0.5: y = 0.125*50 + 0.375*10 + 0.375*10 + 0.125*50 = 20 */
    CHECK(red(cv, 50, 22) == 255);
    CHECK(red(cv, 50, 18) == 0);
    swc_cv_destroy(cv);
}

static void test_a_stroke_with_a_hole_is_an_outline(void) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 10);
    swc_cv_stroke_hole(cv, 6);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 50);
    swc_cv_line_to(cv, 90, 50);
    swc_cv_stroke(cv);
    CHECK_EQ_INT(red(cv, 50, 50), 0);
    CHECK_EQ_INT(red(cv, 50, 49), 0);
    CHECK_EQ_INT(red(cv, 50, 46), 255);
    CHECK_EQ_INT(red(cv, 50, 53), 255);
    CHECK_NEAR(red_area(cv), 2.0 * 2.0 * 80.0, 0.5);
    swc_cv_destroy(cv);
}

static void test_a_stroke_box_cuts_the_stroke(void) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 10);
    swc_cv_stroke_box(cv, true, 0, 40, W, 60);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 50, 10);
    swc_cv_line_to(cv, 50, 90);
    swc_cv_stroke(cv);
    CHECK_NEAR(red_area(cv), 200.0, 0.3);
    CHECK_EQ_INT(red(cv, 50, 39), 0);
    CHECK_EQ_INT(red(cv, 50, 40), 255);
    CHECK_EQ_INT(red(cv, 50, 59), 255);
    CHECK_EQ_INT(red(cv, 50, 60), 0);
    swc_cv_destroy(cv);
}

static void test_a_stroke_box_cuts_in_x_too_and_a_skew_leans_the_picture(void) {
    SwcCanvas *cv = fresh();
    swc_cv_line_width(cv, 10);
    swc_cv_stroke_box(cv, true, 40, 0, 60, H);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 50);
    swc_cv_line_to(cv, 90, 50);
    swc_cv_stroke(cv);
    CHECK_NEAR(red_area(cv), 200.0, 0.3);
    CHECK_EQ_INT(red(cv, 39, 50), 0);
    CHECK_EQ_INT(red(cv, 40, 50), 255);
    CHECK_EQ_INT(red(cv, 59, 50), 255);
    CHECK_EQ_INT(red(cv, 60, 50), 0);
    swc_cv_destroy(cv);
    cv = fresh();
    swc_cv_translate(cv, 20, 80);
    swc_cv_skew(cv, 0.5);
    swc_cv_fill_rect(cv, 0, -40, 10, 40); /* a stem from y 80 up to 40 leans right by 20 at the top */
    CHECK_NEAR(red_area(cv), 400.0, 1e-6);
    CHECK_EQ_INT(red(cv, 25, 79), 255);
    CHECK_EQ_INT(red(cv, 45, 41), 255);
    CHECK_EQ_INT(red(cv, 25, 41), 0);
    swc_cv_destroy(cv);
}

static void test_stroking_can_use_the_fill_style(void) {
    SwcCanvas *cv = swc_cv_create(W, H);
    swc_cv_fill_rgb(cv, 0x00ff00);
    swc_cv_stroke_rgb(cv, 0xff0000);
    swc_cv_line_width(cv, 6);
    swc_cv_stroke_with_fill_style(cv, true);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, 10, 50);
    swc_cv_line_to(cv, 90, 50);
    swc_cv_stroke(cv);
    CHECK_EQ_INT(swc_cv_pixels(cv)[((size_t)50 * W + 50) * 4 + 1], 255);
    CHECK_EQ_INT(red(cv, 50, 50), 0);
    swc_cv_stroke_with_fill_style(cv, false);
    swc_cv_stroke(cv);
    CHECK_EQ_INT(red(cv, 50, 50), 255);
    swc_cv_destroy(cv);
}

static void test_drawing_outside_the_canvas_is_harmless(void) {
    SwcCanvas *cv = fresh();
    swc_cv_fill_rect(cv, -50, -50, 300, 300);
    CHECK_NEAR(red_area(cv), (double)W * H, 1e-6);
    swc_cv_begin_path(cv);
    swc_cv_arc(cv, 500, 500, 30, 0, 6.3, false);
    swc_cv_fill(cv, SWC_NONZERO);
    swc_cv_line_width(cv, 8);
    swc_cv_begin_path(cv);
    swc_cv_move_to(cv, -100, -100);
    swc_cv_line_to(cv, 200, 200);
    swc_cv_stroke(cv);
    CHECK_NEAR(red_area(cv), (double)W * H, 1e-6);
    swc_cv_destroy(cv);
}

static void test_the_same_drawing_gives_the_same_pixels(void) {
    uint8_t first[W * H * 4];
    for (int run = 0; run < 2; run++) {
        SwcCanvas *cv = fresh();
        swc_cv_line_width(cv, 7);
        swc_cv_line_join(cv, SWC_JOIN_ROUND);
        swc_cv_begin_path(cv);
        swc_cv_move_to(cv, 10, 90);
        swc_cv_quad_to(cv, 40, 0, 90, 70);
        swc_cv_stroke(cv);
        if (run == 0)
            memcpy(first, swc_cv_pixels(cv), sizeof(first));
        else
            CHECK(memcmp(first, swc_cv_pixels(cv), sizeof(first)) == 0);
        swc_cv_destroy(cv);
    }
}

int main(void) {
    RUN(test_a_new_canvas_is_opaque_black);
    RUN(test_a_rectangle_on_whole_pixels_is_exact);
    RUN(test_a_rectangle_on_half_pixels_covers_half);
    RUN(test_a_circle_has_the_right_area);
    RUN(test_a_stroke_is_as_wide_as_the_line_width);
    RUN(test_caps_add_their_own_area);
    RUN(test_joins_fill_the_outside_of_a_corner_as_asked);
    RUN(test_a_stroke_closed_by_a_path_has_no_cap_at_the_seam);
    RUN(test_a_gradient_runs_between_its_points_and_clamps);
    RUN(test_a_gradient_follows_the_transform);
    RUN(test_a_gradient_with_several_stops);
    RUN(test_a_rectangle_clip_limits_drawing_and_restore_lifts_it);
    RUN(test_a_circle_clip_and_a_nested_clip_intersect);
    RUN(test_arcs_turn_the_way_a_browser_turns_them);
    RUN(test_the_even_odd_rule_leaves_a_hole);
    RUN(test_transforms_apply_in_order);
    RUN(test_rounded_rectangles_via_arc_to);
    RUN(test_curves_are_flattened_close_to_the_true_curve);
    RUN(test_a_stroke_with_a_hole_is_an_outline);
    RUN(test_a_stroke_box_cuts_the_stroke);
    RUN(test_a_stroke_box_cuts_in_x_too_and_a_skew_leans_the_picture);
    RUN(test_stroking_can_use_the_fill_style);
    RUN(test_drawing_outside_the_canvas_is_harmless);
    RUN(test_the_same_drawing_gives_the_same_pixels);
    return TEST_SUMMARY();
}
