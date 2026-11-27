/* The design canvas fitted into a window (core/src/view.c). */
#include "arcade/view.h"
#include "arcade_test.h"

static void test_exact_16_9_windows_need_no_margin(void) {
    ArcView v;
    arc_view_fit(&v, 1280, 720);
    CHECK_NEAR(v.scale, 1.0, 0.0001);
    CHECK_NEAR(v.off_x, 0.0, 0.0001);
    CHECK_NEAR(v.off_y, 0.0, 0.0001);
    CHECK_NEAR(v.left, 0.0, 0.0001);
    CHECK_NEAR(v.right, ARC_SCREEN_W, 0.0001);
    CHECK_NEAR(v.top, 0.0, 0.0001);
    CHECK_NEAR(v.bottom, ARC_SCREEN_H, 0.0001);

    arc_view_fit(&v, 1920, 1080); /* 1080p: 1.5x */
    CHECK_NEAR(v.scale, 1.5, 0.0001);
    CHECK_NEAR(v.right, ARC_SCREEN_W, 0.0001);
    arc_view_fit(&v, 3840, 2160); /* 4K: 3x */
    CHECK_NEAR(v.scale, 3.0, 0.0001);
    CHECK_NEAR(v.bottom, ARC_SCREEN_H, 0.0001);
}

static void test_wider_window_gets_side_margins(void) {
    ArcView v;
    arc_view_fit(&v, 2560, 1080); /* 21:9: height decides, 320 px free on each side */
    CHECK_NEAR(v.scale, 1.5, 0.0001);
    CHECK_NEAR(v.off_x, 320.0, 0.0001);
    CHECK_NEAR(v.off_y, 0.0, 0.0001);
    CHECK_NEAR(v.left, -320.0 / 1.5, 0.001);
    CHECK_NEAR(v.right, (2560.0 - 320.0) / 1.5, 0.001);
    CHECK_NEAR(v.top, 0.0, 0.0001);
    CHECK_NEAR(v.bottom, ARC_SCREEN_H, 0.0001);
}

static void test_taller_window_gets_top_and_bottom_margins(void) {
    ArcView v;
    arc_view_fit(&v, 960, 1080); /* a half-screen tile: width decides */
    CHECK_NEAR(v.scale, 0.75, 0.0001);
    CHECK_NEAR(v.off_x, 0.0, 0.0001);
    CHECK_NEAR(v.off_y, 270.0, 0.0001);
    CHECK_NEAR(v.top, -360.0, 0.001);
    CHECK_NEAR(v.bottom, 1080.0 / 0.75 - 360.0, 0.001);
    CHECK_NEAR(v.left, 0.0, 0.0001);
    CHECK_NEAR(v.right, ARC_SCREEN_W, 0.0001);
}

static void test_the_corner_is_a_whole_pixel(void) {
    ArcView v;
    arc_view_fit(&v, 1001, 999); /* odd sizes: the centring is not a whole number of pixels */
    CHECK_NEAR(v.off_x, floorf(v.off_x), 0.0001);
    CHECK_NEAR(v.off_y, floorf(v.off_y), 0.0001);
}

static void test_the_whole_canvas_is_always_visible(void) {
    /* for any window size: the canvas fits, and touches the window on at least one axis */
    for (int w = 100; w <= 4200; w += 137) {
        for (int h = 100; h <= 2400; h += 113) {
            ArcView v;
            arc_view_fit(&v, w, h);
            CHECK(v.scale > 0.0f);
            CHECK(v.left <= 0.0f && v.top <= 0.0f);
            CHECK(v.right >= (float)ARC_SCREEN_W - 0.01f);
            CHECK(v.bottom >= (float)ARC_SCREEN_H - 0.01f);
            CHECK(v.off_x >= 0.0f && v.off_y >= 0.0f);
            CHECK(v.off_x + (float)ARC_SCREEN_W * v.scale <= (float)w + 0.01f);
            CHECK(v.off_y + (float)ARC_SCREEN_H * v.scale <= (float)h + 0.01f);
        }
    }
}

