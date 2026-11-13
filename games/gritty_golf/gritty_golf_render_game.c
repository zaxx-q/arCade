/* Gritty Golf - raylib drawing. Ported from the upstream render.c + ui.c (see gg/VENDORED.md); all rules
 * live in gritty_golf_logic.c and gg/.
 *
 * The game is drawn into a 256x256 render texture with its own 3-colour palette, then shown at exactly 2x
 * in the middle of the arCade screen, between two panels. Whole-number scaling and point filtering keep
 * every pixel sharp: only the panels use arCade's colours and font.
 *
 * Draw order inside the canvas, back to front:
 *   terrain -> walls -> buttons / pickups -> flag -> trail -> ball -> aim arrow -> HUD -> dialog ->
 * transition */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"
#include "gg/bmfont.h"
#include "gg/palette.h"
#include "gg/raster.h"
#include "gg/save.h"
#include "gg/util.h"
#include "gg/vec2.h"
#include "gritty_golf_front.h"

/* the whole canvas uses these three colours (gg/palette_colors.h) */
#define C_BG ((Color)COL_BG)
#define C_LAND ((Color)COL_LAND)
#define C_INK ((Color)COL_INK)

typedef struct UiFont {
    BakedFont meta;
    Texture2D atlas;
} UiFont;

typedef struct Assets {
    Texture2D bat_ghost; /* the bat with every other pixel removed: a palette-pure "used" marker */
    Texture2D ball, bat, bat_strike, button_on, button_off, flag;
    Texture2D retry, stage_select, arrow_bg, arrow_fg, nine_patch, splash;
    Texture2D logo[3]; /* three frames of the animated title */
    Texture2D layer;   /* static terrain of the current level */
    UiFont font8, font16;
    Texture2D dither; /* 50% checkerboard of BG: palette-pure dimming */

    RenderTexture2D target; /* the 256x256 picture */
    Canvas canvas;          /* CPU pixels behind `layer` */
    Rgba hazard_tile[32 * 32];
    int tile_w, tile_h;
    int layer_level; /* which level `layer` shows (-1 = none) */
    bool ready;
} Assets;

static Assets A;                          /* the single, file-private asset table */
static Vec2 s_mouse = {-100.0f, -100.0f}; /* mouse in canvas pixels, set before each canvas draw */

static Rectangle to_rec(gritty_golf_Rect r) {
    Rectangle out = {r.x, r.y, r.w, r.h};
    return out;
}

static bool hit(gritty_golf_Rect r) {
    return gritty_golf_in_rect(r, s_mouse);
}

/* ================================================================== *
 *  Text (baked bitmap font, see gg/bmfont.c)
 *  Every glyph is copied from the atlas to a WHOLE-pixel position, 1:1: no scaling, no anti-aliasing.
 * ================================================================== */
static int text_width(const UiFont *f, const char *s) {
    return Bmfont_Width(&f->meta, s);
}

static void draw_glyphs(const UiFont *f, const char *s, int x, int y, Color c) {
    for (const char *p = s; *p != '\0'; p++) {
        const BakedGlyph *g = Bmfont_Find(&f->meta, (unsigned char)*p);
        if (g->w > 0) {
            Rectangle src = {(float)g->x, (float)g->y, (float)g->w, (float)g->h};
            Vector2 at = {(float)(x + g->xoff), (float)(y + g->yoff)};
            DrawTextureRec(f->atlas, src, at, c);
        }
        x += g->advance;
    }
}

/* Outline first (the same text 8 times, shifted one pixel each way, in the background colour), then the
 * text: letters keep a dark edge even over the lighter terrain. */
static void text_draw(const UiFont *f, const char *s, int x, int y, Color ink) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (dx != 0 || dy != 0)
                draw_glyphs(f, s, x + dx, y + dy, C_BG);
    draw_glyphs(f, s, x, y, ink);
}

static void text_draw_plain(const UiFont *f, const char *s, int x, int y, Color ink) {
    draw_glyphs(f, s, x, y, ink);
}

static void text_draw_centered(const UiFont *f, const char *s, int cx, int y, Color ink) {
    text_draw(f, s, cx - text_width(f, s) / 2, y, ink); /* integer division: whole pixels */
}

/* The accent colour (LAND) is used for hints and credits, but ONLY when it is readable enough on the
 * background: 4.5:1 for small text, 3:1 for big text (WCAG). Otherwise the ink colour is used. */
static Color to_color(Rgba c) {
    Color out = {c.r, c.g, c.b, c.a};
    return out;
}

