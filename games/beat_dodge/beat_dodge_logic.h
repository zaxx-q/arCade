/* Beat Dodge - PURE game logic (no raylib). Owner: Member 5
 * Hook: rhythm bullet-dodger. Obstacles come from a song chart and arrive at your line exactly on the beat.
 *
 * A SONG (bd_Song) has up to six CHARTS (easy / normal / hard for Beat Dodge and for Beat Catch, which shares
 * this module) and an optional MELODY that the built-in synthesizer plays. Songs live in assets/beatmaps/ as
 * text files (format below) and are listed in assets/beatmaps/songs.txt. tools/midi2beatmap converts MIDI
 * files into this format. */
#ifndef BEAT_DODGE_LOGIC_H
#define BEAT_DODGE_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/rng.h"
#include "arcade/synth.h"

#define BD_MAX_LANES 5
#define BD_MAX_STEPS 2048 /* characters in one chart */
#define BD_MAX_CHARTS 6   /* 3 dodge + 3 catch charts */
#define BD_MAX_NOTES 1536 /* melody notes in one song */
#define BD_MAX_SONGS 16
#define BD_CLOCKS_PER_BEAT 24 /* time unit of melody notes (the MIDI clock resolution) */
#define BD_LEAD_BEATS 3.0f    /* an obstacle takes this many beats to fall from the top to the player line */
#define BD_START_LIVES 3
#define BD_TOP_Y 60.0f
#define BD_PLAYER_Y 580.0f
#define BD_HUMAN_STEP_SEC                                                                                    \
    0.12f /* time a human needs per lane step (arrow-key taps) in the reachable-lane check */
#define BD_CLOSE_WINDOW_SEC                                                                                  \
    0.30f /* standing in an object's lane this recently before it arrives = close call */

/* Vocabulary: a BEAD is one note on the chart (one step that has a lane). A RIBBON is a run of linked beads
 * that is treated as one OBJECT (a lone note is a ribbon of one bead). Damage limits, spacing and scoring
 * work per object; judging works per bead. */

/* clang-format off */
/* Song file format (text, version 3). Lines starting with '#' are comments.
 *   title Bopeebo
 *   bpm 100
 *   steps 4              characters per beat in the charts (1..8, default 1); write it BEFORE the first chart
 *   chart normal 3       starts a dodge chart: name and number of lanes (2..5, default 3)
 *   chart hard 4 catch   a third word "catch" makes it a Beat Catch chart ("dodge" is the default)
 *   ....0.1.2.1.         one character per step, lines are joined:
 *                          '.'        rest
 *                          '0'..'4'   a bead in that lane that starts a new object
 *                          'a'..'e'   a bead in lane 0..4 that CONTINUES the object of the previous bead
 *                                     (rests may sit in between: fast songs have a rest step inside a run)
 *   melody               optional music, one note per line:  start length pitch voice
 *   0 12 63 0              start/length in 1/24 beats, MIDI pitch, voice 0..3 (3 = quiet backing)
 * A file with only "bpm" and pattern lines (the first version of the format) is still valid: it becomes one
 * 3-lane dodge chart named "normal" with 1 step per beat. Version 2 files (no letters, no "catch") are valid
 * too. The parser is strict: one bad line rejects the whole file and leaves the output untouched. */
/* clang-format on */
typedef enum bd_ChartKind { BD_CHART_DODGE, BD_CHART_CATCH } bd_ChartKind;

