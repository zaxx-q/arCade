#include <stdio.h>

#include "arcade/playfield.h"
#include "arcade_test.h"
#include "master_juggler_logic.h"

#define TMP_FILE "master_juggler_progress_test.tmp" /* *.tmp is git-ignored */

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

/* pointer at a playfield-LOCAL x (the y does not matter), optionally with a click */
static ArcadeInput at(float lx, bool click) {
    ArcadeInput in;
    arcade_input_clear(&in);
    Vec2 p = arc_pf_to_screen(lx, 300.0f);
    in.aim_x = p.x;
    in.aim_y = p.y;
    in.fire_pressed = click;
    return in;
}

static master_juggler_State *fresh(unsigned seed) {
    static master_juggler_State s;
    master_juggler_init_sized(&s, seed, 480, 800);
    return &s;
}

static void start_level(master_juggler_State *s, float bar_x) {
    ArcadeInput in = at(bar_x, true);
    master_juggler_step(s, &in, TICK);
}

/* A scripted player: the bar follows the lowest falling ball. */
static void follow_lowest_ball(master_juggler_State *s, int ticks) {
    for (int t = 0; t < ticks && !s->waiting && !s->over; t++) {
        float target = s->bar_x;
        float lowest = -1.0e9f;
        for (int i = 0; i < s->nb; i++)
            if (s->visible[i] && s->y[i] > lowest) {
                lowest = s->y[i];
                target = s->x[i];
            }
        ArcadeInput in = at(target, false);
        master_juggler_step(s, &in, TICK);
    }
}

static void test_init_is_sane(void) {
    master_juggler_State s;
    master_juggler_init(&s, 1);
    CHECK(!master_juggler_is_over(&s));
    CHECK_EQ_INT(master_juggler_score(&s), 0);
    CHECK_EQ_INT(s.level, 1);
    CHECK_EQ_INT(s.lives, MJ_LIVES);
    CHECK(s.waiting);
}

static void test_spec_constants_at_480x800(void) {
    master_juggler_State *s = fresh(1);
    CHECK_NEAR(s->base_h, 133.33, 0.01);
    CHECK_NEAR(s->bar_y, 634.67, 0.01);
    CHECK_NEAR(s->gh, 1.58476, 0.0001);
    CHECK_NEAR(s->gh * -12.0f, -19.017, 0.001); /* launch speed */
    CHECK_NEAR(s->gh * 0.2f, 0.31695, 0.0001);  /* gravity */
}

static void test_waiting_until_click_or_space(void) {
    master_juggler_State *s = fresh(1);
    ArcadeInput in = at(240.0f, false);
    for (int i = 0; i < 100; i++)
        master_juggler_step(s, &in, TICK);
    CHECK(s->waiting);
    CHECK_EQ_INT(s->nb, 0);
    in.action_pressed = true; /* Space starts too */
    master_juggler_step(s, &in, TICK);
    CHECK(!s->waiting);
}

static void test_launch_schedule_level_3(void) {
    CHECK_EQ_INT(master_juggler_launch_tick(3, 0), 0);
    CHECK_EQ_INT(master_juggler_launch_tick(3, 1), 33);
    CHECK_EQ_INT(master_juggler_launch_tick(3, 2), 67);
    master_juggler_State *s = fresh(2);
    s->level = 3;
    start_level(s, 240.0f);
    ArcadeInput in = at(240.0f, false);
    int tick = 0, launched[3] = {0, 0, 0};
    while (s->nb < 3 && tick < 200) {
        master_juggler_step(s, &in, TICK);
        tick++;
        for (int b = 0; b < s->nb; b++)
            if (!launched[b])
                launched[b] = tick;
    }
    /* start_level used no tick of the clock, so ball0 appears on tick 10, then +33, +67 */
    CHECK_EQ_INT(launched[0], 10);
    CHECK_EQ_INT(launched[1], 43);
    CHECK_EQ_INT(launched[2], 77);
}

static void test_level_n_throws_n_balls(void) {
    for (int level = 1; level <= 5; level++) {
        master_juggler_State *s = fresh(3);
        s->level = level;
        start_level(s, 240.0f);
        ArcadeInput in = at(240.0f, false);
        for (int i = 0; i < 150; i++)
            master_juggler_step(s, &in, TICK);
        CHECK_EQ_INT(s->nb, level);
    }
}

