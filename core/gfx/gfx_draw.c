/* Drawing helpers: backdrop, text, glow, HUD, particles, sandbox view. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"

Color gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    Color c = {r, g, b, a};
    return c;
}

void gfx_backdrop(float t) {
    ClearBackground(ARC_COL_BG);
    Color line = gfx_rgba(40, 50, 100, 60);
    int step = 64;
    int off = (int)fmodf(t * 12.0f, (float)step);
    for (int x = -off; x < ARC_SCREEN_W; x += step)
        DrawLine(x, 0, x, ARC_SCREEN_H, line);
    for (int y = -off; y < ARC_SCREEN_H; y += step)
        DrawLine(0, y, ARC_SCREEN_W, y, line);
}

void gfx_text_center(const char *text, int cx, int y, int size, Color c) {
    DrawText(text, cx - MeasureText(text, size) / 2, y, size, c);
}

void gfx_text_right(const char *text, int right_x, int y, int size, Color c) {
    DrawText(text, right_x - MeasureText(text, size), y, size, c);
}

void gfx_glow_circle(float x, float y, float r, Color c) {
    Vector2 p = {x, y};
    DrawCircleV(p, r * 2.2f, Fade(c, 0.07f));
    DrawCircleV(p, r * 1.6f, Fade(c, 0.14f));
    DrawCircleV(p, r * 1.25f, Fade(c, 0.28f));
    DrawCircleV(p, r, c);
}

void gfx_glow_line(Vec2 a, Vec2 b, float thick, Color c) {
    Vector2 s = {a.x, a.y}, e = {b.x, b.y};
    DrawLineEx(s, e, thick * 4.0f, Fade(c, 0.10f));
    DrawLineEx(s, e, thick * 2.5f, Fade(c, 0.22f));
    DrawLineEx(s, e, thick, c);
}

void gfx_hud(const char *title, int score, const char *extra) {
    char buf[64];
    DrawRectangle(0, 0, ARC_SCREEN_W, 36, Fade(ARC_COL_PANEL, 0.85f));
    DrawText(title, 16, 8, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "SCORE %d", score);
    gfx_text_center(buf, ARC_SCREEN_W / 2, 8, 22, ARC_COL_YELLOW);
    if (extra && extra[0])
        gfx_text_right(extra, ARC_SCREEN_W - 16, 10, 18, ARC_COL_DIM);
}

void gfx_draw_particles(const ParticleSystem *ps, Vec2 offset) {
    for (int i = 0; i < FX_MAX_PARTICLES; i++) {
        const Particle *p = &ps->p[i];
        if (p->life <= 0.0f)
            continue;
        float k = p->life / p->max_life;
        Color c = gfx_rgba(p->r, p->g, p->b, (uint8_t)(255.0f * k));
        DrawCircle((int)(p->x + offset.x), (int)(p->y + offset.y), p->size * (0.4f + 0.6f * k), c);
    }
}

/* ---- sandbox view: one texture, one pixel per cell, scaled up with point filtering ---- */
static uint8_t g_pixels[SANDBOX_MAX_W * SANDBOX_MAX_H * 4];

void gfx_sandbox_view_init(GfxSandboxView *v, const Sandbox *sb) {
    Image img = GenImageColor(sb->w, sb->h, BLACK);
    v->tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(v->tex, TEXTURE_FILTER_POINT);
    v->w = sb->w;
    v->h = sb->h;
}

void gfx_sandbox_view_free(GfxSandboxView *v) {
    if (v->tex.id != 0)
        UnloadTexture(v->tex);
    memset(v, 0, sizeof(*v));
}

void gfx_sandbox_view_draw(GfxSandboxView *v, const Sandbox *sb, Rectangle dst, Color tint) {
    for (int y = 0; y < sb->h; y++) {
        for (int x = 0; x < sb->w; x++) {
            const ElementDef *e = sandbox_element((ElementId)sb->cells[y * sb->w + x]);
            int shade = e->kind == MOVE_NONE && e->density == 0 ? 0 : ((x * 7 + y * 13) & 15) - 8; /* grain */
            int i = (y * sb->w + x) * 4;
            int r = e->r + shade, gr = e->g + shade, b = e->b + shade;
            g_pixels[i + 0] = (uint8_t)arc_clampi(r, 0, 255);
            g_pixels[i + 1] = (uint8_t)arc_clampi(gr, 0, 255);
            g_pixels[i + 2] = (uint8_t)arc_clampi(b, 0, 255);
            g_pixels[i + 3] = 255;
        }
    }
    UpdateTexture(v->tex, g_pixels);
    Rectangle src = {0.0f, 0.0f, (float)sb->w, (float)sb->h};
    Vector2 origin = {0.0f, 0.0f};
    DrawTexturePro(v->tex, src, dst, origin, 0.0f, tint);
}

Sound gfx_make_tone(float freq_hz, float seconds, float volume) {
    const unsigned rate = 44100;
    unsigned frames = (unsigned)(seconds * (float)rate);
    short *samples = (short *)MemAlloc(frames * (unsigned)sizeof(short));
    for (unsigned i = 0; i < frames; i++) {
        float t = (float)i / (float)rate;
        float env = 1.0f - (float)i / (float)frames; /* linear decay: a soft "tick" */
        samples[i] = (short)(sinf(ARC_TAU * freq_hz * t) * env * volume * 30000.0f);
    }
    Wave w;
    w.frameCount = frames;
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = samples;
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w); /* the Sound keeps its own copy */
    return snd;
}
