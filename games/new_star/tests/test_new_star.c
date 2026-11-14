#include "arcade/playfield.h"
#include "arcade_test.h"
#include "new_star_logic.h"

#define TICK 0.02f

static new_star_State *fresh(unsigned seed) {
    static new_star_State s;
    new_star_init_sized(&s, seed, 480, 800);
    return &s;
}

static void advance(new_star_State *s, float seconds) {
    ArcadeInput none;
    arcade_input_clear(&none);
    int ticks = (int)(seconds / TICK + 0.5f);
    for (int i = 0; i < ticks; i++)
        new_star_step(s, &none, TICK);
}

/* steps until the round clock (seconds since the newest star appeared) reaches `target` */
static void advance_to_round_time(new_star_State *s, float target) {
    ArcadeInput none;
    arcade_input_clear(&none);
    int guard = 0;
    while (s->new_t < target && !s->over && guard++ < 100000)
        new_star_step(s, &none, TICK);
}

/* a tap through the real input path: the pointer is in SCREEN pixels */
static void tap_via_input(new_star_State *s, float lx, float ly) {
    ArcadeInput in;
    arcade_input_clear(&in);
    Vec2 p = arc_pf_to_screen(lx, ly);
    in.aim_x = p.x;
    in.aim_y = p.y;
    in.fire_pressed = true;
    new_star_step(s, &in, TICK);
}

/* confirms stars until `count` exist (each round: tap the new star, wait for the next one) */
static void play_to(new_star_State *s, int count) {
    int guard = 0;
    while (s->n < count && !s->over && guard++ < 1000) {
        if (s->score < s->n)
            tap_via_input(s, s->x[s->score], s->y[s->score]);
        advance(s, 1.5f);
    }
}

static void test_init_is_sane(void) {
    new_star_State s;
    new_star_init(&s, 1);
    CHECK(!new_star_is_over(&s));
    CHECK_EQ_INT(new_star_score(&s), 0);
    CHECK_EQ_INT(s.n, 1);
}

static void test_first_star_at_480x800(void) {
    new_star_State *s = fresh(1);
    CHECK_NEAR(s->x[0], 240.0, 0.001);
    CHECK_NEAR(s->y[0], 400.0, 0.001);
    CHECK_EQ_INT(s->type[0], 5);
    CHECK_NEAR(s->size[0], 56.0, 0.001);
    CHECK_NEAR(s->score_pos.x, 240.0, 0.001);
    CHECK_NEAR(s->score_pos.y, 100.0, 0.001);
}

static void test_star_type_sizes(void) {
    static const float sizes[6] = {40.0f, 32.0f, 56.0f, 48.0f, 48.0f, 56.0f};
    for (int k = 0; k < 6; k++)
        CHECK_NEAR(new_star_type_size(k), sizes[k], 0.001);
}

/* spec section 7: overlay alpha vectors */
static void test_overlay_alpha_vectors(void) {
    CHECK_NEAR(new_star_overlay_alpha(0.0f), 0.0, 0.0001);
    CHECK_NEAR(new_star_overlay_alpha(0.5f), 0.7071, 0.0001);
    CHECK_NEAR(new_star_overlay_alpha(1.1f), 1.0, 0.0001);
    CHECK_NEAR(new_star_overlay_alpha(1.7f), 0.7071, 0.0001);
    CHECK(new_star_overlay_visible(2.1f));
    CHECK(!new_star_overlay_visible(2.2f));
}

static void test_new_star_appears_at_1_05_seconds(void) {
    new_star_State *s = fresh(2);
    tap_via_input(s, 240.0f, 400.0f);
    CHECK_EQ_INT(s->score, 1);
    CHECK(s->overlay);
    advance(s, 1.0f);
    CHECK_EQ_INT(s->n, 1); /* still the overlay's fade-in */
    advance(s, 0.08f);
    CHECK_EQ_INT(s->n, 2);
    advance(s, 1.5f); /* the overlay is gone after 2.2 s */
    CHECK(!s->overlay);
}

static void test_taps_are_ignored_for_the_first_1_4_seconds(void) {
    new_star_State *s = fresh(3);
    tap_via_input(s, 240.0f, 400.0f);
    advance(s, 1.3f); /* the new star exists by now (1.05 s) */
    CHECK_EQ_INT(s->n, 2);
    tap_via_input(s, s->x[1], s->y[1]); /* 1.32 s: too early */
    CHECK_EQ_INT(s->score, 1);
    advance(s, 0.1f);
    tap_via_input(s, s->x[1], s->y[1]); /* 1.44 s: accepted */
    CHECK_EQ_INT(s->score, 2);
}

