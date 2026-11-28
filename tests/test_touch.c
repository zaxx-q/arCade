/* Touch controls (core/src/touch.c): which finger does what. */
#include "arcade/touch.h"
#include "arcade_test.h"

static TouchState st;
static TouchOut out;

static void frame(const TouchPoint *pts, int n) {
    touch_update(&st, pts, n, &out);
}

static void setup(void) {
    touch_init(&st); /* the 1280 x 720 canvas */
}

static TouchPoint at(int id, float x, float y) {
    TouchPoint p = {id, x, y};
    return p;
}

static void test_layout_inside_window_and_apart(void) {
    TouchLayout l;
    touch_layout_fit(&l, -160.0f, 0.0f, 1440.0f, 720.0f); /* a wider window than 16:9 */
    const TouchCircle *all[4] = {&l.stick, &l.action, &l.back, &l.keys};
    for (int i = 0; i < 4; i++) {
        CHECK(all[i]->x - all[i]->r >= -160.0f);
        CHECK(all[i]->x + all[i]->r <= 1440.0f);
        CHECK(all[i]->y - all[i]->r >= 0.0f);
        CHECK(all[i]->y + all[i]->r <= 720.0f);
        for (int j = i + 1; j < 4; j++) {
            float dx = all[i]->x - all[j]->x, dy = all[i]->y - all[j]->y, r = all[i]->r + all[j]->r;
            CHECK(dx * dx + dy * dy > r * r); /* no two controls overlap */
        }
    }
    CHECK(l.stick.x < l.action.x); /* stick on the left, action on the right */
    CHECK(l.back.x < l.keys.x);
    CHECK(l.back.y > 36.0f); /* under the top bar of the games */
}

static void test_no_fingers_means_no_input(void) {
    setup();
    frame(NULL, 0);
    CHECK(out.move_x == 0.0f && out.move_y == 0.0f);
    CHECK(!out.action && !out.action_pressed && !out.fire && !out.fire_pressed);
    CHECK(!out.back_pressed && !out.keys_pressed);
    CHECK_NEAR(out.aim_x, 640.0f, 0.01); /* the pointer starts in the middle */
    CHECK_NEAR(out.aim_y, 360.0f, 0.01);
}

static void test_stick_pushes_and_dead_zone(void) {
    setup();
    const TouchCircle s = st.layout.stick;
    TouchPoint p = at(1, s.x + s.r * 0.1f, s.y); /* inside the dead zone */
    frame(&p, 1);
    CHECK(out.move_x == 0.0f && out.move_y == 0.0f);
    CHECK(!out.fire_pressed);  /* the stick finger is not the pointer */
    p = at(1, s.x + s.r, s.y); /* full push right */
    frame(&p, 1);
    CHECK_NEAR(out.move_x, 1.0f, 0.001);
    CHECK_NEAR(out.move_y, 0.0f, 0.001);
    p = at(1, s.x, s.y - s.r * 0.5f); /* half push up */
    frame(&p, 1);
    CHECK_NEAR(out.move_y, -0.5f, 0.001);
    p = at(1, s.x + 5.0f * s.r, s.y); /* far outside the circle: clamped to 1 */
    frame(&p, 1);
    CHECK_NEAR(out.move_x, 1.0f, 0.001);
    frame(NULL, 0); /* lifted */
    CHECK(out.move_x == 0.0f && out.move_y == 0.0f);
}

static void test_stick_keeps_the_finger_that_slid_out(void) {
    setup();
    const TouchCircle s = st.layout.stick;
    TouchPoint p = at(1, s.x, s.y);
    frame(&p, 1);
    p = at(1, s.x + 600.0f, s.y - 300.0f); /* far outside the grab zone */
    frame(&p, 1);
    CHECK(out.move_x > 0.5f && out.move_y < -0.2f); /* still steering */
    CHECK(!out.fire && !out.fire_pressed);          /* and never the pointer */
}

static void test_direction_presses_are_one_shot(void) {
    setup();
    const TouchCircle s = st.layout.stick;
    TouchPoint p = at(1, s.x, s.y);
    frame(&p, 1);
    p = at(1, s.x + s.r * 0.9f, s.y);
    frame(&p, 1);
    CHECK(out.right_pressed && !out.left_pressed);
    frame(&p, 1); /* still held: no second press */
    CHECK(!out.right_pressed);
    p = at(1, s.x - s.r * 0.9f, s.y); /* swing to the left */
    frame(&p, 1);
    CHECK(out.left_pressed && !out.right_pressed);
    p = at(1, s.x, s.y + s.r * 0.9f);
    frame(&p, 1);
    CHECK(out.down_pressed && !out.up_pressed);
    p = at(1, s.x, s.y - s.r * 0.9f);
    frame(&p, 1);
    CHECK(out.up_pressed);
}

static void test_action_button_press_hold_release(void) {
    setup();
    const TouchCircle a = st.layout.action;
    TouchPoint p = at(1, a.x, a.y);
    frame(&p, 1);
    CHECK(out.action && out.action_pressed);
    CHECK(!out.fire_pressed);
    frame(&p, 1);
    CHECK(out.action && !out.action_pressed); /* held, not pressed again */
    frame(NULL, 0);
    CHECK(!out.action && !out.action_pressed);
}