static void test_free_flight_apex(void) {
    master_juggler_State *s = fresh(4);
    start_level(s, 240.0f);
    ArcadeInput in = at(240.0f, false);
    float launch_y = 0.0f, apex = 1.0e9f;
    int ticks_to_apex = 0, t = 0;
    while (t++ < 200) {
        master_juggler_step(s, &in, TICK);
        if (s->nb == 1 && launch_y == 0.0f)
            launch_y = s->y[0];
        if (s->nb == 1 && s->spy[0] < 0.0f) {
            if (s->y[0] < apex)
                apex = s->y[0];
            ticks_to_apex++;
        } else if (s->nb == 1 && s->spy[0] >= 0.0f && ticks_to_apex > 0) {
            break;
        }
    }
    CHECK_NEAR(launch_y, 679.67 - 19.017 + 0.31695, 0.1); /* launch point 679.67 plus the first tick */
    /* spec: apex ~109 is the continuous v^2/(2g); the original integrates per tick (spy += g, then
     * y += spy), which stops v/2 ~ 9.5 px short: 118.7 */
    CHECK_NEAR(apex, 118.7, 0.5);
    CHECK(ticks_to_apex >= 58 && ticks_to_apex <= 62); /* spec: ~60 ticks */
}

static void test_bounce_formula_and_offsets(void) {
    for (int offset = 30; offset <= 59; offset += 29) {
        master_juggler_State *s = fresh(5);
        start_level(s, 240.0f);
        s->waiting = false;
        s->nb = 1;
        s->mj = 1;
        s->visible[0] = true;
        s->bar_x = 200.0f;
        s->x[0] = 200.0f + (float)offset;
        s->y[0] = s->bar_y - 5.0f;
        s->spx[0] = 0.0f;
        s->spy[0] = 6.0f;
        s->frmball = -100.0f; /* no more launches */
        ArcadeInput in = at(200.0f, false);
        master_juggler_step(s, &in, TICK);
        CHECK_EQ_INT(s->bounces[0], 1);
        CHECK(s->spy[0] < 0.0f);
        CHECK_NEAR(s->spy[0], -(0.2 * 1.58476) - (6.0 + 0.2 * 1.58476), 0.001); /* gh*-0.2 - spy */
        CHECK(s->spx[0] >= (float)(offset - 1) / 10.0f - 0.2f &&
              s->spx[0] <= (float)(offset + 1) / 10.0f + 0.2f);
    }
}

static void test_no_bounce_outside_the_bar_width(void) {
    master_juggler_State *s = fresh(6);
    start_level(s, 240.0f);
    s->nb = 1;
    s->mj = 1;
    s->visible[0] = true;
    s->bar_x = 200.0f;
    s->x[0] = 200.0f + 61.0f; /* |dx| must be < 60 */
    s->y[0] = s->bar_y - 5.0f;
    s->spy[0] = 6.0f;
    s->frmball = -100.0f;
    ArcadeInput in = at(200.0f, false);
    master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->bounces[0], 0);
}

static void test_wall_bounce_speeds_up(void) {
    master_juggler_State *s = fresh(7);
    start_level(s, 240.0f);
    s->nb = 1;
    s->visible[0] = true;
    s->x[0] = 14.0f;
    s->y[0] = 100.0f;
    s->spx[0] = -1.0f;
    s->spy[0] = -5.0f; /* rising: no other rule applies */
    s->frmball = -100.0f;
    ArcadeInput in = at(300.0f, false);
    master_juggler_step(s, &in, TICK);
    CHECK_NEAR(s->x[0], 15.0, 0.001);
    CHECK_NEAR(s->spx[0], -0.98 * -2.5, 0.001); /* damped by 0.98, then x -2.5 */
}

static void test_damping_is_098_per_tick(void) {
    master_juggler_State *s = fresh(8);
    start_level(s, 240.0f);
    s->nb = 1;
    s->visible[0] = true;
    s->x[0] = 240.0f;
    s->y[0] = 300.0f;
    s->spx[0] = 1.0f;
    s->spy[0] = -5.0f;
    s->frmball = -100.0f;
    ArcadeInput in = at(300.0f, false);
    master_juggler_step(s, &in, TICK);
    CHECK_NEAR(s->spx[0], 0.98, 0.0001);
    /* and at 60 Hz the same second of damping gives the same factor: 0.98^50 */
    master_juggler_State *a = fresh(8);
    start_level(a, 240.0f);
    a->nb = 1;
    a->visible[0] = true;
    a->x[0] = 240.0f;
    a->y[0] = 100.0f;
    a->spx[0] = 1.0f;
    a->spy[0] = -500.0f; /* never reaches anything */
    a->frmball = -1000.0f;
    for (int i = 0; i < 60; i++)
        master_juggler_step(a, &in, ARC_DT);
    CHECK_NEAR(a->spx[0], powf(0.98f, 50.0f), 0.002);
}

