/* BW Reflex - raylib front-end: window loop and drawing ONLY. All rules live in bw_reflex_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "bw_reflex_logic.h"

/* C-major scale for the 8 match notes (Hz). */
static const float k_scale[8] = {262, 294, 330, 349, 392, 440, 494, 523};

/* ---- shared look: dark backdrop, playfield panel, frame, rules in the left gutter ---- */
static void draw_help(const char *title, const char *const *lines, int n) {
    gfx_text(title, 28, 64, 22, ARC_COL_CYAN);
    for (int i = 0; i < n; i++)
        gfx_text(lines[i], 28, 104 + i * 30, 18, ARC_COL_DIM);
}

static void draw_frame(void) {
    Rectangle r = {(float)ARC_PF_X, (float)ARC_PF_Y, (float)ARC_PF_W, (float)ARC_PF_H};
    DrawRectangleLinesEx(r, 2.0f, Fade(ARC_COL_CYAN, 0.5f));
}

static Color mono(int white) {
    return white ? WHITE : BLACK;
}

static void draw_playfield(const bw_reflex_State *s) {
    /* mid-grey panel so that both black and white stay visible (colour IS the mechanic) */
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, (Color){128, 128, 128, 255});
    Color block_col = mono(s->block_white);
    /* the base strip */
    int base_top = ARC_PF_Y + s->h - s->base_h;
    DrawRectangle(ARC_PF_X, base_top, s->w, s->base_h, mono(s->base_white));
    /* the colour word sits on the base in the block's colour: it vanishes when the base matches */
    gfx_text_center(s->block_white ? "WHITE" : "BLACK", ARC_PF_X + s->w / 2, base_top + s->base_h / 2 - 28,
                    40, block_col);
    /* the big score numbers, tinted like the block */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text_center(buf, ARC_PF_X + s->w / 4, ARC_PF_Y + s->h / 3 - 40, 60, block_col);
    gfx_text_center(buf, ARC_PF_X + 3 * s->w / 4, ARC_PF_Y + s->h / 3 - 40, 60, block_col);
    /* the falling block: a thin outline keeps it readable on the grey panel */
    Rectangle b = {(float)ARC_PF_X + (float)s->w / 2 - BW_BLOCK / 2,
                   (float)ARC_PF_Y + s->block_y - BW_BLOCK / 2, BW_BLOCK, BW_BLOCK};
    DrawRectangleRec(b, block_col);
    DrawRectangleLinesEx(b, 2.0f, Fade(mono(1 - s->block_white), 0.35f));
    if (s->over)
        gfx_text_center("DIFFERENT COLOUR!", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2, 28, ARC_COL_RED);
}

static void draw_right_gutter(const bw_reflex_State *s) {
    char buf[64];
    gfx_text("BLOCKS MATCHED", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "fall speed x%.2f", s->speed / 4.0f);
    gfx_text(buf, 920, 160, 18, ARC_COL_DIM);
}

static void play_if_changed(int now, int *seen, Sound snd, bool audio) {
    if (now != *seen && audio)
        PlaySound(snd);
    *seen = now;
}

static GameResult play(const GameContext *ctx) {
    static bw_reflex_State s;
    bw_reflex_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound notes[8] = {0}, click = {0}, lose = {0};
    if (audio) {
        for (int i = 0; i < 8; i++)
            notes[i] = gfx_make_tone(k_scale[i], 0.25f, 0.5f);
        click = gfx_make_tone(700.0f, 0.04f, 0.3f);
        lose = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int flips_seen = 0, matches_seen = 0;
    bool lose_played = false;

    static const char *const help[] = {"A block falls from the top.", "Click (or press any key)",
                                       "to flip the base colour.",    "",
                                       "When the block lands the",    "colours must be the same.",
                                       "Every match is +1 and",       "the block falls faster."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        bw_reflex_step(&s, &in, ARC_DT);
        t += ARC_DT;
        play_if_changed(s.flips, &flips_seen, click, audio);
        if (s.matches != matches_seen && audio)
            PlaySound(notes[s.last_note]);
        matches_seen = s.matches;
        if (s.over && !lose_played) {
            lose_played = true;
            if (audio)
                PlaySound(lose);
        }
        BeginDrawing();
        gfx_backdrop(t);
        draw_playfield(&s);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("BW REFLEX", bw_reflex_score(&s), TextFormat("base: %s", s.base_white ? "white" : "black"));
        arcade_frame_end();
        if (bw_reflex_is_over(&s))
            break;
    }
    if (audio) {
        for (int i = 0; i < 8; i++)
            UnloadSound(notes[i]);
        UnloadSound(click);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = bw_reflex_score(&s);
    r.finished = bw_reflex_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_bw_reflex_def = {
    "bw_reflex",                                  /* id: equals the folder name        */
    "BW Reflex",                                  /* title                             */
    "Match the base colour to the falling block", /* tagline                           */
    "click or any key: flip the base colour   match the block when it lands", /* controls hint        */
    120,  /* menu order (lower = earlier)      */
    true, /* ranked: score goes to leaderboard */
    play};
