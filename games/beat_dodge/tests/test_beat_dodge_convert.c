#include <string.h>

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
    CHECK_EQ_INT(g_song.chart_count, 3);
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
}

static void test_difficulty_thinning_respects_minimum_gaps(void) {
    new_midi(120.0f); /* 16th note = 0.125 s */
    MidiTrack *t = add_track("Boyfriend");
    for (int i = 0; i < 200; i++) /* a note on EVERY 16th: 8 notes per second */
        add_note(t, (uint32_t)i * 24, 24, (uint8_t)(55 + (i * 7) % 20));
    CHECK(bd_convert_midi(&g_midi, "Dense", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const float min_gap[3] = {0.55f, 0.45f, 0.30f};
    for (int c = 0; c < 3; c++) {
        float step_sec = 0.125f;
        int prev = -1000, n = 0;
        for (int k = 0; k < g_song.charts[c].length; k++) {
            if (g_song.charts[c].lane[k] < 0)
                continue;
            if (prev >= 0)
                CHECK((float)(k - prev) * step_sec >= min_gap[c] - 0.001f);
            prev = k;
            n++;
        }
        CHECK_EQ_INT(n, g_stats.obstacles[c]);
        CHECK(n > 5);
    }
    CHECK(g_stats.obstacles[0] < g_stats.obstacles[1] && g_stats.obstacles[1] < g_stats.obstacles[2]);
    for (int k = 0; k < g_song.charts[0].length; k++) /* easy only uses quarter-note positions */
        if (g_song.charts[0].lane[k] >= 0)
            CHECK_EQ_INT(k % 4, 0);
    for (int k = 0; k < g_song.charts[1].length; k++) /* normal only uses 8th-note positions */
        if (g_song.charts[1].lane[k] >= 0)
            CHECK_EQ_INT(k % 2, 0);
}

static void test_lanes_follow_pitch(void) {
    new_midi(60.0f); /* 1 beat per second: far apart notes, nothing gets thinned on hard */
    MidiTrack *t = add_track("Boyfriend");
    uint8_t pitches[6] = {40, 50, 60, 70, 80, 90}; /* one note each: six distinct pitches over 4 lanes */
    for (int i = 0; i < 6; i++)
        add_note(t, (uint32_t)i * 96 * 2, 48, pitches[i]); /* every 2 beats */
    CHECK(bd_convert_midi(&g_midi, "Pitches", NULL, &g_song, &g_stats, g_err, sizeof(g_err)));
    const bd_Chart *hard = &g_song.charts[2];
    int lanes_in_order[6], n = 0;
    for (int k = 0; k < hard->length; k++)
        if (hard->lane[k] >= 0)
            lanes_in_order[n++] = hard->lane[k];
    CHECK_EQ_INT(n, 6);
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
    int lane_of_66 = -1;
    for (int k = 0; k < c->length; k++)
        if (c->lane[k] >= 0) {
            if (lane_of_66 < 0)
                lane_of_66 = c->lane[k];
            CHECK_EQ_INT(c->lane[k], lane_of_66); /* the player's pitch 66 won every step: one lane only */
        }
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
    static beat_dodge_State s;
    for (int c = 0; c < 3; c++) {
        beat_dodge_use_song(&s, &g_song, c);
        for (int i = 0; i < 60 * 200 && !beat_dodge_is_over(&s); i++) {
            ArcadeInput in;
            arcade_input_clear(&in);
            int j = s.run.next_hit;
            while (j < s.run.length && g_song.charts[c].lane[j] < 0)
                j++;
            if (j < s.run.length && g_song.charts[c].lane[j] == s.run.lane) {
                in.left_pressed = s.run.lane > 0;
                in.right_pressed = s.run.lane == 0;
            }
            beat_dodge_step(&s, &in, ARC_DT);
        }
        CHECK(s.run.won);
        CHECK_EQ_INT(s.hits, 0);
    }
}

int main(void) {
    RUN(test_basic_conversion);
    RUN(test_difficulty_thinning_respects_minimum_gaps);
    RUN(test_lanes_follow_pitch);
    RUN(test_same_pitch_always_same_lane_and_same_step_dedupes);
    RUN(test_player_track_selection_and_errors);
    RUN(test_converted_song_is_playable);
    return TEST_SUMMARY();
}
