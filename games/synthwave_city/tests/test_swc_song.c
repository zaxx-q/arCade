/* The composition. Numbers marked "web version" come from the original TypeScript song for the same seed. */
#include "arcade_test.h"
#include "swc_song.h"

static SwcSong song;
static SwcBar bar;

static int total_notes(uint32_t seed, int *lead) {
    swc_song_init(&song, seed);
    int total = 0;
    *lead = 0;
    for (int b = 0; b < SWC_SONG_BARS; b++) {
        swc_song_bar(&song, b, &bar);
        total += bar.count;
        for (int i = 0; i < bar.count; i++)
            if (bar.notes[i].voice == SWC_VOICE_LEAD)
                (*lead)++;
    }
    return total;
}

static void test_matches_the_web_version(void) {
    int lead;
    CHECK_EQ_INT(total_notes(1u, &lead), 3115);
    CHECK_EQ_INT(lead, 131);
    CHECK_EQ_INT(song.tonic, 0); /* C */
    CHECK_EQ_INT(total_notes(2u, &lead), 3097);
    CHECK_EQ_INT(lead, 115);
    CHECK_EQ_INT(song.tonic, 4); /* E */

    swc_song_init(&song, 1u);
    static const struct {
        int bar, count, kind, variant, tonic, transpose, root, colour;
    } bars[] = {{8, 36, SWC_SECTION_VERSE, 0, 0, 0, 0, 8},   {16, 50, SWC_SECTION_VERSE, 1, 0, 0, 0, 8},
                {24, 64, SWC_SECTION_CHORUS, 0, 0, 0, 3, 8}, {31, 57, SWC_SECTION_CHORUS, 0, 0, 0, 6, 8},
                {32, 22, SWC_SECTION_BREAK, 0, 0, 0, 0, 8},  {63, 57, SWC_SECTION_CHORUS, 2, 2, 2, 6, 8}};
    for (size_t i = 0; i < sizeof bars / sizeof bars[0]; i++) {
        swc_song_bar(&song, bars[i].bar, &bar);
        CHECK_EQ_INT(bar.count, bars[i].count);
        CHECK_EQ_INT(bar.kind, bars[i].kind);
        CHECK_EQ_INT(bar.variant, bars[i].variant);
        CHECK_EQ_INT(bar.tonic, bars[i].tonic);
        CHECK_EQ_INT(bar.transpose, bars[i].transpose);
        CHECK_EQ_INT(bar.chord.root, bars[i].root);
        CHECK_EQ_INT(bar.chord.colour, bars[i].colour);
    }
}

static void test_form_and_energy(void) {
    swc_song_init(&song, 3u);
    static const int form[SWC_SONG_SECTIONS] = {SWC_SECTION_INTRO,  SWC_SECTION_VERSE, SWC_SECTION_VERSE,
                                                SWC_SECTION_CHORUS, SWC_SECTION_BREAK, SWC_SECTION_VERSE,
                                                SWC_SECTION_CHORUS, SWC_SECTION_CHORUS};
    for (int b = 0; b < SWC_SONG_BARS; b++) {
        swc_song_bar(&song, b, &bar);
        CHECK_EQ_INT(bar.kind, form[b / SWC_BARS_PER_SECTION]);
        CHECK_EQ_INT(bar.bar_in_section, b % SWC_BARS_PER_SECTION);
        CHECK(bar.energy > 0.2 && bar.energy <= 1.0);
    }
    swc_song_bar(&song, 3, &bar);
    CHECK_NEAR(bar.energy, 0.3, 1e-12); /* the intro is quiet... */
    swc_song_bar(&song, 62, &bar);
    CHECK_NEAR(bar.energy, 1.0, 1e-12); /* ...and the last chorus is the loudest */
    /* a bar index outside the song is clamped, never read out of range */
    swc_song_bar(&song, -5, &bar);
    CHECK_EQ_INT(bar.index, 0);
    swc_song_bar(&song, 100000, &bar);
    CHECK_EQ_INT(bar.index, SWC_SONG_BARS - 1);
}

