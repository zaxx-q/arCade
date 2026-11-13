/* test_level.c - level data integrity, runtime state, walls, triggers */
#include <math.h>
#include <string.h>
#include "test_framework.h"
#include "level.h"
#include "physics.h"
#include "vec2.h"

static int inside_any(const PolygonData *polys, int count, Vec2 p)
{
    for (int i = 0; i < count; i++)
        if (Physics_PointInPolygon(p, polys[i].points, polys[i].point_count))
            return 1;
    return 0;
}

/* ---- the 25 built-in levels -------------------------------------------- */
static void test_level_data_integrity(void)
{
    CHECK_INT(LEVEL_COUNT, 25);
    for (int i = 0; i < LEVEL_COUNT; i++) {
        const LevelData *d = &g_levels[i];
        CHECK_INT(d->index, i);

        float z = d->camera_zoom;                                  /* one of the 4 allowed zooms */
        CHECK(fabsf(z - 1.0f) < 1e-3f || fabsf(z - 0.75f) < 1e-3f ||
              fabsf(z - 0.66f) < 1e-3f || fabsf(z - 0.33f) < 1e-3f);

        CHECK(d->polyland_count >= 1);
        for (int p = 0; p < d->polyland_count; p++) {
            CHECK(d->polylands[p].point_count >= 3);
            CHECK(d->polylands[p].point_count <= MAX_POLY_POINTS);
        }
        for (int p = 0; p < d->death_polyland_count; p++) {
            CHECK(d->death_polylands[p].point_count >= 3);
            CHECK(d->death_polylands[p].point_count <= MAX_POLY_POINTS);
        }
        CHECK(d->wall_count >= 0 && d->button_count >= 0 && d->strike_count >= 0);
        for (int b = 0; b < d->button_count; b++)                  /* a button targets an existing wall */
            CHECK(d->buttons[b].target_wall_index >= -1 &&
                  d->buttons[b].target_wall_index < d->wall_count);

        float world = (float)VIRTUAL_SIZE / z;                      /* spawn is inside the visible world */
        CHECK(d->player_spawn.x > 0.0f && d->player_spawn.x < world);
        CHECK(d->player_spawn.y > 0.0f && d->player_spawn.y < world);
        CHECK(!inside_any(d->polylands, d->polyland_count, d->player_spawn));        /* not inside rock   */
        CHECK(!inside_any(d->death_polylands, d->death_polyland_count, d->player_spawn)); /* nor a hazard */

        CHECK_INT(d->has_banner, i == 0);                          /* only the tutorial has the banner */
        for (int w = 0; w < d->wall_count; w++) {
            CHECK(d->walls[w].width > 0.0f && d->walls[w].height > 0.0f);
            CHECK(d->walls[w].tween_duration > 0.0f);
            CHECK(d->walls[w].tween_type == TWEEN_BACK_OUT || d->walls[w].tween_type == TWEEN_CUBIC_IN_OUT);
        }
    }
}

static void test_level_names(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        CHECK(strlen(Level_Name(i)) > 0);
        CHECK(strcmp(Level_Name(i), "?") != 0);
        for (int j = i + 1; j < LEVEL_COUNT; j++)                  /* all different */
            CHECK(strcmp(Level_Name(i), Level_Name(j)) != 0);
    }
    CHECK_STR(Level_Name(0), "Tutorial");
    CHECK_STR(Level_Name(24), "The Gauntlet");
    CHECK_STR(Level_Name(-1), "?");
    CHECK_STR(Level_Name(25), "?");
}

static void test_create_destroy(void)
{
    CHECK(Level_Create(-1) == NULL);
    CHECK(Level_Create(25) == NULL);
    CHECK(Level_Create(1000) == NULL);
    CHECK(Level_CreateFromData(NULL) == NULL);
    Level_Destroy(NULL);                                           /* must be harmless */

    for (int i = 0; i < LEVEL_COUNT; i++) {                        /* LeakSanitizer checks the frees */
        Level *lv = Level_Create(i);
        CHECK(lv != NULL);
        if (lv == NULL) continue;
        CHECK(lv->def == &g_levels[i]);
        CHECK_INT(lv->wall_count, g_levels[i].wall_count);
        CHECK_INT(lv->button_count, g_levels[i].button_count);
        CHECK_INT(lv->strike_count, g_levels[i].strike_count);
        CHECK(lv->wall_count == 0 || lv->walls != NULL);
        CHECK(lv->button_count == 0 || lv->buttons != NULL);
        CHECK(lv->strike_count == 0 || lv->strike_active != NULL);
        Level_Destroy(lv);
    }
}

