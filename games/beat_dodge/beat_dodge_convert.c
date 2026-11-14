#include "beat_dodge_convert.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

#define STEPS_PER_BEAT 4 /* 16th notes: nearly every FNF note sits exactly on this grid */
#define MAX_RIBBON 64    /* beads in one ribbon: 1.2 s of 16th notes at 220 BPM is 18, so this never limits */
/* Folding a ribbon into the lanes its span cap allows can be done in several ways (which window of lanes?).
 * The window with the lowest  (lane moves per bead) + FOLD_STARVE_WEIGHT * (seconds a lane the ribbon does
 * NOT touch has already been waiting for a bead, beyond FOLD_STARVE_SEC)  wins. So a lane nobody has been
 * asked to visit for a while pulls the next ribbon towards it instead of staying free again and again
 * (camping). */
#define FOLD_STARVE_SEC 3.0f
#define FOLD_STARVE_WEIGHT 0.5f

typedef struct Ev {
    uint32_t tick, dur;
    uint8_t pitch;
    bool player;
} Ev;

/* One note per chart step, after rounding. */
typedef struct StepEv {
    int step;
    uint8_t pitch;
} StepEv;

/* A run of `count` neighbouring notes (g_*[first .. first+count-1]); count >= 2 is a ribbon. */
typedef struct Obj {
    int first, count;
    bool alive; /* a lone note thinned away by the preset is not alive; ribbons always are */
} Obj;

static Ev g_events[MIDI_MAX_TRACKS *
                   MIDI_MAX_NOTES];   /* ~260 KB: static, this code is only used by a tool and tests */
static StepEv g_all[BD_MAX_STEPS];    /* dodge charts: both voices, the player's note wins a shared step */
static StepEv g_voice1[BD_MAX_STEPS]; /* catch charts: the player voice only */
static Obj g_obj[BD_MAX_STEPS];       /* the objects of the chart being built */
static int g_lane[BD_MAX_STEPS];      /* lane (from pitch) of every note of the chart being built */
static StepEv g_kept[BD_MAX_STEPS];   /* the notes that stay, to balance the lanes over */

/* difficulty presets, 3 dodge charts then 3 catch charts. grid / min_gap thin the LONE notes only (ribbons
 * are what makes every sound visible, so they are always kept); span_cap = most lanes one ribbon may touch.
 * How close two objects may be is finally decided by the reachable-lane check, not by min_gap (catch hard has
 * none).
 */