typedef struct bd_Chart {
    char name[12];
    bd_ChartKind kind;
    int lanes;
    int length;                 /* steps */
    int8_t lane[BD_MAX_STEPS];  /* -1 = rest, otherwise the lane of the bead */
    uint8_t link[BD_MAX_STEPS]; /* 1 = this bead continues the object of the previous bead (a ribbon) */
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
int bd_chart_beads(const bd_Chart *chart);         /* all beads */
int bd_chart_objects(const bd_Chart *chart);       /* beads that start an object (ribbons and lone beads) */
/* First step after `step` that holds a bead, or chart->length when there is none. */
int bd_chart_next_bead(const bd_Chart *chart, int step);
/* true when the bead at `step` belongs to an object of two or more beads (it continues one or is continued).
 */
bool bd_chart_in_ribbon(const bd_Chart *chart, int step);
/* The chart step a melody note belongs to: its start (1/24 beat) rounded to the nearest step, halves going to
 * the even step like Python's round(). The converter, the metrics and the audio all use this ONE function, so
 * a note and the bead made from it always agree. */
int bd_note_step(int start, int steps_per_beat);
float bd_chart_multiplier(const char *name);           /* score multiplier: easy 1.0, normal 1.5, hard 2.0 */
float bd_song_seconds(const bd_Song *song, int chart); /* time at which the chart is finished */
/* Converts the melody to synth notes shifted by `delay` seconds; returns how many were written (<=
 * BD_MAX_NOTES). A note whose 16th step holds a bead in `chart` is LEAD (voice 0, the bright lead: the player
 * sees what it hears); every other note is BACKING on the quiet voice 3. If two notes share a step, both are
 * lead. */
int bd_song_to_synth(const bd_Song *song, int chart, float delay, SynthNote *out);

/* Charts of one kind: how many a song has, and the index of the nth one (-1 when there is none). */
int bd_song_kind_count(const bd_Song *song, bd_ChartKind kind);
int bd_song_kind_chart(const bd_Song *song, bd_ChartKind kind, int nth);

/* ---- Reachable-lane check: the playability oracle. Model a human who needs BD_HUMAN_STEP_SEC per lane step
 * (arrow-key taps; the number keys are a bonus we do not rely on). reach[] = the lanes the player could be
 * standing in. Between two beads it grows by floor(dt / BD_HUMAN_STEP_SEC) lanes each way. At a bead: dodge
 * removes the bead's lane; catch keeps only the lanes within the tolerance of the bead's object
 * (bd_catch_tolerance: 0 for an object that stays in one lane, 1 for one that changes lane). A chart is
 * playable if and only if reach never becomes empty. */
typedef struct bd_Reach {
    bool lane[BD_MAX_LANES];
    int lanes;
} bd_Reach;

/* The lanes between the first and the last lane the object that holds the bead of `step` touches: 0 for a
 * lone bead or a ribbon that stays in one lane, 2 for a ribbon that visits lanes 0, 1 and 2. */
int bd_chart_object_span(const bd_Chart *chart, int step);
/* Beat Catch: how many lanes away from a bead the player may stand and still catch it, for an object of the
 * given span. An object that stays in one lane needs exactly that lane (0). An object that changes lane is
 * caught from its middle (1): nobody can follow lane changes 80 ms apart. The reach check and the rules of
 * Beat Catch both use this ONE function. */
int bd_catch_tolerance(int span);
void bd_reach_start(bd_Reach *r, int lanes); /* the player starts in the middle lane */
int bd_reach_steps(float seconds);           /* lane steps a human manages in `seconds` */
void bd_reach_advance(bd_Reach *r, float seconds);
void bd_reach_bead(bd_Reach *r, bd_ChartKind kind, int bead_lane, int tolerance);
bool bd_reach_empty(const bd_Reach *r);
/* Walks the chart in arrival order. false = unplayable; *fail_step (may be NULL) is the step of the bead that
 * empties reach. */
bool bd_chart_playable(const bd_Song *song, int chart, int *fail_step);
/* A path a human-speed player can follow: plan[step] = the lane to stand in when the bead of `step` is judged
 * (-1 for steps without a bead). plan needs BD_MAX_STEPS entries. false when the chart is unplayable. Tests
 * use it to build a bot that taps one lane step per BD_HUMAN_STEP_SEC. */
bool bd_chart_plan(const bd_Song *song, int chart, int8_t *plan);

/* The songs listed in <dir>/songs.txt (one file name per line, relative to dir). Missing/invalid files are
 * skipped. */
typedef struct bd_SongList {
    int count;
    int skipped;
    bd_Song songs[BD_MAX_SONGS];
} bd_SongList;
int bd_songlist_load(const char *dir, bd_SongList *out);

/* Song selection screen logic: Up/Down = song, Left/Right = difficulty, action = start. The menu of a game
 * mode only offers the songs and charts of its kind (Beat Dodge: dodge charts, Beat Catch: catch charts).
 * m->chart is an index into song->charts. */
typedef struct bd_Menu {
    int song, chart;
    bool chosen;
    bd_ChartKind kind;
} bd_Menu;
void bd_menu_init(bd_Menu *m, bd_ChartKind kind);
void bd_menu_step(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list);
/* Moves to the previous (dir = -1) or next (dir = +1) song that has a chart of the menu's kind and picks its
 * default chart (the second one, "normal", when there is more than one). Used by keys, wheel and mouse. */
void bd_menu_move_song(bd_Menu *m, const bd_SongList *list, int dir);
/* Selects the chart `chart` of the current song when it is of the menu's kind. */
void bd_menu_pick_chart(bd_Menu *m, const bd_SongList *list, int chart);

/* The part of a run that does not depend on the rules of a game mode: the clock, the beat, the lane the
 * player stands in and the chart being played. Beat Dodge embeds it in beat_dodge_State and Beat Catch embeds
 * the same struct, so both games share one clock and one definition of "when does a bead arrive". */
typedef struct bd_Run {
    bd_Song song; /* private copy of the song being played (~34 KB: keep the owner `static`) */
    int chart;    /* index of the chart being played */
    int lanes, length;
    float time;      /* seconds since the start; the first bead needs `lead` seconds to fall */
    float spb;       /* seconds per beat */
    float step_sec;  /* seconds per chart step */
    float lead;      /* fall time of a bead */
    float end_time;  /* the song is won when time reaches this */
    int next_hit;    /* first step whose bead has not yet reached the player line */
    int last_beat;   /* beat number seen in the previous tick */
    bool beat_event; /* true for the one tick in which a new beat begins */
    int lane;        /* player lane */
    float last_in_lane[BD_MAX_LANES]; /* the time the player last stood in each lane (-1000 = never) */
    float hit_flash;                  /* seconds of red flash after a hit or a miss */
    bool over, won;
} bd_Run;

void bd_run_start(bd_Run *r, const bd_Song *song,
                  int chart); /* (re)start: copies the song, resets the clock */
/* One tick of the shared part: time, beat event, flash timer, lane keys. Does nothing once the run is over.
 */
void bd_run_tick(bd_Run *r, const ArcadeInput *in, float dt);
/* Follow an external clock (the music): only corrects drifts larger than 0.15 s so small offsets never
 * jitter. */
void bd_run_sync(bd_Run *r, float clock_seconds);
float bd_run_arrival(const bd_Run *r, int step); /* when the bead of `step` reaches the player line */
/* Vertical pixel position of the bead of `step` (for drawing); < BD_TOP_Y means "not spawned yet". */
float bd_run_obstacle_y(const bd_Run *r, int step);

/* Beat Dodge rules. Every bead is judged when it reaches the player line (same lane = hit). An object (a
 * ribbon or a lone bead) costs at most ONE life: after the first hit inside it the rest of its beads are
 * ignored. An object that ends without a hit scores 10 points, or 20 when it was a CLOSE CALL: the player
 * stood in one of the lanes the object touches within BD_CLOSE_WINDOW_SEC before its first bead arrived.
 * Surviving the song adds 50 per life left. There is no combo. */
typedef struct beat_dodge_State {
    bd_Run run;
    int lives, dodged, hits, close_calls; /* dodged = objects survived, hits = lives lost */
    int points, max_points;
    float multiplier;
    bool obj_hit;      /* the object being judged has already cost a life */
    bool obj_close;    /* the object being judged is a close call */
    float close_flash; /* seconds left of the "CLOSE!" message */
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
