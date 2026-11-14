/* Master Juggler - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A trampoline (a bar) follows the pointer. Balls are thrown up out of it; bounce them so none falls
 * off the bottom. Level N throws N balls and every ball must be bounced 3 * min(N, 3) times.
 * Losing every ball costs a life and repeats the level; at 0 lives the game is over.
 * Score = the highest level COMPLETED. Coordinates are playfield-local. */
#ifndef MASTER_JUGGLER_LOGIC_H
#define MASTER_JUGGLER_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define MJ_MAX_BALLS 32 /* array size; level N throws N balls                         */
#define MJ_MAX_LEVEL 32 /* completing this level ends the game                        */
#define MJ_LIVES 3      /* NEW RULE (the original never ends): tries lost before game over */
#define MJ_BAR_W 90.0f
#define MJ_BALL_R 15.0f
#define MJ_OVER_DELAY 1.2f /* seconds the front-end may show the last miss before is_over() */

/* Colour stage of a ball by progress (the front-end maps it to a colour). */
#define MJ_STAGE_RED 0
#define MJ_STAGE_YELLOW 1
#define MJ_STAGE_GREEN 2
#define MJ_STAGE_GREY 3

typedef struct master_juggler_State {
    int w, h;
    float base_h, bar_y, gh; /* floor height, TOP edge y of the bar, vertical scale */
    float bar_x;             /* centre x of the bar                                 */
    float last_aim_x;        /* previous pointer x: tells hover from keyboard       */
    bool have_aim;
    int level; /* 1..32, the level being played or about to be played */
    int best;  /* highest level completed = the score                 */
    int lives;
    bool waiting;    /* at "click to start": no ball moves                  */
    int nb, bl, mj;  /* balls launched, balls lost, min(level, 3)           */
    float frmball;   /* launch clock, starts at -10 ticks                   */
    bool first_lost; /* the first ball was lost: no more bouncing, all grey */
    float x[MJ_MAX_BALLS], y[MJ_MAX_BALLS], spx[MJ_MAX_BALLS], spy[MJ_MAX_BALLS], alpha[MJ_MAX_BALLS];
    int bounces[MJ_MAX_BALLS];
    bool visible[MJ_MAX_BALLS];
    float since_bounce; /* seconds since the last bounce (spring animation)    */
    bool over, capped;
    float over_t;
    int throws, hits, losses, levels_done, tries_lost; /* event counters for sound              */
    Rng rng;
} master_juggler_State;

void master_juggler_init(master_juggler_State *s, unsigned seed);
void master_juggler_init_sized(master_juggler_State *s, unsigned seed, int w, int h);
void master_juggler_step(master_juggler_State *s, const ArcadeInput *in, float dt);
bool master_juggler_is_over(const master_juggler_State *s);
int master_juggler_score(const master_juggler_State *s);

/* Colour stage 0..3 for a ball with `bounces` bounces on a level (yellow at mj, green at 2mj, grey at 3mj).
 */
int master_juggler_stage(int bounces, int level);
/* Frame index (in ticks of the launch clock) at which ball i of `level` is thrown: (int)(100/level*i + 0.5).
 */
int master_juggler_launch_tick(int level, int i);

#endif
