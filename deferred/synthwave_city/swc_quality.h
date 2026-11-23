/* Synthwave City - quality tiers and the adaptive render scale of the GPU renderer. PURE C99, no raylib.
 *
 * The three tiers and their numbers are those of the web version's `contracts/quality.ts`; the adaptive scale
 * is a port of its `post/adaptive.ts`. The renderer reads a tier through swc_quality(); the HUD and the tests
 * use the names and the pure helpers here, so they agree with the renderer. */
#ifndef SWC_QUALITY_H
#define SWC_QUALITY_H

#include <stdbool.h>

typedef enum SwcTier { SWC_TIER_LOW = 0, SWC_TIER_MEDIUM = 1, SWC_TIER_HIGH = 2 } SwcTier;
#define SWC_TIER_COUNT 3

typedef struct SwcQuality {
    SwcTier tier;
    const char *name;        /* "low", "medium", "high": the names of the web version */
    double max_pixel_ratio;  /* cap on the window's pixels per point (2 on a retina screen) */
    int msaa;                /* samples on the scene target; 0 is off */
    double reflection_scale; /* the wet streets' mirror relative to the frame; 0 turns the mirror pass off */
    double density;          /* multiplier on instance counts: traffic, crowds, lamps, beams, holograms */
    double fade_start;       /* far fade: heights start sinking here (metres) ... */
    double fade_end;         /* ... and reach zero here */
    bool crowds;             /* people on the sidewalks */
    bool sun_shafts;         /* kept from the table; the web version does not use it either */
    double min_render_scale; /* bounds of the adaptive render scale */
    double max_render_scale;
} SwcQuality;

/* The tier's settings (an out-of-range tier gives "high"). The pointer stays valid for the whole run. */
const SwcQuality *swc_quality(SwcTier tier);

/* What the Q key does: high -> medium -> low -> high, the way a player who sees a slow frame rate wants it.
 */
SwcTier swc_tier_next(SwcTier tier);
/* "LOW", "MEDIUM", "HIGH" for the HUD. */
const char *swc_tier_label(SwcTier tier);
/* Reads "low", "medium" or "high" (any case); false and `*out` untouched for anything else. */
bool swc_tier_from_name(const char *name, SwcTier *out);

/* Share of the street-life crowd that is kept: the web version's round(CROWD_PER_FACE * density), or 0 when
 * the tier has no crowds. `per_face_full` is the crowd per sidewalk face at full density. */
int swc_quality_crowd_per_face(const SwcQuality *q, int per_face_full);
/* Where the street layer hands its near cars over to the far traffic: min(handover_end, fade_start * 0.8). */
double swc_quality_handover_end(const SwcQuality *q, double handover_end);

/* The size the scene is drawn at. `screen` is the window in points, `render` its size in pixels (they differ
 * on a high-DPI screen), `scale` the adaptive render scale. The pixel ratio is the window's, capped by the
 * tier, times the scale; sizes are rounded down, never below 1. */
void swc_quality_render_size(const SwcQuality *q, int screen_w, int screen_h, int render_w, int render_h,
                             double scale, int *w, int *h);
/* Size of the mirror target for a frame of w x h (0 x 0 when the tier has no mirror). */
void swc_quality_mirror_size(const SwcQuality *q, int w, int h, int *mw, int *mh);

/* ---- the adaptive render scale (the web version's AdaptiveScale) ----
 * Under vsync a frame is never faster than the display, so headroom cannot be measured. The scale therefore
 * steps down quickly when frames run long, and only tries a step up after a calm spell; a step up that has to
 * be undone is remembered, and the next attempt waits longer. */

#define SWC_ADAPTIVE_START_GRACE 3.0 /* seconds after the start whose frame times are ignored */

typedef struct SwcAdaptiveScale {
    double scale;
    double min, max;
    double target_ms; /* frame time to hold */
    double step;
    double ema, over, calm, cooldown, since_step_up, retry_after, age;
} SwcAdaptiveScale;

void swc_adaptive_init(SwcAdaptiveScale *a, double min, double max);
/* A new tier's bounds: it starts at its own best scale (the one the web version starts at; the adaptive loop
 * steps down when frames run long) and the timers start again (the grace is not repeated). */
void swc_adaptive_set_range(SwcAdaptiveScale *a, double min, double max);
/* frame_ms: time the last frame took. dt: seconds since the last call. True when the scale changed. */
bool swc_adaptive_update(SwcAdaptiveScale *a, double frame_ms, double dt);

#endif
