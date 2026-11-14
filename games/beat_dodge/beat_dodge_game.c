/* Beat Dodge - raylib front-end: song select screen, music playback and drawing.
 * All rules live in beat_dodge_logic.c; the music is synthesised from the song's melody (core/synth). */
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"
#include "beat_dodge_logic.h"

/* If the music sounds early/late compared with the falling obstacles on your machine, nudge this (seconds).
 */
#define BD_AUDIO_OFFSET_S 0.0f

static Color lane_color(int lane) {
    switch (lane) {
    case 0:
        return ARC_COL_MAGENTA;
    case 1:
        return ARC_COL_CYAN;
    case 2:
        return ARC_COL_YELLOW;
    case 3:
        return ARC_COL_GREEN;
    default:
        return ARC_COL_RED;
    }
}

static float lane_spacing(int lanes) {
    return lanes <= 3 ? 220.0f : (lanes == 4 ? 190.0f : 165.0f);
}

static float lane_x(int lanes, int lane) {
    return (float)ARC_SCREEN_W / 2.0f + ((float)lane - (float)(lanes - 1) / 2.0f) * lane_spacing(lanes);
}

static const char *clock_text(float seconds) {
    int s = (int)(seconds + 0.5f);
    return TextFormat("%d:%02d", s / 60, s % 60);
}

/* ------------------------------------------------------------------ song select */
#define MENU_ROWS 8
/* layout of the select screen (shared by drawing and mouse hit-testing) */
#define ROW_X 48
#define ROW_Y0 128
#define ROW_PITCH 64
#define ROW_W 560
#define ROW_H 56
#define CARD_X 678
#define CARD_Y0 250
#define CARD_PITCH 100
#define CARD_W 526
#define CARD_H 84
#define PLAY_X 678
#define PLAY_Y 556
#define PLAY_W 526
#define PLAY_H 56

static int menu_first(const bd_Menu *m, const bd_SongList *list) {
    return arc_clampi(m->song - MENU_ROWS / 2, 0, arc_maxi(0, list->count - MENU_ROWS));
}

