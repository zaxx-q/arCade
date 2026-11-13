/* test_physics.c - geometry queries, ball integration, collisions, prediction */
#include <math.h>
#include <string.h>
#include "test_framework.h"
#include "physics.h"
#include "level.h"
#include "player.h"
#include "vec2.h"

/* ====================================================================== *
 *  Small synthetic levels (so expected values can be worked out by hand)
 * ====================================================================== */
static const Vec2 FLOOR_PTS[]  = { {-500, 200}, {1000, 200}, {1000, 300}, {-500, 300} };
static const PolygonData FLOOR_POLY[] = { { FLOOR_PTS, 4 } };

/* a 6 px thin floor, to test tunnelling */
static const Vec2 THIN_PTS[]   = { {-500, 200}, {1000, 200}, {1000, 206}, {-500, 206} };
static const PolygonData THIN_POLY[] = { { THIN_PTS, 4 } };

/* 30 degree slope: surface y = x * tan(30 deg) */
static const Vec2 SLOPE_PTS[]  = { {0, 0}, {1000, 577.35f}, {1000, 3000}, {0, 3000} };
static const PolygonData SLOPE_POLY[] = { { SLOPE_PTS, 4 } };

/* a hazard block */
static const Vec2 HAZ_PTS[]    = { {100, 100}, {140, 100}, {140, 140}, {100, 140} };
static const PolygonData HAZ_POLY[] = { { HAZ_PTS, 4 } };

/* wall: pivot (300, 0), tall vertical wall.  collider centre (0,78), height 156 -> y 0..156 */
static const WallData WALL_V[] = {
    { {300, 100}, 0.0f, 17.0f, 156.0f, {0, 78}, {0, -156}, 0.3f, 0, true }
};
static const ButtonData BTN[]    = { { {60, 150}, 0 } };
static const Vec2       STRIKE[] = { {200, 120} };

#define LEVEL(name, zoom, polys, npolys, haz, nhaz, walls, nwalls)           \
    static const LevelData name = { 99, (zoom), {50, 100}, {900, 900}, 0.0f, false, \
                                    (polys), (npolys), (haz), (nhaz), (walls), (nwalls), NULL, 0, NULL, 0 }

LEVEL(EMPTY_LVL, 0.1f, NULL, 0, NULL, 0, NULL, 0);          /* huge world, nothing in it */
LEVEL(FLAT_LVL,  0.5f, FLOOR_POLY, 1, NULL, 0, NULL, 0);
LEVEL(THIN_LVL,  0.5f, THIN_POLY, 1, NULL, 0, NULL, 0);
LEVEL(SLOPE_LVL, 0.1f, SLOPE_POLY, 1, NULL, 0, NULL, 0);
LEVEL(HAZ_LVL,   0.5f, FLOOR_POLY, 1, HAZ_POLY, 1, NULL, 0);
LEVEL(WALL_LVL,  0.5f, FLOOR_POLY, 1, NULL, 0, WALL_V, 1);

static const LevelData FULL_LVL = { 98, 1.0f, {50, 100}, {900, 900}, 0.0f, false,
                                    NULL, 0, NULL, 0, NULL, 0, BTN, 1, STRIKE, 1 };

static Ball ball_at(float x, float y, float vx, float vy)
{
    Ball b = { { x, y }, { vx, vy } };
    return b;
}

