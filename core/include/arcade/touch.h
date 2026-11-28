/* Virtual controls for touch screens (Android). Pure C99: no raylib in this file.
 *
 * A phone has no keyboard and no mouse, but every game reads the same ArcadeInput. This module turns the
 * fingers on the glass into the fields the games already know:
 *
 *   stick   (bottom left)   -> move_x / move_y and the one-shot left/right/up/down presses
 *   action  (bottom right)  -> action / action_pressed (Space / Enter)
 *   back    (above the stick)   -> "leave this game" (what Esc does on a computer)
 *   keys    (above the action) -> "show the on-screen keyboard" (the games' hotkeys: P, R, M ...)
 *   any other finger        -> the pointer: aim_x / aim_y and fire / fire_pressed (the left mouse button)
 *
 * A finger belongs to the control it first landed on until it is lifted: a finger that slides out of the
 * stick keeps steering, and a finger that slides onto the action button does not press it. All positions are
 * design units (the 1280 x 720 canvas, see view.h); the raylib side converts window pixels before calling in.
 */
#ifndef ARCADE_TOUCH_H
#define ARCADE_TOUCH_H

#include "arcade/common.h"

#define TOUCH_MAX_POINTS 10 /* fingers followed at once; more are ignored */

typedef struct TouchPoint {
    int id; /* the finger's number: the same finger keeps it from touch down to touch up */
    float x, y;
} TouchPoint;

typedef struct TouchCircle {
    float x, y, r;
} TouchCircle;

typedef struct TouchLayout {
    TouchCircle stick, action, back, keys;
} TouchLayout;

/* Places the controls inside the window rectangle left..right, top..bottom (design units; they can reach
 * past the canvas on a window that is wider or taller than 16:9). */
void touch_layout_fit(TouchLayout *l, float left, float top, float right, float bottom);

typedef enum TouchKind { TOUCH_STICK = 1, TOUCH_ACTION, TOUCH_POINTER, TOUCH_IGNORED } TouchKind;

typedef struct TouchHold {
    int id;
    TouchKind kind;
} TouchHold;

typedef struct TouchState {
    TouchLayout layout;
    TouchHold hold[TOUCH_MAX_POINTS]; /* the fingers on the glass and what each one is doing */
    int hold_n;
    bool dir[4];              /* stick direction of the last frame: left, right, up, down */
    float aim_x, aim_y;       /* where the pointer was last (stays when the finger is lifted) */
    float knob_x, knob_y;     /* -1..1: where the stick's knob is, for drawing */
    bool stick_on, action_on; /* a finger is holding the control, for drawing */
} TouchState;

/* What one frame of touches means. */
typedef struct TouchOut {
    float move_x, move_y; /* -1..1, y grows downwards like the screen */
    bool left_pressed, right_pressed, up_pressed, down_pressed;
    bool action, action_pressed;
    bool back_pressed;
    bool keys_pressed;
    float aim_x, aim_y;
    bool fire, fire_pressed;
} TouchOut;

void touch_init(TouchState *t);

/* One frame: `pts` are the fingers on the glass right now (n of them). */
void touch_update(TouchState *t, const TouchPoint *pts, int n, TouchOut *out);

/* Forgets every finger (the controls are hidden, or the screen changed). */
void touch_release_all(TouchState *t);

#endif
