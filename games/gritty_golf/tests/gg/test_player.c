/* test_player.c - the aim -> power -> shoot state machine and the trail ring buffer */
#include <math.h>
#include <string.h>
#include "test_framework.h"
#include "player.h"
#include "level.h"
#include "vec2.h"

/* an empty, huge world: the ball only falls (nothing to hit) */
static const LevelData SKY = { 99, 0.05f, {100, 100}, {9000, 9000}, 0.0f, false,
                               NULL, 0, NULL, 0, NULL, 0, NULL, 0, NULL, 0 };

static Level *sky(void) { return Level_CreateFromData(&SKY); }

static void tick(Player *p, const Level *lv, bool action, unsigned *ev_acc)
{
    unsigned ev = Player_Update(p, lv, action, PHYSICS_DT);
    if (ev_acc) *ev_acc |= ev;
}

static void test_init_and_reset(void)
{
    Player p;
    Player_Init(&p, V2(12, 34));
    CHECK_NEAR(p.ball.pos.x, 12, 0);
    CHECK_NEAR(p.ball.pos.y, 34, 0);
    CHECK_NEAR(V2_Len(p.ball.vel), 0, 0);
    CHECK_INT(p.state, BALL_STATE_MOVING);
    CHECK_INT(p.strikes, STRIKES_MAX);
    CHECK_INT(p.strokes, 0);
    CHECK_INT(Player_TrailCount(&p), 0);

    p.ball.pos = V2(999, 999); p.ball.vel = V2(5, 5);
    p.state = BALL_STATE_POWER; p.strikes = 0; p.strokes = 3; p.power = 77; p.aim_angle = 2.0f;
    Player_Reset(&p);
    CHECK_NEAR(p.ball.pos.x, 12, 0);
    CHECK_NEAR(V2_Len(p.ball.vel), 0, 0);
    CHECK_INT(p.state, BALL_STATE_MOVING);
    CHECK_INT(p.strikes, 3);
    CHECK_INT(p.strokes, 0);
    CHECK_NEAR(p.power, 0, 0);
    CHECK_NEAR(p.aim_angle, 0, 0);
}

static void test_falls_when_idle(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    for (int i = 0; i < 30; i++) tick(&p, lv, false, NULL);
    CHECK(p.ball.pos.y > 100.0f + 100.0f);        /* gravity pulls it down */
    CHECK_INT(p.state, BALL_STATE_MOVING);
    Level_Destroy(lv);
}

static void test_action_freezes_ball(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    for (int i = 0; i < 10; i++) tick(&p, lv, false, NULL);
    Vec2 where = p.ball.pos;

    unsigned ev = 0;
    tick(&p, lv, true, &ev);                                    /* press: MOVING -> AIMING */
    CHECK_INT(p.state, BALL_STATE_AIMING);
    CHECK(ev & EVT_AIM_START);
    CHECK_NEAR(V2_Len(p.ball.vel), 0, 0);
    CHECK_NEAR(p.aim_angle, 0, 0);

    for (int i = 0; i < 60; i++) tick(&p, lv, false, NULL);     /* it hangs in mid-air */
    CHECK_NEAR(p.ball.pos.x, where.x, 1e-6);
    CHECK_NEAR(p.ball.pos.y, where.y, 1e-6);
    Level_Destroy(lv);
}

static void test_aim_angle_sweeps_and_wraps(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    tick(&p, lv, true, NULL);                                   /* AIMING, angle 0 */

    tick(&p, lv, false, NULL);
    CHECK_NEAR(p.aim_angle, AIM_SPEED * PHYSICS_DT, 1e-6);      /* 270 deg/s -> 4.5 deg per tick */
    for (int i = 0; i < 9; i++) tick(&p, lv, false, NULL);
    CHECK_NEAR(p.aim_angle, 10.0f * AIM_SPEED * PHYSICS_DT, 1e-5);

    float prev = p.aim_angle;
    int wrapped = 0;
    for (int i = 0; i < 100; i++) {                             /* a full turn takes 80 ticks */
        tick(&p, lv, false, NULL);
        CHECK(p.aim_angle >= 0.0f && p.aim_angle < 2.0f * GG_PI);
        if (p.aim_angle < prev) wrapped++;
        prev = p.aim_angle;
    }
    CHECK_INT(wrapped, 1);
    Level_Destroy(lv);
}

static void test_lock_angle_then_power(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    tick(&p, lv, true, NULL);
    for (int i = 0; i < 20; i++) tick(&p, lv, false, NULL);
    float angle = p.aim_angle;

    unsigned ev = 0;
    tick(&p, lv, true, &ev);                                    /* AIMING -> POWER */
    CHECK_INT(p.state, BALL_STATE_POWER);
    CHECK(ev & EVT_ANGLE_LOCK);
    CHECK_NEAR(p.power, 0, 0);

    for (int i = 0; i < 15; i++) tick(&p, lv, false, NULL);
    CHECK_NEAR(p.aim_angle, angle, 1e-6);                       /* the angle is locked */
    CHECK_NEAR(p.power, 15.0f * POWER_SPEED * PHYSICS_DT, 1e-3);/* 200 units/s -> 3.33 per tick */
    Level_Destroy(lv);
}