static Color accent_small(void) {
    Rgba bg = COL_BG, land = COL_LAND, ink = COL_INK;
    return to_color(Palette_PickText(bg, land, ink, 4.5f));
}

static Color accent_large(void) {
    Rgba bg = COL_BG, land = COL_LAND, ink = COL_INK;
    return to_color(Palette_PickText(bg, land, ink, 3.0f));
}

/* ================================================================== *
 *  Loading
 * ================================================================== */
static bool load_tex(Texture2D *out, const char *dir, const char *name) {
    char path[320];
    snprintf(path, sizeof(path), "%s/textures/%s", dir, name);
    if (!FileExists(path)) {
        fprintf(stderr, "gritty_golf: missing texture: %s\n", path);
        return false;
    }
    *out = LoadTexture(path);
    SetTextureFilter(*out, TEXTURE_FILTER_POINT); /* crisp pixels, no blur */
    return out->id != 0;
}

/* <name>.fnt (metrics) and the atlas image it names */
static bool load_font(UiFont *f, const char *dir, const char *fnt_name) {
    char path[320];
    snprintf(path, sizeof(path), "%s/font/%s", dir, fnt_name);
    if (!Bmfont_Load(&f->meta, path)) {
        fprintf(stderr, "gritty_golf: cannot read font metrics: %s\n", path);
        return false;
    }
    snprintf(path, sizeof(path), "%s/font/%.60s", dir, f->meta.atlas);
    if (!FileExists(path)) {
        fprintf(stderr, "gritty_golf: missing font image: %s\n", path);
        return false;
    }
    f->atlas = LoadTexture(path);
    SetTextureFilter(f->atlas, TEXTURE_FILTER_POINT); /* never blur */
    return f->atlas.id != 0;
}

/* The "used strike" marker: the bat sprite with a checkerboard of its pixels removed. It looks dimmed, but
 * every visible pixel is still a real palette colour (no alpha blending). */
static bool load_bat_ghost(const char *dir) {
    char path[320];
    snprintf(path, sizeof(path), "%s/textures/sprBat.png", dir);
    if (!FileExists(path))
        return false;
    Image im = LoadImage(path);
    ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *px = (Color *)im.data;
    for (int y = 0; y < im.height; y++)
        for (int x = 0; x < im.width; x++)
            if ((x + y) % 2 == 1)
                px[y * im.width + x].a = 0;
    A.bat_ghost = LoadTextureFromImage(im);
    UnloadImage(im);
    SetTextureFilter(A.bat_ghost, TEXTURE_FILTER_POINT);
    return true;
}

/* Copies the 12x12 hazard pattern into a plain array that the rasteriser (pure C) can read. */
static bool load_hazard_tile(const char *dir) {
    char path[320];
    snprintf(path, sizeof(path), "%s/textures/sprDeathBg.png", dir);
    if (!FileExists(path)) {
        fprintf(stderr, "gritty_golf: missing texture: %s\n", path);
        return false;
    }
    Image im = LoadImage(path);
    ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    A.tile_w = im.width > 32 ? 32 : im.width;
    A.tile_h = im.height > 32 ? 32 : im.height;
    const Rgba *src = (const Rgba *)im.data;
    for (int y = 0; y < A.tile_h; y++)
        for (int x = 0; x < A.tile_w; x++)
            A.hazard_tile[y * A.tile_w + x] = src[y * im.width + x];
    UnloadImage(im);
    return true;
}

