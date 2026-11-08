/* Small 2D math helpers (header-only, pure C99). */
#ifndef ARCADE_MATHX_H
#define ARCADE_MATHX_H

#include <math.h>

#include "arcade/common.h"

typedef struct Vec2 {
    float x, y;
} Vec2;

static inline Vec2 vec2(float x, float y) {
    Vec2 v;
    v.x = x;
    v.y = y;
    return v;
}
static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x, a.y - b.y);
}
static inline Vec2 vec2_scale(Vec2 a, float s) {
    return vec2(a.x * s, a.y * s);
}
static inline float vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}
static inline float vec2_len(Vec2 a) {
    return sqrtf(a.x * a.x + a.y * a.y);
}
static inline float vec2_dist(Vec2 a, Vec2 b) {
    return vec2_len(vec2_sub(a, b));
}
static inline Vec2 vec2_norm(Vec2 a) {
    float l = vec2_len(a);
    return l > 0.00001f ? vec2_scale(a, 1.0f / l) : vec2(0.0f, 0.0f);
}

static inline bool arc_circles_overlap(Vec2 a, float ar, Vec2 b, float br) {
    float dx = a.x - b.x, dy = a.y - b.y, r = ar + br;
    return dx * dx + dy * dy <= r * r;
}
static inline bool arc_point_in_rect(Vec2 p, float rx, float ry, float rw, float rh) {
    return p.x >= rx && p.x <= rx + rw && p.y >= ry && p.y <= ry + rh;
}
static inline bool arc_rects_overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw,
                                     float bh) {
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

#endif