static void test_colour_stages_by_level(void) {
    /* level 1: 3 bounces: yellow at 1, green at 2, grey at 3 */
    CHECK_EQ_INT(master_juggler_stage(0, 1), MJ_STAGE_RED);
    CHECK_EQ_INT(master_juggler_stage(1, 1), MJ_STAGE_YELLOW);
    CHECK_EQ_INT(master_juggler_stage(2, 1), MJ_STAGE_GREEN);
    CHECK_EQ_INT(master_juggler_stage(3, 1), MJ_STAGE_GREY);
    /* level 2: 6 bounces: yellow at 2, green at 4, grey at 6 */
    CHECK_EQ_INT(master_juggler_stage(1, 2), MJ_STAGE_RED);
    CHECK_EQ_INT(master_juggler_stage(2, 2), MJ_STAGE_YELLOW);
    CHECK_EQ_INT(master_juggler_stage(4, 2), MJ_STAGE_GREEN);
    CHECK_EQ_INT(master_juggler_stage(6, 2), MJ_STAGE_GREY);
    /* level 3 and up: 9 bounces: 3, 6, 9 */
    CHECK_EQ_INT(master_juggler_stage(2, 3), MJ_STAGE_RED);
    CHECK_EQ_INT(master_juggler_stage(3, 3), MJ_STAGE_YELLOW);
    CHECK_EQ_INT(master_juggler_stage(6, 9), MJ_STAGE_GREEN);
    CHECK_EQ_INT(master_juggler_stage(9, 20), MJ_STAGE_GREY);
}

static void test_a_following_player_completes_level_1(void) {
    master_juggler_State *s = fresh(9);
    start_level(s, 240.0f);
    follow_lowest_ball(s, 5000);
    CHECK(s->waiting);
    CHECK_EQ_INT(s->best, 1);
    CHECK_EQ_INT(s->level, 2);
    CHECK_EQ_INT(master_juggler_score(s), 1);
    CHECK_EQ_INT(s->lives, MJ_LIVES);
    CHECK_EQ_INT(s->levels_done, 1);
    CHECK(s->hits >= 3);
}

static void test_a_following_player_completes_level_3(void) {
    master_juggler_State *s = fresh(10);
    s->level = 3;
    start_level(s, 240.0f);
    follow_lowest_ball(s, 20000);
    /* the bot may lose a ball of three, but with a perfect follower level 3 must complete or cost a life */
    CHECK(s->waiting || s->over);
    CHECK(s->best == 3 || s->lives < MJ_LIVES);
}

static void test_first_lost_ball_turns_everything_grey_and_stops_bouncing(void) {
    master_juggler_State *s = fresh(11);
    s->level = 2;
    start_level(s, 100.0f);
    /* bar stays at x = 100 for the first ball, then leaves */
    ArcadeInput in = at(100.0f, false);
    for (int i = 0; i < 12 && s->nb < 1; i++)
        master_juggler_step(s, &in, TICK);
    in = at(450.0f, false); /* the bar runs away: the first ball falls through */
    for (int i = 0; i < 400 && !s->first_lost; i++)
        master_juggler_step(s, &in, TICK);
    CHECK(s->first_lost);
    CHECK_EQ_INT(s->losses, 1);
    CHECK_EQ_INT(s->bl, 1);
    CHECK(!s->waiting); /* one ball of two is still falling: the try is not over yet */
    int hits = s->hits;
    in = at(s->x[1], false); /* even a perfect catch no longer bounces */
    for (int i = 0; i < 200 && !s->waiting; i++) {
        in = at(s->x[1], false);
        master_juggler_step(s, &in, TICK);
    }
    CHECK_EQ_INT(s->hits, hits);
}

