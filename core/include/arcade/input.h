/* Device-independent input snapshot used by ALL game logic.
 * The raylib layer (gfx.h: arcade_poll) fills it from keyboard/mouse; tests and CI smoke runs
 * fill it from the "monkey" (random but reproducible input). Logic never touches the keyboard. */
#ifndef ARCADE_INPUT_H
#define ARCADE_INPUT_H

#include "arcade/common.h"
#include "arcade/rng.h"

typedef struct ArcadeInput {
    float move_x, move_y;                                       /* -1..1  WASD / arrow keys   */
    float aim_x, aim_y;                                         /* pointer position in pixels */
    bool fire, fire_pressed;                                    /* left mouse button          */
    bool alt, alt_pressed;                                      /* right mouse button         */
    bool action, action_pressed;                                /* Space / Enter              */
    bool left_pressed, right_pressed, up_pressed, down_pressed; /* one-shot direction presses */
    int wheel;                                                  /* mouse wheel steps          */
    int typed; /* last printable ASCII key typed this frame, 0 = none */
} ArcadeInput;

void arcade_input_clear(ArcadeInput *in);

typedef struct ArcadeMonkey {
    Rng rng;
    int hold;
    float mx, my;
    bool firing;
} ArcadeMonkey;

void arcade_monkey_init(ArcadeMonkey *m, unsigned seed);
void arcade_monkey_next(ArcadeMonkey *m, ArcadeInput *in); /* next frame of random input */

#endif
