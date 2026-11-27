/* The live scene behind the hub screens. Two scenes share the slot (see app.h):
 *
 *  - the city (default): the whole Synthwave City game running as a guest of the hub, through the small API
 * in deferred/synthwave_city/synthwave_city_session.h. A drone tour flies it by itself; when the menu hides,
 *    the player flies it instead;
 *  - the sand: a falling-sand animation (core/sandbox, same engine as deferred/sandbox). It also holds the
 *    "vault": a stone basin on the right. Fill it with sand or water and it opens, which unlocks the archive
 * of deferred games on the menu (rules: core/vault, docs: docs/DEFERRED.md).
 *
 * Only the scene being shown is stepped, so the vault can never fill up while the city is shown. */
#include <math.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/rng.h"
#include "arcade/vault.h"
#include "synthwave_city_session.h"

#define BD_W 160
#define BD_H 90
#define BD_CELL 8 /* 160 * 8 = 1280, 90 * 8 = 720 */

/* The vault interior in sandbox cells: 16 wide, 10 tall, hanging in the open on the right-hand side.
 * It sits clear of the ledges and streams (x 98..135), and the drain below keeps heaps from ever reaching it,
 * so nothing fills it by accident. Everything left of x = 96 is hidden behind the menu panel: keep it empty.
 */
#define VAULT_X 139
#define VAULT_Y 50
#define VAULT_W 16
#define VAULT_H 10

static Sandbox g_sb;
static GfxSandboxView g_view;
static Vault g_vault;
static Rng g_rng; /* where the next grain of sand / drop of water appears */
static unsigned g_frame;
static BackdropMode g_mode = BACKDROP_CITY;
static bool g_flying; /* city only: the player holds the camera */

/* A stone shelf from x0 up to (not including) x1 on row y, with a 2-cell gap every `period` cells (the first
 * gap starts `first_gap` cells in). Sand and water rain through the gaps, so a shelf never builds a tall
 * heap. */
static void build_sieve(int x0, int x1, int y, int period, int first_gap) {
    for (int x = x0; x < x1; x++) {
        int phase = (x - x0 - first_gap + period) % period;
        if (phase >= 2) /* the 2 cells right after `first_gap` stay empty */
            sandbox_set(&g_sb, x, y, EL_STONE);
    }
}

static void build_scene(void) {
    sandbox_clear(&g_sb);
    /* two sieves under the streams: what falls on them piles a little, then drips on to the next one, and
     * whatever reaches the bottom row drains away, so the scene reaches a steady state on its own */
    build_sieve(100, 132, 44, 8, 3);
    build_sieve(98, 136, 76, 9, 5);
    vault_build(&g_sb, &g_vault, VAULT_X, VAULT_Y, VAULT_W, VAULT_H);
}

void backdrop_init(unsigned seed) {
    sandbox_init(&g_sb, BD_W, BD_H, seed);
    rng_seed(&g_rng, seed);
    build_scene();
    gfx_sandbox_view_init(&g_view, &g_sb);
    g_frame = 0;
    swc_session_begin(seed); /* the city: same seed, so a smoke run always sees the same one */
    g_mode = BACKDROP_CITY;
    g_flying = false;
}

void backdrop_shutdown(void) {
    swc_session_end();
    gfx_sandbox_view_free(&g_view);
}

/* ---------------------------------------------------------------- which scene is shown */
BackdropMode backdrop_mode(void) {
    return g_mode;
}

void backdrop_set_mode(BackdropMode mode) {
    if (mode == g_mode)
        return;
    if (mode != BACKDROP_CITY)
        backdrop_set_flying(false); /* leaving the city: the camera goes back to the drone */
    g_mode = mode;
    /* a hidden city is not stepped, so its soundtrack would starve: it waits, paused, until it is shown again
     */
    swc_session_pause_audio(mode != BACKDROP_CITY);
}

/* ---------------------------------------------------------------- the city */
void backdrop_set_flying(bool flying) {
    g_flying = flying && g_mode == BACKDROP_CITY;
    swc_session_set_control(g_flying);
}