/* ---- point in polygon ------------------------------------------------- */
static void test_point_in_polygon(void)
{
    static const Vec2 sq[] = { {0, 0}, {10, 0}, {10, 10}, {0, 10} };
    CHECK(Physics_PointInPolygon(V2(5, 5), sq, 4));
    CHECK(Physics_PointInPolygon(V2(0.1f, 0.1f), sq, 4));
    CHECK(Physics_PointInPolygon(V2(9.9f, 9.9f), sq, 4));
    CHECK(!Physics_PointInPolygon(V2(-1, 5), sq, 4));
    CHECK(!Physics_PointInPolygon(V2(11, 5), sq, 4));
    CHECK(!Physics_PointInPolygon(V2(5, -1), sq, 4));
    CHECK(!Physics_PointInPolygon(V2(5, 11), sq, 4));

    static const Vec2 tri[] = { {0, 0}, {10, 0}, {0, 10} };
    CHECK(Physics_PointInPolygon(V2(2, 2), tri, 3));
    CHECK(!Physics_PointInPolygon(V2(8, 8), tri, 3));         /* beyond the hypotenuse */

    /* concave "L": the notch (6..10, 6..10) is OUTSIDE */
    static const Vec2 ell[] = { {0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10} };
    CHECK(Physics_PointInPolygon(V2(2, 2), ell, 6));
    CHECK(Physics_PointInPolygon(V2(2, 8), ell, 6));
    CHECK(Physics_PointInPolygon(V2(8, 2), ell, 6));
    CHECK(!Physics_PointInPolygon(V2(8, 8), ell, 6));

    /* winding order must not matter */
    static const Vec2 sq_rev[] = { {0, 10}, {10, 10}, {10, 0}, {0, 0} };
    CHECK(Physics_PointInPolygon(V2(5, 5), sq_rev, 4));
    CHECK(!Physics_PointInPolygon(V2(15, 5), sq_rev, 4));
}

/* ---- circle vs segment ------------------------------------------------ */
static void test_circle_segment(void)
{
    Contact c;
    Vec2 a = V2(0, 0), b = V2(10, 0);

    CHECK(Physics_CircleSegment(V2(5, 3), 4.0f, a, b, &c));   /* 1 px deep */
    CHECK_NEAR(c.depth, 1.0, 1e-5);
    CHECK_NEAR(c.normal.x, 0.0, 1e-5);
    CHECK_NEAR(c.normal.y, 1.0, 1e-5);

    CHECK(!Physics_CircleSegment(V2(5, 5), 4.0f, a, b, &c));  /* too far */
    CHECK(!Physics_CircleSegment(V2(5, 4), 4.0f, a, b, &c));  /* exactly touching: no contact */

    CHECK(Physics_CircleSegment(V2(-3, 0), 4.0f, a, b, &c));  /* beyond the END: uses the endpoint */
    CHECK_NEAR(c.normal.x, -1.0, 1e-5);
    CHECK_NEAR(c.depth, 1.0, 1e-5);

    CHECK(Physics_CircleSegment(V2(13, 0), 4.0f, a, b, &c));
    CHECK_NEAR(c.normal.x, 1.0, 1e-5);

    CHECK(Physics_CircleSegment(V2(5, 0), 4.0f, a, b, &c));   /* centre ON the segment */
    CHECK_NEAR(V2_Len(c.normal), 1.0, 1e-5);
    CHECK_NEAR(c.depth, 4.0, 1e-5);

    CHECK(Physics_CircleSegment(V2(5, 3), 4.0f, a, b, NULL)); /* NULL output allowed */
    CHECK(!Physics_CircleSegment(V2(50, 50), 4.0f, a, b, NULL));

    CHECK(Physics_CircleSegment(V2(1, 1), 4.0f, V2(1, 1), V2(1, 1), &c));   /* zero-length segment */

    /* diagonal segment: the normal is perpendicular to it */
    CHECK(Physics_CircleSegment(V2(5, 3), 4.0f, V2(0, 0), V2(10, 10), &c));
    CHECK_NEAR(fabs(c.normal.x), fabs(c.normal.y), 1e-4);
}

