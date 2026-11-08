/* Echo Maze - PURE game logic (no raylib). Owner: Member 2
 * Hook: the screen is black. Press Space to send out a sound wave; walls it reaches light up for a moment.
 * Walls hidden behind other walls are NOT revealed (line of sight). Find the exit by sonar. */
#ifndef ECHO_MAZE_LOGIC_H
#define ECHO_MAZE_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define EM_W 31 /* tiles, odd so the generator works on a lattice of odd coordinates */
#define EM_H 17
#define EM_TILE 40.0f
#define EM_OX 20.0f /* pixel offset of tile (0,0) */
#define EM_OY 38.0f
#define EM_PING_SPEED 520.0f
#define EM_PING_RANGE 720.0f
#define EM_PING_COOLDOWN 1.0f
#define EM_REVEAL_TIME 3.0f

typedef struct echo_maze_State {
    uint8_t wall[EM_H][EM_W]; /* 1 = wall, 0 = floor */
    float reveal[EM_H][EM_W]; /* seconds left that this wall tile stays visible */
    Vec2 player;              /* pixels */
    Vec2 ping_origin;
    float ping_r;
    bool pinging;
    float ping_cooldown;
    float exit_glow; /* seconds the exit stays visible after a ping touched it */
    int pings;
    float time;
    bool won;
    Rng rng;
} echo_maze_State;

void echo_maze_init(echo_maze_State *s, unsigned seed);
void echo_maze_step(echo_maze_State *s, const ArcadeInput *in, float dt);
bool echo_maze_is_over(const echo_maze_State *s);
int echo_maze_score(const echo_maze_State *s);

/* Helpers shared with the front-end and the tests. */
Vec2 echo_maze_tile_center(int tx, int ty);
bool echo_maze_los_clear(const echo_maze_State *s, Vec2 from, int tx,
                         int ty); /* nothing but (tx,ty) blocks the view */

#endif
