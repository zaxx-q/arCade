/*
 * physics.h  -  ball motion and collision detection (no graphics, no sound)
 *
 * The functions here never play sounds or draw anything.  They only change
 * the Ball and REPORT what happened as a bit mask of EVT_* flags.  The caller
 * (game.c) decides what to do about it.  This is why the same code can be used
 * for the real ball AND for the aim-assist "ghost" ball.
 */
#ifndef GG_PHYSICS_H
#define GG_PHYSICS_H

#include "common.h"
#include "level.h"

/* [T13 Struct] a plain data container: position and velocity of the ball */
typedef struct {
    Vec2 pos;
    Vec2 vel;
} Ball;

/* Result of a collision test: which way to push, and how far. */
typedef struct {
    Vec2  normal;     /* unit vector pointing from the obstacle to the ball */
    float depth;      /* penetration depth in pixels                        */
} Contact;

/* ---- geometry queries --------------------------------------------- */
bool Physics_PointInPolygon(Vec2 p, const Vec2 *pts, int n);
bool Physics_CircleSegment(Vec2 c, float r, Vec2 a, Vec2 b, Contact *out);
bool Physics_CirclePolygon(Vec2 c, float r, const Vec2 *pts, int n, Contact *out);
bool Physics_OutOfBounds(Vec2 p, float world_size);

/* ---- simulation --------------------------------------------------- */
Vec2     Physics_LaunchVelocity(float angle, float power);
unsigned Physics_StepBall(Ball *b, const Level *lv, float dt);

/* Recursive aim-assist: fills `out` with sample points of the future path. */
int Physics_PredictPath(const Level *lv, Ball probe, int ticks_left,
                        Vec2 *out, int count, int max_points);

#endif /* GG_PHYSICS_H */
