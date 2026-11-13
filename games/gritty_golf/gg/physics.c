/*
 * physics.c  -  gravity, collision detection / response, path prediction
 */
#include <float.h>
#include <math.h>
#include "physics.h"
#include "vec2.h"

/* ================================================================== *
 *  Geometry helpers
 * ================================================================== */

/*
 * [T10 Pointer]  POINTER ARITHMETIC
 * `pts` points at the first Vec2 of an array.  `pts + n` points one past the
 * last element.  We walk with a pointer instead of an index.
 *
 * Point-in-polygon, "ray casting" / even-odd rule:
 *   shoot a ray from p to the right; every polygon edge it crosses flips
 *   the answer.  Odd number of crossings = inside.
 *
 *   PSEUDOCODE
 *     inside <- false
 *     FOR each edge (prev -> cur)
 *        IF the edge straddles the horizontal line y = p.y THEN
 *           xcross <- x where the edge meets that line
 *           IF p.x < xcross THEN inside <- NOT inside
 *     return inside
 */
bool Physics_PointInPolygon(Vec2 p, const Vec2 *pts, int n)
{
    bool inside = false;
    const Vec2 *prev = pts + (n - 1);                 /* last vertex       */
    for (const Vec2 *cur = pts; cur < pts + n; prev = cur++) {
        bool straddles = (cur->y > p.y) != (prev->y > p.y);     /* [T04] relational + logical */
        if (straddles) {
            float xcross = (prev->x - cur->x) * (p.y - cur->y)
                         / (prev->y - cur->y) + cur->x;
            if (p.x < xcross)
                inside = !inside;                                /* [T04] logical NOT */
        }
    }
    return inside;
}

/* Closest point to p on the segment a-b (static = private to this file). */
static Vec2 closest_on_segment(Vec2 p, Vec2 a, Vec2 b)
{
    Vec2  ab = V2_Sub(b, a);
    float len_sq = V2_LenSq(ab);
    if (len_sq < 1e-8f)
        return a;                                     /* degenerate edge   */
    float t = V2_Dot(V2_Sub(p, a), ab) / len_sq;      /* projection        */
    t = Clampf(t, 0.0f, 1.0f);                        /* stay on segment   */
    return V2_Add(a, V2_Scale(ab, t));
}

/* Unit normal of an edge, flipped so that it points OUT of the polygon. */
static Vec2 outward_normal(const Vec2 *pts, int n, Vec2 on_edge, Vec2 edge)
{
    Vec2 nrm = V2_Norm(V2(-edge.y, edge.x));
    Vec2 probe = V2_Add(on_edge, V2_Scale(nrm, 0.5f));
    if (Physics_PointInPolygon(probe, pts, n))
        nrm = V2_Scale(nrm, -1.0f);
    return nrm;
}

/*
 * Circle vs ONE segment (the textbook version from the specification).
 *   PSEUDOCODE
 *     t       <- clamp( dot(P-A, B-A) / |B-A|^2 , 0, 1 )
 *     closest <- A + t (B-A)
 *     dist    <- |P - closest|
 *     IF dist < r THEN  normal <- (P-closest)/dist ; depth <- r - dist
 */
bool Physics_CircleSegment(Vec2 c, float r, Vec2 a, Vec2 b, Contact *out)
{
    Vec2  q = closest_on_segment(c, a, b);
    Vec2  d = V2_Sub(c, q);
    float dist = V2_Len(d);
    if (dist >= r)
        return false;

    if (out != NULL) {                    /* [T10] output parameter via pointer */
        out->depth = r - dist;
        if (dist > 1e-5f) {
            out->normal = V2_Scale(d, 1.0f / dist);
        } else {                          /* centre exactly on the segment */
            Vec2 e = V2_Sub(b, a);
            out->normal = V2_Norm(V2(-e.y, e.x));
        }
    }
    return true;
}

