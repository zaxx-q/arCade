#include "beat_dodge_convert.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

#define STEPS_PER_BEAT 4 /* 16th notes: nearly every FNF note sits exactly on this grid */

typedef struct Ev {
    uint32_t tick, dur;
    uint8_t pitch, voice;
    bool player;
} Ev;

static Ev g_events[MIDI_MAX_TRACKS *
                   MIDI_MAX_NOTES];  /* ~260 KB: static, this code is only used by a tool and tests */
static Ev g_steps[BD_MAX_STEPS * 2]; /* one event per step after de-duplication */
static Ev g_kept[BD_MAX_STEPS];      /* the events kept for the chart being built */

/* difficulty presets: how many lanes, which steps may hold an obstacle, and the minimum time between
 * obstacles */
static const struct {
    const char *name;
    int lanes;
    int grid; /* 1 = any 16th, 2 = only 8th-note positions, 4 = only quarter-note positions */
    float min_gap;
} k_preset[BD_MAX_CHARTS] = {
    {"easy", 3, 4, 0.55f},
    {"normal", 3, 2, 0.45f},
    {"hard", 4, 1, 0.30f},
};

static bool contains_nocase(const char *hay, const char *needle) {
    return arc_contains_nocase(hay, needle);
}

static int cmp_events(const void *a, const void *b) {
    const Ev *x = (const Ev *)a, *y = (const Ev *)b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    if (x->player != y->player)
        return x->player ? -1 : 1; /* on a tie the player's note wins the step */
    return (int)y->pitch - (int)x->pitch;
}

static bool fail(char *err, size_t n, const char *msg) {
    snprintf(err, n, "%s", msg);
    return false;
}

/* Splits the m pitches (sorted low to high, cnt[i] notes each) into `lanes` groups of neighbouring pitches so
 * that the busiest group is as small as possible (the classic "linear partition" problem, solved with dynamic
 * programming). group_of[i] = lane of pitch i. Needs m >= lanes. */
static void partition_pitches(const int *cnt, int m, int lanes, int *group_of) {
    int pre[129], best[BD_MAX_LANES + 1][129], cut[BD_MAX_LANES + 1][129];
    pre[0] = 0;
    for (int i = 0; i < m; i++)
        pre[i + 1] = pre[i] + cnt[i];
    for (int i = 1; i <= m; i++)
        best[1][i] = pre[i];
    for (int k = 2; k <= lanes; k++)
        for (int i = k; i <= m; i++) {
            best[k][i] = 1 << 30;
            for (int j = k - 1; j < i; j++) { /* last group = pitches j..i-1 */
                int worst = arc_maxi(best[k - 1][j], pre[i] - pre[j]);
                if (worst < best[k][i]) {
                    best[k][i] = worst;
                    cut[k][i] = j;
                }
            }
        }
    for (int k = lanes, i = m; k >= 1; k--) { /* walk back through the cut points */
        int j = k == 1 ? 0 : cut[k][i];
        for (int t = j; t < i; t++)
            group_of[t] = k - 1;
        i = j;
    }
}

/* Gives every distinct pitch a lane: low pitches left, high pitches right, lanes used about equally often. */
static void build_lane_map(const Ev *ev, int n, int lanes, int *lane_of) {
    int count[128] = {0}, pitches[128], cnt[128], group_of[128], m = 0;
    for (int i = 0; i < n; i++)
        count[ev[i].pitch & 127]++;
    for (int p = 0; p < 128; p++) {
        lane_of[p] = 0;
        if (count[p] > 0) {
            pitches[m] = p;
            cnt[m++] = count[p];
        }
    }
    if (m <= lanes) { /* few pitches: spread them over the whole width */
        for (int i = 0; i < m; i++)
            lane_of[pitches[i]] = m == 1 ? lanes / 2 : (i * (lanes - 1) + (m - 1) / 2) / (m - 1);
        return;
    }
    partition_pitches(cnt, m, lanes, group_of);
    for (int i = 0; i < m; i++)
        lane_of[pitches[i]] = group_of[i];
}

