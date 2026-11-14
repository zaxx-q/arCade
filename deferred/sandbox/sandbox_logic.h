/* Sandbox - PURE game logic (no raylib). Owner: Leader
 * A falling-sand toy built on core/sandbox (the same engine animates the hub background).
 * NOTE: functions use the prefix sbx_ because sandbox_* is already taken by the core engine. */
#ifndef SANDBOX_LOGIC_H
#define SANDBOX_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/sandbox.h"

#define SBX_W 320
#define SBX_H 180
#define SBX_CELL 4 /* 320*4 = 1280, 180*4 = 720: the grid fills the window */

typedef struct sbx_State {
    Sandbox world;
    ElementId element; /* what the left mouse button paints */
    int brush;         /* brush radius in cells */
    bool paused;
    unsigned ticks;
} sbx_State;

void sbx_init(sbx_State *s, unsigned seed);
void sbx_step(sbx_State *s, const ArcadeInput *in, float dt);
bool sbx_is_over(const sbx_State *s); /* a sandbox never ends: always false */
int sbx_score(const sbx_State *s);    /* number of non-empty cells (unranked, just for display) */

#endif
