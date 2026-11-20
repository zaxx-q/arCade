/* Private helpers shared by the core/src/sandbox*.c files. Not part of the public API: games and the hub
 * only include arcade/sandbox.h. */
#ifndef ARCADE_SANDBOX_INTERNAL_H
#define ARCADE_SANDBOX_INTERNAL_H

#include "arcade/sandbox.h"

/* Bits of Sandbox.moved. A cell that already acted this tick is skipped, so nothing moves twice. */
#define MOVED_NOW 1u
#define MOVED_LAST 2u
#define EXCITED                                                                                              \
    4u /* uranium that was hit by a gamma ray and will split on its next turn; kept across ticks */

/* The Life cellular automaton only advances every this many ticks, so patterns can be watched. */
#define LIFE_PERIOD 6

static inline bool sandbox_inside(const Sandbox *sb, int x, int y) {
    return x >= 0 && y >= 0 && x < sb->w && y < sb->h;
}

/* Element at (x, y); EL_STONE outside the grid, so the edges of the world behave like walls. */
static inline ElementId sandbox_at(const Sandbox *sb, int x, int y) {
    return sandbox_inside(sb, x, y) ? (ElementId)sb->cells[y * sb->w + x] : EL_STONE;
}

/* The 8 directions, clockwise from east: 0 = E, 2 = S, 4 = W, 6 = N. Even numbers are the 4 straight ones. */
static inline int sandbox_dx(int dir) {
    static const int t[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    return t[dir & 7];
}
static inline int sandbox_dy(int dir) {
    static const int t[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    return t[dir & 7];
}

/* Fluids can be pushed aside by heavier things: liquids, gases and fire. */
static inline bool sandbox_is_fluid(ElementId id) {
    MoveKind k = sandbox_element(id)->kind;
    return k == MOVE_LIQUID || k == MOVE_GAS || k == MOVE_FIRE;
}

/* ---- sandbox.c ---- */
int sandbox_rand(Sandbox *sb, int n);         /* 0 .. n-1 */
bool sandbox_chance(Sandbox *sb, int n);      /* true with probability n / 256 */
void sandbox_swap(Sandbox *sb, int a, int b); /* swap two cells with all their state; both count as moved */
void sandbox_put(Sandbox *sb, int x, int y, ElementId id,
                 uint8_t aux); /* overwrite a cell; it waits a tick */

/* ---- sandbox_move.c ---- */
void sandbox_move(Sandbox *sb, int x, int y);

/* ---- sandbox_react.c ---- */
void sandbox_react(Sandbox *sb, int x, int y);
void sandbox_explode(Sandbox *sb, int cx, int cy, int radius);
void sandbox_life_step(Sandbox *sb);
void sandbox_gamma_hit(Sandbox *sb, int x, int y); /* a gamma ray just reached cell (x, y) */

#endif
