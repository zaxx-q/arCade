/* Laser Mirrors - PURE game logic (no raylib). Owner: Member 3
 * Hook: rotate mirrors so the laser beam bounces onto every target. Splitters make the beam branch.
 * A run is a campaign: LM_LEVEL_COUNT levels in a fixed order, each one scored against its own par. */
#ifndef LASER_MIRRORS_LOGIC_H
#define LASER_MIRRORS_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"

#define LM_W 14
#define LM_H 8
#define LM_TILE 80.0f
#define LM_OX 80.0f /* pixel offset of tile (0,0) */
#define LM_OY 60.0f
#define LM_LEVEL_COUNT 12
#define LM_MAX_ROT 16 /* most rotatable mirrors a level may have: the solver tries every combination */
#define LM_CLEAR_HOLD                                                                                        \
    30 /* steps after a level is cleared before a click / SPACE moves on (no double-click skips) */

typedef enum LmCell { LM_EMPTY = 0, LM_WALL, LM_SLASH, LM_BACK, LM_SPLIT, LM_TARGET } LmCell;
/* directions: 0 = right, 1 = down, 2 = left, 3 = up */

typedef struct laser_mirrors_State {
    uint8_t cell[LM_H][LM_W];
    bool rotatable[LM_H][LM_W];
    uint8_t beam[LM_H][LM_W]; /* bit d set = a beam travelling in direction d passes through this tile */
    bool lit[LM_H][LM_W];     /* targets that are hit */
    int emitter_x, emitter_y, emitter_dir;
    int targets, targets_lit;
    int level;        /* index of the level on the board (0-based) */
    int par;          /* fewest clicks that solve this level */
    int moves;        /* clicks spent on this level (a reset does not give them back) */
    bool solved;      /* this level is cleared: waiting for the player to go on */
    int total;        /* points banked from cleared levels */
    int level_points; /* points the level that was just cleared gave (shown on the clear banner) */
    int hold;         /* steps since the level was cleared */
    bool finished;    /* the last level was cleared and the player went on: the run is over */
} laser_mirrors_State;

/* What the brute-force solver learns about the mirrors of one board. */
typedef struct LmSolve {
    int rotatable;  /* number of rotatable mirrors on the board */
    int solutions;  /* how many mirror combinations light every target; -1 if rotatable > LM_MAX_ROT */
    int min_moves;  /* fewest clicks from the current mirrors to any winning combination; -1 if none */
    uint32_t flips; /* one fewest-clicks solution: bit i = flip the i-th rotatable mirror (row by row) */
} LmSolve;

/* Starts the campaign on level 0. seed is ignored: the level order is fixed. */
void laser_mirrors_init(laser_mirrors_State *s, unsigned seed);
void laser_mirrors_step(laser_mirrors_State *s, const ArcadeInput *in, float dt);
bool laser_mirrors_is_over(const laser_mirrors_State *s);
int laser_mirrors_score(const laser_mirrors_State *s); /* points banked so far (HUD and hub) */

/* ---- levels ---- */
void laser_mirrors_load_level(laser_mirrors_State *s, int level); /* board, par; moves = 0; keeps total */
const char *laser_mirrors_level_name(int level);
const char *laser_mirrors_level_hint(int level);
int laser_mirrors_level_par(int level);
/* Fills the board from up to LM_H rows of LM_W characters (see the legend in laser_mirrors_logic.c);
 * short rows and missing (NULL) rows are padded with empty tiles. */
void laser_mirrors_load_rows(laser_mirrors_State *s, const char *const rows[LM_H]);
/* Points the current level is worth: 1000, minus 60 per click above par, never below 100. 0 if unsolved. */
int laser_mirrors_level_points(const laser_mirrors_State *s);

/* Re-trace the beam from the emitter (recursive: splitters branch). Called after every change. */
void laser_mirrors_recompute(laser_mirrors_State *s);
/* Level validation: tries EVERY mirror combination (<= LM_MAX_ROT rotatable). */
LmSolve laser_mirrors_solve(const laser_mirrors_State *s);
bool laser_mirrors_can_solve(const laser_mirrors_State *s);
bool laser_mirrors_toggle(laser_mirrors_State *s, int tx,
                          int ty); /* rotate a rotatable mirror; true if it changed */

#endif
