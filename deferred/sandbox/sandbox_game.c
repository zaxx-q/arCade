/* Sandbox - raylib front-end. All rules live in sandbox_logic.c. */
#include "arcade/gfx.h"
#include "sandbox_logic.h"

static const char k_hint[] =
    "0-9 / [ ] / wheel: element   +/-: brush   C: clear   R: reset   Space: pause   right mouse: erase";

static void draw_palette(const sbx_State *s, const ArcadeInput *in) {
    DrawRectangle(0, SBX_PAL_Y - SBX_PAL_PAD, ARC_SCREEN_W, ARC_SCREEN_H - SBX_PAL_Y + SBX_PAL_PAD,
                  ARC_COL_PANEL);
    int hover = sbx_palette_slot_at(in->aim_x, in->aim_y);
    for (int i = 0; i < sandbox_palette_count(); i++) {
        ElementId id = sandbox_palette_id(i);
        const ElementDef *e = sandbox_element(id);
        int x = SBX_PAL_X + (i % SBX_PAL_COLS) * SBX_PAL_SLOT_W;
        int y = SBX_PAL_Y + (i / SBX_PAL_COLS) * SBX_PAL_SLOT_H;
        if (i == hover)
            DrawRectangle(x, y, SBX_PAL_SLOT_W - 4, SBX_PAL_SLOT_H - 3, Fade(WHITE, 0.10f));
        DrawRectangle(x + 4, y + 3, 16, 16, gfx_rgba(e->r, e->g, e->b, 255));
        if (s->element == id)
            DrawRectangleLines(x + 1, y, SBX_PAL_SLOT_W - 4, SBX_PAL_SLOT_H - 3, ARC_COL_CYAN);
        const char *label = i < 10 ? TextFormat("%d %s", i, e->name) : e->name; /* row 1 = number keys */
        int size = gfx_text_width(label, 16) > SBX_PAL_SLOT_W - 32 ? 14 : 16;   /* long names shrink to fit */
        gfx_text(label, x + 26, y + 3 + (16 - size) / 2, size, s->element == id ? ARC_COL_TEXT : ARC_COL_DIM);
    }
}

static GameResult play(const GameContext *ctx) {
    static sbx_State s; /* ~175 KB: must not live on the stack */
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
        Rectangle dst = {0.0f, 0.0f, (float)(SBX_W * SBX_CELL), (float)(SBX_H * SBX_CELL)};
        gfx_sandbox_view_draw(&view, &s.world, dst, WHITE);
        if (!sbx_over_panel(in.aim_x, in.aim_y)) /* brush outline */
            DrawCircleLines((int)in.aim_x, (int)in.aim_y, ((float)s.brush + 0.5f) * (float)SBX_CELL,
                            Fade(WHITE, 0.45f));
        gfx_hud("SANDBOX", sbx_score(&s),
                TextFormat("%s   brush %d%s", sandbox_element(s.element)->name, s.brush,
                           s.paused ? "   PAUSED" : ""));
        gfx_text(k_hint, 16, 42, 16, ARC_COL_DIM);
        draw_palette(&s, &in);
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
    "Paint sand, water, fire, lava and uranium and watch it all react",
    "left mouse: paint   click the palette or press 0-9 / [ ] / wheel: element   +/-: brush   Space: pause",
    90,
    false,
    play};
