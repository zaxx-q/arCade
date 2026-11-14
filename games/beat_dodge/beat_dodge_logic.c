#include "beat_dodge_logic.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

/* ------------------------------------------------------------------ songs */
void bd_song_default(bd_Song *out, unsigned seed) {
    Rng rng;
    rng_seed(&rng, seed);
    memset(out, 0, sizeof(*out));
    arc_strlcpy(out->title, "Practice", sizeof(out->title));
    out->bpm = 120;
    out->steps = 1;
    out->chart_count = 1;
    bd_Chart *c = &out->charts[0];
    arc_strlcpy(c->name, "normal", sizeof(c->name));
    c->lanes = 3;
    c->length = 64;
    for (int i = 0; i < c->length; i++) {
        bool rest = i < 4 || rng_chance(&rng, 0.35f); /* four steps of warm-up, then ~65% obstacles */
        c->lane[i] = rest ? -1 : (int8_t)rng_range(&rng, 0, c->lanes - 1);
    }
}

int bd_chart_beads(const bd_Chart *chart) {
    int n = 0;
    for (int i = 0; i < chart->length; i++)
        if (chart->lane[i] >= 0)
            n++;
    return n;
}

int bd_chart_objects(const bd_Chart *chart) {
    int n = 0;
    for (int i = 0; i < chart->length; i++)
        if (chart->lane[i] >= 0 && !chart->link[i])
            n++;
    return n;
}

int bd_chart_next_bead(const bd_Chart *chart, int step) {
    int i = step + 1;
    while (i < chart->length && chart->lane[i] < 0)
        i++;
    return i;
}

bool bd_chart_in_ribbon(const bd_Chart *chart, int step) {
    if (chart->lane[step] < 0)
        return false;
    int next = bd_chart_next_bead(chart, step);
    return chart->link[step] || (next < chart->length && chart->link[next]);
}

int bd_note_step(int start, int steps_per_beat) {
    int scaled = start * steps_per_beat; /* in 1/24 of a step */
    int step = scaled / BD_CLOCKS_PER_BEAT, rest = scaled % BD_CLOCKS_PER_BEAT;
    if (rest * 2 > BD_CLOCKS_PER_BEAT || (rest * 2 == BD_CLOCKS_PER_BEAT && step % 2 == 1))
        step++;
    return step;
}

float bd_chart_multiplier(const char *name) {
    if (strcmp(name, "easy") == 0)
        return 1.0f;
    if (strcmp(name, "hard") == 0)
        return 2.0f;
    return 1.5f;
}

/* The song lasts until the later of: the last obstacle, the last melody note (plus a one beat outro). */
float bd_song_seconds(const bd_Song *song, int chart) {
    float spb = 60.0f / (float)song->bpm;
    float chart_beats = (float)song->charts[chart].length / (float)song->steps, melody_beats = 0.0f;
    for (int i = 0; i < song->note_count; i++) {
        float e = (float)(song->notes[i].start + song->notes[i].len) / (float)BD_CLOCKS_PER_BEAT;
        melody_beats = e > melody_beats ? e : melody_beats;
    }
    return (BD_LEAD_BEATS + (chart_beats > melody_beats ? chart_beats : melody_beats) + 1.0f) * spb;
}

int bd_song_kind_count(const bd_Song *song, bd_ChartKind kind) {
    int n = 0;
    for (int c = 0; c < song->chart_count; c++)
        if (song->charts[c].kind == kind)
            n++;
    return n;
}

int bd_song_kind_chart(const bd_Song *song, bd_ChartKind kind, int nth) {
    for (int c = 0; c < song->chart_count; c++)
        if (song->charts[c].kind == kind && nth-- == 0)
            return c;
    return -1;
}

/* Voice of a note that has no bead: a quiet organ, so that what you hear loudly is what you see. */
#define BD_BACKING_VOICE 3

