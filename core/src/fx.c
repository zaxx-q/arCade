#include "arcade/fx.h"

#include <string.h>

void fx_init(ParticleSystem *ps, unsigned seed) {
    memset(ps, 0, sizeof(*ps));
    rng_seed(&ps->rng, seed);
}

void fx_burst(ParticleSystem *ps, float x, float y, int count, float speed, float life, float size, uint8_t r,
              uint8_t g, uint8_t b) {
    for (int i = 0; i < count; i++) {
        Particle *p = &ps->p[ps->next];
        ps->next = (ps->next + 1) % FX_MAX_PARTICLES;
        float ang = rng_rangef(&ps->rng, 0.0f, ARC_TAU);
        float spd = rng_rangef(&ps->rng, 0.2f * speed, speed);
        p->x = x;
        p->y = y;
        p->vx = cosf(ang) * spd;
        p->vy = sinf(ang) * spd;
        p->max_life = life * rng_rangef(&ps->rng, 0.6f, 1.0f);
        p->life = p->max_life;
        p->size = size;
        p->r = r;
        p->g = g;
        p->b = b;
    }
}

void fx_update(ParticleSystem *ps, float dt) {
    for (int i = 0; i < FX_MAX_PARTICLES; i++) {
        Particle *p = &ps->p[i];
        if (p->life <= 0.0f)
            continue;
        p->life -= dt;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vx *= 0.985f;
        p->vy *= 0.985f;
    }
}

int fx_alive(const ParticleSystem *ps) {
    int n = 0;
    for (int i = 0; i < FX_MAX_PARTICLES; i++)
        if (ps->p[i].life > 0.0f)
            n++;
    return n;
}

void shake_start(Shake *s, float magnitude, float duration) {
    s->magnitude = magnitude;
    s->duration = duration;
    s->time = duration;
}

void shake_update(Shake *s, float dt) {
    if (s->time > 0.0f)
        s->time = arc_maxf(0.0f, s->time - dt);
}

Vec2 shake_offset(const Shake *s, Rng *rng) {
    if (s->time <= 0.0f || s->duration <= 0.0f)
        return vec2(0.0f, 0.0f);
    float k = s->magnitude * (s->time / s->duration);
    return vec2(rng_rangef(rng, -k, k), rng_rangef(rng, -k, k));
}
