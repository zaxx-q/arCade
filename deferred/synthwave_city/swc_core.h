/* Synthwave City - shared constants, hashing, the city's seeded RNG and wrap-around maths.
 * Header-only, pure C99 (no raylib). Everything here is ported 1:1 from the web version
 * (src/contracts/constants.ts, src/core/random.ts, src/core/math.ts) so that the same seed builds the
 * same city on every machine, in C and in the original TypeScript.
 *
 * Why not arcade/rng.h (PCG32)? Because the city must match the web version seed for seed, and that
 * version uses a hash + "mulberry32". The generator below is still fully deterministic (no rand(), no
 * clock), so the repo rule "same seed => same result" holds. */
#ifndef SWC_CORE_H
#define SWC_CORE_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "arcade/common.h"

/* ---- the world: one square tile that repeats forever (architecture.md section 4) ---- */
#define SWC_TILE 6400.0 /* side of the tile, metres */
#define SWC_HALF_TILE 3200.0
#define SWC_CELL 160.0 /* side of one grid cell, metres */
#define SWC_CELLS 40   /* cells per tile side (= SWC_TILE / SWC_CELL) */
#define SWC_CELL_COUNT (SWC_CELLS * SWC_CELLS)
#define SWC_AVENUE_EVERY 4 /* every 4th grid line is an avenue, the rest are streets */

/* Sky lanes hang above the avenues. Lanes along x and along z never share an altitude. */
#define SWC_SKY_LEVELS 4
#define SWC_SKY_LANE_OFFSET 9.0

/* double-precision pi: arCade's ARC_PI is a float literal, which would perturb every angle */
#define SWC_PI 3.14159265358979323846
#define SWC_TAU 6.28318530717958647692

#define SWC_LOOP_SECONDS 4096.0 /* all lane motion is periodic over this many seconds */
#define SWC_BPM 105.0

typedef struct SwcRoad {
    double corridor_half; /* centre line to building line */
    double road_half;     /* centre line to curb */
    double median;        /* half-width of the central median (0 = none) */
    double sidewalk;
    int lane_count;        /* lanes per direction */
    double lane_offset[3]; /* lane centre offsets from the centre line */
} SwcRoad;

static const SwcRoad SWC_STREET = {12.0, 8.0, 0.0, 4.0, 2, {2.0, 6.0, 0.0}};
static const SwcRoad SWC_AVENUE = {20.0, 14.0, 2.0, 6.0, 3, {4.0, 8.0, 12.0}};

static const double SWC_SKY_LANE_Y_X[SWC_SKY_LEVELS] = {90.0, 170.0, 260.0, 360.0};
static const double SWC_SKY_LANE_Y_Z[SWC_SKY_LEVELS] = {130.0, 215.0, 310.0, 410.0};

/* ---- 3D vector used by collision and camera (double: positions are tile coordinates up to 6400) ---- */
typedef struct SwcVec3 {
    double x, y, z;
} SwcVec3;

