#include "absorb_grow_logic.h"

#define TOP_MARGIN 44.0f

/* New blobs are sized relative to the player so the world always offers both food and danger. */
static void spawn_blob(absorb_grow_State *s, ag_Blob *b) {
    b->r = arc_clampf(s->r * rng_rangef(&s->rng, 0.35f, 1.7f), 5.0f, 200.0f);
    for (int attempt = 0; attempt < 8; attempt++) { /* keep the spawn away from the player */
        b->pos = vec2(rng_rangef(&s->rng, b->r, ARC_SCREEN_W - b->r),
                      rng_rangef(&s->rng, TOP_MARGIN + b->r, ARC_SCREEN_H - b->r));
        if (vec2_dist(b->pos, s->pos) > s->r + b->r + 80.0f)
            break;
    }
    b->vel = vec2(rng_rangef(&s->rng, -30.0f, 30.0f), rng_rangef(&s->rng, -30.0f, 30.0f));
}

void absorb_grow_init(absorb_grow_State *s, unsigned seed) {
    rng_seed(&s->rng, seed);
    s->pos = vec2(ARC_SCREEN_W / 2.0f, ARC_SCREEN_H / 2.0f);
    s->r = AG_START_R;
    s->absorbed = 0;
    s->time_left = AG_TIME;
    s->over = s->won = false;
    s->last_absorb = s->pos;
    s->absorb_events = 0;
    for (int i = 0; i < AG_MAX_BLOBS; i++) {
        spawn_blob(s, &s->blobs[i]);
        if (i % 7 == 0)
            s->blobs[i].r = rng_rangef(&s->rng, 30.0f, 60.0f); /* a few big, scary ones */
    }
}

/* TODO(Member 6): blob kinds (spiky, poison), a scrolling world and growth stages (GAME.md steps 1-3). */
void absorb_grow_step(absorb_grow_State *s, const ArcadeInput *in, float dt) {
    if (s->over)
        return;
    s->time_left -= dt;

    float speed = arc_maxf(90.0f, 230.0f - s->r); /* bigger = slower */
    Vec2 dir = vec2_norm(vec2(in->move_x, in->move_y));
    s->pos = vec2_add(s->pos, vec2_scale(dir, speed * dt));
    s->pos.x = arc_clampf(s->pos.x, s->r, ARC_SCREEN_W - s->r);
    s->pos.y = arc_clampf(s->pos.y, TOP_MARGIN + s->r, ARC_SCREEN_H - s->r);

    for (int i = 0; i < AG_MAX_BLOBS; i++) {
        ag_Blob *b = &s->blobs[i];
        b->pos = vec2_add(b->pos, vec2_scale(b->vel, dt));
        if (b->pos.x < b->r || b->pos.x > ARC_SCREEN_W - b->r)
            b->vel.x = -b->vel.x;
        if (b->pos.y < TOP_MARGIN + b->r || b->pos.y > ARC_SCREEN_H - b->r)
            b->vel.y = -b->vel.y;
        b->pos.x = arc_clampf(b->pos.x, b->r, ARC_SCREEN_W - b->r);
        b->pos.y = arc_clampf(b->pos.y, TOP_MARGIN + b->r, ARC_SCREEN_H - b->r);

        float d = vec2_dist(b->pos, s->pos);
        if (s->r > b->r * 1.1f && d < s->r - b->r * 0.3f) { /* swallow: areas add up (a bit is lost) */
            s->r = sqrtf(s->r * s->r + 0.7f * b->r * b->r);
            s->absorbed++;
            s->absorb_events++;
            s->last_absorb = b->pos;
            spawn_blob(s, b);
        } else if (b->r > s->r * 1.1f && d < b->r - s->r * 0.3f) { /* swallowed by something bigger */
            s->over = true;
            return;
        }
    }
    if (s->r >= AG_WIN_R)
        s->won = s->over = true;
    if (s->time_left <= 0.0f) {
        s->time_left = 0.0f;
        s->over = true;
    }
}

bool absorb_grow_is_over(const absorb_grow_State *s) {
    return s->over;
}

int absorb_grow_score(const absorb_grow_State *s) {
    return (int)(s->r * 10.0f) + s->absorbed * 5 + (s->won ? 500 + (int)(s->time_left * 10.0f) : 0);
}