/*
 * Circle vs a whole polygon (what the ball does against terrain and walls).
 *
 * Improvement over the plain edge test: if the ball CENTRE ever ends up
 * inside the solid (a very fast ball, or a wall sliding into it), the nearest
 * edge would push it deeper.  We detect that with PointInPolygon and push it
 * out through the nearest edge instead, so the ball can never get stuck.
 */
bool Physics_CirclePolygon(Vec2 c, float r, const Vec2 *pts, int n, Contact *out)
{
    float best_sq = FLT_MAX;
    Vec2  best_pt = pts[0];
    Vec2  best_edge = V2(1.0f, 0.0f);

    for (int i = 0; i < n; i++) {
        Vec2 a = pts[i];
        Vec2 b = pts[(i + 1) % n];             /* [T04] % wraps last vertex -> first */
        Vec2 q = closest_on_segment(c, a, b);
        float d_sq = V2_LenSq(V2_Sub(c, q));
        if (d_sq < best_sq) {
            best_sq = d_sq;
            best_pt = q;
            best_edge = V2_Sub(b, a);
        }
    }

    bool inside = Physics_PointInPolygon(c, pts, n);
    if (!inside && best_sq >= r * r)
        return false;                          /* too far away: no contact */
    if (out == NULL)
        return true;                           /* caller only wants yes/no */

    float dist = sqrtf(best_sq);
    if (dist > 1e-5f) {
        Vec2 away = V2_Scale(V2_Sub(c, best_pt), 1.0f / dist);   /* surface -> ball */
        out->normal = inside ? V2_Scale(away, -1.0f) : away;     /* [T05] ternary   */
    } else {
        out->normal = outward_normal(pts, n, best_pt, best_edge);
    }
    out->depth = inside ? dist + r : r - dist;
    return true;
}

/*
 * Out-of-bounds: the 64x64 "visibility box" around the ball no longer touches
 * the camera view, i.e. the ball centre is more than 32 px outside.
 */
bool Physics_OutOfBounds(Vec2 p, float world_size)
{
    const float margin = 32.0f;
    return p.x < -margin || p.x > world_size + margin ||      /* [T04] logical OR */
           p.y < -margin || p.y > world_size + margin;
}

/* ================================================================== *
 *  Simulation
 * ================================================================== */

/* velocity = (cos a, sin a) * power * 6      (power 100 -> 600 px/s) */
Vec2 Physics_LaunchVelocity(float angle, float power)
{
    power = Clampf(power, 0.0f, 100.0f);
    Vec2 dir = V2(cosf(angle), sinf(angle));                   /* [T12] trig */
    return V2_Scale(dir, power * LAUNCH_SCALE);
}

/*
 * Push the ball out of an obstacle and reflect its velocity.
 *     v' = v - (1 + e) (v . n) n        (only if moving INTO the surface)
 * e = 0.33 gives the small bounce; below REST_VELOCITY we use e = 0 so the
 * ball settles instead of jittering forever.  Friction is 0, so the part of
 * the velocity along the surface is untouched and the ball slides.
 */
static void apply_contact(Ball *b, const Contact *c, unsigned *events)
{
    b->pos = V2_Add(b->pos, V2_Scale(c->normal, c->depth));    /* positional fix */

    float vn = V2_Dot(b->vel, c->normal);                      /* speed into surface */
    if (vn < 0.0f) {
        float impact = -vn;
        float e = (impact > REST_VELOCITY) ? BOUNCE : 0.0f;
        b->vel = V2_Sub(b->vel, V2_Scale(c->normal, (1.0f + e) * vn));
        if (impact > SFX_IMPACT_MIN)
            *events |= EVT_BOUNCE;                             /* [T04] set a flag */
    }
}

/* One pass over every solid object (terrain polygons + walls).
 * Returns true if at least one contact was resolved.                       */