void backdrop_fly(const ArcadeInput *in) {
    if (g_mode == BACKDROP_CITY)
        swc_session_update(in, ARC_DT);
}

void backdrop_pause_audio(bool paused) {
    swc_session_pause_audio(paused || g_mode != BACKDROP_CITY);
}

void backdrop_toggle_mute(void) {
    swc_session_set_muted(!swc_session_muted());
}

bool backdrop_city_touring(void) {
    return swc_session_touring();
}

bool backdrop_city_grabbed(void) {
    return swc_session_grabbed();
}

void backdrop_city_mouse_free(void) {
    swc_session_mouse_free();
}

/* ---------------------------------------------------------------- the sand */
void backdrop_update(bool paint_sand, bool paint_water, int mouse_x, int mouse_y) {
    if (g_mode == BACKDROP_CITY) {
        swc_session_update(NULL, ARC_DT); /* nobody flies from here: only the drone moves the camera */
        return;
    }
    g_frame++;
    /* single grains at random x positions: reads as falling sand and water instead of a solid ribbon */
    sandbox_paint(&g_sb, rng_range(&g_rng, 104, 119), 2, 0, EL_SAND); /* sand drizzle  */
    if (g_frame % 3 == 0)
        sandbox_paint(&g_sb, rng_range(&g_rng, 106, 121), 2, 0, EL_WATER); /* water trickle */
    if (paint_sand)
        sandbox_paint(&g_sb, mouse_x / BD_CELL, mouse_y / BD_CELL, 2, EL_SAND);
    if (paint_water)
        sandbox_paint(&g_sb, mouse_x / BD_CELL, mouse_y / BD_CELL, 2, EL_WATER);
    sandbox_step(&g_sb);
    (void)vault_update(&g_sb, &g_vault); /* opens (and drains) once it is full; the menu polls is_open */

    /* The bottom row is a drain: whatever reaches it disappears. Material can only pile up on the ledges and
     * in the vault, so the scene reaches a steady state by itself (no heaps, no periodic wipe). */
    for (int x = 0; x < BD_W; x++)
        sandbox_set(&g_sb, x, BD_H - 1, EL_EMPTY);
}

/* The city can be bright right where the hub's small text sits (the player's name, the key hints, the
 * version). A soft shade along the top and bottom edges keeps that text readable without dimming the whole
 * picture. Plain strips, not a gradient quad: the CPU rasteriser does not blend those right (the game's own
 * HUD shade does the same). */
#define SHADE_STRIPS 14
#define SHADE_TOP_H 140
#define SHADE_BOTTOM_H 110

static void draw_edge_shade(void) {
    /* the shade follows the window's edges, not the 16:9 canvas's: the strips start at the real top and
     * bottom (and are as wide as the window) when the window has another shape */
    Rectangle win = arcade_view_bounds();
    int x = (int)floorf(win.x), w = (int)ceilf(win.width) + 1;
    int top = (int)floorf(win.y), bottom = (int)ceilf(win.y + win.height);
    for (int i = 0; i < SHADE_STRIPS; i++) {
        float k = 1.0f - (float)i / (float)SHADE_STRIPS; /* 1 at the screen edge, towards 0 inside */
        int top_h = SHADE_TOP_H / SHADE_STRIPS, bottom_h = SHADE_BOTTOM_H / SHADE_STRIPS;
        DrawRectangle(x, top + i * top_h, w, top_h, Fade(ARC_COL_BG, 0.80f * k * k));
        DrawRectangle(x, bottom - (i + 1) * bottom_h, w, bottom_h, Fade(ARC_COL_BG, 0.88f * k * k));
    }
}

