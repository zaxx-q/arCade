#include "arcade/playfield.h"
#include "arcade_test.h"
#include "different_color_logic.h"

/* a left click at a playfield-LOCAL position, sent as a screen position like the real input */
static void click_at(different_color_State *s, float lx, float ly) {
    ArcadeInput in;
    arcade_input_clear(&in);
    Vec2 p = arc_pf_to_screen(lx, ly);
    in.aim_x = p.x;
    in.aim_y = p.y;
    in.fire_pressed = true;
    different_color_step(s, &in, ARC_DT);
}

static void click_odd(different_color_State *s, bool bottom) {
    Vec2 c = different_color_odd_center(s);
    click_at(s, c.x, c.y + (bottom ? s->y2 - DC_Y1 : 0.0f));
}

static void idle(different_color_State *s, int frames, float dt) {
    ArcadeInput none;
    arcade_input_clear(&none);
    for (int i = 0; i < frames; i++)
        different_color_step(s, &none, dt);
}

static void test_init_is_sane(void) {
    different_color_State s;
    different_color_init(&s, 1);
    CHECK(!different_color_is_over(&s));
    CHECK_EQ_INT(different_color_score(&s), 0);
    CHECK_EQ_INT(s.n, 2);
    CHECK(!s.timed);
    CHECK(s.dif >= 0 && s.dif < s.n);
}

/* spec section 7: (cols, rows) by score */
static void test_grid_table(void) {
    static const int table[][3] = {{0, 2, 1}, {1, 2, 2}, {2, 2, 2},  {3, 3, 2},
                                   {5, 3, 3}, {8, 4, 3}, {11, 4, 4}, {15, 5, 4}};
    for (size_t i = 0; i < ARC_ARRAY_LEN(table); i++) {
        int c, r;
        different_color_grid_for_score(table[i][0], &c, &r);
        CHECK_EQ_INT(c, table[i][1]);
        CHECK_EQ_INT(r, table[i][2]);
    }
}

static void test_spec_geometry_at_480x800(void) {
    different_color_State s;
    different_color_init_sized(&s, 3, 480, 800);
    CHECK_NEAR(s.rectw, 224.0, 0.001);
    CHECK_NEAR(s.recth, 312.0, 0.001);
    CHECK_NEAR(s.y2, 408.0, 0.001);
    s.dif = 1; /* spec vector: odd cell centre (352, 236) */
    Vec2 c = different_color_odd_center(&s);
    CHECK_NEAR(c.x, 352.0, 0.001);
    CHECK_NEAR(c.y, 236.0, 0.001);
}

static void test_colour_acceptance_vectors(void) {
    CHECK(different_color_random_color_ok(0.5f, 0.0f, 0.0f));
    CHECK(!different_color_random_color_ok(0.45f, 0.4f, 0.4f));  /* too dark */
    CHECK(!different_color_random_color_ok(0.9f, 0.85f, 0.88f)); /* too grey */
    const float base[3] = {0.8f, 0.2f, 0.2f};
    const float green[3] = {0.2f, 0.8f, 0.2f}; /* luma diff 0.1728: rejected */
    const float blue[3] = {0.2f, 0.2f, 0.9f};  /* luma diff 0.0996: rejected */
    const float cyan[3] = {0.1f, 0.9f, 0.9f};  /* luma diff 0.2814: accepted */
    CHECK(!different_color_diff_ok(base, green));
    CHECK(!different_color_diff_ok(base, blue));
    CHECK(different_color_diff_ok(base, cyan));
}

static void test_generated_boards_follow_the_colour_rules(void) {
    for (unsigned seed = 1; seed <= 30; seed++) {
        different_color_State s;
        different_color_init(&s, seed);
        for (int round = 0; round < 12; round++) {
            CHECK(s.cols * s.rows <= DC_MAX_CELLS);
            CHECK(s.cols * s.rows >= s.n);
            for (int i = 0; i < s.cols * s.rows; i++)
                CHECK(different_color_random_color_ok(s.cell[i][0], s.cell[i][1], s.cell[i][2]));
            CHECK(different_color_diff_ok(s.cell[s.dif], s.odd));
            click_odd(&s, round % 2 == 1);
            CHECK(!s.over);
        }
    }
}

static void test_both_boards_accept_the_odd_cell(void) {
    different_color_State s;
    different_color_init(&s, 4);
    click_odd(&s, false);
    CHECK_EQ_INT(s.score, 1);
    click_odd(&s, true);
    CHECK_EQ_INT(s.score, 2);
    CHECK(s.timed);
    CHECK_EQ_INT(s.solved, 2);
}

static void test_wrong_cell_ends_the_game(void) {
    different_color_State s;
    different_color_init_sized(&s, 5, 480, 800);
    s.dif = 1;
    click_at(&s, 120.0f, 236.0f); /* the OTHER cell of the 2 x 1 board (x 16..240) */
    CHECK(s.over);
    CHECK_EQ_INT(s.misses, 1);
    CHECK_EQ_INT(s.score, 0);
    CHECK(!different_color_is_over(&s)); /* ring reveal still running */
    idle(&s, 90, ARC_DT);
    CHECK(different_color_is_over(&s));
}

