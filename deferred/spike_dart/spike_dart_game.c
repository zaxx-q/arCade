/* Spike Dart - raylib front-end: window loop and drawing ONLY. All rules live in spike_dart_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "spike_dart_logic.h"

/* ---- shared look: playfield panel, frame, rules in the left gutter ---- */
static void draw_help(const char *title, const char *const *lines, int n) {
    gfx_text(title, 28, 64, 22, ARC_COL_CYAN);
    for (int i = 0; i < n; i++)
        gfx_text(lines[i], 28, 104 + i * 30, 18, ARC_COL_DIM);
}

static void draw_panel(void) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.92f));
}

static void draw_frame(void) {
    Rectangle r = {(float)ARC_PF_X, (float)ARC_PF_Y, (float)ARC_PF_W, (float)ARC_PF_H};
    DrawRectangleLinesEx(r, 2.0f, Fade(ARC_COL_CYAN, 0.5f));
}

static Vector2 to_screen(float x, float y) {
    Vector2 v = {(float)ARC_PF_X + x, (float)ARC_PF_Y + y};
    return v;
}

/* A spike is a thin triangle lying on a line toward the target centre; `a` is its orbit angle. */
static void draw_spike(float cx, float cy, float a, Color c) {
    float dx = cosf(a), dy = -sinf(a); /* unit vector from the spike's centre toward the target */
    float hl = SD_SPIKE_LEN / 2.0f, hw = SD_SPIKE_W / 2.0f;
    Vector2 tip = to_screen(cx + dx * hl, cy + dy * hl);
    Vector2 b1 = to_screen(cx - dx * hl - dy * hw, cy - dy * hl + dx * hw);
    Vector2 b2 = to_screen(cx - dx * hl + dy * hw, cy - dy * hl - dx * hw);
    DrawTriangle(tip, b1, b2, c); /* raylib culls the wrong winding, so draw both orders */
    DrawTriangle(tip, b2, b1, c);
}

static void draw_world(const spike_dart_State *s, float t) {
    Vector2 tc = to_screen(s->target_x, s->target_y);
    DrawCircleV(tc, SD_TARGET_R, ColorBrightness(ARC_COL_PANEL, 0.15f));
    DrawRing(tc, SD_TARGET_R - 4.0f, SD_TARGET_R, 0.0f, 360.0f, 64, ARC_COL_MAGENTA);
    /* marks that turn with the target (same speed as the orbit: a quarter turn per second) */
    for (int i = 0; i < 6; i++) {
        float a = t * (ARC_PI / 2.0f) + (float)i * ARC_TAU / 6.0f;
        Vector2 m = {tc.x + cosf(a) * 70.0f, tc.y - sinf(a) * 70.0f};
        DrawCircleV(m, 6.0f, Fade(ARC_COL_MAGENTA, 0.45f));
    }
    for (int i = 0; i < s->stuck; i++)
        draw_spike(s->px[i], s->py[i], s->rot[i], ARC_COL_CYAN);
    /* the spike in hand: yellow while waiting/flying, red once it has hit the lane */
    draw_spike(s->spike_x, s->cur_y, ARC_PI / 2.0f, s->over && !s->capped ? ARC_COL_RED : ARC_COL_YELLOW);
    if (s->over) {
        const char *msg = s->capped ? "ALL 64 STUCK!" : "SPIKE HIT!";
        gfx_text_center(msg, ARC_PF_X + s->w / 2, ARC_PF_Y + 40, 28, s->capped ? ARC_COL_GREEN : ARC_COL_RED);
    } else if (!s->flying) {
        gfx_text_center("click / any key", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h - 36, 16, ARC_COL_DIM);
    }
}

static void draw_right_gutter(const spike_dart_State *s) {
    char buf[64];
    gfx_text("SPIKES STUCK", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d / %d", s->stuck, SD_MAX_SPIKES);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    gfx_text("danger: the lane below", 920, 170, 18, ARC_COL_DIM);
    gfx_text("the target (centre column)", 920, 200, 18, ARC_COL_DIM);
}

static GameResult play(const GameContext *ctx) {
    static spike_dart_State s;
    spike_dart_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound throw_snd = {0}, stick_snd = {0}, lose_snd = {0};
    if (audio) {
        throw_snd = gfx_make_tone(880.0f, 0.06f, 0.35f);
        stick_snd = gfx_make_tone(330.0f, 0.10f, 0.5f);
        lose_snd = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int throws_seen = 0, sticks_seen = 0;
    bool lose_played = false;

    static const char *const help[] = {
        "Throw spikes at the target.", "Click, Space or any key.",  "Stuck spikes orbit around it.", "",
        "Never throw while a stuck",   "spike is passing the lane", "straight below the target."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        spike_dart_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.throws != throws_seen && audio)
            PlaySound(throw_snd);
        throws_seen = s.throws;
        if (s.sticks != sticks_seen && audio)
            PlaySound(stick_snd);
        sticks_seen = s.sticks;
        if (s.over && !s.capped && !lose_played) {
            lose_played = true;
            if (audio)
                PlaySound(lose_snd);
        }
        BeginDrawing();
        gfx_backdrop(t);
        draw_panel();
        draw_world(&s, t);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("SPIKE DART", spike_dart_score(&s), TextFormat("max %d", SD_MAX_SPIKES));
        arcade_frame_end();
        if (spike_dart_is_over(&s))
            break;
    }
    if (audio) {
        UnloadSound(throw_snd);
        UnloadSound(stick_snd);
        UnloadSound(lose_snd);
    }
    GameResult r;
    r.score = spike_dart_score(&s);
    r.finished = spike_dart_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_spike_dart_def = {
    "spike_dart",                                                   /* id: equals the folder name        */
    "Spike Dart",                                                   /* title                             */
    "Stick spikes into the spinning target, never into each other", /* tagline                           */
    "click, Space or any key: throw a spike   don't hit a spike passing the bottom", /* controls hint       */
    140,  /* menu order (lower = earlier)      */
    true, /* ranked: score goes to leaderboard */
    play};
