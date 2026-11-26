/* Synthwave City - the unit box and the unit cylinder. See swc_shapes.h. */
#include "swc_shapes.h"

#include <math.h>
#include <string.h>

#include "swc_core.h"

static void add_vertex(SwcShape *s, double px, double py, double pz, double nx, double ny, double nz,
                       double f, double u, double v) {
    int i = s->vertex_count++;
    s->position[i * 3] = (float)px;
    s->position[i * 3 + 1] = (float)py;
    s->position[i * 3 + 2] = (float)pz;
    s->normal[i * 3] = (float)nx;
    s->normal[i * 3 + 1] = (float)ny;
    s->normal[i * 3 + 2] = (float)nz;
    s->face[i * 3] = (float)f;
    s->face[i * 3 + 1] = (float)u;
    s->face[i * 3 + 2] = (float)v;
}

static void add_triangle(SwcShape *s, int a, int b, int c) {
    s->index[s->index_count++] = (uint16_t)a;
    s->index[s->index_count++] = (uint16_t)b;
    s->index[s->index_count++] = (uint16_t)c;
}

void swc_shape_unit_box(SwcShape *s) {
    static const int side_normals[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    static const int corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    memset(s, 0, sizeof(*s));
    for (int f = 0; f < 4; f++) {
        int nx = side_normals[f][0], nz = side_normals[f][1];
        /* direction of increasing u: to the right for a viewer looking at the face from outside */
        int tx = nz, tz = -nx;
        int base = s->vertex_count;
        for (int c = 0; c < 4; c++) {
            double u = corners[c][0], v = corners[c][1];
            add_vertex(s, 0.5 * nx + (u - 0.5) * tx, v, 0.5 * nz + (u - 0.5) * tz, nx, 0, nz, f, u, v);
        }
        add_triangle(s, base, base + 1, base + 2);
        add_triangle(s, base, base + 2, base + 3);
    }
    int base = s->vertex_count;
    for (int c = 0; c < 4; c++) {
        double u = corners[c][0], v = corners[c][1];
        add_vertex(s, u - 0.5, 1, v - 0.5, 0, 1, 0, SWC_FACE_TOP, u, v);
    }
    add_triangle(s, base, base + 3, base + 2);
    add_triangle(s, base, base + 2, base + 1);
}

void swc_shape_unit_cylinder(SwcShape *s, int segments) {
    memset(s, 0, sizeof(*s));
    if (segments < 3)
        segments = 3;
    if (segments > SWC_CYLINDER_SEGMENTS)
        segments = SWC_CYLINDER_SEGMENTS;
    /* The wall. The seam column is duplicated so u runs from 1 down to 0 without wrapping inside a triangle.
     */
    for (int i = 0; i <= segments; i++) {
        double a = ((double)i / (double)segments) * SWC_TAU;
        double c = cos(a), sn = sin(a);
        for (int v = 0; v <= 1; v++)
            add_vertex(s, 0.5 * c, v, 0.5 * sn, c, 0, sn, 0, 1.0 - (double)i / (double)segments, v);
    }
    for (int i = 0; i < segments; i++) {
        int b0 = i * 2, t0 = b0 + 1, b1 = b0 + 2, t1 = b0 + 3;
        add_triangle(s, b0, t1, b1);
        add_triangle(s, b0, t0, t1);
    }
    /* The cap: a fan round a centre vertex. */
    int centre = s->vertex_count;
    add_vertex(s, 0, 1, 0, 0, 1, 0, SWC_FACE_TOP, 0.5, 0.5);
    for (int i = 0; i <= segments; i++) {
        double a = ((double)i / (double)segments) * SWC_TAU;
        double c = cos(a), sn = sin(a);
        add_vertex(s, 0.5 * c, 1, 0.5 * sn, 0, 1, 0, SWC_FACE_TOP, 0.5 + 0.5 * c, 0.5 + 0.5 * sn);
    }
    for (int i = 0; i < segments; i++)
        add_triangle(s, centre, centre + 2 + i, centre + 1 + i);
}
