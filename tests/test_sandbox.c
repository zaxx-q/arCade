#include "arcade/sandbox.h"
#include "arcade_test.h"

static Sandbox g_sb; /* ~115 KB: keep off the stack */

static void run(int ticks) {
    for (int i = 0; i < ticks; i++)
        sandbox_step(&g_sb);
}

static void test_grid_and_bounds(void) {
    sandbox_init(&g_sb, 1000, 1000, 1); /* clamped to the maximum size */
    CHECK_EQ_INT(g_sb.w, SANDBOX_MAX_W);
    CHECK_EQ_INT(g_sb.h, SANDBOX_MAX_H);
    sandbox_init(&g_sb, 10, 10, 1);
    CHECK_EQ_INT(sandbox_get(&g_sb, -1, 0), EL_STONE); /* outside = solid wall */
    CHECK_EQ_INT(sandbox_get(&g_sb, 10, 5), EL_STONE);
    sandbox_set(&g_sb, 99, 99, EL_SAND); /* silently ignored */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
}

static void test_sand_falls_to_floor(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    sandbox_set(&g_sb, 5, 0, EL_SAND);
    run(20);
    CHECK_EQ_INT(sandbox_get(&g_sb, 5, 9), EL_SAND);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 1);
}

static void test_sand_makes_a_pile_and_conserves_mass(void) {
    sandbox_init(&g_sb, 21, 21, 2);
    for (int i = 0; i < 60; i++) {
        sandbox_set(&g_sb, 10, 0, EL_SAND);
        sandbox_step(&g_sb);
    }
    run(80);
    int n = sandbox_count(&g_sb, EL_SAND);
    CHECK(n >= 55); /* some spawns were skipped while the spawn cell was busy, but nothing vanished */
    int width = 0;
    for (int x = 0; x < 21; x++)
        if (sandbox_get(&g_sb, x, 20) == EL_SAND)
            width++;
    CHECK(width >= 5); /* it spread into a pile instead of one tall column */
}

static void test_stone_is_static_and_blocks(void) {
    sandbox_init(&g_sb, 10, 10, 1);
    for (int x = 0; x < 10; x++)
        sandbox_set(&g_sb, x, 5, EL_STONE);
    sandbox_set(&g_sb, 4, 0, EL_SAND);
    run(30);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_STONE), 10);
    CHECK_EQ_INT(sandbox_get(&g_sb, 4, 4), EL_SAND); /* rests on the shelf */
}

static void test_water_spreads_flat(void) {
    sandbox_init(&g_sb, 12, 6, 3);
    for (int i = 0; i < 12; i++)
        sandbox_set(&g_sb, 6, i % 2, EL_WATER);
    sandbox_paint(&g_sb, 6, 0, 1, EL_WATER);
    int before = sandbox_count(&g_sb, EL_WATER);
    run(120);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), before);
    int bottom = 0;
    for (int x = 0; x < 12; x++)
        if (sandbox_get(&g_sb, x, 5) == EL_WATER)
            bottom++;
    CHECK(bottom >= before - 1 || bottom >= 6); /* settles along the floor */
}

static void test_sand_sinks_through_water(void) {
    sandbox_init(&g_sb, 5, 8, 4);
    for (int y = 4; y < 8; y++)
        for (int x = 0; x < 5; x++)
            sandbox_set(&g_sb, x, y, EL_WATER);
    sandbox_set(&g_sb, 2, 0, EL_SAND);
    run(60);
    CHECK_EQ_INT(sandbox_get(&g_sb, 2, 7), EL_SAND);
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_WATER), 20); /* water was displaced, not destroyed */
}

static void test_paint_and_erase(void) {
    sandbox_init(&g_sb, 20, 20, 1);
    sandbox_paint(&g_sb, 10, 10, 3, EL_STONE);
    int stone = sandbox_count(&g_sb, EL_STONE);
    CHECK(stone > 10);
    sandbox_paint(&g_sb, 10, 10, 3, EL_SAND); /* does not overwrite stone */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
    sandbox_paint(&g_sb, 10, 10, 5, EL_EMPTY); /* eraser removes everything in the brush */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_STONE), 0);
    sandbox_paint(&g_sb, -50, -50, 4, EL_SAND); /* far outside: ignored */
    CHECK_EQ_INT(sandbox_count(&g_sb, EL_SAND), 0);
}

static void test_element_table(void) {
    for (int i = 0; i < EL_COUNT; i++)
        CHECK(sandbox_element((ElementId)i)->name != NULL);
    CHECK(sandbox_element((ElementId)999) == sandbox_element(EL_EMPTY));
    CHECK(sandbox_element(EL_SAND)->density > sandbox_element(EL_WATER)->density);
}

static void test_deterministic(void) {
    static Sandbox other;
    sandbox_init(&g_sb, 30, 30, 77);
    sandbox_init(&other, 30, 30, 77);
    for (int i = 0; i < 100; i++) {
        sandbox_paint(&g_sb, 15, 2, 2, i % 3 ? EL_SAND : EL_WATER);
        sandbox_paint(&other, 15, 2, 2, i % 3 ? EL_SAND : EL_WATER);
        sandbox_step(&g_sb);
        sandbox_step(&other);
    }
    for (int i = 0; i < 30 * 30; i++)
        CHECK_EQ_INT(g_sb.cells[i], other.cells[i]);
}

int main(void) {
    RUN(test_grid_and_bounds);
    RUN(test_sand_falls_to_floor);
    RUN(test_sand_makes_a_pile_and_conserves_mass);
    RUN(test_stone_is_static_and_blocks);
    RUN(test_water_spreads_flat);
    RUN(test_sand_sinks_through_water);
    RUN(test_paint_and_erase);
    RUN(test_element_table);
    RUN(test_deterministic);
    return TEST_SUMMARY();
}