static void test_all_balls_lost_costs_a_life_and_repeats_the_level(void) {
    master_juggler_State *s = fresh(12);
    start_level(s, 100.0f);
    ArcadeInput in = at(100.0f, false);
    for (int i = 0; i < 12 && s->nb < 1; i++)
        master_juggler_step(s, &in, TICK);
    in = at(450.0f, false);
    for (int i = 0; i < 500 && !s->waiting; i++)
        master_juggler_step(s, &in, TICK);
    CHECK(s->waiting);
    CHECK_EQ_INT(s->lives, MJ_LIVES - 1);
    CHECK_EQ_INT(s->level, 1); /* same level again */
    CHECK_EQ_INT(s->tries_lost, 1);
    CHECK(!s->over);
    in = at(240.0f, true); /* retry */
    master_juggler_step(s, &in, TICK);
    CHECK(!s->waiting);
    CHECK_EQ_INT(s->nb, 0);
    CHECK_EQ_INT(s->bl, 0);
}

static void test_zero_lives_ends_the_game_with_the_best_level(void) {
    master_juggler_State *s = fresh(13);
    s->best = 4; /* pretend four levels were completed */
    s->level = 5;
    for (int life = 0; life < MJ_LIVES; life++) {
        ArcadeInput in = at(100.0f, true);
        master_juggler_step(s, &in, TICK);
        in = at(100.0f, false);
        for (int i = 0; i < 12 && s->nb < 1; i++)
            master_juggler_step(s, &in, TICK);
        in = at(450.0f, false);
        for (int i = 0; i < 3000 && !s->waiting && !s->over; i++)
            master_juggler_step(s, &in, TICK);
    }
    CHECK(s->over);
    CHECK(!s->capped);
    CHECK_EQ_INT(s->lives, 0);
    CHECK(!master_juggler_is_over(&s[0])); /* reveal delay */
    ArcadeInput in = at(450.0f, false);
    for (int i = 0; i < 100 && !master_juggler_is_over(s); i++)
        master_juggler_step(s, &in, ARC_DT);
    CHECK(master_juggler_is_over(s));
    CHECK_EQ_INT(master_juggler_score(s), 4);
}

static void test_level_32_completion_ends_the_game(void) {
    master_juggler_State *s = fresh(14);
    s->level = MJ_MAX_LEVEL;
    start_level(s, 240.0f);
    s->nb = MJ_MAX_BALLS;
    s->mj = 3;
    s->frmball = -1000.0f;
    for (int i = 0; i < MJ_MAX_BALLS; i++) {
        s->visible[i] = true;
        s->x[i] = 240.0f;
        s->y[i] = 100.0f;
        s->spx[i] = 0.0f;
        s->spy[i] = -50.0f; /* far from everything */
        s->bounces[i] = 9;
        s->alpha[i] = 0.005f;
    }
    ArcadeInput in = at(240.0f, false);
    master_juggler_step(s, &in, TICK);
    CHECK(s->over && s->capped);
    CHECK_EQ_INT(s->best, MJ_MAX_LEVEL);
    CHECK_EQ_INT(s->level, MJ_MAX_LEVEL);
}

static void test_hover_and_keyboard_control_of_the_bar(void) {
    master_juggler_State *s = fresh(15);
    ArcadeInput in = at(100.0f, false);
    master_juggler_step(s, &in, TICK); /* first frame adopts the pointer */
    CHECK_NEAR(s->bar_x, 100.0, 0.001);
    in = at(300.0f, false); /* pointer moved: the bar follows without a click */
    master_juggler_step(s, &in, TICK);
    CHECK_NEAR(s->bar_x, 300.0, 0.001);
    in.move_x = 1.0f; /* pointer still, key held: the bar moves */
    master_juggler_step(s, &in, ARC_DT);
    CHECK_NEAR(s->bar_x, 300.0 + 420.0 * ARC_DT, 0.01);
    in = at(-500.0f, false); /* outside the playfield: clamped so the whole bar stays in the court */
    master_juggler_step(s, &in, TICK);
    CHECK_NEAR(s->bar_x, MJ_BAR_W / 2.0, 0.001);
    in = at(900.0f, false);
    master_juggler_step(s, &in, TICK);
    CHECK_NEAR(s->bar_x, 480.0 - MJ_BAR_W / 2.0, 0.001);
    in = at(200.0f, false);
    in.move_x = 1.0f; /* the pointer moved this frame, so the held key is ignored */
    master_juggler_step(s, &in, ARC_DT);
    CHECK_NEAR(s->bar_x, 200.0, 0.001);
    master_juggler_step(s, &in, ARC_DT); /* the pointer is still now: the key moves the bar */
    CHECK_NEAR(s->bar_x, 200.0 + 420.0 * ARC_DT, 0.01);
}

