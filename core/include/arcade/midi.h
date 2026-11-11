/* Minimal Standard MIDI File reader (pure C): extracts the note events of every track that has notes.
 * Supports formats 0 and 1 with a ticks-per-beat time division. Enough for tools/midi2beatmap.c. */
#ifndef ARCADE_MIDI_H
#define ARCADE_MIDI_H

#include "arcade/common.h"

#define MIDI_MAX_TRACKS 8
#define MIDI_MAX_NOTES 2048

typedef struct MidiNote {
    uint32_t start, dur; /* ticks */
    uint8_t pitch;
} MidiNote;

typedef struct MidiTrack {
    char name[40];
    int note_count;
    MidiNote notes[MIDI_MAX_NOTES]; /* in start order */
} MidiTrack;

typedef struct MidiSong {
    int ticks_per_beat;
    float bpm;         /* the FIRST tempo event (120 if the file has none) */
    int tempo_changes; /* number of later tempo events that differ from the first */
    int track_count;   /* only tracks that contain notes */
    MidiTrack tracks[MIDI_MAX_TRACKS];
    char error[96]; /* filled when loading fails */
} MidiSong;

bool midi_parse(const uint8_t *data, size_t size, MidiSong *out); /* from memory */
bool midi_load(const char *path, MidiSong *out);                  /* from a file */

#endif