bool gritty_golf_front_init(const char *dir) {
    memset(&A, 0, sizeof(A));
    A.layer_level = -1;

    bool ok = true;
    ok = load_tex(&A.ball, dir, "sprBall.png") && ok;
    ok = load_tex(&A.bat, dir, "sprBat.png") && ok;
    ok = load_tex(&A.bat_strike, dir, "sprBatStrike.png") && ok;
    ok = load_tex(&A.button_on, dir, "sprButtonOn.png") && ok;
    ok = load_tex(&A.button_off, dir, "sprButtonOff.png") && ok;
    ok = load_tex(&A.flag, dir, "sprFlag.png") && ok;
    ok = load_tex(&A.retry, dir, "sprRetry.png") && ok;
    ok = load_tex(&A.stage_select, dir, "sprStageSelect.png") && ok;
    ok = load_tex(&A.arrow_bg, dir, "sprArrowBarBg.png") && ok;
    ok = load_tex(&A.arrow_fg, dir, "sprArrowBarFg.png") && ok;
    ok = load_tex(&A.nine_patch, dir, "spr-9pr-StageSelect.png") && ok;
    ok = load_tex(&A.splash, dir, "sprSplash.png") && ok;
    ok = load_tex(&A.logo[0], dir, "sprGameLogo.png") && ok;
    ok = load_tex(&A.logo[1], dir, "sprGameLogoAlt.png") && ok;
    ok = load_tex(&A.logo[2], dir, "sprGameLogoAltAlt.png") && ok;
    ok = load_bat_ghost(dir) && ok;
    ok = load_font(&A.font8, dir, "silkscreen-8.fnt") && ok;
    ok = load_font(&A.font16, dir, "silkscreen-16.fnt") && ok;
    ok = load_hazard_tile(dir) && ok;
    if (!ok) {
        gritty_golf_front_shutdown(); /* free what did load */
        return false;
    }

    /* the CPU canvas that holds the terrain, and the texture that shows it */
    if (!Canvas_Create(&A.canvas, VIRTUAL_SIZE, VIRTUAL_SIZE))
        return false;
    Image img = {A.canvas.pixels, VIRTUAL_SIZE, VIRTUAL_SIZE, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    A.layer = LoadTextureFromImage(img);
    SetTextureFilter(A.layer, TEXTURE_FILTER_POINT);

    /* 2x2 checkerboard: two BG pixels, two transparent. Tiled over the canvas it darkens the picture by
     * exactly 50% using ONLY palette colours (no alpha blending). */
    Image d = GenImageColor(2, 2, BLANK);
    ImageDrawPixel(&d, 0, 0, C_BG);
    ImageDrawPixel(&d, 1, 1, C_BG);
    A.dither = LoadTextureFromImage(d);
    UnloadImage(d);
    SetTextureFilter(A.dither, TEXTURE_FILTER_POINT);
    SetTextureWrap(A.dither, TEXTURE_WRAP_REPEAT);

    A.target = LoadRenderTexture(VIRTUAL_SIZE, VIRTUAL_SIZE);
    SetTextureFilter(A.target.texture, TEXTURE_FILTER_POINT); /* the 2x blow-up must not blur */
    if (A.target.id == 0) {
        gritty_golf_front_shutdown();
        return false;
    }

    A.ready = true;
    return true;
}

void gritty_golf_front_shutdown(void) {
    /* safe after a failed init too: whatever was not loaded is still zero, and unloading zero is a no-op */
    Texture2D *all[] = {&A.bat_ghost, &A.ball,    &A.bat,          &A.bat_strike, &A.button_on, &A.button_off,
                        &A.flag,      &A.retry,   &A.stage_select, &A.arrow_bg,   &A.arrow_fg,  &A.nine_patch,
                        &A.splash,    &A.logo[0], &A.logo[1],      &A.logo[2],    &A.layer};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) /* an array of POINTERS to textures */
        UnloadTexture(*all[i]);
    UnloadTexture(A.font8.atlas);
    UnloadTexture(A.font16.atlas);
    UnloadTexture(A.dither);
    UnloadRenderTexture(A.target);
    Canvas_Destroy(&A.canvas);
    A.ready = false;
}

/* Re-rasterise the terrain when the level changes. */
static void ensure_layer(const Game *g) {
    if (A.layer_level == g->level_idx)
        return;
    Raster_BuildLevelLayer(&A.canvas, g->level->def, A.hazard_tile, A.tile_w, A.tile_h);
    UpdateTexture(A.layer, A.canvas.pixels);
    A.layer_level = g->level_idx;
}

/* ================================================================== *
 *  World objects (drawn inside the zoomed Camera2D)
 * ================================================================== */
static void draw_centered(Texture2D t, Vector2 c, float rot_deg, Color tint) {
    Rectangle src = {0.0f, 0.0f, (float)t.width, (float)t.height};
    Rectangle dst = {c.x, c.y, (float)t.width, (float)t.height};
    Vector2 origin = {(float)t.width * 0.5f, (float)t.height * 0.5f};
    DrawTexturePro(t, src, dst, origin, rot_deg, tint);
}

static void draw_walls(const Level *lv) {
    for (int i = 0; i < lv->wall_count; i++) {
        const WallState *w = &lv->walls[i];
        const WallData *d = w->def;
        Vector2 pivot = {d->position.x + w->offset.x, d->position.y + w->offset.y};
        /* rectangle whose rotation centre (origin) is the wall's pivot */
        Rectangle rec = {pivot.x, pivot.y, d->width, d->height};
        Vector2 origin = {d->width * 0.5f - d->collider_offset.x, d->height * 0.5f - d->collider_offset.y};
        DrawRectanglePro(rec, origin, d->rotation * (180.0f / GG_PI), C_LAND);
    }
}

