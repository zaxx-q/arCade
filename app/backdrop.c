/* Live falling-sand animation behind the hub screens (uses core/sandbox, same engine as games/sandbox). */
#include "app.h"
#include "arcade/gfx.h"

#define BD_W 160
#define BD_H 90
#define BD_CELL 8 /* 160 * 8 = 1280, 90 * 8 = 720 */

static Sandbox g_sb;
static GfxSandboxView g_view;
static unsigned g_frame;

static void build_scene(void) {
    sandbox_clear(&g_sb);
    /* a few stone ledges so the sand and water have something to pile on */
    for (int x = 20; x < 58; x++)
        sandbox_set(&g_sb, x, 70, EL_STONE);
    for (int x = 96; x < 150; x++)
        sandbox_set(&g_sb, x, 78, EL_STONE);
    for (int x = 70; x < 92; x++)
        sandbox_set(&g_sb, x, 56, EL_STONE);
    for (int y = 60; y < 70; y++)
        sandbox_set(&g_sb, 20, y, EL_STONE);
}

void backdrop_init(unsigned seed) {
    sandbox_init(&g_sb, BD_W, BD_H, seed);
    build_scene();
    gfx_sandbox_view_init(&g_view, &g_sb);
    g_frame = 0;
}

void backdrop_shutdown(void) {
    gfx_sandbox_view_free(&g_view);
}

void backdrop_update(bool paint_sand, bool paint_water, int mouse_x, int mouse_y) {
    g_frame++;
    if (g_frame % 3 == 0)
        sandbox_paint(&g_sb, 100 + (int)(g_frame / 3 % 30), 2, 1, EL_SAND); /* sand drizzle  */
    if (g_frame % 5 == 0)
        sandbox_paint(&g_sb, 120 + (int)(g_frame / 5 % 20), 2, 0, EL_WATER); /* water trickle */
    if (paint_sand)
        sandbox_paint(&g_sb, mouse_x / BD_CELL, mouse_y / BD_CELL, 2, EL_SAND);
    if (paint_water)
        sandbox_paint(&g_sb, mouse_x / BD_CELL, mouse_y / BD_CELL, 2, EL_WATER);
    sandbox_step(&g_sb);

    /* when the screen is nearly full, wipe the loose material and keep the stone */
    if (g_frame % 120 == 0 &&
        sandbox_count(&g_sb, EL_SAND) + sandbox_count(&g_sb, EL_WATER) > BD_W * BD_H / 3) {
        for (int y = 0; y < BD_H; y++)
            for (int x = 0; x < BD_W; x++)
                if (sandbox_get(&g_sb, x, y) != EL_STONE)
                    sandbox_set(&g_sb, x, y, EL_EMPTY);
    }
}

void backdrop_draw(float alpha) {
    Rectangle dst = {0.0f, 0.0f, (float)ARC_SCREEN_W, (float)ARC_SCREEN_H};
    gfx_sandbox_view_draw(&g_view, &g_sb, dst, Fade(WHITE, alpha));
}
