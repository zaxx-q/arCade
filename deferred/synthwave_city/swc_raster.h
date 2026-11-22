/* Synthwave City - a small software 2D rasteriser with the drawing model of a canvas context: paths, fills,
 * strokes, linear gradients, clips and a transform stack. PURE C99, no raylib.
 *
 * The hologram ad atlas is drawn with it at start-up (see swc_atlas.h), so no image or font file is needed.
 * Coverage is exact horizontally and sampled 16 times vertically, which gives clean anti-aliased edges.
 * Pixels are RGBA, 8 bits, and every draw composites source-over; the canvas starts opaque black. */
#ifndef SWC_RASTER_H
#define SWC_RASTER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct SwcCanvas SwcCanvas;

typedef enum SwcFillRule { SWC_NONZERO = 0, SWC_EVENODD } SwcFillRule;
typedef enum SwcLineCap { SWC_CAP_BUTT = 0, SWC_CAP_ROUND, SWC_CAP_SQUARE } SwcLineCap;
typedef enum SwcLineJoin { SWC_JOIN_MITER = 0, SWC_JOIN_ROUND, SWC_JOIN_BEVEL } SwcLineJoin;

#define SWC_GRADIENT_STOPS 6

/* A linear gradient between two points (in the user space of the moment it is used), with colour stops at
 * positions 0..1 in increasing order. Colours are 0xRRGGBB. */
typedef struct SwcGradient {
    double x0, y0, x1, y1;
    int stop_count;
    double stop_at[SWC_GRADIENT_STOPS];
    uint32_t stop_rgb[SWC_GRADIENT_STOPS];
} SwcGradient;

/* A canvas of width x height pixels, opaque black. Returns NULL when out of memory. */
SwcCanvas *swc_cv_create(int width, int height);
void swc_cv_destroy(SwcCanvas *cv);
int swc_cv_width(const SwcCanvas *cv);
int swc_cv_height(const SwcCanvas *cv);
/* RGBA8, row 0 at the top, rows tightly packed. */
const uint8_t *swc_cv_pixels(const SwcCanvas *cv);
/* Frees the clip masks made so far; only valid with no saved state (between two drawings). */
void swc_cv_release_clips(SwcCanvas *cv);

/* ---- state ---- */
void swc_cv_save(SwcCanvas *cv);
void swc_cv_restore(SwcCanvas *cv);
void swc_cv_translate(SwcCanvas *cv, double x, double y);
void swc_cv_scale(SwcCanvas *cv, double sx, double sy);
void swc_cv_rotate(SwcCanvas *cv, double radians);
/* Shears x by -k * y (so with y up the screen being negative, a positive k leans the picture to the right).
 */
void swc_cv_skew(SwcCanvas *cv, double k);

void swc_cv_fill_rgb(SwcCanvas *cv, uint32_t rgb);
void swc_cv_stroke_rgb(SwcCanvas *cv, uint32_t rgb);
void swc_cv_fill_gradient(SwcCanvas *cv, const SwcGradient *g);
void swc_cv_stroke_gradient(SwcCanvas *cv, const SwcGradient *g);
void swc_cv_line_width(SwcCanvas *cv, double w);
void swc_cv_line_cap(SwcCanvas *cv, SwcLineCap cap);
void swc_cv_line_join(SwcCanvas *cv, SwcLineJoin join);
void swc_cv_miter_limit(SwcCanvas *cv, double limit);

/* A two-stop gradient, as the canvas's createLinearGradient + two addColorStop calls. */
SwcGradient swc_gradient2(double x0, double y0, double x1, double y1, uint32_t from, uint32_t to);

/* ---- paths ---- */
void swc_cv_begin_path(SwcCanvas *cv);
void swc_cv_move_to(SwcCanvas *cv, double x, double y);
void swc_cv_line_to(SwcCanvas *cv, double x, double y);
void swc_cv_close_path(SwcCanvas *cv);
void swc_cv_rect(SwcCanvas *cv, double x, double y, double w, double h);
void swc_cv_arc(SwcCanvas *cv, double x, double y, double r, double a0, double a1, bool counter_clockwise);
void swc_cv_ellipse(SwcCanvas *cv, double x, double y, double rx, double ry, double rotation, double a0,
                    double a1, bool counter_clockwise);
void swc_cv_arc_to(SwcCanvas *cv, double x1, double y1, double x2, double y2, double r);
void swc_cv_quad_to(SwcCanvas *cv, double cx, double cy, double x, double y);
void swc_cv_bezier_to(SwcCanvas *cv, double c1x, double c1y, double c2x, double c2y, double x, double y);

void swc_cv_fill(SwcCanvas *cv, SwcFillRule rule);
void swc_cv_stroke(SwcCanvas *cv);
void swc_cv_clip(SwcCanvas *cv, SwcFillRule rule);
void swc_cv_fill_rect(SwcCanvas *cv, double x, double y, double w, double h);

/* ---- extras for text drawn as strokes ---- */
/* Strokes (and fills) paint with the current FILL style instead of the stroke style until it is switched
 * back. */
void swc_cv_stroke_with_fill_style(SwcCanvas *cv, bool on);
/* While set, strokes are cut off outside the box x0..x1, y0..y1 of user space. */
void swc_cv_stroke_box(SwcCanvas *cv, bool on, double x0, double y0, double x1, double y1);
/* While > 0, a stroke leaves a hole: it keeps only what lies outside the same stroke made `inner` wide, so a
 * thick stroke becomes an outline. */
void swc_cv_stroke_hole(SwcCanvas *cv, double inner);

#endif