/* ---- circle vs polygon ------------------------------------------------ */
static void test_circle_polygon(void)
{
    static const Vec2 sq[] = { {0, 0}, {10, 0}, {10, 10}, {0, 10} };
    Contact c;

    CHECK(Physics_CirclePolygon(V2(5, -3), 4.0f, sq, 4, &c)); /* outside, overlapping the top */
    CHECK_NEAR(c.normal.x, 0.0, 1e-5);
    CHECK_NEAR(c.normal.y, -1.0, 1e-5);
    CHECK_NEAR(c.depth, 1.0, 1e-5);

    CHECK(!Physics_CirclePolygon(V2(5, -4), 4.0f, sq, 4, &c));
    CHECK(!Physics_CirclePolygon(V2(50, 50), 4.0f, sq, 4, &c));

    /* centre INSIDE: pushed out through the NEAREST edge (here the top, 2 px away) */
    CHECK(Physics_CirclePolygon(V2(5, 2), 4.0f, sq, 4, &c));
    CHECK_NEAR(c.normal.y, -1.0, 1e-5);
    CHECK_NEAR(c.depth, 6.0, 1e-5);                           /* 2 (to the edge) + 4 (radius) */

    CHECK(Physics_CirclePolygon(V2(8.5f, 5), 4.0f, sq, 4, &c));   /* nearest edge is the right one */
    CHECK_NEAR(c.normal.x, 1.0, 1e-5);

    CHECK(Physics_CirclePolygon(V2(5, 5), 4.0f, sq, 4, NULL));    /* yes/no only */

    /* after applying the contact the circle no longer overlaps */
    Vec2 p = V2(3, 1);
    CHECK(Physics_CirclePolygon(p, 4.0f, sq, 4, &c));
    p = V2_Add(p, V2_Scale(c.normal, c.depth + 0.01f));
    CHECK(!Physics_CirclePolygon(p, 4.0f, sq, 4, &c));

    /* a corner: closest point is the vertex, normal points diagonally away */
    CHECK(Physics_CirclePolygon(V2(12, -2), 4.0f, sq, 4, &c));
    CHECK(c.normal.x > 0.0f && c.normal.y < 0.0f);
    CHECK_NEAR(V2_Len(c.normal), 1.0, 1e-5);
}

static void test_out_of_bounds(void)
{
    CHECK(!Physics_OutOfBounds(V2(128, 128), 256.0f));
    CHECK(!Physics_OutOfBounds(V2(-32, 0), 256.0f));          /* exactly on the margin: still in */
    CHECK(Physics_OutOfBounds(V2(-32.1f, 0), 256.0f));
    CHECK(!Physics_OutOfBounds(V2(288, 100), 256.0f));
    CHECK(Physics_OutOfBounds(V2(288.1f, 100), 256.0f));
    CHECK(Physics_OutOfBounds(V2(100, -33), 256.0f));
    CHECK(Physics_OutOfBounds(V2(100, 289), 256.0f));
    CHECK(!Physics_OutOfBounds(V2(700, 700), 776.0f));        /* a zoomed-out level has a bigger world */
    CHECK(Physics_OutOfBounds(V2(700, 700), 256.0f));
}

static void test_launch_velocity(void)
{
    Vec2 v = Physics_LaunchVelocity(0.0f, 100.0f);
    CHECK_NEAR(v.x, 600.0, 1e-3);
    CHECK_NEAR(v.y, 0.0, 1e-3);
    v = Physics_LaunchVelocity(GG_PI / 2.0f, 100.0f);         /* straight DOWN (+y) */
    CHECK_NEAR(v.x, 0.0, 1e-2);
    CHECK_NEAR(v.y, 600.0, 1e-3);
    v = Physics_LaunchVelocity(3.0f * GG_PI / 2.0f, 50.0f);   /* straight UP */
    CHECK_NEAR(v.y, -300.0, 1e-2);
    CHECK_NEAR(V2_Len(Physics_LaunchVelocity(1.234f, 37.0f)), 222.0, 1e-3);   /* |v| = power * 6 */
    CHECK_NEAR(V2_Len(Physics_LaunchVelocity(1.0f, 150.0f)), 600.0, 1e-3);    /* power clamped to 100 */
    CHECK_NEAR(V2_Len(Physics_LaunchVelocity(1.0f, -20.0f)), 0.0, 1e-6);      /* ... and to 0 */
}