static bool in_rect(float x, float y, float rx, float ry, float rw, float rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

/* Mouse: click a song to pick it, click a difficulty to pick it (click it again to start), click PLAY to
 * start; the wheel scrolls through the songs. */
static void menu_mouse(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list) {
    if (list->count == 0)
        return;
    if (in->wheel != 0) {
        m->song = (m->song + (in->wheel > 0 ? list->count - 1 : 1)) % list->count;
        m->chart = list->songs[m->song].chart_count > 1 ? 1 : 0;
    }
    if (!in->fire_pressed)
        return;
    float x = in->aim_x, y = in->aim_y;
    int first = menu_first(m, list);
    for (int row = 0; row < MENU_ROWS && first + row < list->count; row++)
        if (in_rect(x, y, ROW_X, (float)(ROW_Y0 + row * ROW_PITCH), ROW_W, ROW_H)) {
            if (first + row != m->song) {
                m->song = first + row;
                m->chart = list->songs[m->song].chart_count > 1 ? 1 : 0;
            }
            return;
        }
    for (int c = 0; c < list->songs[m->song].chart_count; c++)
        if (in_rect(x, y, CARD_X, (float)(CARD_Y0 + c * CARD_PITCH), CARD_W, CARD_H)) {
            if (c == m->chart)
                m->chosen = true;
            else
                m->chart = c;
            return;
        }
    if (in_rect(x, y, PLAY_X, PLAY_Y, PLAY_W, PLAY_H))
        m->chosen = true;
}

static void draw_select(const bd_SongList *list, const bd_Menu *m, float t) {
    Vector2 mouse = GetMousePosition();
    gfx_backdrop(t);
    gfx_text("Beat Dodge", 48, 22, 40, ARC_COL_CYAN);
    gfx_text("choose a song and a difficulty", 52, 76, 18, ARC_COL_DIM);

    int first = menu_first(m, list);
    for (int row = 0; row < MENU_ROWS && first + row < list->count; row++) {
        int i = first + row, y = 128 + row * 64;
        bool sel = i == m->song;
        Rectangle r = {48, (float)y, 560, 56};
        DrawRectangleRounded(r, 0.2f, 8, Fade(ARC_COL_PANEL, sel ? 0.97f : 0.8f));
        if (sel)
            DrawRectangleRoundedLinesEx(r, 0.2f, 8, 2.0f, ARC_COL_CYAN);
        else if (in_rect(mouse.x, mouse.y, r.x, r.y, r.width, r.height))
            DrawRectangleRoundedLinesEx(r, 0.2f, 8, 1.0f, Fade(ARC_COL_CYAN, 0.5f));
        gfx_text(list->songs[i].title, 66, y + 12, 24, sel ? ARC_COL_CYAN : ARC_COL_TEXT);
        gfx_text_right(TextFormat("%d BPM", list->songs[i].bpm), 590, y + 16, 17, ARC_COL_DIM);
    }
    if (list->count > MENU_ROWS)
        gfx_text(TextFormat("%d / %d", m->song + 1, list->count), 52, 104, 16, ARC_COL_DIM);

    /* details of the selected song */
    const bd_Song *song = &list->songs[m->song];
    DrawRectangleRounded((Rectangle){650, 128, 582, 504}, 0.04f, 8, Fade(ARC_COL_PANEL, 0.9f));
    gfx_text(song->title, 678, 148, 36, ARC_COL_YELLOW);
    gfx_text(
        TextFormat("%d BPM  |  %s", song->bpm, song->note_count > 0 ? "synthesised music" : "metronome only"),
        680, 200, 18, ARC_COL_DIM);
    for (int c = 0; c < song->chart_count; c++) {
        const bd_Chart *ch = &song->charts[c];
        bool sel = c == m->chart;
        Rectangle r = {678, (float)(250 + c * 100), 526, 84};
        DrawRectangleRounded(r, 0.15f, 8, Fade(sel ? ARC_COL_BG : ARC_COL_PANEL, 0.95f));
        if (sel)
            DrawRectangleRoundedLinesEx(r, 0.15f, 8, 2.0f, ARC_COL_YELLOW);
        else if (in_rect(mouse.x, mouse.y, r.x, r.y, r.width, r.height))
            DrawRectangleRoundedLinesEx(r, 0.15f, 8, 1.0f, Fade(ARC_COL_YELLOW, 0.5f));
        float secs = bd_song_seconds(song, c);
        float per_sec = (float)bd_chart_obstacles(ch) / (secs > 5.0f ? secs - 5.0f : 1.0f);
        gfx_text(ch->name, 698, 262 + c * 100, 26, sel ? ARC_COL_YELLOW : ARC_COL_TEXT);
        gfx_text(TextFormat("%d lanes   |   %d obstacles   |   %.1f per second   |   %s", ch->lanes,
                            bd_chart_obstacles(ch), per_sec, clock_text(secs)),
                 698, 298 + c * 100, 17, ARC_COL_DIM);
        gfx_text_right(TextFormat("score x%.1f", bd_chart_multiplier(ch->name)), 1188, 266 + c * 100, 17,
                       ARC_COL_DIM);
    }
    Rectangle play = {PLAY_X, PLAY_Y, PLAY_W, PLAY_H};
    bool play_hover = in_rect(mouse.x, mouse.y, play.x, play.y, play.width, play.height);
    DrawRectangleRounded(play, 0.3f, 8, Fade(ARC_COL_CYAN, play_hover ? 0.35f : 0.18f));
    DrawRectangleRoundedLinesEx(play, 0.3f, 8, 2.0f, ARC_COL_CYAN);
    gfx_text_center("PLAY", (int)(PLAY_X + PLAY_W / 2), (int)PLAY_Y + 12, 28, ARC_COL_TEXT);
    gfx_text("[Up/Down] or click a song   [Left/Right] or click a difficulty   [Enter] or PLAY: start   "
             "[Esc] back",
             48, ARC_SCREEN_H - 36, 17, ARC_COL_DIM);
}

/* ------------------------------------------------------------------ music */
typedef struct SongAudio {
    bool ready;
    Music music;
    unsigned char *wav;
} SongAudio;

/* Renders the song's melody + drums into an in-memory WAV and starts playing it. The timeline matches the
 * game clock exactly: a melody note sounds at the moment its obstacle reaches the player line. */
static bool audio_start(SongAudio *a, const beat_dodge_State *s) {
    memset(a, 0, sizeof(*a));
    if (!arcade_audio_ready() || s->run.song.note_count == 0)
        return false;
    static SynthNote notes[BD_MAX_NOTES];
    int n = bd_song_to_synth(&s->run.song, s->run.lead, notes);
    size_t frames = synth_frames(s->run.end_time + 1.0f, SYNTH_RATE), bytes = synth_wav_bytes(frames);
    a->wav = (unsigned char *)MemAlloc((unsigned int)bytes);
    if (a->wav == NULL)
        return false;
    SynthOptions o;
    o.rate = SYNTH_RATE;
    o.bpm = (float)s->run.song.bpm;
    o.drums = true;
    o.drums_from = s->run.lead;
    o.drums_to = s->run.end_time - s->run.spb;
    synth_render(notes, (size_t)n, &o, (int16_t *)(a->wav + 44),
                 frames); /* samples go right after the header */
    synth_wav_header(frames, SYNTH_RATE, a->wav);
    a->music = LoadMusicStreamFromMemory(".wav", a->wav, (int)bytes);
    if (!IsMusicValid(a->music)) {
        MemFree(a->wav);
        a->wav = NULL;
        return false;
    }
    a->music.looping = false;
    SetMusicVolume(a->music, 0.85f);
    PlayMusicStream(a->music);
    a->ready = true;
    return true;
}

static void audio_stop(SongAudio *a) {
    if (a->ready) {
        StopMusicStream(a->music);
        UnloadMusicStream(a->music);
    }
    if (a->wav != NULL)
        MemFree(a->wav);
    memset(a, 0, sizeof(*a));
}

/* ------------------------------------------------------------------ drawing the game */
static void draw_game(const beat_dodge_State *s, float t) {
    gfx_backdrop(t);
    int lanes = s->run.lanes;
    float spacing = lane_spacing(lanes), w = spacing - 36.0f;
    float phase = s->run.spb > 0.0f ? (s->run.time / s->run.spb) - (float)s->run.last_beat
                                    : 0.0f; /* 0..1 within the beat */
    for (int l = 0; l < lanes; l++) {
        float x = lane_x(lanes, l);
        Color c = lane_color(l);
        DrawRectangle((int)(x - w / 2), (int)BD_TOP_Y, (int)w, (int)(BD_PLAYER_Y - BD_TOP_Y + 60),
                      Fade(ARC_COL_PANEL, 0.55f));
        DrawCircleLines((int)x, (int)BD_PLAYER_Y, 34.0f + 26.0f * (1.0f - phase),
                        Fade(c, 0.6f * (1.0f - phase))); /* beat pulse */
        DrawCircleLines((int)x, (int)BD_PLAYER_Y, 32.0f, Fade(c, 0.35f));
        gfx_text_center(TextFormat("%d", l + 1), (int)x, (int)BD_PLAYER_Y + 48, 16, Fade(ARC_COL_DIM, 0.8f));
    }
    const bd_Chart *chart = &s->run.song.charts[s->run.chart];
    for (int i = s->run.next_hit; i < s->run.length; i++) {
        float y = beat_dodge_obstacle_y(s, i);
        if (y < BD_TOP_Y)
            break; /* later steps have not spawned yet */
        if (chart->lane[i] >= 0)
            gfx_glow_circle(lane_x(lanes, chart->lane[i]), y, 22.0f, lane_color(chart->lane[i]));
    }
    float px = lane_x(lanes, s->run.lane);
    DrawTriangle((Vector2){px, BD_PLAYER_Y - 26}, (Vector2){px - 22, BD_PLAYER_Y + 20},
                 (Vector2){px + 22, BD_PLAYER_Y + 20}, ARC_COL_TEXT);
    if (s->run.hit_flash > 0.0f)
        DrawRectangle(0, 36, ARC_SCREEN_W, ARC_SCREEN_H - 36, Fade(ARC_COL_RED, s->run.hit_flash));
    for (int h = 0; h < s->lives; h++)
        DrawCircle(40 + h * 34, 76, 11, ARC_COL_RED);
    if (s->combo > 1)
        gfx_text_center(TextFormat("combo x%d", s->combo), ARC_SCREEN_W / 2, ARC_SCREEN_H - 52, 26,
                        ARC_COL_YELLOW);
    /* song progress under the HUD */
    DrawRectangle(0, 36, (int)((float)ARC_SCREEN_W * arc_clampf(s->run.time / s->run.end_time, 0.0f, 1.0f)),
                  3, ARC_COL_CYAN);
    if (s->run.time < 2.6f) /* song title card */
        gfx_text_center(s->run.song.title, ARC_SCREEN_W / 2, 250, 54,
                        Fade(ARC_COL_TEXT, arc_clampf((2.6f - s->run.time) / 0.8f, 0.0f, 1.0f)));
}

/* ------------------------------------------------------------------ play */
static bd_SongList g_list; /* ~260 KB */
static beat_dodge_State g_state;
static bd_Menu g_menu;
static bool g_menu_ready;

/* Runs the song select screen. Returns false when the player backs out (ESC) or closes the window. */
static bool select_song(const GameContext *ctx) {
    if (!g_menu_ready) {
        bd_menu_init(&g_menu);
        g_menu_ready = true;
    }
    g_menu.chosen = false;
    g_menu.song = arc_clampi(g_menu.song, 0, g_list.count - 1);
    g_menu.chart = arc_clampi(g_menu.chart, 0, g_list.songs[g_menu.song].chart_count - 1);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        bd_menu_step(&g_menu, &in, &g_list);
        menu_mouse(&g_menu, &in, &g_list);
        t += ARC_DT;
        BeginDrawing();
        draw_select(&g_list, &g_menu, t);
        arcade_frame_end();
        if (g_menu.chosen)
            return true;
    }
    return false;
}

