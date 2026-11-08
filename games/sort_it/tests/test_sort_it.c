#include "arcade/sortlib.h"
#include "arcade_test.h"
#include "sort_it_logic.h"

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static void press_action(sort_it_State *s) {
    ArcadeInput in = idle();
    in.action_pressed = true;
    sort_it_step(s, &in, ARC_DT);
}

static void move_cursor_to(sort_it_State *s, int target) {
    while (s->cursor != target) {
        ArcadeInput in = idle();
        if (s->cursor < target)
            in.right_pressed = true;
        else
            in.left_pressed = true;
        sort_it_step(s, &in, ARC_DT);
    }
}

static void swap_positions(sort_it_State *s, int a, int b) {
    move_cursor_to(s, a);
    press_action(s);
    move_cursor_to(s, b);
    press_action(s);
}

/* Optimal play for permutations of 1..n: put value i+1 at index i, one swap per misplaced element. */
static void solve_round_optimally(sort_it_State *s) {
    int round = s->round;
    for (int i = 0; i < s->n && s->round == round; i++) {
        if (s->values[i] == i + 1)
            continue;
        int j = i;
        while (s->values[j] != i + 1)
            j++;
        swap_positions(s, i, j);
    }
}

static void test_init_makes_an_unsorted_permutation(void) {
    for (unsigned seed = 1; seed <= 40; seed++) {
        sort_it_State s;
        sort_it_init(&s, seed);
        CHECK(!sort_is_sorted(s.values, (size_t)s.n));
        int seen[SI_MAX_N + 1] = {0};
        for (int i = 0; i < s.n; i++)
            seen[s.values[i]]++;
        for (int v = 1; v <= s.n; v++)
            CHECK_EQ_INT(seen[v], 1);
        CHECK_EQ_INT(s.par, sort_min_swaps(s.values, (size_t)s.n));
        CHECK(s.par <= s.selection_swaps);
        CHECK(s.budget > s.par);
    }
}

static void test_mark_toggle_and_cursor_bounds(void) {
    sort_it_State s;
    sort_it_init(&s, 3);
    ArcadeInput in = idle();
    in.left_pressed = true;
    sort_it_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.cursor, 0); /* cannot go below 0 */
    press_action(&s);
    CHECK_EQ_INT(s.mark, 0);
    press_action(&s); /* same bar again cancels */
    CHECK_EQ_INT(s.mark, -1);
    CHECK_EQ_INT(s.swaps, 0);
    move_cursor_to(&s, s.n - 1);
    in = idle();
    in.right_pressed = true;
    sort_it_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.cursor, s.n - 1);
}

static void test_swap_counts_and_exchanges_values(void) {
    sort_it_State s;
    sort_it_init(&s, 4);
    int a = s.values[0], b = s.values[2];
    swap_positions(&s, 0, 2);
    CHECK_EQ_INT(s.values[0], b);
    CHECK_EQ_INT(s.values[2], a);
    CHECK_EQ_INT(s.swaps, 1);
}

static void test_clicking_bars_uses_the_same_rules(void) {
    sort_it_State s;
    sort_it_init(&s, 5);
    float slot = SI_AREA_W / (float)s.n;
    CHECK_EQ_INT(sort_it_index_at(&s, SI_X0 + slot * 2.5f), 2);
    CHECK_EQ_INT(sort_it_index_at(&s, 10.0f), -1);
    CHECK_EQ_INT(sort_it_index_at(&s, SI_X0 + SI_AREA_W + 5.0f), -1);
    ArcadeInput in = idle();
    in.fire_pressed = true;
    in.aim_x = SI_X0 + slot * 2.5f;
    sort_it_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.cursor, 2);
    CHECK_EQ_INT(s.mark, 2);
}

static void test_optimal_play_scores_maximum(void) {
    sort_it_State s;
    sort_it_init(&s, 6);
    for (int r = 0; r < SI_ROUNDS; r++)
        solve_round_optimally(&s);
    CHECK(sort_it_is_over(&s));
    CHECK(!s.lost);
    CHECK_EQ_INT(sort_it_score(&s), 300 * SI_ROUNDS); /* par every round = full marks */
}

static void test_running_out_of_swaps_ends_the_game(void) {
    sort_it_State s;
    sort_it_init(&s, 7);
    int guard = 0;
    while (!sort_it_is_over(&s) && guard++ < 100)
        swap_positions(&s, 0, 1); /* wasteful: undo and redo */
    CHECK(sort_it_is_over(&s));
    CHECK(s.lost || s.round > 0);
    int score = sort_it_score(&s);
    press_action(&s);
    CHECK_EQ_INT(sort_it_score(&s), score);
}

static void test_monkey_is_safe(void) {
    sort_it_State s;
    sort_it_init(&s, 9);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 2);
    for (int i = 0; i < 5000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        sort_it_step(&s, &in, ARC_DT);
        CHECK(s.cursor >= 0 && s.cursor < s.n);
        CHECK(s.mark >= -1 && s.mark < s.n);
        CHECK(sort_it_score(&s) >= 0);
    }
}

int main(void) {
    RUN(test_init_makes_an_unsorted_permutation);
    RUN(test_mark_toggle_and_cursor_bounds);
    RUN(test_swap_counts_and_exchanges_values);
    RUN(test_clicking_bars_uses_the_same_rules);
    RUN(test_optimal_play_scores_maximum);
    RUN(test_running_out_of_swaps_ends_the_game);
    RUN(test_monkey_is_safe);
    return TEST_SUMMARY();
}