/* spec section 7: star at (100,100) size 40: taps within [70..130] x [70..130] count */
static void test_hit_box_edges(void) {
    new_star_State *s = fresh(4);
    s->x[0] = 100.0f;
    s->y[0] = 100.0f;
    s->size[0] = 40.0f;
    new_star_tap(s, 130.1f, 100.0f);
    CHECK_EQ_INT(s->score, 0);
    new_star_tap(s, 100.0f, 69.9f);
    CHECK_EQ_INT(s->score, 0);
    new_star_tap(s, 130.0f, 130.0f); /* the corner of the box */
    CHECK_EQ_INT(s->score, 1);
    new_star_State *t = fresh(4);
    t->x[0] = 100.0f;
    t->y[0] = 100.0f;
    t->size[0] = 40.0f;
    new_star_tap(t, 70.0f, 70.0f);
    CHECK_EQ_INT(t->score, 1);
}

static void test_clicks_in_the_gutters_are_ignored(void) {
    new_star_State s;
    new_star_init(&s, 5);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    in.aim_x = ARC_PF_X + 240.0f - 200.0f - 100.0f; /* left gutter, in line with the star */
    in.aim_y = ARC_PF_Y + s.y[0];
    new_star_step(&s, &in, TICK);
    CHECK_EQ_INT(s.score, 0);
    in.aim_x = ARC_PF_X + s.x[0]; /* the star itself works */
    new_star_step(&s, &in, TICK);
    CHECK_EQ_INT(s.score, 1);
}

static void test_tapping_an_old_star_ends_the_game(void) {
    new_star_State *s = fresh(6);
    play_to(s, 3); /* stars 0 and 1 confirmed, star 2 is new */
    CHECK_EQ_INT(s->score, 2);
    tap_via_input(s, s->x[0], s->y[0]);
    CHECK(s->over);
    CHECK(!s->timed_out);
    CHECK(!new_star_is_over(s)); /* the missed star is blinking */
    advance(s, 1.6f);
    CHECK(new_star_is_over(s));
    CHECK_EQ_INT(new_star_score(s), 2);
}

static void test_tapping_nothing_does_nothing(void) {
    new_star_State *s = fresh(7);
    play_to(s, 3);
    int score = s->score;
    tap_via_input(s, 5.0f, 790.0f); /* a corner: no star there */
    CHECK(!s->over);
    CHECK_EQ_INT(s->score, score);
}

static void test_old_star_taps_are_ignored_during_the_overlay(void) {
    new_star_State *s = fresh(8);
    play_to(s, 2);
    tap_via_input(s, s->x[1], s->y[1]); /* confirm star 1: overlay starts */
    advance(s, 0.5f);
    tap_via_input(s, s->x[0], s->y[0]); /* an old star, but the screen is locked */
    CHECK(!s->over);
}

static void test_no_time_limit_before_the_10th_star(void) {
    new_star_State *s = fresh(9);
    play_to(s, 3);
    advance(s, 60.0f);
    CHECK(!s->over);
    CHECK(!s->alarm);
}

/* spec section 7: with 10 stars, alarm at +10.001 s and game over at +15.001 s (from the star's creation) */
static void test_10th_star_alarm_and_timeout(void) {
    new_star_State *s = fresh(10);
    play_to(s, 10);
    CHECK_EQ_INT(s->n, 10);
    advance_to_round_time(s, 9.9f);
    CHECK(!s->overlay);
    CHECK(!s->alarm);
    advance_to_round_time(s, 10.2f);
    CHECK(s->alarm);
    CHECK_EQ_INT(s->alarms, 1);
    advance(s, 0.2f);
    bool shaken = false;
    for (int i = 0; i < s->n; i++)
        if (fabsf(s->shake[i]) > 0.0f)
            shaken = true;
    CHECK(shaken);
    advance_to_round_time(s, 14.8f);
    CHECK(!s->over);
    advance_to_round_time(s, 15.2f);
    CHECK(s->over && s->timed_out);
    for (int i = 0; i < s->n; i++)
        CHECK_NEAR(s->shake[i], 0.0, 0.0001); /* shaking stops */
    advance(s, 1.6f);
    CHECK(new_star_is_over(s));
    CHECK_EQ_INT(new_star_score(s), 9);
}

