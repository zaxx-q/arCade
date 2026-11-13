/*
 * player.c  -  ball state machine
 */
#include <math.h>
#include <string.h>
#include "player.h"
#include "vec2.h"

void Player_Init(Player *p, Vec2 spawn)
{
    memset(p, 0, sizeof *p);          /* zero every byte of the struct        */
    p->spawn = spawn;
    Player_Reset(p);
}

/* Back to the start position with 3 strikes (death / restart / new level). */
void Player_Reset(Player *p)
{
    p->ball.pos       = p->spawn;
    p->ball.vel       = V2(0.0f, 0.0f);
    p->state          = BALL_STATE_MOVING;
    p->aim_angle      = 0.0f;
    p->power          = 0.0f;
    p->power_growing  = true;
    p->strikes        = STRIKES_MAX;
    p->strokes        = 0;
    p->trail_head     = 0;
    p->trail_count    = 0;
    p->trail_tick     = 0;
}

void Player_RefillStrikes(Player *p)
{
    p->strikes = STRIKES_MAX;
}

/* ---- trail ring buffer --------------------------------------------- *
 * A ring buffer is an array that wraps around:  after writing the last slot
 * the next write goes back to slot 0, silently overwriting the oldest value.
 * The wrap-around is just the MODULO operator:   (i + 1) % TRAIL_LEN
 */
static void trail_push(Player *p, Vec2 v)
{
    p->trail[p->trail_head] = v;
    p->trail_head = (p->trail_head + 1) % TRAIL_LEN;          /* [T04] modulo */
    if (p->trail_count < TRAIL_LEN)
        p->trail_count++;                                      /* [T04] ++     */
}

int Player_TrailCount(const Player *p)
{
    return p->trail_count;
}

/* i-th point counted from the OLDEST one. */
Vec2 Player_TrailPoint(const Player *p, int i)
{
    int slot = (p->trail_head - p->trail_count + i + TRAIL_LEN) % TRAIL_LEN;
    return p->trail[slot];
}

/*
 * Player_Update : one 1/60 s tick of the ball.
 *
 * [T05 Percabangan]  switch/case on an enum  - one `case` per state.
 *
 *   PSEUDOCODE
 *     CASE state OF
 *       MOVING : IF action AND strikes > 0 THEN freeze ball, state <- AIMING
 *                ELSE                            apply physics
 *       AIMING : IF action THEN lock the angle, state <- POWER
 *                ELSE angle <- angle + 270 deg/s * dt
 *       POWER  : IF action THEN strikes <- strikes-1,
 *                               velocity <- (cos a, sin a) * power * 6,
 *                               state <- MOVING
 *                ELSE power ping-pongs 0..100 at 200 units/s
 *
 *   (The button is checked BEFORE the value advances, so the player gets
 *    exactly the angle / power that was on screen when they pressed.)
 *
 * Returns the GameEvent flags that happened, so game.c can play sounds.
 */
unsigned Player_Update(Player *p, const Level *lv, bool action, float dt)
{
    unsigned ev = EVT_NONE;

    switch (p->state) {

    case BALL_STATE_MOVING:
        if (action && p->strikes > 0) {                 /* [T04] logical AND */
            p->state     = BALL_STATE_AIMING;
            p->ball.vel  = V2(0.0f, 0.0f);              /* ball hangs in mid-air */
            p->aim_angle = 0.0f;
            p->power     = 0.0f;
            ev |= EVT_AIM_START;
        } else {
            ev |= Physics_StepBall(&p->ball, lv, dt);
        }
        break;

    case BALL_STATE_AIMING:
        if (action) {                                   /* lock the angle the player SEES */
            p->state         = BALL_STATE_POWER;
            p->power         = 0.0f;
            p->power_growing = true;
            ev |= EVT_ANGLE_LOCK;
        } else {
            p->aim_angle = fmodf(p->aim_angle + AIM_SPEED * dt, 2.0f * GG_PI);  /* [T12] fmodf */
        }
        break;

    case BALL_STATE_POWER:
        if (action) {                                   /* shoot with the power the player SEES */
            p->strikes--;
            p->strokes++;
            p->ball.vel = Physics_LaunchVelocity(p->aim_angle, p->power);
            p->state    = BALL_STATE_MOVING;
            ev |= EVT_LAUNCH;
        } else if (p->power_growing) {
            p->power += POWER_SPEED * dt;
            if (p->power >= 100.0f) {
                p->power = 100.0f;
                p->power_growing = false;
                ev |= EVT_POWER_BOUNCE;
            }
        } else {
            p->power -= POWER_SPEED * dt;
            if (p->power <= 0.0f) {
                p->power = 0.0f;
                p->power_growing = true;
                ev |= EVT_POWER_BOUNCE;
            }
        }
        break;
    }

    /* sample the trail every 2nd tick (30 Hz), in every state */
    p->trail_tick = (p->trail_tick + 1) % 2;
    if (p->trail_tick == 0)
        trail_push(p, p->ball.pos);

    return ev;
}
