#include "different_color_logic.h"

#include "arcade/playfield.h"

#define DC_TOLERANCE 8.0f  /* a click may miss the cell by 8 px on each side */
#define DC_MAX_TRIES 10000 /* rejection sampling never loops forever */

/* ---- pure rules -------------------------------------------------------------------------- */

void different_color_grid_for_score(int score, int *cols, int *rows) {
    int n = score + 2;
    int c = (int)(sqrt((double)n) - 1e-7) + 1;
    int r = (int)((double)n / c - 1e-7) + 1;
    *cols = c;
    *rows = r;
}

bool different_color_random_color_ok(float r, float g, float b) {
    bool grey = fabsf(r - g) < 0.1f && fabsf(r - b) < 0.1f && fabsf(g - b) < 0.1f;
    bool dark = r < 0.5f && g < 0.5f && b < 0.5f;
    return !(grey || dark);
}

static float luma(const float c[3]) {
    return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
}

bool different_color_diff_ok(const float c1[3], const float c2[3]) {
    float manhattan = fabsf(c2[0] - c1[0]) + fabsf(c2[1] - c1[1]) + fabsf(c2[2] - c1[2]);
    return manhattan >= 0.3f && fabsf(luma(c2) - luma(c1)) >= 0.2f;
}

/* ---- board generation -------------------------------------------------------------------- */

static void random_color(different_color_State *s, float c[3]) {
    for (int tries = 0; tries < DC_MAX_TRIES; tries++) {
        c[0] = rng_float(&s->rng);
        c[1] = rng_float(&s->rng);
        c[2] = rng_float(&s->rng);
        if (different_color_random_color_ok(c[0], c[1], c[2]))
            return;
    }
    c[0] = 0.9f; /* unreachable in practice: a safe fixed colour */
    c[1] = 0.2f;
    c[2] = 0.2f;
}

static void odd_color_for(different_color_State *s, const float base[3], float out[3]) {
    for (int tries = 0; tries < DC_MAX_TRIES; tries++) {
        random_color(s, out);
        if (different_color_diff_ok(base, out))
            return;
    }
    out[0] = base[2]; /* unreachable in practice: rotate the channels */
    out[1] = base[0];
    out[2] = base[1];
}

void different_color_new_board(different_color_State *s) {
    s->n = s->score + 2;
    different_color_grid_for_score(s->score, &s->cols, &s->rows);
    s->rectw = ((float)s->w - 32.0f) / (float)s->cols;
    s->recth = (((float)s->h - 48.0f - 128.0f) / (float)s->rows) * 0.5f; /* two boards share the height */
    s->dif = rng_range(&s->rng, 0, s->n - 1);
    /* every grid cell is coloured (the grid is a full rectangle, so it can have more than n cells) */
    for (int i = 0; i < s->cols * s->rows; i++) {
        random_color(s, s->cell[i]);
        if (i == s->dif)
            odd_color_for(s, s->cell[i], s->odd);
    }
    s->tick_t = 0.0f;
}

void different_color_init_sized(different_color_State *s, unsigned seed, int w, int h) {
    rng_seed(&s->rng, seed);
    s->w = w;
    s->h = h;
    s->y2 = (float)h / 2.0f + 8.0f;
    s->score = 0;
    s->timed = false; /* the first board is untimed */
    s->time_left = DC_TIME_LIMIT;
    s->over = s->capped = s->timed_out = false;
    s->over_t = 0.0f;
    s->solved = s->ticks = s->misses = 0;
    different_color_new_board(s);
}

void different_color_init(different_color_State *s, unsigned seed) {
    different_color_init_sized(s, seed, ARC_PF_W, ARC_PF_H);
}

Vec2 different_color_odd_center(const different_color_State *s) {
    return vec2(DC_X1 + s->rectw * (float)(s->dif % s->cols) + s->rectw * 0.5f,
                DC_Y1 + s->recth * (float)(s->dif / s->cols) + s->recth * 0.5f);
}

/* ---- clicking and time ------------------------------------------------------------------- */

static void end_game(different_color_State *s) {
    s->over = true;
    s->over_t = 0.0f;
}

void different_color_click(different_color_State *s, float lx, float ly) {
    if (s->over)
        return;
    Vec2 c = different_color_odd_center(s);
    float dy = s->y2 - DC_Y1; /* offset from a top-board cell to the same cell on the bottom board */
    bool near_x = fabsf(lx - c.x) <= s->rectw * 0.5f + DC_TOLERANCE;
    bool hit_top = near_x && fabsf(ly - c.y) <= s->recth * 0.5f + DC_TOLERANCE;
    bool hit_bottom = near_x && fabsf(ly - (c.y + dy)) <= s->recth * 0.5f + DC_TOLERANCE;
    if (hit_top || hit_bottom) {
        s->score++;
        s->solved++;
        if (s->score + 2 > DC_MAX_CELLS) { /* a 21st column would not fit the arrays: the run ends here */
            s->capped = true;
            end_game(s);
            return;
        }
        s->timed = true;
        s->time_left = DC_TIME_LIMIT;
        different_color_new_board(s);
        return;
    }
    /* a wrong click counts only inside the playing field, and the white separator strip is safe */
    bool in_field =
        lx >= DC_X1 + 1.0f && lx < (float)s->w - 16.0f && ly >= DC_Y1 + 1.0f && ly < (float)s->h - 80.0f;
    bool on_separator = ly >= s->y2 - 16.0f && ly <= s->y2;
    if (in_field && !on_separator) {
        s->misses++;
        end_game(s);
    }
}

void different_color_step(different_color_State *s, const ArcadeInput *in, float dt) {
    if (s->over) {
        s->over_t += dt;
        return;
    }
    if (in->fire_pressed) { /* D4: the pointer is converted to playfield-local pixels once, here */
        Vec2 local = arc_pf_from_screen(in->aim_x, in->aim_y);
        different_color_click(s, local.x, local.y);
        return; /* a click frame does not tick the clock, so a new board starts at exactly 30 s */
    }
    if (!s->timed)
        return;
    s->time_left -= dt;
    s->tick_t += dt;
    if (s->time_left <= DC_WARN_TIME && s->tick_t >= 0.5f) {
        s->tick_t = 0.0f;
        s->ticks++; /* the front-end alternates "tick" and "tack" by parity */
    }
    if (s->time_left < 0.0f) {
        s->timed_out = true;
        end_game(s);
    }
}

bool different_color_is_over(const different_color_State *s) {
    return s->over && s->over_t >= DC_OVER_DELAY;
}

int different_color_score(const different_color_State *s) {
    return s->score;
}