#define PRESETS 6
typedef struct Preset {
    bd_ChartKind kind;
    const char *name;
    int lanes;
    int grid; /* 1 = any 16th, 2 = only 8th-note positions, 4 = only quarter-note positions */
    float min_gap;
    int span_cap;
} Preset;
static const Preset k_preset[PRESETS] = {
    {BD_CHART_DODGE, "easy", 3, 4, 0.55f, 1},   {BD_CHART_DODGE, "normal", 3, 2, 0.45f, 2},
    {BD_CHART_DODGE, "hard", 4, 1, 0.30f, 3},   {BD_CHART_CATCH, "easy", 3, 4, 0.55f, 1},
    {BD_CHART_CATCH, "normal", 4, 2, 0.40f, 2}, {BD_CHART_CATCH, "hard", 4, 1, 0.0f, 3},
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
 * that the lanes are used about equally often: the sum of the squared group sizes is as small as possible
 * (dynamic programming, the "linear partition" problem). Minimising only the busiest group is not enough:
 * when one pitch alone fills a lane, the other groups may be left almost empty and a lane goes unused for
 * minutes. group_of[i] = lane of pitch i. Needs m >= lanes. */
static void partition_pitches(const int *cnt, int m, int lanes, int *group_of) {
    int pre[129], best[BD_MAX_LANES + 1][129], cut[BD_MAX_LANES + 1][129];
    pre[0] = 0;
    for (int i = 0; i < m; i++)
        pre[i + 1] = pre[i] + cnt[i];
    for (int i = 1; i <= m; i++)
        best[1][i] = pre[i] * pre[i];
    for (int k = 2; k <= lanes; k++)
        for (int i = k; i <= m; i++) {
            best[k][i] = 1 << 30;
            for (int j = k - 1; j < i; j++) { /* last group = pitches j..i-1 */
                int size = pre[i] - pre[j];
                int total = best[k - 1][j] + size * size;
                if (total < best[k][i]) {
                    best[k][i] = total;
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
static void build_lane_map(const StepEv *ev, int n, int lanes, int *lane_of) {
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

/* "Rapid" = the gap to the previous note is short enough for them to be one run. Defined in seconds, never in
 * steps: 16th notes at 120 BPM and 8th notes at 190 BPM are both rapid. The metrics use the SAME test for
 * their bursts, so every burst is a ribbon by construction. */
static bool is_rapid(int step_gap, float step_sec) {
    return (float)step_gap * step_sec <= BD_RIBBON_GAP_SEC + 0.0001f;
}

/* Cuts the chain ev[first..last] (every neighbouring gap is rapid) into ribbons of at most BD_RIBBON_MAX_SEC.
 * A too long chain is split at its LARGEST inner gap, leaving at least two notes on each side so that no
 * piece is a lone note (a chain that is too long has 6 or more notes, so such a gap always exists). The
 * pieces are split again until they are short enough (recursion). Appends the ribbons to g_obj. */
static void split_chain(const StepEv *ev, int first, int last, float step_sec, int *obj_n) {
    int count = last - first + 1;
    if ((float)(ev[last].step - ev[first].step) * step_sec <= BD_RIBBON_MAX_SEC + 0.0001f) {
        g_obj[*obj_n].first = first;
        g_obj[*obj_n].count = count;
        g_obj[*obj_n].alive = true;
        (*obj_n)++;
        return;
    }
    int mid2 = ev[first].step + ev[last].step; /* twice the middle step */
    int best = first + 1;
    for (int g = first + 1; g <= last - 2; g++) { /* the gap between note g and note g + 1 */
        int gap = ev[g + 1].step - ev[g].step, best_gap = ev[best + 1].step - ev[best].step;
        int off = abs(ev[g].step + ev[g + 1].step - mid2),
            best_off = abs(ev[best].step + ev[best + 1].step - mid2);
        if (gap > best_gap || (gap == best_gap && off < best_off))
            best = g; /* larger gap wins; on a tie the one nearer the middle (balanced pieces) */
    }
    split_chain(ev, first, best, step_sec, obj_n);
    split_chain(ev, best + 1, last, step_sec, obj_n);
}

/* Everything the placement of one chart needs: the reachable lanes after the last placed object, and when
 * each lane was last touched (for the anti-camping tie-break of the fold). */
typedef struct Build {
    const Preset *p;
    const StepEv *ev;
    float step_sec, lead;
    bd_Chart *chart;
    bd_Reach reach;
    float before; /* time of the last placed bead */
    float last_touch[BD_MAX_LANES];
} Build;

static float bead_time(const Build *b, int step) {
    return b->lead + (float)step * b->step_sec; /* the same expression as bd_chart_playable */
}

/* Walks the `count` notes starting at ev[first] through the reachable-lane check with the given lanes. false
 * when the player could not be anywhere safe (b is left untouched). With commit, true also writes the beads
 * into the chart and updates the state. */
static bool try_object(Build *b, int first, int count, const int *lanes, bool commit) {
    bd_Reach r = b->reach;
    float before = b->before;
    int lo = lanes[0], hi = lanes[0];
    for (int j = 1; j < count; j++) { /* the span of the object decides the catch tolerance */
        lo = arc_mini(lo, lanes[j]);
        hi = arc_maxi(hi, lanes[j]);
    }
    for (int j = 0; j < count; j++) {
        float t = bead_time(b, b->ev[first + j].step);
        bd_reach_advance(&r, t - before);
        before = t;
        bd_reach_bead(&r, b->p->kind, lanes[j], bd_catch_tolerance(hi - lo));
        if (bd_reach_empty(&r))
            return false;
    }
    if (!commit)
        return true;
    b->reach = r;
    b->before = before;
    for (int j = 0; j < count; j++) {
        int step = b->ev[first + j].step;
        b->chart->lane[step] = (int8_t)lanes[j];
        b->chart->link[step] = j > 0 ? 1 : 0;
        b->last_touch[lanes[j]] = bead_time(b, step);
    }
    return true;
}

/* One way to squeeze a ribbon into `size` neighbouring lanes starting at lane `start`: every bead outside the
 * window moves to the nearest lane inside it. cost = how many lanes the beads moved in total. */
typedef struct Cand {
    int size, start, cost;
    float score; /* lower is better */
} Cand;

static bool cand_better(const Cand *a, const Cand *c) {
    if (a->size != c->size)
        return a->size > c->size; /* keep as many lanes as the cap allows */
    if (a->score != c->score)
        return a->score < c->score;
    return a->start < c->start;
}

/* Places one ribbon. The windows are tried best first; the first one the reachable-lane check accepts wins. A
 * window of one lane is always accepted (the player can stand in any lane that is not the ribbon's, in a
 * dodge chart, or in the ribbon's lane, in a catch chart), so a ribbon is never dropped. */
static void place_ribbon(Build *b, const Obj *o, bd_ConvertStats *st, int chart_index) {
    int natural[MAX_RIBBON], mapped[MAX_RIBBON], lo = BD_MAX_LANES, hi = -1;
    int lanes = b->p->lanes, cap = arc_mini(b->p->span_cap, lanes);
    for (int j = 0; j < o->count; j++) {
        natural[j] = g_lane[o->first + j];
        lo = arc_mini(lo, natural[j]);
        hi = arc_maxi(hi, natural[j]);
    }
    float t_first = bead_time(b, b->ev[o->first].step);
    Cand cand[16];
    int nc = 0;
    for (int size = cap; size >= 1; size--)
        for (int w = 0; w + size <= lanes; w++) {
            Cand *c = &cand[nc++];
            bool touched[BD_MAX_LANES] = {false};
            c->size = size;
            c->start = w;
            c->cost = 0;
            for (int j = 0; j < o->count; j++) {
                int moved = arc_clampi(natural[j], w, w + size - 1);
                c->cost += abs(natural[j] - moved);
                touched[moved] = true;
            }
            float starving = 0.0f; /* lanes this ribbon leaves alone although they have waited a long time */
            for (int l = 0; l < lanes; l++)
                if (!touched[l])
                    starving += arc_maxf(0.0f, t_first - b->last_touch[l] - FOLD_STARVE_SEC);
            c->score = (float)c->cost / (float)o->count + FOLD_STARVE_WEIGHT * starving;
        }
    for (int i = 1; i < nc; i++) { /* insertion sort: at most 15 candidates */
        Cand key = cand[i];
        int k = i - 1;
        while (k >= 0 && cand_better(&key, &cand[k])) {
            cand[k + 1] = cand[k];
            k--;
        }
        cand[k + 1] = key;
    }
    if (hi - lo + 1 > cap)
        st->folded[chart_index]++;
    for (int i = 0; i < nc; i++) {
        for (int j = 0; j < o->count; j++)
            mapped[j] = arc_clampi(natural[j], cand[i].start, cand[i].start + cand[i].size - 1);
        if (!try_object(b, o->first, o->count, mapped, true))
            continue;
        if (i > 0)
            st->repaired[chart_index]++;
        return;
    }
    /* cannot happen (see above); keep the counter honest if it ever does */
    st->lost[chart_index]++;
}

/* Builds one chart from the notes ev[0..n) (both voices for a dodge chart, the player voice for a catch
 * chart). */
static void build_chart(bd_Song *song, int chart_index, const StepEv *ev, int n, const Preset *p,
                        float step_sec, float lead, bd_ConvertStats *st) {
    bd_Chart *chart = &song->charts[chart_index];
    arc_strlcpy(chart->name, p->name, sizeof(chart->name));
    chart->kind = p->kind;
    chart->lanes = p->lanes;
    for (int i = 0; i < BD_MAX_STEPS; i++) {
        chart->lane[i] = -1;
        chart->link[i] = 0;
    }

    /* 1. chains of rapid notes become ribbons; every other note is an object of its own */
    int obj_n = 0;
    for (int i = 0; i < n;) {
        int j = i;
        while (j + 1 < n && is_rapid(ev[j + 1].step - ev[j].step, step_sec))
            j++;
        if (j == i) {
            g_obj[obj_n].first = i;
            g_obj[obj_n].count = 1;
            g_obj[obj_n].alive = true;
            obj_n++;
        } else {
            split_chain(ev, i, j, step_sec, &obj_n);
        }
        i = j + 1;
    }

    /* 2. thin the lone notes: right grid position, far enough from the previous object and from a ribbon that
     * follows (ribbons are never given up) */
    int gap = (int)ceilf(p->min_gap / step_sec), last_kept = -1000000, kept_n = 0;
    for (int k = 0; k < obj_n; k++) {
        Obj *o = &g_obj[k];
        int step = ev[o->first].step, last_step = ev[o->first + o->count - 1].step;
        if (o->count == 1) {
            bool next_is_ribbon = k + 1 < obj_n && g_obj[k + 1].count >= 2;
            bool too_close_ahead = next_is_ribbon && ev[g_obj[k + 1].first].step - step < gap;
            o->alive = step % p->grid == 0 && step - last_kept >= gap && !too_close_ahead;
        }
        if (!o->alive)
            continue;
        last_kept = last_step;
        for (int j = 0; j < o->count; j++)
            g_kept[kept_n++] = ev[o->first + j];
    }

    /* 3. lanes from pitch, balanced over the notes that stay in THIS chart */
    int lane_of[128] = {0};
    if (kept_n > 0)
        build_lane_map(g_kept, kept_n, p->lanes, lane_of);
    for (int i = 0; i < n; i++)
        g_lane[i] = lane_of[ev[i].pitch & 127];

    /* 4. place the objects in order, asking the reachable-lane check about each one */
    Build b = {.p = p, .ev = ev, .step_sec = step_sec, .lead = lead, .chart = chart};
    bd_reach_start(&b.reach, p->lanes);
    for (int l = 0; l < BD_MAX_LANES; l++)
        b.last_touch[l] = 0.0f;
    for (int k = 0; k < obj_n; k++) {
        const Obj *o = &g_obj[k];
        if (!o->alive)
            continue;
        if (o->count >= 2) {
            st->ribbons[chart_index]++;
            place_ribbon(&b, o, st, chart_index);
        } else if (!try_object(&b, o->first, 1, &g_lane[o->first], true)) {
            st->dropped[chart_index]++; /* a lone note nobody could reach: drop it, never loosen the check */
        }
    }
    int max_step = 0;
    for (int i = 0; i < BD_MAX_STEPS; i++)
        if (chart->lane[i] >= 0)
            max_step = i;
    chart->length = arc_maxi(8, max_step + 1);
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
        out->notes[i].voice = g_events[i].player ? 1 : 0;
    }
    out->note_count = n;
    stats->melody_notes = n;

    /* 4. one note per 16th-note step. The step comes from bd_note_step, the function the metrics and the
     * audio use too, so a note and the bead made from it always agree. */
    int all_n = 0, voice1_n = 0;
    bool all_last_player = false;
    for (int i = 0; i < n; i++) {
        int step = bd_note_step(out->notes[i].start, STEPS_PER_BEAT);
        if (step >= BD_MAX_STEPS)
            return fail(err, err_size, "song too long for a chart (raise BD_MAX_STEPS)");
        if (all_n > 0 && g_all[all_n - 1].step == step) { /* two notes share a step: the player's note wins */
            if (g_events[i].player && !all_last_player) {
                g_all[all_n - 1].pitch = g_events[i].pitch;
                all_last_player = true;
            }
        } else {
            g_all[all_n].step = step;
            g_all[all_n].pitch = g_events[i].pitch;
            all_n++;
            all_last_player = g_events[i].player;
        }
        if (g_events[i].player && (voice1_n == 0 || g_voice1[voice1_n - 1].step != step)) {
            g_voice1[voice1_n].step = step;
            g_voice1[voice1_n].pitch = g_events[i].pitch;
            voice1_n++;
        }
    }

    /* 5. one chart per preset: three dodge charts (both voices), three catch charts (player voice) */
    float step_sec = 60.0f / (float)out->bpm / (float)STEPS_PER_BEAT;
    float lead = BD_LEAD_BEATS * 60.0f / (float)out->bpm;
    out->chart_count = PRESETS;
    for (int c = 0; c < PRESETS; c++) {
        const Preset *p = &k_preset[c];
        if (p->kind == BD_CHART_DODGE)
            build_chart(out, c, g_all, all_n, p, step_sec, lead, stats);
        else
            build_chart(out, c, g_voice1, voice1_n, p, step_sec, lead, stats);
        if (!bd_chart_playable(out, c, NULL))
            return fail(err, err_size, "internal error: a converted chart fails the reachable-lane check");
        bd_ChartMetrics m;
        bd_chart_metrics(out, c, &m);
        stats->beads[c] = m.beads;
        stats->objects[c] = bd_chart_objects(&out->charts[c]);
        stats->lanes[c] = p->lanes;
        stats->per_second[c] = m.per_second;
    }
    return true;
}

/* ------------------------------------------------------------------ measurements */
static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* Distinct onset steps of the melody, sorted: both voices for a dodge chart, the player voice (1) for a catch
 * chart, because that is the voice a catch chart is made from. */
static int collect_onsets(const bd_Song *song, bd_ChartKind kind, int *out) {
    int n = 0;
    for (int i = 0; i < song->note_count; i++)
        if (kind == BD_CHART_DODGE || song->notes[i].voice == 1)
            out[n++] = bd_note_step(song->notes[i].start, song->steps);
    qsort(out, (size_t)n, sizeof(int), cmp_int);
    int m = 0;
    for (int i = 0; i < n; i++)
        if (m == 0 || out[i] != out[m - 1])
            out[m++] = out[i];
    return m;
}

/* Share of windows of `win` seconds (stride win/4, from t = 0 to the last bead) in which some lane has no
 * bead. */
static float empty_lane_windows(const bd_Chart *c, float step_sec, float win, int last_step) {
    float last = (float)last_step * step_sec, eps = 0.0001f;
    int windows = 0, bad = 0;
    for (int k = 0;; k++) {
        float from = (float)k * win / 4.0f;
        if (from + win > last + eps)
            break;
        bool used[BD_MAX_LANES] = {false};
        for (int i = 0; i <= last_step; i++) {
            float t = (float)i * step_sec;
            if (c->lane[i] >= 0 && t >= from - eps && t <= from + win + eps)
                used[c->lane[i]] = true;
        }
        int n = 0;
        for (int l = 0; l < c->lanes; l++)
            n += used[l] ? 1 : 0;
        windows++;
        bad += n < c->lanes ? 1 : 0;
    }
    return windows > 0 ? (float)bad / (float)windows : 0.0f;
}

/* The longest stretch (seconds) in which one lane has no bead, counted from t = 0 to the last bead. */
static float longest_idle_lane(const bd_Chart *c, float step_sec, int last_step) {
    float since[BD_MAX_LANES] = {0.0f}, longest = 0.0f;
    for (int i = 0; i <= last_step; i++) {
        if (c->lane[i] < 0)
            continue;
        float t = (float)i * step_sec;
        longest = arc_maxf(longest, t - since[c->lane[i]]);
        since[c->lane[i]] = t;
    }
    for (int l = 0; l < c->lanes; l++)
        longest = arc_maxf(longest, (float)last_step * step_sec - since[l]);
    return longest;
}

void bd_chart_metrics(const bd_Song *song, int chart, bd_ChartMetrics *out) {
    memset(out, 0, sizeof(*out));
    const bd_Chart *c = &song->charts[chart];
    float step_sec = 60.0f / (float)song->bpm / (float)song->steps;
    out->fail_step = -1;
    out->playable = bd_chart_playable(song, chart, &out->fail_step);
    int beads = bd_chart_beads(c), last_step = -1, same = 0, prev_lane = -1, pairs = 0;
    out->beads = beads;
    if (beads == 0)
        return;
    int lazy_lane = c->lanes / 2, idle = 0;
    for (int i = 0; i < c->length; i++) {
        int l = c->lane[i];
        if (l < 0)
            continue;
        last_step = i;
        if (prev_lane >= 0) {
            pairs++;
            same += l == prev_lane ? 1 : 0;
        }
        prev_lane = l;
        if (l ==
            lazy_lane) { /* the lazy bot moves only when the bead is in its lane: to the nearest other lane */
            int best = -1;
            for (int k = 0; k < c->lanes; k++)
                if (k != lazy_lane && (best < 0 || abs(k - lazy_lane) < abs(best - lazy_lane)))
                    best = k; /* strict < keeps the lower lane on a tie */
            lazy_lane = best;
        } else {
            idle++;
        }
    }
    out->per_second = last_step > 0 ? (float)beads / ((float)last_step * step_sec) : 0.0f;
    out->same_lane = pairs > 0 ? (float)same / (float)pairs : 0.0f;
    out->lazy_idle = (float)idle / (float)beads;
    out->empty5 = empty_lane_windows(c, step_sec, 5.0f, last_step);
    out->empty10 = empty_lane_windows(c, step_sec, 10.0f, last_step);
    out->longest_idle = longest_idle_lane(c, step_sec, last_step);

    /* onsets and bursts */
    static int onset[BD_MAX_NOTES];
    int n = collect_onsets(song, c->kind, onset), covered = 0, run_start = 0;
    for (int i = 0; i < n; i++) {
        int step = onset[i];
        covered += step < c->length && c->lane[step] >= 0 ? 1 : 0;
        bool last_of_burst = i + 1 == n || !is_rapid(onset[i + 1] - step, step_sec);
        if (!last_of_burst)
            continue;
        if (i - run_start + 1 >= 3) { /* a burst of 3 or more onsets */
            int shown = 0;
            for (int k = run_start; k <= i; k++)
                shown += onset[k] < c->length && c->lane[onset[k]] >= 0 ? 1 : 0;
            out->bursts++;
            out->bursts_mismatched += shown <= 1 ? 1 : 0;
        }
        run_start = i + 1;
    }
    out->covered = n > 0 ? (float)covered / (float)n : 0.0f;
}

float bd_song_voice_overlap(const bd_Song *song) {
    static unsigned char seen[65536 / BD_CLOCKS_PER_BEAT + 2]; /* bit 0 = voice 0, bit 1 = voice 1 */
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < song->note_count; i++)
        if (song->notes[i].voice <= 1)
            seen[song->notes[i].start / BD_CLOCKS_PER_BEAT] |= (unsigned char)(1u << song->notes[i].voice);
    int any = 0, both = 0;
    for (size_t b = 0; b < sizeof(seen); b++) {
        any += seen[b] != 0 ? 1 : 0;
        both += seen[b] == 3 ? 1 : 0;
    }
    return any > 0 ? (float)both / (float)any : 0.0f;
}