bool bd_convert_midi(const MidiSong *midi, const char *title, const char *player_track, bd_Song *out,
                     bd_ConvertStats *stats, char *err, size_t err_size) {
    memset(out, 0, sizeof(*out));
    memset(stats, 0, sizeof(*stats));
    if (midi->track_count == 0 || midi->ticks_per_beat <= 0)
        return fail(err, err_size, "the MIDI has no notes");
    int tpb = midi->ticks_per_beat;

    /* 1. which track is the player? */
    int player = midi->track_count - 1;
    bool found = false;
    for (int t = 0; t < midi->track_count && !found; t++) {
        const char *name = midi->tracks[t].name;
        if (player_track && player_track[0]) {
            if (contains_nocase(name, player_track)) {
                player = t;
                found = true;
            }
        } else if (contains_nocase(name, "boyfriend") || arc_stricmp(name, "bf") == 0) {
            player = t;
            found = true;
        }
    }
    if (player_track && player_track[0] && !found)
        return fail(err, err_size, "no track name contains the requested --player text");
    arc_strlcpy(stats->player_track, midi->tracks[player].name, sizeof(stats->player_track));

    /* 2. collect and sort all notes */
    int n = 0;
    for (int t = 0; t < midi->track_count; t++)
        for (int i = 0; i < midi->tracks[t].note_count; i++) {
            const MidiNote *m = &midi->tracks[t].notes[i];
            g_events[n].tick = m->start;
            g_events[n].dur = m->dur;
            g_events[n].pitch = m->pitch;
            g_events[n].player = t == player;
            g_events[n].voice = t == player ? 1 : 0;
            n++;
        }
    qsort(g_events, (size_t)n, sizeof(Ev), cmp_events);
    stats->midi_notes = n;

    /* 3. song header and melody (time unit: 1/24 beat) */
    arc_strlcpy(out->title, (title && title[0]) ? title : "Untitled", sizeof(out->title));
    out->bpm = (int)lroundf(midi->bpm);
    out->steps = STEPS_PER_BEAT;
    if (out->bpm < 60 || out->bpm > 220)
        return fail(err, err_size, "tempo outside 60..220 BPM");
    if (n > BD_MAX_NOTES)
        return fail(err, err_size, "too many notes for one song (raise BD_MAX_NOTES)");
    for (int i = 0; i < n; i++) {
        uint32_t start =
            ((uint32_t)g_events[i].tick * BD_CLOCKS_PER_BEAT + (uint32_t)tpb / 2u) / (uint32_t)tpb;
        uint32_t len = ((uint32_t)g_events[i].dur * BD_CLOCKS_PER_BEAT + (uint32_t)tpb / 2u) / (uint32_t)tpb;
        if (start > 65535u)
            return fail(err, err_size, "song too long");
        out->notes[i].start = (uint16_t)start;
        out->notes[i].len = (uint16_t)(len < 1u ? 1u : (len > 65535u ? 65535u : len));
        out->notes[i].pitch = g_events[i].pitch;
        out->notes[i].voice = g_events[i].voice;
    }
    out->note_count = n;
    stats->melody_notes = n;

    /* 4. one candidate obstacle per 16th-note step */
    int steps_n = 0, last_step = -1;
    for (int i = 0; i < n; i++) {
        int step =
            (int)(((uint64_t)g_events[i].tick * STEPS_PER_BEAT * 2u + (uint64_t)tpb) / (2u * (uint64_t)tpb));
        if (step >= BD_MAX_STEPS)
            return fail(err, err_size, "song too long for a chart (raise BD_MAX_STEPS)");
        if (step == last_step) { /* two notes round to the same step: the player's note wins */
            if (g_events[i].player && !g_steps[steps_n - 1].player) {
                g_steps[steps_n - 1] = g_events[i];
                g_steps[steps_n - 1].tick = (uint32_t)step;
            }
            continue;
        }
        g_steps[steps_n] = g_events[i];
        g_steps[steps_n].tick = (uint32_t)step; /* reuse the field: now a step index */
        steps_n++;
        last_step = step;
    }

    /* 5. one chart per difficulty preset */
    float step_sec = 60.0f / (float)out->bpm / (float)STEPS_PER_BEAT;
    out->chart_count = BD_MAX_CHARTS;
    for (int c = 0; c < BD_MAX_CHARTS; c++) {
        bd_Chart *chart = &out->charts[c];
        arc_strlcpy(chart->name, k_preset[c].name, sizeof(chart->name));
        chart->lanes = k_preset[c].lanes;
        int gap = (int)ceilf(k_preset[c].min_gap / step_sec);
        int last_kept = -1000000, max_step = 0, kept_n = 0;
        for (int i = 0; i < BD_MAX_STEPS; i++)
            chart->lane[i] = -1;
        /* pass 1: choose which notes become obstacles (right grid position, far enough from the previous one)
         */
        for (int i = 0; i < steps_n; i++) {
            int step = (int)g_steps[i].tick;
            if (step % k_preset[c].grid != 0 || step - last_kept < gap)
                continue;
            g_kept[kept_n++] = g_steps[i];
            last_kept = step;
            max_step = step;
        }
        /* pass 2: lanes from pitch, balanced over the notes that were actually kept in THIS chart */
        int lane_of[128];
        if (kept_n > 0)
            build_lane_map(g_kept, kept_n, chart->lanes, lane_of);
        for (int i = 0; i < kept_n; i++)
            chart->lane[g_kept[i].tick] = (int8_t)lane_of[g_kept[i].pitch & 127];
        chart->length = arc_maxi(8, max_step + 1);
        stats->obstacles[c] = bd_chart_obstacles(chart);
        stats->lanes[c] = chart->lanes;
        stats->per_second[c] = (float)stats->obstacles[c] /
                               (bd_song_seconds(out, c) - (BD_LEAD_BEATS + 1.0f) * 60.0f / (float)out->bpm);
    }
    return true;
}
