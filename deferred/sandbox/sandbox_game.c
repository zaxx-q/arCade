/* Sandbox - raylib front-end. All rules live in sandbox_logic.c. */
#include "arcade/gfx.h"
#include "sandbox_logic.h"

static void draw_palette(const sbx_State *s) {
    DrawRectangle(0, ARC_SCREEN_H - 44, ARC_SCREEN_W, 44, Fade(ARC_COL_PANEL, 0.9f));
    for (int i = 0; i < EL_COUNT; i++) {
        const ElementDef *e = sandbox_element((ElementId)i);
        int x = 16 + i * 170;
        Color swatch = gfx_rgba(e->r, e->g, e->b, 255);
        DrawRectangle(x, ARC_SCREEN_H - 34, 24, 24, swatch);
        if ((int)s->element == i)
            DrawRectangleLines(x - 3, ARC_SCREEN_H - 37, 30, 30, ARC_COL_CYAN);
        gfx_text(TextFormat("%d %s", i, e->name), x + 34, ARC_SCREEN_H - 32, 18, ARC_COL_TEXT);
    }
    gfx_text(TextFormat("brush %d   [+/-]   [C] clear   [R] reset   [Space] %s   right mouse: erase",
                        s->brush, s->paused ? "RESUME" : "pause"),
             16 + EL_COUNT * 170, ARC_SCREEN_H - 32, 16, ARC_COL_DIM);
}

static GameResult play(const GameContext *ctx) {
    static sbx_State s; /* ~115 KB: must not live on the stack */
    static GfxSandboxView view;
    sbx_init(&s, ctx->seed);
    gfx_sandbox_view_init(&view, &s.world);
    arcade_loop_begin(ctx);
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        sbx_step(&s, &in, ARC_DT);
        BeginDrawing();
        ClearBackground(ARC_COL_BG);
        Rectangle dst = {0.0f, 0.0f, (float)ARC_SCREEN_W, (float)ARC_SCREEN_H};
        gfx_sandbox_view_draw(&view, &s.world, dst, WHITE);
        gfx_hud("SANDBOX", sbx_score(&s), s.paused ? "PAUSED" : "");
        draw_palette(&s);
        arcade_frame_end();
    }
    gfx_sandbox_view_free(&view);
    GameResult r;
    r.score = sbx_score(&s);
    r.finished = false; /* unranked: nothing is recorded */
    return r;
}

const GameDef game_sandbox_def = {
    "sandbox",
    "Sandbox",
    "Paint sand, water and stone and watch it flow",
    "left mouse: paint   right mouse: erase   0-3 / wheel: element   +/-: brush   Space: pause",
    90,
    false,
    play};
