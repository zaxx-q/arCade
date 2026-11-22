/* Synthwave City - the composition. PURE C99, no raylib.
 *
 * Ported from src/audio/song.ts of the web version: the same seed gives the same chords, bass lines,
 * arpeggios and lead melodies, bar for bar. Only the FIRST movement (64 bars: intro, verse, verse, chorus,
 * break, verse, chorus, chorus) is ported; the web version chains endless movements. The front-end renders a
 * loop of it with the core software synthesizer (arcade/synth.h), so the sound is much simpler than the web
 * version's Web Audio mix, but the notes are the same. */
#ifndef SWC_SONG_H
#define SWC_SONG_H

#include "arcade/synth.h"
#include "swc_core.h"

#define SWC_BEATS_PER_BAR 4
#define SWC_STEPS_PER_BAR 16
#define SWC_BARS_PER_SECTION 8
#define SWC_SONG_SECTIONS 8
#define SWC_SONG_BARS (SWC_SONG_SECTIONS * SWC_BARS_PER_SECTION)
#define SWC_BAR_MAX_NOTES 160

typedef enum SwcVoice {
    SWC_VOICE_KICK = 0,
    SWC_VOICE_SNARE,
    SWC_VOICE_HAT,
    SWC_VOICE_OPEN_HAT,
    SWC_VOICE_TOM,
    SWC_VOICE_CRASH,
    SWC_VOICE_RISER,
    SWC_VOICE_BASS,
    SWC_VOICE_PAD,
    SWC_VOICE_ARP,
    SWC_VOICE_LEAD,
    SWC_VOICE_WHOOSH,
    SWC_VOICE_COUNT
} SwcVoice;

typedef enum SwcSectionKind {
    SWC_SECTION_INTRO = 0,
    SWC_SECTION_VERSE,
    SWC_SECTION_CHORUS,
    SWC_SECTION_BREAK
} SwcSectionKind;

typedef struct SwcNote {
    int voice;
    double beat;     /* onset in beats from the start of the bar, [0, 4) */
    double duration; /* beats */
    int midi;        /* melodic voices and toms; 0 for noise voices */
    double velocity; /* 0..1 */
    double tone;     /* 0..1 */
    double pan;      /* -1..1 */
    int glide;       /* lead only: semitones the pitch slides in from; 0 for none */
} SwcNote;

typedef struct SwcChord {
    int root;   /* scale degree of the root, 0..6 */
    int colour; /* added note in scale steps above the root: 6 seventh, 8 ninth */
} SwcChord;

typedef struct SwcMotifNote {
    int step, length, degree;
} SwcMotifNote;

typedef struct SwcMotif {
    SwcMotifNote note[2][8]; /* [0] the busier call bar, [1] the answer bar */
    int count[2];
} SwcMotif;

typedef struct SwcSong {
    uint32_t seed;
    int tonic; /* pitch class of the first movement's key, 0 (C) .. 11 (B) */
    int kind[SWC_SONG_SECTIONS];
    int variant[SWC_SONG_SECTIONS];
    SwcChord verse[4], chorus[4], turnaround;
    int bass_verse, bass_chorus, arp; /* indices into the pattern tables */
    SwcMotif motif_a, motif_b;
    int answer_shift;
    double lead_tone;
} SwcSong;

typedef struct SwcBar {
    int index;
    int section;        /* 0..7 */
    int kind;           /* SwcSectionKind */
    int variant;        /* how many sections of this kind came before it */
    int bar_in_section; /* 0..7 */
    int tonic;          /* pitch class this bar sounds in (the key, +2 in a movement's third chorus) */
    int transpose;
    SwcChord chord;
    double energy; /* 0..1 */
    int count;
    SwcNote notes[SWC_BAR_MAX_NOTES]; /* sorted by beat, then voice */
} SwcBar;

void swc_song_init(SwcSong *song, uint32_t seed);
/* Builds bar `bar` (clamped to 0 .. SWC_SONG_BARS-1). */
void swc_song_bar(const SwcSong *song, int bar, SwcBar *out);
/* Loudness 0..1 at a beat position, eased over the first beat of each bar (as the web version does). */
double swc_song_energy_at(const SwcSong *song, double beat);

/* Turns `bars` bars starting at bar `first` into notes for the core synthesizer, at `bpm`, time starting at
 * 0. Mapping: lead -> bright lead, bass -> soft organ, arpeggio -> thin bell, pad -> quiet backing; the drums
 * are left to the synthesizer's own pattern and the noise voices (riser, whoosh, crash) are dropped. Returns
 * the number of notes written (at most `cap`). */
int swc_song_synth_notes(const SwcSong *song, int first, int bars, double bpm, SynthNote *out, int cap);

#endif