int bd_song_to_synth(const bd_Song *song, int chart, float delay, SynthNote *out) {
    float spb = 60.0f / (float)song->bpm;
    const bd_Chart *c = &song->charts[arc_clampi(chart, 0, song->chart_count - 1)];
    int n = song->note_count < BD_MAX_NOTES ? song->note_count : BD_MAX_NOTES;
    for (int i = 0; i < n; i++) {
        int step = bd_note_step(song->notes[i].start, song->steps);
        bool lead = step < c->length && c->lane[step] >= 0;
        out[i].start = delay + (float)song->notes[i].start / (float)BD_CLOCKS_PER_BEAT * spb;
        out[i].dur = (float)song->notes[i].len / (float)BD_CLOCKS_PER_BEAT * spb;
        out[i].pitch = song->notes[i].pitch;
        out[i].voice = lead ? 0 : BD_BACKING_VOICE;
    }
    return n;
}

/* ------------------------------------------------------------------ parsing */
static char *trim(char *s) {
    while (*s == ' ' || *s == '\t')
        s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
    return s;
}

/* true if line starts with `word` followed by a space/tab/end of line */
static bool is_directive(const char *line, const char *word) {
    size_t n = strlen(word);
    return strncmp(line, word, n) == 0 && (line[n] == '\0' || line[n] == ' ' || line[n] == '\t');
}

typedef enum Section { SEC_NONE, SEC_CHART, SEC_MELODY } Section;

/* true when some step already in the chart holds a bead: a ribbon letter needs an object to continue. */
static bool has_bead(const bd_Chart *c) {
    for (int i = c->length - 1; i >= 0; i--) /* from the end: the nearest bead is usually just behind us */
        if (c->lane[i] >= 0)
            return true;
    return false;
}

static bool add_pattern(bd_Chart *c, const char *p) {
    for (; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            continue;
        if (c->length >= BD_MAX_STEPS)
            return false;
        int i = c->length;
        if (*p == '.') {
            c->lane[i] = -1;
        } else if (*p >= '0' && *p < '0' + c->lanes) { /* a bead that starts a new object */
            c->lane[i] = (int8_t)(*p - '0');
        } else if (*p >= 'a' && *p < 'a' + c->lanes && has_bead(c)) { /* a bead that continues the object */
            c->lane[i] = (int8_t)(*p - 'a');
            c->link[i] = 1;
        } else {
            return false; /* unknown character, a lane the chart does not have, or a letter with nothing to
                             continue */
        }
        c->length++;
    }
    return true;
}

/* "chart <name> [lanes] [catch|dodge]": strict, any other shape rejects the whole file. */
static bool parse_chart_header(const char *args, char *name, size_t name_size, int *lanes,
                               bd_ChartKind *kind) {
    char w[4][16] = {"", "", "", ""};
    int got = sscanf(args, "%15s %15s %15s %15s", w[0], w[1], w[2], w[3]);
    if (got < 1 || got > 3 || strlen(w[0]) >= name_size)
        return false;
    arc_strlcpy(name, w[0], name_size);
    *lanes = 3;
    *kind = BD_CHART_DODGE;
    if (got >= 2) {
        for (const char *d = w[1]; *d; d++)
            if (!isdigit((unsigned char)*d))
                return false;
        *lanes = atoi(w[1]);
    }
    if (got == 3) {
        if (strcmp(w[2], "catch") == 0)
            *kind = BD_CHART_CATCH;
        else if (strcmp(w[2], "dodge") != 0)
            return false; /* unknown third word */
    }
    return *lanes >= 2 && *lanes <= BD_MAX_LANES;
}

