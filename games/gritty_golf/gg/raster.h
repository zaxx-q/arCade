/*
 * raster.h  -  tiny software renderer for the level's STATIC geometry
 *
 * Terrain is drawn ONCE per level into a 256x256 pixel array (a Canvas) and
 * uploaded as a texture.  Doing it ourselves has two benefits:
 *   - concave / self-touching polygons just work (no triangulation needed)
 *   - it uses the SAME even-odd rule as the collision code, so what you see
 *     is exactly what the ball collides with.
 * This file has no raylib dependency, so it is unit-tested.
 */
#ifndef GG_RASTER_H
#define GG_RASTER_H

#include "common.h"

typedef struct {
    uint8_t *pixels;     /* w*h*4 bytes, order R,G,B,A (row by row)      */
    int      w, h;
} Canvas;

/* [T10 Pointer] a SHADER is a function pointer: "what colour is pixel (x,y)?" */
typedef Rgba (*PixelShader)(int x, int y, const void *ctx);

typedef struct {                 /* context for Shader_Tiled */
    const Rgba *tile;            /* tw*th pixels                      */
    int         tw, th;
    float       zoom;            /* screen -> world scale             */
} TileCtx;

bool Canvas_Create(Canvas *c, int w, int h);
void Canvas_Destroy(Canvas *c);
void Canvas_Clear(Canvas *c);
Rgba Canvas_Get(const Canvas *c, int x, int y);
void Canvas_Put(Canvas *c, int x, int y, Rgba color);

Rgba Shader_Solid(int x, int y, const void *ctx);   /* ctx = const Rgba*    */
Rgba Shader_Tiled(int x, int y, const void *ctx);   /* ctx = const TileCtx* */

void Raster_FillPolygon(Canvas *c, const Vec2 *pts, int n, float zoom,
                        PixelShader shader, const void *ctx);
void Raster_StrokePolygon(Canvas *c, const Vec2 *pts, int n, float zoom,
                          float thickness, Rgba color);

/* Draw hazards (tiled fill + outline) then solid terrain of one level. */
void Raster_BuildLevelLayer(Canvas *c, const LevelData *lv,
                            const Rgba *hazard_tile, int tw, int th);

#endif /* GG_RASTER_H */
