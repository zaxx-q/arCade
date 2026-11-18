#include "arcade_test.h"
#include "tower_stacker_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput none(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static ArcadeInput tap(void) {
    ArcadeInput in = none();
    in.fire_pressed = true;
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
    CHECK(!tower_stacker_can_continue(s)); /* the short reveal is running */
    ArcadeInput in = none();
    for (int i = 0; i < 60 && !tower_stacker_can_continue(s); i++)
        tower_stacker_step(s, &in, ARC_DT);
    CHECK(tower_stacker_can_continue(s));
    CHECK(!tower_stacker_is_over(s)); /* the game waits for the player, it does not close by itself */
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
    CHECK(tower_stacker_can_continue(s));   /* the long reveal is done after 2.5 s */
    /* a short tower has neither the zoom nor the long delay */
    tower_stacker_State *t = fresh(16);
    drop_at(t, 361.0f);
    tower_stacker_step(t, &in, 0.9f);
    CHECK(tower_stacker_can_continue(t));
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
    CHECK(!tower_stacker_can_continue(s));
    for (int i = 0; i < 10; i++)
        tower_stacker_step(s, &in, TICK);
    CHECK(tower_stacker_can_continue(s));
}

/* A scripted player that drops at once every time ends the game by itself: the tower narrows
 * until the next bar misses it entirely. */
static void test_dropping_at_once_ends_the_game(void) {
    tower_stacker_State s;
    tower_stacker_init(&s, 1);
    int frames = 0;
    while (!s.over && frames++ < 60 * 120) {
        ArcadeInput in = none();
        if (s.state == TS_SLIDING)
            in.fire_pressed = true;
        tower_stacker_step(&s, &in, ARC_DT);
    }
    CHECK(s.over);
    CHECK(tower_stacker_score(&s) >= 1);
}

/* ---- the bar enters from a random side ------------------------------------------------------- */

/* Checks the start position of the bar that was just created: which side, and the mod-5 alignment. */
static bool start_is_valid(const tower_stacker_State *s) {
    float r = fmodf(fabsf(s->bar_x - s->base_x), 5.0f);
    if (!(r < 0.001f || r > 4.999f))
        return false; /* must stay congruent to the floor edge, or a perfect is unreachable */
    float right_edge = (float)s->w - s->bar_w;
    if (s->bar_spx > 0.0f) /* from the left */
        return s->bar_x >= 0.0f && s->bar_x < 5.0f;
    return s->bar_x <= right_edge && s->bar_x > right_edge - 5.0f; /* from the right */
}

static void test_bars_enter_from_both_sides(void) {
    int left = 0, right = 0;
    for (unsigned seed = 1; seed <= 40; seed++) {
        tower_stacker_State *s = fresh(seed);
        CHECK(start_is_valid(s));
        if (s->bar_spx > 0.0f)
            left++;
        else
            right++;
    }
    CHECK(left >= 8 && right >= 8); /* a fair coin: neither side is ever starved over 40 bars */
}

/* After cuts the tower is narrower and off-centre: the right-hand start must follow the new width. */
static void test_every_new_bar_starts_aligned_on_either_side(void) {
    int left = 0, right = 0;
    for (unsigned seed = 1; seed <= 20; seed++) {
        tower_stacker_State *s = fresh(seed);
        for (int i = 0; i < 12 && !s->over; i++) {
            float off = (i % 2 == 0) ? 25.0f : -25.0f; /* alternate right and left cuts */
            drop_at(s, s->base_x + off);
            wait_for_new_bar(s);
            if (s->over)
                break;
            CHECK(start_is_valid(s));
            if (s->bar_spx > 0.0f)
                left++;
            else
                right++;
        }
    }
    CHECK(left >= 20 && right >= 20);
}

/* A bar that starts at the right edge moves left, bounces off the left wall and keeps its alignment. */
static void test_a_right_start_moves_left_and_bounces(void) {
    tower_stacker_State *s = fresh(1);
    s->bar_spx = -5.0f; /* force a right start for a known position */
    s->bar_x = (float)s->w - s->bar_w;
    ArcadeInput in = none();
    float first = s->bar_x;
    tower_stacker_step(s, &in, TICK);
    CHECK_NEAR(s->bar_x, first - 5.0, 0.001);
    bool went_right_again = false;
    for (int i = 0; i < 400; i++) {
        float before = s->bar_x;
        tower_stacker_step(s, &in, TICK);
        if (s->bar_x > before)
            went_right_again = true;
        float r = fmodf(fabsf(s->bar_x - s->base_x), 5.0f);
        CHECK(r < 0.001f || r > 4.999f);
    }
    CHECK(went_right_again);
}

/* The 15-perfect scripted run of the 60 Hz test, repeated over many seeds so that both entry sides
 * are exercised: a perfect landing is reachable whichever side the bar comes from. */
static void test_perfects_are_reachable_from_both_sides(void) {
    int left = 0, right = 0;
    for (unsigned seed = 1; seed <= 8; seed++) {
        tower_stacker_State s;
        tower_stacker_init(&s, seed);
        int prev = -1, frames = 0;
        while (s.score < 12 && !s.over && frames++ < 60 * 600) {
            if (s.state == TS_SLIDING && prev != TS_SLIDING) { /* a new bar just entered */
                if (s.bar_spx > 0.0f)
                    left++;
                else
                    right++;
            }
            prev = s.state;
            ArcadeInput in = none();
            if (s.state == TS_SLIDING && fabsf(s.bar_x - s.base_x) < 1.0f)
                in.fire_pressed = true;
            tower_stacker_step(&s, &in, ARC_DT);
        }
        CHECK_EQ_INT(s.score, 12);
        CHECK_EQ_INT(s.perfects, 12);
        CHECK_EQ_INT(s.cuts, 0);
    }
    CHECK(left >= 10 && right >= 10);
}