static void test_back_and_keys_act_once_and_are_not_pointers(void) {
    setup();
    TouchPoint p = at(1, st.layout.back.x, st.layout.back.y);
    frame(&p, 1);
    CHECK(out.back_pressed && !out.keys_pressed && !out.fire_pressed);
    frame(&p, 1); /* the finger stays down */
    CHECK(!out.back_pressed && !out.fire);
    frame(NULL, 0);
    p = at(2, st.layout.keys.x, st.layout.keys.y);
    frame(&p, 1);
    CHECK(out.keys_pressed && !out.back_pressed && !out.fire_pressed);
}

static void test_finger_sliding_onto_a_button_does_not_press_it(void) {
    setup();
    TouchPoint p = at(1, 640.0f, 360.0f); /* lands on the playfield: the pointer */
    frame(&p, 1);
    CHECK(out.fire_pressed && out.fire);
    p = at(1, st.layout.back.x, st.layout.back.y); /* slides onto the back button */
    frame(&p, 1);
    CHECK(!out.back_pressed);
    CHECK(out.fire); /* it is still the pointer */
    CHECK_NEAR(out.aim_x, st.layout.back.x, 0.01);
}

static void test_pointer_aim_fire_and_memory(void) {
    setup();
    TouchPoint p = at(7, 300.0f, 200.0f);
    frame(&p, 1);
    CHECK(out.fire && out.fire_pressed);
    CHECK_NEAR(out.aim_x, 300.0f, 0.01);
    CHECK_NEAR(out.aim_y, 200.0f, 0.01);
    p = at(7, 310.0f, 210.0f);
    frame(&p, 1);
    CHECK(out.fire && !out.fire_pressed); /* held */
    CHECK_NEAR(out.aim_x, 310.0f, 0.01);
    frame(NULL, 0); /* lifted: the aim stays where it was */
    CHECK(!out.fire);
    CHECK_NEAR(out.aim_x, 310.0f, 0.01);
    CHECK_NEAR(out.aim_y, 210.0f, 0.01);
    p = at(8, 500.0f, 400.0f); /* a new finger is a new press */
    frame(&p, 1);
    CHECK(out.fire && out.fire_pressed);
}

static void test_two_thumbs_stick_and_pointer(void) {
    setup();
    const TouchCircle s = st.layout.stick;
    TouchPoint both[2] = {at(1, s.x + s.r, s.y), at(2, 700.0f, 300.0f)};
    frame(both, 2);
    CHECK_NEAR(out.move_x, 1.0f, 0.001);
    CHECK(out.fire && out.fire_pressed);
    CHECK_NEAR(out.aim_x, 700.0f, 0.01); /* the aim follows the second finger, not the stick finger */
    CHECK_NEAR(out.aim_y, 300.0f, 0.01);
}

static void test_a_second_pointer_finger_is_ignored(void) {
    setup();
    TouchPoint both[2] = {at(1, 400.0f, 300.0f), at(2, 900.0f, 500.0f)};
    frame(both, 2);
    CHECK_NEAR(out.aim_x, 400.0f, 0.01);
    TouchPoint one = at(2, 900.0f, 500.0f); /* the first finger is lifted */
    frame(&one, 1);
    CHECK(!out.fire); /* finger 2 landed while 1 was the pointer: it stays ignored */
    CHECK(!out.fire_pressed);
}

static void test_ids_may_be_reused_after_a_lift(void) {
    setup();
    TouchPoint p = at(0, st.layout.action.x, st.layout.action.y);
    frame(&p, 1);
    CHECK(out.action_pressed);
    frame(NULL, 0);
    p = at(0, 640.0f, 360.0f); /* the OS hands out id 0 again */
    frame(&p, 1);
    CHECK(out.fire_pressed && !out.action);
}

static void test_release_all_and_too_many_fingers(void) {
    setup();
    TouchPoint many[TOUCH_MAX_POINTS + 5];
    for (int i = 0; i < TOUCH_MAX_POINTS + 5; i++)
        many[i] = at(i, 600.0f + (float)i, 300.0f);
    frame(many, TOUCH_MAX_POINTS + 5); /* must not overflow */
    CHECK(out.fire);
    touch_release_all(&st);
    CHECK_EQ_INT(st.hold_n, 0);
}

int main(void) {
    RUN(test_layout_inside_window_and_apart);
    RUN(test_no_fingers_means_no_input);
    RUN(test_stick_pushes_and_dead_zone);
    RUN(test_stick_keeps_the_finger_that_slid_out);
    RUN(test_direction_presses_are_one_shot);
    RUN(test_action_button_press_hold_release);
    RUN(test_back_and_keys_act_once_and_are_not_pointers);
    RUN(test_finger_sliding_onto_a_button_does_not_press_it);
    RUN(test_pointer_aim_fire_and_memory);
    RUN(test_two_thumbs_stick_and_pointer);
    RUN(test_a_second_pointer_finger_is_ignored);
    RUN(test_ids_may_be_reused_after_a_lift);
    RUN(test_release_all_and_too_many_fingers);
    return TEST_SUMMARY();
}