static void test_initial_and_reset_state(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int s = 0; s < lv->strike_count; s++) CHECK(lv->strike_active[s]);
        for (int b = 0; b < lv->button_count; b++) CHECK(!lv->buttons[b].pressed);
        for (int w = 0; w < lv->wall_count; w++) {
            CHECK(!lv->walls[w].opening);
            CHECK_NEAR(V2_Len(lv->walls[w].offset), 0.0, 1e-9);
            CHECK(lv->walls[w].def == &lv->def->walls[w]);
        }
        /* dirty everything, then reset */
        for (int s = 0; s < lv->strike_count; s++) lv->strike_active[s] = false;
        for (int b = 0; b < lv->button_count; b++) lv->buttons[b].pressed = true;
        for (int w = 0; w < lv->wall_count; w++) { lv->walls[w].opening = true; lv->walls[w].offset = V2(5, 5); lv->walls[w].elapsed = 1.0f; }
        Level_Reset(lv);
        for (int s = 0; s < lv->strike_count; s++) CHECK(lv->strike_active[s]);
        for (int b = 0; b < lv->button_count; b++) CHECK(!lv->buttons[b].pressed);
        for (int w = 0; w < lv->wall_count; w++) {
            CHECK(!lv->walls[w].opening);
            CHECK_NEAR(V2_Len(lv->walls[w].offset), 0.0, 1e-9);
            CHECK_NEAR(lv->walls[w].elapsed, 0.0, 1e-9);
        }
        Level_Destroy(lv);
    }
}

static void test_world_size(void)
{
    Level *lv = Level_Create(0);                                   /* zoom 1.0 */
    CHECK_NEAR(Level_WorldSize(lv), 256.0, 1e-3);
    Level_Destroy(lv);
    lv = Level_Create(19);                                         /* zoom 0.33 */
    CHECK_NEAR(Level_WorldSize(lv), 256.0 / 0.33, 0.05);
    Level_Destroy(lv);
}

/* ---- wall geometry ------------------------------------------------------- */
static void bounding_box(const Vec2 c[4], float *x0, float *x1, float *y0, float *y1)
{
    *x0 = *x1 = c[0].x; *y0 = *y1 = c[0].y;
    for (int i = 1; i < 4; i++) {
        if (c[i].x < *x0) *x0 = c[i].x;
        if (c[i].x > *x1) *x1 = c[i].x;
        if (c[i].y < *y0) *y0 = c[i].y;
        if (c[i].y > *y1) *y1 = c[i].y;
    }
}

static void test_wall_corners_known_walls(void)
{
    float x0, x1, y0, y1;
    Vec2 c[4];

    Level *lv = Level_Create(14);                  /* wall 0: pivot (0,171), rot -90 deg, 17 x 156 */
    Level_WallCorners(&lv->walls[0], c);
    bounding_box(c, &x0, &x1, &y0, &y1);
    CHECK_NEAR(x0, 0.0, 0.01);   CHECK_NEAR(x1, 156.0, 0.01);
    CHECK_NEAR(y0, 162.5, 0.01); CHECK_NEAR(y1, 179.5, 0.01);
    Level_Destroy(lv);

    lv = Level_Create(24);                         /* the wall that blocks the shaft at x ~ 246 */
    Level_WallCorners(&lv->walls[0], c);
    bounding_box(c, &x0, &x1, &y0, &y1);
    CHECK_NEAR(x0, 225.0, 0.01); CHECK_NEAR(x1, 275.0, 0.01);
    CHECK_NEAR(y0, 141.5, 0.01); CHECK_NEAR(y1, 158.5, 0.01);
    CHECK(246.0f > x0 && 246.0f < x1);             /* it really covers the strike pickups' column */
    Level_Destroy(lv);
}

static void test_wall_corners_offset_and_shape(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int w = 0; w < lv->wall_count; w++) {
            Vec2 a[4], b[4];
            Level_WallCorners(&lv->walls[w], a);
            /* rectangle: opposite sides equal, sides have the data's width/height */
            CHECK_NEAR(V2_Dist(a[0], a[1]), lv->walls[w].def->width, 1e-3);
            CHECK_NEAR(V2_Dist(a[1], a[2]), lv->walls[w].def->height, 1e-3);
            CHECK_NEAR(V2_Dist(a[2], a[3]), lv->walls[w].def->width, 1e-3);
            CHECK_NEAR(V2_Dist(a[3], a[0]), lv->walls[w].def->height, 1e-3);
            CHECK_NEAR(V2_Dot(V2_Sub(a[1], a[0]), V2_Sub(a[2], a[1])), 0.0, 0.5);   /* right angle */
            /* an offset translates every corner by the same vector */
            lv->walls[w].offset = V2(10.0f, -20.0f);
            Level_WallCorners(&lv->walls[w], b);
            for (int k = 0; k < 4; k++) {
                CHECK_NEAR(b[k].x - a[k].x, 10.0, 1e-3);
                CHECK_NEAR(b[k].y - a[k].y, -20.0, 1e-3);
            }
        }
        Level_Destroy(lv);
    }
}