static void test_new_star_resets_the_shake_and_the_alarm(void) {
    new_star_State *s = fresh(11);
    play_to(s, 10);
    advance(s, 11.0f);
    CHECK(s->alarm);
    tap_via_input(s, s->x[s->score], s->y[s->score]); /* in time, 11 s into the round */
    CHECK(!s->over);
    advance(s, 1.5f); /* the 11th star appears */
    CHECK_EQ_INT(s->n, 11);
    CHECK(!s->alarm);
    for (int i = 0; i < s->n; i++)
        CHECK_NEAR(s->shake[i], 0.0, 0.0001);
}

static void test_all_stars_rotate_150_deg_per_second(void) {
    new_star_State *s = fresh(12);
    play_to(s, 4);
    float before[4];
    for (int i = 0; i < 4; i++)
        before[i] = s->rot[i];
    advance(s, 1.0f);
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(s->rot[i] - before[i], 150.0, 0.5);
}

static void test_placement_invariants(void) {
    for (unsigned seed = 1; seed <= 30; seed++) {
        new_star_State *s = fresh(seed);
        play_to(s, 25);
        CHECK_EQ_INT(s->n, 25);
        for (int i = 1; i < s->n; i++) {
            CHECK(s->x[i] >= 38.0f && s->x[i] <= 442.0f);
            CHECK(s->y[i] >= 78.0f && s->y[i] <= 762.0f);
            CHECK(!(fabsf(s->x[i] - 240.0f) < 100.0f && fabsf(s->y[i] - 100.0f) < 60.0f));
            CHECK(s->type[i] >= 0 && s->type[i] < 6);
            CHECK(s->rot[i] > -1.0f);
        }
        for (int i = 0; i < s->n; i++)
            for (int j = i + 1; j < s->n; j++)
                CHECK(vec2_dist(vec2(s->x[i], s->y[i]), vec2(s->x[j], s->y[j])) >
                      (s->size[i] + s->size[j]) / 1.8f);
    }
}

static void test_cap_of_512_stars_ends_the_game(void) {
    new_star_State *s = fresh(13);
    s->n = NS_MAX_STARS;
    s->score = NS_MAX_STARS - 1;
    s->x[NS_MAX_STARS - 1] = 100.0f;
    s->y[NS_MAX_STARS - 1] = 300.0f;
    s->size[NS_MAX_STARS - 1] = 40.0f;
    new_star_tap(s, 100.0f, 300.0f);
    CHECK(s->over && s->capped);
    CHECK_EQ_INT(s->score, NS_MAX_STARS);
    advance(s, 1.6f);
    CHECK(new_star_is_over(s));
}

static void test_same_seed_same_result(void) {
    new_star_State a, b;
    new_star_init(&a, 9);
    new_star_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        new_star_step(&a, &ia, ARC_DT);
        new_star_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.score, b.score);
    CHECK_EQ_INT(a.n, b.n);
    CHECK_NEAR(a.rot[0], b.rot[0], 0.0001);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        new_star_State s;
        new_star_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 17);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            new_star_step(&s, &in, ARC_DT);
            CHECK(s.n >= 1 && s.n <= NS_MAX_STARS);
            CHECK(s.score >= 0 && s.score <= s.n);
            CHECK(s.n - s.score <= 1);
            for (int j = 0; j < s.n; j++)
                CHECK(isfinite(s.x[j]) && isfinite(s.y[j]) && isfinite(s.rot[j]) && isfinite(s.shake[j]));
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_first_star_at_480x800);
    RUN(test_star_type_sizes);
    RUN(test_overlay_alpha_vectors);
    RUN(test_new_star_appears_at_1_05_seconds);
    RUN(test_taps_are_ignored_for_the_first_1_4_seconds);
    RUN(test_hit_box_edges);
    RUN(test_clicks_in_the_gutters_are_ignored);
    RUN(test_tapping_an_old_star_ends_the_game);
    RUN(test_tapping_nothing_does_nothing);
    RUN(test_old_star_taps_are_ignored_during_the_overlay);
    RUN(test_no_time_limit_before_the_10th_star);
    RUN(test_10th_star_alarm_and_timeout);
    RUN(test_new_star_resets_the_shake_and_the_alarm);
    RUN(test_all_stars_rotate_150_deg_per_second);
    RUN(test_placement_invariants);
    RUN(test_cap_of_512_stars_ends_the_game);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