/* ---- the "a ball was lost: try again" hint ------------------------------------------------------- */

/* Loses the current try on purpose: the bar runs to the corner and every ball falls through. */
static void lose_a_try(master_juggler_State *s) {
    ArcadeInput in = at(100.0f, true);
    master_juggler_step(s, &in, TICK); /* start */
    in = at(100.0f, false);
    for (int i = 0; i < 12 && s->nb < 1; i++)
        master_juggler_step(s, &in, TICK);
    in = at(450.0f, false);
    for (int i = 0; i < 3000 && !s->waiting && !s->over; i++)
        master_juggler_step(s, &in, TICK);
}

/* Bug fixed: the hint used to depend on "lives < 3", so it stayed on screen for the rest of the run. */
static void test_try_again_hint_is_only_for_the_try_that_was_just_lost(void) {
    int levels_won_after_a_loss = 0;
    for (unsigned seed = 1; seed <= 30; seed++) {
        master_juggler_State *s = fresh(seed);
        CHECK(!s->last_try_failed); /* nothing lost yet */
        lose_a_try(s);
        CHECK(s->waiting && s->lives == MJ_LIVES - 1);
        CHECK(s->last_try_failed); /* the hint is shown on the retry screen ... */
        start_level(s, 240.0f);
        CHECK(!s->last_try_failed); /* ... and gone the moment the retry starts */
        follow_lowest_ball(s, 5000);
        if (s->waiting && s->best == 1) { /* the retry was won: level 2 is next, a life is still missing */
            levels_won_after_a_loss++;
            CHECK(s->lives < MJ_LIVES);
            CHECK(!s->last_try_failed); /* the old code showed the hint here */
            CHECK_EQ_INT(s->level, 2);
        }
    }
    CHECK(levels_won_after_a_loss >= 5); /* the scenario really happened for many seeds */
}

static void test_try_again_hint_returns_after_the_next_lost_try(void) {
    master_juggler_State *s = fresh(3);
    lose_a_try(s);
    CHECK(s->last_try_failed);
    s->lives = MJ_LIVES; /* pretend the player recovered */
    s->level = 3;        /* another level */
    s->last_try_failed = false;
    lose_a_try(s);
    CHECK(s->last_try_failed); /* lost again: the hint is back */
    CHECK_EQ_INT(s->lives, MJ_LIVES - 1);
}

/* ---- continuing from the saved level ---------------------------------------------------------------- */

static void test_resume_starts_at_the_saved_level_and_counts_earlier_ones(void) {
    master_juggler_State *s = fresh(1);
    master_juggler_resume(s, 6);
    CHECK_EQ_INT(s->level, 6);
    CHECK_EQ_INT(s->resume_level, 6);
    CHECK_EQ_INT(s->best, 5); /* levels 1..5 were completed in earlier runs */
    CHECK_EQ_INT(master_juggler_score(s), 5);
    CHECK(s->waiting && s->fresh);
    start_level(s, 240.0f);
    ArcadeInput in = at(240.0f, false);
    for (int i = 0; i < 400; i++)
        master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->nb, 6); /* level 6 throws 6 balls: no replay of levels 1..5 */
    CHECK_EQ_INT(s->mj, 3);
}

static void test_resume_is_clamped_and_level_1_changes_nothing(void) {
    master_juggler_State *s = fresh(1);
    master_juggler_resume(s, 0);
    CHECK_EQ_INT(s->level, 1);
    CHECK_EQ_INT(s->best, 0);
    master_juggler_resume(s, 99);
    CHECK_EQ_INT(s->level, MJ_MAX_LEVEL);
    CHECK_EQ_INT(s->resume_level, MJ_MAX_LEVEL);
    master_juggler_resume(s, 1);
    CHECK_EQ_INT(s->level, 1);
    CHECK_EQ_INT(s->best, 0);
}