/* ---- wall animation ------------------------------------------------------ */
static void test_wall_tween(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int w = 0; w < lv->wall_count; w++) {
            WallState *ws = &lv->walls[w];
            Vec2 target = ws->def->open_displacement;

            Level_Update(lv, 0.1f);                               /* not opening: nothing moves */
            CHECK_NEAR(V2_Len(ws->offset), 0.0, 1e-9);

            ws->opening = true;
            ws->elapsed = 0.0f;
            Level_Update(lv, ws->def->tween_duration * 0.5f);     /* halfway in time */
            CHECK(ws->opening);
            CHECK(V2_Len(ws->offset) > 0.0f);

            Level_Update(lv, ws->def->tween_duration);            /* far past the end */
            CHECK(!ws->opening);
            CHECK_NEAR(ws->offset.x, target.x, 1e-3);             /* ends EXACTLY on the target */
            CHECK_NEAR(ws->offset.y, target.y, 1e-3);

            Level_Update(lv, 1.0f);                               /* stays there */
            CHECK_NEAR(ws->offset.y, target.y, 1e-3);
        }
        Level_Destroy(lv);
    }
}

static void test_back_out_overshoots(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int w = 0; w < lv->wall_count; w++) {
            WallState *ws = &lv->walls[w];
            if (ws->def->tween_type != TWEEN_BACK_OUT) continue;
            float target = V2_Len(ws->def->open_displacement);
            float max_seen = 0.0f;
            ws->opening = true; ws->elapsed = 0.0f;
            for (int t = 0; t < 30; t++) {
                Level_Update(lv, 0.01f);
                if (V2_Len(ws->offset) > max_seen) max_seen = V2_Len(ws->offset);
            }
            CHECK(max_seen > target);                             /* overshoot, then settles */
        }
        Level_Destroy(lv);
    }
}

/* ---- triggers ------------------------------------------------------------ */
static void test_flag_trigger_real_levels(void)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        CHECK(Level_CheckTriggers(lv, lv->def->flagpole_pos) & EVT_GOAL);
        CHECK_INT(Level_CheckTriggers(lv, lv->def->player_spawn) & EVT_GOAL, 0);   /* no free wins */

        Vec2 near_edge = V2_Add(lv->def->flagpole_pos, V2(BALL_RADIUS + FLAG_RADIUS - 0.1f, 0));
        CHECK(Level_CheckTriggers(lv, near_edge) & EVT_GOAL);
        Vec2 just_out = V2_Add(lv->def->flagpole_pos, V2(BALL_RADIUS + FLAG_RADIUS + 0.1f, 0));
        CHECK_INT(Level_CheckTriggers(lv, just_out) & EVT_GOAL, 0);
        Level_Destroy(lv);
    }
}

static void test_button_triggers_real_levels(void)
{
    int tested = 0;
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int b = 0; b < lv->button_count; b++) {
            Vec2 pos = lv->def->buttons[b].position;
            unsigned ev = Level_CheckTriggers(lv, pos);
            CHECK(ev & EVT_BUTTON);
            CHECK(lv->buttons[b].pressed);
            int t = lv->def->buttons[b].target_wall_index;
            if (t >= 0) {
                CHECK(lv->walls[t].opening);
                CHECK_INT((ev & EVT_WALL_OPEN) != 0, lv->walls[t].def->play_hit_sfx);
            }
            CHECK_INT(Level_CheckTriggers(lv, pos) & EVT_BUTTON, 0);   /* a pressed button is silent */
            tested++;
        }
        Level_Destroy(lv);
    }
    CHECK(tested >= 6);                                               /* the game has several buttons */
}

static void test_strike_pickups_real_levels(void)
{
    int tested = 0;
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        for (int s = 0; s < lv->strike_count; s++) {
            Vec2 pos = lv->def->strikes[s];
            CHECK(Level_CheckTriggers(lv, pos) & EVT_PICKUP);
            CHECK(!lv->strike_active[s]);
            CHECK_INT(Level_CheckTriggers(lv, pos) & EVT_PICKUP, 0);   /* collected only once */
            Level_Reset(lv);
            CHECK(lv->strike_active[s]);                               /* back after a reset */
            tested++;
        }
        Level_Destroy(lv);
    }
    CHECK(tested >= 5);
}

static void test_trigger_distances(void)
{
    /* find any level with a strike, check the exact radius (ball 4 + pickup 16.0312) */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        Level *lv = Level_Create(i);
        if (lv->strike_count > 0) {
            Vec2 c = lv->def->strikes[0];
            CHECK_INT(Level_CheckTriggers(lv, V2_Add(c, V2(20.1f, 0))) & EVT_PICKUP, 0);
            CHECK(Level_CheckTriggers(lv, V2_Add(c, V2(19.9f, 0))) & EVT_PICKUP);
            Level_Destroy(lv);
            break;
        }
        Level_Destroy(lv);
    }
}

void run_level_tests(void)
{
    RUN(test_level_data_integrity);
    RUN(test_level_names);
    RUN(test_create_destroy);
    RUN(test_initial_and_reset_state);
    RUN(test_world_size);
    RUN(test_wall_corners_known_walls);
    RUN(test_wall_corners_offset_and_shape);
    RUN(test_wall_tween);
    RUN(test_back_out_overshoots);
    RUN(test_flag_trigger_real_levels);
    RUN(test_button_triggers_real_levels);
    RUN(test_strike_pickups_real_levels);
    RUN(test_trigger_distances);
}
