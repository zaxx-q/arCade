#include "post_it_logic.h"

#include "arcade/playfield.h"

#define PI_ACC_PX 240.0f   /* pointer offset (px) that equals a tilt of 1.0 */
#define PI_WRAP 16.0f      /* items wrap this far outside the playfield... */
#define PI_WRAP_SPAN 31.0f /* ...by this distance (so they re-enter on the opposite side) */
#define PI_MIN_VIRUS_DIST 144.0f

/* ---- pure helpers --------------------------------------------------------------------------- */

void post_it_tilt_from_input(const ArcadeInput *in, int w, int h, float *ax, float *ay) {
    if (in->move_x != 0.0f || in->move_y != 0.0f) { /* held keys win; they are saturated half tilts */
        *ax = 0.5f * in->move_x;
        *ay = -0.5f * in->move_y; /* arCade's move_y is +1 for Down */
        return;
    }
    Vec2 p = arc_pf_from_screen(in->aim_x, in->aim_y);
    *ax = arc_clampf((p.x - (float)w / 2.0f) / PI_ACC_PX, -1.0f, 1.0f);
    *ay = arc_clampf(((float)h / 2.0f - p.y) / PI_ACC_PX, -1.0f, 1.0f);
}

void post_it_drift_from_tilt(float ax, float ay, float *spx, float *spy) {
    *spx = arc_clampf(ax, -0.5f, 0.5f) * 8.0f;
    *spy = arc_clampf(ay, -0.5f, 0.5f) * -8.0f;
}

int post_it_scale_index(int blockn) {
    int m = blockn % 16;
    int idx = m < 9 ? m : 17 - m;
    return idx == 0 ? 1 : idx;
}

/* ---- spawning ------------------------------------------------------------------------------- */

static void new_green(post_it_State *s, int i) { /* anywhere, it may even land on the cluster */
    s->gx[i] = (float)rng_range(&s->rng, 0, s->w);
    s->gy[i] = (float)rng_range(&s->rng, 0, s->h);
    s->grot[i] = rng_float(&s->rng) * 360.0f;
    s->gcol[i] = rng_range(&s->rng, 0, PI_COLORS - 1);
}

static void new_red(post_it_State *s, int i) { /* never closer than 144 px to the head */
    float x, y;
    int guard = 0;
    do {
        x = (float)rng_range(&s->rng, 0, s->w);
        y = (float)rng_range(&s->rng, 0, s->h);
    } while (vec2_dist(vec2(x, y), s->head) < PI_MIN_VIRUS_DIST && ++guard < 10000);
    s->rx[i] = x;
    s->ry[i] = y;
    s->rrot[i] = rng_float(&s->rng) * 360.0f;
    s->rnear[i] = false;
}

void post_it_init_sized(post_it_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->head = vec2((float)w * 0.5f, (float)h * 0.5f);
    s->spx = s->spy = 0.0f;
    s->tilt_x = s->tilt_y = 0.0f;
    s->blockn = 1;
    s->bx[0] = s->head.x;
    s->by[0] = s->head.y;
    s->brot[0] = 0.0f;
    s->bcol[0] = 0;
    for (int i = 0; i < PI_GREENS; i++)
        new_green(s, i);
    for (int i = 0; i < PI_REDS; i++)
        new_red(s, i);
    s->near_count = 0;
    s->paused = false;
    s->over = s->capped = false;
    s->killer = -1;
    s->over_t = 0.0f;
    s->grabs = 0;
    s->last_note = 0;
}

