#include "arcade/vault.h"
#include "arcade_test.h"

static Sandbox g_sb; /* ~115 KB: keep off the stack */
static Vault g_v;

/* a 12 x 6 vault with its interior at x 14..25, y 26..31 and the floor on row 32 */
static void make_vault(void) {
    sandbox_init(&g_sb, 40, 40, 11);
    vault_build(&g_sb, &g_v, 14, 26, 12, 6);
}

static void test_build_geometry(void) {
    make_vault();
    CHECK(!g_v.open);
    CHECK_EQ_INT(sandbox_get(&g_sb, 13, 28), EL_STONE);                     /* left wall */
    CHECK_EQ_INT(sandbox_get(&g_sb, 26, 28), EL_STONE);                     /* right wall */
    CHECK_EQ_INT(sandbox_get(&g_sb, 20, 32), EL_STONE);                     /* floor, including the plug */
    CHECK_EQ_INT(sandbox_get(&g_sb, 13, 32), EL_STONE);                     /* floor corner */
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 14, 26, 12, 6, EL_EMPTY), 12 * 6); /* interior is empty */
    CHECK_NEAR(vault_progress(&g_sb, &g_v), 0.0, 0.0001);
    CHECK(vault_contains(&g_v, 14, 26) && vault_contains(&g_v, 25, 31));
    CHECK(!vault_contains(&g_v, 13, 26) && !vault_contains(&g_v, 26, 26)); /* the walls are not interior */
    CHECK(!vault_contains(&g_v, 14, 25) && !vault_contains(&g_v, 14, 32)); /* nor are the rim and floor */
}

static void test_build_clears_the_interior(void) {
    sandbox_init(&g_sb, 40, 40, 11);
    sandbox_paint(&g_sb, 20, 28, 4, EL_SAND);
    vault_build(&g_sb, &g_v, 14, 26, 12, 6);
    CHECK_EQ_INT(sandbox_count_in(&g_sb, 14, 26, 12, 6, EL_SAND), 0);
}

static void test_progress_follows_the_fill(void) {
    make_vault();
    for (int y = 29; y < 32; y++) /* bottom half: 3 of 6 rows */
        for (int x = 14; x < 26; x++)
            sandbox_set(&g_sb, x, y, EL_SAND);
    float half = vault_progress(&g_sb, &g_v);
    CHECK(half > 0.5f && half < 0.6f); /* 0.5 / 0.9 = 0.555... */
    CHECK(!vault_update(&g_sb, &g_v)); /* not full enough: stays closed */
    CHECK(!g_v.open);
}

static void test_a_full_vault_opens_once_and_drains(void) {
    make_vault();
    for (int y = 26; y < 32; y++)
        for (int x = 14; x < 26; x++)
            sandbox_set(&g_sb, x, y, EL_SAND);
    CHECK_NEAR(vault_progress(&g_sb, &g_v), 1.0, 0.0001);
    CHECK(vault_update(&g_sb, &g_v)); /* opens on this tick... */
    CHECK(g_v.open);
    CHECK(!vault_update(&g_sb, &g_v));                  /* ...and only on this tick */
    CHECK_EQ_INT(sandbox_get(&g_sb, 19, 32), EL_EMPTY); /* the plug is gone */
    CHECK_EQ_INT(sandbox_get(&g_sb, 20, 32), EL_EMPTY);
    CHECK_EQ_INT(sandbox_get(&g_sb, 18, 32), EL_STONE); /* the rest of the floor is still there */
    for (int i = 0; i < 400; i++)
        sandbox_step(&g_sb);
    CHECK(sandbox_count_in(&g_sb, 14, 26, 12, 6, EL_SAND) < 12 * 6 / 2); /* it drains through the gap */
    CHECK(g_v.open);                                                     /* and stays open */
}

static void test_water_poured_from_above_opens_it(void) {
    make_vault();
    bool opened = false;
    int opened_at = -1;
    for (int tick = 0; tick < 600 && !opened; tick++) {
        if (tick < 80)
            sandbox_paint(&g_sb, 20, 6, 2, EL_WATER); /* a hand holding the right mouse button above it */
        sandbox_step(&g_sb);
        opened = vault_update(&g_sb, &g_v);
        opened_at = tick;
    }
    CHECK(opened);
    CHECK(opened_at < 200); /* a short pour is enough, no endless waiting */
}

static void test_sand_poured_from_above_opens_it_too(void) {
    make_vault();
    bool opened = false;
    for (int tick = 0; tick < 600 && !opened; tick++) {
        if (tick < 80)
            sandbox_paint(&g_sb, 20, 6, 2, EL_SAND);
        sandbox_step(&g_sb);
        opened = vault_update(&g_sb, &g_v);
    }
    CHECK(opened); /* sand can never leave it stuck: a clogged vault still opens */
}

static void test_open_now(void) {
    make_vault();
    vault_open_now(&g_sb, &g_v);
    CHECK(g_v.open);
    CHECK_EQ_INT(sandbox_get(&g_sb, 19, 32), EL_EMPTY);
    CHECK(!vault_update(&g_sb, &g_v)); /* already open: no second "just opened" */
}

int main(void) {
    RUN(test_build_geometry);
    RUN(test_build_clears_the_interior);
    RUN(test_progress_follows_the_fill);
    RUN(test_a_full_vault_opens_once_and_drains);
    RUN(test_water_poured_from_above_opens_it);
    RUN(test_sand_poured_from_above_opens_it_too);
    RUN(test_open_now);
    return TEST_SUMMARY();
}
