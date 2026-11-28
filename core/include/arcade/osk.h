/* An on-screen keyboard for touch screens (Android has none of its own inside a raylib window). Pure C99: no
 * raylib in this file; the raylib side (core/gfx/gfx_touch.c) draws the keys and feeds in the taps.
 *
 * Two pages: letters and digits, and symbols. Together they hold every character the hub's name and search
 * lines accept (letters, digits, space and common punctuation) and every hotkey the games listen for
 * (letters, digits and - + = [ ] < > / ^ #). Shift is one-shot: it applies to the next letter.
 * All positions are design units. */
#ifndef ARCADE_OSK_H
#define ARCADE_OSK_H

#include "arcade/common.h"

#define OSK_MAX_KEYS 48

typedef enum OskKind {
    OSK_CHAR,      /* types `ch` (a letter is upper case while shift is on) */
    OSK_SHIFT,     /* next letter in upper case */
    OSK_PAGE,      /* letters <-> symbols */
    OSK_BACKSPACE, /* erases the last character */
    OSK_SPACE,     /* types ' ' */
    OSK_ENTER      /* accepts the line */
} OskKind;

typedef struct OskKey {
    float x, y, w, h;
    OskKind kind;
    char ch;       /* OSK_CHAR: the character, lower case for a letter */
    char label[8]; /* what is drawn on the key (already upper case while shift is on) */
} OskKey;

typedef struct Osk {
    OskKey keys[OSK_MAX_KEYS];
    int count;
    bool shift;              /* the next letter is upper case */
    bool symbols;            /* the symbols page is showing */
    float cx, bottom, max_w; /* where it sits (see osk_layout), so a page change can rebuild it */
    float x, y, w, h;        /* the rectangle that holds all the keys */
} Osk;

/* What a tap on a key produced. At most one of the fields is set. */
typedef struct OskEvent {
    int ch; /* a printable character (32..126), 0 = none */
    bool backspace;
    bool enter;
} OskEvent;

void osk_init(Osk *k);

/* Builds the keys of the current page: centred on cx, its bottom edge on `bottom`, at most max_w wide. */
void osk_layout(Osk *k, float cx, float bottom, float max_w);

/* The key under (x, y), or -1. */
int osk_hit(const Osk *k, float x, float y);

/* Taps key `index`: applies shift and the page switch, and says what was typed (ev may be NULL). */
void osk_press(Osk *k, int index, OskEvent *ev);

#endif
