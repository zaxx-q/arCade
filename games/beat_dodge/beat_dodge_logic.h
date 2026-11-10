/* Beat Dodge - PURE game logic (no raylib). Owner: Member 5
 * Hook: rhythm bullet-dodger. Obstacles come from a song chart and arrive at your line exactly on the beat.
 *
 * A SONG (bd_Song) has one to three CHARTS (easy / normal / hard) and an optional MELODY that the built-in
 * synthesizer plays. Songs live in assets/beatmaps/ as text files (format below) and are listed in
 * assets/beatmaps/songs.txt. tools/midi2beatmap converts MIDI files into this format. */
#ifndef BEAT_DODGE_LOGIC_H
#define BEAT_DODGE_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/rng.h"
#include "arcade/synth.h"

#define BD_MAX_LANES 5
#define BD_MAX_STEPS 2048 /* characters in one chart */
#define BD_MAX_CHARTS 3
#define BD_MAX_NOTES 1536 /* melody notes in one song */
#define BD_MAX_SONGS 16
#define BD_CLOCKS_PER_BEAT 24 /* time unit of melody notes (the MIDI clock resolution) */
#define BD_LEAD_BEATS 3.0f    /* an obstacle takes this many beats to fall from the top to the player line */
#define BD_START_LIVES 3
#define BD_TOP_Y 60.0f
#define BD_PLAYER_Y 580.0f

/* Song file format (text). Lines starting with '#' are comments.
 *   title Bopeebo
 *   bpm 100
 *   steps 4              characters per beat in the charts (1..8, default 1)
 *   chart normal 3       starts a chart: name and number of lanes (2..5, default 3)
 *   ....0.1.2.1.         one character per step: '.' = rest, '0'..'4' = obstacle lane (lines are joined)
 *   melody               optional music, one note per line:  start length pitch voice
 *   0 12 63 0            start/length in 1/24 beats, MIDI pitch, voice 0..3
 * A file with only "bpm" and pattern lines (the first version of the format) is still valid: it becomes one
 * 3-lane chart named "normal" with 1 step per beat. */
typedef struct bd_Chart {
    char name[12];
    int lanes;
    int length;                /* steps */
    int8_t lane[BD_MAX_STEPS]; /* -1 = rest, otherwise the lane of the obstacle */
} bd_Chart;

typedef struct bd_Note {
    uint16_t start, len; /* 1/24 beat units */
    uint8_t pitch, voice;
} bd_Note;

typedef struct bd_Song {
    char title[48];
    int bpm;
    int steps; /* steps per beat */
    int chart_count;
    bd_Chart charts[BD_MAX_CHARTS];
    int note_count;
    bd_Note notes[BD_MAX_NOTES];
} bd_Song;

bool bd_song_load(const char *path, bd_Song *out); /* false if missing or invalid (out is untouched) */
bool bd_song_save(const char *path, const bd_Song *song);
void bd_song_default(bd_Song *out, unsigned seed); /* built-in generated practice song, always valid */
int bd_chart_obstacles(const bd_Chart *chart);
float bd_chart_multiplier(const char *name);           /* score multiplier: easy 1.0, normal 1.5, hard 2.0 */
float bd_song_seconds(const bd_Song *song, int chart); /* time at which the chart is finished */
/* Converts the melody to synth notes shifted by `delay` seconds; returns how many were written (<=
 * BD_MAX_NOTES). */
int bd_song_to_synth(const bd_Song *song, float delay, SynthNote *out);

/* The songs listed in <dir>/songs.txt (one file name per line, relative to dir). Missing/invalid files are
 * skipped. */
typedef struct bd_SongList {
    int count;
    int skipped;
    bd_Song songs[BD_MAX_SONGS];
} bd_SongList;
int bd_songlist_load(const char *dir, bd_SongList *out);

/* Song selection screen logic: Up/Down = song, Left/Right = difficulty, action = start. */
typedef struct bd_Menu {
    int song, chart;
    bool chosen;
} bd_Menu;
void bd_menu_init(bd_Menu *m);
void bd_menu_step(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list);

typedef struct beat_dodge_State {
    bd_Song song;
    int chart; /* index of the chart being played */
    int lanes, length;
    float time;      /* seconds since the start; the first obstacle needs `lead` seconds to fall */
    float spb;       /* seconds per beat */
    float step_sec;  /* seconds per chart step */
    float lead;      /* fall time of an obstacle */
    float end_time;  /* the song is won when time reaches this */
    int next_hit;    /* first step whose obstacle has not yet reached the player line */
    int last_beat;   /* beat number seen in the previous step */
    bool beat_event; /* true for the one step in which a new beat begins */
    int lane;        /* player lane */
    int lives, dodged, hits, combo, best_combo;
    int points, max_points;
    float multiplier;
    float hit_flash; /* seconds of red flash after a hit */
    bool over, won;
} beat_dodge_State;

void beat_dodge_init(beat_dodge_State *s, unsigned seed); /* the built-in practice song */
void beat_dodge_use_song(beat_dodge_State *s, const bd_Song *song, int chart); /* (re)start with a song */
void beat_dodge_step(beat_dodge_State *s, const ArcadeInput *in, float dt);
/* Follow an external clock (the music): only corrects drifts larger than 0.15 s so small offsets never
 * jitter. */
void beat_dodge_sync(beat_dodge_State *s, float clock_seconds);
bool beat_dodge_is_over(const beat_dodge_State *s);
int beat_dodge_score(
    const beat_dodge_State *s); /* normalised: 1000 for a perfect run x difficulty multiplier */
float beat_dodge_arrival(const beat_dodge_State *s,
                         int step); /* when the obstacle of `step` reaches the line */
/* Vertical pixel position of the obstacle of `step` (for drawing); < BD_TOP_Y means "not spawned yet". */
float beat_dodge_obstacle_y(const beat_dodge_State *s, int step);

#endif