static void test_every_note_is_well_formed_and_sorted(void) {
    for (uint32_t seed = 1; seed <= 20u; seed++) {
        swc_song_init(&song, seed * 7919u);
        for (int b = 0; b < SWC_SONG_BARS; b++) {
            swc_song_bar(&song, b, &bar);
            CHECK(bar.count > 0 && bar.count < SWC_BAR_MAX_NOTES);
            for (int i = 0; i < bar.count; i++) {
                const SwcNote *n = &bar.notes[i];
                CHECK(n->voice >= 0 && n->voice < SWC_VOICE_COUNT);
                CHECK(n->beat >= 0.0 && n->beat < SWC_BEATS_PER_BAR);
                CHECK(n->duration > 0.0);
                CHECK(n->velocity >= 0.0 && n->velocity <= 1.0);
                CHECK(n->tone >= 0.0 && n->tone <= 1.0);
                CHECK(n->pan >= -1.0 && n->pan <= 1.0);
                if (n->voice == SWC_VOICE_BASS)
                    CHECK(n->midi >= 33 &&
                          n->midi <= 66); /* from the bass floor up to an octave and a bit above the root */
                if (n->voice == SWC_VOICE_LEAD)
                    CHECK(n->midi >= 55 && n->midi <= 100);
                if (i > 0) {
                    const SwcNote *p = &bar.notes[i - 1];
                    CHECK(p->beat < n->beat || (p->beat == n->beat && p->voice <= n->voice));
                }
            }
        }
    }
}

static void test_deterministic_and_seed_dependent(void) {
    static SwcSong a, b;
    static SwcBar ba, bb;
    swc_song_init(&a, 11u);
    swc_song_init(&b, 11u);
    swc_song_bar(&a, 27, &ba);
    swc_song_bar(&b, 27, &bb);
    CHECK_EQ_INT(ba.count, bb.count);
    for (int i = 0; i < ba.count; i++)
        CHECK(ba.notes[i].midi == bb.notes[i].midi && ba.notes[i].beat == bb.notes[i].beat);
    /* a bar does not depend on which bars were asked for before it */
    swc_song_bar(&b, 3, &bb);
    swc_song_bar(&b, 27, &bb);
    CHECK_EQ_INT(ba.count, bb.count);
    int differs = 0;
    for (uint32_t s = 12; s < 24; s++) {
        swc_song_init(&b, s);
        differs += b.tonic != a.tonic || b.arp != a.arp || b.bass_verse != a.bass_verse;
    }
    CHECK(differs > 6);
}

static void test_synth_loop_is_a_clean_loop(void) {
    static SynthNote notes[2048];
    swc_song_init(&song, 1u);
    int n = swc_song_synth_notes(&song, 16, 16, 105.0, notes, 2048);
    CHECK(n > 300 && n < 2048);
    double loop = 16.0 * 4.0 * 60.0 / 105.0;
    int voices[4] = {0, 0, 0, 0};
    for (int i = 0; i < n; i++) {
        CHECK(notes[i].start >= 0.0f && notes[i].start < (float)loop);
        CHECK(notes[i].dur > 0.0f);
        CHECK(notes[i].voice < 4);
        CHECK(notes[i].pitch > 20 && notes[i].pitch < 110);
        voices[notes[i].voice]++;
    }
    for (int v = 0; v < 4; v++)
        CHECK(voices[v] > 0); /* lead, organ bass, bell arpeggio, backing pad all play */
    /* the capacity is honoured */
    CHECK_EQ_INT(swc_song_synth_notes(&song, 16, 16, 105.0, notes, 10), 10);
}

static void test_energy_eases_in_over_the_first_beat(void) {
    swc_song_init(&song, 1u);
    double before = swc_song_energy_at(&song, 4.0 * 7.0 + 3.9); /* last beat of the intro's bar 7: 0.5 */
    double at = swc_song_energy_at(&song, 4.0 * 8.0);           /* bar 8 starts: the verse, 0.65 */
    double half = swc_song_energy_at(&song, 4.0 * 8.0 + 0.5);
    double later = swc_song_energy_at(&song, 4.0 * 8.0 + 2.0);
    CHECK_NEAR(before, 0.5, 1e-9);
    CHECK_NEAR(at, 0.5, 1e-9);
    CHECK(half > 0.5 && half < 0.65);
    CHECK_NEAR(later, 0.65, 1e-9);
}

int main(void) {
    RUN(test_matches_the_web_version);
    RUN(test_form_and_energy);
    RUN(test_every_note_is_well_formed_and_sorted);
    RUN(test_deterministic_and_seed_dependent);
    RUN(test_synth_loop_is_a_clean_loop);
    RUN(test_energy_eases_in_over_the_first_beat);
    return TEST_SUMMARY();
}
