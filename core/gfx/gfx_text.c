/* Text rendering with the project font (Poiret One, SIL OFL 1.1: assets/fonts/OFL.txt). The font file is
 * built into the executable (arcade/assets.h); every pixel size is rendered from those bytes in memory.
 *
 * Poiret One is a thin display font, so it needs help to stay readable on a busy, dark game screen:
 *  - one font atlas per pixel size (crisp 1:1 glyphs instead of scaling a big atlas down),
 *  - a global size boost (GFX_TEXT_SCALE) because its x-height is small,
 *  - slightly wider letter spacing, a faux-bold second strike and a soft drop shadow for small text.
 * If the font file is missing, raylib's built-in font is used so the game still runs. */
#include <math.h>
#include <stdio.h>

#include "arcade/assets.h"
#include "arcade/gfx.h"

#define FONT_FILE "fonts/PoiretOne-Regular.ttf"
#define FONT_CACHE 40

static struct {
    int px;
    Font font;
} g_cache[FONT_CACHE];
static int g_cache_n;
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

void gfx_text_shutdown(void) {
    for (int i = 0; i < g_cache_n; i++)
        UnloadFont(g_cache[i].font);
    g_cache_n = 0;
    arc_asset_release(&g_file);
    g_have_file = false;
}

static int px_for(int size) {
    int px = (int)lroundf((float)size * GFX_TEXT_SCALE);
    return px < 9 ? 9 : px;
}

static float spacing_for(int px) {
    return (float)px * GFX_TEXT_TRACKING;
}

static Font font_for(int px) {
    for (int i = 0; i < g_cache_n; i++)
        if (g_cache[i].px == px)
            return g_cache[i].font;
    if (!g_have_file || g_cache_n >= FONT_CACHE)
        return GetFontDefault();
    /* NULL codepoints = printable ASCII */
    Font f = LoadFontFromMemory(".ttf", g_file.data, (int)g_file.size, px, NULL, 0);
    if (!IsFontValid(f)) {
        g_have_file = false;
        return GetFontDefault();
    }
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    g_cache[g_cache_n].px = px;
    g_cache[g_cache_n].font = f;
    g_cache_n++;
    return f;
}

int gfx_text_width(const char *text, int size) {
    int px = px_for(size);
    return (int)ceilf(MeasureTextEx(font_for(px), text, (float)px, spacing_for(px)).x);
}

void gfx_text(const char *text, int x, int y, int size, Color c) {
    int px = px_for(size);
    Font f = font_for(px);
    float sp = spacing_for(px);
    Vector2 pos = {(float)x, (float)y + (float)px * GFX_TEXT_YSHIFT};
    if (px < GFX_TEXT_SHADOW_BELOW) {
        Vector2 sh = {pos.x + 1.0f, pos.y + 1.0f};
        DrawTextEx(f, text, sh, (float)px, sp, Fade(BLACK, 0.6f));
    }
    DrawTextEx(f, text, pos, (float)px, sp, c);
    if (px < GFX_TEXT_BOLD_BELOW) { /* faux bold: strike again one pixel to the right and one down */
        Vector2 b = {pos.x + 1.0f, pos.y};
        Vector2 d = {pos.x, pos.y + 1.0f};
        DrawTextEx(f, text, b, (float)px, sp, c);
        DrawTextEx(f, text, d, (float)px, sp, c);
    }
}