bool bd_song_load(const char *path, bd_Song *out) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    static bd_Song song; /* ~34 KB: not on the stack */
    memset(&song, 0, sizeof(song));
    arc_strlcpy(song.title, "Untitled", sizeof(song.title));
    song.steps = 1;
    Section section = SEC_NONE;
    bd_Chart *chart = NULL;
    bool ok = true;
    char raw[BD_MAX_STEPS + 256];

    while (ok && fgets(raw, sizeof(raw), fp)) {
        char *line = trim(raw);
        if (*line == '\0' || *line == '#')
            continue;
        if (is_directive(line, "title")) {
            arc_strlcpy(song.title, trim(line + 5), sizeof(song.title));
        } else if (is_directive(line, "bpm")) {
            song.bpm = atoi(line + 3);
        } else if (is_directive(line, "steps")) {
            song.steps = atoi(line + 5);
            ok =
                song.steps >= 1 && song.steps <= 8 && song.chart_count == 0; /* must come before the charts */
        } else if (is_directive(line, "chart")) {
            char name[16] = "";
            int lanes = 3;
            bd_ChartKind kind = BD_CHART_DODGE;
            ok = song.chart_count < BD_MAX_CHARTS &&
                 parse_chart_header(line + 5, name, sizeof(chart->name), &lanes, &kind);
            if (ok) {
                chart = &song.charts[song.chart_count++];
                arc_strlcpy(chart->name, name, sizeof(chart->name));
                chart->lanes = lanes;
                chart->kind = kind;
                section = SEC_CHART;
            }
        } else if (is_directive(line, "melody")) {
            section = SEC_MELODY;
        } else if (section == SEC_MELODY) {
            int start, len, pitch, voice;
            ok = sscanf(line, "%d %d %d %d", &start, &len, &pitch, &voice) == 4 && start >= 0 &&
                 start <= 65535 && len >= 1 && len <= 65535 && pitch >= 21 && pitch <= 108 && voice >= 0 &&
                 voice <= 3 && song.note_count < BD_MAX_NOTES;
            if (ok) {
                bd_Note *n = &song.notes[song.note_count++];
                n->start = (uint16_t)start;
                n->len = (uint16_t)len;
                n->pitch = (uint8_t)pitch;
                n->voice = (uint8_t)voice;
            }
        } else if (isdigit((unsigned char)line[0]) || line[0] == '.' ||
                   (line[0] >= 'a' && line[0] <= 'e')) { /* pattern line */
            if (song.chart_count == 0) {                 /* first-version file: an implicit chart */
                chart = &song.charts[song.chart_count++];
                arc_strlcpy(chart->name, "normal", sizeof(chart->name));
                chart->lanes = 3;
                section = SEC_CHART;
            }
            ok = section == SEC_CHART && add_pattern(chart, line);
        } else {
            ok = false; /* unknown directive: reject the file so typos are noticed */
        }
    }
    fclose(fp);
    if (ok) {
        ok = song.bpm >= 60 && song.bpm <= 220 && song.chart_count >= 1;
        for (int i = 0; ok && i < song.chart_count; i++)
            ok = song.charts[i].length >= 8;
    }
    if (!ok)
        return false;
    *out = song;
    return true;
}

bool bd_song_save(const char *path, const bd_Song *song) {
    FILE *fp = fopen(path, "w");
    if (!fp)
        return false;
    fprintf(fp, "# arCade song file (see games/beat_dodge/beat_dodge_logic.h for the format)\n");
    fprintf(fp, "title %s\nbpm %d\nsteps %d\n", song->title, song->bpm, song->steps);
    for (int c = 0; c < song->chart_count; c++) {
        const bd_Chart *ch = &song->charts[c];
        fprintf(fp, "chart %s %d%s\n", ch->name, ch->lanes, ch->kind == BD_CHART_CATCH ? " catch" : "");
        for (int i = 0; i < ch->length; i++) {
            if (ch->lane[i] < 0)
                fputc('.', fp);
            else
                fputc((ch->link[i] ? 'a' : '0') + ch->lane[i], fp); /* letters continue the previous object */
            if (i % 64 == 63 || i == ch->length - 1)
                fputc('\n', fp);
        }
    }
    if (song->note_count > 0) {
        fprintf(fp, "melody\n");
        for (int i = 0; i < song->note_count; i++)
            fprintf(fp, "%u %u %u %u\n", song->notes[i].start, song->notes[i].len, song->notes[i].pitch,
                    song->notes[i].voice);
    }
    return fclose(fp) == 0;
}

