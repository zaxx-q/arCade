#include "master_juggler_logic.h"

#include "arcade/playfield.h"

#define MJ_BAR_HIT_HALF_W 60.0f /* |ball.x - bar.x| < 60 bounces                          */
#define MJ_KEY_SPEED 420.0f     /* keyboard bar speed, px per second (hover wins when the pointer moves) */
#define MJ_WALL 15.0f

/* ---- pure helpers --------------------------------------------------------------------------- */

int master_juggler_stage(int bounces, int level) {
    int mj = level <= 2 ? level : 3;
    if (bounces >= 3 * mj)
        return MJ_STAGE_GREY;
    if (bounces >= 2 * mj)
        return MJ_STAGE_GREEN;
    return bounces >= mj ? MJ_STAGE_YELLOW : MJ_STAGE_RED;
}

int master_juggler_launch_tick(int level, int i) {
    return (int)((100.0 / (double)level) * (double)i + 0.5);
}

/* ---- setup ---------------------------------------------------------------------------------- */

void master_juggler_init_sized(master_juggler_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->base_h = (float)h / 6.0f;
    s->bar_y = (float)h - (float)h / 6.0f - (float)h / 25.0f;
    s->gh = (((float)h * 9.0f / 10.0f - (float)h / 25.0f) - (float)h / 6.0f) / 350.0f;
    s->bar_x = (float)w * 0.5f;
    s->last_aim_x = 0.0f;
    s->have_aim = false;
    s->level = 1;
    s->best = 0;
    s->lives = MJ_LIVES;
    s->waiting = true;
    s->nb = s->bl = 0;
    s->mj = 1;
    s->frmball = -10.0f;
    s->first_lost = false;
    for (int i = 0; i < MJ_MAX_BALLS; i++) {
        s->x[i] = s->y[i] = s->spx[i] = s->spy[i] = 0.0f;
        s->alpha[i] = 1.0f;
        s->bounces[i] = 0;
        s->visible[i] = false;
    }
    s->since_bounce = 10.0f;
    s->over = s->capped = false;
    s->over_t = 0.0f;
    s->throws = s->hits = s->losses = s->levels_done = s->tries_lost = 0;
}

