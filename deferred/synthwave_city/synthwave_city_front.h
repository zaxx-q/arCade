/* Synthwave City - what the front-end files share. No raylib in this header: plain types only. */
#ifndef SYNTHWAVE_CITY_FRONT_H
#define SYNTHWAVE_CITY_FRONT_H

#include <stdbool.h>

#include "swc_layers.h"
#include "swc_look.h"
#include "swc_quality.h"
#include "synthwave_city_logic.h"

/* Builds the textures the renderer needs. Call once after the window exists. */
void swc_render_init(void);
void swc_render_shutdown(void);

/* ---- the CPU renderer (synthwave_city_render_game.c): works everywhere, even headless ---- */

/* Draws the sky, the city and the traffic as seen from the camera in `s`: everything except the HUD.
 * `time_s` is real time (it drives things that move on their own, like search lights). */
void swc_render_scene(const synthwave_city_State *s, double time_s);

/* The same drawing with hooks for the GPU path, which draws the sky and the post-processing itself.
 * SWC_LEGACY_NO_SKY leaves the sky to the caller; SWC_LEGACY_LINEAR writes linear light (for an HDR target)
 * and takes the haze colour from `look`; SWC_LEGACY_NO_BUILDINGS leaves the buildings (solids, window
 * overlays, roof trims, glows, roof lights) to the caller; SWC_LEGACY_NO_BEAMS does the same for the
 * searchlights, pillar beacons and laser fans; SWC_LEGACY_NO_GROUND for the streets, lamps and the wet
 * mirror; SWC_LEGACY_NO_AIR for the craft of the sky lanes; SWC_LEGACY_NO_STREET for the cars on the streets;
 * SWC_LEGACY_NO_HOLO for the hologram panels, wire solids and ticker rings. */
#define SWC_LEGACY_NO_SKY 1u
#define SWC_LEGACY_LINEAR 2u
#define SWC_LEGACY_NO_BUILDINGS 4u
#define SWC_LEGACY_NO_BEAMS 8u
#define SWC_LEGACY_NO_GROUND 16u
#define SWC_LEGACY_NO_AIR 32u
#define SWC_LEGACY_NO_STREET 64u
#define SWC_LEGACY_NO_HOLO 128u
typedef struct SwcLegacyOptions {
    unsigned flags;
    const SwcLook *look; /* SWC_LEGACY_LINEAR: the live mood */
    /* called once the opaque geometry is drawn, still inside the 3D pass */
    void (*after_solid)(void *user, int w, int h);
    void *user;
} SwcLegacyOptions;
void swc_render_scene_legacy(const synthwave_city_State *s, double time_s, const SwcLegacyOptions *opt);

/* The layers the player has not switched off (the L key, swc_layers.h); both renderers read it and leave out
 * what is cleared, whoever would have drawn it. Everything on by default. */
void swc_live_layers_set(unsigned mask);
unsigned swc_live_layers(void);

/* ---- the GPU renderer (synthwave_city_render_gpu_game.c): OpenGL 3.3 shaders, not under rlsw ---- */

/* True when this build runs on a real OpenGL 3.3 context. */
bool swc_gpu_available(void);
/* Builds the shaders and render targets. Returns false (and leaves nothing behind) if the GPU path cannot
 * run. */
bool swc_gpu_init(void);
void swc_gpu_shutdown(void);
/* Draws the same frame as swc_render_scene, with the web version's sky and post-processing. */
void swc_render_scene_gpu(const synthwave_city_State *s, double time_s);
/* Debug switches for the verification harness (headless runs only; ignored otherwise). */
typedef struct SwcGpuDebug {
    int layers;   /* bit 0 sky, bit 1 legacy city (traffic, sites, whatever the GPU does not draw yet), bit 2
                     buildings, bit 3 beams, bit 4 ground (streets, lamps, wet mirror), bit 5 air traffic,
                     bit 6 street life (cars and people), bit 7 holograms; default   all (255) */
    bool post;    /* false: write the HDR frame straight to the screen, no bloom, grade or aberration */
    bool bloom;   /* false: no bloom */
    bool reflect; /* false: the wet streets show no mirror image */
    bool msaa;    /* false: no multisampling whatever the tier says */
} SwcGpuDebug;
void swc_gpu_set_debug(const SwcGpuDebug *d);

/* The quality tier (swc_quality.h), "high" until set: density of the traffic, crowd, lamps, beams and ads,
 * the far fade, the mirror, MSAA and the bounds of the adaptive render scale. A change takes effect on the
 * next frame (the layers pack again; the ad atlas is kept). */
void swc_gpu_set_tier(SwcTier tier);
SwcTier swc_gpu_tier(void);
/* A real window (true) or a harness run (false, the default). A real window adapts the render scale to how
 * long frames really take and spreads the start-up work (the ad atlas) over frames. A harness run keeps the
 * scale at the tier's maximum, whatever its frame times, and finishes everything in the first frame. */
void swc_gpu_set_interactive(bool on);
/* How long the last frame really took, in seconds; the adaptive scale reads it. Call once per frame. */
void swc_gpu_note_frame(double seconds);
/* What the HUD shows: the scene's share of the window's pixels per side (1 = every pixel) and the MSAA
 * samples in use (0 = none). */
double swc_gpu_render_scale(void);
int swc_gpu_msaa_samples(void);

/* Names for the HUD. */
const char *swc_mood_name(int mood);

#endif
