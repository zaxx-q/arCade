#include <stdlib.h>
#include <string.h>

#include "arcade/synth.h"
#include "arcade_test.h"

static int16_t g_pcm[SYNTH_RATE * 4];

static SynthOptions no_drums(void) {
    SynthOptions o;
    memset(&o, 0, sizeof(o));
    o.rate = SYNTH_RATE;
    o.bpm = 120.0f;
    return o;
}

static int rising_zero_crossings(const int16_t *p, size_t from, size_t to) {
    int n = 0;
    for (size_t i = from + 1; i < to; i++)
        if (p[i - 1] < 0 && p[i] >= 0)
            n++;
    return n;
}

static void test_pitch_to_frequency(void) {
    CHECK_NEAR(synth_midi_to_hz(69), 440.0, 0.001);
    CHECK_NEAR(synth_midi_to_hz(81), 880.0, 0.01);
    CHECK_NEAR(synth_midi_to_hz(60), 261.63, 0.05);
}

static void test_rendered_note_has_the_right_pitch_and_timing(void) {
    SynthNote n = {0.5f, 1.0f, 69, 0};
    SynthOptions o = no_drums();
    size_t frames = synth_frames(2.0f, SYNTH_RATE);
    synth_render(&n, 1, &o, g_pcm, frames);
    for (size_t i = 0; i < (size_t)(0.49f * SYNTH_RATE); i++)
        CHECK_EQ_INT(g_pcm[i], 0); /* silence before the note starts */
    size_t from = (size_t)(0.7f * SYNTH_RATE),
           to = (size_t)(1.2f * SYNTH_RATE); /* inside the sustained part */
    int crossings = rising_zero_crossings(g_pcm, from, to);
    CHECK(crossings >= 215 && crossings <= 225); /* 440 Hz * 0.5 s = 220 */
    for (size_t i = (size_t)(1.6f * SYNTH_RATE); i < frames; i++)
        CHECK_EQ_INT(g_pcm[i], 0); /* released and silent again */
}

static void test_octave_up_doubles_the_frequency(void) {
    SynthNote lo = {0.0f, 0.8f, 57, 1}, hi = {0.0f, 0.8f, 69, 1};
    SynthOptions o = no_drums();
    size_t frames = synth_frames(1.0f, SYNTH_RATE);
    synth_render(&lo, 1, &o, g_pcm, frames);
    int a = rising_zero_crossings(g_pcm, 4000, 12000);
    synth_render(&hi, 1, &o, g_pcm, frames);
    int b = rising_zero_crossings(g_pcm, 4000, 12000);
    CHECK(a > 0);
    CHECK(b >= 2 * a - 2 && b <= 2 * a + 2);
}

static int peak_of_voice(uint8_t voice) {
    SynthNote n = {0.0f, 0.5f, 69, voice};
    SynthOptions o = no_drums();
    size_t frames = synth_frames(1.0f, SYNTH_RATE);
    synth_render(&n, 1, &o, g_pcm, frames);
    int peak = 0;
    for (size_t i = 0; i < frames; i++)
        peak = abs(g_pcm[i]) > peak ? abs(g_pcm[i]) : peak;
    return peak;
}

static void test_backing_voice_is_much_quieter_than_the_lead(void) {
    int lead = peak_of_voice(0), organ = peak_of_voice(1), backing = peak_of_voice(3);
    CHECK(backing > 500);       /* still audible */
    CHECK(backing * 2 < lead);  /* less than half the lead ... */
    CHECK(backing * 2 < organ); /* ... and than the organ, which used to be voice 3's fallback */
    SynthNote n = {0.0f, 0.5f, 69, 3};
    SynthOptions o = no_drums();
    size_t frames = synth_frames(1.0f, SYNTH_RATE);
    synth_render(&n, 1, &o, g_pcm, frames);
    int crossings = rising_zero_crossings(g_pcm, (size_t)(0.1f * SYNTH_RATE), (size_t)(0.4f * SYNTH_RATE));
    CHECK(crossings >= 128 && crossings <= 136); /* still 440 Hz: 0.3 s * 440 = 132 */
}

static void test_many_notes_never_clip_and_are_deterministic(void) {
    static SynthNote notes[200];
    for (int i = 0; i < 200; i++) {
        notes[i].start = (float)(i % 40) * 0.05f; /* heavy overlap */
        notes[i].dur = 0.5f;
        notes[i].pitch = (uint8_t)(50 + i % 30);
        notes[i].voice = (uint8_t)(i % 4);
    }
    SynthOptions o = no_drums();
    o.drums = true;
    o.drums_from = 0.0f;
    o.drums_to = 2.5f;
    size_t frames = synth_frames(3.0f, SYNTH_RATE);
    static int16_t other[SYNTH_RATE * 4];
    synth_render(notes, 200, &o, g_pcm, frames);
    synth_render(notes, 200, &o, other, frames);
    CHECK(memcmp(g_pcm, other, frames * sizeof(int16_t)) == 0);
    int peak = 0;
    for (size_t i = 0; i < frames; i++)
        peak = abs(g_pcm[i]) > peak ? abs(g_pcm[i]) : peak;
    CHECK(peak > 3000);
}

static void test_drums_stay_inside_their_window(void) {
    SynthOptions o = no_drums();
    o.drums = true;
    o.drums_from = 1.0f;
    o.drums_to = 2.0f;
    size_t frames = synth_frames(3.0f, SYNTH_RATE);
    synth_render(NULL, 0, &o, g_pcm, frames);
    for (size_t i = 0; i < (size_t)(0.99f * SYNTH_RATE); i++)
        CHECK_EQ_INT(g_pcm[i], 0);
    int loud = 0;
    for (size_t i = (size_t)(1.0f * SYNTH_RATE); i < (size_t)(1.2f * SYNTH_RATE); i++)
        loud += abs(g_pcm[i]) > 1000;
    CHECK(loud > 100); /* the kick on the downbeat */
    for (size_t i = (size_t)(2.4f * SYNTH_RATE); i < frames; i++)
        CHECK_EQ_INT(g_pcm[i], 0);
}

static void test_wav_container(void) {
    int16_t pcm[4] = {0, 1000, -1000, 32767};
    uint8_t wav[64];
    size_t n = synth_wav_encode(pcm, 4, 22050, wav);
    CHECK_EQ_INT(n, synth_wav_bytes(4));
    CHECK(memcmp(wav, "RIFF", 4) == 0 && memcmp(wav + 8, "WAVEfmt ", 8) == 0 &&
          memcmp(wav + 36, "data", 4) == 0);
    CHECK_EQ_INT(wav[20], 1);                                 /* PCM */
    CHECK_EQ_INT(wav[22], 1);                                 /* mono */
    CHECK_EQ_INT(wav[24] | (wav[25] << 8), 22050);            /* sample rate */
    CHECK_EQ_INT(wav[40], 8);                                 /* data bytes = 4 frames * 2 */
    CHECK_EQ_INT((int16_t)(wav[46] | (wav[47] << 8)), 1000);  /* second sample, little endian */
    CHECK_EQ_INT((int16_t)(wav[50] | (wav[51] << 8)), 32767); /* last sample */
}

int main(void) {
    RUN(test_pitch_to_frequency);
    RUN(test_rendered_note_has_the_right_pitch_and_timing);
    RUN(test_octave_up_doubles_the_frequency);
    RUN(test_backing_voice_is_much_quieter_than_the_lead);
    RUN(test_many_notes_never_clip_and_are_deterministic);
    RUN(test_drums_stay_inside_their_window);
    RUN(test_wav_container);
    return TEST_SUMMARY();
}
