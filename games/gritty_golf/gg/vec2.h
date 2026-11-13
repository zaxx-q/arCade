/*
 * vec2.h  -  tiny 2D vector library built on the Vec2 struct from levels.h
 *
 *   [T11 Fungsi]      small FUNCTIONS that take values and RETURN a value
 *   [T12 Math]        sqrtf, sinf, cosf, fabsf, fmodf ... from <math.h>
 *   [T04 Operator]    arithmetic + - * /
 *
 * "static inline" in a header: every .c file gets its own copy, the compiler
 * may paste the body at the call site (no call overhead).
 */
#ifndef GG_VEC2_H
#define GG_VEC2_H

#include <math.h>
#include "common.h"

/* [T11] A FUNCTION: input (x, y)  ->  returns a Vec2 (passed BY VALUE) */
static inline Vec2  V2(float x, float y)         { Vec2 v = { x, y };              return v; }
static inline Vec2  V2_Add(Vec2 a, Vec2 b)       { return V2(a.x + b.x, a.y + b.y); }
static inline Vec2  V2_Sub(Vec2 a, Vec2 b)       { return V2(a.x - b.x, a.y - b.y); }
static inline Vec2  V2_Scale(Vec2 a, float k)    { return V2(a.x * k, a.y * k);     }
static inline float V2_Dot(Vec2 a, Vec2 b)       { return a.x * b.x + a.y * b.y;    }
static inline float V2_LenSq(Vec2 a)             { return V2_Dot(a, a);             }
static inline float V2_Len(Vec2 a)               { return sqrtf(V2_LenSq(a));       }   /* [T12] sqrtf */
static inline float V2_Dist(Vec2 a, Vec2 b)      { return V2_Len(V2_Sub(a, b));     }

/* Unit-length copy of a.  The `if` protects against dividing by zero. */
static inline Vec2 V2_Norm(Vec2 a)
{
    float len = V2_Len(a);
    return (len > 1e-6f) ? V2_Scale(a, 1.0f / len) : V2(0.0f, 0.0f);   /* [T05] ternary */
}

/* Rotate a vector by `angle` radians (clockwise on screen because +y is down). */
static inline Vec2 V2_Rotate(Vec2 v, float angle)
{
    float c = cosf(angle), s = sinf(angle);                           /* [T12] cosf, sinf */
    return V2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static inline float Clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;                                            /* [T05] early returns */
    if (v > hi) return hi;
    return v;
}

static inline float Lerpf(float a, float b, float t) { return a + (b - a) * t; }

#endif /* GG_VEC2_H */
