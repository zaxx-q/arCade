/* Absorb and Grow - raylib front-end. All rules live in absorb_grow_logic.c. */
#include "absorb_grow_logic.h"
#include "arcade/gfx.h"

static void draw(const absorb_grow_State *s, float t) {
    gfx_backdrop(t * 0.5f);
    for (int i = 0; i < AG_MAX_BLOBS; i++) {
        const ag_Blob *b = &s->blobs[i];
        Color c =
            b->r < s->r / 1.1f
                ? ARC_COL_GREEN
                : (b->r > s->r * 1.1f ? ARC_COL_RED : ARC_COL_YELLOW); /* edible / dangerous / similar */
        DrawCircleV((Vector2){b->pos.x, b->pos.y}, b->r, Fade(c, 0.35f));
        DrawCircleLines((int)b->pos.x, (int)b->pos.y, b->r, c);
    }
    gfx_glow_circle(s->pos.x, s->pos.y, s->r, ARC_COL_CYAN);
    float k = arc_clampf(s->r / AG_WIN_R, 0.0f, 1.0f); /* progress bar toward the win size */
    DrawRectangle(16, ARC_SCREEN_H - 24, 304, 12, Fade(ARC_COL_PANEL, 0.9f));
    DrawRectangle(18, ARC_SCREEN_H - 22, (int)(300.0f * k), 8, ARC_COL_CYAN);
    DrawText("SIZE", 328, ARC_SCREEN_H - 26, 16, ARC_COL_DIM);
}

static GameResult play(const GameContext *ctx) {
    static absorb_grow_State s;
    static ParticleSystem fx;
    absorb_grow_init(&s, ctx->seed);
    fx_init(&fx, ctx->seed + 5u);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    int events_seen = 0;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        absorb_grow_step(&s, &in, ARC_DT);
        if (s.absorb_events != events_seen) {
            events_seen = s.absorb_events;
            fx_burst(&fx, s.last_absorb.x, s.last_absorb.y, 18, 220.0f, 0.5f, 3.0f, 80, 255, 160);
        }
        fx_update(&fx, ARC_DT);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_draw_particles(&fx, vec2(0.0f, 0.0f));
        gfx_hud("ABSORB AND GROW", absorb_grow_score(&s),
                TextFormat("%.0fs   eaten %d", s.time_left, s.absorbed));
        arcade_frame_end();
        if (absorb_grow_is_over(&s))
            break;
    }
    GameResult r;
    r.score = absorb_grow_score(&s);
    r.finished = absorb_grow_is_over(&s);
    return r;
}

const GameDef game_absorb_grow_def = {
    "absorb_grow",
    "Absorb and Grow",
    "Swallow the small, avoid the big, keep growing",
    "WASD: move   green = edible   red = dangerous   reach size 150 or survive 90 s",
    60,
    true,
    play};