/* ---- after the game: look at the tower, tap to continue ------------------------------------------ */

static void test_a_short_tower_waits_for_a_tap(void) {
    tower_stacker_State *s = fresh(18);
    drop_at(s, 361.0f); /* miss at once: 0 floors */
    CHECK(s->over);
    ArcadeInput in = tap();
    tower_stacker_step(s, &in, 0.1f); /* a tap during the reveal (0.8 s) is ignored */
    CHECK(!tower_stacker_can_continue(s));
    CHECK(!tower_stacker_is_over(s));
    in = none();
    for (int i = 0; i < 600; i++) /* 10 s of doing nothing: the tower stays on screen */
        tower_stacker_step(s, &in, ARC_DT);
    CHECK(tower_stacker_can_continue(s));
    CHECK(!tower_stacker_is_over(s));
    in = tap();
    tower_stacker_step(s, &in, ARC_DT);
    CHECK(tower_stacker_is_over(s));
    CHECK(!tower_stacker_can_continue(s)); /* the prompt is gone once the player has continued */
}

static void test_a_tall_tower_zooms_then_waits_for_a_tap(void) {
    tower_stacker_State *s = fresh(19);
    for (int i = 0; i < 10; i++) {
        drop_at(s, s->base_x);
        wait_for_new_bar(s);
    }
    drop_at(s, 361.0f);
    CHECK(s->over);
    ArcadeInput in = tap();
    for (int i = 0; i < 100; i++) /* tapping all through the 2.5 s zoom does not skip it */
        tower_stacker_step(s, &in, TICK);
    CHECK(!tower_stacker_is_over(s));
    CHECK(!tower_stacker_can_continue(s));
    in = none();
    for (int i = 0; i < 30; i++)
        tower_stacker_step(s, &in, TICK); /* the zoom is over now (2.5 s) */
    CHECK(tower_stacker_can_continue(s));
    float top, h;
    tower_stacker_zoom(s, &top, &h);
    CHECK_NEAR(h, 58.18, 0.05);
    for (int i = 0; i < 500; i++) /* and it stays zoomed out as long as the player looks */
        tower_stacker_step(s, &in, TICK);
    float top2, h2;
    tower_stacker_zoom(s, &top2, &h2);
    CHECK_NEAR(top2, top, 0.001);
    CHECK_NEAR(h2, h, 0.001);
    CHECK(!tower_stacker_is_over(s));
    in = none();
    in.typed = 'x'; /* any key continues, not only the mouse */
    tower_stacker_step(s, &in, TICK);
    CHECK(tower_stacker_is_over(s));
    CHECK_EQ_INT(tower_stacker_score(s), 10); /* the score is the tower you were looking at */
}

static void test_every_drop_input_can_continue(void) {
    for (int k = 0; k < 4; k++) {
        tower_stacker_State *s = fresh(20);
        drop_at(s, 361.0f);
        ArcadeInput in = none();
        tower_stacker_step(s, &in, 1.0f); /* reveal done */
        CHECK(tower_stacker_can_continue(s));
        in = none();
        if (k == 0)
            in.fire_pressed = true;
        else if (k == 1)
            in.action_pressed = true;
        else if (k == 2)
            in.left_pressed = true;
        else
            in.typed = 'z';
        tower_stacker_step(s, &in, ARC_DT);
        CHECK(tower_stacker_is_over(s));
    }
}

static void test_a_full_tower_also_waits_for_a_tap(void) {
    tower_stacker_State *s = fresh(15);
    s->score = TS_MAX_FLOORS;
    drop_at(s, s->base_x); /* the array is full */
    CHECK(s->over && s->capped);
    ArcadeInput in = none();
    tower_stacker_step(s, &in, 3.0f);
    CHECK(tower_stacker_can_continue(s));
    CHECK(!tower_stacker_is_over(s));
    in = tap();
    tower_stacker_step(s, &in, ARC_DT);
    CHECK(tower_stacker_is_over(s));
}

static void test_no_tap_before_the_game_ends_closes_it(void) {
    tower_stacker_State *s = fresh(21);
    ArcadeInput in = tap();
    for (int i = 0; i < 20; i++)
        tower_stacker_step(s, &in, ARC_DT); /* ordinary play: taps only drop bars */
    CHECK(!s->over);
    CHECK(!tower_stacker_is_over(s));
    CHECK(!tower_stacker_can_continue(s));
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
    RUN(test_bars_enter_from_both_sides);
    RUN(test_every_new_bar_starts_aligned_on_either_side);
    RUN(test_a_right_start_moves_left_and_bounces);
    RUN(test_perfects_are_reachable_from_both_sides);
    RUN(test_a_short_tower_waits_for_a_tap);
    RUN(test_a_tall_tower_zooms_then_waits_for_a_tap);
    RUN(test_every_drop_input_can_continue);
    RUN(test_a_full_tower_also_waits_for_a_tap);
    RUN(test_no_tap_before_the_game_ends_closes_it);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