static bool resolve_solids(Ball *b, const Level *lv, unsigned *events)
{
    bool any = false;
    Contact c;

    for (int i = 0; i < lv->def->polyland_count; i++) {
        const PolygonData *poly = &lv->def->polylands[i];
        if (Physics_CirclePolygon(b->pos, BALL_RADIUS, poly->points, poly->point_count, &c)) {
            apply_contact(b, &c, events);
            any = true;
        }
    }
    for (int i = 0; i < lv->wall_count; i++) {
        Vec2 corners[4];
        Level_WallCorners(&lv->walls[i], corners);
        if (Physics_CirclePolygon(b->pos, BALL_RADIUS, corners, 4, &c)) {
            apply_contact(b, &c, events);
            any = true;
        }
    }
    return any;
}

static bool touches_hazard(Vec2 p, const Level *lv)
{
    for (int i = 0; i < lv->def->death_polyland_count; i++) {
        const PolygonData *poly = &lv->def->death_polylands[i];
        if (Physics_CirclePolygon(p, BALL_RADIUS, poly->points, poly->point_count, NULL))
            return true;
    }
    return false;
}

/*
 * Physics_StepBall : advance the ball by dt seconds (one 1/60 s tick).
 *
 * The tick is cut into PHYSICS_SUBSTEPS small steps so a fast ball
 * (600 px/s = 10 px per tick, bigger than its own radius) cannot tunnel
 * through thin geometry.
 *
 *   PSEUDOCODE
 *     h <- dt / SUBSTEPS
 *     REPEAT SUBSTEPS times
 *        vel.y <- vel.y + g*h              // gravity (semi-implicit Euler)
 *        pos   <- pos + vel*h
 *        REPEAT up to 3 passes: resolve contacts with terrain and walls
 *        IF touching a hazard THEN raise EVT_HAZARD
 *     IF out of bounds THEN raise EVT_OOB
 */
unsigned Physics_StepBall(Ball *b, const Level *lv, float dt)
{
    unsigned events = EVT_NONE;
    const float h = dt / (float)PHYSICS_SUBSTEPS;

    for (int s = 0; s < PHYSICS_SUBSTEPS; s++) {
        b->vel.y += GRAVITY * h;
        b->pos    = V2_Add(b->pos, V2_Scale(b->vel, h));

        for (int pass = 0; pass < 3; pass++) {
            if (!resolve_solids(b, lv, &events))
                break;                              /* nothing touched: done */
        }
        if (touches_hazard(b->pos, lv))
            events |= EVT_HAZARD;
    }

    if (Physics_OutOfBounds(b->pos, Level_WorldSize(lv)))
        events |= EVT_OOB;
    return events;
}

/* ================================================================== *
 *  [T12 Rekursif]  RECURSIVE path prediction (aim assist)
 *
 *  A recursive function solves a big problem by solving a slightly smaller
 *  copy of the same problem.  Here:
 *      "path of the ball for N more ticks"  =
 *          "move ONE tick"  +  "path of the ball for N-1 more ticks"
 *
 *  Every recursive function needs
 *    1. a BASE CASE   - when to stop (no ticks left / array full / ball died)
 *    2. a RECURSIVE CASE that moves toward the base case (ticks_left - 1)
 *
 *  `probe` is passed BY VALUE, so each call works on its own copy: the real
 *  ball is never touched.
 * ================================================================== */
int Physics_PredictPath(const Level *lv, Ball probe, int ticks_left,
                        Vec2 *out, int count, int max_points)
{
    /* base cases */
    if (ticks_left <= 0 || count >= max_points)
        return count;

    unsigned ev = Physics_StepBall(&probe, lv, PHYSICS_DT);
    if (ev & (EVT_HAZARD | EVT_OOB))               /* [T04] test several bits at once */
        return count;                              /* the real ball would die here */

    if (ticks_left % 3 == 0)                       /* keep every 3rd position      */
        out[count++] = probe.pos;

    /* recursive case: same problem, one tick shorter */
    return Physics_PredictPath(lv, probe, ticks_left - 1, out, count, max_points);
}
