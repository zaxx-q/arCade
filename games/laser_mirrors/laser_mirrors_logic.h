/* Laser Mirrors - PURE game logic (no raylib). Owner: Member 3
 * Hook: rotate mirrors so the laser beam bounces onto every target. Splitters make the beam branch. */
#ifndef LASER_MIRRORS_LOGIC_H
#define LASER_MIRRORS_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"

#define LM_W 14
#define LM_H 8
#define LM_TILE 80.0f
#define LM_OX 80.0f /* pixel offset of tile (0,0) */
#define LM_OY 60.0f
#define LM_PAR_MOVES 2 /* fewest toggles that solve the built-in level */

typedef enum LmCell { LM_EMPTY = 0, LM_WALL, LM_SLASH, LM_BACK, LM_SPLIT, LM_TARGET } LmCell;
/* directions: 0 = right, 1 = down, 2 = left, 3 = up */

typedef struct laser_mirrors_State {
    uint8_t cell[LM_H][LM_W];
    bool rotatable[LM_H][LM_W];
    uint8_t beam[LM_H][LM_W]; /* bit d set = a beam travelling in direction d passes through this tile */
    bool lit[LM_H][LM_W];     /* targets that are hit */
    int emitter_x, emitter_y, emitter_dir;
    int targets, targets_lit;
    int moves;
    bool solved;
} laser_mirrors_State;

void laser_mirrors_init(laser_mirrors_State *s, unsigned seed);
void laser_mirrors_step(laser_mirrors_State *s, const ArcadeInput *in, float dt);
bool laser_mirrors_is_over(const laser_mirrors_State *s);
int laser_mirrors_score(const laser_mirrors_State *s);

/* Re-trace the beam from the emitter (recursive: splitters branch). Called after every change. */
void laser_mirrors_recompute(laser_mirrors_State *s);
/* Level validation: is there ANY mirror configuration that lights every target? (brute force, <= 16
 * rotatable) */
bool laser_mirrors_can_solve(const laser_mirrors_State *s);
bool laser_mirrors_toggle(laser_mirrors_State *s, int tx,
                          int ty); /* rotate a rotatable mirror; true if it changed */

#endif