/* ---- integration ------------------------------------------------------ */
static void test_free_fall(void)
{
    Level *lv = Level_CreateFromData(&EMPTY_LVL);
    Ball b = ball_at(0, 0, 0, 0);
    for (int i = 0; i < 60; i++)
        CHECK_INT(Physics_StepBall(&b, lv, PHYSICS_DT), 0);
    /* after 1 s:  v = g t = 980 (semi-implicit Euler is exact for v),  y ~ g t^2 / 2 = 490 */
    CHECK_NEAR(b.vel.y, 980.0, 1.0);
    CHECK_NEAR(b.pos.y, 490.0, 5.0);
    CHECK_NEAR(b.pos.x, 0.0, 1e-6);
    CHECK_NEAR(b.vel.x, 0.0, 1e-6);
    Level_Destroy(lv);
}

static void test_projectile_motion(void)
{
    Level *lv = Level_CreateFromData(&EMPTY_LVL);
    Ball b = ball_at(0, 0, 300, -400);                        /* up and to the right */
    float top = 0.0f;
    for (int i = 0; i < 60; i++) {
        Physics_StepBall(&b, lv, PHYSICS_DT);
        if (b.pos.y < top) top = b.pos.y;
    }
    CHECK_NEAR(b.vel.x, 300.0, 1e-3);                         /* no horizontal force */
    CHECK_NEAR(top, -400.0 * 400.0 / (2.0 * 980.0), 4.0);     /* apex height = vy^2 / 2g ~ 81.6 */
    Level_Destroy(lv);
}

static void test_bounce_restitution(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball b = ball_at(100, 100, 0, 0);                         /* falls onto the floor at y = 200 */
    float impact = 0.0f, rebound = 0.0f;
    unsigned bounce_events = 0;
    for (int i = 0; i < 120 && rebound == 0.0f; i++) {
        float before = b.vel.y;
        bounce_events |= Physics_StepBall(&b, lv, PHYSICS_DT) & EVT_BOUNCE;
        if (before > 0.0f && b.vel.y < 0.0f) {                /* velocity flipped = we bounced */
            impact = before;
            rebound = -b.vel.y;
        }
    }
    CHECK(impact > 380.0f);                                   /* fell ~96 px: v ~ sqrt(2*980*96) = 434 */
    CHECK_NEAR(rebound / impact, 0.33, 0.04);                 /* coefficient of restitution */
    CHECK(bounce_events & EVT_BOUNCE);                        /* a hard hit is audible */
    Level_Destroy(lv);
}

static void test_comes_to_rest(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball b = ball_at(100, 150, 0, 0);
    float lowest = 0.0f;
    unsigned late_events = 0;
    for (int i = 0; i < 400; i++) {
        unsigned ev = Physics_StepBall(&b, lv, PHYSICS_DT);
        if (i > 200) late_events |= ev;
        if (b.pos.y > lowest) lowest = b.pos.y;
    }
    CHECK_NEAR(b.pos.y, 196.0, 0.6);                          /* floor 200 minus radius 4 */
    CHECK(V2_Len(b.vel) < 8.0f);
    CHECK(lowest < 197.0f);                                   /* never sank into the floor */
    CHECK_INT(late_events, 0);                                /* resting contact is silent */
    Level_Destroy(lv);
}

static void test_horizontal_slide_keeps_speed(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball b = ball_at(0, 196, 200, 0);                         /* friction is 0: speed must not decay */
    for (int i = 0; i < 90; i++)
        Physics_StepBall(&b, lv, PHYSICS_DT);
    CHECK_NEAR(b.vel.x, 200.0, 2.0);
    CHECK_NEAR(b.pos.y, 196.0, 0.6);
    Level_Destroy(lv);
}

