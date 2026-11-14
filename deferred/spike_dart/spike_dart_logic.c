#include "spike_dart_logic.h"

#include "arcade/playfield.h"

#define SD_ORBIT_SPEED (ARC_PI / 2.0f) /* rad per second: one turn every 4 s */
#define SD_LANE_HALF 8.0f              /* |x - lane| < 8 counts as "in the lane" */
#define SD_EPS 0.01f                   /* float slack so a spike that lands exactly on the edge sticks */

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

bool spike_dart_lane_blocked(const spike_dart_State *s) {
    for (int i = 0; i < s->stuck; i++)
        if (s->py[i] > s->target_dn && fabsf(s->px[i] - s->spike_x) < SD_LANE_HALF)
            return true;
    return false;
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

void spike_dart_step(spike_dart_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (!s->flying && throw_requested(in)) {
        s->flying = true;
        s->throws++;
    }
    if (s->flying) {
        float k = dt * 50.0f; /* k = 1 is one tick of the original 50 Hz game */
        s->cur_y -= s->spike_spd * k;
        float tip = s->cur_y - SD_SPIKE_LEN / 2.0f + SD_SPIKE_W;
        if (tip < s->target_dn + 68.0f &&
            spike_dart_lane_blocked(s)) { /* only the x distance is tested, as in the original */
            s->over = true;
            s->over_t = 0.0f;
            return;
        }
        if (tip <= s->target_dn + SD_EPS) { /* crossing the threshold, so a big step cannot tunnel through */
            stick_current(s);
            return; /* no orbit update on the frame a spike sticks */
        }
    }
    orbit_all(s, dt);
}

bool spike_dart_is_over(const spike_dart_State *s) {
    return s->over && s->over_t >= SD_OVER_DELAY;
}

int spike_dart_score(const spike_dart_State *s) {
    return s->stuck;
}
