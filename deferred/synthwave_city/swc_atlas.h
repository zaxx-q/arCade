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

#endif
