/* test_raster.c - software polygon rasteriser, shaders, level layers */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "raster.h"
#include "physics.h"
#include "vec2.h"

static int same_color(Rgba a, Rgba b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

static int count_painted(const Canvas *c)
{
    int n = 0;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++)
            if (Canvas_Get(c, x, y).a != 0) n++;
    return n;
}

static void test_canvas_basics(void)
{
    Canvas c;
    CHECK(Canvas_Create(&c, 8, 4));
    CHECK_INT(c.w, 8);
    CHECK_INT(c.h, 4);
    CHECK_INT(count_painted(&c), 0);                           /* starts fully transparent */

    Rgba red = { 255, 0, 0, 255 };
    Canvas_Put(&c, 3, 2, red);
    CHECK(same_color(Canvas_Get(&c, 3, 2), red));
    CHECK_INT(count_painted(&c), 1);

    Canvas_Put(&c, -1, 0, red);                                /* out of range: silently ignored */
    Canvas_Put(&c, 8, 0, red);
    Canvas_Put(&c, 0, 4, red);
    Canvas_Put(&c, 0, -1, red);
    CHECK_INT(count_painted(&c), 1);
    CHECK_INT(Canvas_Get(&c, -5, 0).a, 0);                     /* ... and reads give "transparent" */
    CHECK_INT(Canvas_Get(&c, 99, 99).a, 0);

    Canvas_Clear(&c);
    CHECK_INT(count_painted(&c), 0);
    Canvas_Destroy(&c);
    CHECK(c.pixels == NULL);
    Canvas_Destroy(&c);                                        /* double destroy is harmless */
}

static void test_fill_rectangle_exact(void)
{
    Canvas c; Canvas_Create(&c, 64, 64);
    Rgba col = { 10, 20, 30, 255 };
    Vec2 rect[] = { {10, 10}, {20, 10}, {20, 30}, {10, 30} };

    Raster_FillPolygon(&c, rect, 4, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 10 * 20);                     /* exactly width x height pixels */
    CHECK(same_color(Canvas_Get(&c, 10, 10), col));            /* top-left pixel included */
    CHECK(same_color(Canvas_Get(&c, 19, 29), col));            /* last pixel included */
    CHECK_INT(Canvas_Get(&c, 20, 10).a, 0);                    /* right/bottom edge excluded */
    CHECK_INT(Canvas_Get(&c, 10, 30).a, 0);
    CHECK_INT(Canvas_Get(&c, 9, 15).a, 0);

    Canvas_Clear(&c);
    Raster_FillPolygon(&c, rect, 4, 2.0f, Shader_Solid, &col); /* zoom 2 doubles both sides */
    CHECK_INT(count_painted(&c), 20 * 40);

    Canvas_Clear(&c);
    Raster_FillPolygon(&c, rect, 4, 0.5f, Shader_Solid, &col); /* zoom 0.5 halves them */
    CHECK_INT(count_painted(&c), 5 * 10);
    Canvas_Destroy(&c);
}

static void test_fill_triangle_area(void)
{
    Canvas c; Canvas_Create(&c, 128, 128);
    Rgba col = { 1, 2, 3, 255 };
    Vec2 tri[] = { {0, 0}, {100, 0}, {0, 100} };
    Raster_FillPolygon(&c, tri, 3, 1.0f, Shader_Solid, &col);
    int n = count_painted(&c);
    CHECK(abs(n - 5000) < 150);                                /* area 100*100/2, within 3% */
    CHECK_INT(Canvas_Get(&c, 10, 10).a, 255);
    CHECK_INT(Canvas_Get(&c, 90, 90).a, 0);                    /* beyond the hypotenuse */
    Canvas_Destroy(&c);
}

