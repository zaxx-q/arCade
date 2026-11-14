/* Post It - raylib front-end: window loop and drawing ONLY. All rules live in post_it_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "post_it_logic.h"

#define ALARM_PERIOD 0.35f /* seconds between alarm beeps while a virus is near */

/* C-major scale for the grab notes (Hz). */
static const float k_scale[8] = {262, 294, 330, 349, 392, 440, 494, 523};

/* The 8 post-it colours (index = state.gcol / bcol); the head is drawn yellow. */
static const Color k_postit[PI_COLORS] = {{255, 64, 200, 255},  {0, 229, 255, 255},   {80, 255, 160, 255},
                                          {255, 150, 50, 255},  {170, 120, 255, 255}, {190, 255, 90, 255},
                                          {255, 130, 170, 255}, {120, 190, 255, 255}};
static const Color k_head = {255, 230, 100, 255};

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

static void draw_postit(float x, float y, float rot_deg, Color c) {
    Vector2 p = to_screen(x, y);
    Rectangle r = {p.x, p.y, 24.0f, 24.0f};
    Vector2 origin = {12.0f, 12.0f};
    DrawRectanglePro(r, origin, rot_deg, c);
    Rectangle inner = {p.x, p.y, 14.0f, 14.0f};
    Vector2 inner_origin = {7.0f, 7.0f};
    DrawRectanglePro(inner, inner_origin, rot_deg, ColorBrightness(c, 0.3f)); /* a lighter sticky strip */
}

/* A virus is a disc with 8 spikes; it blinks red/white while it is near the cluster. */
static void draw_virus(float x, float y, bool near_cluster, float t) {
    Vector2 p = to_screen(x, y);
    bool white = near_cluster && ((int)(t / 0.3f) % 2 == 1);
    Color c = white ? WHITE : ARC_COL_RED;
    for (int i = 0; i < 8; i++) {
        float a = (float)i * ARC_TAU / 8.0f + t * 0.8f;
        Vector2 tip = {p.x + cosf(a) * 13.0f, p.y + sinf(a) * 13.0f};
        DrawLineEx(p, tip, 3.0f, c);
    }
    DrawCircleV(p, 9.0f, c);
}

static void draw_world(const post_it_State *s, float t) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.95f));
    /* items wrap up to ~28 px outside the playfield: clip them so they never bleed into the gutters */
    BeginScissorMode(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->blockn);
    gfx_text_center(buf, ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 4 - 40, 60, Fade(ARC_COL_DIM, 0.45f));
    for (int i = 1; i < s->blockn; i++)
        draw_postit(s->bx[i], s->by[i], s->brot[i], k_postit[s->bcol[i]]);
    for (int i = 0; i < PI_GREENS; i++)
        draw_postit(s->gx[i], s->gy[i], s->grot[i], k_postit[s->gcol[i]]);
    for (int i = 0; i < PI_REDS; i++)
        draw_virus(s->rx[i], s->ry[i], s->rnear[i], t);
    /* the head pulses while it is alone so the player can find it */
    bool dim = s->blockn == 1 && ((int)(t / 0.3f) % 2 == 0);
    draw_postit(s->bx[0], s->by[0], 0.0f, dim ? Fade(k_head, 0.35f) : k_head);
    if (s->over && s->killer >= 0) {
        Vector2 p = to_screen(s->rx[s->killer], s->ry[s->killer]);
        DrawRing(p, 24.0f, 28.0f, 0.0f, 360.0f, 32, ARC_COL_RED);
    }
    if (s->paused) {
        DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(BLACK, 0.55f));
        gfx_text_center("PAUSED", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 - 40, 40, ARC_COL_YELLOW);
        gfx_text_center("click or P to resume", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 + 20, 18,
                        ARC_COL_TEXT);
    }
    if (s->over)
        gfx_text_center(s->capped ? "CLUSTER FULL!" : "VIRUS!", ARC_PF_X + s->w / 2, ARC_PF_Y + 24, 28,
                        s->capped ? ARC_COL_GREEN : ARC_COL_RED);
    EndScissorMode();
}

