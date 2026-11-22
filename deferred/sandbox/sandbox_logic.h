/* Sandbox - PURE game logic (no raylib). Owner: Leader
 * A falling-sand toy built on core/sandbox (the same engine animates the hub background).
 * NOTE: functions use the prefix sbx_ because sandbox_* is already taken by the core engine. */
#ifndef SANDBOX_LOGIC_H
#define SANDBOX_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/sandbox.h"

#define SBX_W 320
#define SBX_H 155
#define SBX_CELL 4 /* 320*4 = 1280, 155*4 = 620: the grid fills the window above the palette panel */

/* The element palette is a grid of clickable slots along the bottom of the window. Slot i holds the i-th
 * visible element (sandbox_palette_id), so the first row is exactly the number keys 0-9. The geometry lives
 * here, not in the drawing code, so clicks can be tested without raylib. */
#define SBX_PAL_COLS 10
#define SBX_PAL_ROWS 4
#define SBX_PAL_SLOT_W 124
#define SBX_PAL_SLOT_H 23
#define SBX_PAL_PAD 6
#define SBX_PAL_X ((ARC_SCREEN_W - SBX_PAL_COLS * SBX_PAL_SLOT_W) / 2)
#define SBX_PAL_Y (SBX_H * SBX_CELL + SBX_PAL_PAD) /* the panel starts right where the world ends */

typedef struct sbx_State {
    Sandbox world;
    ElementId element;  /* what the left mouse button paints (never a hidden element) */
    ElementId element2; /* what the right mouse button paints: the eraser until another one is assigned */
    int brush;          /* brush radius in cells */
    bool paused;
    bool pal_hold;  /* the left press started on the palette: it must not paint when dragged onto the world */
    bool pal_hold2; /* the same for the right button */
    unsigned ticks;
} sbx_State;

void sbx_init(sbx_State *s, unsigned seed);
void sbx_step(sbx_State *s, const ArcadeInput *in, float dt);
bool sbx_is_over(const sbx_State *s); /* a sandbox never ends: always false */
int sbx_score(const sbx_State *s);    /* number of non-empty cells (unranked, just for display) */

/* Palette helpers. Pointer positions are in window pixels. */
bool sbx_over_panel(float px, float py);           /* is the pointer on the palette panel? */
int sbx_palette_slot_at(float px, float py);       /* palette index under the pointer, or -1 */
void sbx_select(sbx_State *s, int palette_index);  /* left mouse: ignores out-of-range indices */
void sbx_select2(sbx_State *s, int palette_index); /* right mouse: the same for the second slot */

/* What is under the pointer: the element and its temperature in degrees Celsius. False when the pointer is
 * not over the world (on the palette, or outside the window). Empty space reports EL_EMPTY at the air
 * temperature. */
bool sbx_probe(const sbx_State *s, float px, float py, ElementId *id, int *temp_c);

#endif
