#include "arcade/input.h"

#include <string.h>

void arcade_input_clear(ArcadeInput *in) {
    memset(in, 0, sizeof(*in));
}

void arcade_monkey_init(ArcadeMonkey *m, unsigned seed) {
    memset(m, 0, sizeof(*m));
    rng_seed(&m->rng, seed);
}

void arcade_monkey_next(ArcadeMonkey *m, ArcadeInput *in) {
    arcade_input_clear(in);
    if (m->hold <= 0) {
        m->mx = (float)rng_range(&m->rng, -1, 1);
        m->my = (float)rng_range(&m->rng, -1, 1);
        m->firing = rng_chance(&m->rng, 0.5f);
        m->hold = rng_range(&m->rng, 10, 40);
    }
    m->hold--;
    in->move_x = m->mx;
    in->move_y = m->my;
    in->aim_x = rng_rangef(&m->rng, 0.0f, (float)ARC_SCREEN_W);
    in->aim_y = rng_rangef(&m->rng, 0.0f, (float)ARC_SCREEN_H);
    in->fire = m->firing;
    in->fire_pressed = rng_chance(&m->rng, 0.10f);
    in->alt_pressed = rng_chance(&m->rng, 0.02f);
    in->alt = in->alt_pressed;
    in->action_pressed = rng_chance(&m->rng, 0.04f);
    in->action = in->action_pressed;
    in->left_pressed = rng_chance(&m->rng, 0.05f);
    in->right_pressed = rng_chance(&m->rng, 0.05f);
    in->up_pressed = rng_chance(&m->rng, 0.05f);
    in->down_pressed = rng_chance(&m->rng, 0.05f);
    in->wheel = rng_chance(&m->rng, 0.02f) ? rng_range(&m->rng, -1, 1) : 0;
    if (rng_chance(&m->rng, 0.02f)) {
        static const char keys[] = "1234cr+-";
        in->typed = keys[rng_range(&m->rng, 0, (int)sizeof(keys) - 2)];
    }
}
