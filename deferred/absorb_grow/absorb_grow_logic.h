/* Absorb and Grow - PURE game logic (no raylib). Owner: Member 6
 * Hook: you are a blob. Swallow anything clearly smaller than you, avoid anything clearly bigger.
 * Everything you absorb adds to your area, so the world you play in keeps changing. */
#ifndef ABSORB_GROW_LOGIC_H
#define ABSORB_GROW_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define AG_MAX_BLOBS 40
#define AG_START_R 14.0f
#define AG_WIN_R 150.0f
#define AG_TIME 90.0f /* seconds */

typedef struct ag_Blob {
    Vec2 pos, vel;
    float r;
} ag_Blob;

typedef struct absorb_grow_State {
    Vec2 pos;
    float r;
    ag_Blob blobs[AG_MAX_BLOBS];
    int absorbed;
    float time_left;
    bool over, won;
    Vec2 last_absorb;  /* position of the latest absorbed blob (front-end spawns particles) */
    int absorb_events; /* increases by one per absorbed blob */
    Rng rng;
} absorb_grow_State;

void absorb_grow_init(absorb_grow_State *s, unsigned seed);
void absorb_grow_step(absorb_grow_State *s, const ArcadeInput *in, float dt);
bool absorb_grow_is_over(const absorb_grow_State *s);
int absorb_grow_score(const absorb_grow_State *s);

#endif
