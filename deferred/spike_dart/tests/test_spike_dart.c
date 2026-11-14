#include "arcade_test.h"
#include "spike_dart_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput throw_input(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    return in;
}

static void idle(spike_dart_State *s, int ticks, float dt) {
    ArcadeInput none;
    arcade_input_clear(&none);
    for (int i = 0; i < ticks; i++)
        spike_dart_step(s, &none, dt);
}

/* throws one spike and steps until it sticks (or gives up) */
static int throw_and_wait(spike_dart_State *s, float dt) {
    ArcadeInput in = throw_input();
    int n = 0;
    int before = s->stuck;
    spike_dart_step(s, &in, dt);
    n++;
    while (s->stuck == before && !s->over && n < 200) {
        idle(s, 1, dt);
        n++;
    }
    return n;
}

static void test_init_is_sane(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    CHECK(!spike_dart_is_over(&s));
    CHECK_EQ_INT(spike_dart_score(&s), 0);
    CHECK_NEAR(s.target_x, 240.0, 0.001);
}

static void test_spec_constants_at_480x800(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    CHECK_NEAR(s.target_y, 340.0, 0.001);
    CHECK_NEAR(s.target_dn, 460.0, 0.001);
    CHECK_NEAR(s.spike_y, 720.0, 0.001);
    CHECK_NEAR(s.spike_spd, 22.8, 0.001); /* KNIFEDIST 228 / 10 */
}

static void test_first_spike_sticks_in_10_ticks_at_orbit_radius(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    int ticks = throw_and_wait(&s, TICK);
    CHECK_EQ_INT(ticks, 10);
    CHECK_EQ_INT(s.stuck, 1);
    CHECK_NEAR(s.px[0], 240.0, 0.01);
    CHECK_NEAR(s.py[0], 492.0, 0.01); /* TARGETY + 152 */
}

static void test_orbit_after_one_second(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    throw_and_wait(&s, TICK);
    idle(&s, 50, TICK); /* 1 s: a quarter turn */
    CHECK_NEAR(s.px[0], 392.0, 0.05);
    CHECK_NEAR(s.py[0], 340.0, 0.05);
}

static void test_orbit_period_at_60hz(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    throw_and_wait(&s, ARC_DT);
    float x0 = s.px[0], y0 = s.py[0];
    float r0 = s.rot[0];
    idle(&s, 240, ARC_DT); /* 4 s = one full turn */
    CHECK_NEAR(s.rot[0] - r0, ARC_TAU, 0.01);
    CHECK_NEAR(s.px[0], x0, 1.5);
    CHECK_NEAR(s.py[0], y0, 1.5);
}

static void test_lane_test_matches_spec_vectors(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.px[0] = 244.0f; /* spec vector: (244, 470) is inside the lane */
    s.py[0] = 470.0f;
    CHECK(spike_dart_lane_blocked(&s));
    s.py[0] = 450.0f; /* above the target's bottom edge (460): not in the lane */
    CHECK(!spike_dart_lane_blocked(&s));
    s.py[0] = 470.0f;
    s.px[0] = 248.5f; /* 8.5 px off the lane: safe */
    CHECK(!spike_dart_lane_blocked(&s));
}

/* A stuck spike keeps orbiting during the 7 ticks before the check zone starts (tick 8), so aim its
 * start angle so that it is exactly in the lane when the new spike arrives. */
static void test_throw_into_a_passing_spike_ends_game(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.rot[0] = ARC_PI / 2.0f - 7.0f * (ARC_PI / 2.0f) * TICK;
    s.px[0] = 392.0f; /* parked out of the lane until the first orbit update */
    s.py[0] = 340.0f;
    throw_and_wait(&s, TICK);
    CHECK(s.over);
    CHECK(!s.capped);
    CHECK_EQ_INT(s.stuck, 1); /* the losing spike did not stick */
    idle(&s, 70, TICK);
    CHECK(spike_dart_is_over(&s));
}

static void test_throw_with_clear_lane_survives(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.rot[0] = ARC_PI / 2.0f + 1.0f; /* far from the lane when the new spike arrives */
    s.px[0] = 392.0f;
    s.py[0] = 340.0f;
    throw_and_wait(&s, TICK);
    CHECK(!s.over);
    CHECK_EQ_INT(s.stuck, 2);
}

/* No waiting at all: a spike eventually meets one passing the lane. The player ends the game by
 * playing badly, so no test relies on the monkey. */
static void test_spamming_throws_eventually_loses(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    for (int i = 0; i < 3000 && !spike_dart_is_over(&s); i++)
        spike_dart_step(&s, &in, ARC_DT);
    CHECK(spike_dart_is_over(&s));
    CHECK(spike_dart_score(&s) >= 1 && spike_dart_score(&s) < SD_MAX_SPIKES);
}

static void test_throw_ignored_while_flying(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    spike_dart_step(&s, &in, ARC_DT);
    spike_dart_step(&s, &in, ARC_DT);
    spike_dart_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.throws, 1);
}

static void test_cap_of_64_ends_the_game(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = SD_MAX_SPIKES - 1;
    for (int i = 0; i < s.stuck; i++) { /* park the old spikes out of the lane */
        s.px[i] = 392.0f;
        s.py[i] = 340.0f;
        s.rot[i] = 0.0f;
    }
    throw_and_wait(&s, TICK);
    CHECK_EQ_INT(s.stuck, SD_MAX_SPIKES);
    CHECK(s.over && s.capped);
    idle(&s, 70, TICK);
    CHECK(spike_dart_is_over(&s));
    CHECK_EQ_INT(spike_dart_score(&s), SD_MAX_SPIKES);
}

static void test_big_step_cannot_tunnel(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    spike_dart_step(&s, &in, 0.2f); /* a huge dt: the spike must still stick, not fly past */
    CHECK(s.stuck == 1 || s.flying);
    for (int i = 0; i < 5 && s.stuck == 0; i++)
        idle(&s, 1, 0.2f);
    CHECK_EQ_INT(s.stuck, 1);
}

static void test_same_seed_same_result(void) {
    spike_dart_State a, b;
    spike_dart_init(&a, 9);
    spike_dart_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        spike_dart_step(&a, &ia, ARC_DT);
        spike_dart_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.stuck, b.stuck);
    CHECK_NEAR(a.cur_y, b.cur_y, 0.0001);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        spike_dart_State s;
        spike_dart_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 50);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            spike_dart_step(&s, &in, ARC_DT);
            CHECK(s.stuck >= 0 && s.stuck <= SD_MAX_SPIKES);
            CHECK(isfinite(s.cur_y) && s.cur_y <= s.spike_y + 0.01f);
            for (int j = 0; j < s.stuck; j++)
                CHECK(isfinite(s.px[j]) && isfinite(s.py[j]));
            CHECK(spike_dart_score(&s) >= 0);
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_spec_constants_at_480x800);
    RUN(test_first_spike_sticks_in_10_ticks_at_orbit_radius);
    RUN(test_orbit_after_one_second);
    RUN(test_orbit_period_at_60hz);
    RUN(test_lane_test_matches_spec_vectors);
    RUN(test_throw_into_a_passing_spike_ends_game);
    RUN(test_throw_with_clear_lane_survives);
    RUN(test_spamming_throws_eventually_loses);
    RUN(test_throw_ignored_while_flying);
    RUN(test_cap_of_64_ends_the_game);
    RUN(test_big_step_cannot_tunnel);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
