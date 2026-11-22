/* Synthwave City - the buildings layer's instance data, packed once from the city plan. PURE C99, no raylib.
 * Ported from the web version (src/layers/buildings/pack.ts): every box of the plan becomes one instance of a
 * unit box or a unit cylinder, and every roof light one sprite. The GPU renderer uploads these arrays as
 * they are, so the layout (floats per instance, order of the fields) is the shader's attribute layout. */
#ifndef SWC_PACK_H
#define SWC_PACK_H

#include "swc_city.h"

/* Floats per instance in each attribute. */
#define SWC_POS_SIZE 3   /* centre x, base y, centre z */
#define SWC_SIZE_SIZE 3  /* full size along x, y, z */
#define SWC_STYLE_SIZE 4 /* style, palette index A, palette index B, flags */
#define SWC_SEED_SIZE 3  /* box seed 0..1, building seed 0..1, roof height of the whole building (m) */

#define SWC_SHAPE_SQUARE 0 /* drawn with the unit box */
#define SWC_SHAPE_ROUND 1  /* flagged SWC_FLAG_ROUND: drawn with the unit cylinder */

/* All boxes of a plan. The arrays hold the square instances first, then the round ones; instance k of shape
 * s is at index first[s] + k. Each shape keeps the plan's order. About 1.2 MB: keep it in static storage. */
typedef struct SwcPackedBoxes {
    int count[2];
    int first[2];
    float pos[SWC_MAX_BOXES * SWC_POS_SIZE];
    float size[SWC_MAX_BOXES * SWC_SIZE_SIZE];
    float style[SWC_MAX_BOXES * SWC_STYLE_SIZE];
    float seed[SWC_MAX_BOXES * SWC_SEED_SIZE];
    float roof[SWC_MAX_BUILDINGS]; /* per building: roof height, spires excluded (what seed[2] holds) */
} SwcPackedBoxes;

void swc_pack_boxes(const SwcCity *city, SwcPackedBoxes *out);

#define SWC_LIGHT_SITE_SIZE 4 /* x, y, z, size */
#define SWC_LIGHT_KIND_SIZE 2 /* palette index, seed 0..1 */

/* The aviation lights: one sprite per roof-light site, in plan order. */
typedef struct SwcPackedLights {
    int count;
    float site[SWC_MAX_SITES * SWC_LIGHT_SITE_SIZE];
    float kind[SWC_MAX_SITES * SWC_LIGHT_KIND_SIZE];
} SwcPackedLights;

void swc_pack_roof_lights(const SwcCity *city, SwcPackedLights *out);

#endif
