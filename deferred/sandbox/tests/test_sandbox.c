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
    sandbox_clear(&g_s.world); /* the starter scene has seeds that grow: use a lifeless one here */
    for (int x = 20; x < 60; x++)
        sandbox_set(&g_s.world, x, 100, EL_STONE);
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
        CHECK(sandbox_palette_index(g_s.element) >= 0); /* the selection is always a visible element */
    }
    CHECK(sbx_score(&g_s) >= 0);
}

/* Centre of palette slot `i` in window pixels. */
static float slot_x(int i) {
    return (float)(SBX_PAL_X + (i % SBX_PAL_COLS) * SBX_PAL_SLOT_W + SBX_PAL_SLOT_W / 2);
}
static float slot_y(int i) {
    return (float)(SBX_PAL_Y + (i / SBX_PAL_COLS) * SBX_PAL_SLOT_H + SBX_PAL_SLOT_H / 2);
}

static void test_palette_geometry(void) {
    int n = sandbox_palette_count();
    CHECK(n <= SBX_PAL_COLS * SBX_PAL_ROWS); /* every element has a slot */
    CHECK(SBX_PAL_X >= 0 && SBX_PAL_X + SBX_PAL_COLS * SBX_PAL_SLOT_W <= ARC_SCREEN_W);
    CHECK(SBX_PAL_Y + SBX_PAL_ROWS * SBX_PAL_SLOT_H <= ARC_SCREEN_H);
    for (int i = 0; i < n; i++)
        CHECK_EQ_INT(sbx_palette_slot_at(slot_x(i), slot_y(i)), i); /* the slot centre hits its own slot */
    CHECK_EQ_INT(sbx_palette_slot_at(slot_x(n), slot_y(n)), -1);    /* the spare slot at the end is empty */
    CHECK_EQ_INT(sbx_palette_slot_at(5.0f, 5.0f), -1);              /* the world is not the palette */
    CHECK(sbx_over_panel(slot_x(0), slot_y(0)));
    CHECK(!sbx_over_panel(640.0f, 300.0f));
    /* the world ends exactly where the panel begins: no cell hides behind the panel */
    CHECK(SBX_H * SBX_CELL <= SBX_PAL_Y - SBX_PAL_PAD);
    CHECK(!sbx_over_panel(640.0f, (float)(SBX_H * SBX_CELL) - 1.0f));
    CHECK(sbx_over_panel(640.0f, (float)(SBX_H * SBX_CELL)));
}

static void test_click_on_palette_selects_without_painting(void) {
    sbx_init(&g_s, 1);
    sandbox_clear(&g_s.world);
    int lava = sandbox_palette_index(EL_LAVA);
    ArcadeInput in = idle();
    in.aim_x = slot_x(lava);
    in.aim_y = slot_y(lava);
    in.fire = in.fire_pressed = true;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, EL_LAVA);
    CHECK_EQ_INT(sbx_score(&g_s), 0); /* the click went to the palette, not to the world below it */
    /* dragging the pressed mouse up onto the world must not paint either */
    in = idle();
    in.fire = true;
    in.aim_x = 600.0f;
    in.aim_y = 200.0f;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sandbox_count(&g_s.world, EL_LAVA), 0);
    /* once the button is released and pressed again over the world, painting works */
    in = idle();
    sbx_step(&g_s, &in, ARC_DT);
    in.fire = in.fire_pressed = true;
    in.aim_x = 600.0f;
    in.aim_y = 200.0f;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(sandbox_count(&g_s.world, EL_LAVA) > 0);
}

static void test_cannot_paint_through_the_panel(void) {
    sbx_init(&g_s, 1);
    sandbox_clear(&g_s.world);
    ArcadeInput in = idle();
    in.fire = true;
    in.alt = true;
    int empty = sandbox_palette_count(); /* the first slot that holds no element */
    in.aim_x = slot_x(empty);
    in.aim_y = slot_y(empty);
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sbx_score(&g_s), 0);
    CHECK_EQ_INT(g_s.element, EL_SAND); /* clicking an empty slot does not change the selection */
}

static void test_number_keys_pick_the_first_row(void) {
    sbx_init(&g_s, 1);
    for (int k = 0; k <= 9; k++) {
        ArcadeInput in = idle();
        in.typed = '0' + k;
        sbx_step(&g_s, &in, ARC_DT);
        CHECK_EQ_INT(g_s.element, sandbox_palette_id(k));
    }
    ArcadeInput in = idle();
    in.typed = '3';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, EL_STONE); /* 0-3 still mean eraser, sand, water, stone */
}

static void test_bracket_keys_and_wheel_cycle_and_wrap(void) {
    sbx_init(&g_s, 1);
    int n = sandbox_palette_count();
    ArcadeInput in = idle();
    in.typed = '[';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, EL_EMPTY); /* sand is slot 1: one step back is the eraser */
    in.typed = '[';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, sandbox_palette_id(n - 1)); /* and one more wraps to the last element */
    in = idle();
    in.typed = ']';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, EL_EMPTY);
    in = idle();
    in.wheel = 3;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, sandbox_palette_id(3));
    in.wheel = -7;
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.element, sandbox_palette_id(n - 4)); /* the wheel wraps in both directions */
    for (int i = 0; i < 3 * n; i++) {                     /* a full tour never lands on a hidden element */
        in = idle();
        in.wheel = 1;
        sbx_step(&g_s, &in, ARC_DT);
        CHECK(!(sandbox_element(g_s.element)->flags & EF_HIDDEN));
    }
}