static void test_slope_acceleration(void)
{
    Level *lv = Level_CreateFromData(&SLOPE_LVL);
    Ball b = ball_at(102.0f, 54.3f, 0, 0);                    /* resting on the 30 degree slope */
    float v_a = 0.0f, v_b = 0.0f;
    for (int i = 0; i < 120; i++) {
        Physics_StepBall(&b, lv, PHYSICS_DT);
        if (i == 59)  v_a = V2_Len(b.vel);                    /* t = 1.0 s */
        if (i == 119) v_b = V2_Len(b.vel);                    /* t = 2.0 s */
    }
    /* acceleration along a frictionless slope = g * sin(30 deg) = 490 px/s^2 */
    CHECK_NEAR((v_b - v_a) / 1.0f, 490.0, 25.0);
    CHECK(b.vel.x > 0.0f && b.vel.y > 0.0f);                  /* sliding down-right */
    Level_Destroy(lv);
}

static void test_no_tunnelling(void)
{
    Level *lv = Level_CreateFromData(&THIN_LVL);              /* 6 px thin floor */
    float speeds[] = { 300.0f, 600.0f, 900.0f, 1200.0f };
    for (int s = 0; s < 4; s++) {
        Ball b = ball_at(100, 150, 0, speeds[s]);             /* shot straight down */
        for (int i = 0; i < 40; i++)
            Physics_StepBall(&b, lv, PHYSICS_DT);
        CHECK(b.pos.y < 200.0f);                              /* still above the floor */
    }
    Level_Destroy(lv);

    lv = Level_CreateFromData(&WALL_LVL);                     /* thin vertical wall, shot sideways */
    Ball b = ball_at(200, 100, 600, 0);
    float max_x = 0.0f;
    for (int i = 0; i < 60; i++) {
        Physics_StepBall(&b, lv, PHYSICS_DT);
        if (b.pos.x > max_x) max_x = b.pos.x;
    }
    CHECK(max_x < 300.0f);                                    /* never reached the wall's far side */
    CHECK(b.vel.x < 0.0f);                                    /* bounced back */
    Level_Destroy(lv);
}

static void test_deep_penetration_recovery(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball b = ball_at(100, 203, 0, 0);                         /* centre 3 px INSIDE the floor */
    Physics_StepBall(&b, lv, PHYSICS_DT);
    CHECK(b.pos.y < 200.0f);                                  /* popped out on top, not dragged down */
    Level_Destroy(lv);
}

static void test_hazard_and_oob_events(void)
{
    Level *lv = Level_CreateFromData(&HAZ_LVL);
    Ball b = ball_at(120, 90, 0, 0);                          /* falls into the hazard block */
    unsigned ev = 0;
    for (int i = 0; i < 30; i++)
        ev |= Physics_StepBall(&b, lv, PHYSICS_DT);
    CHECK(ev & EVT_HAZARD);

    Ball far = ball_at(300, 100, 0, 0);
    CHECK_INT(Physics_StepBall(&far, lv, PHYSICS_DT) & EVT_HAZARD, 0);

    Ball out = ball_at(-100, 100, 0, 0);                      /* world is 256/0.5 = 512 wide */
    CHECK(Physics_StepBall(&out, lv, PHYSICS_DT) & EVT_OOB);
    Ball in = ball_at(100, 100, 0, 0);
    CHECK_INT(Physics_StepBall(&in, lv, PHYSICS_DT) & EVT_OOB, 0);
    Level_Destroy(lv);
}

static void test_wall_collision(void)
{
    Level *lv = Level_CreateFromData(&WALL_LVL);              /* wall spans x 291.5..308.5, y 100..256 */
    Ball b = ball_at(250, 150, 300, 0);
    for (int i = 0; i < 60; i++)
        Physics_StepBall(&b, lv, PHYSICS_DT);
    CHECK(b.pos.x < 291.5f);                                  /* stopped on the left of the wall */

    lv->walls[0].offset = V2(0, -156);                        /* wall slid away (fully open) */
    Ball c = ball_at(250, 150, 300, 0);
    for (int i = 0; i < 30; i++)
        Physics_StepBall(&c, lv, PHYSICS_DT);
    CHECK(c.pos.x > 310.0f);                                  /* now it flies straight through */
    Level_Destroy(lv);
}

