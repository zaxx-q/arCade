#include "arcade/playfield.h"
#include "arcade_test.h"
#include "bw_reflex_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput click(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    return in;
}

static void test_init_is_sane(void) {
    bw_reflex_State s;
    bw_reflex_init(&s, 1);
    CHECK(!bw_reflex_is_over(&s));
    CHECK_EQ_INT(bw_reflex_score(&s), 0);
    CHECK_EQ_INT(s.base_h, 114); /* round(684 / 6) */
    CHECK(s.base_white == 0);
}

static void test_spec_geometry_at_480x800(void) {
    bw_reflex_State s;
    bw_reflex_init_sized(&s, 1, 480, 800);
    CHECK_EQ_INT(s.base_h, 133); /* round(800 / 6) */
    CHECK_NEAR(bw_reflex_contact_y(&s), 800 - 133 + 1.5, 0.001);
    /* per-tick fall at speed 4: (800 - 133) * 4 / 400 = 6.67 px */
    ArcadeInput none;
    arcade_input_clear(&none);
    float y0 = s.block_y;
    bw_reflex_step(&s, &none, TICK);
    CHECK_NEAR(s.block_y - y0, 6.67, 0.001);
}

static void test_same_seed_same_result(void) {
    bw_reflex_State a, b;
    bw_reflex_init(&a, 9);
    bw_reflex_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 600; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        bw_reflex_step(&a, &ia, ARC_DT);
        bw_reflex_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.score, b.score);
    CHECK_NEAR(a.block_y, b.block_y, 0.0001);
    CHECK_EQ_INT(a.over, b.over);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        bw_reflex_State s;
        bw_reflex_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 100);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            bw_reflex_step(&s, &in, ARC_DT);
            CHECK(isfinite(s.block_y) && isfinite(s.speed));
            CHECK(s.block_y <= (float)s.h);
            CHECK(bw_reflex_score(&s) >= 0);
            CHECK(s.last_note >= 0 && s.last_note < 8);
        }
    }
}

/* Spec vectors: speed after 1 success = 4.28 (mult 0.067375), after 2 = 4.5684... */
static void test_speed_law(void) {
    bw_reflex_State s;
    bw_reflex_init_sized(&s, 5, 480, 800);
    for (int round = 1; round <= 2; round++) {
        /* keep the base equal to the block, then let it land */
        int guard = 0;
        int before = s.score;
        while (s.score == before && guard++ < 1000) {
            ArcadeInput in;
            arcade_input_clear(&in);
            if (s.base_white != s.block_white)
                in.fire_pressed = true;
            bw_reflex_step(&s, &in, TICK);
        }
        CHECK_EQ_INT(s.score, round);
    }
    CHECK_NEAR(s.speed, 4.0 * 1.07 * (1.0 + 0.067375), 0.0001);
    CHECK_NEAR(s.mult, 0.07 * 0.9625 * 0.9625, 0.00001);
}

static void test_note_index_uses_old_score(void) {
    bw_reflex_State s;
    bw_reflex_init(&s, 7);
    for (int target = 1; target <= 9; target++) {
        int guard = 0;
        while (s.score < target && guard++ < 3000) {
            ArcadeInput in;
            arcade_input_clear(&in);
            if (s.base_white != s.block_white)
                in.fire_pressed = true;
            bw_reflex_step(&s, &in, ARC_DT);
        }
        CHECK_EQ_INT(s.score, target);
        CHECK_EQ_INT(s.last_note, (target - 1) % 8); /* score 8 -> index 0 again */
    }
}

/* With dt = 1/60 the first block needs ~2.0 s (121 frames +- one) to land at speed 4. */
static void test_fall_time_at_60hz(void) {
    bw_reflex_State s;
    bw_reflex_init(&s, 3);
    ArcadeInput none;
    arcade_input_clear(&none);
    int frames = 0;
    while (!s.over && s.matches == 0 && frames < 1000) {
        bw_reflex_step(&s, &none, ARC_DT);
        frames++;
    }
    CHECK(frames >= 120 && frames <= 124);
}

static void test_mismatch_ends_the_game_after_reveal(void) {
    bw_reflex_State s;
    bw_reflex_init(&s, 4);
    ArcadeInput none;
    arcade_input_clear(&none);
    /* make the base differ from the block, never click */
    s.base_white = 1 - s.block_white;
    for (int i = 0; i < 400 && !s.over; i++)
        bw_reflex_step(&s, &none, ARC_DT);
    CHECK(s.over);
    CHECK(!bw_reflex_is_over(&s)); /* the reveal delay is still running */
    for (int i = 0; i < 90 && !bw_reflex_is_over(&s); i++)
        bw_reflex_step(&s, &none, ARC_DT);
    CHECK(bw_reflex_is_over(&s));
    CHECK_EQ_INT(bw_reflex_score(&s), 0);
}

static void test_any_input_flips_and_flips_back(void) {
    bw_reflex_State s;
    bw_reflex_init(&s, 2);
    ArcadeInput in = click();
    bw_reflex_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.base_white, 1);
    arcade_input_clear(&in);
    in.typed = 'x';
    bw_reflex_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.base_white, 0);
    CHECK_EQ_INT(s.flips, 2);
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_spec_geometry_at_480x800);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    RUN(test_speed_law);
    RUN(test_note_index_uses_old_score);
    RUN(test_fall_time_at_60hz);
    RUN(test_mismatch_ends_the_game_after_reveal);
    RUN(test_any_input_flips_and_flips_back);
    return TEST_SUMMARY();
}