static void test_start_level_can_be_lowered_but_never_above_the_saved_level(void) {
    master_juggler_State *s = fresh(2);
    master_juggler_resume(s, 5);
    ArcadeInput in = at(240.0f, false);
    in.down_pressed = true;
    master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 4);
    CHECK_EQ_INT(s->best, 3); /* the score follows the chosen start level */
    in = at(240.0f, false);
    in.wheel = -1; /* the wheel works too */
    master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 3);
    in = at(240.0f, false);
    in.up_pressed = true;
    for (int i = 0; i < 5; i++) /* Up goes back, then stops at the saved level */
        master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 5);
    CHECK_EQ_INT(s->best, 4);
    in = at(240.0f, false);
    in.down_pressed = true;
    for (int i = 0; i < 10; i++) /* all the way down: from scratch is still possible */
        master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 1);
    CHECK_EQ_INT(s->best, 0);
    CHECK_EQ_INT(s->resume_level, 5); /* the saved level itself is not lowered by browsing */
    CHECK(s->waiting && s->nb == 0);  /* choosing never starts the level */
}

/* Regression: idle frames on the start screen must not touch the chosen level or the carried score. */
static void test_idle_start_screen_changes_nothing(void) {
    master_juggler_State *s = fresh(9);
    master_juggler_resume(s, 5);
    ArcadeInput in = at(240.0f, false);
    in.down_pressed = true;
    master_juggler_step(s, &in, TICK); /* the player picks level 4 */
    in = at(240.0f, false);
    for (int i = 0; i < 200; i++)
        master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 4);
    CHECK_EQ_INT(s->best, 3);
    master_juggler_State *t = fresh(9); /* a state that was set up directly (as the other tests do) */
    t->level = 3;
    t->best = 2;
    for (int i = 0; i < 50; i++)
        master_juggler_step(t, &in, TICK);
    CHECK_EQ_INT(t->level, 3);
    CHECK_EQ_INT(t->best, 2);
}

static void test_start_level_is_locked_once_a_try_has_started(void) {
    master_juggler_State *s = fresh(4);
    master_juggler_resume(s, 5);
    start_level(s, 240.0f);
    CHECK(!s->fresh);
    ArcadeInput in = at(240.0f, false);
    in.down_pressed = true;
    master_juggler_step(s, &in, TICK);
    CHECK_EQ_INT(s->level, 5);
    /* and still locked on the retry screen after a lost try */
    master_juggler_State *t = fresh(4);
    master_juggler_resume(t, 5);
    lose_a_try(t);
    CHECK(t->waiting);
    in = at(240.0f, false);
    in.down_pressed = true;
    master_juggler_step(t, &in, TICK);
    CHECK_EQ_INT(t->level, 5);
    CHECK_EQ_INT(t->best, 4);
}

static void test_a_resumed_run_keeps_the_carried_score_when_it_fails(void) {
    master_juggler_State *s = fresh(5);
    master_juggler_resume(s, 4);
    for (int life = 0; life < MJ_LIVES; life++)
        lose_a_try(s);
    CHECK(s->over);
    CHECK_EQ_INT(master_juggler_score(s), 3);          /* levels 1..3 still count */
    CHECK_EQ_INT(master_juggler_progress_level(s), 4); /* resume at the level that was lost */
}

static void test_progress_level_never_goes_down_and_follows_new_levels(void) {
    master_juggler_State *s = fresh(6);
    CHECK_EQ_INT(master_juggler_progress_level(s), 1); /* a new player */
    master_juggler_resume(s, 6);
    ArcadeInput in = at(240.0f, false);
    in.wheel = -1;
    for (int i = 0; i < 10; i++)
        master_juggler_step(s, &in, TICK); /* the player picks level 1 for practice */
    CHECK_EQ_INT(s->level, 1);
    CHECK_EQ_INT(master_juggler_progress_level(s), 6); /* the saved progress is kept */
    s->level = 8;                                      /* later in the run: beyond the old progress */
    CHECK_EQ_INT(master_juggler_progress_level(s), 8);
}

/* ---- the progress file -------------------------------------------------------------------------- */

static void write_text(const char *text) {
    FILE *fp = fopen(TMP_FILE, "w");
    if (fp != NULL) {
        fputs(text, fp);
        fclose(fp);
    }
}

