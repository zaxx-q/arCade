/* Time Shooter - raylib front-end. All rules live in time_shooter_logic.c. */
#include "arcade/gfx.h"
#include "time_shooter_logic.h"

static void draw(const time_shooter_State *s, float t, Vec2 shake) {
    gfx_backdrop(t * s->time_scale); /* the grid freezes together with the world */
    for (int i = 0; i < TS_MAX_ENEMIES; i++)
        if (s->enemies[i].alive)
            gfx_glow_circle(s->enemies[i].pos.x + shake.x, s->enemies[i].pos.y + shake.y, 12.0f,
                            ARC_COL_MAGENTA);
    for (int b = 0; b < TS_MAX_BULLETS; b++) {
        const ts_Bullet *bl = &s->bullets[b];
        if (!bl->alive)
            continue;
        Vec2 tail = vec2_sub(bl->pos, vec2_scale(vec2_norm(bl->vel), 22.0f));
        gfx_glow_line(tail, bl->pos, 4.0f, ARC_COL_YELLOW);
    }
    gfx_glow_circle(s->player.x + shake.x, s->player.y + shake.y, 14.0f, ARC_COL_CYAN);
    /* time bar: full = normal time, tiny = frozen */
    DrawRectangle(16, ARC_SCREEN_H - 24, 204, 12, Fade(ARC_COL_PANEL, 0.9f));
    DrawRectangle(18, ARC_SCREEN_H - 22, (int)(200.0f * s->time_scale), 8,
                  s->time_scale > 0.5f ? ARC_COL_GREEN : ARC_COL_CYAN);
    gfx_text("TIME", 228, ARC_SCREEN_H - 26, 16, ARC_COL_DIM);
    if (s->time_scale < 0.5f)
        DrawRectangle(0, 36, ARC_SCREEN_W, ARC_SCREEN_H - 36, Fade(ARC_COL_CYAN, 0.04f));
}

static GameResult play(const GameContext *ctx) {
    static time_shooter_State s;
    static ParticleSystem fx;
    Shake shake = {0, 0, 0};
    Rng shake_rng;
    rng_seed(&shake_rng, ctx->seed + 99u);
    time_shooter_init(&s, ctx->seed);
    fx_init(&fx, ctx->seed + 1u);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    int kills_seen = 0;
    bool death_burst = false;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        time_shooter_step(&s, &in, ARC_DT);
        if (s.kills != kills_seen) { /* a kill happened this frame: juice! */
            kills_seen = s.kills;
            fx_burst(&fx, s.last_kill.x, s.last_kill.y, 24, 260.0f, 0.6f, 3.0f, 255, 64, 200);
            shake_start(&shake, 6.0f, 0.15f);
        }
        if (s.over && !death_burst) {
            death_burst = true;
            fx_burst(&fx, s.player.x, s.player.y, 40, 320.0f, 0.8f, 4.0f, 0, 229, 255);
        }
        fx_update(&fx, ARC_DT);
        shake_update(&shake, ARC_DT);
        t += ARC_DT;
        Vec2 off = shake_offset(&shake, &shake_rng);
        BeginDrawing();
        draw(&s, t, off);
        gfx_draw_particles(&fx, off);
        gfx_hud("TIME SHOOTER", time_shooter_score(&s), TextFormat("kills %d", s.kills));
        arcade_frame_end();
        if (time_shooter_is_over(&s))
            break;
    }
    GameResult r;
    r.score = time_shooter_score(&s);
    r.finished = time_shooter_is_over(&s);
    return r;
}

const GameDef game_time_shooter_def = {
    "time_shooter",
    "Time Shooter",
    "Time only moves when you move",
    "WASD: move (time runs)   mouse: aim   left click: shoot   stand still = frozen world",
    10,
    true,
    play};