static void test_fill_concave_and_self_intersecting(void)
{
    Canvas c; Canvas_Create(&c, 64, 64);
    Rgba col = { 9, 9, 9, 255 };

    Vec2 ell[] = { {0, 0}, {20, 0}, {20, 10}, {10, 10}, {10, 20}, {0, 20} };   /* L shape: 300 px */
    Raster_FillPolygon(&c, ell, 6, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 300);
    CHECK_INT(Canvas_Get(&c, 15, 15).a, 0);                    /* the notch stays empty */
    CHECK_INT(Canvas_Get(&c, 5, 15).a, 255);

    Canvas_Clear(&c);
    Vec2 bow[] = { {0, 0}, {20, 20}, {20, 0}, {0, 20} };       /* hourglass: even-odd rule */
    Raster_FillPolygon(&c, bow, 4, 1.0f, Shader_Solid, &col);
    CHECK_INT(Canvas_Get(&c, 3, 10).a, 255);                   /* left triangle */
    CHECK_INT(Canvas_Get(&c, 17, 10).a, 255);                  /* right triangle */
    CHECK_INT(Canvas_Get(&c, 10, 3).a, 0);                     /* top gap */
    CHECK_INT(Canvas_Get(&c, 10, 17).a, 0);                    /* bottom gap */

    Canvas_Clear(&c);
    Vec2 holes[] = { {0, 0}, {30, 0}, {30, 30}, {0, 30}, {0, 0},          /* outer square ... */
                     {10, 10}, {10, 20}, {20, 20}, {20, 10}, {10, 10} };  /* ... with an inner hole */
    Raster_FillPolygon(&c, holes, 10, 1.0f, Shader_Solid, &col);
    CHECK_INT(Canvas_Get(&c, 15, 15).a, 0);
    CHECK_INT(Canvas_Get(&c, 5, 5).a, 255);
    Canvas_Destroy(&c);
}

static void test_fill_clipping_and_bad_input(void)
{
    Canvas c; Canvas_Create(&c, 32, 32);
    Rgba col = { 5, 5, 5, 255 };

    Vec2 big[] = { {-100, -100}, {200, -100}, {200, 200}, {-100, 200} };   /* covers everything */
    Raster_FillPolygon(&c, big, 4, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 32 * 32);

    Canvas_Clear(&c);
    Vec2 off[] = { {100, 100}, {120, 100}, {120, 120}, {100, 120} };       /* completely outside */
    Raster_FillPolygon(&c, off, 4, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 0);

    Vec2 half[] = { {-10, 0}, {10, 0}, {10, 10}, {-10, 10} };              /* partly outside */
    Raster_FillPolygon(&c, half, 4, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 10 * 10);

    Canvas_Clear(&c);
    Vec2 two[] = { {0, 0}, {10, 10} };                                     /* fewer than 3 points: ignored */
    Raster_FillPolygon(&c, two, 2, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 0);

    Vec2 line[] = { {0, 5}, {10, 5}, {20, 5} };                            /* degenerate (zero area) */
    Raster_FillPolygon(&c, line, 3, 1.0f, Shader_Solid, &col);
    CHECK_INT(count_painted(&c), 0);
    Canvas_Destroy(&c);
}

static void test_shaders(void)
{
    Rgba col = { 40, 50, 60, 255 };
    CHECK(same_color(Shader_Solid(3, 4, &col), col));
    CHECK(same_color(Shader_Solid(300, 400, &col), col));

    Rgba tile[4] = { {1,0,0,255}, {2,0,0,255}, {3,0,0,255}, {4,0,0,255} };   /* 2x2 pattern */
    TileCtx t = { tile, 2, 2, 1.0f };
    CHECK_INT(Shader_Tiled(0, 0, &t).r, 1);
    CHECK_INT(Shader_Tiled(1, 0, &t).r, 2);
    CHECK_INT(Shader_Tiled(0, 1, &t).r, 3);
    CHECK_INT(Shader_Tiled(1, 1, &t).r, 4);
    CHECK_INT(Shader_Tiled(2, 2, &t).r, 1);                    /* repeats */
    CHECK_INT(Shader_Tiled(5, 3, &t).r, 4);
    CHECK_INT(Shader_Tiled(100, 101, &t).r, 3);

    /* zoom 0.5: one screen pixel spans 2 world pixels; the sample is taken at the pixel centre:
     *   screen 0 -> world floor(0.5/0.5) = 1  -> tile column/row 1  -> tile[3] */
    TileCtx half = { tile, 2, 2, 0.5f };
    CHECK_INT(Shader_Tiled(0, 0, &half).r, 4);
    /* zoom 2: every tile pixel is magnified to 2x2 screen pixels */
    TileCtx twice = { tile, 2, 2, 2.0f };
    CHECK_INT(Shader_Tiled(0, 0, &twice).r, 1);
    CHECK_INT(Shader_Tiled(1, 1, &twice).r, 1);
    CHECK_INT(Shader_Tiled(2, 0, &twice).r, 2);
    CHECK_INT(Shader_Tiled(0, 2, &twice).r, 3);
    CHECK_INT(Shader_Tiled(2, 2, &twice).r, 4);
    CHECK_INT(Shader_Tiled(4, 4, &twice).r, 1);               /* and the pattern repeats */
}

