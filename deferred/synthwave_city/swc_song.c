/* Synthwave City - the composition (see swc_song.h). PURE C99, no raylib.
 * Every random draw happens in the same order as in song.ts, so the same seed gives the same notes. Where
 * the TypeScript evaluates a call inside an argument list, the draw is hoisted to a local here, in that
 * order. */
#include "swc_song.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HOME_KEY_COUNT 7
#define LEAD_LOW (-3) /* scale degrees from the tonic, in the lead octave */
#define LEAD_HIGH 7
#define LEAD_LONG_NOTE 6
#define LEAD_REPEAT 0.14
#define LEAD_KEEP_DIRECTION 0.68
#define BASS_LOW 33
#define PAD_LOW 57
#define PAD_COLOUR_LOW 65
#define ARP_LOW 55
#define LEAD_LOW_MIDI 65
#define TOM_HIGH 50
#define TOM_MID 45
#define TOM_LOW 41
#define FINAL_CHORUS_LIFT 2
#define STEP_BEATS ((double)SWC_BEATS_PER_BAR / (double)SWC_STEPS_PER_BAR)
#define DEGREES 7

static const int HOME_KEYS[HOME_KEY_COUNT] = {9, 2, 4, 6, 11, 0, 7};
static const int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

static const int FIRST_FORM[SWC_SONG_SECTIONS] = {SWC_SECTION_INTRO,  SWC_SECTION_VERSE, SWC_SECTION_VERSE,
                                                  SWC_SECTION_CHORUS, SWC_SECTION_BREAK, SWC_SECTION_VERSE,
                                                  SWC_SECTION_CHORUS, SWC_SECTION_CHORUS};

static const int HOME_PROGRESSIONS[7][4] = {{0, 5, 2, 6}, {0, 6, 5, 6}, {0, 2, 6, 3}, {0, 4, 5, 3},
                                            {0, 5, 3, 6}, {0, 0, 5, 6}, {0, 3, 5, 6}};
static const int LIFT_PROGRESSIONS[6][4] = {{5, 6, 0, 0}, {5, 2, 6, 0}, {3, 5, 0, 6},
                                            {5, 6, 4, 0}, {5, 6, 0, 2}, {3, 6, 2, 5}};

static const int BASS_PATTERNS[6][16] = {
    {0, 0, 7, 0, 0, 0, 7, 0, 0, 0, 7, 0, 0, 0, 7, 7},    {0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7},
    {0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 7, 4},    {0, 4, 7, 4, 0, 4, 7, 9, 0, 4, 7, 4, 0, 7, 4, 7},
    {0, -1, 0, 0, 7, -1, 0, 0, 0, -1, 0, 0, 7, 0, 4, 7}, {0, 0, 7, 0, 4, 0, 7, 0, 0, 0, 7, 0, 9, 7, 4, 7}};

static const int ARP_PATTERNS[6][16] = {
    {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3}, {0, 1, 2, 3, 4, 3, 2, 1, 0, 1, 2, 3, 4, 3, 2, 1},
    {0, 2, 3, 2, 4, 2, 3, 2, 0, 2, 3, 2, 4, 2, 3, 2}, {0, 2, 1, 3, 2, 4, 3, 5, 0, 2, 1, 3, 2, 4, 3, 2},
    {0, 1, 2, 0, 1, 2, 0, 1, 3, 4, 5, 3, 4, 5, 3, 4}, {3, 2, 1, 0, 3, 2, 1, 0, 4, 3, 2, 1, 4, 3, 2, 1}};

typedef struct Rhythm {
    int count;
    int step[6];
} Rhythm;
static const Rhythm CALL_RHYTHMS[7] = {
    {5, {0, 3, 6, 8, 12}},      {5, {0, 4, 6, 10, 12}},    {4, {0, 6, 8, 14}},    {5, {2, 4, 6, 8, 12}},
    {6, {0, 3, 6, 10, 12, 14}}, {6, {0, 2, 4, 8, 11, 14}}, {5, {0, 4, 8, 10, 12}}};
static const Rhythm ANSWER_RHYTHMS[6] = {{3, {0, 3, 6}}, {3, {0, 4, 8}},    {4, {0, 2, 4, 8}},
                                         {3, {0, 6, 8}}, {4, {0, 3, 6, 8}}, {3, {2, 4, 8}}};