static void draw_buttons_and_pickups(const Game *g) {
    const Level *lv = g->level;
    for (int i = 0; i < lv->button_count; i++) {
        const ButtonState *b = &lv->buttons[i];
        Vector2 c = {b->def->position.x, b->def->position.y};
        draw_centered(b->pressed ? A.button_off : A.button_on, c, 0.0f, WHITE);
    }
    for (int i = 0; i < lv->strike_count; i++) {
        if (!lv->strike_active[i])
            continue;
        float bob = floorf(sinf(g->banner_time * 4.0f + (float)i) * 1.5f + 0.5f); /* hover in whole pixels */
        Vector2 c = {lv->def->strikes[i].x, lv->def->strikes[i].y + bob};
        draw_centered(A.bat_strike, c, 0.0f, WHITE);
    }
}

static void draw_flag(const Level *lv) {
    /* the sprite is centred at (+18, -31) from the pole base and rotates around the base */
    Vec2 off = V2_Rotate(V2(18.0f, -31.0f), lv->def->flagpole_rotation);
    Vector2 c = {lv->def->flagpole_pos.x + off.x, lv->def->flagpole_pos.y + off.y};
    draw_centered(A.flag, c, lv->def->flagpole_rotation * (180.0f / GG_PI), WHITE);
}

/* A tapering line behind the ball: width 0 -> 7 px, solid INK (no transparency, so the picture stays inside
 * the three-colour palette). */
static void draw_trail(const Player *p) {
    int n = Player_TrailCount(p);
    if (n < 1)
        return;

    Vec2 pts[TRAIL_LEN + 1];
    for (int i = 0; i < n; i++)
        pts[i] = Player_TrailPoint(p, i);
    pts[n] = p->ball.pos; /* the head is the ball itself */
    int m = n + 1;

    for (int i = 0; i + 1 < m; i++) {
        float k = ((float)i + 0.5f) / (float)(m - 1); /* 0 = tail ... 1 = head */
        Vector2 a = {pts[i].x, pts[i].y}, b = {pts[i + 1].x, pts[i + 1].y};
        DrawLineEx(a, b, 7.0f * k, C_INK);
    }
}

static void draw_aim_indicator(const Player *p) {
    if (p->state == BALL_STATE_MOVING)
        return;
    float deg = p->aim_angle * (180.0f / GG_PI);
    /* the bar sits at local offset (+10, -5) from the ball and rotates about the ball */
    Vector2 origin = {-10.0f, 5.0f};
    Rectangle dst_bg = {p->ball.pos.x, p->ball.pos.y, 34.0f, 10.0f};
    Rectangle src_bg = {0.0f, 0.0f, 34.0f, 10.0f};
    DrawTexturePro(A.arrow_bg, src_bg, dst_bg, origin, deg, WHITE);

    if (p->state == BALL_STATE_POWER && p->power > 0.0f) {
        float w = 34.0f * p->power / 100.0f; /* the fill grows with power */
        Rectangle src_fg = {0.0f, 0.0f, w, 10.0f};
        Rectangle dst_fg = {p->ball.pos.x, p->ball.pos.y, w, 10.0f};
        DrawTexturePro(A.arrow_fg, src_fg, dst_fg, origin, deg, WHITE);
    }
}

static void draw_world(const Game *g) {
    const Level *lv = g->level;
    Camera2D cam = {0};
    cam.zoom = lv->def->camera_zoom; /* world -> canvas scale */

    BeginMode2D(cam);
    draw_walls(lv);
    draw_buttons_and_pickups(g);
    draw_flag(lv);
    draw_trail(&g->player);

    for (int i = 0; i < g->preview_count; i++) { /* aim-assist dots */
        float r = (i % 2 == 0) ? 1.5f : 1.0f;    /* alternate sizes: reads as a dotted line */
        Vector2 at = {floorf(g->preview[i].x), floorf(g->preview[i].y)};
        DrawCircleV(at, r, C_INK);
    }

    Vector2 bp = {g->player.ball.pos.x, g->player.ball.pos.y};
    draw_centered(A.ball, bp, 0.0f, WHITE); /* rotation is locked: always upright */
    draw_aim_indicator(&g->player);
    EndMode2D();
}

/* ================================================================== *
 *  Canvas UI (256x256)
 * ================================================================== */
static void draw_sprite(Texture2D t, Vector2 center, float scale, Color tint) {
    Rectangle src = {0.0f, 0.0f, (float)t.width, (float)t.height};
    Rectangle dst = {center.x, center.y, (float)t.width * scale, (float)t.height * scale};
    Vector2 origin = {dst.width * 0.5f, dst.height * 0.5f};
    DrawTexturePro(t, src, dst, origin, 0.0f, tint);
}

