/* Synthwave City - the two unit shapes every building is an instance of. PURE C99, no raylib.
 * Ported from the web version (src/layers/buildings/geometry.ts).
 *
 * Both shapes sit on y = 0, are one unit tall and one unit across, and have no bottom: the bottom of a box
 * is always on the street or on the roof of the box below. Every vertex carries `face`:
 *   x  face id: 0 +x, 1 -x, 2 +z, 3 -z for the side faces (the cylinder wall is 0), 4 for the top
 *   y  u across the face, 0..1, running left to right as seen from outside
 *   z  v up the face, 0..1 (on the top: u is x + 0.5 and v is z + 0.5)
 * Triangles wind counter-clockwise seen from outside. */
#ifndef SWC_SHAPES_H
#define SWC_SHAPES_H

#include <stdint.h>

#define SWC_CYLINDER_SEGMENTS 28 /* sides of the cylinder used for round towers */
#define SWC_SHAPE_MAX_VERTICES 100
#define SWC_SHAPE_MAX_INDICES 320
#define SWC_FACE_TOP 4

typedef struct SwcShape {
    int vertex_count, index_count;
    float position[SWC_SHAPE_MAX_VERTICES * 3];
    float normal[SWC_SHAPE_MAX_VERTICES * 3];
    float face[SWC_SHAPE_MAX_VERTICES * 3];
    uint16_t index[SWC_SHAPE_MAX_INDICES];
} SwcShape;

void swc_shape_unit_box(SwcShape *s);
/* segments: 3 to SWC_CYLINDER_SEGMENTS */
void swc_shape_unit_cylinder(SwcShape *s, int segments);

#endif