static void test_mouse_mapping_round_trips(void) {
    ArcView v;
    arc_view_fit(&v, 1920, 1200);                 /* 16:10: 60 px bars above and below */
    Vec2 d = arc_view_to_design(&v, 0.0f, 60.0f); /* the canvas's top-left corner on screen */
    CHECK_NEAR(d.x, 0.0, 0.001);
    CHECK_NEAR(d.y, 0.0, 0.001);
    d = arc_view_to_design(&v, 960.0f, 600.0f); /* the middle of the window is the middle of the canvas */
    CHECK_NEAR(d.x, ARC_SCREEN_W / 2.0, 0.001);
    CHECK_NEAR(d.y, ARC_SCREEN_H / 2.0, 0.001);
    d = arc_view_to_design(&v, 10.0f, 10.0f); /* a click in the top bar maps above the canvas */
    CHECK(d.y < 0.0f);
    Vec2 w = arc_view_to_window(&v, 333.0f, 222.0f);
    d = arc_view_to_design(&v, w.x, w.y);
    CHECK_NEAR(d.x, 333.0, 0.001);
    CHECK_NEAR(d.y, 222.0, 0.001);
}

static void test_scissor_rectangle_covers_the_design_rectangle(void) {
    ArcView v;
    int x, y, w, h;
    arc_view_fit(&v, 1920, 1080);
    arc_view_window_rect(&v, 400.0f, 36.0f, 480.0f, 684.0f, &x, &y, &w, &h); /* the portrait playfield */
    CHECK_EQ_INT(x, 600);
    CHECK_EQ_INT(y, 54);
    CHECK_EQ_INT(w, 720);
    CHECK_EQ_INT(h, 1026);
    arc_view_fit(&v, 1366, 768); /* a fractional scale rounds outwards, never inwards */
    arc_view_window_rect(&v, 400.0f, 36.0f, 480.0f, 684.0f, &x, &y, &w, &h);
    Vec2 a = arc_view_to_window(&v, 400.0f, 36.0f);
    Vec2 b = arc_view_to_window(&v, 880.0f, 720.0f);
    CHECK((float)x <= a.x && (float)y <= a.y);
    CHECK((float)(x + w) >= b.x && (float)(y + h) >= b.y);
    CHECK(w > 0 && h > 0);
}

static void test_degenerate_windows_do_not_divide_by_zero(void) {
    ArcView v;
    arc_view_fit(&v, 0, 0); /* a minimised window can report this */
    CHECK(v.scale > 0.0f);
    CHECK_EQ_INT(v.win_w, 1);
    CHECK_EQ_INT(v.win_h, 1);
    arc_view_fit(&v, -5, 300);
    CHECK(v.scale > 0.0f);
}

static void test_font_atlas_scale_is_quantised(void) {
    CHECK_NEAR(arc_view_text_scale(1.0f), 1.0, 0.0001);
    CHECK_NEAR(arc_view_text_scale(1.5f), 1.5, 0.0001);
    CHECK_NEAR(arc_view_text_scale(2.0f), 2.0, 0.0001);
    CHECK_NEAR(arc_view_text_scale(3.0f), 3.0, 0.0001);
    CHECK_NEAR(arc_view_text_scale(1.0667f), 1.125, 0.0001); /* 1366x768 -> 9/8 */
    CHECK_NEAR(arc_view_text_scale(1.04f), 1.0, 0.0001);     /* a few pixels of drag: same atlases */
    CHECK_NEAR(arc_view_text_scale(0.2f), 0.5, 0.0001);      /* never tiny */
    /* the error against the real scale stays within 1/16 */
    for (float s = 0.5f; s < 6.0f; s += 0.013f)
        CHECK_NEAR(arc_view_text_scale(s), s, 0.0626);
}

int main(void) {
    RUN(test_exact_16_9_windows_need_no_margin);
    RUN(test_wider_window_gets_side_margins);
    RUN(test_taller_window_gets_top_and_bottom_margins);
    RUN(test_the_corner_is_a_whole_pixel);
    RUN(test_the_whole_canvas_is_always_visible);
    RUN(test_mouse_mapping_round_trips);
    RUN(test_scissor_rectangle_covers_the_design_rectangle);
    RUN(test_degenerate_windows_do_not_divide_by_zero);
    RUN(test_font_atlas_scale_is_quantised);
    return TEST_SUMMARY();
}