static int logo_frame(float t) { /* 0,1,2,0,1,2 ... every 0.4 s */
    return ((int)(t / 0.4f)) % 3;
}

static void draw_splash(const Game *g) {
    float t = g->splash_time;
    /* fade in for 0.4 s, hold, fade out for the last 0.4 s */
    float fade = Clampf(fminf(t / 0.4f, (1.8f - t) / 0.4f), 0.0f, 1.0f);
    Vector2 origin = {0.0f, 0.0f};
    DrawTextureEx(A.splash, origin, 0.0f, 1.0f, Fade(WHITE, fade));
}

/* Level 0: the animated logo and the tutorial text. */
static void draw_banner(const Game *g) {
    Vector2 logo_at = {128.0f, 44.0f};
    draw_sprite(A.logo[logo_frame(g->banner_time)], logo_at, 3.0f, WHITE);

    text_draw_centered(&A.font8, "TUTORIAL", 128, 86, accent_small());
    text_draw_centered(&A.font8, "1. PRESS SPACE OR CLICK", 128, 98, C_INK);
    text_draw_centered(&A.font8, "2. PRESS AGAIN TO LOCK THE ANGLE", 128, 108, C_INK);
    text_draw_centered(&A.font8, "3. PRESS AGAIN: POWER, THEN SHOOT", 128, 118, C_INK);
    text_draw(&A.font8, "R RESTART  P AIM ASSIST  L STAGES", 8, 131, accent_small());
}

static void draw_hud(const Game *g) {
    const Player *p = &g->player;

    /* strike bats: 3 slots; a used one is shown as a dithered ghost */
    for (int i = 0; i < STRIKES_MAX; i++) {
        Texture2D t = (i < p->strikes) ? A.bat : A.bat_ghost;
        Vector2 at = {13.0f + 24.0f * (float)i, 243.0f};
        draw_sprite(t, at, 1.0f, WHITE);
    }

    /* the two icons: the hovered one is shown in the ink colour */
    Vector2 stage_at = {198.0f, 232.0f}, retry_at = {229.0f, 232.0f};
    draw_sprite(A.stage_select, stage_at, 1.0f, hit(gritty_golf_rect_stage_button()) ? C_INK : WHITE);
    draw_sprite(A.retry, retry_at, 1.0f, hit(gritty_golf_rect_retry_button()) ? C_INK : WHITE);

    /* shot counter and level clock (between the bats and the icons: x 80 .. 183) */
    char buf[64], clock_text[16];
    Str_FormatTime(clock_text, sizeof(clock_text), g->level_time);
    snprintf(buf, sizeof(buf), "SHOTS %d   %s", p->strokes, clock_text);
    text_draw(&A.font8, buf, 84, 241, C_INK);

    /* level name, shown for 2.5 s; the big font if it fits, else the small one */
    if (g->banner_time < 2.5f && !g->level->def->has_banner) {
        snprintf(buf, sizeof(buf), "%02d  %s", g->level_idx, Level_Name(g->level_idx));
        float fade = Clampf(2.5f - g->banner_time, 0.0f, 1.0f);
        const UiFont *f = (text_width(&A.font16, buf) <= 244) ? &A.font16 : &A.font8;
        text_draw_centered(f, buf, 128, (f == &A.font16) ? 3 : 5, Fade(C_INK, fade));
    }
    if (g->toast_time > 0.0f)
        text_draw_centered(&A.font8, g->toast, 128, 22, Fade(C_INK, Clampf(g->toast_time, 0.0f, 1.0f)));

    if (g->settings.aim_assist)
        text_draw(&A.font8, "ASSIST", 4, 225, accent_small()); /* above the bats: nothing else lives there */

    /* out of strikes and standing still: a steady hint (a blinking message is hidden half the time) */
    if (g->rest_time > 0.8f)
        text_draw_centered(&A.font8, "OUT OF STRIKES - PRESS R", 128, 214, C_INK);
}

