#include "arcade/synth.h"

#include <math.h>
#include <string.h>

float synth_midi_to_hz(int pitch) {
    return 440.0f * powf(2.0f, (float)(pitch - 69) / 12.0f);
}

size_t synth_frames(float seconds, int rate) {
    return (size_t)ceilf(seconds * (float)rate) + 1u;
}

static void mix(int16_t *pcm, size_t frames, size_t i, float v) {
    if (i >= frames)
        return;
    int s = (int)pcm[i] + (int)lrintf(v * 32767.0f);
    pcm[i] = (int16_t)(s > 32767 ? 32767 : (s < -32768 ? -32768 : s));
}

/* Attack, decay to a sustain level, hold for the note length, then a short release. */
static float envelope(float t, float dur) {
    const float attack = 0.006f, decay = 0.09f, sustain = 0.62f, release = 0.05f;
    float e;
    if (t < attack)
        e = t / attack;
    else if (t < attack + decay)
        e = 1.0f - (1.0f - sustain) * ((t - attack) / decay);
    else
        e = sustain;
    if (t > dur) {
        float r = 1.0f - (t - dur) / release;
        e *= r > 0.0f ? r : 0.0f;
    }
    return e;
}

/* Waveforms are sums of a few sine harmonics: no aliasing, and each voice sounds different. */
static float voice_wave(int voice, float phase, float *gain) {
    switch (voice) {
    case 0: /* bright, square-like lead */
        *gain = 0.15f;
        return (sinf(phase) + 0.33f * sinf(3.0f * phase) + 0.2f * sinf(5.0f * phase)) * 0.75f;
    case 2: /* thin bell */
        *gain = 0.12f;
        return sinf(phase) + 0.5f * sinf(4.0f * phase) * 0.6f;
    default: /* soft organ */
        *gain = 0.19f;
        return (sinf(phase) + 0.45f * sinf(2.0f * phase) + 0.2f * sinf(3.0f * phase)) * 0.6f;
    }
}

static float noise(uint32_t *state) { /* xorshift32 -> [-1, 1] */
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return (float)(x >> 8) / 8388608.0f - 1.0f;
}

static void render_note(const SynthNote *n, int rate, int16_t *pcm, size_t frames) {
    float dur = n->dur < 0.07f ? 0.07f : n->dur;
    float freq = synth_midi_to_hz(n->pitch);
    size_t i0 = (size_t)(n->start > 0.0f ? n->start * (float)rate : 0.0f);
    size_t count = (size_t)((dur + 0.05f) * (float)rate);
    for (size_t k = 0; k < count; k++) {
        float t = (float)k / (float)rate, gain;
        float v = voice_wave(n->voice, 6.2831853f * freq * t, &gain);
        mix(pcm, frames, i0 + k, v * gain * envelope(t, dur));
    }
}

static void render_kick(float at, int rate, int16_t *pcm, size_t frames) {
    size_t i0 = (size_t)(at * (float)rate), count = (size_t)(0.18f * (float)rate);
    float phase = 0.0f;
    for (size_t k = 0; k < count; k++) {
        float t = (float)k / (float)rate;
        phase +=
            6.2831853f * (45.0f + 85.0f * expf(-t * 30.0f)) / (float)rate; /* pitch drops fast: a "thump" */
        mix(pcm, frames, i0 + k, sinf(phase) * 0.30f * expf(-t * 14.0f));
    }
}

static void render_noise_hit(float at, int rate, int16_t *pcm, size_t frames, uint32_t seed, float length,
                             float decay, float gain, float tone_hz) {
    size_t i0 = (size_t)(at * (float)rate), count = (size_t)(length * (float)rate);
    uint32_t st = seed * 2654435761u + 1u;
    float prev = 0.0f;
    for (size_t k = 0; k < count; k++) {
        float t = (float)k / (float)rate, n = noise(&st);
        float hp = n - prev; /* crude high-pass: bright "tss" */
        prev = n;
        float v = hp * 0.5f;
        if (tone_hz > 0.0f)
            v += sinf(6.2831853f * tone_hz * t) * 0.35f;
        mix(pcm, frames, i0 + k, v * gain * expf(-t * decay));
    }
}

void synth_render(const SynthNote *notes, size_t count, const SynthOptions *opt, int16_t *pcm,
                  size_t frames) {
    memset(pcm, 0, frames * sizeof(int16_t));
    for (size_t i = 0; i < count; i++)
        render_note(&notes[i], opt->rate, pcm, frames);
    if (!opt->drums || opt->bpm <= 0.0f)
        return;
    float spb = 60.0f / opt->bpm;
    int first = (int)ceilf(opt->drums_from / spb), last = (int)floorf(opt->drums_to / spb);
    for (int b = first; b < last; b++) {
        float at = (float)b * spb;
        if (at < opt->drums_from || at >= opt->drums_to)
            continue;
        render_kick(at, opt->rate, pcm, frames);
        if (b % 4 == 1 || b % 4 == 3) /* snare on beats 2 and 4 of each bar */
            render_noise_hit(at, opt->rate, pcm, frames, (uint32_t)b, 0.14f, 22.0f, 0.55f, 190.0f);
        render_noise_hit(at + spb * 0.5f, opt->rate, pcm, frames, (uint32_t)b + 100000u, 0.05f, 55.0f, 0.30f,
                         0.0f);
    }
}

size_t synth_wav_bytes(size_t frames) {
    return 44u + frames * 2u;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void synth_wav_header(size_t frames, int rate, uint8_t *out) {
    uint32_t data = (uint32_t)(frames * 2u);
    memcpy(out, "RIFF", 4);
    put32(out + 4, 36u + data);
    memcpy(out + 8, "WAVEfmt ", 8);
    put32(out + 16, 16u); /* fmt chunk size */
    put16(out + 20, 1u);  /* PCM */
    put16(out + 22, 1u);  /* mono */
    put32(out + 24, (uint32_t)rate);
    put32(out + 28, (uint32_t)rate * 2u); /* byte rate */
    put16(out + 32, 2u);                  /* block align */
    put16(out + 34, 16u);                 /* bits per sample */
    memcpy(out + 36, "data", 4);
    put32(out + 40, data);
}

size_t synth_wav_encode(const int16_t *pcm, size_t frames, int rate, uint8_t *out) {
    synth_wav_header(frames, rate, out);
    for (size_t i = 0; i < frames; i++)
        put16(out + 44 + i * 2u, (uint16_t)pcm[i]);
    return 44u + frames * 2u;
}
