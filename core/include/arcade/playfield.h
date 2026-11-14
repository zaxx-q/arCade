/* Portrait playfield used by the ported mini-games (header-only, pure C99).
 *
 * The window is 1280x720. A portrait game gets a 480 x 684 playfield centred under the 36 px HUD
 * bar, so the left and right gutters (400 px each) are free for rules and stats.
 * Game LOGIC works in playfield-local coordinates (origin = top-left of the playfield); the
 * front-end adds ARC_PF_X / ARC_PF_Y when drawing and subtracts them from the pointer. */
#ifndef ARCADE_PLAYFIELD_H
#define ARCADE_PLAYFIELD_H

#include "arcade/common.h"
#include "arcade/mathx.h"

#define ARC_PF_W 480
#define ARC_PF_H (ARC_SCREEN_H - 36)
#define ARC_PF_X ((ARC_SCREEN_W - ARC_PF_W) / 2)
#define ARC_PF_Y 36

/* Screen pixels -> playfield-local pixels (the result can be negative or > size: outside). */
static inline Vec2 arc_pf_from_screen(float sx, float sy) {
    return vec2(sx - (float)ARC_PF_X, sy - (float)ARC_PF_Y);
}

/* Playfield-local pixels -> screen pixels. */
static inline Vec2 arc_pf_to_screen(float lx, float ly) {
    return vec2(lx + (float)ARC_PF_X, ly + (float)ARC_PF_Y);
}

/* True when a LOCAL point lies inside a w x h playfield (use for "ignore clicks in the gutters"). */
static inline bool arc_pf_contains(Vec2 local, float w, float h) {
    return local.x >= 0.0f && local.x < w && local.y >= 0.0f && local.y < h;
}

#endif