static void draw_stage_select(const Game *g) {
    /* dim the game behind the dialog with a 50% checkerboard of the background colour */
    Rectangle all = {0.0f, 0.0f, (float)VIRTUAL_SIZE, (float)VIRTUAL_SIZE};
    Vector2 zero = {0.0f, 0.0f};
    DrawTextureRec(A.dither, all, zero, WHITE);

    NPatchInfo info = {{0.0f, 0.0f, 24.0f, 24.0f}, 8, 8, 8, 8, NPATCH_NINE_PATCH};
    DrawTextureNPatch(A.nine_patch, info, to_rec(gritty_golf_rect_dialog()), zero, 0.0f, WHITE);

    text_draw_centered(&A.font16, "STAGE SELECT", 116, 38, C_INK);
    text_draw_centered(&A.font8, "L OR X TO CLOSE", 128, 53, accent_small());

    gritty_golf_Rect cl = gritty_golf_rect_close();
    text_draw_centered(&A.font16, "X", (int)(cl.x + cl.w * 0.5f), (int)cl.y + 3,
                       hit(cl) ? accent_large() : C_INK);

    int next = Save_FirstUncleared(&g->save); /* where to continue */
    for (int i = 0; i < LEVEL_COUNT; i++) {
        gritty_golf_Rect cell = gritty_golf_rect_stage_cell(i);
        Rectangle r = to_rec(cell);
        bool cleared = Save_IsCleared(&g->save, i);
        bool current = (i == g->level_idx);
        bool hover = hit(cell);

        DrawRectangleRec(r, cleared ? C_LAND : C_BG);
        DrawRectangleLinesEx(r, (hover || current) ? 2.0f : 1.0f, (hover || current) ? C_INK : C_LAND);

        char label[8];
        snprintf(label, sizeof(label), "%02d", i);
        int lx = (int)(r.x + r.width * 0.5f) - text_width(&A.font16, label) / 2;
        int ly = (int)r.y + 6;
        /* A cleared cell is filled with the terrain colour and its digits are dark: an outline in the
         * background colour would only fatten them, so draw them plain. */
        if (cleared)
            text_draw_plain(&A.font16, label, lx, ly, C_BG);
        else
            text_draw(&A.font16, label, lx, ly, accent_large());
        if (i == next) /* small marker under the "next" level */
            DrawRectangle((int)(r.x + r.width * 0.5f) - 3, (int)(r.y + r.height) + 2, 6, 2, C_INK);
    }

    char line[48];
    snprintf(line, sizeof(line), "CLEARED %d / %d", Save_CountCleared(&g->save), LEVEL_COUNT);
    text_draw_centered(&A.font8, line, 128, 210, accent_small());
}

static void draw_ending(const Game *g) {
    Vector2 logo_at = {128.0f, 44.0f};
    draw_sprite(A.logo[logo_frame(g->banner_time)], logo_at, 3.0f, WHITE);
    text_draw_centered(&A.font16, "THANKS FOR PLAYING!", 128, 88, accent_large());

    char buf[64], clock_text[16];
    snprintf(buf, sizeof(buf), "PLAYER   %s", g->save.name);
    text_draw_centered(&A.font8, buf, 128, 112, C_INK);
    snprintf(buf, sizeof(buf), "CLEARED  %d / %d", Save_CountCleared(&g->save), LEVEL_COUNT);
    text_draw_centered(&A.font8, buf, 128, 123, C_INK);
    snprintf(buf, sizeof(buf), "STROKES  %d", Save_TotalBestStrokes(&g->save));
    text_draw_centered(&A.font8, buf, 128, 134, C_INK);
    Str_FormatTime(clock_text, sizeof(clock_text), Save_TotalBestTime(&g->save));
    snprintf(buf, sizeof(buf), "TIME     %s", clock_text);
    text_draw_centered(&A.font8, buf, 128, 145, C_INK);
    snprintf(buf, sizeof(buf), "SCORE    %d", Save_Score(&g->save));
    text_draw_centered(&A.font8, buf, 128, 160, accent_small());

    if (((int)(g->banner_time * 1.5f)) % 2 == 0)
        text_draw_centered(&A.font8, "PRESS SPACE TO FINISH", 128, 186, C_INK);

    text_draw_centered(&A.font8, "ORIGINAL GAME BY ESCADA GAMES", 128, 226, accent_small());
    text_draw_centered(&A.font8, "C PORT - EDUCATIONAL PROJECT", 128, 236, accent_small());
}

/* Diamond-wave transition: a 9 x 9 grid of diamonds (a 4-sided DrawPoly) whose radius grows from 0 to 32. */
static void draw_transition(const Game *g) {
    if (g->tr.phase == TR_NONE)
        return;
    for (int cy = 0; cy < TR_GRID; cy++) {
        for (int cx = 0; cx < TR_GRID; cx++) {
            float s = Game_TransitionScale(g, cx, cy);
            if (s <= 0.0f)
                continue;
            float r = (float)TR_CELL * s + (s >= 1.0f ? 2.0f : 0.0f); /* +2: no hairline gaps */
            Vector2 at = {(float)(cx * TR_CELL), (float)(cy * TR_CELL)};
            DrawPoly(at, 4, r, 0.0f, C_BG);
        }
    }
}