static void test_progress_file_round_trip(void) {
    remove(TMP_FILE);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ANA"), 1); /* no file: from scratch */
    CHECK(master_juggler_progress_save(TMP_FILE, "ANA", 7));
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ANA"), 7);
    CHECK(master_juggler_progress_save(TMP_FILE, "BUDI", 3));
    CHECK(master_juggler_progress_save(TMP_FILE, "ANA", 9)); /* updates, does not duplicate */
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ANA"), 9);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "BUDI"), 3);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "CARA"), 1); /* unknown player */
    FILE *fp = fopen(TMP_FILE, "r");
    CHECK(fp != NULL);
    int lines = 0;
    char buf[128];
    while (fgets(buf, sizeof(buf), fp) != NULL)
        lines++;
    fclose(fp);
    CHECK_EQ_INT(lines, 2); /* one line per player */
    remove(TMP_FILE);
}

static void test_progress_names_are_case_insensitive_and_sanitised(void) {
    remove(TMP_FILE);
    CHECK(master_juggler_progress_save(TMP_FILE, "Ana", 5));
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ANA"), 5);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ana"), 5);
    CHECK(master_juggler_progress_save(TMP_FILE, "a|b\nc", 4)); /* '|' and newline would break the format */
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "a|b\nc"), 4);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "Ana"), 5); /* the other player is untouched */
    CHECK(master_juggler_progress_save(TMP_FILE, NULL, 2));         /* no name: the hub's default */
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "PLAYER"), 2);
    remove(TMP_FILE);
}

static void test_progress_levels_are_clamped(void) {
    remove(TMP_FILE);
    CHECK(master_juggler_progress_save(TMP_FILE, "X", 0));
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "X"), 1);
    CHECK(master_juggler_progress_save(TMP_FILE, "X", 500));
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "X"), MJ_MAX_LEVEL);
    remove(TMP_FILE);
}

/* A damaged or hand-edited file must never crash the game: bad lines are skipped, good ones kept. */
static void test_progress_file_ignores_bad_lines(void) {
    write_text("no separator here\n"
               "|5\n"      /* empty name */
               "BAD|abc\n" /* level not a number */
               "ZERO|0\n"  /* out of range */
               "HUGE|33\n" /* out of range */
               "NEG|-4\n"
               "TRAIL|7x\n" /* trailing junk */
               "GOOD|12\n"
               "\n"
               "LAST|4"); /* no final newline */
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "GOOD"), 12);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "LAST"), 4);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "BAD"), 1);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "ZERO"), 1);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "HUGE"), 1);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "NEG"), 1);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "TRAIL"), 1);
    CHECK(master_juggler_progress_save(TMP_FILE, "GOOD", 13)); /* saving cleans the file up */
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "GOOD"), 13);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "LAST"), 4);
    remove(TMP_FILE);
    /* a very long line and binary junk */
    FILE *fp = fopen(TMP_FILE, "wb");
    CHECK(fp != NULL);
    for (int i = 0; i < 1000; i++)
        fputc('A', fp);
    fputs("|5\n\x01\x02|\xff\nOK|2\n", fp);
    fclose(fp);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "OK"), 2);
    remove(TMP_FILE);
}

static void test_progress_file_forgets_the_oldest_player_when_full(void) {
    remove(TMP_FILE);
    char name[16];
    for (int i = 0; i < 70; i++) { /* more players than the 64 slots */
        snprintf(name, sizeof(name), "P%d", i);
        CHECK(master_juggler_progress_save(TMP_FILE, name, 1 + i % 30));
    }
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "P69"), 1 + 69 % 30);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "P6"), 1 + 6 % 30);
    CHECK_EQ_INT(master_juggler_progress_load(TMP_FILE, "P5"), 1); /* the oldest were dropped */
    remove(TMP_FILE);
}

static void test_progress_save_fails_cleanly_for_a_bad_path(void) {
    CHECK(!master_juggler_progress_save("no_such_folder_xyz/progress.txt", "ANA", 3));
    CHECK_EQ_INT(master_juggler_progress_load("no_such_folder_xyz/progress.txt", "ANA"), 1);
}

/* A whole session through the logic: play, "quit", come back and continue. */
static void test_a_new_session_continues_where_the_last_one_ended(void) {
    remove(TMP_FILE);
    master_juggler_State *a = fresh(7);
    a->level = 5; /* the first session got as far as level 5 */
    a->best = 4;
    CHECK(master_juggler_progress_save(TMP_FILE, "ANA", master_juggler_progress_level(a)));
    master_juggler_State b;
    master_juggler_init_sized(&b, 8, 480, 800);
    master_juggler_resume(&b, master_juggler_progress_load(TMP_FILE, "ANA"));
    CHECK_EQ_INT(b.level, 5);
    CHECK_EQ_INT(master_juggler_score(&b), 4);
    remove(TMP_FILE);
}

