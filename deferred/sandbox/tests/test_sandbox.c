#include "arcade_test.h"
#include "sandbox_logic.h"

static sbx_State g_s; /* big: keep off the stack */

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static void test_init_and_idle_stepping(void) {
    sbx_init(&g_s, 1);
    CHECK(!sbx_is_over(&g_s));
    int before = sbx_score(&g_s);
    ArcadeInput in = idle();
    for (int i = 0; i < 10; i++)
        sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.ticks, 10);
    CHECK_EQ_INT(sbx_score(&g_s), before); /* nothing painted, nothing created */
}

static void test_select_element_and_paint(void) {
    sbx_init(&g_s, 1);
    ArcadeInput in = idle();
    in.typed = '2';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, EL_WATER);
    in = idle();
    in.fire = true;
    in.aim_x = 100.0f * SBX_CELL;
    in.aim_y = 20.0f * SBX_CELL;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(sandbox_count(&g_s.world, EL_WATER) > 0);
}

static void test_clear_pause_and_brush(void) {
    sbx_init(&g_s, 1);
    ArcadeInput in = idle();
    in.action_pressed = true;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(g_s.paused);
    unsigned t = g_s.ticks;
    in = idle();
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.ticks, t); /* paused: the world does not advance */
    in = idle();
    in.typed = '+';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.brush, 4);
    in = idle();
    in.typed = 'c';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sbx_score(&g_s), 0);
}

static void test_monkey_is_safe(void) {
    sbx_init(&g_s, 2);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 8);
    for (int i = 0; i < 3000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        sbx_step(&g_s, &in, ARC_DT);
        CHECK(g_s.brush >= 0 && g_s.brush <= 12);
        CHECK((int)g_s.element >= 0 && g_s.element < EL_COUNT);
    }
    CHECK(sbx_score(&g_s) >= 0);
}

int main(void) {
    RUN(test_init_and_idle_stepping);
    RUN(test_select_element_and_paint);
    RUN(test_clear_pause_and_brush);
    RUN(test_monkey_is_safe);
    return TEST_SUMMARY();
}
