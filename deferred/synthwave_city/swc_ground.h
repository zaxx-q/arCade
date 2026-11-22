/* Synthwave City - the ground layer's data: where the street lamps stand, the disc the ground shader is
 * drawn on, and the one lamp post that is instanced. PURE C99, no raylib. Ported from the web version
 * (src/layers/ground/layout.ts); the shaders carry the other half of the ground (streets, markings, plazas,
 * wet reflections), derived from the same constants. */
#ifndef SWC_GROUND_H
#define SWC_GROUND_H

#include <stdint.h>

#include "swc_core.h"

/* Numbers the shaders and the layout share (the web version's GROUND object; metres unless noted). */
#define SWC_LAMP_SETBACK 0.9 /* pole centre behind the curb */
#define SWC_LAMP_ARM 2.6     /* how far the head reaches back over the road */
#define SWC_LAMP_HEIGHT_STREET 7.0
#define SWC_LAMP_HEIGHT_AVENUE 9.0
#define SWC_LAMP_MODEL_HEIGHT 7.0 /* height the post mesh is modelled at */
#define SWC_LAMP_SPACING 40.0     /* along a road at full density; divides SWC_CELL */
#define SWC_LAMP_SPACING_SPARSE 80.0
#define SWC_LAMP_COLOR_EVEN 3 /* palette index: orange */
#define SWC_LAMP_COLOR_ODD 1  /* palette index: cyan */
#define SWC_POST_FADE_START                                                                                  \
    520.0 /* posts are drawn this close to the camera; beyond it a lamp is its glow                          \
           */
#define SWC_POST_FADE_END 760.0

/* The lamp layout repeats with this period in x and z: the avenue period, which the colour period divides. */
#define SWC_LAMP_PATTERN (SWC_CELL * SWC_AVENUE_EVERY)
/* How many copies of the pattern the post lattice spans on each side of the camera. */
#define SWC_POST_LATTICE 2

/* Floats per lamp: pole base x, z; unit direction towards the road centre x, z; head height; palette index.
 */
#define SWC_LAMP_STRIDE 6
enum { SWC_LAMP_X, SWC_LAMP_Z, SWC_LAMP_DIR_X, SWC_LAMP_DIR_Z, SWC_LAMP_HEIGHT, SWC_LAMP_COLOR };

/* Lamp spacing for the density of a quality tier. */
double swc_lamp_spacing(double density);
/* The palette index of the lamps along grid line `line` (any integer). */
int swc_lamp_color(int line);
/* Lamps in a square of the given side whose corner is the tile origin; extent is a multiple of the pattern.
 */
int swc_lamp_count(double spacing, double extent);
/* Every lamp whose road line and place along it fall in [0, extent): pairs facing each other across each
 * road, `spacing` apart, starting half a spacing from the line crossings so none stands in a junction. `out`
 * holds swc_lamp_count() * SWC_LAMP_STRIDE floats; returns the number of lamps. */
int swc_build_lamps(double spacing, double extent, float *out);
/* Where the head of lamp i (and its glow) sits: tile x, height, tile z. */
void swc_lamp_head(const float *lamps, int i, float out[3]);

/* The lamp posts as instances: the pattern repeated on a (2 * lattice)^2 grid of copies round the origin.
 * aLamp (4 floats each) is the pole base in the pattern's frame plus the direction to the road; aInfo (2
 * floats each) the head height and palette index. Returns the instance count, perPattern * (2 * lattice)^2.
 */
int swc_build_post_instances(const float *pattern, int per_pattern, int lattice, float *a_lamp,
                             float *a_info);

/* ---- meshes ---- */

#define SWC_DISC_SEGMENTS 48
#define SWC_DISC_FIRST_RING 2.0
#define SWC_DISC_MAX_RINGS 24
#define SWC_DISC_MAX_VERTS (1 + SWC_DISC_MAX_RINGS * SWC_DISC_SEGMENTS)
#define SWC_DISC_MAX_INDICES (SWC_DISC_SEGMENTS * (3 + 6 * (SWC_DISC_MAX_RINGS - 1)))

/* The ground: a flat disc of rings whose radius doubles, centred under the camera, so triangles stay small
 * where a pixel covers millimetres. Wound to face up (+y). */
typedef struct SwcDisc {
    int vertex_count, index_count;
    float position[SWC_DISC_MAX_VERTS * 3];
    uint16_t index[SWC_DISC_MAX_INDICES];
} SwcDisc;

void swc_build_disc(double outer_radius, SwcDisc *out);

#define SWC_POST_VERTS 24
#define SWC_POST_INDICES 108

/* One lamp post in its own frame: +x points at the road, +y is up. A tapered pole that bends into an arm
 * (part 0) and a flat head at the end of it (part 1). axis is, for each vertex, the point on the pole's
 * centre line that it surrounds (the vertex itself on the head). */
typedef struct SwcPostMesh {
    int vertex_count, index_count;
    float position[SWC_POST_VERTS * 3];
    float part[SWC_POST_VERTS];
    float axis[SWC_POST_VERTS * 3];
    uint16_t index[SWC_POST_INDICES];
} SwcPostMesh;

void swc_build_post(SwcPostMesh *out);

/* True when every marking and lamp period divides the cell and the tile, so the ground wraps without a seam.
 */
bool swc_ground_periods_divide_tile(void);

#endif
