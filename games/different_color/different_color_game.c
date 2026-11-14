/* A Different Color - raylib front-end: window loop and drawing ONLY. All rules live in
 * different_color_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "different_color_logic.h"

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

static Color to_color(const float c[3], float scale) {
    Color k = {(uint8_t)(c[0] * scale * 255.0f), (uint8_t)(c[1] * scale * 255.0f),
               (uint8_t)(c[2] * scale * 255.0f), 255};
    return k;
}

/* Cells share their edges (rounded once per edge) so no 1 px seams show between them. */
static void draw_board(const different_color_State *s, float top, bool bottom) {
    for (int i = 0; i < s->cols * s->rows; i++) {
        int col = i % s->cols, row = i / s->cols;
        int x0 = (int)lroundf(DC_X1 + s->rectw * (float)col);
        int x1 = (int)lroundf(DC_X1 + s->rectw * (float)(col + 1));
        int y0 = (int)lroundf(top + s->recth * (float)row);
        int y1 = (int)lroundf(top + s->recth * (float)(row + 1));
        const float *c = (bottom && i == s->dif) ? s->odd : s->cell[i];
        DrawRectangle(ARC_PF_X + x0, ARC_PF_Y + y0, x1 - x0, y1 - y0, to_color(c, 1.0f));
    }
}

static void draw_ring_on_odd(const different_color_State *s, bool bottom) {
    Vec2 c = different_color_odd_center(s);
    float dy = bottom ? s->y2 - DC_Y1 : 0.0f;
    Vector2 p = {(float)ARC_PF_X + c.x, (float)ARC_PF_Y + c.y + dy};
    float rmin = s->recth < s->rectw ? s->recth : s->rectw;
    float r = rmin * 0.5f - 8.0f;
    if (r < 4.0f)
        r = 4.0f;
    DrawRing(p, r - 5.0f > 0.0f ? r - 5.0f : 0.0f, r + 5.0f, 0.0f, 360.0f, 48,
             to_color(bottom ? s->odd : s->cell[s->dif], 0.5f));
    if (s->n <= 30)
        DrawCircleV(p, 6.0f, Fade(BLACK, 0.8f));
}

static void draw_world(const different_color_State *s) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.95f));
    draw_board(s, DC_Y1, false);
    draw_board(s, s->y2, true);
    /* white separator strip with a thin black line in the middle */
    DrawRectangle(ARC_PF_X, ARC_PF_Y + (int)(s->y2 - 16.0f), s->w, 16, WHITE);
    DrawRectangle(ARC_PF_X, ARC_PF_Y + s->h / 2 - 1, s->w, 2, BLACK);
    if (s->timed) {
        float frac = arc_clampf(s->time_left / DC_TIME_LIMIT, 0.0f, 1.0f);
        Color bar = frac > 0.33f ? ARC_COL_GREEN : (frac > 0.15f ? ARC_COL_YELLOW : ARC_COL_RED);
        DrawRectangle(ARC_PF_X + 16, ARC_PF_Y + s->h - 48, (int)((float)(s->w - 32) * frac), 16, bar);
    } else if (!s->over) {
        gfx_text_center("Tap the different block", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h - 56, 22,
                        ARC_COL_TEXT);
    }
    if (s->over) {
        draw_ring_on_odd(s, false);
        draw_ring_on_odd(s, true);
        const char *msg = s->capped ? "ALL BOARDS SOLVED!" : (s->timed_out ? "TIME UP!" : "WRONG CELL!");
        /* the empty strip above the top board keeps the message off the cells and the separator */
        gfx_text_center(msg, ARC_PF_X + s->w / 2, ARC_PF_Y + 24, 28, s->capped ? ARC_COL_GREEN : ARC_COL_RED);
    }
}

static void draw_right_gutter(const different_color_State *s) {
    char buf[64];
    gfx_text("BOARDS SOLVED", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "this board: %d cells (%d x %d)", s->n, s->cols, s->rows);
    gfx_text(buf, 920, 170, 18, ARC_COL_DIM);
    if (s->timed) {
        snprintf(buf, sizeof(buf), "time left: %.1f s", s->time_left > 0.0f ? s->time_left : 0.0f);
        gfx_text(buf, 920, 200, 18, s->time_left < 10.0f ? ARC_COL_RED : ARC_COL_DIM);
    } else {
        gfx_text("first board: no timer", 920, 200, 18, ARC_COL_DIM);
    }
}

static GameResult play(const GameContext *ctx) {
    static different_color_State s; /* ~5 KB of colours: static keeps it off the stack */
    different_color_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound chime = {0}, tick = {0}, tack = {0}, lose = {0};
    if (audio) {
        chime = gfx_make_tone(880.0f, 0.15f, 0.5f);
        tick = gfx_make_tone(1400.0f, 0.04f, 0.35f);
        tack = gfx_make_tone(1000.0f, 0.04f, 0.35f);
        lose = gfx_make_tone(160.0f, 0.5f, 0.6f);
    }
    int solved_seen = 0, ticks_seen = 0;
    bool lose_played = false;

    static const char *const help[] = {"One cell on the BOTTOM board",
                                       "has a slightly different",
                                       "colour than on the top board.",
                                       "Click it on either board.",
                                       "",
                                       "Correct: +1 and more cells.",
                                       "Wrong cell: game over.",
                                       "30 s per board after the first."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        different_color_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.solved != solved_seen && audio)
            PlaySound(chime);
        solved_seen = s.solved;
        if (s.ticks != ticks_seen && audio)
            PlaySound(s.ticks % 2 ? tick : tack);
        ticks_seen = s.ticks;
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
        gfx_hud("A DIFFERENT COLOR", different_color_score(&s), TextFormat("%d cells", s.n));
        arcade_frame_end();
        if (different_color_is_over(&s))
            break;
    }
    if (audio) {
        UnloadSound(chime);
        UnloadSound(tick);
        UnloadSound(tack);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = different_color_score(&s);
    r.finished = different_color_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_different_color_def = {
    "different_color",                                    /* id                */
    "A Different Color",                                  /* title             */
    "Find the one cell with a slightly different colour", /* tagline           */
    "click the odd cell on either board   30 s per board after the first   a wrong click ends the run",
    130,  /* menu order        */
    true, /* ranked            */
    play};
