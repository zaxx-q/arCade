/* Synthwave City as a guest: the whole game (city, traffic, flight, tour, soundtrack, HUD) running inside
 * somebody else's loop. The hub uses it as the picture behind its menu (app/backdrop.c): the drone tour flies
 * by itself while the menu is up, and the player can take the camera and play the game as usual.
 *
 * One guest at a time. It keeps its own state and soundtrack, so the game can still be started on its own
 * through play() (from the hub's archive, or by a smoke run) while the guest exists.
 * No raylib in this header: plain types only. The implementation is in synthwave_city_game.c. */
#ifndef SYNTHWAVE_CITY_SESSION_H
#define SYNTHWAVE_CITY_SESSION_H

#include "arcade/common.h"
#include "arcade/input.h"

/* Builds the city, the renderer and (when an audio device exists) the soundtrack, then starts the drone tour.
 * Call it after the window exists. Calling it again replaces the guest. */
void swc_session_begin(unsigned seed);
void swc_session_end(void);
bool swc_session_active(void);

/* Who holds the camera.
 *   false: the host's own screen is in front. The drone tour flies the camera by itself (it takes the camera
 *          back if the player left it somewhere else), the soundtrack is turned down and the HUD is hidden.
 *   true:  the player flies. Every update reads the real input, the HUD is shown (its help text restarts) and
 *          the soundtrack is at full volume. */
void swc_session_set_control(bool player);

/* One fixed step of ARC_DT. `in` is only read while the player has control (it may be NULL otherwise). */
void swc_session_update(const ArcadeInput *in, float dt);

/* Draws the sky, the city and the traffic, plus the HUD while the player has control. Inside a begun frame.
 */
void swc_session_draw(void);

/* The soundtrack. Pausing is for the host starting another game: it must be silent while that one plays. */
void swc_session_pause_audio(bool paused);
void swc_session_set_muted(bool muted);
bool swc_session_muted(void);

/* True while the drone flies the camera (the player has not taken it, or gave it back). */
bool swc_session_touring(void);

/* True while the mouse is grabbed (the cursor is hidden). The hub uses this to route ESC. */
bool swc_session_grabbed(void);

/* Frees the mouse (EnableCursor, pointer_lock off). The hub calls this when ESC is pressed while grabbed:
 * the first ESC frees the mouse, the next one gives the menu back. Also called when the player gives back
 * control to the drone. */
void swc_session_mouse_free(void);

#endif
