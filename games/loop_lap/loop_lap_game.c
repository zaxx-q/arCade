/* Loop Lap - raylib front-end: window loop and drawing ONLY. All rules live in loop_lap_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "loop_lap_logic.h"

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

static Vector2 to_screen(float x, float y) {
    Vector2 v = {(float)ARC_PF_X + x, (float)ARC_PF_Y + y};
    return v;
}

/* A pedal in a bottom corner; lit while its side is held. */
static void draw_pedal(const loop_lap_State *s, bool right, bool lit, const char *label) {
    float cx = right ? (float)s->w - 42.0f : 42.0f;
    float cy = (float)s->h - 57.0f;
    Vector2 p = to_screen(cx, cy);
    Rectangle r = {p.x - 30.0f, p.y - 45.0f, 60.0f, 90.0f};
    Color c = lit ? ARC_COL_CYAN : Fade(ARC_COL_CYAN, 0.28f);
    DrawRectangleRounded(r, 0.35f, 8, Fade(c, lit ? 0.55f : 0.25f));
    DrawRectangleRoundedLinesEx(r, 0.35f, 8, 2.0f, c);
    gfx_text_center(label, (int)p.x, (int)p.y - 10, 16, lit ? ARC_COL_TEXT : ARC_COL_DIM);
}

/* A small chequered flag at 3 o'clock of the player's circle: it flashes on every lap. */
static void draw_flag(const loop_lap_State *s) {
    float flash = s->flash_t < 0.5f ? 1.0f - s->flash_t / 0.5f : 0.0f;
    float alpha = 0.3f + 0.7f * flash;
    Vector2 p = to_screen((float)s->w - 28.0f - 16.0f, s->c1y - 16.0f);
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            bool dark = (row + col) % 2 == 0;
            Color c = dark ? Fade(ARC_COL_MAGENTA, alpha) : Fade(WHITE, alpha);
            DrawRectangle((int)p.x + col * 8, (int)p.y + row * 8, 8, 8, c);
        }
}

static void draw_world(const loop_lap_State *s) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.92f));
    Vector2 c1 = to_screen(s->c1x, s->c1y), c2 = to_screen(s->c2x, s->c2y);
    DrawRing(c1, s->c1r - 3.0f, s->c1r + 3.0f, 0.0f, 360.0f, 64, Fade(ARC_COL_MAGENTA, 0.9f));
    DrawRing(c2, s->c2r - 3.0f, s->c2r + 3.0f, 0.0f, 360.0f, 64, Fade(ARC_COL_MAGENTA, 0.9f));
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text_center(buf, ARC_PF_X + (int)s->c1x, ARC_PF_Y + (int)s->c1y - 40, 60,
                    Fade(ARC_COL_MAGENTA, 0.55f));
    draw_flag(s);
    for (int i = 0; i < s->enemy_n; i++) {
        Vector2 p = to_screen(s->enemy_pos[i].x, s->enemy_pos[i].y);
        gfx_glow_circle(p.x, p.y, LL_ENEMY_W / 2.0f, ARC_COL_MAGENTA);
    }
    Vector2 me = to_screen(s->my_pos.x, s->my_pos.y);
    gfx_glow_circle(me.x, me.y, s->my_w / 2.0f, ARC_COL_CYAN);
    draw_pedal(s, false, s->mode == LL_BRAKE, "BRAKE");
    draw_pedal(s, true, s->mode == LL_ACCEL, "GO");
    if (s->paused) {
        DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(BLACK, 0.55f));
        gfx_text_center("PAUSED", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 - 30, 40, ARC_COL_YELLOW);
        gfx_text_center("press P to resume", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 + 24, 18, ARC_COL_TEXT);
    }
    if (s->over)
        gfx_text_center("CRASH!", ARC_PF_X + s->w / 2, ARC_PF_Y + 16, 28, ARC_COL_RED);
}

static void draw_right_gutter(const loop_lap_State *s) {
    char buf[64];
    gfx_text("LAPS", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "enemies: %d / %d", s->enemy_n, LL_MAX_ENEMIES);
    gfx_text(buf, 920, 170, 18, ARC_COL_DIM);
    if (s->enemy_n < LL_MAX_ENEMIES) {
        snprintf(buf, sizeof(buf), "next enemy at lap %d", loop_lap_join_lap(s->enemy_n));
        gfx_text(buf, 920, 200, 18, ARC_COL_DIM);
    }
}

static GameResult play(const GameContext *ctx) {
    static loop_lap_State s;
    loop_lap_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound ding = {0}, join = {0}, whoosh = {0}, lose = {0};
    if (audio) {
        ding = gfx_make_tone(1000.0f, 0.15f, 0.45f);
        join = gfx_make_tone(330.0f, 0.25f, 0.45f);
        whoosh = gfx_make_tone(300.0f, 0.12f, 0.2f);
        lose = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int laps_seen = 0, joins_seen = 0, whooshes_seen = 0;
    bool lose_played = false;

    static const char *const help[] = {"Your ball runs round the",
                                       "top circle by itself.",
                                       "Hold to change its speed:",
                                       "left side / Left, A, S = brake",
                                       "right side / Right, D, W = go",
                                       "or hold Space for go.",
                                       "",
                                       "Each lap is +1. Enemies join",
                                       "on the lower circle. Don't touch."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        loop_lap_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.laps != laps_seen && audio)
            PlaySound(ding);
        laps_seen = s.laps;
        if (s.joins != joins_seen && audio)
            PlaySound(join);
        joins_seen = s.joins;
        if (s.whooshes != whooshes_seen && audio)
            PlaySound(whoosh);
        whooshes_seen = s.whooshes;
        if (s.over && !lose_played) {
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
        gfx_hud("LOOP LAP", loop_lap_score(&s), s.paused ? "paused" : "P: pause");
        arcade_frame_end();
        if (loop_lap_is_over(&s))
            break;
    }
    if (audio) {
        UnloadSound(ding);
        UnloadSound(join);
        UnloadSound(whoosh);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = loop_lap_score(&s);
    r.finished = loop_lap_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_loop_lap_def = {"loop_lap", /* id                */
                                   "Loop Lap", /* title             */
                                   "Race round the loop and slip past the enemy balls", /* tagline */
                                   "hold left mouse: left half brakes, right half accelerates   keys: "
                                   "Right/D/Up/Space accelerate, Left/A/Down/S brake   P: pause",
                                   150,  /* menu order */
                                   true, /* ranked            */
                                   play};