/* ---- recursion: aim-assist path prediction ------------------------------ */
static void test_predict_matches_real_flight(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball start = ball_at(50, 150, 220, -300);
    Ball before = start;

    Vec2 pts[MAX_PREVIEW_POINTS];
    int n = Physics_PredictPath(lv, start, MAX_PREVIEW_POINTS * 3, pts, 0, MAX_PREVIEW_POINTS);
    CHECK(n > 5);
    CHECK(memcmp(&before, &start, sizeof start) == 0);        /* by-value: caller's ball untouched */

    Ball real = start;                                        /* predicted[k] = state after 3k+1 ticks */
    int tick = 0;
    for (int k = 0; k < n; k++) {
        while (tick < 3 * k + 1) { Physics_StepBall(&real, lv, PHYSICS_DT); tick++; }
        CHECK_NEAR(pts[k].x, real.pos.x, 1e-3);
        CHECK_NEAR(pts[k].y, real.pos.y, 1e-3);
    }
    Level_Destroy(lv);
}

static void test_predict_base_cases(void)
{
    Level *lv = Level_CreateFromData(&FLAT_LVL);
    Ball b = ball_at(50, 150, 100, 0);
    Vec2 pts[MAX_PREVIEW_POINTS];

    CHECK_INT(Physics_PredictPath(lv, b, 0, pts, 0, MAX_PREVIEW_POINTS), 0);      /* no ticks left   */
    CHECK_INT(Physics_PredictPath(lv, b, 90, pts, 0, 0), 0);                      /* no room         */
    CHECK_INT(Physics_PredictPath(lv, b, 90, pts, 0, 5), 5);                      /* bounded by max  */
    CHECK_INT(Physics_PredictPath(lv, b, 9, pts, 0, MAX_PREVIEW_POINTS), 3);      /* 9 ticks -> 3 pts */
    CHECK_INT(Physics_PredictPath(lv, b, 90, pts, 7, MAX_PREVIEW_POINTS), 7 + 30);/* continues from count */
    Level_Destroy(lv);

    lv = Level_CreateFromData(&HAZ_LVL);                      /* path ends when the ball would die */
    Ball falling = ball_at(120, 60, 0, 0);
    int n = Physics_PredictPath(lv, falling, MAX_PREVIEW_POINTS * 3, pts, 0, MAX_PREVIEW_POINTS);
    CHECK(n < MAX_PREVIEW_POINTS);
    for (int i = 0; i < n; i++)
        CHECK(pts[i].y < 100.0f);                             /* every kept point is before the hazard */
    Level_Destroy(lv);
}

/* ---- real data -------------------------------------------------------- */
static void test_real_level0_settles(void)
{
    Level *lv = Level_Create(0);
    Ball b = { lv->def->player_spawn, { 0, 0 } };
    unsigned ev = 0;
    for (int i = 0; i < 300; i++)
        ev |= Physics_StepBall(&b, lv, PHYSICS_DT);
    CHECK_INT(ev & (EVT_HAZARD | EVT_OOB), 0);
    CHECK(V2_Len(b.vel) < 5.0f);                              /* came to rest in the valley */
    CHECK(b.pos.y > 160.0f && b.pos.y < 190.0f);
    Level_Destroy(lv);
}

void run_physics_tests(void)
{
    (void)FULL_LVL;
    RUN(test_point_in_polygon);
    RUN(test_circle_segment);
    RUN(test_circle_polygon);
    RUN(test_out_of_bounds);
    RUN(test_launch_velocity);
    RUN(test_free_fall);
    RUN(test_projectile_motion);
    RUN(test_bounce_restitution);
    RUN(test_comes_to_rest);
    RUN(test_horizontal_slide_keeps_speed);
    RUN(test_slope_acceleration);
    RUN(test_no_tunnelling);
    RUN(test_deep_penetration_recovery);
    RUN(test_hazard_and_oob_events);
    RUN(test_wall_collision);
    RUN(test_predict_matches_real_flight);
    RUN(test_predict_base_cases);
    RUN(test_real_level0_settles);
}
