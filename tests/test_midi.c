#include <string.h>

#include "arcade/midi.h"
#include "arcade_test.h"

static MidiSong g_song; /* ~400 KB */

/* helper: append bytes */
static size_t put(uint8_t *buf, size_t n, const uint8_t *src, size_t len) {
    memcpy(buf + n, src, len);
    return n + len;
}

/* Builds a small format-1 file: a tempo track and one note track named "Lead" (running status, a note-on with
 * velocity 0 as note-off, one note without any note-off). */
static size_t build_midi(uint8_t *buf) {
    static const uint8_t header[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0, 96};
    static const uint8_t tempo_track[] = {'M',  'T',  'r',  'k',  0,    0,    0,    11,   0x00, 0xFF,
                                          0x51, 0x03, 0x07, 0xA1, 0x20, 0x00, 0xFF, 0x2F, 0x00};
    static const uint8_t note_body[] = {
        0x00, 0xFF, 0x03, 0x04, 'L', 'e', 'a', 'd', /* track name */
        0x00, 0x90, 60,   100,                      /* note on C4 at tick 0 */
        0x60, 0x80, 60,   0,                        /* note off after 96 ticks */
        0x00, 0x90, 64,   100,                      /* note on E4 at tick 96 */
        0x30, 64,   0,                              /* running status: velocity 0 = off after 48 ticks */
        0x00, 62,   90,              /* running status note on D4 at tick 144, never released */
        0x81, 0x40, 0xFF, 0x2F, 0x00 /* end of track 192 ticks later (VLQ 0x81 0x40 = 192) */
    };
    uint8_t chunk[8] = {'M', 'T', 'r', 'k', 0, 0, 0, (uint8_t)sizeof(note_body)};
    size_t n = 0;
    n = put(buf, n, header, sizeof(header));
    n = put(buf, n, tempo_track, sizeof(tempo_track));
    n = put(buf, n, chunk, 8);
    n = put(buf, n, note_body, sizeof(note_body));
    return n;
}

static void test_parses_notes_tempo_and_names(void) {
    uint8_t buf[256];
    size_t n = build_midi(buf);
    CHECK(midi_parse(buf, n, &g_song));
    CHECK_EQ_INT(g_song.ticks_per_beat, 96);
    CHECK_NEAR(g_song.bpm, 120.0, 0.01); /* 0x07A120 = 500000 microseconds per beat */
    CHECK_EQ_INT(g_song.tempo_changes, 0);
    CHECK_EQ_INT(g_song.track_count, 1); /* the tempo-only track has no notes */
    CHECK_STR_EQ(g_song.tracks[0].name, "Lead");
    CHECK_EQ_INT(g_song.tracks[0].note_count, 3);
    CHECK_EQ_INT(g_song.tracks[0].notes[0].pitch, 60);
    CHECK_EQ_INT(g_song.tracks[0].notes[0].start, 0);
    CHECK_EQ_INT(g_song.tracks[0].notes[0].dur, 96);
    CHECK_EQ_INT(g_song.tracks[0].notes[1].start, 96);
    CHECK_EQ_INT(g_song.tracks[0].notes[1].dur, 48); /* velocity-0 note-on closed it */
    CHECK_EQ_INT(g_song.tracks[0].notes[2].start, 144);
    CHECK_EQ_INT(g_song.tracks[0].notes[2].dur, 192); /* unreleased note ends with the track */
}

static void test_rejects_garbage(void) {
    uint8_t buf[256];
    size_t n = build_midi(buf);
    CHECK(!midi_parse((const uint8_t *)"nope", 4, &g_song));
    CHECK(strlen(g_song.error) > 0);
    buf[0] = 'X'; /* broken magic */
    CHECK(!midi_parse(buf, n, &g_song));
    n = build_midi(buf);
    CHECK(!midi_parse(buf, n - 12, &g_song)); /* truncated file */
    n = build_midi(buf);
    buf[13] = 0;
    buf[12] = 0x80; /* SMPTE time division */
    CHECK(!midi_parse(buf, n, &g_song));
    CHECK(!midi_load("no_such_file.mid", &g_song));
}

static void test_truncation_never_crashes(void) {
    uint8_t buf[256];
    size_t n = build_midi(buf);
    for (size_t cut = 0; cut < n; cut++) {
        midi_parse(buf, cut, &g_song); /* any prefix must be handled (return value does not matter) */
    }
    CHECK(midi_parse(buf, n, &g_song));
}

int main(void) {
    RUN(test_parses_notes_tempo_and_names);
    RUN(test_rejects_garbage);
    RUN(test_truncation_never_crashes);
    return TEST_SUMMARY();
}