static void test_separator_and_outside_are_ignored(void) {
    different_color_State s;
    different_color_init_sized(&s, 5, 480, 800);
    s.dif = 1;
    click_at(&s, 120.0f, 400.0f); /* white separator strip (392..408) */
    CHECK(!s.over);
    click_at(&s, 120.0f, 50.0f); /* above the top board */
    CHECK(!s.over);
    click_at(&s, 5.0f, 200.0f); /* left margin */
    CHECK(!s.over);
    click_at(&s, 120.0f, 760.0f); /* below the bottom board / timer bar area */
    CHECK(!s.over);
}

static void test_click_in_a_gutter_is_ignored(void) {
    different_color_State s;
    different_color_init(&s, 6);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    in.aim_x = 100.0f; /* left gutter in SCREEN pixels */
    in.aim_y = 300.0f;
    different_color_step(&s, &in, ARC_DT);
    CHECK(!s.over);
    CHECK_EQ_INT(s.score, 0);
}

static void test_first_board_is_untimed_then_30_seconds(void) {
    different_color_State s;
    different_color_init(&s, 7);
    idle(&s, 3000, ARC_DT); /* 50 s of waiting on the first board */
    CHECK(!s.over);
    click_odd(&s, false);
    CHECK(s.timed);
    CHECK_NEAR(s.time_left, 30.0, 0.001);
    idle(&s, 29 * 60, ARC_DT);
    CHECK(!s.over);
    idle(&s, 2 * 60, ARC_DT);
    CHECK(s.over && s.timed_out);
    idle(&s, 90, ARC_DT);
    CHECK(different_color_is_over(&s));
    CHECK_EQ_INT(different_color_score(&s), 1);
}

static void test_tick_tack_every_half_second_in_the_last_10_seconds(void) {
    different_color_State s;
    different_color_init(&s, 8);
    click_odd(&s, false);
    idle(&s, 20 * 60, ARC_DT); /* 20 s used, 10 s left: warning about to start */
    int before = s.ticks;
    idle(&s, 5 * 60, ARC_DT); /* 5 s more: 10 ticks */
    CHECK(s.ticks - before >= 9 && s.ticks - before <= 11);
}

static void test_cap_ends_the_game_at_400_cells(void) {
    different_color_State s;
    different_color_init(&s, 9);
    s.score = DC_MAX_CELLS - 3; /* n = 399 */
    different_color_new_board(&s);
    click_odd(&s, false); /* score 398: n = 400 board is fine */
    CHECK(!s.over);
    CHECK_EQ_INT(s.n, 400);
    CHECK_EQ_INT(s.cols * s.rows, 400);
    click_odd(&s, false); /* score 399: the next board would not fit */
    CHECK(s.over && s.capped);
    CHECK_EQ_INT(different_color_score(&s), DC_MAX_CELLS - 1);
}

/* A scripted player that always finds the odd cell plays the whole run: the game ends by itself. */
static void test_perfect_player_reaches_the_cap(void) {
    different_color_State s;
    different_color_init(&s, 10);
    int guard = 0;
    while (!s.over && guard++ < 1000)
        click_odd(&s, guard % 2 == 0);
    CHECK(s.over && s.capped);
    idle(&s, 90, ARC_DT);
    CHECK(different_color_is_over(&s));
    CHECK_EQ_INT(different_color_score(&s), DC_MAX_CELLS - 1);
}

static void test_same_seed_same_result(void) {
    different_color_State a, b;
    different_color_init(&a, 9);
    different_color_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 600; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        different_color_step(&a, &ia, ARC_DT);
        different_color_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.score, b.score);
    CHECK_EQ_INT(a.dif, b.dif);
    CHECK_NEAR(a.cell[0][0], b.cell[0][0], 0.00001);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        different_color_State s;
        different_color_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 70);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            different_color_step(&s, &in, ARC_DT);
            CHECK(s.score >= 0 && s.score < DC_MAX_CELLS);
            CHECK(s.dif >= 0 && s.dif < s.n);
            CHECK(s.cols * s.rows <= DC_MAX_CELLS);
            CHECK(isfinite(s.time_left));
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_grid_table);
    RUN(test_spec_geometry_at_480x800);
    RUN(test_colour_acceptance_vectors);
    RUN(test_generated_boards_follow_the_colour_rules);
    RUN(test_both_boards_accept_the_odd_cell);
    RUN(test_wrong_cell_ends_the_game);
    RUN(test_separator_and_outside_are_ignored);
    RUN(test_click_in_a_gutter_is_ignored);
    RUN(test_first_board_is_untimed_then_30_seconds);
    RUN(test_tick_tack_every_half_second_in_the_last_10_seconds);
    RUN(test_cap_ends_the_game_at_400_cells);
    RUN(test_perfect_player_reaches_the_cap);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