void backdrop_draw(float alpha) {
    if (g_mode == BACKDROP_CITY) {
        swc_session_draw();
        if (alpha <
            1.0f) { /* a hub screen is on top: the sand fades into the dark background, the city too */
            Rectangle win = arcade_view_bounds();
            DrawRectangle((int)floorf(win.x), (int)floorf(win.y), (int)ceilf(win.width) + 1,
                          (int)ceilf(win.height) + 1, Fade(ARC_COL_BG, 1.0f - alpha));
            draw_edge_shade();
        }
        return;
    }
    Rectangle dst = {0.0f, 0.0f, (float)ARC_SCREEN_W, (float)ARC_SCREEN_H};
    gfx_sandbox_view_draw(&g_view, &g_sb, dst, Fade(WHITE, alpha));
}

/* ---------------------------------------------------------------- vault */
bool backdrop_vault_is_open(void) {
    return g_vault.open;
}

float backdrop_vault_progress(void) {
    return vault_progress(&g_sb, &g_vault);
}

void backdrop_vault_open_now(void) {
    vault_open_now(&g_sb, &g_vault);
}

void backdrop_vault_mouth(int *x, int *y) {
    *x = (g_vault.x + g_vault.w / 2) * BD_CELL;
    *y = (g_vault.y - 4) * BD_CELL; /* a little above the open top, where a hand would hold the water */
}

/* A padlock from a rounded rectangle and a ring; `open` lifts the shackle off the body. (cx, cy) = body top.
 */
static void draw_lock(int cx, int cy, bool open, Color c) {
    float lift = open ? 7.0f : 0.0f;
    Vector2 arc_centre = {(float)cx, (float)cy - 6.0f - lift};
    DrawRing(arc_centre, 5.0f, 7.5f, 180.0f, 360.0f, 14, c); /* the top half of the shackle */
    DrawRectangle(cx - 8, cy - 6 - (int)lift, 3, 6, c);      /* its two legs */
    DrawRectangle(cx + 5, cy - 6 - (int)lift, 3, 6, c);
    DrawRectangleRounded((Rectangle){(float)cx - 13.0f, (float)cy, 26.0f, 19.0f}, 0.25f, 6, c);
    DrawCircle(cx, cy + 8, 2.5f, ARC_COL_BG); /* keyhole */
    DrawRectangle(cx - 1, cy + 9, 2, 5, ARC_COL_BG);
}

void backdrop_vault_draw(bool archive_open, float t) {
    if (g_mode != BACKDROP_SANDBOX)
        return; /* the vault is part of the sand scene */
    int left = (g_vault.x - 1) * BD_CELL, right = (g_vault.x + g_vault.w + 1) * BD_CELL;
    int top = g_vault.y * BD_CELL, floor_bottom = (g_vault.y + g_vault.h + 1) * BD_CELL;
    int cx = (left + right) / 2;
    float progress = backdrop_vault_progress();

    if (!archive_open) {
        /* a slow pulse on the outline is the one thing that moves on its own: it says "this is interactive"
         */
        float pulse = 0.5f + 0.5f * sinf(t * 2.2f);
        Rectangle frame = {(float)left - 5.0f, (float)top - 6.0f, (float)(right - left) + 10.0f,
                           (float)(floor_bottom - top) + 12.0f};
        DrawRectangleRoundedLinesEx(frame, 0.08f, 6, 2.0f, Fade(ARC_COL_CYAN, 0.18f + 0.22f * pulse));
        /* dashes mark how full is "full enough" */
        int level_y = (g_vault.y + g_vault.h - (int)((float)g_vault.h * VAULT_OPEN_SHARE)) * BD_CELL;
        for (int x = left + BD_CELL; x < right - BD_CELL; x += 14)
            DrawRectangle(x, level_y, 8, 2, Fade(ARC_COL_CYAN, 0.45f));
    }

    Color lock_colour = archive_open ? ARC_COL_GREEN : ColorLerp(ARC_COL_DIM, ARC_COL_YELLOW, progress);
    draw_lock(cx, floor_bottom + 14, archive_open, lock_colour);
    gfx_text_center(archive_open ? "archive open" : "archive", cx, floor_bottom + 40, 14,
                    archive_open ? ARC_COL_GREEN : ARC_COL_DIM);
}
