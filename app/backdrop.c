/* Live falling-sand animation behind the hub screens (uses core/sandbox, same engine as deferred/sandbox).
 * The scene also holds the "vault": a stone basin on the right. Fill it with sand or water and it opens,
 * which unlocks the archive of deferred games on the menu (rules: core/vault, docs: docs/DEFERRED.md). */
#include <math.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/rng.h"
#include "arcade/vault.h"

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
}

void backdrop_shutdown(void) {
    gfx_sandbox_view_free(&g_view);
}

void backdrop_update(bool paint_sand, bool paint_water, int mouse_x, int mouse_y) {
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

void backdrop_draw(float alpha) {
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
