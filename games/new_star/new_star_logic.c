#include "new_star_logic.h"

#include "arcade/playfield.h"

#define NS_X_MARGIN 38
#define NS_Y_TOP 78 /* x margin + 40: room for the score text */
#define NS_MAX_ATTEMPTS 2000
#define NS_ZONE_HALF_W 100.0f
#define NS_ZONE_HALF_H 60.0f
#define NS_CREATE_T 1.05f /* a new star appears this long after a correct tap (overlay opaque) */
#define NS_LOCK_T 1.4f    /* taps are ignored this long after a correct tap                    */

static const float k_size_factor[NS_TYPES] = {1.0f, 0.8f, 1.4f, 1.2f, 1.2f, 1.4f};

/* ---- pure helpers --------------------------------------------------------------------------- */

float new_star_type_size(int type) {
    return k_size_factor[arc_clampi(type, 0, NS_TYPES - 1)] * 40.0f;
}

float new_star_overlay_alpha(float t) {
    if (t <= 0.999f)
        return sinf(t / 1.0f * (ARC_PI / 2.0f));
    if (t <= 1.199f)
        return 1.0f;
    if (t <= 2.199f)
        return sinf((2.2f - t) / 1.0f * (ARC_PI / 2.0f));
    return 0.0f;
}

bool new_star_overlay_visible(float t) {
    return t <= 2.199f;
}

/* ---- creating stars --------------------------------------------------------------------------- */

static bool in_score_zone(const new_star_State *s, float x, float y) {
    return fabsf(x - s->score_pos.x) < NS_ZONE_HALF_W && fabsf(y - s->score_pos.y) < NS_ZONE_HALF_H;
}

static bool overlaps_old_star(const new_star_State *s, float x, float y, float size, int count) {
    for (int j = 0; j < count; j++)
        if (vec2_dist(vec2(x, y), vec2(s->x[j], s->y[j])) <= (size + s->size[j]) / 1.8f)
            return true;
    return false;
}

static void new_star(new_star_State *s) {
    int t = s->n; /* index of the new star (equals the score when this is called) */
    if (t < 0 || t >= NS_MAX_STARS)
        return;
    float x, y;
    int k;
    if (t == 0) { /* the first star is always the centre, type 5 */
        x = (float)(int)((float)s->w * 0.5f);
        y = (float)(int)((float)s->h * 0.5f);
        k = 5;
    } else {
        int x_max = (int)((float)s->w - (float)NS_X_MARGIN);
        int y_max = (int)((float)s->h - (float)NS_X_MARGIN);
        int attempts = 0;
        for (;;) {
            x = (float)rng_range(&s->rng, NS_X_MARGIN, x_max);
            y = (float)rng_range(&s->rng, NS_Y_TOP, y_max);
            k = rng_range(&s->rng, 0, NS_TYPES - 1);
            attempts++;
            if (attempts >= NS_MAX_ATTEMPTS)
                break; /* after 2000 failures the last candidate is accepted anyway */
            if (in_score_zone(s, x, y))
                continue;
            if (overlaps_old_star(s, x, y, new_star_type_size(k), t))
                continue;
            break;
        }
    }
    s->x[t] = x;
    s->y[t] = y;
    s->type[t] = k;
    s->size[t] = new_star_type_size(k);
    s->rot[t] = (float)rng_range(&s->rng, 0, 359);
    s->shake[t] = 0.0f;
    s->n++;
    s->created++;
    s->alarm = false;
    s->new_t = 0.0f;
    for (int i = 0; i < s->n; i++) /* undo any shake */
        s->shake[i] = 0.0f;
}

