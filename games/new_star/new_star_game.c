/* New Star - raylib front-end: window loop and drawing ONLY. All rules live in new_star_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/playfield.h"
#include "new_star_logic.h"

#define OVERLAY_COLOR ((Color){10, 10, 30, 255})

/* Shape and colour of each of the six star types. */
typedef struct {
    int points;
    float inner; /* inner radius as a fraction of the outer one */
    Color color;
} StarLook;

static const StarLook k_look[NS_TYPES] = {
    {5, 0.45f, {255, 214, 10, 255}}, {6, 0.55f, {0, 229, 255, 255}},  {5, 0.40f, {255, 64, 200, 255}},
    {7, 0.55f, {80, 255, 160, 255}}, {4, 0.45f, {255, 150, 50, 255}}, {8, 0.70f, {170, 120, 255, 255}},
};

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

/* A star: a fan of triangles from the centre to an alternating outer/inner polygon, turned by rot
 * (clockwise). */
static void draw_star_shape(Vector2 c, float size, float rot_deg, const StarLook *look, Color fill) {
    int n = look->points * 2;
    float outer = size / 2.0f, inner = outer * look->inner;
    float a0 = rot_deg * (ARC_PI / 180.0f) - ARC_PI / 2.0f;
    Vector2 prev = {0.0f, 0.0f}, first = {0.0f, 0.0f};
    for (int k = 0; k <= n; k++) {
        float r = k % 2 == 0 ? outer : inner;
        float a = a0 + (float)k * ARC_TAU / (float)n;
        Vector2 v = {c.x + cosf(a) * r, c.y + sinf(a) * r};
        if (k == 0)
            first = v;
        else {
            DrawTriangle(c, prev, v, fill); /* raylib culls the wrong winding, so draw both orders */
            DrawTriangle(c, v, prev, fill);
            DrawLineEx(prev, v, 1.5f, Fade(BLACK, 0.35f));
        }
        prev = v;
    }
    (void)first;
}

/* A little face per type; faces never rotate. */
static void draw_face(Vector2 c, float size, int type) {
    float re = size * 0.06f + 1.5f;
    Color ink = Fade(BLACK, 0.85f);
    Vector2 le = {c.x - size * 0.14f, c.y - size * 0.08f}, rg = {c.x + size * 0.14f, c.y - size * 0.08f};
    if (type == 3) { /* big round eyes */
        DrawRing(le, re, re + 2.0f, 0.0f, 360.0f, 16, ink);
        DrawRing(rg, re, re + 2.0f, 0.0f, 360.0f, 16, ink);
    } else {
        DrawCircleV(le, re, ink);
        DrawCircleV(rg, re, ink);
    }
    float mw = size * 0.16f, my = c.y + size * 0.14f;
    switch (type) {
    case 1: /* surprised "o" */
        DrawRing((Vector2){c.x, my + 1.0f}, size * 0.05f, size * 0.05f + 2.0f, 0.0f, 360.0f, 16, ink);
        break;
    case 2: /* flat mouth */
        DrawLineEx((Vector2){c.x - mw, my}, (Vector2){c.x + mw, my}, 2.0f, ink);
        break;
    case 4: /* frown */
        for (int i = 0; i < 4; i++) {
            float t0 = -1.0f + (float)i * 0.5f, t1 = t0 + 0.5f;
            DrawLineEx((Vector2){c.x + t0 * mw, my - 3.0f * (1.0f - t0 * t0)},
                       (Vector2){c.x + t1 * mw, my - 3.0f * (1.0f - t1 * t1)}, 2.0f, ink);
        }
        break;
    default: /* smile (wider for 3 and 5) */
    {
        float w = type == 0 ? mw : mw * 1.3f;
        for (int i = 0; i < 4; i++) {
            float t0 = -1.0f + (float)i * 0.5f, t1 = t0 + 0.5f;
            DrawLineEx((Vector2){c.x + t0 * w, my + 3.0f * (1.0f - t0 * t0)},
                       (Vector2){c.x + t1 * w, my + 3.0f * (1.0f - t1 * t1)}, 2.0f, ink);
        }
    } break;
    }
}