/* ---- small maths helpers (core/math.ts) ---- */
static inline double swc_clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Hermite step. Requires e0 < e1. */
static inline double swc_smoothstep(double e0, double e1, double x) {
    double t = swc_clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

/* v wrapped into [0, period). A tiny negative remainder plus the period can round to the period itself;
 * that is 0 on the circle. */
static inline double swc_wrap(double v, double period) {
    double r = fmod(v, period);
    if (r >= 0.0)
        return r;
    double s = r + period;
    return s >= period ? 0.0 : s;
}

/* Shortest signed difference a - b on a circle of the given period: in [-period/2, period/2). */
static inline double swc_wrap_delta(double a, double b, double period) {
    double d = a - b;
    return d - period * floor(d / period + 0.5);
}

/* Position wrapped into the tile [0, TILE). */
static inline double swc_wrap_tile(double v) {
    double r = swc_wrap(v, SWC_TILE);
    return r >= SWC_TILE ? 0.0 : r;
}

/* Grid line index (any integer) -> avenue or street. */
static inline bool swc_is_avenue(int line) {
    int wrapped = ((line % SWC_CELLS) + SWC_CELLS) % SWC_CELLS;
    return wrapped % SWC_AVENUE_EVERY == 0;
}
static inline const SwcRoad *swc_road_spec(int line) {
    return swc_is_avenue(line) ? &SWC_AVENUE : &SWC_STREET;
}

/* Cell index of cell (cx, cz), wrapping around the tile. */
static inline int swc_cell_index(int cx, int cz) {
    return ((cz % SWC_CELLS + SWC_CELLS) % SWC_CELLS) * SWC_CELLS +
           ((cx % SWC_CELLS + SWC_CELLS) % SWC_CELLS);
}

/* ---- hashing (core/random.ts): a well-mixed 32-bit hash of an integer ---- */
static inline uint32_t swc_hash32(uint32_t x) {
    uint32_t h = x;
    h = (h ^ (h >> 16)) * 0x7feb352du;
    h = (h ^ (h >> 15)) * 0x846ca68bu;
    return h ^ (h >> 16);
}

/* Combine several integers into one hash. */
static inline uint32_t swc_hash_ints(const uint32_t *values, int n) {
    uint32_t h = 0x9e3779b9u;
    for (int i = 0; i < n; i++)
        h = swc_hash32(h ^ values[i]);
    return h;
}

/* Hash to a float in [0, 1). The 2/3/4-argument forms keep call sites short. */
static inline double swc_hash_unit2(uint32_t a, uint32_t b) {
    uint32_t v[2];
    v[0] = a;
    v[1] = b;
    return (double)swc_hash_ints(v, 2) / 4294967296.0;
}
static inline double swc_hash_unit3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t v[3];
    v[0] = a;
    v[1] = b;
    v[2] = c;
    return (double)swc_hash_ints(v, 3) / 4294967296.0;
}
static inline double swc_hash_unit4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t v[4];
    v[0] = a;
    v[1] = b;
    v[2] = c;
    v[3] = d;
    return (double)swc_hash_ints(v, 4) / 4294967296.0;
}

/* ---- seeded random numbers: "mulberry32", small, fast, good enough for layout decisions ---- */
typedef struct SwcRng {
    uint32_t state;
    uint32_t base; /* the seed the generator started from: forks derive from it, not from `state` */
} SwcRng;

static inline void swc_rng_seed(SwcRng *r, uint32_t seed) {
    uint32_t s = swc_hash32(seed);
    if (s == 0u)
        s = 1u;
    r->state = s;
    r->base = s;
}

/* Float in [0, 1). */
static inline double swc_rng_next(SwcRng *r) {
    r->state += 0x6d2b79f5u;
    uint32_t t = (r->state ^ (r->state >> 15)) * (1u | r->state);
    t = (t + (t ^ (t >> 7)) * (61u | t)) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}
/* Float in [lo, hi). */
static inline double swc_rng_range(SwcRng *r, double lo, double hi) {
    return lo + (hi - lo) * swc_rng_next(r);
}
/* Integer in [lo, hi], both inclusive. */
static inline int swc_rng_int(SwcRng *r, int lo, int hi) {
    return lo + (int)floor(swc_rng_next(r) * (double)(hi - lo + 1));
}
/* True with probability p. */
static inline bool swc_rng_chance(SwcRng *r, double p) {
    return swc_rng_next(r) < p;
}
/* Index of one element of a non-empty list of `count` items. */
static inline int swc_rng_pick(SwcRng *r, int count) {
    return (int)floor(swc_rng_next(r) * (double)count);
}
/* A new independent generator derived from this one's seed and a label. */
static inline SwcRng swc_rng_fork(const SwcRng *r, uint32_t label) {
    uint32_t v[2];
    SwcRng f;
    v[0] = r->base;
    v[1] = label;
    swc_rng_seed(&f, swc_hash_ints(v, 2));
    return f;
}

#endif