int bd_songlist_load(const char *dir, bd_SongList *out) {
    out->count = 0;
    out->skipped = 0;
    char index[600];
    snprintf(index, sizeof(index), "%s/songs.txt", dir);
    FILE *fp = fopen(index, "r");
    if (!fp)
        return 0;
    char raw[300];
    while (fgets(raw, sizeof(raw), fp)) {
        char *name = trim(raw);
        if (*name == '\0' || *name == '#')
            continue;
        if (out->count >= BD_MAX_SONGS)
            break;
        char path[960];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (bd_song_load(path, &out->songs[out->count]))
            out->count++;
        else
            out->skipped++;
    }
    fclose(fp);
    return out->count;
}

/* ------------------------------------------------------------------ menu */
void bd_menu_init(bd_Menu *m, bd_ChartKind kind) {
    m->song = 0;
    m->chart = 0;
    m->chosen = false;
    m->kind = kind;
}

/* The chart a song starts on: the second chart of the menu's kind ("normal") when there is more than one. */
static int default_chart(const bd_Song *song, bd_ChartKind kind) {
    int nth = bd_song_kind_count(song, kind) > 1 ? 1 : 0;
    return bd_song_kind_chart(song, kind, nth);
}

void bd_menu_move_song(bd_Menu *m, const bd_SongList *list, int dir) {
    for (int k = 1; k <= list->count; k++) { /* at most one full turn */
        int i = ((m->song + dir * k) % list->count + list->count) % list->count;
        if (bd_song_kind_count(&list->songs[i], m->kind) > 0) {
            m->song = i;
            m->chart = default_chart(&list->songs[i], m->kind);
            return;
        }
    }
}

void bd_menu_pick_chart(bd_Menu *m, const bd_SongList *list, int chart) {
    const bd_Song *song = &list->songs[m->song];
    if (chart >= 0 && chart < song->chart_count && song->charts[chart].kind == m->kind)
        m->chart = chart;
}

void bd_menu_step(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list) {
    if (list->count == 0)
        return;
    m->song = arc_clampi(m->song, 0, list->count - 1);
    if (bd_song_kind_count(&list->songs[m->song], m->kind) == 0)
        bd_menu_move_song(m, list, 1); /* this song has no chart for this game: go to one that has */
    if (in->down_pressed)
        bd_menu_move_song(m, list, 1);
    if (in->up_pressed)
        bd_menu_move_song(m, list, -1);
    const bd_Song *song = &list->songs[m->song];
    int nth = 0; /* position of the current chart among the charts of this kind */
    for (int c = 0; c < song->chart_count && c < m->chart; c++)
        nth += song->charts[c].kind == m->kind ? 1 : 0;
    if (m->chart < 0 || m->chart >= song->chart_count || song->charts[m->chart].kind != m->kind)
        nth = -1; /* the chart is not of this kind (first call): pick the default below */
    int count = bd_song_kind_count(song, m->kind);
    if (nth < 0) {
        m->chart = default_chart(song, m->kind);
    } else {
        if (in->left_pressed)
            nth = arc_maxi(0, nth - 1);
        if (in->right_pressed)
            nth = arc_mini(count - 1, nth + 1);
        m->chart = bd_song_kind_chart(song, m->kind, nth);
    }
    if (in->action_pressed && m->chart >= 0)
        m->chosen = true;
}

/* ------------------------------------------------------------------ reachable-lane check */
void bd_reach_start(bd_Reach *r, int lanes) {
    r->lanes = lanes;
    for (int l = 0; l < BD_MAX_LANES; l++)
        r->lane[l] = l == lanes / 2;
}

