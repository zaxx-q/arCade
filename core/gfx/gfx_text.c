/* Text rendering with the project font (Poiret One, SIL OFL 1.1: assets/fonts/OFL.txt). The font file is
 * built into the executable (arcade/assets.h); every pixel size is rendered from those bytes in memory.
 *
 * Poiret One is a thin display font, so it needs help to stay readable on a busy, dark game screen:
 *  - one font atlas per pixel size (crisp 1:1 glyphs instead of scaling a big atlas down),
 *  - a global size boost (GFX_TEXT_SCALE) because its x-height is small,
 *  - slightly wider letter spacing, a faux-bold second strike and a soft drop shadow for small text.
 * If the font file is missing, raylib's built-in font is used so the game still runs.
 *
 * The window can be bigger or smaller than the 1280 x 720 design canvas (see "Window size and full screen" in
 * gfx.h). Text is still laid out in design pixels, but each atlas is rasterised at the size it will really
 * have on screen (design size x view scale) and the text origin is moved to a whole window pixel, so the
 * glyphs land on the screen 1:1 and stay sharp instead of being a small bitmap stretched up or a big one
 * squeezed down. */
#include <math.h>
#include <stdio.h>

#include "arcade/assets.h"
#include "arcade/gfx.h"
#include "arcade/view.h"
#include "rlgl.h"

#define FONT_FILE "fonts/PoiretOne-Regular.ttf"
#define FONT_CACHE 40
#define ATLAS_PX_MAX 512 /* never rasterise bigger than this: a 4K screen still fits in one texture */

static struct {
    int px; /* the size the atlas is rasterised at (window pixels) */
    Font font;
} g_cache[FONT_CACHE];
static int g_cache_n;
static float g_cache_scale = 1.0f; /* the text scale the cached atlases were made for */
static ArcAsset g_file; /* the TTF bytes: kept, because a new atlas is made the first time a size is used */
static bool g_have_file;

void gfx_text_init(const char *assets_dir) {
    g_cache_n = 0;
    arc_asset_release(&g_file); /* a second init must not leak the first copy */
    g_file = arc_asset_load(assets_dir, FONT_FILE);
    g_have_file = g_file.data != NULL;
    if (!g_have_file)
        TraceLog(LOG_WARNING, "ARCADE: font '%s' not found, using the built-in font", FONT_FILE);
}

static void cache_clear(void) {
    for (int i = 0; i < g_cache_n; i++)
        UnloadFont(g_cache[i].font);
    g_cache_n = 0;
}

void gfx_text_shutdown(void) {
    cache_clear();
    arc_asset_release(&g_file);
    g_have_file = false;
}

/* The atlas scale for what is being drawn now: the view scale, rounded so that dragging a window edge does
 * not rebuild every atlas for each pixel of size (arc_view_text_scale). */
static float text_scale(void) {
    return arc_view_text_scale(arcade_view_scale());
}

void gfx_text_frame_begin(void) {
    float s = text_scale();
    if (s != g_cache_scale) {
        /* The window was resized: the atlases are for another size. Nothing is queued yet at the start of a
         * frame, so the textures can be freed; they are rebuilt on demand by font_for(). */
        cache_clear();
        g_cache_scale = s;
    }
}

static int px_for(int size) {
    int px = (int)lroundf((float)size * GFX_TEXT_SCALE);
    return px < 9 ? 9 : px;
}

static float spacing_for(int px) {
    return (float)px * GFX_TEXT_TRACKING;
}

/* The font whose atlas is rasterised for the logical size px (design pixels) at the current window scale. */
static Font font_for(int px) {
    int apx = (int)lroundf((float)px * text_scale());
    if (apx < 6)
        apx = 6;
    if (apx > ATLAS_PX_MAX)
        apx = ATLAS_PX_MAX;
    for (int i = 0; i < g_cache_n; i++)
        if (g_cache[i].px == apx)
            return g_cache[i].font;
    if (!g_have_file)
        return GetFontDefault();
    if (g_cache_n >= FONT_CACHE) {
        /* more sizes than slots (only possible with a lot of different sizes at once): start over. The draw
         * calls queued so far still point at the old textures, so draw them before freeing those. */
        rlDrawRenderBatchActive();
        cache_clear();
    }
    /* NULL codepoints = printable ASCII */
    Font f = LoadFontFromMemory(".ttf", g_file.data, (int)g_file.size, apx, NULL, 0);
    if (!IsFontValid(f)) {
        g_have_file = false;
        return GetFontDefault();
    }
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    g_cache[g_cache_n].px = apx;
    g_cache[g_cache_n].font = f;
    g_cache_n++;
    return f;
}

int gfx_text_width(const char *text, int size) {
    int px = px_for(size);
    return (int)ceilf(MeasureTextEx(font_for(px), text, (float)px, spacing_for(px)).x);
}

/* One window pixel in design units, for the 1 px offsets of the shadow and the faux bold: they stay whole
 * window pixels, so the extra strikes do not blur at a fractional scale. */
static float one_pixel(void) {
    float s = arcade_view_scale();
    float whole = roundf(s);
    return (whole < 1.0f ? 1.0f : whole) / s;
}

void gfx_text(const char *text, int x, int y, int size, Color c) {
    int px = px_for(size);
    Font f = font_for(px);
    float sp = spacing_for(px);
    Vector2 pos = {(float)x, (float)y + (float)px * GFX_TEXT_YSHIFT};
    pos = arcade_view_snap(pos);
    float one = one_pixel();
    if (px < GFX_TEXT_SHADOW_BELOW) {
        Vector2 sh = {pos.x + one, pos.y + one};
        DrawTextEx(f, text, sh, (float)px, sp, Fade(BLACK, 0.6f));
    }
    DrawTextEx(f, text, pos, (float)px, sp, c);
    if (px < GFX_TEXT_BOLD_BELOW) { /* faux bold: strike again one pixel to the right and one down */
        Vector2 b = {pos.x + one, pos.y};
        Vector2 d = {pos.x, pos.y + one};
        DrawTextEx(f, text, b, (float)px, sp, c);
        DrawTextEx(f, text, d, (float)px, sp, c);
    }
}