/* Smoke runs have no input, so they just draw the select screen for a moment (screenshot: <name>_select.png).
 */
static void preview_select(const GameContext *ctx) {
    char path[700];
    const char *shot = ctx->screenshot_path;
    const char *dot = shot != NULL ? strrchr(shot, '.') : NULL;
    if (dot == NULL)
        return;
    snprintf(path, sizeof(path), "%.*s_select%s", (int)(dot - shot), shot, dot);
    GameContext c = *ctx;
    c.screenshot_path = path;
    c.max_frames = 30;
    bd_Menu m;
    bd_menu_init(&m);
    m.song = g_list.count > 1 ? 1 : 0;
    m.chart = g_list.songs[m.song].chart_count > 1 ? 1 : 0;
    arcade_loop_begin(&c);
    float t = 0.0f;
    while (arcade_loop_running()) {
        t += ARC_DT;
        BeginDrawing();
        draw_select(&g_list, &m, t);
        arcade_frame_end();
    }
}

static GameResult play(const GameContext *ctx) {
    char dir[700];
    snprintf(dir, sizeof(dir), "%s/beatmaps", ctx->assets_dir ? ctx->assets_dir : "assets");
    int count = bd_songlist_load(dir, &g_list);

    if (count == 0) { /* no song files found: play the built-in practice song */
        beat_dodge_init(&g_state, ctx->seed);
    } else if (ctx->headless) { /* smoke runs: no menu, no audio; the first real song on its hardest chart */
        preview_select(ctx);
        const bd_Song *song = &g_list.songs[count > 1 ? 1 : 0];
        beat_dodge_use_song(&g_state, song, song->chart_count - 1);
    } else {
        if (!select_song(ctx)) {
            GameResult cancelled = {0, false};
            return cancelled;
        }
        beat_dodge_use_song(&g_state, &g_list.songs[g_menu.song], g_menu.chart);
    }
    beat_dodge_State *s = &g_state;

    Sound tick = {0}; /* metronome for songs without music */
    SongAudio music;
    bool has_music = !ctx->headless && audio_start(&music, s);
    bool metronome = !ctx->headless && !has_music && arcade_audio_ready();
    if (metronome)
        tick = gfx_make_tone(880.0f, 0.06f, 0.4f);

    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        if (has_music) {
            UpdateMusicStream(music.music);
            float mt = GetMusicTimePlayed(music.music);
            if (mt > 0.5f) /* follow the audio clock if the game falls far behind (slow machine) */
                beat_dodge_sync(s, mt + BD_AUDIO_OFFSET_S);
        }
        beat_dodge_step(s, &in, ARC_DT);
        if (metronome && s->run.beat_event)
            PlaySound(tick);
        t += ARC_DT;
        BeginDrawing();
        draw_game(s, t);
        gfx_hud("BEAT DODGE", beat_dodge_score(s),
                TextFormat("%s [%s]   %d BPM", s->run.song.title, s->run.song.charts[s->run.chart].name,
                           s->run.song.bpm));
        arcade_frame_end();
        if (beat_dodge_is_over(s))
            break;
    }
    if (has_music)
        audio_stop(&music);
    if (metronome)
        UnloadSound(tick);
    GameResult r;
    r.score = beat_dodge_score(s);
    r.finished = beat_dodge_is_over(s);
    return r;
}

const GameDef game_beat_dodge_def = {
    "beat_dodge",
    "Beat Dodge",
    "Dodge notes that arrive on the beat of real songs",
    "Pick a song, then Left/Right or 1-4: change lane  obstacles reach the line exactly on the beat  3 lives",
    50,
    true,
    play};
