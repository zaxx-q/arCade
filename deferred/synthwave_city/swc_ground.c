/* Synthwave City - the ground layer's data. See swc_ground.h. */
#include "swc_ground.h"

#include <math.h>

double swc_lamp_spacing(double density) {
    return density >= 0.8 ? SWC_LAMP_SPACING : SWC_LAMP_SPACING_SPARSE;
}

int swc_lamp_color(int line) {
    return (((line % 2) + 2) % 2 == 0) ? SWC_LAMP_COLOR_EVEN : SWC_LAMP_COLOR_ODD;
}

int swc_lamp_count(double spacing, double extent) {
    return (int)(2.0 * 2.0 * (extent / SWC_CELL) * (extent / spacing));
}

int swc_build_lamps(double spacing, double extent, float *out) {
    int o = 0;
    int lines = (int)(extent / SWC_CELL);
    int along_count = (int)(extent / spacing);
    for (int axis = 0; axis < 2; axis++) {
        for (int line = 0; line < lines; line++) {
            const SwcRoad *spec = swc_road_spec(line);
            double height = spec->median > 0.0 ? SWC_LAMP_HEIGHT_AVENUE : SWC_LAMP_HEIGHT_STREET;
            int color = swc_lamp_color(line);
            for (int s = 0; s < 2; s++) {
                double side = s == 0 ? -1.0 : 1.0;
                double fixed = (double)line * SWC_CELL + side * (spec->road_half + SWC_LAMP_SETBACK);
                for (int k = 0; k < along_count; k++) {
                    double along = ((double)k + 0.5) * spacing;
                    /* axis 0: the road runs along x, so the lamp row has a fixed z. axis 1: the reverse. */
                    out[o + SWC_LAMP_X] = (float)(axis == 0 ? along : fixed);
                    out[o + SWC_LAMP_Z] = (float)(axis == 0 ? fixed : along);
                    out[o + SWC_LAMP_DIR_X] = (float)(axis == 0 ? 0.0 : -side);
                    out[o + SWC_LAMP_DIR_Z] = (float)(axis == 0 ? -side : 0.0);
                    out[o + SWC_LAMP_HEIGHT] = (float)height;
                    out[o + SWC_LAMP_COLOR] = (float)color;
                    o += SWC_LAMP_STRIDE;
                }
            }
        }
    }
    return o / SWC_LAMP_STRIDE;
}

void swc_lamp_head(const float *lamps, int i, float out[3]) {
    const float *l = &lamps[i * SWC_LAMP_STRIDE];
    out[0] = (float)((double)l[SWC_LAMP_X] + (double)l[SWC_LAMP_DIR_X] * SWC_LAMP_ARM);
    out[1] = (float)((double)l[SWC_LAMP_HEIGHT] - 0.05);
    out[2] = (float)((double)l[SWC_LAMP_Z] + (double)l[SWC_LAMP_DIR_Z] * SWC_LAMP_ARM);
}

int swc_build_post_instances(const float *pattern, int per_pattern, int lattice, float *a_lamp,
                             float *a_info) {
    int n = 0;
    for (int cz = -lattice; cz < lattice; cz++) {
        for (int cx = -lattice; cx < lattice; cx++) {
            for (int i = 0; i < per_pattern; i++) {
                const float *l = &pattern[i * SWC_LAMP_STRIDE];
                a_lamp[n * 4] = (float)((double)l[SWC_LAMP_X] + (double)cx * SWC_LAMP_PATTERN);
                a_lamp[n * 4 + 1] = (float)((double)l[SWC_LAMP_Z] + (double)cz * SWC_LAMP_PATTERN);
                a_lamp[n * 4 + 2] = l[SWC_LAMP_DIR_X];
                a_lamp[n * 4 + 3] = l[SWC_LAMP_DIR_Z];
                a_info[n * 2] = l[SWC_LAMP_HEIGHT];
                a_info[n * 2 + 1] = l[SWC_LAMP_COLOR];
                n++;
            }
        }
    }
    return n;
}