static void test_tiled_fill(void)
{
    Canvas c; Canvas_Create(&c, 16, 16);
    Rgba tile[4] = { {1,0,0,255}, {2,0,0,255}, {3,0,0,255}, {4,0,0,255} };
    TileCtx t = { tile, 2, 2, 1.0f };
    Vec2 rect[] = { {0, 0}, {8, 0}, {8, 8}, {0, 8} };
    Raster_FillPolygon(&c, rect, 4, 1.0f, Shader_Tiled, &t);
    CHECK_INT(Canvas_Get(&c, 0, 0).r, 1);
    CHECK_INT(Canvas_Get(&c, 1, 0).r, 2);
    CHECK_INT(Canvas_Get(&c, 0, 1).r, 3);
    CHECK_INT(Canvas_Get(&c, 7, 7).r, 4);
    Canvas_Destroy(&c);
}

static void test_stroke(void)
{
    Canvas c; Canvas_Create(&c, 64, 64);
    Rgba col = { 200, 200, 200, 255 };
    Vec2 sq[] = { {10, 10}, {50, 10}, {50, 50}, {10, 50} };
    Raster_StrokePolygon(&c, sq, 4, 1.0f, 4.0f, col);

    CHECK_INT(Canvas_Get(&c, 30, 10).a, 255);                  /* on the top edge */
    CHECK_INT(Canvas_Get(&c, 30, 11).a, 255);                  /* within half the thickness */
    CHECK_INT(Canvas_Get(&c, 30, 9).a, 255);
    CHECK_INT(Canvas_Get(&c, 30, 13).a, 0);                    /* beyond 2 px: empty */
    CHECK_INT(Canvas_Get(&c, 30, 30).a, 0);                    /* the inside is NOT filled */
    CHECK_INT(Canvas_Get(&c, 5, 30).a, 0);                     /* outside */
    CHECK_INT(Canvas_Get(&c, 10, 10).a, 255);                  /* corners are covered */
    CHECK_INT(Canvas_Get(&c, 50, 50).a, 255);

    Canvas_Clear(&c);                                          /* thinner line paints fewer pixels */
    Raster_StrokePolygon(&c, sq, 4, 1.0f, 2.0f, col);
    int thin = count_painted(&c);
    Canvas_Clear(&c);
    Raster_StrokePolygon(&c, sq, 4, 1.0f, 6.0f, col);
    CHECK(count_painted(&c) > thin * 2);
    Canvas_Destroy(&c);
}

/* ---- the 25 real levels ----------------------------------------------------------- */
static int inside_solid(const LevelData *lv, Vec2 world)
{
    for (int i = 0; i < lv->polyland_count; i++) {
        const PolygonData *p = &lv->polylands[i];
        if (Physics_PointInPolygon(world, p->points, p->point_count))
            return 1;
    }
    return 0;
}

static void test_level_layers(void)
{
    Canvas c; Canvas_Create(&c, VIRTUAL_SIZE, VIRTUAL_SIZE);
    Rgba tile[12 * 12];
    for (int i = 0; i < 144; i++) { Rgba t = { 231, 255, 238, (i % 3 == 0) ? 255 : 0 }; tile[i] = t; }
    const Rgba land = COL_LAND;

    for (int i = 0; i < LEVEL_COUNT; i++) {
        const LevelData *lv = &g_levels[i];
        Raster_BuildLevelLayer(&c, lv, tile, 12, 12);
        int painted = count_painted(&c);
        CHECK(painted > 500);                                  /* every level has real terrain */
        CHECK(painted < VIRTUAL_SIZE * VIRTUAL_SIZE);          /* ... and some empty space */

        /* the ball spawns in the open: the pixel there is not land */
        int sx = (int)(lv->player_spawn.x * lv->camera_zoom);
        int sy = (int)(lv->player_spawn.y * lv->camera_zoom);
        CHECK(!same_color(Canvas_Get(&c, sx, sy), land));

        /* WHAT YOU SEE IS WHAT YOU COLLIDE WITH: orange pixels <=> inside a solid polygon
         * (tiny disagreement allowed only for pixels sitting exactly on an edge)        */
        int mismatches = 0, land_pixels = 0;
        for (int y = 0; y < VIRTUAL_SIZE; y++) {
            for (int x = 0; x < VIRTUAL_SIZE; x++) {
                Vec2 w = V2(((float)x + 0.5f) / lv->camera_zoom, ((float)y + 0.5f) / lv->camera_zoom);
                int is_land = same_color(Canvas_Get(&c, x, y), land);
                if (is_land) land_pixels++;
                if (is_land != inside_solid(lv, w)) mismatches++;
            }
        }
        CHECK(land_pixels > 300);
        CHECK(mismatches * 1000 < VIRTUAL_SIZE * VIRTUAL_SIZE);   /* < 0.1 % of all pixels */
    }
    Canvas_Destroy(&c);
}

