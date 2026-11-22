/* Synthwave City - what the two front-end files share. No raylib in this header: plain types only. */
#ifndef SYNTHWAVE_CITY_FRONT_H
#define SYNTHWAVE_CITY_FRONT_H

#include "synthwave_city_logic.h"

/* Builds the textures the renderer needs. Call once after the window exists. */
void swc_render_init(void);
void swc_render_shutdown(void);

/* Draws the sky, the city and the traffic as seen from the camera in `s`: everything except the HUD.
 * `time_s` is real time (it drives things that move on their own, like search lights). */
void swc_render_scene(const synthwave_city_State *s, double time_s);

/* Names for the HUD. */
const char *swc_mood_name(int mood);

#endif