void master_juggler_init(master_juggler_State *s, unsigned seed) {
    master_juggler_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

static void start_try(master_juggler_State *s) {
    s->nb = s->bl = 0;
    s->mj = s->level <= 2 ? s->level : 3;
    s->frmball = -10.0f;
    s->first_lost = false;
    s->waiting = false;
    for (int i = 0; i < MJ_MAX_BALLS; i++)
        s->visible[i] = false;
}

static void new_ball(master_juggler_State *s) {
    int i = s->nb;
    s->x[i] = s->bar_x;
    s->y[i] = s->bar_y + 45.0f; /* behind the bar: the ball seems to shoot out of it */
    s->spx[i] = 0.0f;
    s->spy[i] = s->gh * -12.0f;
    s->alpha[i] = 1.0f;
    s->bounces[i] = 0;
    s->visible[i] = true;
    s->nb++;
    s->throws++;
}

/* ---- bar -------------------------------------------------------------------------------------- */

/* The bar follows the pointer (no click needed); the keyboard moves it only when the pointer stood still. */
static void move_bar(master_juggler_State *s, const ArcadeInput *in, float dt) {
    Vec2 p = arc_pf_from_screen(in->aim_x, in->aim_y);
    bool pointer_moved = !s->have_aim || in->aim_x != s->last_aim_x; /* the first frame adopts the pointer */
    if (pointer_moved)
        s->bar_x = p.x;
    else if (in->move_x != 0.0f)
        s->bar_x += in->move_x * MJ_KEY_SPEED * dt;
    s->have_aim = true;
    s->last_aim_x = in->aim_x;
    /* the whole bar stays inside the court (a ball at the wall x = 15 is still within the 60 px reach) */
    s->bar_x = arc_clampf(s->bar_x, MJ_BAR_W / 2.0f, (float)s->w - MJ_BAR_W / 2.0f);
}

/* ---- the step --------------------------------------------------------------------------------- */

static void level_complete(master_juggler_State *s) {
    s->best = s->level;
    s->levels_done++;
    if (s->level >= MJ_MAX_LEVEL) { /* the last level: nothing above it, the run ends here */
        s->over = true;
        s->capped = true;
        s->over_t = 0.0f;
    } else {
        s->level++;
    }
    s->waiting = true;
}

static void try_failed(master_juggler_State *s) {
    s->tries_lost++;
    s->lives--;
    s->waiting = true;
    if (s->lives <= 0) {
        s->over = true;
        s->over_t = 0.0f;
    }
}

/* Moves one ball and handles falling, bouncing and finishing. Returns true when the try just ended. */
static bool step_ball(master_juggler_State *s, int i, float k) {
    float gh = s->gh, w = (float)s->w, h = (float)s->h;
    s->spx[i] *= powf(0.98f, k); /* the original's per-frame (1 - 1/fps) at 50 Hz */
    s->spy[i] += k * gh * 0.2f;
    s->x[i] += k * s->spx[i];
    s->y[i] += k * s->spy[i];
    if (s->x[i] < MJ_WALL || s->x[i] > w - MJ_WALL) { /* the -2.5 factor is real, not a typo */
        s->spx[i] *= -2.5f;
        s->x[i] = arc_clampf(s->x[i], MJ_WALL, w - MJ_WALL);
    }
    if (s->bl == 0 && s->bounces[i] >= 3 * s->mj) { /* a finished ball fades out and cannot be lost */
        s->alpha[i] -= 0.01f * k;
        if (i == s->nb - 1 && s->alpha[i] <= 0.0f) {
            level_complete(s);
            return true;
        }
        return false;
    }
    if (s->spy[i] < 0.0f)
        return false;  /* still rising */
    if (s->y[i] > h) { /* lost off the bottom */
        if (!s->first_lost) {
            s->first_lost = true;
            s->losses++;
            for (int j = 0; j < s->nb; j++)
                s->alpha[j] = 1.0f; /* the front-end paints every ball grey */
        }
        s->visible[i] = false;
        s->bl++;
        if (s->bl >= s->level) { /* every ball is gone: this try is lost */
            try_failed(s);
            return true;
        }
        return false;
    }
    if (s->first_lost)
        return false; /* no bouncing after the first loss */
    if (fabsf(s->y[i] - s->bar_y) < gh * 20.0f && fabsf(s->x[i] - s->bar_x) < MJ_BAR_HIT_HALF_W) {
        s->spy[i] = gh * -0.2f - s->spy[i];
        s->spx[i] = ((s->x[i] - s->bar_x) + (float)rng_range(&s->rng, -1, 1)) / 10.0f;
        s->bounces[i]++;
        s->hits++;
        s->since_bounce = 0.0f;
    }
    return false;
}

void master_juggler_step(master_juggler_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    s->since_bounce += dt;
    move_bar(s, in, dt);
    if (s->waiting) {
        if (in->fire_pressed || in->action_pressed)
            start_try(s);
        return;
    }
    float k = dt * 50.0f; /* k = 1 is one tick of the original 50 Hz game */
    s->frmball += k;
    if (s->nb < s->level && s->nb < MJ_MAX_BALLS &&
        s->frmball >= (float)master_juggler_launch_tick(s->level, s->nb))
        new_ball(s);
    for (int i = 0; i < s->nb; i++) {
        if (!s->visible[i])
            continue;
        if (step_ball(s, i, k))
            return;
    }
}

bool master_juggler_is_over(const master_juggler_State *s) {
    return s->over && s->over_t >= MJ_OVER_DELAY;
}

int master_juggler_score(const master_juggler_State *s) {
    return s->best;
}
