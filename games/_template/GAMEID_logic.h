/* @TITLE@ - PURE game logic (no raylib, no drawing, no keyboard). Owner: @OWNER@
 * Everything here must be deterministic: same seed + same inputs => same result. */
#ifndef @IDU@_LOGIC_H
#define @IDU@_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

typedef struct @ID@_State {
    Vec2 pos;        /* player position (pixels) */
    Vec2 pellet;     /* the thing to collect     */
    int score;
    float time_left; /* seconds */
    Rng rng;
} @ID@_State;

void @ID@_init(@ID@_State *s, unsigned seed);
void @ID@_step(@ID@_State *s, const ArcadeInput *in, float dt);
bool @ID@_is_over(const @ID@_State *s);
int @ID@_score(const @ID@_State *s);

#endif
