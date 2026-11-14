#include "arcade_test.h"
#include "tower_stacker_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput none(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static tower_stacker_State *fresh(unsigned seed) {
    static tower_stacker_State s;
    tower_stacker_init_sized(&s, seed, 480, 800);
    return &s;
}

/* Puts the sliding bar at x, drops it, and waits until it has landed (or the game ended). */
static void drop_at(tower_stacker_State *s, float x) {
    s->bar_x = x;
    ArcadeInput in = none();
    in.fire_pressed = true;
    tower_stacker_step(s, &in, TICK);
    in = none();
    int guard = 0;
    while (s->state == TS_FALLING && !s->over && guard++ < 2000)
        tower_stacker_step(s, &in, TICK);
}

static void wait_for_new_bar(tower_stacker_State *s) {
    ArcadeInput in = none();
    int guard = 0;
    while (s->state != TS_SLIDING && !s->over && guard++ < 2000)
        tower_stacker_step(s, &in, TICK);
}

static void test_init_is_sane(void) {
    tower_stacker_State s;
    tower_stacker_init(&s, 1);
    CHECK(!tower_stacker_is_over(&s));
    CHECK_EQ_INT(tower_stacker_score(&s), 0);
    CHECK_NEAR(s.base_h0, 34.0, 0.001); /* (int)(684 / 20) */
    CHECK_EQ_INT(s.state, TS_SLIDING);
}

static void test_spec_constants_at_480x800(void) {
    tower_stacker_State *s = fresh(1);
    CHECK_NEAR(s->base_w0, 240.0, 0.001);
    CHECK_NEAR(s->base_h0, 40.0, 0.001);
    CHECK_NEAR(s->base_x0, 120.0, 0.001);
    CHECK_NEAR(s->bar_y0, 200.0, 0.001);
    CHECK_NEAR(s->speed_y, 11.2, 0.001);
    CHECK_EQ_INT(s->bases_x[0], 120);
    CHECK_EQ_INT(s->bases_w[0], 240);
    CHECK_NEAR(s->base_y, 760.0, 0.001); /* floor rect (120, 760)-(360, 800) */
}

static void test_cut_on_the_right_drop_at_150(void) {
    tower_stacker_State *s = fresh(2);
    drop_at(s, 150.0f);
    CHECK_EQ_INT(s->last_cut_class, 2); /* d = 30 */
    CHECK_NEAR(s->base_w, 210.0, 0.001);
    CHECK_NEAR(s->base_x, 150.0, 0.001);
    CHECK_NEAR(s->bar_x, 360.0, 0.001); /* the piece sits at the right end: x 360 */
    CHECK_NEAR(s->bar_w, 30.0, 0.001);
    CHECK_NEAR(s->base_y, 720.0, 0.001);
    CHECK_EQ_INT(s->score, 1);
    CHECK_EQ_INT(s->bases_x[1], 150);
    CHECK_EQ_INT(s->bases_w[1], 210);
    CHECK_EQ_INT(s->state, TS_FADING);
}

static void test_cut_on_the_left_drop_at_105(void) {
    tower_stacker_State *s = fresh(3);
    drop_at(s, 105.0f);
    CHECK_EQ_INT(s->last_cut_class, 1); /* d = 15 */
    CHECK_NEAR(s->base_w, 225.0, 0.001);
    CHECK_NEAR(s->base_x, 120.0, 0.001); /* unchanged */
    CHECK_NEAR(s->bar_x, 105.0, 0.001);  /* the piece stays at bar.x, width 15 */
    CHECK_NEAR(s->bar_w, 15.0, 0.001);
}

static void test_perfect_at_120_4_keeps_the_full_width(void) {
    tower_stacker_State *s = fresh(4);
    drop_at(s, 120.4f);
    CHECK_EQ_INT(s->perfects, 1);
    CHECK_EQ_INT(s->cuts, 0);
    CHECK_NEAR(s->base_w, 240.0, 0.001);
    CHECK_EQ_INT(s->score, 1);
    CHECK_EQ_INT(s->state, TS_SLIDING); /* a new bar, no fading piece */
}

