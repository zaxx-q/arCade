/* Loop Lap - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * Your ball runs round the TOP circle, enemy balls round the BOTTOM circle (the circles overlap).
 * You only control speed: brake (big ball, slow), cruise, or accelerate (small ball, fast).
 * Every lap is +1; new enemies join at laps 1, 3, 8, 17, 31, 51, 78, 113. Touching one ends the game.
 * Coordinates are playfield-local. */
#ifndef LOOP_LAP_LOGIC_H
#define LOOP_LAP_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define LL_MAX_ENEMIES 8
#define LL_ENEMY_W 48.0f   /* enemy ball diameter */
#define LL_OVER_DELAY 1.0f /* seconds the front-end may show the crash before is_over() */

/* Speed modes. */
#define LL_BRAKE (-1)
#define LL_CRUISE 0
#define LL_ACCEL 1

typedef struct loop_lap_State {
    int w, h;
    float c1x, c1y, c1r; /* top circle (player)          */
    float c2x, c2y, c2r; /* bottom circle (enemies)      */
    double my_a;         /* player angle in radians (pi/2 = 12 o'clock) */
    int mode;            /* LL_BRAKE / LL_CRUISE / LL_ACCEL */
    float my_w;          /* player ball diameter: 60 / 48 / 36 */
    int score;           /* laps                         */
    int enemy_n;
    double ea[LL_MAX_ENEMIES]; /* enemy angle (not wrapped)    */
    double ew[LL_MAX_ENEMIES]; /* enemy angular speed, rad per original tick */
    Vec2 my_pos, enemy_pos[LL_MAX_ENEMIES];
    float flash_t; /* seconds since the last lap (the flag flashes for 0.5 s) */
    float near_cd; /* cool-down of the near-miss whoosh */
    bool paused, over;
    float over_t;
    int laps, joins, whooshes; /* event counters for sound */
    Rng rng;
} loop_lap_State;

void loop_lap_init(loop_lap_State *s, unsigned seed);
void loop_lap_init_sized(loop_lap_State *s, unsigned seed, int w, int h);
void loop_lap_step(loop_lap_State *s, const ArcadeInput *in, float dt);
bool loop_lap_is_over(const loop_lap_State *s);
int loop_lap_score(const loop_lap_State *s);

/* Pure helpers (tested on their own). */
/* Held input -> LL_BRAKE / LL_CRUISE / LL_ACCEL. `in->aim_x` is a SCREEN x; it is compared with the
 * playfield centre. Accelerate wins when both are asked. */
int loop_lap_mode_from_input(const ArcadeInput *in, int w);
/* Largest centre distance at which the balls count as touching (spec: my.w/2 + 24 - 2). */
float loop_lap_hit_distance(float my_w);
/* Lap at which the i-th enemy (0..7) joins: 1, 3, 8, 17, 31, 51, 78, 113. */
int loop_lap_join_lap(int i);

#endif
