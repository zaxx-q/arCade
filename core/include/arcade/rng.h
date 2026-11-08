/* Deterministic PCG32 random numbers. Same seed => same sequence on every OS.
 * Game logic must use this (never rand()) so tests and replays are reproducible. */
#ifndef ARCADE_RNG_H
#define ARCADE_RNG_H

#include "arcade/common.h"

typedef struct Rng {
    uint64_t state;
    uint64_t inc;
} Rng;

void rng_seed(Rng *r, uint64_t seed);
uint32_t rng_u32(Rng *r);
int rng_range(Rng *r, int lo, int hi); /* inclusive on both ends */
float rng_float(Rng *r);               /* [0, 1) */
float rng_rangef(Rng *r, float lo, float hi);
bool rng_chance(Rng *r, float p); /* true with probability p (0..1) */

#endif