static void test_no_overlap_ends_the_game(void) {
    tower_stacker_State *s = fresh(5);
    drop_at(s, 361.0f); /* d = 241 >= 239 */
    CHECK(s->over);
    CHECK_EQ_INT(s->state, TS_MISSED);
    CHECK_EQ_INT(s->score, 0);
    CHECK(!tower_stacker_is_over(s)); /* the short reveal is running */
    ArcadeInput in = none();
    for (int i = 0; i < 60 && !tower_stacker_is_over(s); i++)
        tower_stacker_step(s, &in, ARC_DT);
    CHECK(tower_stacker_is_over(s));
    /* d = 238 still overlaps by 1 px: not a miss */
    tower_stacker_State *t = fresh(5);
    drop_at(t, 120.0f + 238.0f);
    CHECK(!t->over);
}

static void test_cut_class_boundaries(void) {
    static const float d[4] = {9.9f, 10.0f, 29.9f, 30.0f};
    static const int cls[4] = {0, 1, 1, 2};
    for (int i = 0; i < 4; i++) {
        tower_stacker_State *s = fresh(6);
        drop_at(s, 120.0f + d[i]);
        CHECK_EQ_INT(s->last_cut_class, cls[i]);
    }
}

static void test_tower_scrolls_from_floor_10(void) {
    tower_stacker_State *s = fresh(7);
    for (int i = 0; i < 9; i++) {
        drop_at(s, s->base_x); /* perfect */
        wait_for_new_bar(s);
    }
    CHECK_EQ_INT(s->score, 9);
    CHECK_NEAR(s->base_y, 400.0, 0.001); /* 800 - 10 * 40: the landing line stops rising here */
    drop_at(s, s->base_x);
    CHECK_EQ_INT(s->score, 10);
    CHECK_NEAR(s->base_y, 400.0, 0.001);      /* forever */
    CHECK_NEAR(s->base_y_move, 360.0, 0.001); /* floor 10 starts above the line ... */
    ArcadeInput in = none();
    for (int i = 0; i < 39; i++)
        tower_stacker_step(s, &in, TICK);
    CHECK(s->base_y_move < 400.0f);
    tower_stacker_step(s, &in, TICK);
    tower_stacker_step(s, &in, TICK);
    CHECK(s->base_y_move >= 400.0f); /* ... and scrolls down 40 px in 40 ticks */
    CHECK(s->base_y_move < 402.0f);
}

/* The bar's x always has the same remainder (mod 5) as the floor edge below it. */
static void test_bar_stays_aligned_mod_5_at_60hz(void) {
    tower_stacker_State *s = fresh(8);
    ArcadeInput in = none();
    for (int i = 0; i < 1000; i++) {
        tower_stacker_step(s, &in, ARC_DT);
        float r = fmodf(fabsf(s->bar_x - s->base_x), 5.0f);
        CHECK(r < 0.001f || r > 4.999f);
    }
}

static void test_alignment_survives_cuts_and_random_play(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        tower_stacker_State *s = fresh(seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 90);
        for (int i = 0; i < 4000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            tower_stacker_step(s, &in, ARC_DT);
            if (s->state == TS_SLIDING) {
                float r = fmodf(fabsf(s->bar_x - s->base_x), 5.0f);
                CHECK(r < 0.001f || r > 4.999f);
            }
        }
    }
}

/* A scripted player that taps when the bar is exactly over the tower gets perfects at 60 Hz. */
static void test_scripted_player_gets_perfects_at_60hz(void) {
    tower_stacker_State s;
    tower_stacker_init(&s, 11);
    int frames = 0;
    while (s.score < 15 && !s.over && frames++ < 60 * 600) {
        ArcadeInput in = none();
        if (s.state == TS_SLIDING && fabsf(s.bar_x - s.base_x) < 1.0f)
            in.fire_pressed = true;
        tower_stacker_step(&s, &in, ARC_DT);
    }
    CHECK_EQ_INT(s.score, 15);
    CHECK_EQ_INT(s.perfects, 15);
    CHECK_EQ_INT(s.cuts, 0);
    CHECK_NEAR(s.base_w, 240.0, 0.001);
    CHECK(!s.over);
}

