/* Sandbox - raylib front-end. All rules live in sandbox_logic.c. */
#include "arcade/gfx.h"
#include "sandbox_logic.h"

static const char k_hint[] = "0-9 / [ ] / wheel: element   right-click a slot: 2nd element   X: swap   "
                             "+/-: brush   T: thermal   C: clear   R: reset   Space: pause";

/* "Steel  412 C" for the cell under the pointer (the built-in font has no degree sign). */
static const char *probe_text(const sbx_State *s, const ArcadeInput *in) {
    ElementId id;
    int t;
    if (!sbx_probe(s, in->aim_x, in->aim_y, &id, &t))
        return NULL;
    return TextFormat("%s  %d C", id == EL_EMPTY ? "Air" : sandbox_element(id)->name, t);
}

/* A small label next to the pointer, kept inside the window. */
static void draw_probe_tip(const char *text, const ArcadeInput *in) {
    int w = gfx_text_width(text, 16) + 12;
    int x = (int)in->aim_x + 16, y = (int)in->aim_y + 14;
    if (x + w > ARC_SCREEN_W - 4)
        x = (int)in->aim_x - 16 - w;
    if (y + 24 > SBX_H * SBX_CELL)
        y = (int)in->aim_y - 14 - 24;
    DrawRectangle(x, y, w, 24, Fade(ARC_COL_PANEL, 0.9f));
    DrawRectangleLines(x, y, w, 24, Fade(WHITE, 0.25f));
    gfx_text(text, x + 6, y + 4, 16, ARC_COL_TEXT);
}

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
        /* the right mouse button's element: magenta, inset so both can show on a slot */
        if (s->element2 == id)
            DrawRectangleLines(x + 2, y + 1, SBX_PAL_SLOT_W - 6, SBX_PAL_SLOT_H - 5, ARC_COL_MAGENTA);
        if (s->element == id)
            DrawRectangleLines(x + 1, y, SBX_PAL_SLOT_W - 4, SBX_PAL_SLOT_H - 3, ARC_COL_CYAN);
        const char *label = i < 10 ? TextFormat("%d %s", i, e->name) : e->name; /* row 1 = number keys */
        int size = gfx_text_width(label, 16) > SBX_PAL_SLOT_W - 32 ? 14 : 16;   /* long names shrink to fit */
        bool picked = s->element == id || s->element2 == id;
        gfx_text(label, x + 26, y + 3 + (16 - size) / 2, size, picked ? ARC_COL_TEXT : ARC_COL_DIM);
    }
}

/* One frame: the world, the brush outline, the HUD, the temperature label and the palette. */
static void draw_frame(const sbx_State *s, GfxSandboxView *view, const ArcadeInput *in) {
    ClearBackground(ARC_COL_BG);
    Rectangle dst = {0.0f, 0.0f, (float)(SBX_W * SBX_CELL), (float)(SBX_H * SBX_CELL)};
    gfx_sandbox_view_draw(view, &s->world, dst, WHITE);
    if (!sbx_over_panel(in->aim_x, in->aim_y)) /* brush outline */
        DrawCircleLines((int)in->aim_x, (int)in->aim_y, ((float)s->brush + 0.5f) * (float)SBX_CELL,
                        Fade(WHITE, 0.45f));
    const char *probe = probe_text(s, in);
    gfx_hud("SANDBOX", sbx_score(s),
            TextFormat("L %s   R %s   brush %d%s%s", sandbox_element(s->element)->name,
                       sandbox_element(s->element2)->name, s->brush,
                       s->world.heat_view ? "   THERMAL VIEW" : "", s->paused ? "   PAUSED" : ""));
    gfx_text(k_hint, 16, 42, 16, ARC_COL_DIM);
    if (probe != NULL)
        draw_probe_tip(probe, in);
    draw_palette(s, in);
}

static GameResult play(const GameContext *ctx) {
    static sbx_State s; /* ~400 KB: must not live on the stack */
    static GfxSandboxView view;
    sbx_init(&s, ctx->seed);
    gfx_sandbox_view_init(&view, &s.world);
    arcade_loop_begin(ctx);
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        sbx_step(&s, &in, ARC_DT);
        BeginDrawing();
        draw_frame(&s, &view, &in);
        arcade_frame_end();
    }
    gfx_sandbox_view_free(&view);
    GameResult r;
    r.score = sbx_score(&s);
    /* there is no game over: leaving ends the run, and the score is the cells filled at that moment (the
     * starter scene already counts, and its plants grow by themselves). Only a run in which the player
     * painted is recorded, so a quick look and Esc does not put the untouched scene on the board. */
    r.finished = sbx_painted(&s);
    return r;
}

const GameDef game_sandbox_def = {"sandbox",
                                  "Sandbox",
                                  "Paint sand, water, fire, lava and uranium and watch it all react",
                                  "left mouse: paint   right mouse: 2nd element (eraser)   0-9 / [ ] / "
                                  "wheel: element   +/-: brush   T: thermal   Space: pause",
                                  90,
                                  play};
