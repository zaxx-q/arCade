/* Internal to core/gfx: the touch front-end (virtual stick and buttons, on-screen keyboard, tap zones).
 * gfx.c calls these at fixed points of the frame. Games and the hub never include this file: they use the
 * arcade_touch_* / arcade_osk_* / arcade_key_pressed / arcade_text_char functions of gfx.h. */
#ifndef ARCADE_GFX_TOUCH_H
#define ARCADE_GFX_TOUCH_H

#include "arcade/gfx.h"
#include "arcade/view.h"

/* Once, after the window exists. The touch UI is on in the Android build, and on a computer when the
 * environment variable ARCADE_TOUCH_UI=1 is set (the left mouse button then plays the part of one finger). */
void gfx_touch_init(bool headless, const ArcView *v);

/* A game loop starts (virtual controls on) or the hub takes over again (arcade_flush_input: controls off, the
 * on-screen keyboard closed, and the fingers that are down are ignored until they are lifted). */
void gfx_touch_game_begin(void);
void gfx_touch_flush(const ArcView *v);

/* Once per drawn frame. begin: the tap zones of the last frame are forgotten (the new draw code registers its
 * own). draw: the controls and the keyboard, inside the design transform, just before EndDrawing. end: right
 * after EndDrawing, when raylib has polled the fingers. */
void gfx_touch_frame_begin(void);
void gfx_touch_draw(void);
void gfx_touch_frame_end(const ArcView *v);

/* arcade_poll: folds the fingers into the keyboard-and-mouse input of this frame. */
void gfx_touch_merge(ArcadeInput *in);

/* True while a smoke test drives the fingers (arcade_touch_test_begin): arcade_poll then reads real input. */
bool gfx_touch_testing(void);

#endif