static void test_fade_takes_half_a_second_and_blocks_a_second_drop(void) {
    tower_stacker_State *s = fresh(12);
    drop_at(s, 150.0f);
    CHECK_EQ_INT(s->state, TS_FADING);
    ArcadeInput in = none();
    in.fire_pressed = true;
    float spy = s->bar_spy;
    tower_stacker_step(s, &in, ARC_DT);
    CHECK_NEAR(s->bar_spy, spy, 0.0001); /* a tap during the fade does nothing */
    in = none();
    int frames = 1;
    while (s->state == TS_FADING && frames < 200) {
        tower_stacker_step(s, &in, ARC_DT);
        frames++;
    }
    CHECK(frames >= 29 && frames <= 31); /* ~0.5 s at 60 Hz */
    CHECK_EQ_INT(s->state, TS_SLIDING);
    CHECK_NEAR(s->bar_w, 210.0, 0.001); /* the new bar has the new width */
}

static void test_drop_only_once_per_bar(void) {
    tower_stacker_State *s = fresh(13);
    ArcadeInput in = none();
    in.fire_pressed = true;
    tower_stacker_step(s, &in, TICK);
    float y = s->bar_y;
    in.typed = 'x';
    tower_stacker_step(s, &in, TICK);
    CHECK_NEAR(s->bar_y - y, s->speed_y, 0.001); /* one drop speed, not doubled */
    CHECK_EQ_INT(s->state, TS_FALLING);
}

static void test_windows_follow_the_rules(void) {
    tower_stacker_State *s = fresh(14);
    int with_windows = 0;
    for (int i = 0; i < 8; i++) {
        drop_at(s, s->base_x);
        wait_for_new_bar(s);
    }
    for (int i = 1; i <= 8; i++)
        for (int j = 0; j < TS_WINDOWS; j++) {
            int x = s->window[i][j];
            if (x != 0) {
                with_windows++;
                CHECK(x >= s->bases_x[i] + 10 && x <= s->bases_x[i] + s->bases_w[i] - 20);
            }
        }
    CHECK(with_windows > 0);
    /* narrow floors (< 31 px) get no windows: halve the tower until it is 30 and 15 wide */
    tower_stacker_State *t = fresh(14);
    drop_at(t, 0.0f); /* d = 120: width 120 */
    wait_for_new_bar(t);
    drop_at(t, t->base_x - t->base_w / 2.0f); /* width 60 */
    wait_for_new_bar(t);
    drop_at(t, t->base_x - t->base_w / 2.0f); /* width 30 */
    wait_for_new_bar(t);
    CHECK_EQ_INT(t->bases_w[3], 30);
    for (int j = 0; j < TS_WINDOWS; j++)
        CHECK_EQ_INT(t->window[3][j], 0);
}

static void test_floor_cap_ends_the_game(void) {
    tower_stacker_State *s = fresh(15);
    s->score = TS_MAX_FLOORS - 1;
    drop_at(s, s->base_x); /* lands as floor 1023 */
    CHECK_EQ_INT(s->score, TS_MAX_FLOORS);
    CHECK(!s->over);
    wait_for_new_bar(s);
    drop_at(s, s->base_x); /* the array is full */
    CHECK(s->over && s->capped);
    CHECK_EQ_INT(s->score, TS_MAX_FLOORS);
}

/* Spec: score 10 (11 floors): hfit = (800 - 160) / 11 = 58.18, the tower ends with its top at 160. */
static void test_zoom_to_fit_after_a_tall_game(void) {
    tower_stacker_State *s = fresh(16);
    for (int i = 0; i < 10; i++) {
        drop_at(s, s->base_x);
        wait_for_new_bar(s);
    }
    CHECK_EQ_INT(s->score, 10);
    drop_at(s, 361.0f); /* miss */
    CHECK(s->over);
    float top, h;
    tower_stacker_zoom(s, &top, &h);
    CHECK_NEAR(top, s->base_y_move, 0.001); /* over_t = 0: no zoom yet */
    CHECK_NEAR(h, 40.0, 0.001);
    ArcadeInput in = none();
    for (int i = 0; i < 126; i++) /* 125 ticks = 2.5 s, plus one so float rounding cannot matter */
        tower_stacker_step(s, &in, TICK);
    tower_stacker_zoom(s, &top, &h);
    CHECK_NEAR(top, 160.0, 0.5);
    CHECK_NEAR(h, 58.18, 0.05);
    CHECK_NEAR(top + 11.0 * h, 800.0, 0.6); /* all 11 floors end exactly at the bottom */
    CHECK(tower_stacker_is_over(s));        /* the long reveal is done after 2.5 s */
    /* a short tower has neither the zoom nor the long delay */
    tower_stacker_State *t = fresh(16);
    drop_at(t, 361.0f);
    tower_stacker_step(t, &in, 0.9f);
    CHECK(tower_stacker_is_over(t));
    tower_stacker_zoom(t, &top, &h);
    CHECK_NEAR(h, 40.0, 0.001);
}

