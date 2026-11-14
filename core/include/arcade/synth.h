/* Tiny software synthesizer (pure C, deterministic): turns a list of notes into 16-bit mono PCM.
 * Used by Beat Dodge to play MIDI-derived songs without shipping any audio files.
 * The front-end wraps the PCM in a WAV and plays it with raylib; this module knows nothing about raylib. */
#ifndef ARCADE_SYNTH_H
#define ARCADE_SYNTH_H

#include "arcade/common.h"

#define SYNTH_RATE 22050 /* samples per second: small files, plenty for a chiptune-style lead */

typedef struct SynthNote {
    float start, dur; /* seconds */
    uint8_t pitch;    /* MIDI note number, 69 = A4 = 440 Hz */
    uint8_t voice;    /* 0 = bright lead, 1 = soft organ, 2 = thin bell, 3 = quiet backing, others: organ */
} SynthNote;

typedef struct SynthOptions {
    int rate;         /* usually SYNTH_RATE */
    float bpm;        /* tempo for the drum pattern */
    bool drums;       /* kick on every beat, snare on 2 and 4, hi-hat on the off-beats */
    float drums_from; /* seconds: the drum pattern runs only inside [from, to) */
    float drums_to;
} SynthOptions;

float synth_midi_to_hz(int pitch);
size_t synth_frames(float seconds, int rate); /* number of samples needed for `seconds` */
/* Renders every note (and the drums) into pcm[0..frames). pcm is overwritten. */
void synth_render(const SynthNote *notes, size_t count, const SynthOptions *opt, int16_t *pcm, size_t frames);
/* WAV container: size in bytes for `frames` mono 16-bit samples, and the encoder (out needs
 * synth_wav_bytes()). */
size_t synth_wav_bytes(size_t frames);
/* Writes only the 44-byte header (so the samples can be rendered straight after it; samples must be little
 * endian). */
void synth_wav_header(size_t frames, int rate, uint8_t *out44);
size_t synth_wav_encode(const int16_t *pcm, size_t frames, int rate, uint8_t *out);

#endif