int bd_reach_steps(float seconds) {
    if (seconds <= 0.0f)
        return 0;
    return (int)floorf(seconds / BD_HUMAN_STEP_SEC + 0.001f); /* + 0.001: 0.36 s must give 3 steps, not 2 */
}

void bd_reach_advance(bd_Reach *r, float seconds) {
    int w = bd_reach_steps(seconds);
    if (w == 0)
        return;
    bool grown[BD_MAX_LANES] = {false};
    for (int l = 0; l < r->lanes; l++)
        for (int k = 0; k < r->lanes; k++)
            if (r->lane[k] && abs(l - k) <= w)
                grown[l] = true;
    for (int l = 0; l < r->lanes; l++)
        r->lane[l] = grown[l];
}

void bd_reach_bead(bd_Reach *r, bd_ChartKind kind, int bead_lane, bool in_ribbon) {
    if (kind == BD_CHART_DODGE) {
        r->lane[bead_lane] = false; /* standing there now would be a hit */
        return;
    }
    int tolerance = in_ribbon ? 1 : 0; /* catch: a ribbon is caught from its middle lane */
    for (int l = 0; l < r->lanes; l++)
        if (abs(l - bead_lane) > tolerance)
            r->lane[l] = false;
}

bool bd_reach_empty(const bd_Reach *r) {
    for (int l = 0; l < r->lanes; l++)
        if (r->lane[l])
            return false;
    return true;
}

bool bd_chart_playable(const bd_Song *song, int chart, int *fail_step) {
    const bd_Chart *c = &song->charts[chart];
    float step_sec = 60.0f / (float)song->bpm / (float)song->steps;
    float lead = BD_LEAD_BEATS * 60.0f / (float)song->bpm;
    bd_Reach reach;
    bd_reach_start(&reach, c->lanes);
    float before = 0.0f; /* the player stands in the middle lane from t = 0 */
    for (int i = 0; i < c->length; i++) {
        if (c->lane[i] < 0)
            continue;
        float t = lead + (float)i * step_sec;
        bd_reach_advance(&reach, t - before);
        before = t;
        bd_reach_bead(&reach, c->kind, c->lane[i], bd_chart_in_ribbon(c, i));
        if (bd_reach_empty(&reach)) {
            if (fail_step)
                *fail_step = i;
            return false;
        }
    }
    return true;
}

/* Two passes. Forward: remember the lanes the player could be in right after each bead. Backward: from the
 * last bead to the first, pick a remembered lane that is within reach of the lane chosen for the next bead.
 */
bool bd_chart_plan(const bd_Song *song, int chart, int8_t *plan) {
    const bd_Chart *c = &song->charts[chart];
    float step_sec = 60.0f / (float)song->bpm / (float)song->steps;
    float lead = BD_LEAD_BEATS * 60.0f / (float)song->bpm;
    bool after[BD_MAX_STEPS][BD_MAX_LANES]; /* 10 KB: fine on the stack */
    bd_Reach reach;
    bd_reach_start(&reach, c->lanes);
    float before = 0.0f;
    for (int i = 0; i < BD_MAX_STEPS; i++)
        plan[i] = -1;
    for (int i = 0; i < c->length; i++) {
        if (c->lane[i] < 0)
            continue;
        float t = lead + (float)i * step_sec;
        bd_reach_advance(&reach, t - before);
        before = t;
        bd_reach_bead(&reach, c->kind, c->lane[i], bd_chart_in_ribbon(c, i));
        if (bd_reach_empty(&reach))
            return false;
        for (int l = 0; l < BD_MAX_LANES; l++)
            after[i][l] = reach.lane[l];
    }
    int next = -1; /* step of the bead after the one being planned */
    for (int i = c->length - 1; i >= 0; i--) {
        if (c->lane[i] < 0)
            continue;
        int w = next < 0 ? c->lanes : bd_reach_steps((float)(next - i) * step_sec);
        int best = -1, goal = next < 0 ? c->lanes / 2 : plan[next];
        for (int l = 0; l < c->lanes; l++) {
            if (!after[i][l] || abs(l - goal) > w)
                continue;
            if (best < 0 || abs(l - goal) < abs(best - goal))
                best = l;
        }
        if (best < 0)
            return false; /* cannot happen when the forward pass succeeded */
        plan[i] = (int8_t)best;
        next = i;
    }
    return true;
}

