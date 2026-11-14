/* Master Juggler - raylib front-end: window loop and drawing ONLY. All rules live in
 * master_juggler_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "master_juggler_logic.h"

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

static Color stage_color(int stage) {
    switch (stage) {
    case MJ_STAGE_YELLOW:
        return ARC_COL_YELLOW;
    case MJ_STAGE_GREEN:
        return ARC_COL_GREEN;
    case MJ_STAGE_GREY:
        return ARC_COL_DIM;
    default:
        return ARC_COL_RED;
    }
}

static void draw_world(const master_juggler_State *s) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.92f));
    /* the floor strip: a ball that reaches it is lost */
    DrawRectangle(ARC_PF_X, ARC_PF_Y + s->h - (int)s->base_h, s->w, (int)s->base_h, Fade(ARC_COL_DIM, 0.25f));
    DrawRectangle(ARC_PF_X, ARC_PF_Y + s->h - (int)s->base_h, s->w, 2, Fade(ARC_COL_RED, 0.6f));
    /* balls first: they start hidden behind the bar so they seem to shoot out of it */
    for (int i = 0; i < s->nb; i++) {
        if (!s->visible[i])
            continue;
        int stage = s->first_lost ? MJ_STAGE_GREY : master_juggler_stage(s->bounces[i], s->level);
        Color c = Fade(stage_color(stage), arc_clampf(s->alpha[i], 0.0f, 1.0f));
        gfx_glow_circle((float)ARC_PF_X + s->x[i], (float)ARC_PF_Y + s->y[i], MJ_BALL_R * 0.8f, c);
    }
    /* trampoline: bar, and a spring on top that squeezes for a moment after a bounce */
    float bar_h = (float)s->h / 25.0f;
    float spring_full = (float)s->h / 50.0f;
    float squeeze = 10.0f * arc_clampf(1.0f - s->since_bounce / 0.12f, 0.0f, 1.0f);
    float spring_h = spring_full - squeeze;
    float bx = (float)ARC_PF_X + s->bar_x - MJ_BAR_W / 2.0f;
    float by = (float)ARC_PF_Y + s->bar_y;
    DrawRectangle((int)bx + 8, (int)(by - spring_h), (int)MJ_BAR_W - 16, (int)spring_h,
                  Fade(ARC_COL_MAGENTA, 0.8f));
    DrawRectangleRounded((Rectangle){bx, by, MJ_BAR_W, bar_h}, 0.4f, 6, ARC_COL_CYAN);
    if (s->waiting && !s->over) {
        char buf[48];
        DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(BLACK, 0.45f));
        snprintf(buf, sizeof(buf), "LEVEL %d", s->level);
        gfx_text_center(buf, ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 4 - 24, 40, ARC_COL_YELLOW);
        snprintf(buf, sizeof(buf), "%d ball%s, %d bounces each", s->level, s->level == 1 ? "" : "s",
                 3 * (s->level < 3 ? s->level : 3));
        gfx_text_center(buf, ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 4 + 36, 18, ARC_COL_TEXT);
        gfx_text_center("click or Space to start", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 - 10, 22,
                        ARC_COL_TEXT);
        if (s->lives < MJ_LIVES) {
            gfx_text_center("a ball was lost: try again", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 + 30, 18,
                            ARC_COL_RED);
        }
    }
    if (s->over)
        gfx_text_center(s->capped ? "ALL 32 LEVELS DONE!" : "OUT OF LIVES", ARC_PF_X + s->w / 2,
                        ARC_PF_Y + 30, 28, s->capped ? ARC_COL_GREEN : ARC_COL_RED);
}

static void draw_right_gutter(const master_juggler_State *s) {
    char buf[64];
    gfx_text("BEST LEVEL DONE", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->best);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "playing level %d", s->level);
    gfx_text(buf, 920, 170, 18, ARC_COL_DIM);
    gfx_text("LIVES", 920, 220, 22, ARC_COL_CYAN);
    for (int i = 0; i < MJ_LIVES; i++) {
        Vector2 c = {932.0f + (float)i * 40.0f, 276.0f};
        if (i < s->lives)
            DrawCircleV(c, 12.0f, ARC_COL_RED);
        else
            DrawRing(c, 10.0f, 12.0f, 0.0f, 360.0f, 24, Fade(ARC_COL_DIM, 0.6f));
    }
}

static GameResult play(const GameContext *ctx) {
    static master_juggler_State s;
    master_juggler_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound throw_snd = {0}, bump = {0}, lose = {0}, win = {0}, life = {0};
    if (audio) {
        throw_snd = gfx_make_tone(500.0f, 0.06f, 0.3f);
        bump = gfx_make_tone(350.0f, 0.06f, 0.4f);
        lose = gfx_make_tone(130.0f, 0.5f, 0.5f);
        win = gfx_make_tone(880.0f, 0.35f, 0.5f);
        life = gfx_make_tone(180.0f, 0.6f, 0.6f);
    }
    int throws_seen = 0, hits_seen = 0, losses_seen = 0, done_seen = 0, tries_seen = 0;

    static const char *const help[] = {"Move the mouse: the bar",
                                       "follows it (or use A/D).",
                                       "Balls fly out of the bar.",
                                       "Bounce every ball 3 x min(",
                                       "level, 3) times. It turns",
                                       "red, yellow, green, grey.",
                                       "",
                                       "A ball that falls = try lost.",
                                       "3 lives. Score = best level."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        master_juggler_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.throws != throws_seen && audio)
            PlaySound(throw_snd);
        throws_seen = s.throws;
        if (s.hits != hits_seen && audio)
            PlaySound(bump);
        hits_seen = s.hits;
        if (s.losses != losses_seen && audio)
            PlaySound(lose);
        losses_seen = s.losses;
        if (s.levels_done != done_seen && audio)
            PlaySound(win);
        done_seen = s.levels_done;
        if (s.tries_lost != tries_seen && audio)
            PlaySound(life);
        tries_seen = s.tries_lost;
        BeginDrawing();
        gfx_backdrop(t);
        draw_world(&s);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("MASTER JUGGLER", master_juggler_score(&s),
                TextFormat("level %d  lives %d", s.level, s.lives));
        arcade_frame_end();
        if (master_juggler_is_over(&s))
            break;
    }
    if (audio) {
        UnloadSound(throw_snd);
        UnloadSound(bump);
        UnloadSound(lose);
        UnloadSound(win);
        UnloadSound(life);
    }
    GameResult r;
    r.score = master_juggler_score(&s);
    r.finished = master_juggler_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_master_juggler_def = {
    "master_juggler",                             /* id                */
    "Master Juggler",                             /* title             */
    "Keep every ball bouncing on the trampoline", /* tagline           */
    "mouse: move the trampoline   click or Space: start a level   A/D or arrows: move by keyboard   3 lives",
    110,  /* menu order        */
    true, /* ranked            */
    play};