/* The monkey presses Up/Down/wheel at random on the start screen: the invariants must hold. */
static void test_monkey_with_a_resumed_run_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        master_juggler_State s;
        master_juggler_init(&s, seed);
        master_juggler_resume(&s, 3 + (int)seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 77);
        for (int i = 0; i < 4000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            master_juggler_step(&s, &in, ARC_DT);
            CHECK(s.level >= 1 && s.level <= MJ_MAX_LEVEL);
            if (s.fresh) {
                CHECK(s.level <= s.resume_level);
                CHECK_EQ_INT(s.best, s.level - 1);
            }
            CHECK(master_juggler_progress_level(&s) >= s.resume_level);
            CHECK(s.best >= 0 && s.best <= MJ_MAX_LEVEL);
        }
    }
}

static void test_same_seed_same_result(void) {
    master_juggler_State a, b;
    master_juggler_init(&a, 9);
    master_juggler_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        master_juggler_step(&a, &ia, ARC_DT);
        master_juggler_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.best, b.best);
    CHECK_EQ_INT(a.lives, b.lives);
    CHECK_NEAR(a.bar_x, b.bar_x, 0.0001);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        master_juggler_State s;
        master_juggler_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 11);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            master_juggler_step(&s, &in, ARC_DT);
            CHECK(s.level >= 1 && s.level <= MJ_MAX_LEVEL);
            CHECK(s.lives >= 0 && s.lives <= MJ_LIVES);
            CHECK(s.nb >= 0 && s.nb <= MJ_MAX_BALLS && s.nb <= s.level);
            CHECK(s.best >= 0 && s.best <= MJ_MAX_LEVEL);
            CHECK(s.bar_x >= MJ_BAR_W / 2.0f && s.bar_x <= (float)s.w - MJ_BAR_W / 2.0f);
            for (int j = 0; j < s.nb; j++)
                CHECK(isfinite(s.x[j]) && isfinite(s.y[j]));
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_spec_constants_at_480x800);
    RUN(test_waiting_until_click_or_space);
    RUN(test_launch_schedule_level_3);
    RUN(test_level_n_throws_n_balls);
    RUN(test_free_flight_apex);
    RUN(test_bounce_formula_and_offsets);
    RUN(test_no_bounce_outside_the_bar_width);
    RUN(test_wall_bounce_speeds_up);
    RUN(test_damping_is_098_per_tick);
    RUN(test_colour_stages_by_level);
    RUN(test_a_following_player_completes_level_1);
    RUN(test_a_following_player_completes_level_3);
    RUN(test_first_lost_ball_turns_everything_grey_and_stops_bouncing);
    RUN(test_all_balls_lost_costs_a_life_and_repeats_the_level);
    RUN(test_zero_lives_ends_the_game_with_the_best_level);
    RUN(test_level_32_completion_ends_the_game);
    RUN(test_hover_and_keyboard_control_of_the_bar);
    RUN(test_try_again_hint_is_only_for_the_try_that_was_just_lost);
    RUN(test_try_again_hint_returns_after_the_next_lost_try);
    RUN(test_resume_starts_at_the_saved_level_and_counts_earlier_ones);
    RUN(test_resume_is_clamped_and_level_1_changes_nothing);
    RUN(test_start_level_can_be_lowered_but_never_above_the_saved_level);
    RUN(test_idle_start_screen_changes_nothing);
    RUN(test_start_level_is_locked_once_a_try_has_started);
    RUN(test_a_resumed_run_keeps_the_carried_score_when_it_fails);
    RUN(test_progress_level_never_goes_down_and_follows_new_levels);
    RUN(test_progress_file_round_trip);
    RUN(test_progress_names_are_case_insensitive_and_sanitised);
    RUN(test_progress_levels_are_clamped);
    RUN(test_progress_file_ignores_bad_lines);
    RUN(test_progress_file_forgets_the_oldest_player_when_full);
    RUN(test_progress_save_fails_cleanly_for_a_bad_path);
    RUN(test_a_new_session_continues_where_the_last_one_ended);
    RUN(test_monkey_with_a_resumed_run_never_breaks_invariants);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
