#include "arcade/midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

typedef struct Reader {
    const uint8_t *p, *end;
} Reader;

static bool fail(MidiSong *s, const char *msg) {
    arc_strlcpy(s->error, msg, sizeof(s->error));
    return false;
}

static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static uint32_t be16(const uint8_t *p) {
    return ((uint32_t)p[0] << 8) | (uint32_t)p[1];
}

static bool rd_u8(Reader *r, uint8_t *v) {
    if (r->p >= r->end)
        return false;
    *v = *r->p++;
    return true;
}

/* variable-length quantity: 7 bits per byte, high bit = "more bytes follow" */
static bool rd_vlq(Reader *r, uint32_t *v) {
    uint32_t out = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t b;
        if (!rd_u8(r, &b))
            return false;
        out = (out << 7) | (b & 0x7Fu);
        if (!(b & 0x80u)) {
            *v = out;
            return true;
        }
    }
    return false;
}

static bool parse_track(const uint8_t *data, size_t len, MidiSong *song, bool *first_tempo_seen) {
    Reader r = {data, data + len};
    MidiTrack *tr =
        &song->tracks[song->track_count < MIDI_MAX_TRACKS ? song->track_count : MIDI_MAX_TRACKS - 1];
    bool room = song->track_count < MIDI_MAX_TRACKS;
    int pending[128]; /* index of the sounding note per pitch, -1 = none */
    for (int i = 0; i < 128; i++)
        pending[i] = -1;
    if (room) {
        tr->name[0] = '\0';
        tr->note_count = 0;
    }
    uint32_t tick = 0;
    uint8_t status = 0;
    bool had_notes = false;

    while (r.p < r.end) {
        uint32_t delta;
        if (!rd_vlq(&r, &delta))
            return fail(song, "truncated delta time");
        tick += delta;
        uint8_t b;
        if (!rd_u8(&r, &b))
            return fail(song, "truncated event");
        if (b & 0x80u)
            status = b;
        else if (status == 0)
            return fail(song, "data byte without running status");
        else
            r.p--; /* running status: this byte is the first data byte */

        if (status == 0xFF) { /* meta event */
            uint8_t type;
            uint32_t mlen;
            if (!rd_u8(&r, &type) || !rd_vlq(&r, &mlen) || (size_t)(r.end - r.p) < mlen)
                return fail(song, "truncated meta event");
            if (type == 0x03 && room) {
                size_t n = mlen < sizeof(tr->name) - 1 ? mlen : sizeof(tr->name) - 1;
                memcpy(tr->name, r.p, n);
                tr->name[n] = '\0';
            } else if (type == 0x51 && mlen == 3) {
                uint32_t us = ((uint32_t)r.p[0] << 16) | ((uint32_t)r.p[1] << 8) | r.p[2];
                float bpm = us > 0 ? 60000000.0f / (float)us : 120.0f;
                if (!*first_tempo_seen) {
                    song->bpm = bpm;
                    *first_tempo_seen = true;
                } else if (bpm > song->bpm + 0.01f || bpm < song->bpm - 0.01f) {
                    song->tempo_changes++;
                }
            }
            r.p += mlen;
            if (type == 0x2F)
                break;                                 /* end of track */
        } else if (status == 0xF0 || status == 0xF7) { /* sysex: skip */
            uint32_t slen;
            if (!rd_vlq(&r, &slen) || (size_t)(r.end - r.p) < slen)
                return fail(song, "truncated sysex");
            r.p += slen;
        } else {
            uint8_t kind = status & 0xF0u, d1, d2 = 0;
            if (!rd_u8(&r, &d1))
                return fail(song, "truncated channel event");
            if (kind != 0xC0u && kind != 0xD0u && !rd_u8(&r, &d2))
                return fail(song, "truncated channel event");
            if (kind == 0x90u && d2 > 0) { /* note on */
                if (!room)
                    return fail(song, "too many tracks with notes");
                if (pending[d1 & 0x7F] >= 0) /* same pitch still sounding: close it first */
                    tr->notes[pending[d1 & 0x7F]].dur = tick - tr->notes[pending[d1 & 0x7F]].start;
                if (tr->note_count >= MIDI_MAX_NOTES)
                    return fail(song, "too many notes in one track");
                MidiNote *n = &tr->notes[tr->note_count];
                n->start = tick;
                n->dur = 0;
                n->pitch = (uint8_t)(d1 & 0x7F);
                pending[d1 & 0x7F] = tr->note_count++;
                had_notes = true;
            } else if (kind == 0x80u || (kind == 0x90u && d2 == 0)) { /* note off */
                int idx = pending[d1 & 0x7F];
                if (idx >= 0) {
                    tr->notes[idx].dur = tick - tr->notes[idx].start;
                    pending[d1 & 0x7F] = -1;
                }
            }
        }
    }
    for (int i = 0; i < 128; i++) /* notes that never got a note-off end with the track */
        if (pending[i] >= 0 && room)
            tr->notes[pending[i]].dur = tick - tr->notes[pending[i]].start;
    if (had_notes && room)
        song->track_count++;
    return true;
}

bool midi_parse(const uint8_t *data, size_t size, MidiSong *out) {
    memset(out, 0, sizeof(*out));
    out->bpm = 120.0f;
    if (size < 14 || memcmp(data, "MThd", 4) != 0 || be32(data + 4) < 6)
        return fail(out, "not a MIDI file (missing MThd header)");
    unsigned format = be16(data + 8), ntracks = be16(data + 10), division = be16(data + 12);
    if (format > 1)
        return fail(out, "MIDI format 2 is not supported");
    if (division & 0x8000u)
        return fail(out, "SMPTE time division is not supported");
    if (division == 0)
        return fail(out, "invalid ticks per beat");
    out->ticks_per_beat = (int)division;

    size_t pos = 8u + be32(data + 4);
    bool tempo_seen = false;
    for (unsigned t = 0; t < ntracks; t++) {
        if (pos + 8 > size || memcmp(data + pos, "MTrk", 4) != 0)
            return fail(out, "missing MTrk chunk");
        uint32_t len = be32(data + pos + 4);
        pos += 8;
        if (len > size - pos)
            return fail(out, "track length exceeds the file size");
        if (!parse_track(data + pos, len, out, &tempo_seen))
            return false;
        pos += len;
    }
    if (out->track_count == 0)
        return fail(out, "the file contains no notes");
    return true;
}

bool midi_load(const char *path, MidiSong *out) {
    memset(out, 0, sizeof(*out));
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return fail(out, "cannot open the file");
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0 || size > 4 * 1024 * 1024) {
        fclose(fp);
        return fail(out, "file is empty or too large");
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)size);
    if (!buf) {
        fclose(fp);
        return fail(out, "out of memory");
    }
    size_t got = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    bool ok = got == (size_t)size && midi_parse(buf, got, out);
    if (got != (size_t)size)
        fail(out, "could not read the whole file");
    free(buf);
    return ok;
}
