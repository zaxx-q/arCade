/* MIDI -> Beat Dodge / Beat Catch song conversion (pure C; used by tools/midi2beatmap.c and unit-tested).
 *
 * What a MIDI file gives us: note TIMES and PITCHES (the Friday Night Funkin' MIDI packs hold each
 * character's melody, not the arrow lanes). So lanes are DERIVED from pitch: every distinct pitch is placed
 * on a lane by its rank in the song's pitch range (low notes left, high notes right, each lane used about
 * equally). Six charts come out: easy / normal / hard for Beat Dodge (both voices) and easy / normal / hard
 * for Beat Catch (the player voice).
 *
 * How a chart is made:
 *   1. notes at most BD_RIBBON_GAP_SEC apart form a chain; a chain longer than BD_RIBBON_MAX_SEC is split at
 * its largest inner gap. Every piece is a RIBBON, so every rapid run of sounds has a visible counterpart.
 *   2. isolated notes are thinned by the preset (grid and minimum gap); ribbons are always kept.
 *   3. a ribbon may touch only `span_cap` lanes: the extra lanes are folded into the nearest allowed lane.
 *   4. validate and repair: the objects are walked in order through the reachable-lane check
 *      (bd_chart_playable). A ribbon that would leave the player nowhere to stand is folded into a narrower
 *      window of lanes; a lone note that would do the same is dropped. The check is never loosened.
 *   Folding also works against camping: a window that leaves a long-waiting lane untouched is penalised. */
#ifndef BEAT_DODGE_CONVERT_H
#define BEAT_DODGE_CONVERT_H

#include "arcade/midi.h"
#include "beat_dodge_logic.h"

#define BD_RIBBON_GAP_SEC                                                                                    \
    0.25f /* notes at most this far apart are one run (also the "burst" of the metrics) */
#define BD_RIBBON_MAX_SEC 1.2f /* the longest a ribbon may be, first bead to last */

typedef struct bd_ConvertStats {
    int midi_notes;              /* notes in all tracks */
    int melody_notes;            /* notes written to the song's melody */
    int beads[BD_MAX_CHARTS];    /* beads per chart */
    int objects[BD_MAX_CHARTS];  /* ribbons and lone beads per chart */
    int ribbons[BD_MAX_CHARTS];  /* objects of two or more beads */
    int folded[BD_MAX_CHARTS];   /* ribbons that touched more lanes than the span cap allows */
    int repaired[BD_MAX_CHARTS]; /* ribbons the reachable-lane check had to move to another window */
    int dropped[BD_MAX_CHARTS];  /* lone beads the reachable-lane check removed */
    int lost[BD_MAX_CHARTS];     /* ribbons that could not be placed at all (a bug when not 0) */
    float per_second[BD_MAX_CHARTS];
    int lanes[BD_MAX_CHARTS];
    char player_track[40]; /* the track that plays the "player" voice */
} bd_ConvertStats;

/* player_track: part of a track name (case-insensitive); NULL = "boyfriend"/"bf", else the last track.
 * Returns false and fills err when the song cannot be converted (too long, too many notes, ...). */
bool bd_convert_midi(const MidiSong *midi, const char *title, const char *player_track, bd_Song *out,
                     bd_ConvertStats *stats, char *err, size_t err_size);

/* ---- measurements (Appendix A of the Beat Dodge plan): how a chart compares with the melody it was made
 * from. Used by `midi2beatmap --report/--metrics` and by tests, never by the game itself. */
typedef struct bd_ChartMetrics {
    int beads;
    float per_second;      /* beads divided by the time of the last bead */
    float covered;         /* share of heard onsets whose 16th step holds a bead */
    int bursts;            /* bursts of 3 or more onsets, each at most 0.25 s after the previous one */
    int bursts_mismatched; /* of those, bursts in which at most one step holds a bead */
    float same_lane;       /* share of consecutive beads in the same lane */
    float lazy_idle;       /* share of beads a lazy bot dodges without moving */
    float empty5, empty10; /* share of 5 s / 10 s windows in which some lane stays empty */
    float longest_idle;    /* anti-camping: the longest time (s) any one lane goes without a bead */
    bool playable;         /* passes the reachable-lane check (bd_chart_playable) */
    int fail_step;         /* where it fails, -1 when playable */
} bd_ChartMetrics;

void bd_chart_metrics(const bd_Song *song, int chart, bd_ChartMetrics *out);
/* Share of beats that hold notes of both voices (0 when the melody is empty). */
float bd_song_voice_overlap(const bd_Song *song);

#endif
