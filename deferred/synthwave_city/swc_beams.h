/* Synthwave City - the beams layer's instance data: searchlights sweeping from roofs and plazas, beacon
 * pillars over the megatowers and plazas, and laser fans above downtown. PURE C99, no raylib.
 * Ported from the web version (src/layers/beams/pack.ts). Every beam is one instance with the same seven
 * attributes whatever its kind; its direction is a closed-form function of the loop phase (the vertex
 * shader's bm_dir(), mirrored by swc_beam_direction below), so nothing is animated on the CPU. */
#ifndef SWC_BEAMS_H
#define SWC_BEAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"

#define SWC_BEAM_SEARCHLIGHT 0
#define SWC_BEAM_PILLAR 1
#define SWC_BEAM_LASER 2

#define SWC_BEAM_MAX                                                                                         \
    1024 /* per set: a plan lights about 100 to 160 emitters, 200 beams at most (checked over many seeds) */

/* One set of beams drawn together. Attribute k of beam i is at [i * stride]. */
typedef struct SwcBeamSet {
    int count;
    float origin[SWC_BEAM_MAX * 3]; /* emitter x, y, z (tile coordinates) */
    float shape[SWC_BEAM_MAX * 4];  /* length, radius at the emitter, radius at the far end, kind */
    float look[SWC_BEAM_MAX * 4];   /* palette index, seed 0..1, intensity, position in its fan (-0.5..0.5) */
    float tilt[SWC_BEAM_MAX * 4];   /* angle off vertical: mid, amplitude, whole cycles per loop, phase */
    float azim[SWC_BEAM_MAX *
               4]; /* azimuth: base, whole turns per loop (signed), sway amplitude, sway cycles */
    float fan[SWC_BEAM_MAX * 4]; /* fan opening (full angle): mid, amplitude, whole cycles per loop, phase */
    float roll[SWC_BEAM_MAX *
               4]; /* roll about the centre line: base, turns per loop; sway phase, flare share */
} SwcBeamSet;

typedef struct SwcPackedBeams {
    SwcBeamSet sky;     /* searchlights and lasers: not mirrored in the wet streets */
    SwcBeamSet pillars; /* pillars: mirrored */
    int sites_used[3];  /* emitters lit, per SWC_BEAM_ kind */
} SwcPackedBeams;

/* The seed the web version gives the beams layer: hashInts(seed, hashText("beams")). */
uint32_t swc_beam_layer_seed(uint32_t city_seed);

/* Which sites are lit: lit[i] for every site of the plan (false for every site that is no emitter). Each kind
 * is thinned on its own so that no two lit emitters of a kind stand closer than a spacing that grows as the
 * density falls (1 is full density). `lit` needs city->site_count entries. */
void swc_choose_emitters(const SwcCity *city, double density, uint32_t layer_seed, bool *lit);

/* About 220 KB: keep it in static storage. */
void swc_pack_beams(const SwcCity *city, double density, uint32_t layer_seed, SwcPackedBeams *out);

/* Unit direction of beam i of a set at loop phase `loop` (0..1). The CPU mirror of bm_dir() in the shaders.
 */
void swc_beam_direction(const SwcBeamSet *set, int i, double loop, double out[3]);

#endif
