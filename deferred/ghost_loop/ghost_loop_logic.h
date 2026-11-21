/* Ghost Loop - PURE game logic (no raylib). Owner: Member 4
 * Hook: every round is recorded. Next round your past runs replay as ghosts. Cooperate with your past
 * self: a ghost stands on a pad to keep a door open while the present you walks through it.
 * A run is a campaign of GL_LEVEL_COUNT rooms; each level has its own walls, pads, doors and ghost limit. */
#ifndef GHOST_LOOP_LOGIC_H
#define GHOST_LOOP_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"

#define GL_ROUND_STEPS 480 /* 8 seconds at 60 steps per second */
#define GL_MAX_GHOSTS 4    /* most ghosts any level can allow */
#define GL_SPEED 220.0f
#define GL_R 12.0f
/* the room (pixels): every level is played inside it */
#define GL_ROOM_X0 120.0f
#define GL_ROOM_X1 1160.0f
#define GL_ROOM_Y0 80.0f
#define GL_ROOM_Y1 680.0f
#define GL_PAD_R 36.0f  /* an actor whose centre is this close to a pad presses it */
#define GL_EXIT_R 36.0f /* the player wins a level by getting this close to the exit */
#define GL_MAX_WALLS 12
#define GL_MAX_PADS 4
#define GL_MAX_DOORS 4
#define GL_LEVEL_COUNT 6
#define GL_CLEAR_HOLD 30    /* steps after a level is cleared before SPACE moves on */
#define GL_NOTICE_STEPS 150 /* how long a notice (time up, no ghosts left...) stays on screen */

typedef struct gl_Rect {
    float x0, y0, x1, y1;
} gl_Rect;

/* A door is solid while its pad is NOT pressed (by the player or by any ghost). */
typedef struct gl_Door {
    gl_Rect box;
    int pad; /* index into the level's pads */
} gl_Door;

typedef struct gl_Level {
    const char *name;
    const char *hint;
    Vec2 start, exit;
    int max_ghosts; /* ghost slots: the level allows this many past runs at once */
    int par_ghosts; /* ghosts the intended solution uses; every extra one costs points */
    int wall_count;
    gl_Rect walls[GL_MAX_WALLS]; /* solid always */
    int pad_count;
    Vec2 pads[GL_MAX_PADS];
    int door_count;
    gl_Door doors[GL_MAX_DOORS];
} gl_Level;

typedef struct gl_Move {
    int8_t mx, my; /* -1, 0 or 1 per axis: all a ghost needs to replay a run exactly */
} gl_Move;

typedef enum GlNotice {
    GL_NOTICE_NONE = 0,
    GL_NOTICE_TIME_UP,  /* the last round ran out: the same round starts again */
    GL_NOTICE_NO_GHOSTS /* SPACE was pressed but every ghost slot is used */
} GlNotice;

typedef struct ghost_loop_State {
    gl_Move recording[GL_ROUND_STEPS];                  /* inputs of the round in progress */
    gl_Move ghost_moves[GL_MAX_GHOSTS][GL_ROUND_STEPS]; /* inputs of the finished rounds */
    int ghost_count;
    Vec2 player;
    Vec2 ghost_pos[GL_MAX_GHOSTS];
    int step;                     /* step inside the current round */
    int round;                    /* 0-based: always equal to ghost_count */
    bool pad_down[GL_MAX_PADS];   /* pressed during the last step */
    bool door_open[GL_MAX_DOORS]; /* open during the last step */
    int level;                    /* index of the level being played (0-based) */
    bool cleared;                 /* the level was just cleared: waiting for SPACE */
    int clear_hold;               /* steps since it was cleared */
    int level_points;             /* points the level just cleared gave */
    int total;                    /* points banked from cleared levels */
    bool won;                     /* the last level was cleared and the player went on: the run is over */
    GlNotice notice;
    int notice_steps;
} ghost_loop_State;

/* Starts the campaign on level 0. seed is ignored: the level order is fixed. */
void ghost_loop_init(ghost_loop_State *s, unsigned seed);
/* dt is ignored on purpose: the simulation advances in exact fixed steps so replays are identical.
 * Keys: move = WASD / arrows, SPACE (action) = end the round now, R = redo the round, U = undo the last
 * ghost, C = restart the level. */
void ghost_loop_step(ghost_loop_State *s, const ArcadeInput *in, float dt);
bool ghost_loop_is_over(const ghost_loop_State *s);
int ghost_loop_score(const ghost_loop_State *s); /* points banked so far (HUD and hub) */

/* ---- levels ---- */
void ghost_loop_load_level(ghost_loop_State *s, int level); /* fresh round 0, no ghosts; keeps total */
const gl_Level *ghost_loop_level(int level);                /* the level table entry (index is clamped) */
/* Points the player earns for clearing the current level now: 1500, minus 300 per ghost above par and 1 per
 * step spent in the winning round, never below 100. */
int ghost_loop_level_points(const ghost_loop_State *s);
/* Level validation by flood fill: can an actor of the player's size walk from 'from' to within 'radius' of
 * 'target' when the doors in the bit mask 'open_doors' (bit i = door i) are open and all the others are shut?
 * Walls are always solid. Used by the tests to prove that no level can be walked around. */
bool ghost_loop_can_reach(int level, unsigned open_doors, Vec2 from, Vec2 target, float radius);

#endif