/* One 256x256 frame. */
static void draw_canvas(const Game *g) {
    ClearBackground(C_BG);

    switch (g->screen) {
    case SCREEN_SPLASH:
        draw_splash(g);
        return;

    case SCREEN_ENDING:
        draw_ending(g);
        draw_transition(g);
        return;

    case SCREEN_PLAY:
        ensure_layer(g);
        DrawTexture(A.layer, 0, 0, WHITE); /* static terrain, already zoomed */
        if (g->level->def->has_banner)
            draw_banner(g);
        draw_world(g);
        draw_hud(g);
        if (g->stage_select_open)
            draw_stage_select(g);
        draw_transition(g);
        return;
    }
}

void gritty_golf_front_render_canvas(const gritty_golf_State *s, Vec2 mouse) {
    if (!A.ready || !s->ready)
        return;
    s_mouse = mouse;
    BeginTextureMode(A.target);
    draw_canvas(&s->game);
    EndTextureMode();
}

/* ================================================================== *
 *  The arCade screen: HUD bar, two panels, the canvas in the middle
 * ================================================================== */
#define PANEL_TOP GG_VIEW_Y
#define PANEL_H GG_VIEW_SIZE
#define LEFT_X 24
#define LEFT_W 336
#define RIGHT_X (GG_VIEW_X + GG_VIEW_SIZE + 16)
#define RIGHT_W (ARC_SCREEN_W - 24 - RIGHT_X)

static void draw_panel(int x, int w) {
    Rectangle r = {(float)x, (float)PANEL_TOP, (float)w, (float)PANEL_H};
    DrawRectangleRec(r, Fade(ARC_COL_PANEL, 0.85f));
    DrawRectangleLinesEx(r, 1.0f, Fade(ARC_COL_DIM, 0.45f));
}

static void draw_left_panel(void) {
    draw_panel(LEFT_X, LEFT_W);
    int x = LEFT_X + 20, y = PANEL_TOP + 18;
    gfx_text("HOW TO PLAY", x, y, 22, ARC_COL_CYAN);
    y += 40;
    gfx_text("SPACE / ENTER / CLICK", x, y, 18, ARC_COL_YELLOW);
    gfx_text("1  stops the ball, aim", x, y + 26, 18, ARC_COL_TEXT);
    gfx_text("2  locks the angle", x, y + 52, 18, ARC_COL_TEXT);
    gfx_text("3  locks the power: shoot", x, y + 78, 18, ARC_COL_TEXT);
    gfx_text("Reach the flag. Every shot uses one", x, y + 112, 16, ARC_COL_DIM);
    gfx_text("of 3 strikes; a bat pickup refills", x, y + 132, 16, ARC_COL_DIM);
    gfx_text("them. Spikes restart the level.", x, y + 152, 16, ARC_COL_DIM);

    y += 192;
    gfx_text("KEYS", x, y, 22, ARC_COL_CYAN);
    static const char *const keys[][2] = {
        {"R", "restart level"}, {"L", "stage select"},    {"P", "aim assist"},
        {"M", "mute sound"},    {"F", "finish + submit"}, {"ESC", "back to hub"},
    };
    for (int i = 0; i < 6; i++) {
        gfx_text(keys[i][0], x, y + 34 + i * 25, 18, ARC_COL_YELLOW);
        gfx_text(keys[i][1], x + 64, y + 34 + i * 25, 18, ARC_COL_TEXT);
    }
    gfx_text("Original game by Escada Games", x, PANEL_TOP + PANEL_H - 28, 15, ARC_COL_DIM);
}

static void draw_button(gritty_golf_Rect r, const char *label, Color tint, bool hover, float fill) {
    Rectangle rec = to_rec(r);
    DrawRectangleRec(rec, Fade(ARC_COL_BG, 0.9f));
    if (fill > 0.0f) { /* the confirmation window running out */
        Rectangle bar = {rec.x, rec.y, rec.width * fill, rec.height};
        DrawRectangleRec(bar, Fade(tint, 0.28f));
    }
    if (hover)
        DrawRectangleRec(rec, Fade(tint, 0.18f));
    DrawRectangleLinesEx(rec, hover ? 2.0f : 1.0f, hover ? tint : Fade(tint, 0.6f));
    gfx_text_center(label, (int)(rec.x + rec.width * 0.5f), (int)rec.y + 11, 18, tint);
}

