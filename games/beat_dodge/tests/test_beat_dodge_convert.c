#include <string.h>

#include "arcade/rng.h"
#include "arcade_test.h"
#include "beat_dodge_convert.h"

static MidiSong g_midi; /* ~400 KB */
static bd_Song g_song;
static bd_ConvertStats g_stats;
static char g_err[96];

static MidiTrack *add_track(const char *name) {
    MidiTrack *t = &g_midi.tracks[g_midi.track_count++];
    memset(t, 0, sizeof(*t));
    snprintf(t->name, sizeof(t->name), "%s", name);
    return t;
}

static void add_note(MidiTrack *t, uint32_t tick, uint32_t dur, uint8_t pitch) {
    t->notes[t->note_count].start = tick;
    t->notes[t->note_count].dur = dur;
    t->notes[t->note_count].pitch = pitch;
    t->note_count++;
}

static void new_midi(float bpm) {
    memset(&g_midi, 0, sizeof(g_midi));
    g_midi.ticks_per_beat = 96;
    g_midi.bpm = bpm;
}

static void test_basic_conversion(void) {
    new_midi(120.0f);
    MidiTrack *opp = add_track("Daddy Dearest");
    MidiTrack *bf = add_track("Boyfriend");
    for (int i = 0; i < 16; i++) { /* a note every 8th note (48 ticks), pitches climbing */
        add_note(opp, (uint32_t)i * 96, 48, (uint8_t)(60 + i % 4));
        add_note(bf, (uint32_t)i * 96 + 48, 48, (uint8_t)(70 + i % 4));
    }
    CHECK(bd_convert_midi(&g_midi, "Test Song", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    CHECK_STR_EQ(g_song.title, "Test Song");
    CHECK_EQ_INT(g_song.bpm, 120);
    CHECK_EQ_INT(g_song.steps, 4);
    CHECK_EQ_INT(g_song.chart_count, 6); /* 3 dodge charts, then 3 catch charts */
    CHECK_STR_EQ(g_stats.player_track, "Boyfriend");
    CHECK_EQ_INT(g_song.note_count, 32);
    CHECK_EQ_INT(g_song.notes[1].start, 12); /* 48 ticks of 96 = half a beat = 12 clocks of 24 */
    CHECK_EQ_INT(g_song.notes[1].voice, 1);  /* the player track gets the "player" voice */
    CHECK_EQ_INT(g_song.notes[0].voice, 0);
    CHECK_EQ_INT(g_song.notes[0].len, 12);
    CHECK_STR_EQ(g_song.charts[0].name, "easy");
    CHECK_STR_EQ(g_song.charts[2].name, "hard");
    CHECK_EQ_INT(g_song.charts[0].lanes, 3);
    CHECK_EQ_INT(g_song.charts[2].lanes, 4);
    for (int c = 0; c < 6; c++) {
        CHECK_EQ_INT(g_song.charts[c].kind, c < 3 ? BD_CHART_DODGE : BD_CHART_CATCH);
        CHECK_STR_EQ(g_song.charts[c].name, c % 3 == 0 ? "easy" : (c % 3 == 1 ? "normal" : "hard"));
        CHECK(bd_chart_playable(&g_song, c, NULL));
    }
    CHECK_EQ_INT(g_song.charts[3].lanes, 3);
    CHECK_EQ_INT(g_song.charts[4].lanes, 4);
    CHECK_EQ_INT(g_song.charts[5].lanes, 4);
}

static void test_lone_notes_are_thinned_by_grid_and_minimum_gap(void) {
    new_midi(120.0f); /* 16th note = 0.125 s */
    MidiTrack *t = add_track("Boyfriend");
    for (int i = 0; i < 200; i++) /* a note every 3 steps = 0.375 s: too far apart to be a ribbon */
        add_note(t, (uint32_t)i * 72, 24, (uint8_t)(55 + (i * 7) % 20));
    CHECK(bd_convert_midi(&g_midi, "Isolated", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const float min_gap[3] = {0.55f, 0.45f, 0.30f};
    for (int c = 0; c < 3; c++) {
        float step_sec = 0.125f;
        int prev = -1000, n = 0;
        for (int k = 0; k < g_song.charts[c].length; k++) {
            if (g_song.charts[c].lane[k] < 0)
                continue;
            CHECK_EQ_INT(g_song.charts[c].link[k], 0); /* no ribbons here */
            if (prev >= 0)
                CHECK((float)(k - prev) * step_sec >= min_gap[c] - 0.001f);
            prev = k;
            n++;
        }
        CHECK_EQ_INT(n, g_stats.beads[c]);
        CHECK(n > 5);
        CHECK_EQ_INT(g_stats.ribbons[c], 0);
    }
    CHECK(g_stats.beads[0] < g_stats.beads[1] && g_stats.beads[1] < g_stats.beads[2]);
    for (int k = 0; k < g_song.charts[0].length; k++) /* easy only uses quarter-note positions */
        if (g_song.charts[0].lane[k] >= 0)
            CHECK_EQ_INT(k % 4, 0);
    for (int k = 0; k < g_song.charts[1].length; k++) /* normal only uses 8th-note positions */
        if (g_song.charts[1].lane[k] >= 0)
            CHECK_EQ_INT(k % 2, 0);
}

/* Visits every object of a chart: calls nothing, but checks that a ribbon is at most BD_RIBBON_MAX_SEC long,
 * touches at most `cap` lanes and has at least two beads. Returns the number of ribbons in *ribbons. */
static void check_ribbons(const bd_Chart *c, float step_sec, int cap, int *ribbons) {
    *ribbons = 0;
    int first = -1, last = -1, beads = 0;
    bool touched[BD_MAX_LANES];
    for (int k = 0; k <= c->length; k++) {
        bool bead = k < c->length && c->lane[k] >= 0;
        bool starts = bead && c->link[k] == 0;
        if (k == c->length || starts) { /* the previous object ends here */
            if (beads >= 2) {
                (*ribbons)++;
                int lanes_used = 0;
                for (int l = 0; l < BD_MAX_LANES; l++)
                    lanes_used += touched[l] ? 1 : 0;
                CHECK(lanes_used <= cap);
                CHECK((float)(last - first) * step_sec <= BD_RIBBON_MAX_SEC + 0.001f);
            }
            beads = 0;
            for (int l = 0; l < BD_MAX_LANES; l++)
                touched[l] = false;
            first = -1;
        }
        if (bead) {
            if (first < 0)
                first = k;
            last = k;
            beads++;
            touched[c->lane[k]] = true;
        }
    }
}

static void test_rapid_notes_become_ribbons_that_obey_the_span_cap(void) {
    new_midi(120.0f); /* 16th note = 0.125 s: a note on EVERY 16th is one 25 s long chain */
    MidiTrack *t = add_track("Boyfriend");
    for (int i = 0; i < 200; i++)
        add_note(t, (uint32_t)i * 24, 24, (uint8_t)(55 + (i * 7) % 20));
    CHECK(bd_convert_midi(&g_midi, "Dense", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const int cap[3] = {1, 2, 3};
    for (int c = 0; c < 3; c++) {
        const bd_Chart *chart = &g_song.charts[c];
        CHECK_EQ_INT(bd_chart_beads(chart), 200);                  /* every sound keeps its bead ... */
        CHECK_EQ_INT(bd_chart_objects(chart), g_stats.ribbons[c]); /* ... and every object is a ribbon */
        CHECK(g_stats.ribbons[c] >= 25);                           /* 25 s / 1.2 s */
        int ribbons;
        check_ribbons(chart, 0.125f, cap[c], &ribbons);
        CHECK_EQ_INT(ribbons, g_stats.ribbons[c]);
        bd_ChartMetrics m;
        bd_chart_metrics(&g_song, c, &m);
        CHECK_EQ_INT(m.bursts, 1);
        CHECK_EQ_INT(m.bursts_mismatched, 0);
        CHECK_NEAR(m.covered, 1.0f, 0.0001f);
        CHECK(m.playable);
    }
    CHECK(g_stats.folded[0] > 0); /* easy: a bar is one lane, so most of these ribbons had to be folded */
}

static void test_catch_charts_use_the_player_voice_only(void) {
    new_midi(120.0f);
    MidiTrack *opp = add_track("Daddy Dearest");
    MidiTrack *bf = add_track("Boyfriend");
    for (int i = 0; i < 40; i++) {
        add_note(bf, (uint32_t)i * 96, 48, (uint8_t)(60 + (i * 5) % 12));      /* steps 0, 4, 8, ... */
        add_note(opp, (uint32_t)i * 96 + 48, 48, (uint8_t)(50 + (i * 3) % 9)); /* steps 2, 6, 10, ... */
    }
    CHECK(bd_convert_midi(&g_midi, "Voices", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    bool dodge_has_opponent_step = false;
    for (int c = 0; c < 6; c++) {
        const bd_Chart *chart = &g_song.charts[c];
        for (int k = 0; k < chart->length; k++) {
            if (chart->lane[k] < 0)
                continue;
            if (chart->kind == BD_CHART_CATCH)
                CHECK_EQ_INT(k % 4, 0); /* only the player's steps */
            else if (k % 4 == 2)
                dodge_has_opponent_step = true;
        }
        if (c >= 3)
            CHECK(bd_chart_beads(chart) > 5);
    }
    CHECK(dodge_has_opponent_step);             /* dodge charts hear both voices */
    CHECK(g_stats.beads[5] <= 40);              /* catch: at most one bead per player note */
    CHECK(g_stats.beads[3] < g_stats.beads[4]); /* catch easy thins more than normal (notes 0.5 s apart) */
}

static void test_hostile_songs_still_give_playable_charts(void) {
    for (int seed = 1; seed <= 6; seed++) {
        Rng rng;
        rng_seed(&rng, (uint64_t)seed);
        new_midi(seed % 2 ? 190.0f : 100.0f);
        MidiTrack *a = add_track("Opp");
        MidiTrack *b = add_track("Boyfriend");
        for (int i = 0; i < 500;
             i++) { /* random positions on the 16th grid, pitches jumping over 4 octaves */
            MidiTrack *tr = rng_chance(&rng, 0.5f) ? a : b;
            add_note(tr, (uint32_t)rng_range(&rng, 0, 1400) * 24u, 24, (uint8_t)rng_range(&rng, 36, 84));
        }
        CHECK(bd_convert_midi(&g_midi, "Hostile", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
        for (int c = 0; c < 6; c++) {
            CHECK(bd_chart_playable(&g_song, c, NULL));
            CHECK_EQ_INT(g_stats.lost[c], 0);
            bd_ChartMetrics m;
            bd_chart_metrics(&g_song, c, &m);
            CHECK_EQ_INT(m.bursts_mismatched, 0); /* a run of rapid notes is never left without a ribbon */
        }
    }
}

static void test_lanes_follow_pitch(void) {
    new_midi(60.0f); /* 1 beat per second: far apart notes, nothing gets thinned on hard */
    MidiTrack *t = add_track("Boyfriend");
    uint8_t pitches[6] = {40, 50, 60, 70, 80, 90}; /* one note each: six distinct pitches over 4 lanes */
    for (int i = 0; i < 6; i++)
        add_note(t, (uint32_t)i * 96 * 2, 48, pitches[i]); /* every 2 beats = every 8 steps */
    CHECK(bd_convert_midi(&g_midi, "Pitches", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const bd_Chart *hard = &g_song.charts[2];
    int lanes_in_order[6]; /* the lane of each melody note (drum beads on other steps are not melody) */
    for (int i = 0; i < 6; i++) {
        lanes_in_order[i] = hard->lane[i * 8];
        CHECK(lanes_in_order[i] >= 0);
    }
    CHECK_EQ_INT(lanes_in_order[0], 0);               /* lowest pitch: leftmost lane */
    CHECK_EQ_INT(lanes_in_order[5], hard->lanes - 1); /* highest pitch: rightmost lane */
    for (int i = 1; i < 6; i++)
        CHECK(lanes_in_order[i] >= lanes_in_order[i - 1]); /* never goes backwards as pitch rises */
    bool used[BD_MAX_LANES] = {false};
    for (int i = 0; i < 6; i++)
        used[lanes_in_order[i]] = true;
    for (int l = 0; l < hard->lanes; l++)
        CHECK(used[l]); /* every lane is used when there are enough distinct pitches */
}

static void test_same_pitch_always_same_lane_and_same_step_dedupes(void) {
    new_midi(60.0f);
    MidiTrack *a = add_track("Dad");
    MidiTrack *b = add_track("Boyfriend");
    for (int i = 0; i < 12; i++) {
        add_note(a, (uint32_t)i * 192, 48, (uint8_t)(i % 2 ? 72 : 60));
        add_note(b, (uint32_t)i * 192 + 3, 48, 66); /* 3 ticks later: rounds to the same step, player wins */
    }
    CHECK(bd_convert_midi(&g_midi, "Dedupe", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const bd_Chart *c = &g_song.charts[2];
    int lane_of_66 = c->lane[0];
    CHECK(lane_of_66 >= 0);
    for (int i = 0; i < 12; i++)                  /* a note every 8 steps (drum beads sit on other steps) */
        CHECK_EQ_INT(c->lane[i * 8], lane_of_66); /* the player's pitch 66 won every step: one lane only */
}

static void test_player_track_selection_and_errors(void) {
    new_midi(120.0f);
    MidiTrack *x = add_track("Alpha");
    MidiTrack *y = add_track("Beta");
    for (int i = 0; i < 12; i++) {
        add_note(x, (uint32_t)i * 96, 48, 60);
        add_note(y, (uint32_t)i * 96 + 48, 48, 64);
    }
    CHECK(bd_convert_midi(&g_midi, "T", "alp", &g_song, &g_stats, g_err,
                          sizeof(g_err))); /* case-insensitive part */
    CHECK_STR_EQ(g_stats.player_track, "Alpha");
    CHECK(bd_convert_midi(&g_midi, "T", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    CHECK_STR_EQ(g_stats.player_track, "Beta"); /* default: the last track */
    CHECK(!bd_convert_midi(&g_midi, "T", "zzz", &g_song, &g_stats, g_err, sizeof(g_err)));
    CHECK(strlen(g_err) > 0);
    new_midi(300.0f); /* absurd tempo */
    add_note(add_track("Boyfriend"), 0, 48, 60);
    CHECK(!bd_convert_midi(&g_midi, "T", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    new_midi(120.0f); /* a song longer than a chart can hold */
    MidiTrack *z = add_track("Boyfriend");
    add_note(z, 0, 48, 60);
    add_note(z, 96u * 600u, 48, 60); /* beat 600 = step 2400 > BD_MAX_STEPS */
    CHECK(!bd_convert_midi(&g_midi, "T", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
}

static void test_converted_song_is_playable(void) {
    new_midi(150.0f);
    MidiTrack *opp = add_track("Opp");
    MidiTrack *bf = add_track("Boyfriend");
    for (int i = 0; i < 120; i++) {
        add_note(opp, (uint32_t)i * 48, 24, (uint8_t)(55 + (i * 5) % 17));
        add_note(bf, (uint32_t)i * 48 + 24, 24, (uint8_t)(60 + (i * 3) % 13));
    }
    CHECK(bd_convert_midi(&g_midi, "Playable", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    static int8_t plan[BD_MAX_STEPS];
    for (int c = 0; c < g_song.chart_count; c++) {
        CHECK(bd_chart_playable(&g_song, c, NULL));
        CHECK(bd_chart_plan(&g_song, c, plan)); /* a path a human can follow exists */
    }
}

int main(void) {
    RUN(test_basic_conversion);
    RUN(test_lone_notes_are_thinned_by_grid_and_minimum_gap);
    RUN(test_rapid_notes_become_ribbons_that_obey_the_span_cap);
    RUN(test_catch_charts_use_the_player_voice_only);
    RUN(test_lanes_follow_pitch);
    RUN(test_same_pitch_always_same_lane_and_same_step_dedupes);
    RUN(test_player_track_selection_and_errors);
    RUN(test_converted_song_is_playable);
    RUN(test_hostile_songs_still_give_playable_charts);
    return TEST_SUMMARY();
}
