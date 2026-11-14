/* Beat Catch - raylib front-end: window loop and the drawing that is specific to catching.
 * All rules live in beat_catch_logic.c. The song select screen, the music and the playfield are shared with
 * Beat Dodge (beat_dodge_ui.h); here they are used with the "catch" look: hollow rings and a cup. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "beat_catch_logic.h"
#include "beat_dodge_ui.h"

static bd_SongList g_list; /* ~544 KB: static, never a local */
static beat_catch_State g_state;
static bd_Menu g_menu;
static bool g_menu_ready;

/* Health bar under the HUD: green while healthy, yellow below half, red below a quarter. */
static void draw_health(int health) {
    const float x = 40.0f, y = 68.0f, w = 240.0f, h = 16.0f;
    Color c = health > BC_START_HEALTH / 2 ? ARC_COL_GREEN
                                           : (health > BC_START_HEALTH / 4 ? ARC_COL_YELLOW : ARC_COL_RED);
    DrawRectangle((int)x, (int)y, (int)w, (int)h, Fade(ARC_COL_PANEL, 0.9f));
    DrawRectangle((int)x, (int)y, (int)(w * (float)health / (float)BC_START_HEALTH), (int)h, c);
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, ARC_COL_DIM);
}

static void draw_game(const beat_catch_State *s, float t) {
    float pop = arc_clampf(s->catch_flash / 0.3f, 0.0f, 1.0f);
    bd_ui_draw_field(&s->run, BD_UI_CATCH, t, s->catch_lane, pop);
    if (s->run.hit_flash > 0.0f) /* a missed bead */
        DrawRectangle(0, 36, ARC_SCREEN_W, ARC_SCREEN_H - 36, Fade(ARC_COL_RED, s->run.hit_flash * 0.8f));
    draw_health(s->health);
    if (s->combo >= 2) {
        int mult = beat_catch_combo_multiplier(s->combo);
        gfx_text_center(TextFormat("combo %d", s->combo), ARC_SCREEN_W / 2, ARC_SCREEN_H - 82, 26,
                        mult > 1 ? ARC_COL_YELLOW : ARC_COL_TEXT);
        if (mult > 1)
            gfx_text_center(TextFormat("x%d", mult), ARC_SCREEN_W / 2, ARC_SCREEN_H - 52, 24, ARC_COL_YELLOW);
    }
    /* song progress under the HUD */
    DrawRectangle(0, 36, (int)((float)ARC_SCREEN_W * arc_clampf(s->run.time / s->run.end_time, 0.0f, 1.0f)),
                  3, ARC_COL_CYAN);
    if (s->run.time < 2.6f) /* song title card */
        gfx_text_center(s->run.song.title, ARC_SCREEN_W / 2, 250, 54,
                        Fade(ARC_COL_TEXT, arc_clampf((2.6f - s->run.time) / 0.8f, 0.0f, 1.0f)));
}

static GameResult play(const GameContext *ctx) {
    char dir[700];
    snprintf(dir, sizeof(dir), "%s/beatmaps", ctx->assets_dir ? ctx->assets_dir : "assets");
    int count = bd_songlist_load(dir, &g_list);
    beat_catch_State *s = &g_state;

    if (count == 0) { /* no song files found: play the built-in practice song */
        beat_catch_init(s, ctx->seed);
    } else if (ctx->headless) { /* smoke runs: no menu, no audio; the first real song on its hardest catch
                                   chart */
        bd_ui_preview_select(ctx, "Beat Catch", BD_CHART_CATCH, &g_list);
        const bd_Song *song = &g_list.songs[count > 1 ? 1 : 0];
        int hardest = bd_song_kind_chart(song, BD_CHART_CATCH, bd_song_kind_count(song, BD_CHART_CATCH) - 1);
        beat_catch_use_song(s, song, hardest);
    } else {
        if (!g_menu_ready) {
            bd_menu_init(&g_menu, BD_CHART_CATCH);
            g_menu_ready = true;
        }
        if (!bd_ui_select_song(ctx, "Beat Catch", &g_menu, &g_list)) {
            GameResult cancelled = {0, false};
            return cancelled;
        }
        beat_catch_use_song(s, &g_list.songs[g_menu.song], g_menu.chart);
    }

    Sound tick = {0}; /* metronome for songs without music */
    bool has_music = !ctx->headless && bd_ui_music_start(&s->run);
    bool metronome = !ctx->headless && !has_music && arcade_audio_ready();
    if (metronome)
        tick = gfx_make_tone(880.0f, 0.06f, 0.4f);
    int last_missed = 0; /* a short low tone for every missed bead */
    Sound oops = {0};
    if (!ctx->headless && arcade_audio_ready())
        oops = gfx_make_tone(160.0f, 0.12f, 0.35f);

    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        if (has_music) {
            float mt = bd_ui_music_time();
            if (mt > 0.0f) /* follow the audio clock if the game falls far behind (slow machine) */
                beat_catch_sync(s, mt);
        }
        beat_catch_step(s, &in, ARC_DT);
        if (metronome && s->run.beat_event)
            PlaySound(tick);
        if (s->missed > last_missed && oops.frameCount > 0)
            PlaySound(oops);
        last_missed = s->missed;
        t += ARC_DT;
        BeginDrawing();
        draw_game(s, t);
        gfx_hud("BEAT CATCH", beat_catch_score(s),
                TextFormat("%s [%s]   %d BPM", s->run.song.title, s->run.song.charts[s->run.chart].name,
                           s->run.song.bpm));
        arcade_frame_end();
        if (beat_catch_is_over(s))
            break;
    }
    if (has_music)
        bd_ui_music_stop();
    if (metronome)
        UnloadSound(tick);
    if (oops.frameCount > 0)
        UnloadSound(oops);
    GameResult r;
    r.score = beat_catch_score(s);
    r.finished = beat_catch_is_over(s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_beat_catch_def = {
    "beat_catch",
    "Beat Catch",
    "Catch the notes of real songs on the beat",
    "Pick a song, then Left/Right or 1-4: change lane  be in a note's lane when "
    "it reaches the line  keep the combo",
    55,
    true,
    play};