/* ------------------------------------------------------------------ the shared run (clock + lane) */
void bd_run_start(bd_Run *r, const bd_Song *song, int chart) {
    if (&r->song != song) /* the caller may pass the copy this run already holds */
        r->song = *song;
    r->chart = arc_clampi(chart, 0, song->chart_count - 1);
    const bd_Chart *c = &r->song.charts[r->chart];
    r->lanes = c->lanes;
    r->length = c->length;
    r->time = 0.0f;
    r->spb = 60.0f / (float)r->song.bpm;
    r->step_sec = r->spb / (float)r->song.steps;
    r->lead = BD_LEAD_BEATS * r->spb;
    r->end_time = bd_song_seconds(&r->song, r->chart);
    r->next_hit = 0;
    r->last_beat = -1;
    r->beat_event = false;
    r->lane = r->lanes / 2;
    for (int l = 0; l < BD_MAX_LANES; l++)
        r->last_in_lane[l] = -1000.0f;
    r->last_in_lane[r->lane] = 0.0f;
    r->hit_flash = 0.0f;
    r->over = r->won = false;
}

float bd_run_arrival(const bd_Run *r, int step) {
    return r->lead + (float)step * r->step_sec;
}

float bd_run_obstacle_y(const bd_Run *r, int step) {
    float k = 1.0f - (bd_run_arrival(r, step) - r->time) / r->lead; /* 0 = just spawned, 1 = arrived */
    return BD_TOP_Y + (BD_PLAYER_Y - BD_TOP_Y) * k;
}

void bd_run_tick(bd_Run *r, const ArcadeInput *in, float dt) {
    if (r->over)
        return;
    r->time += dt;
    r->hit_flash = arc_maxf(0.0f, r->hit_flash - dt);

    int beat = (int)(r->time / r->spb);
    r->beat_event = beat != r->last_beat;
    r->last_beat = beat;

    if (in->left_pressed)
        r->lane = arc_maxi(0, r->lane - 1);
    if (in->right_pressed)
        r->lane = arc_mini(r->lanes - 1, r->lane + 1);
    if (in->typed >= '1' && in->typed < '1' + r->lanes) /* direct lane keys: 1..N */
        r->lane = in->typed - '1';
    r->last_in_lane[r->lane] = r->time;
}

void bd_run_sync(bd_Run *r, float clock_seconds) {
    if (!r->over && fabsf(r->time - clock_seconds) > 0.15f)
        r->time = clock_seconds;
}

/* ------------------------------------------------------------------ Beat Dodge rules */
/* All scoring numbers live in these three functions so the live game and the "perfect run" that defines
 * max_points can never disagree. */
static int award_object(bool close_call) { /* an object that ended without a hit */
    return close_call ? 20 : 10;
}

static int award_finish(int lives) { /* bonus for surviving the whole song */
    return lives * 50;
}

static void reset_rules(beat_dodge_State *s) { /* everything except the shared run, which bd_run_start did */
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    s->lives = BD_START_LIVES;
    s->dodged = s->hits = s->close_calls = s->points = 0;
    s->multiplier = bd_chart_multiplier(c->name);
    s->obj_hit = s->obj_close = false;
    s->close_flash = 0.0f;
    /* a perfect run: every object a close call, no life lost. It is replayed through the same award functions
     * as the real game. Humans land below it: the maximum is a ceiling, not a target. */
    s->max_points = award_finish(BD_START_LIVES);
    for (int k = 0; k < bd_chart_objects(c); k++)
        s->max_points += award_object(true);
}

