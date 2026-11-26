/* Synthwave City - the hologram ad atlas: every advert, drawn by code once at start-up. PURE C99, no raylib.
 * Ported from the web version (src/layers/holograms/atlas.ts), which draws them with a canvas 2D context.
 *
 * The canvas does not hold finished colours but three INKS, one per channel: red is the panel's first palette
 * colour (ink A), green its second (ink B), blue is white-hot. The panel shader mixes them into light, so one
 * advert appears in many colourways. Black is "no light". Every brand is invented.
 *
 * The art uses only strokes, fills and gradients (swc_raster.h) and the stroke font (swc_font.h); no image
 * or font file is read. The letter shapes are this port's own, so text is laid out like the web version's but
 * not drawn with the same glyphs. */
#ifndef SWC_ATLAS_H
#define SWC_ATLAS_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_holo.h"
#include "swc_raster.h"

/* A new SWC_ATLAS_SIZE x SWC_ATLAS_SIZE canvas of black, or NULL when out of memory. */
SwcCanvas *swc_atlas_create(void);

/* Draws one cell's advert. `layer_seed` is the holograms layer's seed (swc_layer_seed(seed, "holograms")).
 * Cells are independent, so they may be drawn in any order, spread over several frames. */
void swc_atlas_draw_cell(SwcCanvas *cv, const SwcAtlasCell *cell, uint32_t layer_seed);

/* The whole atlas at once; NULL when out of memory. */
SwcCanvas *swc_atlas_draw_all(uint32_t layer_seed);

/* ---- the same work in small pieces, so a real window can spread it over frames ---- */

typedef struct SwcAtlasJob {
    SwcCanvas *cv;
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int cell_count;
    int next; /* the next cell to draw */
    uint32_t layer_seed;
} SwcAtlasJob;

/* Creates the canvas and lays the cells out. False when out of memory (nothing is left to free). */
bool swc_atlas_job_start(SwcAtlasJob *j, uint32_t layer_seed);
/* Draws one cell. True while cells remain: the canvas equals swc_atlas_draw_all's once it returns false. */
bool swc_atlas_job_next(SwcAtlasJob *j);
/* Hands the canvas over (the caller destroys it) and clears the job. */
SwcCanvas *swc_atlas_job_take(SwcAtlasJob *j);
void swc_atlas_job_free(SwcAtlasJob *j);

/* IEEE binary16 bits of a float (round to nearest; overflow gives infinity, tiny values denormals). */
uint16_t swc_half_bits(float f);
/* sRGB byte -> linear light as a half float, for the 256 possible values. */
void swc_atlas_srgb_lut(uint16_t lut[256]);
/* Rows [first, first + count) of the texture the holograms shader reads, as RGBA half floats in linear light
 * (alpha 1): texture row y is canvas row h - 1 - y, as a canvas uploaded with flipY. `texels` is the whole
 * texture, width * height * 4 values; only those rows are written. */
void swc_atlas_half_rows(const SwcCanvas *cv, const uint16_t lut[256], int first, int count,
                         uint16_t *texels);

#endif