void post_it_init(post_it_State *s, unsigned seed) {
    post_it_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

/* ---- one item moves ------------------------------------------------------------------------- */

/* The whole world drifts with the tilt; the item also walks along its own heading at half that speed. */
static void move_item(post_it_State *s, float *x, float *y, float *rot, float k) {
    float v = sqrtf(s->spx * s->spx + s->spy * s->spy);
    float a = *rot * (ARC_PI / 180.0f);
    *x += s->spx * k + v * k * cosf(a) * 0.5f;
    *y += s->spy * k + v * k * sinf(a) * 0.5f;
    float w = (float)s->w, h = (float)s->h;
    if (*x > w + PI_WRAP) {
        *x -= w + PI_WRAP_SPAN;
        *rot = rng_float(&s->rng) * 360.0f; /* every wrap picks a new heading */
    }
    if (*x < -PI_WRAP) {
        *x += w + PI_WRAP_SPAN;
        *rot = rng_float(&s->rng) * 360.0f;
    }
    if (*y > h + PI_WRAP) {
        *y -= h + PI_WRAP_SPAN;
        *rot = rng_float(&s->rng) * 360.0f;
    }
    if (*y < -PI_WRAP) {
        *y += h + PI_WRAP_SPAN;
        *rot = rng_float(&s->rng) * 360.0f;
    }
}

/* Glue green[i] to the cluster where it touches it, then respawn it. */
static void grab_green(post_it_State *s, int i) {
    int b = s->blockn;
    s->bx[b] = s->gx[i];
    s->by[b] = s->gy[i];
    s->brot[b] = s->grot[i];
    s->bcol[b] = s->gcol[i];
    s->blockn++;
    s->grabs++;
    s->last_note = post_it_scale_index(s->blockn) - 1;
    new_green(s, i);
}

static void test_green(post_it_State *s, int i) {
    /* blockn is re-read every pass: it can grow inside the loop, and the respawned item keeps being
     * tested against the remaining blocks (spec section 5) */
    for (int j = 0; j < s->blockn; j++) {
        if (s->blockn < PI_MAX_BLOCKS &&
            vec2_dist(vec2(s->gx[i], s->gy[i]), vec2(s->bx[j], s->by[j])) < PI_HIT_R)
            grab_green(s, i);
    }
}

static void test_red(post_it_State *s, int i) {
    bool near_i = false;
    for (int j = 0; j < s->blockn; j++) {
        float d = vec2_dist(vec2(s->rx[i], s->ry[i]), vec2(s->bx[j], s->by[j]));
        if (d < PI_HIT_R && !s->over) {
            s->over = true;
            s->killer = i;
        }
        if (d < PI_NEAR_R)
            near_i = true;
    }
    s->rnear[i] = near_i;
}

/* ---- the step -------------------------------------------------------------------------------- */

static bool toggle_pause_requested(const ArcadeInput *in) {
    return in->fire_pressed || in->typed == 'p' || in->typed == 'P';
}

void post_it_step(post_it_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (toggle_pause_requested(in))
        s->paused = !s->paused;
    if (s->paused)
        return;

    post_it_tilt_from_input(in, s->w, s->h, &s->tilt_x, &s->tilt_y);
    post_it_drift_from_tilt(s->tilt_x, s->tilt_y, &s->spx, &s->spy);

    float k = dt * 50.0f;                 /* k = 1 is one tick of the original 50 Hz game */
    for (int i = 0; i < PI_GREENS; i++) { /* greens first (their grab test is inside this loop) ... */
        move_item(s, &s->gx[i], &s->gy[i], &s->grot[i], k);
        test_green(s, i);
    }
    for (int i = 0; i < PI_REDS; i++) { /* ... then the viruses */
        move_item(s, &s->rx[i], &s->ry[i], &s->rrot[i], k);
        test_red(s, i);
    }
    s->near_count = 0;
    for (int i = 0; i < PI_REDS; i++)
        if (s->rnear[i])
            s->near_count++;
    if (s->blockn >= PI_MAX_BLOCKS && !s->over) { /* the cluster is full: the run ends here */
        s->over = true;
        s->capped = true;
    }
    if (s->over)
        s->over_t = 0.0f;
}

bool post_it_is_over(const post_it_State *s) {
    return s->over && s->over_t >= PI_OVER_DELAY;
}

int post_it_score(const post_it_State *s) {
    return s->blockn;
}