void beat_dodge_init(beat_dodge_State *s, unsigned seed) {
    bd_song_default(&s->run.song, seed);
    bd_run_start(&s->run, &s->run.song, 0);
    reset_rules(s);
}

void beat_dodge_use_song(beat_dodge_State *s, const bd_Song *song, int chart) {
    bd_run_start(&s->run, song, chart);
    reset_rules(s);
}

float beat_dodge_arrival(const beat_dodge_State *s, int step) {
    return bd_run_arrival(&s->run, step);
}

float beat_dodge_obstacle_y(const beat_dodge_State *s, int step) {
    return bd_run_obstacle_y(&s->run, step);
}

/* A close call: the player stood in a lane that the object touches (any of its beads) within
 * BD_CLOSE_WINDOW_SEC before its first bead arrived. last_in_lane[] is the time the player last stood in a
 * lane. */
static bool is_close_call(const beat_dodge_State *s, const bd_Chart *c, int first) {
    float arrival = bd_run_arrival(&s->run, first);
    int step = first;
    do {
        if (arrival - s->run.last_in_lane[c->lane[step]] <= BD_CLOSE_WINDOW_SEC)
            return true;
        step = bd_chart_next_bead(c, step);
    } while (step < c->length && c->link[step]);
    return false;
}

/* Judges the bead of `step`, which has just reached the player line. */
static void judge_bead(beat_dodge_State *s, const bd_Chart *c, int step) {
    bd_Run *r = &s->run;
    if (!c->link[step]) { /* a new object starts: forget the last one */
        s->obj_hit = false;
        s->obj_close = is_close_call(s, c, step);
    }
    if (!s->obj_hit && c->lane[step] == r->lane) { /* hit: one life per object, the rest of it is ignored */
        s->obj_hit = true;
        s->lives--;
        s->hits++;
        r->hit_flash = 0.35f;
        if (s->lives <= 0)
            r->over = true;
    }
    int next = bd_chart_next_bead(c, step);
    bool last_of_object = next >= c->length || !c->link[next];
    if (last_of_object && !s->obj_hit) { /* the object ended without a hit */
        s->dodged++;
        s->points += award_object(s->obj_close);
        if (s->obj_close) {
            s->close_calls++;
            s->close_flash = 0.6f;
        }
    }
}

/* TODO(Member 5): drive `time` from a music stream clock instead of the fixed dt so the beat stays locked to
 * real audio on slow machines (beat_dodge_sync does this; the front-end calls it): see GAME.md
 * "Implementation steps". */
void beat_dodge_step(beat_dodge_State *s, const ArcadeInput *in, float dt) {
    bd_Run *r = &s->run;
    if (r->over)
        return;
    bd_run_tick(r, in, dt);
    s->close_flash = arc_maxf(0.0f, s->close_flash - dt);

    /* resolve every bead that has now arrived at the player line */
    const bd_Chart *c = &r->song.charts[r->chart];
    while (r->next_hit < r->length && r->time >= bd_run_arrival(r, r->next_hit) && !r->over) {
        if (c->lane[r->next_hit] >= 0)
            judge_bead(s, c, r->next_hit);
        r->next_hit++;
    }
    if (!r->over && r->time >= r->end_time) { /* survived the whole song */
        r->won = true;
        r->over = true;
        s->points += award_finish(s->lives);
    }
}

void beat_dodge_sync(beat_dodge_State *s, float clock_seconds) {
    bd_run_sync(&s->run, clock_seconds);
}

bool beat_dodge_is_over(const beat_dodge_State *s) {
    return s->run.over;
}

int beat_dodge_score(const beat_dodge_State *s) {
    if (s->max_points <= 0)
        return 0;
    return (int)(1000.0f * s->multiplier * (float)s->points / (float)s->max_points);
}
