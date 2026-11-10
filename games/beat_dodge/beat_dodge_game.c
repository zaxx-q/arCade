/* Beat Dodge - raylib front-end. All rules live in beat_dodge_logic.c. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "beat_dodge_logic.h"

static float lane_x(int lane) {
    return ARC_SCREEN_W / 2.0f + ((float)lane - 1.0f) * BD_LANE_SPACING;
}

static void draw(const beat_dodge_State *s, float t) {
    gfx_backdrop(t);
    float phase = s->spb > 0.0f ? (s->time / s->spb) - (float)s->last_beat : 0.0f; /* 0..1 within the beat */
    for (int l = 0; l < BD_LANES; l++) {
        float x = lane_x(l);
        DrawRectangle((int)(x - 80), (int)BD_TOP_Y, 160, (int)(BD_PLAYER_Y - BD_TOP_Y + 60),
                      Fade(ARC_COL_PANEL, 0.55f));
        DrawCircleLines((int)x, (int)BD_PLAYER_Y, 34.0f + 26.0f * (1.0f - phase),
                        Fade(ARC_COL_CYAN, 0.65f * (1.0f - phase))); /* beat pulse */
    }
    for (int i = s->next_hit; i < s->map.length; i++) {
        float y = beat_dodge_obstacle_y(s, i);
        if (y < BD_TOP_Y)
            break; /* later beats have not spawned yet */
        if (s->map.lane[i] >= 0)
            gfx_glow_circle(lane_x(s->map.lane[i]), y, 22.0f, ARC_COL_MAGENTA);
    }
    float px = lane_x(s->lane);
    DrawTriangle((Vector2){px, BD_PLAYER_Y - 26}, (Vector2){px - 22, BD_PLAYER_Y + 20},
                 (Vector2){px + 22, BD_PLAYER_Y + 20}, ARC_COL_CYAN);
    if (s->hit_flash > 0.0f)
        DrawRectangle(0, 36, ARC_SCREEN_W, ARC_SCREEN_H - 36, Fade(ARC_COL_RED, s->hit_flash));
    for (int h = 0; h < s->lives; h++)
        DrawCircle(40 + h * 34, 70, 11, ARC_COL_RED);
    if (s->combo > 1)
        gfx_text_center(TextFormat("combo x%d", s->combo), ARC_SCREEN_W / 2, ARC_SCREEN_H - 50, 26,
                        ARC_COL_YELLOW);
}

static GameResult play(const GameContext *ctx) {
    static beat_dodge_State s;
    beat_dodge_init(&s, ctx->seed);

    /* optional: a hand-made beat map from assets/beatmaps/default.txt overrides the generated one */
    char path[600];
    snprintf(path, sizeof(path), "%s/beatmaps/default.txt", ctx->assets_dir ? ctx->assets_dir : "assets");
    bd_BeatMap loaded;
    if (!ctx->headless && bd_beatmap_load(path, &loaded))
        beat_dodge_use_map(&s, &loaded);

    Sound tick = {0};
    bool audio = arcade_audio_ready();
    if (audio)
        tick = gfx_make_tone(880.0f, 0.06f, 0.4f); /* synthesised beep: no audio file needed */

    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        beat_dodge_step(&s, &in, ARC_DT);
        if (audio && s.beat_event)
            PlaySound(tick);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_hud(
            "BEAT DODGE", beat_dodge_score(&s),
            TextFormat("%d BPM   beat %d/%d", s.map.bpm, s.last_beat < 0 ? 0 : s.last_beat, s.map.length));
        arcade_frame_end();
        if (beat_dodge_is_over(&s))
            break;
    }
    if (audio)
        UnloadSound(tick);
    GameResult r;
    r.score = beat_dodge_score(&s);
    r.finished = beat_dodge_is_over(&s);
    return r;
}

const GameDef game_beat_dodge_def = {
    "beat_dodge",
    "Beat Dodge",
    "Dodge obstacles that arrive on the beat",
    "Left/Right (or A/D): change lane   obstacles hit the line exactly on the beat   3 lives",
    50,
    true,
    play};