static void test_hazard_layer(void)
{
    Canvas c; Canvas_Create(&c, VIRTUAL_SIZE, VIRTUAL_SIZE);
    Rgba tile[4] = { {7,7,7,255}, {7,7,7,255}, {7,7,7,255}, {7,7,7,255} };
    const Rgba outline = COL_OUTLINE;
    int lvl = 17;                                              /* first level with hazards */
    CHECK(g_levels[lvl].death_polyland_count > 0);

    Raster_BuildLevelLayer(&c, &g_levels[lvl], tile, 2, 2);
    int tile_px = 0, outline_px = 0;
    for (int y = 0; y < VIRTUAL_SIZE; y++)
        for (int x = 0; x < VIRTUAL_SIZE; x++) {
            Rgba p = Canvas_Get(&c, x, y);
            if (p.r == 7 && p.a == 255) tile_px++;
            if (same_color(p, outline)) outline_px++;
        }
    CHECK(tile_px > 100);                                      /* hazard interior uses the tile */
    CHECK(outline_px > 100);                                   /* ... and has a bright outline */

    Raster_BuildLevelLayer(&c, &g_levels[lvl], NULL, 0, 0);    /* no tile available: outline only, no crash */
    tile_px = 0;
    for (int y = 0; y < VIRTUAL_SIZE; y++)
        for (int x = 0; x < VIRTUAL_SIZE; x++)
            if (Canvas_Get(&c, x, y).r == 7) tile_px++;
    CHECK_INT(tile_px, 0);
    Canvas_Destroy(&c);
}

/* The hazard stripes are anchored to SCREEN pixels, so they repeat every 12 px whatever the
 * camera zoom is (anchored to the world they would alias into noise on zoomed-out levels). */
static void test_hazard_stripes_are_regular_at_every_zoom(void)
{
    Canvas c; Canvas_Create(&c, VIRTUAL_SIZE, VIRTUAL_SIZE);
    Rgba tile[12 * 12];
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 12; x++) {
            Rgba t = { (uint8_t)(((x + y) % 12 < 5) ? 7 : 9), 0, 0, 255 };   /* diagonal stripes: r = 7 or 9 */
            tile[y * 12 + x] = t;
        }
    int tested = 0, zoomed_out = 0;
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        if (g_levels[lvl].death_polyland_count == 0)
            continue;                                             /* this level has no hazards */
        Raster_BuildLevelLayer(&c, &g_levels[lvl], tile, 12, 12);
        int pairs = 0, mismatches = 0;
        for (int y = 0; y < VIRTUAL_SIZE; y++)
            for (int x = 0; x + 12 < VIRTUAL_SIZE; x++) {
                Rgba a = Canvas_Get(&c, x, y), b = Canvas_Get(&c, x + 12, y);
                if ((a.r == 7 || a.r == 9) && a.g == 0 && (b.r == 7 || b.r == 9) && b.g == 0) {
                    pairs++;
                    if (a.r != b.r) mismatches++;
                }
            }
        if (pairs == 0)
            continue;                                             /* hazard entirely off-screen (level 19) */
        CHECK(pairs > 50);                                        /* there is hazard to look at */
        CHECK_INT(mismatches, 0);                                 /* ... and the pattern repeats every 12 px */
        tested++;
        if (g_levels[lvl].camera_zoom < 0.99f) zoomed_out++;
    }
    CHECK(tested >= 5);
    CHECK(zoomed_out >= 2);                                       /* the zoomed-out levels are the ones that used to alias */
    Canvas_Destroy(&c);
}

void run_raster_tests(void)
{
    RUN(test_canvas_basics);
    RUN(test_fill_rectangle_exact);
    RUN(test_fill_triangle_area);
    RUN(test_fill_concave_and_self_intersecting);
    RUN(test_fill_clipping_and_bad_input);
    RUN(test_shaders);
    RUN(test_tiled_fill);
    RUN(test_stroke);
    RUN(test_level_layers);
    RUN(test_hazard_layer);
    RUN(test_hazard_stripes_are_regular_at_every_zoom);
}
