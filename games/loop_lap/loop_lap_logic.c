#include "loop_lap_logic.h"

#include "arcade/playfield.h"

#define LL_SPEED_BRAKE (ARC_TAU / 625.0)  /* rad per original tick: a lap takes 12.5 s */
#define LL_SPEED_CRUISE (ARC_TAU / 250.0) /* 5 s   */
#define LL_SPEED_ACCEL (ARC_TAU / 100.0)  /* 2 s   */
#define LL_PATH_INSET 3.0f                /* balls run on a path 3 px inside the circle */
#define LL_NEAR_EXTRA 16.0f               /* a near miss is 16 px beyond touching       */
#define LL_WHOOSH_COOLDOWN 1.0f

static const int k_join_lap[LL_MAX_ENEMIES] = {1, 3, 8, 17, 31, 51, 78, 113};

/* ---- pure helpers --------------------------------------------------------------------------- */

int loop_lap_mode_from_input(const ArcadeInput *in, int w) {
    Vec2 p = arc_pf_from_screen(in->aim_x, in->aim_y);
    bool right_half = p.x >= (float)w / 2.0f; /* even when the pointer is outside the column */
    bool accel = (in->fire && right_half) || in->action || in->move_x > 0.0f || in->move_y < 0.0f;
    bool brake = (in->fire && !right_half) || in->move_x < 0.0f || in->move_y > 0.0f;
    if (accel)
        return LL_ACCEL;
    return brake ? LL_BRAKE : LL_CRUISE;
}

float loop_lap_hit_distance(float my_w) {
    return my_w / 2.0f + LL_ENEMY_W / 2.0f - 2.0f;
}

int loop_lap_join_lap(int i) {
    return k_join_lap[arc_clampi(i, 0, LL_MAX_ENEMIES - 1)];
}

/* ---- setup ---------------------------------------------------------------------------------- */

static void update_positions(loop_lap_State *s) {
    float r = s->c1r - LL_PATH_INSET;
    s->my_pos = vec2(s->c1x + r * (float)cos(s->my_a), s->c1y - r * (float)sin(s->my_a));
    for (int i = 0; i < s->enemy_n; i++) {
        float r2 = s->c2r - LL_PATH_INSET;
        s->enemy_pos[i] = vec2(s->c2x + r2 * (float)cos(s->ea[i]), s->c2y - r2 * (float)sin(s->ea[i]));
    }
}

void loop_lap_init_sized(loop_lap_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->c1r = (float)w * 3.0f * 0.125f;
    s->c1x = (float)w / 2.0f;
    s->c1y = (float)h / 2.0f - s->c1r * 5.0f * 0.125f;
    s->c2r = s->c1r;
    s->c2x = (float)w / 2.0f;
    s->c2y = (float)h / 2.0f + s->c1r * 5.0f * 0.125f;
    s->my_a = (double)ARC_PI / 2.0; /* 12 o'clock */
    s->mode = LL_CRUISE;
    s->my_w = 48.0f;
    s->score = 0;
    s->enemy_n = 0;
    for (int i = 0; i < LL_MAX_ENEMIES; i++) {
        s->ea[i] = s->ew[i] = 0.0;
        s->enemy_pos[i] = vec2(0.0f, 0.0f);
    }
    s->flash_t = 10.0f; /* no flash at the start */
    s->near_cd = 0.0f;
    s->paused = s->over = false;
    s->over_t = 0.0f;
    s->laps = s->joins = s->whooshes = 0;
    update_positions(s);
}

void loop_lap_init(loop_lap_State *s, unsigned seed) {
    loop_lap_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

static void new_enemy(loop_lap_State *s) {
    int i = s->enemy_n;
    s->ea[i] = (double)rng_float(&s->rng) * (double)ARC_TAU;
    s->ew[i] = (double)rng_float(&s->rng) * 0.03518584 + 0.00670206; /* always counter-clockwise */
    s->enemy_n++;
    s->joins++;
}

/* ---- the step ------------------------------------------------------------------------------- */

static void set_mode(loop_lap_State *s, int mode) {
    s->mode = mode;
    s->my_w = mode == LL_BRAKE ? 60.0f : (mode == LL_ACCEL ? 36.0f : 48.0f);
}

static double speed_of(int mode) {
    return mode == LL_BRAKE ? LL_SPEED_BRAKE : (mode == LL_ACCEL ? LL_SPEED_ACCEL : LL_SPEED_CRUISE);
}

static void finish_lap(loop_lap_State *s) {
    s->my_a -= (double)ARC_TAU;
    s->score++;
    s->laps++;
    s->flash_t = 0.0f;
    if (s->enemy_n < LL_MAX_ENEMIES && s->score == k_join_lap[s->enemy_n])
        new_enemy(s);
}

void loop_lap_step(loop_lap_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (in->typed == 'p' || in->typed == 'P')
        s->paused = !s->paused;
    if (s->paused)
        return;

    set_mode(s, loop_lap_mode_from_input(in, s->w));
    float k = dt * 50.0f; /* k = 1 is one tick of the original 50 Hz game */
    s->flash_t += dt;
    s->near_cd -= dt;

    s->my_a += speed_of(s->mode) * (double)k;
    if (s->my_a > (double)ARC_TAU)
        finish_lap(s);
    for (int i = 0; i < s->enemy_n; i++)
        s->ea[i] += s->ew[i] * (double)k;
    update_positions(s);

    for (int i = 0; i < s->enemy_n; i++) {
        float d = vec2_dist(s->my_pos, s->enemy_pos[i]);
        if (d <= loop_lap_hit_distance(s->my_w)) {
            s->over = true;
            s->over_t = 0.0f;
            return;
        }
        if (s->near_cd <= 0.0f && d <= s->my_w / 2.0f + LL_ENEMY_W / 2.0f + LL_NEAR_EXTRA) {
            s->near_cd = LL_WHOOSH_COOLDOWN; /* at most one whoosh per second */
            s->whooshes++;
        }
    }
}

bool loop_lap_is_over(const loop_lap_State *s) {
    return s->over && s->over_t >= LL_OVER_DELAY;
}

int loop_lap_score(const loop_lap_State *s) {
    return s->score;
}
