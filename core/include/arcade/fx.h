/* Particles and screen shake: cheap "juice" (pure C; drawing lives in gfx.h). */
#ifndef ARCADE_FX_H
#define ARCADE_FX_H

#include "arcade/common.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define FX_MAX_PARTICLES 512

typedef struct Particle {
    float x, y, vx, vy;
    float life, max_life; /* alive while life > 0 */
    float size;
    uint8_t r, g, b;
} Particle;

typedef struct ParticleSystem {
    Particle p[FX_MAX_PARTICLES];
    int next; /* ring-buffer write position */
    Rng rng;
} ParticleSystem;

void fx_init(ParticleSystem *ps, unsigned seed);
void fx_burst(ParticleSystem *ps, float x, float y, int count, float speed, float life, float size, uint8_t r,
              uint8_t g, uint8_t b);
void fx_update(ParticleSystem *ps, float dt);
int fx_alive(const ParticleSystem *ps);

typedef struct Shake {
    float time, duration, magnitude;
} Shake;

void shake_start(Shake *s, float magnitude, float duration);
void shake_update(Shake *s, float dt);
Vec2 shake_offset(const Shake *s, Rng *rng); /* random offset, fades out over the duration */

#endif
