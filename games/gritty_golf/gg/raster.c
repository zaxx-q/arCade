/*
 * raster.c  -  scanline polygon filling
 *
 *   [T09 Array]     the canvas is a 1-D array treated as a 2-D grid:
 *                   index = (y * width + x) * 4
 *   [T06 Loop]      nested loops: rows -> edges -> pixels
 *   [T14 Sorting]   insertion sort of the crossing positions
 *   [T10 Pointer]   function pointer (shader) + pointer arithmetic
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "raster.h"
#include "algo.h"
#include "vec2.h"

bool Canvas_Create(Canvas *c, int w, int h)
{
    c->w = w;
    c->h = h;
    c->pixels = calloc((size_t)w * (size_t)h * 4u, 1);    /* zero = transparent */
    return c->pixels != NULL;
}

void Canvas_Destroy(Canvas *c)
{
    free(c->pixels);
    c->pixels = NULL;
}

void Canvas_Clear(Canvas *c)
{
    memset(c->pixels, 0, (size_t)c->w * (size_t)c->h * 4u);
}

/* Bounds-checked pixel write: ignores coordinates outside the canvas. */
void Canvas_Put(Canvas *c, int x, int y, Rgba color)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return;
    uint8_t *p = c->pixels + ((size_t)y * (size_t)c->w + (size_t)x) * 4u;
    p[0] = color.r; p[1] = color.g; p[2] = color.b; p[3] = color.a;
}

Rgba Canvas_Get(const Canvas *c, int x, int y)
{
    Rgba none = { 0, 0, 0, 0 };
    if (x < 0 || y < 0 || x >= c->w || y >= c->h)
        return none;
    const uint8_t *p = c->pixels + ((size_t)y * (size_t)c->w + (size_t)x) * 4u;
    Rgba out = { p[0], p[1], p[2], p[3] };
    return out;
}

/* ---- shaders -------------------------------------------------------- */
Rgba Shader_Solid(int x, int y, const void *ctx)
{
    (void)x; (void)y;                       /* unused: same colour everywhere */
    return *(const Rgba *)ctx;              /* cast the generic pointer back  */
}

/* Repeat a small image over the polygon; the pattern is anchored to WORLD
 * coordinates so it scales together with the camera zoom.                   */
Rgba Shader_Tiled(int x, int y, const void *ctx)
{
    const TileCtx *t = ctx;
    int wx = (int)floorf(((float)x + 0.5f) / t->zoom);       /* [T12] floorf */
    int wy = (int)floorf(((float)y + 0.5f) / t->zoom);
    int tx = ((wx % t->tw) + t->tw) % t->tw;                 /* positive modulo */
    int ty = ((wy % t->th) + t->th) % t->th;
    return t->tile[ty * t->tw + tx];
}

/*
 * Raster_FillPolygon : SCANLINE FILL with the even-odd rule
 *
 *   PSEUDOCODE
 *     convert the polygon to screen space (x * zoom, y * zoom)
 *     FOR each pixel row y inside the polygon's vertical extent
 *        yc <- y + 0.5                              // sample the pixel centre
 *        xs[] <- x where every edge crosses the line y = yc
 *        sort xs[] ascending                        // insertion sort
 *        FOR each PAIR (xs[0],xs[1]), (xs[2],xs[3]) ...
 *           colour every pixel between the pair     // inside the polygon
 */
