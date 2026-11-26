/* Synthwave City - the buildings layer's instance data. See swc_pack.h. */
#include "swc_pack.h"

#include <string.h>

void swc_pack_boxes(const SwcCity *city, SwcPackedBoxes *out) {
    int n = city->box_count;
    int round_count = 0;
    for (int i = 0; i < n; i++)
        if (city->boxes[i].flags & SWC_FLAG_ROUND)
            round_count++;
    out->count[SWC_SHAPE_SQUARE] = n - round_count;
    out->count[SWC_SHAPE_ROUND] = round_count;
    out->first[SWC_SHAPE_SQUARE] = 0;
    out->first[SWC_SHAPE_ROUND] = n - round_count;

    /* Roof height per building, spires excluded: facade gradients run over the whole stack, not one box.
     * The web version keeps it in a Float32Array and adds in double, so the sum is rounded only on the store.
     */
    float *top = out->roof;
    int buildings = city->building_count < SWC_MAX_BUILDINGS ? city->building_count : SWC_MAX_BUILDINGS;
    memset(top, 0, sizeof(float) * (size_t)buildings);
    for (int i = 0; i < n; i++) {
        const SwcBox *b = &city->boxes[i];
        if (b->style == SWC_STYLE_SPIRE || b->building < 0 || b->building >= buildings)
            continue;
        double y1 = (double)b->y0 + (double)b->sy;
        if (y1 > (double)top[b->building])
            top[b->building] = (float)y1;
    }

    int next[2] = {out->first[SWC_SHAPE_SQUARE], out->first[SWC_SHAPE_ROUND]};
    for (int i = 0; i < n; i++) {
        const SwcBox *b = &city->boxes[i];
        int shape = (b->flags & SWC_FLAG_ROUND) ? SWC_SHAPE_ROUND : SWC_SHAPE_SQUARE;
        int k = next[shape]++;
        float *pos = &out->pos[k * SWC_POS_SIZE];
        float *size = &out->size[k * SWC_SIZE_SIZE];
        float *style = &out->style[k * SWC_STYLE_SIZE];
        float *seed = &out->seed[k * SWC_SEED_SIZE];
        pos[0] = b->cx;
        pos[1] = b->y0;
        pos[2] = b->cz;
        size[0] = b->sx;
        size[1] = b->sy;
        size[2] = b->sz;
        style[0] = (float)b->style;
        style[1] = (float)b->color_a;
        style[2] = (float)b->color_b;
        style[3] = (float)b->flags;
        float roof = (b->building >= 0 && b->building < buildings) ? top[b->building] : 0.0f;
        seed[0] = b->seed;
        seed[1] = (float)swc_hash_unit3(city->seed, (uint32_t)b->building, 0x5bu);
        seed[2] = roof > 1.0f ? roof : 1.0f;
    }
}

void swc_pack_roof_lights(const SwcCity *city, SwcPackedLights *out) {
    int k = 0;
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *s = &city->sites[i];
        if (s->kind != SWC_SITE_ROOF_LIGHT)
            continue;
        out->site[k * SWC_LIGHT_SITE_SIZE] = (float)s->x;
        out->site[k * SWC_LIGHT_SITE_SIZE + 1] = (float)s->y;
        out->site[k * SWC_LIGHT_SITE_SIZE + 2] = (float)s->z;
        out->site[k * SWC_LIGHT_SITE_SIZE + 3] = (float)s->w;
        out->kind[k * SWC_LIGHT_KIND_SIZE] = (float)s->color;
        out->kind[k * SWC_LIGHT_KIND_SIZE + 1] = (float)s->seed;
        k++;
    }
    out->count = k;
}