void new_star_init_sized(new_star_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->score_pos = vec2((float)w * 0.5f, (float)h * 0.125f);
    s->n = 0;
    s->score = 0;
    for (int i = 0; i < NS_MAX_STARS; i++) {
        s->x[i] = s->y[i] = -1000.0f;
        s->size[i] = 40.0f;
        s->rot[i] = s->shake[i] = 0.0f;
        s->type[i] = 0;
    }
    s->overlay = false;
    s->phase_t = 0.0f;
    s->new_t = 0.0f;
    s->alarm = false;
    s->over = s->capped = s->timed_out = false;
    s->over_t = 0.0f;
    s->corrects = s->alarms = s->created = 0;
    new_star(s);
}

void new_star_init(new_star_State *s, unsigned seed) {
    new_star_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

/* ---- tapping ---------------------------------------------------------------------------------- */

/* The tap box is the star's axis-aligned w x h grown by 10 px on every side (rotation is ignored). */
static bool star_hit(const new_star_State *s, int i, float lx, float ly) {
    float half = s->size[i] / 2.0f + NS_TAP_TOLERANCE;
    return fabsf(lx - s->x[i]) <= half && fabsf(ly - s->y[i]) <= half;
}

static void end_game(new_star_State *s) {
    s->over = true;
    s->over_t = 0.0f;
    for (int i = 0; i < s->n; i++)
        s->shake[i] = 0.0f; /* the stars stop shaking */
}

void new_star_tap(new_star_State *s, float lx, float ly) {
    if (s->over)
        return;
    if (!arc_pf_contains(vec2(lx, ly), (float)s->w, (float)s->h))
        return; /* D7: clicks in the gutters do nothing */
    if (s->overlay && s->phase_t < NS_LOCK_T)
        return;
    if (s->score >= s->n)
        return;
    if (star_hit(s, s->score, lx, ly)) { /* the NEW star: correct */
        s->score++;
        s->corrects++;
        if (s->score >= NS_MAX_STARS) { /* no 513th star exists: the run ends here */
            s->capped = true;
            end_game(s);
            return;
        }
        s->overlay = true;
        s->phase_t = 0.0f;
        s->new_t = 0.0f;
        return;
    }
    for (int j = 0; j < s->score; j++) {
        if (star_hit(s, j, lx, ly)) { /* an old star: lose */
            end_game(s);
            return;
        }
    }
    /* anything else: nothing happens */
}

/* ---- the step --------------------------------------------------------------------------------- */

void new_star_step(new_star_State *s, const ArcadeInput *in, float dt) {
    if (s->n > 0)
        s->rot[s->n - 1] += NS_ROT_SPEED * dt; /* the newest star always rotates, even after game over */
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (in->fire_pressed) {
        Vec2 p = arc_pf_from_screen(in->aim_x, in->aim_y);
        new_star_tap(s, p.x, p.y);
        if (s->over)
            return;
    }
    for (int i = 0; i < s->n; i++) {
        if (i < s->n - 1)
            s->rot[i] += NS_ROT_SPEED * dt; /* every star rotates at the same speed */
        if (s->alarm)
            s->shake[i] += rng_float(&s->rng) * 2.0f - 1.0f; /* shake sideways */
    }
    s->new_t += dt;
    if (s->overlay)
        s->phase_t += dt;
    if (s->n >= NS_TIMED_FROM && !s->overlay) {
        if (!s->alarm && s->new_t >= NS_ALARM_TIME) {
            s->alarm = true;
            s->alarms++;
        }
        if (s->new_t >= NS_LIMIT_TIME) {
            s->timed_out = true;
            end_game(s);
            return;
        }
    }
    if (!s->overlay)
        return;
    if (!new_star_overlay_visible(s->phase_t)) {
        s->overlay = false;
    } else if (s->phase_t < NS_CREATE_T) {
        return;
    }
    if (s->n == s->score) /* every star so far confirmed: add the next one */
        new_star(s);
}

bool new_star_is_over(const new_star_State *s) {
    return s->over && s->over_t >= NS_OVER_DELAY;
}

int new_star_score(const new_star_State *s) {
    return s->score;
}