static void draw_world(const new_star_State *s) {
    DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(ARC_COL_PANEL, 0.92f));
    BeginScissorMode(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text_center(buf, ARC_PF_X + (int)s->score_pos.x, ARC_PF_Y + (int)s->score_pos.y - 40, 60,
                    Fade(ARC_COL_DIM, 0.6f));
    for (int i = 0; i < s->n; i++) {
        const StarLook *look = &k_look[s->type[i]];
        Vector2 c = {(float)ARC_PF_X + s->x[i] + s->shake[i], (float)ARC_PF_Y + s->y[i]};
        Color fill = look->color;
        if (s->over && i == s->score) { /* the star you should have tapped blinks black 0.3 s, white 0.7 s */
            fill = fmodf(s->over_t, 1.0f) < 0.3f ? (Color){20, 20, 20, 255} : WHITE;
        }
        draw_star_shape(c, s->size[i], s->rot[i], look, fill);
        draw_face(c, s->size[i], s->type[i]);
    }
    if (s->overlay) {
        float alpha = new_star_overlay_alpha(s->phase_t);
        DrawRectangle(ARC_PF_X, ARC_PF_Y, ARC_PF_W, ARC_PF_H, Fade(OVERLAY_COLOR, alpha));
    }
    if (s->over)
        gfx_text_center(s->capped ? "ALL 512 FOUND!" : (s->timed_out ? "TIME UP!" : "WRONG STAR!"),
                        ARC_PF_X + s->w / 2, ARC_PF_Y + s->h - 44, 28,
                        s->capped ? ARC_COL_GREEN : ARC_COL_RED);
    else if (s->score == 0)
        gfx_text_center("tap the star", ARC_PF_X + s->w / 2, ARC_PF_Y + s->h / 2 + 70, 18, ARC_COL_TEXT);
    else if (!s->overlay && s->n >= NS_TIMED_FROM) { /* a bar that runs out from the 10th star on */
        float frac = arc_clampf(1.0f - s->new_t / NS_LIMIT_TIME, 0.0f, 1.0f);
        Color bar = s->alarm ? ARC_COL_RED : ARC_COL_GREEN;
        DrawRectangle(ARC_PF_X + 16, ARC_PF_Y + s->h - 24, (int)((float)(s->w - 32) * frac), 10, bar);
    }
    EndScissorMode();
}

static void draw_right_gutter(const new_star_State *s) {
    char buf[64];
    gfx_text("NEW STARS FOUND", 920, 64, 22, ARC_COL_CYAN);
    snprintf(buf, sizeof(buf), "%d", s->score);
    gfx_text(buf, 920, 100, 40, ARC_COL_YELLOW);
    snprintf(buf, sizeof(buf), "stars on screen: %d", s->n);
    gfx_text(buf, 920, 170, 18, ARC_COL_DIM);
    if (s->n >= NS_TIMED_FROM) {
        float left = NS_LIMIT_TIME - s->new_t;
        snprintf(buf, sizeof(buf), "time left: %.1f s", left > 0.0f ? left : 0.0f);
        gfx_text(buf, 920, 200, 18, s->alarm ? ARC_COL_RED : ARC_COL_DIM);
    } else {
        gfx_text("timer starts at star 10", 920, 200, 18, ARC_COL_DIM);
    }
}

static GameResult play(const GameContext *ctx) {
    static new_star_State s; /* ~15 KB of star arrays: static keeps it off the stack */
    new_star_init(&s, ctx->seed);

    bool audio = !ctx->headless && arcade_audio_ready();
    Sound ok = {0}, pop = {0}, alarm = {0}, lose = {0};
    if (audio) {
        ok = gfx_make_tone(660.0f, 0.12f, 0.4f);
        pop = gfx_make_tone(520.0f, 0.08f, 0.25f);
        alarm = gfx_make_tone(880.0f, 0.12f, 0.35f);
        lose = gfx_make_tone(110.0f, 0.6f, 0.6f);
    }
    int corrects_seen = 0, created_seen = 1;
    float alarm_cd = 0.0f;
    bool lose_played = false;

    static const char *const help[] = {"One star is on screen.",
                                       "Tap it. The screen goes dark",
                                       "and a NEW star appears.",
                                       "Tap the newest star each",
                                       "round. Tapping an old star",
                                       "ends the run.",
                                       "",
                                       "From the 10th star on you",
                                       "have 15 s per round."};
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        new_star_step(&s, &in, ARC_DT);
        t += ARC_DT;
        if (s.corrects != corrects_seen && audio)
            PlaySound(ok);
        corrects_seen = s.corrects;
        if (s.created != created_seen && audio)
            PlaySound(pop);
        created_seen = s.created;
        alarm_cd -= ARC_DT;
        if (s.alarm && !s.over && alarm_cd <= 0.0f) {
            alarm_cd = 0.5f; /* the original plays an alarm loop: a short beep repeated here */
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
        draw_world(&s);
        draw_frame();
        draw_help("HOW TO PLAY", help, (int)ARC_ARRAY_LEN(help));
        draw_right_gutter(&s);
        gfx_hud("NEW STAR", new_star_score(&s), TextFormat("stars: %d", s.n));
        arcade_frame_end();
        if (new_star_is_over(&s))
            break;
    }
    if (audio) {
        UnloadSound(ok);
        UnloadSound(pop);
        UnloadSound(alarm);
        UnloadSound(lose);
    }
    GameResult r;
    r.score = new_star_score(&s);
    r.finished = new_star_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_new_star_def = {"new_star",                         /* id                */
                                   "New Star",                         /* title             */
                                   "Spot the star that just appeared", /* tagline           */
                                   "click the newest star each round   tapping an old star ends the run   "
                                   "from star 10 you have 15 s per round",
                                   170,  /* menu order        */
                                   true, /* ranked            */
                                   play};