static void draw_right_panel(const gritty_golf_State *s, Vec2 mouse) {
    const Game *g = &s->game;
    draw_panel(RIGHT_X, RIGHT_W);
    int x = RIGHT_X + 20, y = PANEL_TOP + 18;

    char buf[64], clock_text[16];
    bool playing = g->screen == SCREEN_PLAY;
    if (g->screen == SCREEN_ENDING) {
        gfx_text("ALL COURSES CLEARED", x, y, 22, ARC_COL_YELLOW);
        gfx_text("Press Space to finish", x, y + 34, 18, ARC_COL_TEXT);
    } else {
        snprintf(buf, sizeof(buf), "LEVEL %02d / %d", g->level_idx, LEVEL_COUNT - 1);
        gfx_text(buf, x, y, 22, ARC_COL_YELLOW);
        gfx_text(Level_Name(g->level_idx), x, y + 34, 18, ARC_COL_TEXT);
    }

    Str_FormatTime(clock_text, sizeof(clock_text), g->level_time);
    snprintf(buf, sizeof(buf), "SHOTS   %d", g->player.strokes);
    gfx_text(buf, x, y + 84, 18, ARC_COL_TEXT);
    snprintf(buf, sizeof(buf), "TIME    %s", clock_text);
    gfx_text(buf, x, y + 112, 18, ARC_COL_TEXT);
    snprintf(buf, sizeof(buf), "CLEARED %d / %d", Save_CountCleared(&g->save), LEVEL_COUNT);
    gfx_text(buf, x, y + 140, 18, ARC_COL_TEXT);
    snprintf(buf, sizeof(buf), "SCORE   %d", Save_Score(&g->save));
    gfx_text(buf, x, y + 168, 18, ARC_COL_YELLOW);

    for (int i = 0; i < GG_BTN_COUNT; i++) {
        gritty_golf_Rect r = gritty_golf_rect_panel_button(i);
        bool hover = gritty_golf_in_rect(r, mouse);
        if (!playing && i != GG_BTN_SUBMIT) { /* the title card and the ending ignore these three */
            static const char *const names[] = {"STAGE SELECT   [L]", "RESTART LEVEL   [R]",
                                                "AIM ASSIST   [P]"};
            draw_button(r, names[i], ARC_COL_DIM, false, 0.0f);
            continue;
        }
        switch (i) {
        case GG_BTN_STAGES:
            draw_button(r, g->stage_select_open ? "CLOSE STAGES   [L]" : "STAGE SELECT   [L]", ARC_COL_CYAN,
                        hover, 0.0f);
            break;
        case GG_BTN_RESTART:
            draw_button(r, "RESTART LEVEL   [R]", ARC_COL_CYAN, hover, 0.0f);
            break;
        case GG_BTN_ASSIST:
            draw_button(r, g->settings.aim_assist ? "AIM ASSIST: ON   [P]" : "AIM ASSIST: OFF   [P]",
                        g->settings.aim_assist ? ARC_COL_GREEN : ARC_COL_CYAN, hover, 0.0f);
            break;
        default:
            if (s->finish_armed > 0)
                draw_button(r, "CLICK AGAIN TO FINISH", ARC_COL_YELLOW, hover,
                            (float)s->finish_armed / (float)GG_FINISH_CONFIRM_FRAMES);
            else
                draw_button(r, "FINISH + SUBMIT   [F]", ARC_COL_MAGENTA, hover, 0.0f);
            break;
        }
    }
}

void gritty_golf_front_draw_screen(const gritty_golf_State *s, Vec2 mouse, float t) {
    gfx_backdrop(t * 0.2f);
    if (!A.ready || !s->ready)
        return;
    draw_left_panel();
    draw_right_panel(s, mouse);

    /* the canvas: 256 px shown at exactly 2x, point filtered; a render texture is stored upside down,
     * hence the negative source height */
    Rectangle frame = {(float)(GG_VIEW_X - 3), (float)(GG_VIEW_Y - 3), (float)(GG_VIEW_SIZE + 6),
                       (float)(GG_VIEW_SIZE + 6)};
    DrawRectangleLinesEx(frame, 2.0f, Fade(ARC_COL_CYAN, 0.55f));
    Rectangle src = {0.0f, 0.0f, (float)VIRTUAL_SIZE, -(float)VIRTUAL_SIZE};
    Rectangle dst = {(float)GG_VIEW_X, (float)GG_VIEW_Y, (float)GG_VIEW_SIZE, (float)GG_VIEW_SIZE};
    Vector2 origin = {0.0f, 0.0f};
    DrawTexturePro(A.target.texture, src, dst, origin, 0.0f, WHITE);

    char extra[64];
    snprintf(extra, sizeof(extra), "player %s", s->game.save.name);
    gfx_hud("GRITTY GOLF", Save_Score(&s->game.save), extra);
}
