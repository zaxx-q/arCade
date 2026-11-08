#include "arcade/rng.h"

void rng_seed(Rng *r, uint64_t seed) {
    r->state = 0u;
    r->inc = (seed << 1u) | 1u;
    (void)rng_u32(r);
    r->state += seed;
    (void)rng_u32(r);
}

uint32_t rng_u32(Rng *r) {
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + (r->inc | 1u);
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

int rng_range(Rng *r, int lo, int hi) {
    if (hi <= lo)
        return lo;
    return lo + (int)(rng_u32(r) % (uint32_t)(hi - lo + 1));
}

float rng_float(Rng *r) {
    return (float)(rng_u32(r) >> 8) * (1.0f / 16777216.0f);
}

float rng_rangef(Rng *r, float lo, float hi) {
    return lo + (hi - lo) * rng_float(r);
}

bool rng_chance(Rng *r, float p) {
    return rng_float(r) < p;
}
