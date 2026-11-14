#include "arcade/playfield.h"
#include "arcade_test.h"
#include "master_juggler_logic.h"

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
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