void Raster_FillPolygon(Canvas *c, const Vec2 *pts, int n, float zoom,
                        PixelShader shader, const void *ctx)
{
    if (n < 3 || n > MAX_POLY_POINTS)
        return;

    Vec2 s[MAX_POLY_POINTS];                       /* polygon in screen space */
    float ymin = 1e9f, ymax = -1e9f;
    for (int i = 0; i < n; i++) {
        s[i] = V2_Scale(pts[i], zoom);
        if (s[i].y < ymin) ymin = s[i].y;
        if (s[i].y > ymax) ymax = s[i].y;
    }

    int y0 = (int)fmaxf(0.0f, floorf(ymin));       /* [T12] fmaxf / floorf */
    int y1 = (int)fminf((float)c->h - 1.0f, ceilf(ymax));

    float xs[MAX_POLY_POINTS];                     /* crossings of one row */
    for (int y = y0; y <= y1; y++) {
        float yc = (float)y + 0.5f;
        int k = 0;

        for (int i = 0; i < n; i++) {
            Vec2 a = s[i];
            Vec2 b = s[(i + 1) % n];
            if ((a.y <= yc) != (b.y <= yc))        /* edge crosses this row */
                xs[k++] = a.x + (yc - a.y) * (b.x - a.x) / (b.y - a.y);
        }
        Sort_InsertionFloat(xs, k);                /* left-to-right order   */

        for (int i = 0; i + 1 < k; i += 2) {       /* inside between pairs  */
            int xa = (int)ceilf(xs[i]     - 0.5f);
            int xb = (int)ceilf(xs[i + 1] - 0.5f) - 1;
            if (xa < 0) xa = 0;
            if (xb > c->w - 1) xb = c->w - 1;
            for (int x = xa; x <= xb; x++)
                Canvas_Put(c, x, y, shader(x, y, ctx));
        }
    }
}

/*
 * Raster_StrokePolygon : closed outline with a given thickness.
 * For every edge, every pixel whose centre is within thickness/2 of the
 * segment is painted (this also gives rounded corners for free).
 */
void Raster_StrokePolygon(Canvas *c, const Vec2 *pts, int n, float zoom,
                          float thickness, Rgba color)
{
    float half = fmaxf(0.5f, thickness * zoom * 0.5f);

    for (int i = 0; i < n; i++) {
        Vec2 a = V2_Scale(pts[i], zoom);
        Vec2 b = V2_Scale(pts[(i + 1) % n], zoom);
        Vec2 ab = V2_Sub(b, a);
        float len_sq = V2_LenSq(ab);

        int x0 = (int)fmaxf(0.0f, floorf(fminf(a.x, b.x) - half));
        int x1 = (int)fminf((float)c->w - 1.0f, ceilf(fmaxf(a.x, b.x) + half));
        int y0 = (int)fmaxf(0.0f, floorf(fminf(a.y, b.y) - half));
        int y1 = (int)fminf((float)c->h - 1.0f, ceilf(fmaxf(a.y, b.y) + half));

        for (int y = y0; y <= y1; y++) {
            for (int x = x0; x <= x1; x++) {
                Vec2 p = V2((float)x + 0.5f, (float)y + 0.5f);
                float t = (len_sq > 1e-8f) ? V2_Dot(V2_Sub(p, a), ab) / len_sq : 0.0f;
                t = Clampf(t, 0.0f, 1.0f);
                Vec2 q = V2_Add(a, V2_Scale(ab, t));
                if (V2_Dist(p, q) <= half)
                    Canvas_Put(c, x, y, color);
            }
        }
    }
}

/* Paint all static geometry of a level, back to front. */
void Raster_BuildLevelLayer(Canvas *c, const LevelData *lv,
                            const Rgba *hazard_tile, int tw, int th)
{
    Canvas_Clear(c);

    /* 1. hazards: striped fill, then a bright outline */
    /* zoom 1.0 here on purpose: the stripe pattern is anchored to SCREEN pixels, so it stays a
     * clean, regular diagonal at every camera zoom (anchored to the world it would alias).  */
    TileCtx tile = { hazard_tile, tw, th, 1.0f };
    const Rgba outline = COL_OUTLINE;
    for (int i = 0; i < lv->death_polyland_count; i++) {
        const PolygonData *p = &lv->death_polylands[i];
        if (hazard_tile != NULL)
            Raster_FillPolygon(c, p->points, p->point_count, lv->camera_zoom, Shader_Tiled, &tile);
        Raster_StrokePolygon(c, p->points, p->point_count, lv->camera_zoom, 4.0f, outline);
    }

    /* 2. solid terrain in orange, drawn on top */
    const Rgba land = COL_LAND;
    for (int i = 0; i < lv->polyland_count; i++) {
        const PolygonData *p = &lv->polylands[i];
        Raster_FillPolygon(c, p->points, p->point_count, lv->camera_zoom, Shader_Solid, &land);
    }
}
