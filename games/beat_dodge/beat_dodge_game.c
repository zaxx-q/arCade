/* Beat Dodge - raylib front-end: song select screen, music playback and drawing.
 * All rules live in beat_dodge_logic.c; the music is synthesised from the song's melody (core/synth).
 * The select screen, the music and the playfield are shared with Beat Catch (beat_dodge_ui.h), which is why
 * they are not static. */
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"
#include "beat_dodge_logic.h"
#include "beat_dodge_ui.h"

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

float bd_ui_lane_x(int lanes, int lane) {
    return lane_x(lanes, lane);
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
 * start; the wheel scrolls through the songs. Only songs and charts of the menu's kind are offered. */
static void menu_mouse(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list) {
    if (list->count == 0)
        return;
    if (in->wheel != 0)
        bd_menu_move_song(m, list, in->wheel > 0 ? -1 : 1);
    if (!in->fire_pressed)
        return;
    float x = in->aim_x, y = in->aim_y;
    int first = menu_first(m, list);
    for (int row = 0; row < MENU_ROWS && first + row < list->count; row++)
        if (in_rect(x, y, ROW_X, (float)(ROW_Y0 + row * ROW_PITCH), ROW_W, ROW_H)) {
            if (first + row != m->song && bd_song_kind_count(&list->songs[first + row], m->kind) > 0) {
                m->song = first + row;
                m->chart = bd_song_kind_chart(&list->songs[m->song], m->kind,
                                              bd_song_kind_count(&list->songs[m->song], m->kind) > 1 ? 1 : 0);
            }
            return;
        }
    const bd_Song *song = &list->songs[m->song];
    for (int nth = 0; nth < bd_song_kind_count(song, m->kind); nth++)
        if (in_rect(x, y, CARD_X, (float)(CARD_Y0 + nth * CARD_PITCH), CARD_W, CARD_H)) {
            int c = bd_song_kind_chart(song, m->kind, nth);
            if (c == m->chart)
                m->chosen = true;
            else
                bd_menu_pick_chart(m, list, c);
            return;
        }
    if (in_rect(x, y, PLAY_X, PLAY_Y, PLAY_W, PLAY_H))
        m->chosen = true;
}

static void draw_select(const char *title, const bd_SongList *list, const bd_Menu *m, float t) {
    Vector2 mouse = GetMousePosition();
    gfx_backdrop(t);
    gfx_text(title, 48, 22, 40, ARC_COL_CYAN);
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
        gfx_text(list->songs[i].title, 66, y + 12, 24,
                 bd_song_kind_count(&list->songs[i], m->kind) == 0 ? ARC_COL_DIM
                                                                   : (sel ? ARC_COL_CYAN : ARC_COL_TEXT));
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
    for (int nth = 0; nth < bd_song_kind_count(song, m->kind); nth++) {
        int c = bd_song_kind_chart(song, m->kind, nth);
        const bd_Chart *ch = &song->charts[c];
        bool sel = c == m->chart;
        Rectangle r = {678, (float)(250 + nth * 100), 526, 84};
        DrawRectangleRounded(r, 0.15f, 8, Fade(sel ? ARC_COL_BG : ARC_COL_PANEL, 0.95f));
        if (sel)
            DrawRectangleRoundedLinesEx(r, 0.15f, 8, 2.0f, ARC_COL_YELLOW);
        else if (in_rect(mouse.x, mouse.y, r.x, r.y, r.width, r.height))
            DrawRectangleRoundedLinesEx(r, 0.15f, 8, 1.0f, Fade(ARC_COL_YELLOW, 0.5f));
        float secs = bd_song_seconds(song, c);
        int objects = bd_chart_objects(ch), ribbons = 0; /* ribbons = objects of two or more beads */
        for (int k = 0; k < ch->length; k++)
            if (ch->lane[k] >= 0 && !ch->link[k] && bd_chart_in_ribbon(ch, k))
                ribbons++;
        /* Beat Dodge counts objects (one life each); Beat Catch judges every bead, so it shows the beads */
        int shown = m->kind == BD_CHART_DODGE ? objects : bd_chart_beads(ch);
        float per_sec = (float)shown / (secs > 5.0f ? secs - 5.0f : 1.0f);
        gfx_text(ch->name, 698, 262 + nth * 100, 26, sel ? ARC_COL_YELLOW : ARC_COL_TEXT);
        gfx_text(TextFormat("%d lanes  |  %d %s, %d %s  |  %.1f/s  |  %s", ch->lanes, shown,
                            m->kind == BD_CHART_DODGE ? "obstacles" : "notes", ribbons,
                            ribbons == 1 ? "ribbon" : "ribbons", per_sec, clock_text(secs)),
                 698, 298 + nth * 100, 17, ARC_COL_DIM);
        gfx_text_right(TextFormat("score x%.1f", bd_chart_multiplier(ch->name)), 1188, 266 + nth * 100, 17,
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

static SongAudio g_music; /* the one song that plays now */

/* Renders the song's melody + drums into an in-memory WAV and starts playing it. The timeline matches the
 * game clock exactly: a melody note sounds at the moment its bead reaches the player line. Notes that have a
 * bead are the bright lead; all others are quiet backing (bd_song_to_synth). */
bool bd_ui_music_start(const bd_Run *run) {
    SongAudio *a = &g_music;
    memset(a, 0, sizeof(*a));
    if (!arcade_audio_ready() || run->song.note_count == 0)
        return false;
    static SynthNote notes[BD_MAX_NOTES];
    int n = bd_song_to_synth(&run->song, run->chart, run->lead, notes);
    size_t frames = synth_frames(run->end_time + 1.0f, SYNTH_RATE), bytes = synth_wav_bytes(frames);
    a->wav = (unsigned char *)MemAlloc((unsigned int)bytes);
    if (a->wav == NULL)
        return false;
    SynthOptions o;
    o.rate = SYNTH_RATE;
    o.bpm = (float)run->song.bpm;
    o.drums = true;
    o.drums_from = run->lead;
    o.drums_to = run->end_time - run->spb;
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

float bd_ui_music_time(void) {
    if (!g_music.ready)
        return -1.0f;
    UpdateMusicStream(g_music.music);
    float mt = GetMusicTimePlayed(g_music.music);
    return mt > 0.5f ? mt + BD_AUDIO_OFFSET_S : -1.0f;
}

void bd_ui_music_stop(void) {
    SongAudio *a = &g_music;
    if (a->ready) {
        StopMusicStream(a->music);
        UnloadMusicStream(a->music);
    }
    if (a->wav != NULL)
        MemFree(a->wav);
    memset(a, 0, sizeof(*a));
}

/* ------------------------------------------------------------------ drawing the game */
/* A ribbon is drawn as a band that joins its beads, so a run of fast notes reads as ONE object. The band is
 * cut where it leaves the falling area (above the top, or past the player line after its earlier beads
 * passed). */
static void ribbon_band(float x0, float y0, float x1, float y1, Color c) {
    float low = BD_PLAYER_Y + 30.0f; /* (x0, y0) is the earlier bead: lower on the screen */
    if (y0 > low) {                  /* its start already passed the line: cut it there */
        float k = (y0 - low) / (y0 - y1);
        x0 += (x1 - x0) * k;
        y0 = low;
    }
    if (y1 < BD_TOP_Y) { /* its end has not spawned yet: cut it at the top */
        float k = (BD_TOP_Y - y1) / (y0 - y1);
        x1 += (x0 - x1) * k;
        y1 = BD_TOP_Y;
    }
    if (y0 <= y1)
        return;
    DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, 18.0f, Fade(c, 0.35f));
    DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, 5.0f, Fade(ARC_COL_TEXT, 0.8f));
}

/* One bead: a solid glowing disc to dodge, or a hollow ring to catch. */
static void draw_bead(float x, float y, float radius, Color c, bd_UiStyle style) {
    if (style == BD_UI_DODGE) {
        gfx_glow_circle(x, y, radius, c);
        return;
    }
    DrawCircle((int)x, (int)y, radius, Fade(c, 0.16f));
    DrawRing((Vector2){x, y}, radius - 6.0f, radius, 0.0f, 360.0f, 28, c);
}

/* The beads that are still on their way, and the bands joining the beads of each ribbon. */
static void draw_beads(const bd_Run *r, const bd_Chart *chart, bd_UiStyle style) {
    int lanes = r->lanes;
    int prev = -1; /* the bead before i: the one a band to i starts at */
    for (int i = 0; i < r->next_hit && i < r->length; i++)
        if (chart->lane[i] >= 0)
            prev = i; /* beads that already passed (only the last one matters) */
    for (int i = r->next_hit; i < r->length; i++) {
        if (chart->lane[i] < 0)
            continue;
        float y = bd_run_obstacle_y(r, i);
        if (chart->link[i] && prev >= 0)
            ribbon_band(lane_x(lanes, chart->lane[prev]), bd_run_obstacle_y(r, prev),
                        lane_x(lanes, chart->lane[i]), y, lane_color(chart->lane[i]));
        if (y < BD_TOP_Y)
            break; /* later steps have not spawned yet */
        prev = i;
    }
    for (int i = r->next_hit; i < r->length; i++) {
        if (chart->lane[i] < 0)
            continue;
        float y = bd_run_obstacle_y(r, i);
        if (y < BD_TOP_Y)
            break;
        /* the first bead of an object is a little bigger than the ones that follow it in a ribbon */
        draw_bead(lane_x(lanes, chart->lane[i]), y, chart->link[i] ? 18.0f : 23.0f,
                  lane_color(chart->lane[i]), style);
    }
}

void bd_ui_draw_field(const bd_Run *run, bd_UiStyle style, float t, int pop_lane, float pop) {
    gfx_backdrop(t);
    int lanes = run->lanes;
    float spacing = lane_spacing(lanes), w = spacing - 36.0f;
    float phase =
        run->spb > 0.0f ? (run->time / run->spb) - (float)run->last_beat : 0.0f; /* 0..1 within the beat */
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
    draw_beads(run, &run->song.charts[run->chart], style);
    if (pop > 0.0f && pop_lane >= 0 && pop_lane < lanes) /* a caught bead: a ring that grows and fades */
        DrawRing((Vector2){lane_x(lanes, pop_lane), BD_PLAYER_Y}, 30.0f + 40.0f * (1.0f - pop),
                 34.0f + 40.0f * (1.0f - pop), 0.0f, 360.0f, 32, Fade(lane_color(pop_lane), pop));
    float px = lane_x(lanes, run->lane);
    if (style == BD_UI_DODGE) {
        DrawTriangle((Vector2){px, BD_PLAYER_Y - 26}, (Vector2){px - 22, BD_PLAYER_Y + 20},
                     (Vector2){px + 22, BD_PLAYER_Y + 20}, ARC_COL_TEXT);
    } else { /* a cup that opens upwards: raylib angles run clockwise from 3 o'clock, so 0..180 is the lower
                half */
        DrawRing((Vector2){px, BD_PLAYER_Y - 6.0f}, 20.0f, 28.0f, 0.0f, 180.0f, 24, ARC_COL_TEXT);
        DrawRectangle((int)px - 28, (int)BD_PLAYER_Y - 10, 8, 8, ARC_COL_TEXT); /* the two rims */
        DrawRectangle((int)px + 20, (int)BD_PLAYER_Y - 10, 8, 8, ARC_COL_TEXT);
    }
}

static void draw_game(const beat_dodge_State *s, float t) {
    bd_ui_draw_field(&s->run, BD_UI_DODGE, t, -1, 0.0f);
    float px = lane_x(s->run.lanes, s->run.lane);
    if (s->run.hit_flash > 0.0f)
        DrawRectangle(0, 36, ARC_SCREEN_W, ARC_SCREEN_H - 36, Fade(ARC_COL_RED, s->run.hit_flash));
    for (int h = 0; h < s->lives; h++)
        DrawCircle(40 + h * 34, 76, 11, ARC_COL_RED);
    if (s->close_flash > 0.0f) /* no combo in Beat Dodge: the reward is the close call itself */
        gfx_text_center("CLOSE!", (int)px, (int)BD_PLAYER_Y - 78, 30,
                        Fade(ARC_COL_YELLOW, arc_clampf(s->close_flash / 0.3f, 0.0f, 1.0f)));
    if (s->close_calls > 0)
        gfx_text_center(TextFormat("close calls %d", s->close_calls), ARC_SCREEN_W / 2, ARC_SCREEN_H - 52, 24,
                        ARC_COL_YELLOW);
    /* song progress under the HUD */
    DrawRectangle(0, 36, (int)((float)ARC_SCREEN_W * arc_clampf(s->run.time / s->run.end_time, 0.0f, 1.0f)),
                  3, ARC_COL_CYAN);
    if (s->run.time < 2.6f) /* song title card */
        gfx_text_center(s->run.song.title, ARC_SCREEN_W / 2, 250, 54,
                        Fade(ARC_COL_TEXT, arc_clampf((2.6f - s->run.time) / 0.8f, 0.0f, 1.0f)));
}

/* ------------------------------------------------------------------ play */
bool bd_ui_select_song(const GameContext *ctx, const char *title, bd_Menu *menu, const bd_SongList *list) {
    menu->chosen = false;
    menu->song = arc_clampi(menu->song, 0, list->count - 1);
    menu->chart = arc_clampi(menu->chart, 0, list->songs[menu->song].chart_count - 1);
    ArcadeInput none;
    arcade_input_clear(&none);
    bd_menu_step(menu, &none, list); /* moves to a song and chart of this menu's kind if needed */
    menu->chosen = false;
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        bd_menu_step(menu, &in, list);
        menu_mouse(menu, &in, list);
        t += ARC_DT;
        BeginDrawing();
        draw_select(title, list, menu, t);
        arcade_frame_end();
        if (menu->chosen)
            return true;
    }
    return false;
}

/* Smoke runs have no input, so they just draw the select screen for a moment (screenshot: <name>_select.png).
 */
void bd_ui_preview_select(const GameContext *ctx, const char *title, bd_ChartKind kind,
                          const bd_SongList *list) {
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
    bd_menu_init(&m, kind);
    m.song = list->count > 1 ? 1 : 0;
    m.chart = bd_song_kind_chart(&list->songs[m.song], kind,
                                 bd_song_kind_count(&list->songs[m.song], kind) > 1 ? 1 : 0);
    arcade_loop_begin(&c);
    float t = 0.0f;
    while (arcade_loop_running()) {
        t += ARC_DT;
        BeginDrawing();
        draw_select(title, list, &m, t);
        arcade_frame_end();
    }
}

/* ------------------------------------------------------------------ play */
static bd_SongList g_list; /* ~544 KB */
static beat_dodge_State g_state;
static bd_Menu g_menu;
static bool g_menu_ready;

static GameResult play(const GameContext *ctx) {
    char dir[700];
    snprintf(dir, sizeof(dir), "%s/beatmaps", ctx->assets_dir ? ctx->assets_dir : "assets");
    int count = bd_songlist_load(dir, &g_list);

    if (count == 0) { /* no song files found: play the built-in practice song */
        beat_dodge_init(&g_state, ctx->seed);
    } else if (ctx->headless) { /* smoke runs: no menu, no audio; the first real song on its hardest chart */
        bd_ui_preview_select(ctx, "Beat Dodge", BD_CHART_DODGE, &g_list);
        const bd_Song *song = &g_list.songs[count > 1 ? 1 : 0];
        int hardest = bd_song_kind_chart(song, BD_CHART_DODGE, bd_song_kind_count(song, BD_CHART_DODGE) - 1);
        beat_dodge_use_song(&g_state, song, hardest);
    } else {
        if (!g_menu_ready) {
            bd_menu_init(&g_menu, BD_CHART_DODGE);
            g_menu_ready = true;
        }
        if (!bd_ui_select_song(ctx, "Beat Dodge", &g_menu, &g_list)) {
            GameResult cancelled = {0, false};
            return cancelled;
        }
        beat_dodge_use_song(&g_state, &g_list.songs[g_menu.song], g_menu.chart);
    }
    beat_dodge_State *s = &g_state;

    Sound tick = {0}; /* metronome for songs without music */
    bool has_music = !ctx->headless && bd_ui_music_start(&s->run);
    bool metronome = !ctx->headless && !has_music && arcade_audio_ready();
    if (metronome)
        tick = gfx_make_tone(880.0f, 0.06f, 0.4f);

    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        if (has_music) {
            float mt = bd_ui_music_time();
            if (mt > 0.0f) /* follow the audio clock if the game falls far behind (slow machine) */
                beat_dodge_sync(s, mt);
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
        bd_ui_music_stop();
    if (metronome)
        UnloadSound(tick);
    GameResult r;
    r.score = beat_dodge_score(s);
    r.finished = beat_dodge_is_over(s);
    return r;
}

const GameDef game_beat_dodge_def = {"beat_dodge",
                                     "Beat Dodge",
                                     "Dodge notes that arrive on the beat of real songs",
                                     "Pick a song, then Left/Right or 1-4: change lane  obstacles reach the "
                                     "line on the beat  3 lives  close calls pay double",
                                     50,
                                     true,
                                     play};
