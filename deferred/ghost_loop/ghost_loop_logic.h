/* Ghost Loop - PURE game logic (no raylib). Owner: Member 4
 * Hook: every round is recorded. Next round your past runs replay as ghosts. Cooperate with your past
 * self: a ghost stands on the pad to keep the door open while the present you walks through it. */
#ifndef GHOST_LOOP_LOGIC_H
#define GHOST_LOOP_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"

#define GL_ROUND_STEPS 480 /* 8 seconds at 60 steps per second */
#define GL_MAX_GHOSTS 4
#define GL_SPEED 220.0f
#define GL_R 12.0f
/* room layout (pixels) */
#define GL_ROOM_X0 120.0f
#define GL_ROOM_X1 1160.0f
#define GL_ROOM_Y0 80.0f
#define GL_ROOM_Y1 680.0f
#define GL_START_X 200.0f
#define GL_START_Y 500.0f
#define GL_PAD_X 300.0f
#define GL_PAD_Y 200.0f
#define GL_PAD_R 36.0f
#define GL_DOOR_X 880.0f
#define GL_DOOR_W 30.0f
#define GL_DOOR_Y0 200.0f
#define GL_DOOR_Y1 560.0f
#define GL_EXIT_X 1040.0f
#define GL_EXIT_Y 380.0f
#define GL_EXIT_R 36.0f

typedef struct gl_Move {
    int8_t mx, my; /* -1, 0 or 1 per axis: all a ghost needs to replay a run exactly */
} gl_Move;

typedef struct ghost_loop_State {
    gl_Move recording[GL_ROUND_STEPS];                  /* inputs of the round in progress */
    gl_Move ghost_moves[GL_MAX_GHOSTS][GL_ROUND_STEPS]; /* inputs of the finished rounds */
    int ghost_count;
    Vec2 player;
    Vec2 ghost_pos[GL_MAX_GHOSTS];
    int step;  /* step inside the current round */
    int round; /* 0-based */
    bool door_open;
    bool won, lost;
    int score;
} ghost_loop_State;

/* dt is ignored on purpose: the simulation advances in exact fixed steps so replays are identical. */
void ghost_loop_init(ghost_loop_State *s, unsigned seed);
void ghost_loop_step(ghost_loop_State *s, const ArcadeInput *in, float dt);
bool ghost_loop_is_over(const ghost_loop_State *s);
int ghost_loop_score(const ghost_loop_State *s);

#endif