static const int LEAD_SIZES[7] = {1, 1, 1, 1, 2, 2, 3};
static const int ANSWER_SHIFTS[4] = {-2, -1, 1, 2};
static const int START_DEGREES[4] = {0, 2, 4, 7};
static const double PAD_PANS[4] = {-0.45, 0.45, -0.2, 0.2};
static const int TRIAD[3] = {0, 2, 4};
static const int ROOT_OR_FIFTH[2] = {0, 4};

/* ---- theory.ts ---- */

static int mod_i(int v, int m) {
    return ((v % m) + m) % m;
}

static int floor_div(int a, int b) { /* b > 0 */
    int q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

static int degree_to_semitones(int degree) {
    int octave = floor_div(degree, DEGREES);
    return octave * 12 + MINOR_SCALE[degree - octave * DEGREES];
}

static int interval_from(int from, int steps) {
    return degree_to_semitones(from + steps) - degree_to_semitones(from);
}

static int place_at_or_above(int pitch_class, int low) {
    return low + mod_i(pitch_class - low, 12);
}

static bool in_list(const int *list, int n, int v) {
    for (int i = 0; i < n; i++)
        if (list[i] == v)
            return true;
    return false;
}

static int snap_to_chord(int degree, int root, const int *allowed, int n) {
    for (int k = 0; k < DEGREES; k++) {
        if (in_list(allowed, n, mod_i(degree - k - root, DEGREES)))
            return degree - k;
        if (in_list(allowed, n, mod_i(degree + k - root, DEGREES)))
            return degree + k;
    }
    return degree;
}

/* ---- the movement ---- */

/* A seventh on VII is a dominant seventh that pulls to the relative major, so VII takes the ninth; the ninth
 * on v is a minor ninth, a clash, so v takes the seventh. */
static int colour_for(int root, bool prefer_ninth) {
    if (root == 6)
        return 8;
    if (root == 4)
        return 6;
    return prefer_ninth ? 8 : 6;
}

static void to_chords(const int *roots, SwcRng *rng, SwcChord *out) {
    for (int i = 0; i < 4; i++) {
        bool ninth = swc_rng_chance(rng, 0.5);
        out[i].root = roots[i];
        out[i].colour = colour_for(roots[i], ninth);
    }
}

typedef struct MotifWalk {
    int degree, direction;
    bool first;
} MotifWalk;

static void motif_phrase(SwcRng *rng, MotifWalk *w, const Rhythm *r, int end, SwcMotifNote *out) {
    for (int i = 0; i < r->count; i++) {
        if (!w->first && !swc_rng_chance(rng, LEAD_REPEAT)) {
            if (!swc_rng_chance(rng, LEAD_KEEP_DIRECTION))
                w->direction = -w->direction;
            int size = LEAD_SIZES[swc_rng_pick(rng, 7)];
            int next = w->degree + w->direction * size;
            if (next > LEAD_HIGH || next < LEAD_LOW)
                w->direction = -w->direction;
            w->degree += w->direction * size;
        }
        w->first = false;
        int until = i + 1 < r->count ? r->step[i + 1] : end;
        int length = until - r->step[i];
        out[i].step = r->step[i];
        out[i].length = length < 8 ? length : 8;
        out[i].degree = w->degree;
    }
}

/* A two-bar motif: a busier bar, then a bar that ends on a long note. */
static void build_motif(SwcRng *rng, int start, SwcMotif *m) {
    MotifWalk w;
    w.degree = start;
    w.direction = swc_rng_chance(rng, 0.5) ? 1 : -1;
    w.first = true;
    const Rhythm *call = &CALL_RHYTHMS[swc_rng_pick(rng, 7)];
    motif_phrase(rng, &w, call, SWC_STEPS_PER_BAR, m->note[0]);
    m->count[0] = call->count;
    if (w.degree != start)
        w.direction = w.degree > start ? -1 : 1;
    const Rhythm *answer = &ANSWER_RHYTHMS[swc_rng_pick(rng, 6)];
    motif_phrase(rng, &w, answer, SWC_STEPS_PER_BAR - 1, m->note[1]);
    m->count[1] = answer->count;
}

void swc_song_init(SwcSong *song, uint32_t seed) {
    memset(song, 0, sizeof(*song));
    song->seed = seed;
    uint32_t v[3];
    v[0] = seed;
    v[1] = 0x5eedu;
    v[2] = 0u; /* movement 0 */
    SwcRng rng;
    swc_rng_seed(&rng, swc_hash_ints(v, 3));

    song->tonic = HOME_KEYS[swc_rng_pick(&rng, HOME_KEY_COUNT)];
    int seen[4] = {0, 0, 0, 0};
    for (int s = 0; s < SWC_SONG_SECTIONS; s++) {
        song->kind[s] = FIRST_FORM[s];
        song->variant[s] = seen[FIRST_FORM[s]]++;
    }

    int verse_index = swc_rng_pick(&rng, 7);
    const int *verse_roots = HOME_PROGRESSIONS[verse_index];
    const int *chorus_roots;
    if (swc_rng_chance(&rng, 0.75)) {
        chorus_roots = LIFT_PROGRESSIONS[swc_rng_pick(&rng, 6)];
    } else {
        int i = swc_rng_pick(&rng, 7);
        chorus_roots = HOME_PROGRESSIONS[i];
        if (i == verse_index) /* the same progression twice: take a lift instead */
            chorus_roots = LIFT_PROGRESSIONS[0 % 6];
    }

    song->bass_verse = swc_rng_pick(&rng, 6);
    song->bass_chorus = swc_rng_pick(&rng, 6);
    if (song->bass_chorus == song->bass_verse)
        song->bass_chorus = (song->bass_verse + 3) % 6;

    int start_a = START_DEGREES[swc_rng_pick(&rng, 4)];
    SwcRng fork1 = swc_rng_fork(&rng, 1u);
    build_motif(&fork1, start_a, &song->motif_a);
    SwcRng fork2 = swc_rng_fork(&rng, 2u);
    int start_b = start_a + swc_rng_int(&rng, 2, 4);
    build_motif(&fork2, start_b < LEAD_HIGH ? start_b : LEAD_HIGH, &song->motif_b);

    to_chords(verse_roots, &rng, song->verse);
    to_chords(chorus_roots, &rng, song->chorus);
    if (swc_rng_chance(&rng, 0.6)) {
        song->turnaround.root = 6;
        song->turnaround.colour = colour_for(6, true);
    } else {
        song->turnaround.root = 4;
        song->turnaround.colour = colour_for(4, true);
    }
    song->arp = swc_rng_pick(&rng, 6);
    song->answer_shift = ANSWER_SHIFTS[swc_rng_pick(&rng, 4)];
    song->lead_tone = swc_rng_range(&rng, 0.5, 0.8);
}

static SwcChord chord_at(const SwcSong *song, int section, int b) {
    int kind = song->kind[section];
    if (kind == SWC_SECTION_BREAK) /* a break takes the verse progression at half speed */
        return song->verse[(b >> 1) % 4];
    SwcChord chord = (kind == SWC_SECTION_CHORUS ? song->chorus : song->verse)[b % 4];
    if (b == SWC_BARS_PER_SECTION - 1 && chord.root == 0) /* do not end a section at rest on the tonic */
        return song->turnaround;
    return chord;
}

static bool same_chord(SwcChord a, SwcChord b) {
    return a.root == b.root && a.colour == b.colour;
}

static double bar_energy(int kind, int variant, int b) {
    switch (kind) {
    case SWC_SECTION_INTRO:
        return b < 4 ? 0.3 : 0.5;
    case SWC_SECTION_VERSE:
        return variant == 0 ? 0.65 : 0.72;
    case SWC_SECTION_CHORUS:
        return variant == 0 ? 0.92 : 1.0;
    default:
        return b < 6 ? 0.35 : (b == 6 ? 0.45 : 0.6);
    }
}

/* ---- one bar ---- */

static double clamp01(double v) {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

static void push(SwcBar *bar, int voice, double step, double steps, int midi, double velocity, double tone,
                 double pan, int glide) {
    if (bar->count >= SWC_BAR_MAX_NOTES)
        return;
    SwcNote *n = &bar->notes[bar->count++];
    n->voice = voice;
    n->beat = step * STEP_BEATS;
    n->duration = steps * STEP_BEATS;
    n->midi = midi;
    n->velocity = clamp01(velocity);
    n->tone = clamp01(tone);
    n->pan = pan;
    n->glide = glide;
}

/* default tone 0.5, no pan, no glide */
static void put(SwcBar *bar, int voice, double step, double steps, int midi, double velocity) {
    push(bar, voice, step, steps, midi, velocity, 0.5, 0.0, 0);
}
static void put_tone(SwcBar *bar, int voice, double step, double steps, int midi, double velocity,
                     double tone) {
    push(bar, voice, step, steps, midi, velocity, tone, 0.0, 0);
}

typedef struct BarCtx {
    int kind, variant, next_kind, prev_kind;
} BarCtx;

enum { FILL_NONE, FILL_LIFT, FILL_DROP, FILL_TURN };

static int fill_for(int kind, int next_kind, int b) {
    if (b != SWC_BARS_PER_SECTION - 1 || kind == SWC_SECTION_BREAK)
        return FILL_NONE;
    if (next_kind == SWC_SECTION_BREAK || next_kind == SWC_SECTION_INTRO)
        return FILL_DROP;
    if (kind == SWC_SECTION_INTRO || (kind == SWC_SECTION_VERSE && next_kind == SWC_SECTION_CHORUS))
        return FILL_LIFT;
    return FILL_TURN;
}

static void drums(SwcBar *out, const BarCtx *c, int b, SwcRng *rng) {
    int kind = c->kind, variant = c->variant;
    bool groove = kind == SWC_SECTION_VERSE || kind == SWC_SECTION_CHORUS;
    bool intro_groove = kind == SWC_SECTION_INTRO && b >= 4;
    bool break_build = kind == SWC_SECTION_BREAK && b >= 4;
    int fill = fill_for(kind, c->next_kind, b);

    if (groove || intro_groove) { /* four on the floor */
        double hit = kind == SWC_SECTION_CHORUS ? 1.0 : (kind == SWC_SECTION_VERSE ? 0.86 : 0.72);
        for (int beat = 0; beat < SWC_BEATS_PER_BAR; beat++)
            put(out, SWC_VOICE_KICK, beat * 4, 1, 0, hit);
    }
    if (kind == SWC_SECTION_CHORUS && (b == 1 || b == 5) && swc_rng_chance(rng, 0.4))
        put(out, SWC_VOICE_KICK, 14, 1, 0, 0.7);

    if (groove) { /* the big snare on 2 and 4 */
        double hit = kind == SWC_SECTION_CHORUS ? 1.0 : 0.88;
        put_tone(out, SWC_VOICE_SNARE, 4, 2, 0, hit, 0.9);
        put_tone(out, SWC_VOICE_SNARE, 12, 2, 0, hit, 0.9);
    }
    if (kind == SWC_SECTION_INTRO && b == 7)
        put_tone(out, SWC_VOICE_SNARE, 12, 1, 0, 0.9, 0.9);
    if (kind == SWC_SECTION_BREAK &&
        b == 6) /* a break ends in a snare build: quarters, eighths, sixteenths */
        for (int step = 0; step < SWC_STEPS_PER_BAR; step += 4)
            put_tone(out, SWC_VOICE_SNARE, step, 1, 0, 0.42 + step * 0.008, 0.45);
    if (kind == SWC_SECTION_BREAK && b == 7) {
        for (int step = 0; step < 8; step += 2)
            put_tone(out, SWC_VOICE_SNARE, step, 1, 0, 0.5 + step * 0.02, 0.45);
        for (int step = 8; step < SWC_STEPS_PER_BAR; step++)
            put_tone(out, SWC_VOICE_SNARE, step, 1, 0, 0.45 + (step - 8) * 0.055, 0.35);
    }

    if (fill == FILL_LIFT) {
        put_tone(out, SWC_VOICE_TOM, 13, 1, TOM_HIGH, 0.8, 0.6);
        put_tone(out, SWC_VOICE_TOM, 14, 1, TOM_MID, 0.9, 0.6);
        put_tone(out, SWC_VOICE_TOM, 15, 1, TOM_LOW, 1.0, 0.6);
    } else if (fill == FILL_DROP) {
        put_tone(out, SWC_VOICE_TOM, 8, 2, TOM_HIGH, 0.85, 0.6);
        put_tone(out, SWC_VOICE_TOM, 10, 2, TOM_HIGH - 2, 0.85, 0.6);
        put_tone(out, SWC_VOICE_TOM, 14, 1, TOM_MID, 0.9, 0.6);
        put_tone(out, SWC_VOICE_TOM, 15, 1, TOM_LOW, 1.0, 0.6);
    } else if (fill == FILL_TURN) {
        if (kind == SWC_SECTION_CHORUS) {
            put_tone(out, SWC_VOICE_TOM, 10, 1, TOM_HIGH, 0.8, 0.6);
            put_tone(out, SWC_VOICE_TOM, 11, 1, TOM_MID, 0.8, 0.6);
            put_tone(out, SWC_VOICE_SNARE, 14, 1, 0, 0.5, 0.5);
        }
        put_tone(out, SWC_VOICE_SNARE, 15, 1, 0, kind == SWC_SECTION_CHORUS ? 0.72 : 0.55, 0.5);
    }

    /* hats: eighths in a verse, sixteenths in a chorus, with open hats on the off-beats */
    if (groove || intro_groove || break_build) {
        bool sixteenths = kind == SWC_SECTION_CHORUS || (kind == SWC_SECTION_VERSE && variant >= 1 && b >= 4);
        int stop = fill == FILL_LIFT ? 12 : (fill == FILL_DROP ? 8 : SWC_STEPS_PER_BAR);
        double level = kind == SWC_SECTION_INTRO ? 0.7 : (kind == SWC_SECTION_BREAK ? 0.55 : 1.0);
        for (int step = 0; step < stop; step++) {
            bool off_beat = step % 4 == 2;
            bool open;
            if (kind == SWC_SECTION_CHORUS)
                open = off_beat && (variant >= 1 || step == 6 || step == 14);
            else
                open = kind == SWC_SECTION_VERSE && step == 14 && (b & 1) == 1;
            double jitter = 0.94 + 0.12 * swc_rng_next(rng);
            if (open) {
                push(out, SWC_VOICE_OPEN_HAT, step, 2, 0, 0.7 * level * jitter, 0.5, -0.18, 0);
            } else if (sixteenths) {
                double accent = step % 4 == 0 ? 0.62 : (off_beat ? 0.88 : (step % 4 == 1 ? 0.3 : 0.36));
                push(out, SWC_VOICE_HAT, step, 1, 0, accent * level * jitter, 0.5, 0.18, 0);
            } else if (step % 2 == 0) {
                push(out, SWC_VOICE_HAT, step, 1, 0, (off_beat ? 0.85 : 0.5) * level * jitter, 0.5, 0.18, 0);
            }
        }
    }

    /* a later chorus marks its half-way point too */
    if (b == 4 && kind == SWC_SECTION_CHORUS && variant >= 1)
        put(out, SWC_VOICE_CRASH, 0, 12, 0, 0.5);
    if (b == 0) {
        if (kind == SWC_SECTION_CHORUS)
            put(out, SWC_VOICE_CRASH, 0, 12, 0, 0.85);
        else if (kind == SWC_SECTION_VERSE && c->prev_kind != SWC_SECTION_VERSE)
            put(out, SWC_VOICE_CRASH, 0, 12, 0, 0.6);
        else if (kind == SWC_SECTION_BREAK)
            put(out, SWC_VOICE_CRASH, 0, 12, 0, 0.45);
    }

    /* noise sweeps into the next section */
    if (kind == SWC_SECTION_BREAK && b == 6)
        put(out, SWC_VOICE_RISER, 0, 2 * SWC_STEPS_PER_BAR, 0, 0.85);
    else if (kind == SWC_SECTION_VERSE && c->next_kind == SWC_SECTION_CHORUS && b == 7)
        put(out, SWC_VOICE_RISER, 0, SWC_STEPS_PER_BAR, 0, 0.6);
    else if (kind == SWC_SECTION_INTRO && b == 7)
        put(out, SWC_VOICE_RISER, 0, SWC_STEPS_PER_BAR, 0, 0.5);
}

static void bass(SwcBar *out, const SwcSong *song, const BarCtx *c, int b, SwcChord chord) {
    int root = place_at_or_above(song->tonic + degree_to_semitones(chord.root), BASS_LOW);
    if (c->kind == SWC_SECTION_BREAK) { /* one long dark note a bar */
        put_tone(out, SWC_VOICE_BASS, 0, 15.5, root, 0.8, 0.1 + 0.04 * b);
        return;
    }
    if (c->kind == SWC_SECTION_INTRO) {
        if (b < 4)
            return;
        for (int step = 0; step < SWC_STEPS_PER_BAR; step += 2)
            put_tone(out, SWC_VOICE_BASS, step, 1.6, root, step % 4 == 0 ? 0.72 : 0.56, 0.25);
        return;
    }
    const int *pattern = BASS_PATTERNS[c->kind == SWC_SECTION_CHORUS ? song->bass_chorus : song->bass_verse];
    double tone = c->kind == SWC_SECTION_CHORUS ? 0.72 : 0.45 + 0.06 * (c->variant < 2 ? c->variant : 2);
    double level = c->kind == SWC_SECTION_CHORUS ? 1.0 : 0.82;
    for (int step = 0; step < SWC_STEPS_PER_BAR; step++) {
        int offset = pattern[step];
        if (offset < 0)
            continue;
        double accent = step % 4 == 0 ? 1.0 : (step % 4 == 2 ? 0.86 : 0.74);
        put_tone(out, SWC_VOICE_BASS, step, 0.8, root + interval_from(chord.root, offset), accent * level,
                 tone);
    }
}

/* The four notes of a chord, ascending. The triad sits inside one octave so chords move by small steps; the
 * colour note goes on top, an octave higher rather than rub a semitone against the triad. */
static void pad_voicing(int tonic, SwcChord chord, int notes[4]) {
    for (int i = 0; i < 3; i++)
        notes[i] = place_at_or_above(tonic + degree_to_semitones(chord.root + TRIAD[i]), PAD_LOW);
    int colour = place_at_or_above(tonic + degree_to_semitones(chord.root + chord.colour), PAD_COLOUR_LOW);
    for (int i = 0; i < 3; i++)
        if (abs(notes[i] - colour) == 1) {
            colour += 12;
            break;
        }
    notes[3] = colour;
    for (int i = 1; i < 4; i++) { /* insertion sort */
        int v = notes[i], j = i - 1;
        while (j >= 0 && notes[j] > v) {
            notes[j + 1] = notes[j];
            j--;
        }
        notes[j + 1] = v;
    }
}

static void pad(SwcBar *out, const SwcSong *song, const BarCtx *c, int section, int b, SwcChord chord) {
    bool odd = (b & 1) == 1;
    if (odd &&
        same_chord(chord_at(song, section, b - 1), chord)) /* two bars of one chord are one long chord */
        return;
    bool is_long = !odd && same_chord(chord_at(song, section, b + 1), chord);
    double steps = (is_long ? 2 : 1) * SWC_STEPS_PER_BAR - 0.5;
    double tone, velocity;
    int variant = c->variant < 2 ? c->variant : 2;
    if (c->kind == SWC_SECTION_INTRO) {
        tone = 0.2 + 0.04 * b;
        velocity = 0.8;
    } else if (c->kind == SWC_SECTION_VERSE) {
        tone = 0.42 + 0.06 * variant;
        velocity = 0.8;
    } else if (c->kind == SWC_SECTION_CHORUS) {
        tone = 0.75;
        velocity = 1.0;
    } else {
        tone = 0.3 + 0.05 * b;
        velocity = 0.9;
    }
    int notes[4];
    pad_voicing(song->tonic, chord, notes);
    for (int i = 0; i < 4; i++)
        push(out, SWC_VOICE_PAD, 0, steps, notes[i], velocity, tone, PAD_PANS[i % 4], 0);
}

static void arp(SwcBar *out, const SwcSong *song, const BarCtx *c, int b, SwcChord chord, SwcRng *rng) {
    int every;
    double velocity, tone;
    if (c->kind == SWC_SECTION_INTRO) {
        every = b < 4 ? 2 : 1;
        velocity = b < 4 ? 0.5 : 0.6;
        tone = 0.3 + 0.05 * b;
    } else if (c->kind == SWC_SECTION_VERSE) {
        if (c->variant == 0 && b < 4)
            return;
        every = c->variant == 0 ? 2 : 1;
        velocity = c->variant == 0 ? 0.45 : 0.5;
        tone = 0.4;
    } else if (c->kind == SWC_SECTION_CHORUS) {
        every = 1;
        velocity = 0.62;
        tone = 0.6;
    } else {
        every = 1;
        velocity = 0.8;
        tone = 0.25 + 0.09 * b;
    }
    int base = place_at_or_above(song->tonic + degree_to_semitones(chord.root), ARP_LOW);
    for (int step = 0; step < SWC_STEPS_PER_BAR; step += every) {
        int rung = ARP_PATTERNS[song->arp][step];
        int steps = 2 * (rung % 3) + DEGREES * (rung / 3);
        double accent = step % 4 == 0 ? 1.0 : 0.8;
        double jitter = 0.94 + 0.12 * swc_rng_next(rng);
        push(out, SWC_VOICE_ARP, step, 1, base + interval_from(chord.root, steps), velocity * accent * jitter,
             tone, (step & 1) == 1 ? 0.45 : -0.45, 0);
    }
}

/* One bar of the lead. `phrase` 0 states the motif, 1 repeats it with a changed ending, 2 is the contrasting
 * motif, 3 repeats the first and comes to rest on the root or fifth. Notes on beats one and three, long notes
 * and phrase endings go to the nearest chord tone; the rest keep the motif's line. */
static void lead(SwcBar *out, const SwcSong *song, int phrase, int half, SwcChord chord, double velocity,
                 double tone, int lift) {
    const SwcMotif *motif = phrase == 2 ? &song->motif_b : &song->motif_a;
    const SwcMotifNote *notes = motif->note[half == 0 ? 0 : 1];
    int count = motif->count[half == 0 ? 0 : 1];
    int tonic = place_at_or_above(song->tonic, LEAD_LOW_MIDI);
    int previous_end = -1, previous_midi = 0, previous_degree = 0, previous_written = 0;
    bool has_previous = false;
    for (int i = 0; i < count; i++) {
        const SwcMotifNote *n = &notes[i];
        bool last = i == count - 1;
        bool ending = last && half == 1;
        int written = n->degree + (phrase == 2 ? lift : 0) + (phrase == 1 && ending ? song->answer_shift : 0);
        int degree = written;
        if (phrase == 3 && ending) {
            degree = snap_to_chord(degree, chord.root, ROOT_OR_FIFTH, 2);
        } else if (n->step % 8 == 0 || n->length >= LEAD_LONG_NOTE || ending) {
            degree = snap_to_chord(degree, chord.root, TRIAD, 3);
            /* moving to a chord tone must not turn a line that moves into a repeated note */
            if (has_previous && degree == previous_degree && written != previous_written) {
                int direction = written > previous_written ? 1 : -1;
                do
                    degree += direction;
                while (!in_list(TRIAD, 3, mod_i(degree - chord.root, DEGREES)));
            }
        }
        int midi = tonic + degree_to_semitones(degree);
        bool legato = previous_end == n->step && abs(previous_midi - midi) <= 7;
        push(out, SWC_VOICE_LEAD, n->step, n->length * 0.94, midi, velocity * (n->step % 4 == 0 ? 1.0 : 0.88),
             tone, 0.0, legato ? previous_midi - midi : 0);
        previous_end = n->step + n->length;
        previous_midi = midi;
        previous_degree = degree;
        previous_written = written;
        has_previous = true;
    }
}

/* Rare distant fly-bys for the city bed: at most one in any four bars. They follow the bar clock. */
static void ambience(SwcBar *out, uint32_t seed, int bar) {
    int block = bar / 4;
    uint32_t v[3];
    v[0] = seed;
    v[1] = 0xf1bu;
    v[2] = (uint32_t)block;
    SwcRng rng;
    swc_rng_seed(&rng, swc_hash_ints(v, 3));
    if (!swc_rng_chance(&rng, 0.35) || swc_rng_int(&rng, 0, 3) != bar - block * 4)
        return;
    int step = swc_rng_int(&rng, 0, SWC_STEPS_PER_BAR - 1);
    int steps = swc_rng_int(&rng, 20, 34);
    double velocity = swc_rng_range(&rng, 0.5, 1.0);
    double tone = swc_rng_next(&rng);
    double pan = swc_rng_chance(&rng, 0.5) ? 1.0 : -1.0;
    push(out, SWC_VOICE_WHOOSH, step, steps, 0, velocity, tone, pan, 0);
}

static bool is_melodic(int voice) {
    return voice == SWC_VOICE_BASS || voice == SWC_VOICE_PAD || voice == SWC_VOICE_ARP ||
           voice == SWC_VOICE_LEAD;
}

static bool note_before(const SwcNote *a, const SwcNote *b) {
    if (a->beat != b->beat)
        return a->beat < b->beat;
    return a->voice < b->voice;
}

void swc_song_bar(const SwcSong *song, int index, SwcBar *out) {
    int i = index < 0 ? 0 : (index >= SWC_SONG_BARS ? SWC_SONG_BARS - 1 : index);
    int s = i / SWC_BARS_PER_SECTION;
    int b = i - s * SWC_BARS_PER_SECTION;
    BarCtx c;
    c.kind = song->kind[s];
    c.variant = song->variant[s];
    /* After the last section the web version starts a new movement, which always opens with a break or an
     * intro; both give the same drum fill, so a break stands in for it. */
    c.next_kind = s + 1 < SWC_SONG_SECTIONS ? song->kind[s + 1] : SWC_SECTION_BREAK;
    c.prev_kind = s > 0 ? song->kind[s - 1] : -1;
    int transpose = c.kind == SWC_SECTION_CHORUS && c.variant >= 2 ? FINAL_CHORUS_LIFT : 0;
    SwcChord chord = chord_at(song, s, b);

    memset(out, 0, sizeof(*out));
    out->index = i;
    out->section = s;
    out->kind = c.kind;
    out->variant = c.variant;
    out->bar_in_section = b;
    out->tonic = (song->tonic + transpose) % 12;
    out->transpose = transpose;
    out->chord = chord;
    out->energy = bar_energy(c.kind, c.variant, b);

    uint32_t v[3];
    v[0] = song->seed;
    v[1] = 0xba5u;
    v[2] = (uint32_t)i;
    SwcRng base;
    swc_rng_seed(&base, swc_hash_ints(v, 3));
    SwcRng drum_rng = swc_rng_fork(&base, 1u);
    SwcRng arp_rng = swc_rng_fork(&base, 2u);

    drums(out, &c, b, &drum_rng);
    bass(out, song, &c, b, chord);
    pad(out, song, &c, s, b, chord);
    arp(out, song, &c, b, chord, &arp_rng);
    if (c.kind == SWC_SECTION_CHORUS)
        lead(out, song, b >> 1, b & 1, chord, 0.9, song->lead_tone, c.variant >= 1 ? 2 : 0);
    else if (c.kind == SWC_SECTION_VERSE && c.variant >= 1 &&
             b >= 4) /* the second verse hints at the chorus */
        lead(out, song, b < 6 ? 0 : 3, b & 1, chord, 0.6, song->lead_tone - 0.2, 0);
    if (transpose != 0)
        for (int k = 0; k < out->count; k++)
            if (is_melodic(out->notes[k].voice))
                out->notes[k].midi += transpose;
    ambience(out, song->seed, i);

    for (int k = 1; k < out->count; k++) { /* stable insertion sort by beat, then voice */
        SwcNote v2 = out->notes[k];
        int j = k - 1;
        while (j >= 0 && note_before(&v2, &out->notes[j])) {
            out->notes[j + 1] = out->notes[j];
            j--;
        }
        out->notes[j + 1] = v2;
    }
}

static double energy_of_bar(const SwcSong *song, int index) {
    int i = index < 0 ? 0 : (index >= SWC_SONG_BARS ? SWC_SONG_BARS - 1 : index);
    int s = i / SWC_BARS_PER_SECTION;
    return bar_energy(song->kind[s], song->variant[s], i - s * SWC_BARS_PER_SECTION);
}

double swc_song_energy_at(const SwcSong *song, double beat) {
    double position = beat > 0.0 ? beat / SWC_BEATS_PER_BAR : 0.0;
    int index = (int)floor(position);
    double now = energy_of_bar(song, index);
    if (index == 0)
        return now;
    double t = fmin(1.0, (position - index) * SWC_BEATS_PER_BAR); /* ease in over the first beat */
    double before = energy_of_bar(song, index - 1);
    return before + (now - before) * t;
}

int swc_song_synth_notes(const SwcSong *song, int first, int bars, double bpm, SynthNote *out, int cap) {
    static const int voice_map[SWC_VOICE_COUNT] = {-1, -1, -1, -1, -1, -1, -1, 1, 3, 2, 0, -1};
    double spb = 60.0 / bpm;
    int n = 0;
    SwcBar bar;
    for (int k = 0; k < bars; k++) {
        swc_song_bar(song, first + k, &bar);
        double bar_start = (double)k * SWC_BEATS_PER_BAR * spb;
        for (int j = 0; j < bar.count && n < cap; j++) {
            const SwcNote *note = &bar.notes[j];
            int voice = voice_map[note->voice];
            if (voice < 0)
                continue;
            out[n].start = (float)(bar_start + note->beat * spb);
            out[n].dur = (float)(note->duration * spb);
            out[n].pitch = (uint8_t)(note->midi < 0 ? 0 : (note->midi > 127 ? 127 : note->midi));
            out[n].voice = (uint8_t)voice;
            n++;
        }
    }
    return n;
}
