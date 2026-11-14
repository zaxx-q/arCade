#include "tower_stacker_logic.h"

#include "arcade/playfield.h"

#define TS_BAR_SPEED_X 5.0f /* px per 50 Hz tick */

/* ---- pure helpers --------------------------------------------------------------------------- */

static bool drop_requested(const ArcadeInput *in) {
    return in->fire_pressed || in->action_pressed || in->typed != 0 || in->left_pressed ||
           in->right_pressed || in->up_pressed || in->down_pressed;
}

void tower_stacker_zoom(const tower_stacker_State *s, float *top_y, float *floor_h) {
    if (!s->over || s->score < TS_SCROLL_FLOORS || s->over_t <= 0.0f) {
        *top_y = s->base_y_move;
        *floor_h = s->base_h;
        return;
    }
    float target_y = s->bar_y0 - s->base_h0; /* the tower ends up filling the screen from here down */
    float hfit = ((float)s->h - target_y) / (float)(s->score + 1);
    float p = arc_clampf(s->over_t / TS_ZOOM_TIME, 0.0f, 1.0f);
    *top_y = s->base_y + (target_y - s->base_y) * p;
    *floor_h = s->base_h + (hfit - s->base_h) * p;
}

/* ---- the tower -------------------------------------------------------------------------------- */

/* Records floor `score` (called with the NEW score already incremented) and rolls its windows. */
static void record_floor(tower_stacker_State *s) {
    int i = s->score;
    s->bases_x[i] = (int)(s->base_x + 0.5f); /* round half up */
    s->bases_w[i] = (int)(s->base_w + 0.5f);
    float f8 = s->base_y - s->base_h;
    if (s->score < TS_SCROLL_FLOORS)
        s->base_y = f8; /* the landing line rises one floor per point, only for the first 10 floors */
    s->base_y_move = (float)(int)f8;
    for (int j = 0; j < TS_WINDOWS; j++) {
        if (rng_float(&s->rng) < 0.5f && s->bases_w[i] >= 31)
            s->window[i][j] = rng_range(&s->rng, s->bases_x[i] + 10, s->bases_x[i] + s->bases_w[i] - 20);
        else
            s->window[i][j] = 0;
    }
}

static void new_bar(tower_stacker_State *s) {
    s->state = TS_SLIDING;
    s->bar_w = s->base_w;
    s->bar_h = s->base_h0;
    s->bar_spx = TS_BAR_SPEED_X;
    s->bar_spy = 0.0f;
    /* start at base_x mod 5 so the 5 px steps can land exactly on the tower's left edge */
    s->bar_x = s->base_x - TS_BAR_SPEED_X * (float)(int)(s->base_x / TS_BAR_SPEED_X);
    s->bar_y = s->bar_y0;
    s->bar_alpha = 1.0f;
    s->bar_acc = 0.0f;
}

void tower_stacker_init_sized(tower_stacker_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->base_w0 = (float)w * 0.5f;
    s->base_h0 = (float)(int)((float)h / 20.0f);
    s->base_x0 = ((float)w - s->base_w0) * 0.5f;
    s->bar_y0 = (float)h * 0.25f;
    s->speed_x = TS_BAR_SPEED_X;
    s->speed_y = (((float)h - s->base_h0) - (float)h * 0.25f) / 50.0f;
    s->base_x = s->base_x0;
    s->base_w = s->base_w0;
    s->base_h = s->base_h0;
    s->base_y = (float)h;
    s->score = 0;
    s->over = s->capped = false;
    s->over_t = 0.0f;
    s->perfects = s->cuts = s->landings = 0;
    s->last_cut_class = 0;
    s->last_cut_d = 0.0f;
    record_floor(s); /* floor 0 */
    new_bar(s);
}

void tower_stacker_init(tower_stacker_State *s, unsigned seed) {
    tower_stacker_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

/* ---- landing ---------------------------------------------------------------------------------- */

static void end_game(tower_stacker_State *s) {
    s->over = true;
    s->over_t = 0.0f;
}

static void land(tower_stacker_State *s) {
    float d = fabsf(s->bar_x - s->base_x);
    if (d >= s->base_w - 1.0f) { /* no overlap at all */
        s->state = TS_MISSED;
        end_game(s);
        return;
    }
    if (d < 1.0f) { /* perfect: the full width is kept */
        s->perfects++;
        new_bar(s);
    } else {
        s->cuts++;
        s->last_cut_d = d;
        s->last_cut_class = d < 10.0f ? 0 : (d < 30.0f ? 1 : 2);
        s->base_w -= d;
        s->bar_w = d; /* the cut-off piece falls slowly and fades */
        s->bar_alpha = 0.99f;
        s->bar_spy /= 10.0f;
        s->state = TS_FADING;
        if (s->bar_x > s->base_x) { /* bar to the right: the new floor starts at bar.x, the piece sits at its
                                       right end */
            s->base_x = s->bar_x;
            s->bar_x = s->bar_x + s->base_w;
        } /* bar to the left (or equal): base_x stays, the piece stays at bar.x */
    }
    if (s->score >= TS_MAX_FLOORS) { /* arrays are full: the run ends here */
        s->capped = true;
        end_game(s);
        return;
    }
    s->score++;
    s->landings++;
    record_floor(s);
}

/* ---- the step ----------------------------------------------------------------------------------- */

/* One 50 Hz tick of sideways movement: 5 px, then bounce at the walls. */
static void slide_tick(tower_stacker_State *s) {
    s->bar_x += s->bar_spx;
    if (s->bar_spx > 0.0f && s->bar_x >= (float)s->w - s->bar_w)
        s->bar_spx = -s->bar_spx;
    else if (s->bar_spx < 0.0f && s->bar_x <= 0.0f)
        s->bar_spx = -s->bar_spx;
}

void tower_stacker_step(tower_stacker_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (s->state == TS_SLIDING && drop_requested(in)) { /* once per bar: freeze x, start falling */
        s->state = TS_FALLING;
        s->bar_spx = 0.0f;
        s->bar_spy = s->speed_y;
    }
    float k = dt * 50.0f; /* k = 1 is one tick of the original 50 Hz game */
    if (s->state == TS_SLIDING) {
        s->bar_acc += k;
        while (s->bar_acc >= 1.0f) { /* whole 5 px steps only (see the header) */
            s->bar_acc -= 1.0f;
            slide_tick(s);
        }
    }
    s->bar_y += s->bar_spy * k;
    if (s->score >= TS_SCROLL_FLOORS && s->base_y_move < s->base_y)
        s->base_y_move += k; /* the tower scrolls down 1 px per tick until its top reaches the landing line */
    if (s->state == TS_FADING) {
        s->bar_alpha -= dt / 0.5f; /* 0.5 s fade */
        if (s->bar_alpha <= 0.0f)
            new_bar(s);
        return;
    }
    if (s->state == TS_FALLING && s->bar_y >= s->base_y)
        land(s);
}

bool tower_stacker_is_over(const tower_stacker_State *s) {
    if (!s->over)
        return false;
    float delay = s->score >= TS_SCROLL_FLOORS ? TS_ZOOM_TIME : TS_OVER_DELAY_SHORT;
    return s->over_t >= delay;
}

int tower_stacker_score(const tower_stacker_State *s) {
    return s->score;
}