static void test_long_reveal_for_tall_towers(void) {
    tower_stacker_State *s = fresh(17);
    for (int i = 0; i < 10; i++) {
        drop_at(s, s->base_x);
        wait_for_new_bar(s);
    }
    drop_at(s, 361.0f);
    ArcadeInput in = none();
    for (int i = 0; i < 120; i++) /* 2.4 s */
        tower_stacker_step(s, &in, TICK);
    CHECK(!tower_stacker_is_over(s));
    for (int i = 0; i < 10; i++)
        tower_stacker_step(s, &in, TICK);
    CHECK(tower_stacker_is_over(s));
}

/* A scripted player that drops at once every time ends the game by itself: the tower narrows
 * until the next bar misses it entirely. */
static void test_dropping_at_once_ends_the_game(void) {
    tower_stacker_State s;
    tower_stacker_init(&s, 1);
    int frames = 0;
    while (!tower_stacker_is_over(&s) && frames++ < 60 * 120) {
        ArcadeInput in = none();
        if (s.state == TS_SLIDING)
            in.fire_pressed = true;
        tower_stacker_step(&s, &in, ARC_DT);
    }
    CHECK(tower_stacker_is_over(&s));
    CHECK(tower_stacker_score(&s) >= 1);
}

static void test_same_seed_same_result(void) {
    static tower_stacker_State a, b;
    tower_stacker_init(&a, 9);
    tower_stacker_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 2000; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        tower_stacker_step(&a, &ia, ARC_DT);
        tower_stacker_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.score, b.score);
    CHECK_NEAR(a.bar_x, b.bar_x, 0.0001);
    CHECK_EQ_INT(a.window[1][0], b.window[1][0]);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        tower_stacker_State *s = fresh(seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 21);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            tower_stacker_step(s, &in, ARC_DT);
            CHECK(s->score >= 0 && s->score <= TS_MAX_FLOORS);
            CHECK(s->base_w > 0.99f && s->base_w <= 240.0f + 0.01f);
            CHECK(isfinite(s->bar_x) && isfinite(s->bar_y));
            CHECK((s->bar_x >= -5.01f && s->bar_x <= 480.0f + 5.01f) || s->state != TS_SLIDING);
            CHECK(s->state >= TS_SLIDING && s->state <= TS_MISSED);
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_spec_constants_at_480x800);
    RUN(test_cut_on_the_right_drop_at_150);
    RUN(test_cut_on_the_left_drop_at_105);
    RUN(test_perfect_at_120_4_keeps_the_full_width);
    RUN(test_no_overlap_ends_the_game);
    RUN(test_cut_class_boundaries);
    RUN(test_tower_scrolls_from_floor_10);
    RUN(test_bar_stays_aligned_mod_5_at_60hz);
    RUN(test_alignment_survives_cuts_and_random_play);
    RUN(test_scripted_player_gets_perfects_at_60hz);
    RUN(test_fade_takes_half_a_second_and_blocks_a_second_drop);
    RUN(test_drop_only_once_per_bar);
    RUN(test_windows_follow_the_rules);
    RUN(test_floor_cap_ends_the_game);
    RUN(test_zoom_to_fit_after_a_tall_game);
    RUN(test_long_reveal_for_tall_towers);
    RUN(test_dropping_at_once_ends_the_game);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