static void test_starter_scene_and_reset(void) {
    sbx_init(&g_s, 1);
    CHECK(sandbox_count(&g_s.world, EL_WATER) > 100);
    CHECK_EQ_INT(sandbox_count(&g_s.world, EL_SEED), 3);
    CHECK(sandbox_count(&g_s.world, EL_SOIL) > 100);
    ArcadeInput in = idle();
    for (int i = 0; i < 400; i++)
        sbx_step(&g_s, &in, ARC_DT);
    CHECK(sandbox_count(&g_s.world, EL_PLANT) > 0); /* the seeds sprout by themselves on the soil */
    in.typed = 'c';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sbx_score(&g_s), 0);
    in.typed = 'r';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sandbox_count(&g_s.world, EL_SEED), 3); /* R brings the starter scene back */
}

/* Centre of world cell (cx, cy) in window pixels. */
static float cell_px(int c) {
    return (float)(c * SBX_CELL + SBX_CELL / 2);
}

static void test_hover_probe_reports_element_and_temperature(void) {
    sbx_init(&g_s, 1);
    sandbox_clear(&g_s.world);
    sandbox_set(&g_s.world, 50, 40, EL_STEEL);
    sandbox_set_temp(&g_s.world, 50, 40, 321);
    ElementId id = EL_SAND;
    int t = 0;
    CHECK(sbx_probe(&g_s, cell_px(50), cell_px(40), &id, &t));
    CHECK_EQ_INT(id, EL_STEEL);
    CHECK_EQ_INT(t, 321);
    CHECK(sbx_probe(&g_s, cell_px(60), cell_px(40), &id, &t)); /* empty space still has a temperature */
    CHECK_EQ_INT(id, EL_EMPTY);
    CHECK_EQ_INT(t, SANDBOX_AMBIENT);
    CHECK(!sbx_probe(&g_s, slot_x(0), slot_y(0), &id, &t)); /* on the palette: nothing to read */
    CHECK(!sbx_probe(&g_s, -5.0f, 10.0f, &id, &t));         /* outside the window */
    CHECK(!sbx_probe(&g_s, 5000.0f, 10.0f, &id, &t));
}

static void test_thermal_view_key_toggles(void) {
    sbx_init(&g_s, 1);
    CHECK(!g_s.world.heat_view);
    ArcadeInput in = idle();
    in.typed = 't';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(g_s.world.heat_view);
    in.typed = 'T';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(!g_s.world.heat_view);
    in.typed = 'T';
    sbx_step(&g_s, &in, ARC_DT);
    CHECK(g_s.world.heat_view);
    in.typed = 'C'; /* the thermal view is a view only: clearing the world leaves it on */
    sbx_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(sbx_score(&g_s), 0);
    CHECK(g_s.world.heat_view);
}

static void test_the_starter_kettle_heats_and_boils(void) {
    sbx_init(&g_s, 1);
    ElementId id;
    int t;
    ArcadeInput in = idle();
    in.aim_x = cell_px(164); /* the middle of the kettle's water */
    in.aim_y = cell_px(145);
    CHECK(sbx_probe(&g_s, in.aim_x, in.aim_y, &id, &t));
    CHECK_EQ_INT(id, EL_WATER);
    CHECK_EQ_INT(t, SANDBOX_AMBIENT);
    for (int i = 0; i < 120; i++)
        sbx_step(&g_s, &in, ARC_DT);
    CHECK(sbx_probe(&g_s, in.aim_x, in.aim_y, &id, &t));
    CHECK(t > 50);                                   /* the heater has warmed the water through the steel */
    CHECK(sandbox_temp(&g_s.world, 170, 148) > 300); /* the steel plate is far hotter than the water */
    for (int i = 0; i < 700; i++)
        sbx_step(&g_s, &in, ARC_DT);
    CHECK(sandbox_count(&g_s.world, EL_STEAM) > 20);            /* and it has been boiling */
    CHECK_EQ_INT(g_s.world.cells[149 * SBX_W + 140], EL_EMPTY); /* the heater did not set the room alight */
}

static void test_painting_every_element_is_safe(void) {
    sbx_init(&g_s, 3);
    for (int i = 0; i < sandbox_palette_count(); i++) {
        ArcadeInput in = idle();
        in.aim_x = slot_x(i);
        in.aim_y = slot_y(i);
        in.fire = in.fire_pressed = true;
        sbx_step(&g_s, &in, ARC_DT); /* pick it on the palette */
        in = idle();
        in.fire = in.fire_pressed = true;
        in.aim_x = (float)(40 + i * 10) * SBX_CELL;
        in.aim_y = 30.0f * SBX_CELL;
        for (int t = 0; t < 20; t++)
            sbx_step(&g_s, &in, ARC_DT);
    }
    for (int t = 0; t < 600; t++) {
        ArcadeInput in = idle();
        sbx_step(&g_s, &in, ARC_DT);
    }
    CHECK(sbx_score(&g_s) >= 0);
}

int main(void) {
    RUN(test_init_and_idle_stepping);
    RUN(test_select_element_and_paint);
    RUN(test_clear_pause_and_brush);
    RUN(test_monkey_is_safe);
    RUN(test_palette_geometry);
    RUN(test_click_on_palette_selects_without_painting);
    RUN(test_cannot_paint_through_the_panel);
    RUN(test_number_keys_pick_the_first_row);
    RUN(test_bracket_keys_and_wheel_cycle_and_wrap);
    RUN(test_starter_scene_and_reset);
    RUN(test_hover_probe_reports_element_and_temperature);
    RUN(test_thermal_view_key_toggles);
    RUN(test_the_starter_kettle_heats_and_boils);
    RUN(test_painting_every_element_is_safe);
    return TEST_SUMMARY();
}
