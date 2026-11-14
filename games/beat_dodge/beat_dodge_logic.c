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

int bd_chart_obstacles(const bd_Chart *chart) {
    int n = 0;
    for (int i = 0; i < chart->length; i++)
        if (chart->lane[i] >= 0)
            n++;
    return n;
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

int bd_song_to_synth(const bd_Song *song, float delay, SynthNote *out) {
    float spb = 60.0f / (float)song->bpm;
    int n = song->note_count < BD_MAX_NOTES ? song->note_count : BD_MAX_NOTES;
    for (int i = 0; i < n; i++) {
        out[i].start = delay + (float)song->notes[i].start / (float)BD_CLOCKS_PER_BEAT * spb;
        out[i].dur = (float)song->notes[i].len / (float)BD_CLOCKS_PER_BEAT * spb;
        out[i].pitch = song->notes[i].pitch;
        out[i].voice = song->notes[i].voice;
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

static bool add_pattern(bd_Chart *c, const char *p) {
    for (; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            continue;
        if (c->length >= BD_MAX_STEPS)
            return false;
        if (*p == '.')
            c->lane[c->length++] = -1;
        else if (*p >= '0' && *p < '0' + c->lanes)
            c->lane[c->length++] = (int8_t)(*p - '0');
        else
            return false; /* unknown character or a lane the chart does not have */
    }
    return true;
}

bool bd_song_load(const char *path, bd_Song *out) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        return false;
    static bd_Song song; /* ~16 KB: not on the stack */
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
            int got = sscanf(line + 5, "%15s %d", name, &lanes);
            ok = got >= 1 && strlen(name) < sizeof(chart->name) && lanes >= 2 && lanes <= BD_MAX_LANES &&
                 song.chart_count < BD_MAX_CHARTS;
            if (ok) {
                chart = &song.charts[song.chart_count++];
                arc_strlcpy(chart->name, name, sizeof(chart->name));
                chart->lanes = lanes;
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
        } else if (isdigit((unsigned char)line[0]) || line[0] == '.') { /* pattern line */
            if (song.chart_count == 0) { /* first-version file: an implicit chart */
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
        fprintf(fp, "chart %s %d\n", ch->name, ch->lanes);
        for (int i = 0; i < ch->length; i++) {
            fputc(ch->lane[i] < 0 ? '.' : '0' + ch->lane[i], fp);
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
void bd_menu_init(bd_Menu *m) {
    m->song = 0;
    m->chart = 0;
    m->chosen = false;
}

void bd_menu_step(bd_Menu *m, const ArcadeInput *in, const bd_SongList *list) {
    if (list->count == 0)
        return;
    int before = m->song;
    if (in->down_pressed)
        m->song = (m->song + 1) % list->count;
    if (in->up_pressed)
        m->song = (m->song + list->count - 1) % list->count;
    if (m->song != before) /* a new song: start on the second chart (normal) when there is one */
        m->chart = list->songs[m->song].chart_count > 1 ? 1 : 0;
    int charts = list->songs[m->song].chart_count;
    if (in->left_pressed)
        m->chart = arc_maxi(0, m->chart - 1);
    if (in->right_pressed)
        m->chart = arc_mini(charts - 1, m->chart + 1);
    m->chart = arc_clampi(m->chart, 0, charts - 1);
    if (in->action_pressed)
        m->chosen = true;
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
/* All scoring numbers live in these two functions so the live game and the "perfect run" that defines
 * max_points can never disagree. */
static int
award_dodge(int combo) { /* points for one dodged obstacle; the longer the combo, the more (max x4) */
    return 10 * arc_mini(combo, 4);
}

static int award_finish(int lives) { /* bonus for surviving the whole song */
    return lives * 50;
}

static void reset_rules(beat_dodge_State *s) { /* everything except the shared run, which bd_run_start did */
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    s->lives = BD_START_LIVES;
    s->dodged = s->hits = s->combo = s->best_combo = s->points = 0;
    s->multiplier = bd_chart_multiplier(c->name);
    int obstacles = bd_chart_obstacles(c);
    s->max_points = award_finish(BD_START_LIVES);
    for (int k = 1; k <= obstacles; k++) /* what a perfect run earns: every obstacle dodged, no hit */
        s->max_points += award_dodge(k);
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

/* TODO(Member 5): drive `time` from a music stream clock instead of the fixed dt so the beat stays locked to
 * real audio on slow machines (beat_dodge_sync does this; the front-end calls it): see GAME.md
 * "Implementation steps". */
void beat_dodge_step(beat_dodge_State *s, const ArcadeInput *in, float dt) {
    bd_Run *r = &s->run;
    if (r->over)
        return;
    bd_run_tick(r, in, dt);

    /* resolve every obstacle that has now arrived at the player line */
    const bd_Chart *c = &r->song.charts[r->chart];
    while (r->next_hit < r->length && r->time >= bd_run_arrival(r, r->next_hit) && !r->over) {
        int l = c->lane[r->next_hit];
        if (l >= 0) {
            if (l == r->lane) { /* hit */
                s->lives--;
                s->hits++;
                s->combo = 0;
                r->hit_flash = 0.35f;
                if (s->lives <= 0)
                    r->over = true;
            } else { /* dodged */
                s->dodged++;
                s->combo++;
                s->best_combo = arc_maxi(s->best_combo, s->combo);
                s->points += award_dodge(s->combo);
            }
        }
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
