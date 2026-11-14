/* Tower Stacker - raylib front-end: window loop and drawing ONLY. All rules live in
 * tower_stacker_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "tower_stacker_logic.h"

/* Tones for the three "lost part" sizes (small cut = high, big cut = low). */
static const float k_cut_hz[3] = {523.0f, 392.0f, 262.0f};

/* ---- shared look: playfield panel, frame, rules in the left gutter ---- */
static void draw_help(const char *title, const char *const *lines, int n) {
    gfx_text(title, 28, 64, 22, ARC_COL_CYAN);
    for (int i = 0; i < n; i++)
        gfx_text(lines[i], 28, 104 + i * 30, 18, ARC_COL_DIM);
}

static void draw_frame(void) {
    Rectangle r = {(float)ARC_PF_X, (float)ARC_PF_Y, (float)ARC_PF_W, (float)ARC_PF_H};
    DrawRectangleLinesEx(r, 2.0f, Fade(ARC_COL_CYAN, 0.5f));
}

/* The whole tower is redrawn from the arrays every frame (floor `score` on top, floor 0 at the bottom).
 * `top` and `fh` come from tower_stacker_zoom(): the normal view, or the zoom after a tall game over. */
static void draw_tower(const tower_stacker_State *s, float top, float fh) {
    for (int i = s->score; i >= 0; i--) {
        float y = top + (float)(s->score - i) * fh;
        if (y > (float)s->h || y + fh < 0.0f)
            continue; /* off screen: skip (a tall tower has up to 1024 floors) */
        float x = (float)ARC_PF_X + (float)s->bases_x[i];
        float sy = (float)ARC_PF_Y + y;
        Color c = i % 2 == 0 ? ARC_COL_CYAN : ColorBrightness(ARC_COL_CYAN, -0.25f);
        DrawRectangle((int)x, (int)sy, s->bases_w[i], (int)fh, c);
        DrawRectangleLines((int)x, (int)sy, s->bases_w[i], (int)fh, Fade(BLACK, 0.5f));
        for (int j = 0; j < TS_WINDOWS; j++) {
            int wx = s->window[i][j];
            if (wx != 0)
                DrawRectangle(ARC_PF_X + wx, (int)(sy + fh / 3.0f), (int)(s->base_h / 3.0f), (int)(fh / 3.0f),
                              ARC_COL_YELLOW);
        }
    }
}

static void draw_bar(const tower_stacker_State *s) {
    Color c = ARC_COL_MAGENTA;
    if (s->state == TS_MISSED)
        c = ARC_COL_RED;
    else if (s->state == TS_FADING)
        c = Fade(ARC_COL_MAGENTA, arc_clampf(s->bar_alpha, 0.0f, 1.0f));
    Rectangle r = {(float)ARC_PF_X + s->bar_x, (float)ARC_PF_Y + s->bar_y - s->bar_h, s->bar_w, s->bar_h};
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 2.0f, Fade(BLACK, 0.4f));
}

static void draw_world(const tower_stacker_State *s) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.92f));
    BeginScissorMode(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text_center(buf, ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 8 - 4, 60, Fade(ARC_COL_DIM, 0.55f));
    float top, fh;
    tower_stacker_zoom(s, &top, &fh);
    draw_tower(s, top, fh);
    bool zooming = s->over && s->score >= TS_SCROLL_FLOORS && s->over_t > 0.0f;
    if (!zooming) /* during the zoom the bar is not part of the picture */
        draw_bar(s);
    if (s->over)
        gfx_text_center(s->capped ? "TOWER COMPLETE!" : "MISSED!", ARC_PF_X + s->w / 2, ARC_PF_Y + 16, 28,
                        s->capped ? ARC_COL_GREEN : ARC_COL_RED);
    else if (s->score == 0 && s->state == TS_SLIDING)
        gfx_text_center("click / Space / any key: drop", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 - 150, 18,
                        ARC_COL_TEXT);
    EndScissorMode();
}

static void draw_right_gutter(const tower_stacker_State *s) {
    char buf[64];
    gfx_text("FLOORS", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "tower width: %d px", (int)(s->base_w + 0.5f));
    gfx_text(buf, 920, 170, 18, ARC_COL_DIM);
    snprintf(buf, sizeof(buf), "perfect landings: %d", s->perfects);
    gfx_text(buf, 920, 200, 18, ARC_COL_DIM);
}

static GameResult play(const GameContext *ctx) {
    static tower_stacker_State s; /* ~20 KB of floor arrays: static keeps it off the stack */
    tower_stacker_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound cut[3] = {0}, perfect = {0}, lose = {0};
    if (audio) {
        for (int i = 0; i < 3; i++)
            cut[i] = gfx_make_tone(k_cut_hz[i], 0.18f, 0.5f);
        perfect = gfx_make_tone(1047.0f, 0.25f, 0.3f);
        lose = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int perfects_seen = 0, cuts_seen = 0;
    bool lose_played = false;

    static const char *const help[] = {"A bar slides left and right.",
                                       "Click, Space or any key",
                                       "drops it on the tower.",
                                       "",
                                       "The overhang is cut off, so",
                                       "the tower gets narrower.",
                                       "A perfect landing keeps",
                                       "the full width.",
                                       "",
                                       "Miss the tower = game over."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        tower_stacker_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.perfects != perfects_seen && audio)
            PlaySound(perfect);
        perfects_seen = s.perfects;
        if (s.cuts != cuts_seen && audio)
            PlaySound(cut[s.last_cut_class]);
        cuts_seen = s.cuts;
        if (s.over && !s.capped && !lose_played) {
            lose_played = true;
            if (audio)
                PlaySound(lose);
        }
        BeginDrawing();
        gfx_backdrop(t);
        draw_world(&s);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("TOWER STACKER", tower_stacker_score(&s), TextFormat("width %d", (int)(s.base_w + 0.5f)));
        arcade_frame_end();
        if (tower_stacker_is_over(&s))
            break;
    }
    if (audio) {
        for (int i = 0; i < 3; i++)
            UnloadSound(cut[i]);
        UnloadSound(perfect);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = tower_stacker_score(&s);
    r.finished = tower_stacker_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_tower_stacker_def = {
    "tower_stacker",                                 /* id                */
    "Tower Stacker",                                 /* title             */
    "Drop the sliding bar and keep your tower wide", /* tagline           */
    "click, Space or any key: drop the bar   a perfect landing keeps the full width",
    160,  /* menu order        */
    true, /* ranked            */
    play};