/* A small gauge: the dot shows the current tilt, the ring is the maximum (+-0.5 g). */
static void draw_tilt_gauge(const post_it_State *s) {
    Vector2 c = {1040.0f, 330.0f};
    DrawRing(c, 58.0f, 60.0f, 0.0f, 360.0f, 48, Fade(ARC_COL_CYAN, 0.6f));
    DrawLineEx((Vector2){c.x - 60.0f, c.y}, (Vector2){c.x + 60.0f, c.y}, 1.0f, Fade(ARC_COL_DIM, 0.4f));
    DrawLineEx((Vector2){c.x, c.y - 60.0f}, (Vector2){c.x, c.y + 60.0f}, 1.0f, Fade(ARC_COL_DIM, 0.4f));
    float gx = arc_clampf(s->tilt_x, -0.5f, 0.5f) / 0.5f * 56.0f;
    float gy = -arc_clampf(s->tilt_y, -0.5f, 0.5f) / 0.5f * 56.0f; /* screen y is down */
    DrawCircleV((Vector2){c.x + gx, c.y + gy}, 6.0f, ARC_COL_YELLOW);
}

static void draw_right_gutter(const post_it_State *s) {
    char buf[64];
    gfx_text("CLUSTER SIZE", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d / %d", s->blockn, PI_MAX_BLOCKS);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "viruses near: %d", s->near_count);
    gfx_text(buf, 920, 170, 18, s->near_count > 0 ? ARC_COL_RED : ARC_COL_DIM);
    gfx_text("TILT (pointer offset)", 920, 230, 18, ARC_COL_DIM);
    draw_tilt_gauge(s);
}

static GameResult play(const GameContext *ctx) {
    static post_it_State s;
    post_it_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound notes[8] = {0}, alarm = {0}, lose = {0};
    if (audio) {
        for (int i = 0; i < 8; i++)
            notes[i] = gfx_make_tone(k_scale[i], 0.2f, 0.5f);
        alarm = gfx_make_tone(900.0f, 0.1f, 0.3f);
        lose = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int grabs_seen = 0;
    float alarm_cd = 0.0f;
    bool lose_played = false;

    static const char *const help[] = {
        "Move the mouse away from", "the centre to tilt the world", "(or use arrows / WASD).",      "",
        "Glue post-its to your",    "cluster: +1 each. Viruses",    "(red) end the game on touch.", "",
        "Click or P: pause."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        post_it_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.grabs != grabs_seen && audio)
            PlaySound(notes[s.last_note]);
        grabs_seen = s.grabs;
        alarm_cd -= ARC_DT;
        if (s.near_count > 0 && !s.paused && !s.over && alarm_cd <= 0.0f) {
            alarm_cd = ALARM_PERIOD; /* the original loops an alarm: a short beep repeated here */
            if (audio)
                PlaySound(alarm);
        }
        if (s.over && !lose_played) {
            lose_played = true;
            if (audio)
                PlaySound(lose);
        }
        BeginDrawing();
        gfx_backdrop(t);
        draw_world(&s, t);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("POST IT", post_it_score(&s), s.paused ? "paused" : "P: pause");
        arcade_frame_end();
        if (post_it_is_over(&s))
            break;
    }
    if (audio) {
        for (int i = 0; i < 8; i++)
            UnloadSound(notes[i]);
        UnloadSound(alarm);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = post_it_score(&s);
    r.finished = post_it_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_post_it_def = {
    "post_it",                                         /* id                */
    "Post It",                                         /* title             */
    "Tilt the world, glue on post-its, dodge viruses", /* tagline           */
    "mouse: tilt (pointer offset from centre)   arrows/WASD: tilt   click or P: pause",
    100,  /* menu order        */
    true, /* ranked            */
    play};
