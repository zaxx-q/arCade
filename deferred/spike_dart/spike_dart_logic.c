#include "spike_dart_logic.h"

#include "arcade/playfield.h"

#define SD_ORBIT_SPEED (ARC_PI / 2.0f) /* rad per second: one turn every 4 s */
#define SD_EPS 0.01f                   /* float slack so a spike that lands exactly on the edge sticks */
#define SD_SUBSTEP                                                                                           \
    4.0f /* the flight is hit-tested every 4 px (the spike is 80 px long and a                               \
            fast frame moves it ~14 px, so nothing can be jumped over) */

static bool throw_requested(const ArcadeInput *in) {
    return in->fire_pressed || in->action_pressed || in->left_pressed || in->right_pressed ||
           in->up_pressed || in->down_pressed || in->typed != 0;
}

/* Centre of a stuck spike at orbit angle t (spec: (TX - cos t * 152, TY + sin t * 152)). */
static void place_on_orbit(spike_dart_State *s, int i) {
    s->px[i] = s->target_x - cosf(s->rot[i]) * SD_ORBIT_R;
    s->py[i] = s->target_y + sinf(s->rot[i]) * SD_ORBIT_R;
}

void spike_dart_init_sized(spike_dart_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->target_x = (float)w * 0.5f;
    s->target_y = (float)h * 0.5f - 60.0f;
    s->target_dn = s->target_y + SD_TARGET_R;
    s->spike_x = (float)w * 0.5f;
    s->spike_y = (float)h - 80.0f;
    /* the spike's tip (8 px of overlap allowed) must travel from spike_y to the target's bottom in 10 ticks
     */
    s->spike_spd = (((s->spike_y - SD_SPIKE_LEN / 2.0f) + SD_SPIKE_W) - s->target_dn) / 10.0f;
    s->stuck = 0;
    s->cur_y = s->spike_y;
    s->flying = false;
    s->over = false;
    s->capped = false;
    s->over_t = 0.0f;
    s->throws = 0;
    s->sticks = 0;
    for (int i = 0; i < SD_MAX_SPIKES; i++) {
        s->px[i] = s->py[i] = s->rot[i] = 0.0f;
    }
}

void spike_dart_init(spike_dart_State *s, unsigned seed) {
    spike_dart_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

/* ---- the hit box = the drawn triangle -------------------------------------------------------- */

void spike_dart_shape(float cx, float cy, float angle, Vec2 tri[3]) {
    float dx = cosf(angle), dy = -sinf(angle); /* unit vector from the spike's centre toward the target */
    float hl = SD_SPIKE_LEN / 2.0f, hw = SD_SPIKE_W / 2.0f;
    tri[0] = vec2(cx + dx * hl, cy + dy * hl);                     /* tip                  */
    tri[1] = vec2(cx - dx * hl - dy * hw, cy - dy * hl + dx * hw); /* flat end, one corner */
    tri[2] = vec2(cx - dx * hl + dy * hw, cy - dy * hl - dx * hw); /* flat end, other      */
}

/* The shadow (min and max) of a triangle on an axis. */
static void project(const Vec2 t[3], Vec2 axis, float *lo, float *hi) {
    *lo = *hi = vec2_dot(t[0], axis);
    for (int i = 1; i < 3; i++) {
        float p = vec2_dot(t[i], axis);
        *lo = arc_minf(*lo, p);
        *hi = arc_maxf(*hi, p);
    }
}

/* Separating axis test: two convex shapes are apart if their shadows do not overlap on the normal of
 * some edge. Here they only count as touching when EVERY shadow overlaps by more than SD_HIT_SLACK. */
static bool triangles_touch(const Vec2 a[3], const Vec2 b[3]) {
    for (int side = 0; side < 2; side++) {
        const Vec2 *t = side == 0 ? a : b;
        for (int i = 0; i < 3; i++) {
            Vec2 edge = vec2_sub(t[(i + 1) % 3], t[i]);
            Vec2 axis = vec2_norm(vec2(-edge.y, edge.x));
            float a_lo, a_hi, b_lo, b_hi;
            project(a, axis, &a_lo, &a_hi);
            project(b, axis, &b_lo, &b_hi);
            float overlap = arc_minf(a_hi, b_hi) - arc_maxf(a_lo, b_lo);
            if (overlap <= SD_HIT_SLACK)
                return false; /* found a gap (or only a graze): they do not touch */
        }
    }
    return true;
}

bool spike_dart_would_touch(const spike_dart_State *s, float y) {
    Vec2 mine[3], other[3];
    spike_dart_shape(s->spike_x, y, ARC_PI / 2.0f, mine);
    for (int i = 0; i < s->stuck; i++) {
        spike_dart_shape(s->px[i], s->py[i], s->rot[i], other);
        if (triangles_touch(mine, other))
            return true;
    }
    return false;
}

/* Where a spike's centre is when it has just stuck: on the orbit, straight below the target. */
static float landing_y(const spike_dart_State *s) {
    return s->target_y + SD_ORBIT_R;
}

bool spike_dart_lane_blocked(const spike_dart_State *s) {
    return spike_dart_would_touch(s, landing_y(s));
}

static void stick_current(spike_dart_State *s) {
    int i = s->stuck;
    s->rot[i] = ARC_PI / 2.0f; /* thrown from the bottom = orbit angle pi/2 */
    place_on_orbit(s, i);
    s->stuck++;
    s->sticks++;
    s->flying = false;
    s->cur_y = s->spike_y;
    if (s->stuck >= SD_MAX_SPIKES) { /* the original would index past its 64-spike pool here */
        s->over = true;
        s->capped = true;
        s->over_t = 0.0f;
    }
}

static void orbit_all(spike_dart_State *s, float dt) {
    for (int i = 0; i < s->stuck; i++) {
        s->rot[i] += SD_ORBIT_SPEED * dt; /* real seconds, so the spin is the same at any frame rate */
        place_on_orbit(s, i);
    }
}

static void lose(spike_dart_State *s) {
    s->over = true;
    s->over_t = 0.0f;
}

/* One frame of a flying spike, cut into sub-steps of at most SD_SUBSTEP px. Each sub-step moves the
 * stuck spikes a little (they keep orbiting), moves the flying spike, and then asks: did it arrive at the
 * target? did it touch a stuck spike? */
static void fly(spike_dart_State *s, float dt) {
    float dist = s->spike_spd * dt * 50.0f; /* px this frame: spike_spd is per tick of the 50 Hz original */
    int n = (int)ceilf(dist / SD_SUBSTEP);
    if (n < 1)
        n = 1;
    for (int i = 0; i < n; i++) {
        orbit_all(s, dt / (float)n);
        s->cur_y -= dist / (float)n;
        if (s->cur_y <= landing_y(s) + SD_EPS) { /* arrived (crossing, so a big step cannot tunnel) */
            s->cur_y = landing_y(s);             /* it sticks exactly on the orbit: test that spot */
            if (spike_dart_would_touch(s, s->cur_y))
                lose(s);
            else
                stick_current(s);
            return;
        }
        if (spike_dart_would_touch(s, s->cur_y)) {
            lose(s);
            return;
        }
    }
}

void spike_dart_step(spike_dart_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (!s->flying && throw_requested(in)) {
        s->flying = true;
        s->throws++;
    }
    if (s->flying)
        fly(s, dt);
    else
        orbit_all(s, dt);
}

bool spike_dart_is_over(const spike_dart_State *s) {
    return s->over && s->over_t >= SD_OVER_DELAY;
}

int spike_dart_score(const spike_dart_State *s) {
    return s->stuck;
}