static void test_power_ping_pong(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    tick(&p, lv, true, NULL);
    tick(&p, lv, true, NULL);                                   /* POWER */

    int top_hits = 0, bottom_hits = 0;
    float prev = 0.0f;
    int direction_changes = 0;
    bool rising = true;
    for (int i = 0; i < 200; i++) {                             /* 3.3 s: ~3 sweeps of 0.5 s */
        unsigned ev = 0;
        tick(&p, lv, false, &ev);
        CHECK(p.power >= 0.0f && p.power <= 100.0f);
        if (ev & EVT_POWER_BOUNCE) {
            if (p.power >= 99.9f) top_hits++; else bottom_hits++;
        }
        bool now_rising = p.power > prev;
        if (now_rising != rising && p.power != prev) { direction_changes++; rising = now_rising; }
        prev = p.power;
    }
    CHECK(top_hits >= 3);
    CHECK(bottom_hits >= 2);
    CHECK(direction_changes >= 5);
    Level_Destroy(lv);
}

static void test_shoot_launches_ball(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    tick(&p, lv, true, NULL);                                   /* aim */
    for (int i = 0; i < 40; i++) tick(&p, lv, false, NULL);
    tick(&p, lv, true, NULL);                                   /* lock angle */
    float angle = p.aim_angle;
    for (int i = 0; i < 9; i++) tick(&p, lv, false, NULL);
    float power = p.power;

    unsigned ev = 0;
    tick(&p, lv, true, &ev);                                    /* SHOOT */
    CHECK(ev & EVT_LAUNCH);
    CHECK_INT(p.state, BALL_STATE_MOVING);
    CHECK_INT(p.strikes, 2);
    CHECK_INT(p.strokes, 1);

    Vec2 want = Physics_LaunchVelocity(angle, power);           /* speed = power * 6, direction = angle */
    CHECK_NEAR(p.ball.vel.x, want.x, 1e-3);
    CHECK_NEAR(p.ball.vel.y, want.y, 1e-3);
    CHECK_NEAR(V2_Len(p.ball.vel), power * 6.0f, 1e-2);
    Level_Destroy(lv);
}

static void shoot_once(Player *p, const Level *lv)
{
    tick(p, lv, true, NULL);                                    /* aim   */
    tick(p, lv, false, NULL);
    tick(p, lv, true, NULL);                                    /* lock  */
    tick(p, lv, false, NULL);
    tick(p, lv, true, NULL);                                    /* shoot */
}

static void test_three_strikes_then_no_more(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    for (int s = 0; s < 3; s++) {
        shoot_once(&p, lv);
        CHECK_INT(p.strikes, 2 - s);
        CHECK_INT(p.strokes, s + 1);
        CHECK_INT(p.state, BALL_STATE_MOVING);
    }
    unsigned ev = 0;
    tick(&p, lv, true, &ev);                                    /* 4th press does nothing */
    CHECK_INT(p.state, BALL_STATE_MOVING);
    CHECK_INT(ev & EVT_AIM_START, 0);
    CHECK_INT(p.strikes, 0);

    Player_RefillStrikes(&p);                                   /* a pickup gives them back */
    CHECK_INT(p.strikes, 3);
    tick(&p, lv, true, &ev);
    CHECK_INT(p.state, BALL_STATE_AIMING);
    Level_Destroy(lv);
}

static void test_trail_ring_buffer(void)
{
    Level *lv = sky();
    Player p; Player_Init(&p, V2(100, 100));
    p.ball.vel = V2(100, 0);

    tick(&p, lv, false, NULL);
    CHECK_INT(Player_TrailCount(&p), 0);                        /* sampled every 2nd tick */
    tick(&p, lv, false, NULL);
    CHECK_INT(Player_TrailCount(&p), 1);
    for (int i = 0; i < 6; i++) tick(&p, lv, false, NULL);
    CHECK_INT(Player_TrailCount(&p), 4);

    for (int i = 0; i < 100; i++) tick(&p, lv, false, NULL);    /* saturates at TRAIL_LEN */
    CHECK_INT(Player_TrailCount(&p), TRAIL_LEN);

    for (int i = 1; i < TRAIL_LEN; i++)                         /* oldest -> newest = increasing x */
        CHECK(Player_TrailPoint(&p, i).x > Player_TrailPoint(&p, i - 1).x);
    CHECK(Player_TrailPoint(&p, TRAIL_LEN - 1).x <= p.ball.pos.x);
    CHECK(p.ball.pos.x - Player_TrailPoint(&p, TRAIL_LEN - 1).x < 10.0f);   /* newest is close to the ball */
    Level_Destroy(lv);
}

static void test_deterministic(void)
{
    Level *lv = sky();
    Player a, b;
    Player_Init(&a, V2(100, 100)); Player_Init(&b, V2(100, 100));
    for (int i = 0; i < 400; i++) {
        bool press = (i == 5 || i == 20 || i == 33 || i == 100 || i == 140 || i == 150 || i == 260);
        tick(&a, lv, press, NULL);
        tick(&b, lv, press, NULL);
    }
    CHECK(memcmp(&a.ball, &b.ball, sizeof a.ball) == 0);        /* same inputs => bit-identical result */
    CHECK_INT(a.strikes, b.strikes);
    Level_Destroy(lv);
}

void run_player_tests(void)
{
    RUN(test_init_and_reset);
    RUN(test_falls_when_idle);
    RUN(test_action_freezes_ball);
    RUN(test_aim_angle_sweeps_and_wraps);
    RUN(test_lock_angle_then_power);
    RUN(test_power_ping_pong);
    RUN(test_shoot_launches_ball);
    RUN(test_three_strikes_then_no_more);
    RUN(test_trail_ring_buffer);
    RUN(test_deterministic);
}
