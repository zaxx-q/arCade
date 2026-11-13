/* Gritty Golf - private interface between gritty_golf_game.c and its raylib helper files
 * (gritty_golf_render_game.c draws, gritty_golf_audio_game.c plays sound).
 *
 * No raylib types appear here on purpose: the repo rule keeps raylib out of every header except gfx.h,
 * so the picture and the sound are reached only through these plain-C functions. */
#ifndef GRITTY_GOLF_FRONT_H
#define GRITTY_GOLF_FRONT_H

#include "gg/audio.h" /* Audio_Init / Audio_Shutdown / Audio_ToggleMute / Audio_PlaySfx (upstream interface) */
#include "gritty_golf_logic.h"

/* Loads textures, the two bitmap fonts and the 256x256 render target from `dir` (= <assets>/gritty_golf).
 * On a missing file it prints which one to stderr and returns false. */
bool gritty_golf_front_init(const char *dir);
void gritty_golf_front_shutdown(void);

/* Draws the 256x256 game picture into its render texture. `mouse` is in canvas pixels (hover highlights).
 * Call it BEFORE BeginDrawing(): a render texture may not be active while the screen frame is. */
void gritty_golf_front_render_canvas(const gritty_golf_State *s, Vec2 mouse);

/* Draws the arCade screen: backdrop, HUD bar, side panels and the canvas at 2x. Call it between
 * BeginDrawing() and arcade_frame_end(). `mouse` is in screen pixels, `t` is seconds since start. */
void gritty_golf_front_draw_screen(const gritty_golf_State *s, Vec2 mouse, float t);

#endif
