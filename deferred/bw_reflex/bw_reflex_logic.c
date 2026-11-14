#include "bw_reflex_logic.h"

#include "arcade/playfield.h"

#define BW_SPEED0 4.0f
#define BW_MULT0 0.07f
#define BW_MULT_DECAY 0.9625f

/* Any click or key flips the base (the original has no other control). */
static bool flip_requested(const ArcadeInput *in) {
    return in->fire_pressed || in->action_pressed || in->left_pressed || in->right_pressed ||
           in->up_pressed || in->down_pressed || in->typed != 0;
}

static void new_block(bw_reflex_State *s) {
    s->block_y = -BW_BLOCK / 2.0f;
    s->block_white = rng_range(&s->rng, 0, 1); /* inclusive: 0 or 1 */
}

void bw_reflex_init_sized(bw_reflex_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->base_h = (int)lroundf((float)h / 6.0f);
    s->base_white = 0;
    s->speed = BW_SPEED0;
    s->mult = BW_MULT0;
    s->score = 0;
    s->over = false;
    s->over_t = 0.0f;
    s->flips = 0;
    s->matches = 0;
    s->last_note = 0;
    new_block(s);
}

void bw_reflex_init(bw_reflex_State *s, unsigned seed) {
    bw_reflex_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

float bw_reflex_contact_y(const bw_reflex_State *s) {
    return (float)(s->h - s->base_h) + 1.5f;
}

void bw_reflex_step(bw_reflex_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (flip_requested(in)) {
        s->base_white ^= 1;
        s->flips++;
    }
    /* k = 1 is one tick of the original 50 Hz game */
    float k = dt * 50.0f;
    s->block_y += (float)(s->h - s->base_h) * s->speed * k / 400.0f;
    if (s->block_y + BW_BLOCK / 2.0f <= bw_reflex_contact_y(s))
        return;
    if (s->block_white == s->base_white) {
        s->last_note = s->score % 8; /* uses the score BEFORE the increment */
        s->score++;
        s->matches++;
        s->speed *= 1.0f + s->mult;
        s->mult *= BW_MULT_DECAY;
        new_block(s);
    } else {
        s->over = true;
        s->over_t = 0.0f;
    }
}

bool bw_reflex_is_over(const bw_reflex_State *s) {
    return s->over && s->over_t >= BW_OVER_DELAY;
}

int bw_reflex_score(const bw_reflex_State *s) {
    return s->score;
}