void swc_build_disc(double outer_radius, SwcDisc *out) {
    double radii[SWC_DISC_MAX_RINGS];
    int rings = 0;
    for (double r = SWC_DISC_FIRST_RING; r < outer_radius * 0.75 && rings < SWC_DISC_MAX_RINGS - 1; r *= 2.0)
        radii[rings++] = r;
    radii[rings++] = outer_radius;
    const int segments = SWC_DISC_SEGMENTS;
    out->vertex_count = 1 + rings * segments;
    out->position[0] = out->position[1] = out->position[2] = 0.0f;
    for (int j = 0; j < rings; j++) {
        for (int i = 0; i < segments; i++) {
            double a = ((double)i / (double)segments) * SWC_TAU;
            int o = (1 + j * segments + i) * 3;
            out->position[o] = (float)(cos(a) * radii[j]);
            out->position[o + 1] = 0.0f;
            out->position[o + 2] = (float)(sin(a) * radii[j]);
        }
    }
    int k = 0;
    for (int i = 0; i < segments; i++) {
        int n = (i + 1) % segments;
        /* Wound so the faces look up (+y). */
        out->index[k++] = 0;
        out->index[k++] = (uint16_t)(1 + n);
        out->index[k++] = (uint16_t)(1 + i);
        for (int j = 0; j + 1 < rings; j++) {
            int a = 1 + j * segments + i, b = 1 + j * segments + n;
            int c = a + segments, d = b + segments;
            out->index[k++] = (uint16_t)a;
            out->index[k++] = (uint16_t)b;
            out->index[k++] = (uint16_t)d;
            out->index[k++] = (uint16_t)a;
            out->index[k++] = (uint16_t)d;
            out->index[k++] = (uint16_t)c;
        }
    }
    out->index_count = k;
}

void swc_build_post(SwcPostMesh *out) {
    const double h = SWC_LAMP_MODEL_HEIGHT, arm = SWC_LAMP_ARM;
    const double s = 0.70710678118654752440; /* Math.SQRT1_2 */
    /* Each ring: centre, radius, and the two axes that span it. */
    const double rings[4][10] = {
        {0, 0, 0, 0.17, 1, 0, 0, 0, 0, 1},
        {0, h - 0.9, 0, 0.1, 1, 0, 0, 0, 0, 1},
        {0.5, h - 0.12, 0, 0.08, -s, s, 0, 0, 0, 1},
        {arm - 0.5, h, 0, 0.07, 0, 1, 0, 0, 0, 1},
    };
    static const int KA[4] = {1, 0, -1, 0}, KB[4] = {0, 1, 0, -1};
    int v = 0;
    for (int j = 0; j < 4; j++) {
        const double *r = rings[j];
        for (int q = 0; q < 4; q++) {
            out->position[v * 3] = (float)(r[0] + (r[4] * KA[q] + r[7] * KB[q]) * r[3]);
            out->position[v * 3 + 1] = (float)(r[1] + (r[5] * KA[q] + r[8] * KB[q]) * r[3]);
            out->position[v * 3 + 2] = (float)(r[2] + (r[6] * KA[q] + r[9] * KB[q]) * r[3]);
            out->part[v] = 0.0f;
            out->axis[v * 3] = (float)r[0];
            out->axis[v * 3 + 1] = (float)r[1];
            out->axis[v * 3 + 2] = (float)r[2];
            v++;
        }
    }
    int k = 0;
    for (int j = 0; j + 1 < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int a = j * 4 + i, b = j * 4 + ((i + 1) % 4);
            out->index[k++] = (uint16_t)a;
            out->index[k++] = (uint16_t)b;
            out->index[k++] = (uint16_t)(b + 4);
            out->index[k++] = (uint16_t)a;
            out->index[k++] = (uint16_t)(b + 4);
            out->index[k++] = (uint16_t)(a + 4);
        }
    }
    /* The head: a flat box at the end of the arm. */
    int base = v;
    const double hx = 0.62, hy = 0.09, hz = 0.2, y = h - 0.02;
    for (int i = 0; i < 8; i++) {
        float px = (float)(arm + ((i & 1) ? hx : -hx));
        float py = (float)(y + ((i & 2) ? hy : -hy));
        float pz = (float)((i & 4) ? hz : -hz);
        out->position[v * 3] = px;
        out->position[v * 3 + 1] = py;
        out->position[v * 3 + 2] = pz;
        out->part[v] = 1.0f;
        out->axis[v * 3] = px;
        out->axis[v * 3 + 1] = py;
        out->axis[v * 3 + 2] = pz;
        v++;
    }
    static const int QUADS[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                                    {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
    for (int q = 0; q < 6; q++) {
        int a = QUADS[q][0], b = QUADS[q][1], c = QUADS[q][2], d = QUADS[q][3];
        out->index[k++] = (uint16_t)(base + a);
        out->index[k++] = (uint16_t)(base + b);
        out->index[k++] = (uint16_t)(base + c);
        out->index[k++] = (uint16_t)(base + a);
        out->index[k++] = (uint16_t)(base + c);
        out->index[k++] = (uint16_t)(base + d);
    }
    out->vertex_count = v;
    out->index_count = k;
}

bool swc_ground_periods_divide_tile(void) {
    static const double IN_CELL[4] = {10.0, 2.0, SWC_LAMP_SPACING,
                                      SWC_LAMP_SPACING_SPARSE}; /* dash, slab, lamps */
    for (int i = 0; i < 4; i++)
        if (floor(SWC_CELL / IN_CELL[i]) != SWC_CELL / IN_CELL[i])
            return false;
    return floor(SWC_TILE / SWC_LAMP_PATTERN) == SWC_TILE / SWC_LAMP_PATTERN;
}
