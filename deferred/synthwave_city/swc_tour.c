/* Synthwave City - the automatic drone tour: see swc_tour.h. A line-by-line port of src/camera/tour.ts;
 * the comments keep the web version's reasoning. */
#include "swc_tour.h"

#include <math.h>
#include <string.h>

/* The view direction is averaged over this many seconds each side, which takes the edge off fast turns. */
#define VIEW_SMOOTH 1.5
#define NEED_BACK (VIEW_SMOOTH + SWC_TOUR_LOOK_SMOOTH + 0.1)
#define KEEP_BACK 8.0
#define KEEP_AHEAD 7.0

#define ROLL_ACCEL 11.0
#define SUN_PITCH 0.05
#define SECTION (SWC_TOUR_SMOOTH * 1.06)
#define SLOPE 0.26
#define ORBIT_SLOPE 0.45
#define RING_SLOPE 0.3
#define PRE_ROLL 20.0
#define CORNER_MARGIN 3.5
#define CORNER_MARGIN_AVENUE 6.0

static const double VIEW_WEIGHTS[5] = {1.0 / 9.0, 2.0 / 9.0, 0.0, 2.0 / 9.0, 1.0 / 9.0};

/* Segment record layout. */
enum { T0 = 0, DUR, PX, PY, PZ, VX, VY, VZ, LK, LX, LZ, LH, LS };

enum { LOOK_NONE = 0, LOOK_SUN = 1, LOOK_POINT = 2 };
enum { SCENE_CANYON = 0, SCENE_SKIM = 1, SCENE_HIGH = 2, SCENE_MEGA = 3 };
enum { MEGA_ROUTE = 0, MEGA_ORBIT = 1 };

/* Heading index to direction: 0 is -z (towards the sun), 1 is -x, 2 is +z, 3 is +x. Yaw is heading * PI / 2.
 */
static const double DIR_X[4] = {0.0, -1.0, 0.0, 1.0};
static const double DIR_Z[4] = {-1.0, 0.0, 1.0, 0.0};

static double angle_delta(double a, double b) {
    return swc_wrap_delta(b, a, SWC_TAU);
}

/* Integral over u in [-SMOOTH, t] of the kernel times the distance past a corner: see cornerLimit. */
static double cut(double t) {
    double d = SWC_TOUR_SMOOTH;
    if (t <= -d)
        return 0.0;
    if (t >= d)
        return t;
    double near = pow(d - fabs(t), 3.0) / (6.0 * d * d);
    return t <= 0.0 ? near : t + near;
}

double swc_tour_corner_limit(double free_in, double free_out) {
    double lo = 0.0, hi = 200.0;
    for (int i = 0; i < 40; i++) {
        double v = (lo + hi) / 2.0;
        bool ok = false;
        for (int k = -200; k <= 200 && !ok; k++) {
            double t = ((double)k / 200.0) * SWC_TOUR_SMOOTH;
            ok = v * cut(t) <= free_in && v * cut(-t) <= free_out;
        }
        if (ok)
            lo = v;
        else
            hi = v;
    }
    return lo;
}

static double wrap_cells(double d) {
    return d - (double)SWC_CELLS * floor(d / (double)SWC_CELLS + 0.5);
}

/* How strongly a look is applied when its target is `delta` radians off the direction of travel. */
static double look_taper(double delta) {
    return 1.0 - swc_smoothstep(0.9, SWC_PI, fabs(delta));
}

/* ---- generator ---- */

static double cell_height(const SwcTour *t, int cx, int cz) {
    return (double)t->city->cells[swc_cell_index(cx, cz)].height;
}

/* How tall the city is a few blocks ahead of a node in a direction: the tour drifts towards towers. */
static double interest(const SwcTour *t, int ix, int iz, int h) {
    double sum = 0.0;
    for (int step = 3; step <= 6; step += 3) {
        int nx = ix + (int)DIR_X[h] * step, nz = iz + (int)DIR_Z[h] * step;
        sum += cell_height(t, nx - 1, nz - 1) + cell_height(t, nx, nz - 1) + cell_height(t, nx - 1, nz) +
               cell_height(t, nx, nz);
    }
    return sum / 8.0;
}

/* Cells from a node to the next avenue crossing along a heading, 1..AVENUE_EVERY. */
static int cells_to_avenue(int ix, int iz, int h) {
    int c = (((((h & 1) == 0) ? iz : ix) % SWC_AVENUE_EVERY) + SWC_AVENUE_EVERY) % SWC_AVENUE_EVERY;
    return h >= 2 ? SWC_AVENUE_EVERY - c : (c == 0 ? SWC_AVENUE_EVERY : c);
}

/* Index of the megatower nearest a point, or -1; its distance is left in t->near_d. */
static int nearest_mega(SwcTour *t, double x, double z) {
    int best = -1;
    double best_d = INFINITY;
    for (int i = 0; i < t->mega_count; i++) {
        double d =
            hypot(swc_wrap_delta(t->mega_x[i], x, SWC_TILE), swc_wrap_delta(t->mega_z[i], z, SWC_TILE));
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    t->near_d = best_d;
    return best;
}

static void look_at_mega(SwcTour *t, int index, double strength) {
    t->n_look = LOOK_POINT;
    t->n_look_x = t->mega_x[index];
    t->n_look_z = t->mega_z[index];
    t->n_look_h = t->mega_h[index];
    t->n_look_s = strength;
}

/* Choose a megatower and the corner of its orbit to arrive at: near, ahead, and not the last two visited. */
static void pick_mega(SwcTour *t, int ix, int iz, int h) {
    int left = (h + 1) & 3;
    double best = INFINITY;
    for (int m = 0; m < t->mega_count; m++) {
        if ((m == t->last_mega && t->mega_count > 1) || (m == t->older_mega && t->mega_count > 2))
            continue;
        for (int c = 0; c < 4; c++) {
            int side = 2 * t->ring + 1;
            int tx = t->mega_cell_x[m] - t->ring + (c & 1) * side;
            int tz = t->mega_cell_z[m] - t->ring + (c >> 1) * side;
            double dx = wrap_cells((double)(tx - ix)), dz = wrap_cells((double)(tz - iz));
            double ahead = dx * DIR_X[h] + dz * DIR_Z[h];
            double across = dx * DIR_X[left] + dz * DIR_Z[left];
            double cost = fabs(ahead) + fabs(across) + swc_rng_next(&t->rng) * 1.5;
            if (ahead < 0.0)
                cost += 8.0;
            else if (ahead == 0.0)
                cost += 3.0;
            if (across != 0.0)
                cost += 2.0;
            if (fabs(ahead) + fabs(across) < 3.0)
                cost += 20.0;
            if (cost < best) {
                best = cost;
                t->mega_index = m;
                t->target_ix = tx;
                t->target_iz = tz;
            }
        }
    }
}

static void pick_scene(SwcTour *t, int ix, int iz, int h, double y) {
    SwcRng *rng = &t->rng;
    int from = t->mega_done ? SCENE_MEGA : t->scene;
    t->mega_done = false;
    double w_canyon = from == SCENE_CANYON ? 0.0 : 3.5;
    double w_skim = from == SCENE_SKIM ? 0.0 : 3.0;
    double w_high = from == SCENE_HIGH ? 0.0 : 1.5;
    double w_mega = (from == SCENE_MEGA || t->mega_count == 0) ? 0.0 : 2.5;
    if (from == SCENE_MEGA) {
        /* Off the top of a tower: cruise for a while, or dive. */
        w_high = 2.5;
        w_skim = 3.0;
        w_canyon = 2.5;
    }
    double r = swc_rng_next(rng) * (w_canyon + w_skim + w_high + w_mega);
    if (r < w_canyon) {
        t->scene = SCENE_CANYON;
        t->scene_y = swc_rng_range(rng, 60.0, 180.0);
        t->legs_left = swc_rng_int(rng, 3, 5);
        t->legs_max = t->legs_left;
    } else if (r < w_canyon + w_skim) {
        t->scene = SCENE_SKIM;
        t->scene_y = swc_rng_range(rng, SWC_TOUR_MIN_Y, 9.0);
        t->legs_left = swc_rng_int(rng, 2, 3);
        t->legs_max = t->legs_left + 6;
    } else if (r < w_canyon + w_skim + w_high) {
        t->scene = SCENE_HIGH;
        t->scene_y = swc_rng_range(rng, 520.0, 680.0);
        t->legs_left = swc_rng_int(rng, 2, 4);
        t->legs_max = t->legs_left + 5;
    } else {
        t->scene = SCENE_MEGA;
        t->mega_phase = MEGA_ROUTE;
        t->scene_y = y > 350.0 ? y : swc_rng_range(rng, 120.0, 220.0);
        t->legs_left = 1;
        t->legs_max = 1;
        /* Mostly a wide circle that shows the whole tower; sometimes a tight climb up its face. */
        t->ring = swc_rng_chance(rng, 0.7) ? 1 : 0;
        pick_mega(t, ix, iz, h);
    }
}

static void last_mega_visited(SwcTour *t, int m) {
    t->older_mega = t->last_mega;
    t->last_mega = m;
}

/* The megatower scene: fly to a corner of its orbit, then climb round it looking at the tower. */
static void decide_mega(SwcTour *t, int ix, int iz, int h, double ex, double ey, double ez) {
    SwcRng *rng = &t->rng;
    int m = t->mega_index;
    int left = (h + 1) & 3;
    if (t->mega_phase == MEGA_ROUTE) {
        double dx = wrap_cells((double)(t->target_ix - ix)), dz = wrap_cells((double)(t->target_iz - iz));
        double ahead = dx * DIR_X[h] + dz * DIR_Z[h];
        double side = dx * DIR_X[left] + dz * DIR_Z[left];
        if (ahead != 0.0 || side != 0.0) {
            int turn = 0;
            double cells = 1.0;
            if (ahead > 0.0)
                cells = ahead;
            else if (side != 0.0) {
                turn = side > 0.0 ? 1 : -1;
                cells = fabs(side);
            } else
                turn = swc_rng_chance(rng, 0.5) ? 1 : -1;
            int h2 = (h + turn + 4) & 3;
            double len = cells * SWC_CELL;
            bool avenue = swc_is_avenue((h2 & 1) == 0 ? ix : iz);
            t->n_turn = turn;
            t->n_len = len;
            t->n_y1 = ey + swc_clamp(t->scene_y - ey, -SLOPE * len, SLOPE * len);
            t->n_v = avenue ? swc_rng_range(rng, 36.0, 42.0) : swc_rng_range(rng, 28.0, 32.0);
            double mx = ex + DIR_X[h2] * len * 0.5, mz = ez + DIR_Z[h2] * len * 0.5;
            double d =
                hypot(swc_wrap_delta(t->mega_x[m], mx, SWC_TILE), swc_wrap_delta(t->mega_z[m], mz, SWC_TILE));
            if (d < 1300.0)
                look_at_mega(t, m, d < 500.0 ? 0.75 : 0.55);
            return;
        }
        t->mega_phase = MEGA_ORBIT;
        t->orbit_left = t->ring > 0 ? swc_rng_int(rng, 2, 4) : swc_rng_int(rng, 3, 4);
        t->orbit_top = t->mega_h[m] * swc_rng_range(rng, 0.42, 0.58);
        if (ey > t->orbit_top - 100.0)
            t->orbit_top = fmin(t->mega_h[m] * 0.85, ey + 120.0);
    }
    if (t->orbit_left <= 0) {
        /* Leave along the line we are on, and let the next scene take over. */
        int k = cells_to_avenue(ix, iz, h);
        double len = (double)(k < 2 ? k + SWC_AVENUE_EVERY : k) * SWC_CELL;
        t->n_turn = 0;
        t->n_len = len;
        t->n_y1 = ey + fmin(30.0, 0.1 * len);
        t->n_v = swc_rng_range(rng, 38.0, 44.0);
        last_mega_visited(t, m);
        t->scene = SCENE_CANYON;
        t->mega_done = true;
        t->legs_left = 0;
        return;
    }
    /* The two sides of the orbit's square that leave this corner. */
    double span = (double)(2 * t->ring + 1) * SWC_CELL;
    int i = (int)wrap_cells((double)(ix - t->mega_cell_x[m] + t->ring));
    int j = (int)wrap_cells((double)(iz - t->mega_cell_z[m] + t->ring));
    int side_x = i == 0 ? 3 : 1;
    int side_z = j == 0 ? 2 : 0;
    int h2;
    if (h == side_x || h == side_z)
        h2 = h;
    else
        h2 = ((h + 2) & 3) == side_x ? side_z : side_x;
    int d = (h2 - h + 4) & 3;
    t->n_turn = d == 0 ? 0 : (d == 1 ? 1 : -1);
    double steep = (t->ring > 0 ? RING_SLOPE : ORBIT_SLOPE) * span;
    t->n_len = span;
    t->n_y1 = ey + swc_clamp((t->orbit_top - ey) / (double)t->orbit_left, -steep, steep);
    t->n_v = t->ring > 0 ? swc_rng_range(rng, 36.0, 42.0) : 32.0;
    look_at_mega(t, m, 0.9);
    t->orbit_left--;
}

/* Decide the leg that follows the pending one. */
static void decide(SwcTour *t) {
    int h = t->p_h;
    double ex = t->gx + DIR_X[h] * t->p_len;
    double ez = t->gz + DIR_Z[h] * t->p_len;
    double ey = t->p_y1;
    int ix = (int)swc_round_half_up(ex / SWC_CELL), iz = (int)swc_round_half_up(ez / SWC_CELL);
    t->n_look = LOOK_NONE;
    t->n_look_s = 0.0;
    if (t->scene != SCENE_MEGA && (t->legs_left <= 0 || t->legs_max <= 0))
        pick_scene(t, ix, iz, h, ey);
    if (t->scene == SCENE_MEGA) {
        decide_mega(t, ix, iz, h, ex, ey, ez);
        return;
    }
    SwcRng *rng = &t->rng;
    int scene = t->scene;
    bool even = (h & 1) == 0;
    bool run_avenue = swc_is_avenue(even ? ix : iz);
    bool cross_avenue = swc_is_avenue(even ? iz : ix);

    /* Turn or carry straight on. Low and high runs want an avenue; the canyon wanders. */
    double p_turn;
    if (scene == SCENE_CANYON)
        p_turn = cross_avenue ? (run_avenue ? 0.65 : 0.8) : (run_avenue ? 0.2 : 0.1);
    else if (!run_avenue)
        p_turn = cross_avenue ? 0.85 : 0.04;
    else
        p_turn = cross_avenue ? (scene == SCENE_SKIM ? 0.3 : 0.45) : 0.03;
    double w_straight = (1.0 - p_turn) * (30.0 + interest(t, ix, iz, h));
    double w_left = 0.5 * p_turn * (30.0 + interest(t, ix, iz, (h + 1) & 3));
    double w_right = 0.5 * p_turn * (30.0 + interest(t, ix, iz, (h + 3) & 3));
    double r = swc_rng_next(rng) * (w_straight + w_left + w_right);
    int turn = r < w_straight ? 0 : (r < w_straight + w_left ? 1 : -1);
    int h2 = (h + turn + 4) & 3;
    bool avenue = turn == 0 ? run_avenue : cross_avenue;

    int k = cells_to_avenue(ix, iz, h2);
    int cells;
    if (scene == SCENE_CANYON)
        cells = swc_rng_chance(rng, 0.7) ? (k >= 2 ? k : k + SWC_AVENUE_EVERY) : swc_rng_int(rng, 2, 4);
    else if (avenue)
        cells = k + (k < SWC_AVENUE_EVERY || swc_rng_chance(rng, 0.5) ? SWC_AVENUE_EVERY : 0);
    else
        cells = k;
    double len = (double)cells * SWC_CELL;

    double aim = scene == SCENE_CANYON ? swc_clamp(t->scene_y + swc_rng_range(rng, -25.0, 25.0), 45.0, 200.0)
                                       : t->scene_y;
    double y1 = ey + swc_clamp(aim - ey, -SLOPE * len, SLOPE * len);
    if (scene == SCENE_CANYON || fabs(ey - t->scene_y) < 25.0)
        t->legs_left--;
    t->legs_max--;

    double mid = (ey + y1) / 2.0;
    double v;
    if (scene == SCENE_CANYON)
        v = avenue ? swc_rng_range(rng, 34.0, 40.0) : swc_rng_range(rng, 27.0, 31.0);
    else if (scene == SCENE_SKIM)
        v = mid < 40.0 ? swc_rng_range(rng, 30.0, 36.0) : swc_rng_range(rng, 40.0, 46.0);
    else
        v = mid > 250.0 ? swc_rng_range(rng, 50.0, 60.0) : swc_rng_range(rng, 40.0, 46.0);

    /* Now and then, look at something: the nearest megatower, or from high up the sun. */
    double mx = ex + DIR_X[h2] * len * 0.5, mz = ez + DIR_Z[h2] * len * 0.5;
    int mega = nearest_mega(t, mx, mz);
    double mega_chance = scene == SCENE_CANYON ? 0.45 : (scene == SCENE_HIGH ? 0.55 : 0.3);
    if (mega >= 0 && t->near_d < 1100.0 && swc_rng_chance(rng, mega_chance)) {
        look_at_mega(t, mega, swc_rng_range(rng, 0.45, 0.7));
    } else if (mid > 300.0 && (h2 & 1) == 1 && swc_rng_chance(rng, 0.6)) {
        t->n_look = LOOK_SUN;
        t->n_look_s = 0.6;
    }

    t->n_turn = turn;
    t->n_len = len;
    t->n_y1 = y1;
    t->n_v = v;
}

static void push(SwcTour *t, double len, double v, double at, double slope) {
    double *s = t->seg + t->count * SWC_TOUR_STRIDE;
    int h = t->p_h;
    s[T0] = t->gt;
    s[DUR] = len / v;
    s[PX] = t->gx + DIR_X[h] * at;
    s[PY] = t->gy + slope * at;
    s[PZ] = t->gz + DIR_Z[h] * at;
    s[VX] = DIR_X[h] * v;
    s[VY] = slope * v;
    s[VZ] = DIR_Z[h] * v;
    s[LK] = (double)t->p_look;
    s[LX] = t->p_look_x;
    s[LZ] = t->p_look_z;
    s[LH] = t->p_look_h;
    s[LS] = t->p_look_s;
    t->gt += len / v;
    t->count++;
}

/* Keep stored coordinates small: move everything by whole tiles, which changes no difference. */
static void rebase(SwcTour *t, int field, double shift, bool is_x) {
    for (int i = 0; i < t->count; i++)
        t->seg[i * SWC_TOUR_STRIDE + field] -= shift;
    if (is_x)
        t->gx -= shift;
    else
        t->gz -= shift;
}

/* Decide the next leg, write the pending one as one to three segments, and make the next one pending. */
static void step(SwcTour *t) {
    decide(t);
    int h = t->p_h;
    double end_corner = 0.0;
    if (t->n_turn != 0) {
        double ex = t->gx + DIR_X[h] * t->p_len, ez = t->gz + DIR_Z[h] * t->p_len;
        int ix = (int)swc_round_half_up(ex / SWC_CELL), iz = (int)swc_round_half_up(ez / SWC_CELL);
        bool even = (h & 1) == 0;
        int kinds = (swc_is_avenue(even ? ix : iz) ? 1 : 0) + (swc_is_avenue(even ? iz : ix) ? 1 : 0);
        end_corner = fmin(t->corner_limit[kinds], 0.5 * (t->p_v + t->n_v));
    }

    double len = t->p_len;
    double slope = (t->p_y1 - t->gy) / len;
    double a = t->p_corner > 0.0 ? t->p_corner * SECTION : 0.0;
    double b = end_corner > 0.0 ? end_corner * SECTION : 0.0;
    if (a + b > len) {
        /* Too short for two corner sections: one speed that keeps both corners' windows constant. */
        double v = len / (2.0 * SECTION);
        if (t->p_corner > 0.0)
            v = fmin(v, t->p_corner);
        if (end_corner > 0.0)
            v = fmin(v, end_corner);
        push(t, len, v, 0.0, slope);
    } else if (len - a - b < 12.0) {
        if (a > 0.0 && b > 0.0) {
            double a2 = (len * a) / (a + b);
            push(t, a2, t->p_corner, 0.0, slope);
            push(t, len - a2, end_corner, a2, slope);
        } else
            push(t, len, a > 0.0 ? t->p_corner : end_corner, 0.0, slope);
    } else {
        if (a > 0.0)
            push(t, a, t->p_corner, 0.0, slope);
        push(t, len - a - b, t->p_v, a, slope);
        if (b > 0.0)
            push(t, b, end_corner, len - b, slope);
    }

    t->gx += DIR_X[h] * len;
    t->gz += DIR_Z[h] * len;
    t->gy = t->p_y1;
    t->p_h = (h + t->n_turn + 4) & 3;
    t->p_len = t->n_len;
    t->p_y1 = t->n_y1;
    t->p_v = t->n_v;
    t->p_corner = end_corner;
    t->p_look = t->n_look;
    t->p_look_x = t->n_look_x;
    t->p_look_z = t->n_look_z;
    t->p_look_h = t->n_look_h;
    t->p_look_s = t->n_look_s;

    if (fabs(t->gx) > 2.0 * SWC_TILE)
        rebase(t, PX, swc_round_half_up(t->gx / SWC_TILE) * SWC_TILE, true);
    if (fabs(t->gz) > 2.0 * SWC_TILE)
        rebase(t, PZ, swc_round_half_up(t->gz / SWC_TILE) * SWC_TILE, false);
}

static void rewind_tour(SwcTour *t) {
    const SwcTourStart *s = &t->start;
    swc_rng_seed(&t->rng, t->start_seed);
    t->count = 0;
    t->cursor = 0;
    t->dropped = false;
    t->mega_index = -1;
    t->mega_done = false;
    t->last_mega = -1;
    t->older_mega = -1;

    int h = s->heading;
    bool even = (h & 1) == 0;
    /* Snap onto the line, then find the next intersection ahead. */
    t->gx = even ? swc_round_half_up(s->x / SWC_CELL) * SWC_CELL : s->x;
    t->gz = even ? s->z : swc_round_half_up(s->z / SWC_CELL) * SWC_CELL;
    t->gy = s->y;
    t->gt = 0.0;
    t->p_h = h;
    double c = even ? t->gz : t->gx;
    bool forward = h >= 2;
    double node = forward ? ceil(c / SWC_CELL - 1e-9) * SWC_CELL : floor(c / SWC_CELL + 1e-9) * SWC_CELL;
    double to_node = fabs(node - c);
    t->p_corner = 0.0;
    t->p_look = LOOK_NONE;
    t->p_look_s = 0.0;

    if (t->canonical) {
        t->scene = SCENE_CANYON;
        t->scene_y = 125.0;
        t->legs_left = 2;
        t->legs_max = 4;
        t->p_len = to_node + 2.0 * SWC_AVENUE_EVERY * SWC_CELL;
        t->p_y1 = 115.0;
        t->p_v = 38.0;
    } else {
        bool line_avenue = swc_is_avenue((int)swc_round_half_up((even ? t->gx : t->gz) / SWC_CELL));
        if (s->y < 40.0) {
            t->scene = SCENE_SKIM;
            t->scene_y = fmax(SWC_TOUR_MIN_Y, fmin(s->y, 9.0));
            t->p_v = 32.0;
        } else if (s->y < 330.0) {
            t->scene = SCENE_CANYON;
            t->scene_y = swc_clamp(s->y, 55.0, 185.0);
            t->p_v = line_avenue ? 36.0 : 29.0;
        } else {
            t->scene = SCENE_HIGH;
            t->scene_y = swc_clamp(s->y, 520.0, 680.0);
            t->p_v = 46.0;
        }
        t->legs_left = 2;
        t->legs_max = 6;
        t->p_len = to_node + SWC_CELL * (double)swc_rng_int(&t->rng, 1, 3);
        t->p_y1 = s->y + swc_clamp(t->scene_y - s->y, -SLOPE * t->p_len, SLOPE * t->p_len);
    }

    /* A run-in before time zero along the first leg's own line, so that the kernel has something to
     * average and the smoothed path passes exactly through the start. */
    double *seg = t->seg + t->count * SWC_TOUR_STRIDE;
    double climb = ((t->p_y1 - t->gy) / t->p_len) * t->p_v;
    seg[T0] = -PRE_ROLL;
    seg[DUR] = PRE_ROLL;
    seg[PX] = t->gx - DIR_X[h] * t->p_v * PRE_ROLL;
    seg[PY] = t->gy - climb * PRE_ROLL;
    seg[PZ] = t->gz - DIR_Z[h] * t->p_v * PRE_ROLL;
    seg[VX] = DIR_X[h] * t->p_v;
    seg[VY] = climb;
    seg[VZ] = DIR_Z[h] * t->p_v;
    seg[LK] = LOOK_NONE;
    seg[LX] = 0.0;
    seg[LZ] = 0.0;
    seg[LH] = 0.0;
    seg[LS] = 0.0;
    t->count = 1;
}

void swc_tour_init(SwcTour *t, const SwcCity *city, uint32_t seed) {
    memset(t, 0, sizeof *t);
    t->city = city;
    for (int i = 0; i < city->landmark_count && t->mega_count < SWC_TOUR_MAX_MEGA; i++) {
        const SwcLandmark *l = &city->landmarks[i];
        if (l->kind != SWC_LM_MEGATOWER)
            continue;
        int m = t->mega_count++;
        t->mega_x[m] = l->x;
        t->mega_z[m] = l->z;
        t->mega_h[m] = l->height;
        t->mega_cell_x[m] = l->cell_x;
        t->mega_cell_z[m] = l->cell_z;
    }
    /* Distance kept from the building line through a corner, more on an avenue. */
    double free_street = SWC_STREET.corridor_half - CORNER_MARGIN;
    double free_avenue = SWC_AVENUE.corridor_half - CORNER_MARGIN_AVENUE;
    t->corner_limit[0] = swc_tour_corner_limit(free_street, free_street) * 0.97;
    t->corner_limit[1] = swc_tour_corner_limit(free_street, free_avenue) * 0.97;
    t->corner_limit[2] = swc_tour_corner_limit(free_avenue, free_avenue) * 0.97;
    swc_tour_restart(t, NULL, seed);
}

void swc_tour_restart(SwcTour *t, const SwcTourStart *start, uint32_t seed) {
    if (start == NULL) {
        const SwcLandmark *downtown = NULL;
        for (int i = 0; i < t->city->landmark_count; i++) {
            if (t->city->landmarks[i].kind == SWC_LM_DOWNTOWN) {
                downtown = &t->city->landmarks[i];
                break;
            }
        }
        int cell_x = downtown ? downtown->cell_x : SWC_CELLS / 2;
        int cell_z = downtown ? downtown->cell_z : SWC_CELLS / 2;
        t->canonical = true;
        /* Two avenue crossings short of downtown, on the avenue that runs past it towards the sun. */
        t->start.x = swc_round_half_up((double)cell_x / SWC_AVENUE_EVERY) * SWC_AVENUE_EVERY * SWC_CELL;
        t->start.y = 150.0;
        t->start.z = ceil((double)(cell_z + 4) / SWC_AVENUE_EVERY) * SWC_AVENUE_EVERY * SWC_CELL;
        t->start.heading = 0;
    } else {
        t->canonical = false;
        t->start.x = start->x;
        t->start.y = start->y;
        t->start.z = start->z;
        t->start.heading = start->heading & 3;
    }
    t->start_seed = seed;
    rewind_tour(t);
}

/* Make sure segments cover the kernels' reach around time t. */
static void ensure(SwcTour *t, double time) {
    double *seg = t->seg;
    if (t->dropped && time - NEED_BACK < seg[T0])
        rewind_tour(t);
    while (t->gt < time + KEEP_AHEAD) {
        if (t->count > SWC_TOUR_CAP - 4) {
            int k = 0;
            while (k < t->count - 1 &&
                   seg[k * SWC_TOUR_STRIDE + T0] + seg[k * SWC_TOUR_STRIDE + DUR] < time - KEEP_BACK)
                k++;
            if (k == 0)
                return;
            memmove(seg, seg + k * SWC_TOUR_STRIDE,
                    (size_t)(t->count - k) * SWC_TOUR_STRIDE * sizeof(double));
            t->count -= k;
            t->cursor = t->cursor - k > 0 ? t->cursor - k : 0;
            t->dropped = true;
        }
        step(t);
    }
}

/* ---- evaluator ---- */

static int find(SwcTour *t, double time) {
    const double *seg = t->seg;
    int i = t->cursor < t->count - 1 ? t->cursor : t->count - 1;
    while (i > 0 && seg[i * SWC_TOUR_STRIDE + T0] > time)
        i--;
    while (i < t->count - 1 && seg[i * SWC_TOUR_STRIDE + T0] + seg[i * SWC_TOUR_STRIDE + DUR] <= time)
        i++;
    t->cursor = i;
    return i;
}

/* The unsmoothed route at time t, into ox/oy/oz; x and z are relative to the origin (bx, bz). */
static void raw(SwcTour *t, double time) {
    const double *s = t->seg + find(t, time) * SWC_TOUR_STRIDE;
    double u = time - s[T0];
    t->ox = s[PX] - t->bx + s[VX] * u;
    t->oy = s[PY] + s[VY] * u;
    t->oz = s[PZ] - t->bz + s[VZ] * u;
}

/* The smoothed position and velocity at time t, into ox.. and ovx..: the route convolved with a triangle
 * kernel. Each piece of the integrand is a quadratic, so Simpson's rule is exact. x and z are relative to
 * the origin (bx, bz): differences of stored coordinates do not change when the buffer is moved by whole
 * tiles, so the result is the same to the last bit whenever that happened. */
static void smooth(SwcTour *t, double time) {
    const double *seg = t->seg;
    double d = SWC_TOUR_SMOOTH;
    double end = time + d;
    double a = time - d;
    int i = find(t, a);
    double x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
    while (a < end) {
        const double *s = seg + i * SWC_TOUR_STRIDE;
        double t0 = s[T0];
        double seg_end = i < t->count - 1 ? t0 + s[DUR] : INFINITY;
        double b = fmin(end, seg_end);
        if (a < time && b > time)
            b = time;
        double w = b - a;
        double ka = (1.0 - fabs(a - time) / d) / d;
        double kb = (1.0 - fabs(b - time) / d) / d;
        double km = (ka + kb) / 2.0;
        double ua = a - t0, ub = b - t0, um = (ua + ub) / 2.0;
        double px = s[PX] - t->bx, py = s[PY], pz = s[PZ] - t->bz;
        double sx = s[VX], sy = s[VY], sz = s[VZ];
        double f = w / 6.0;
        x += f * (ka * (px + sx * ua) + 4.0 * km * (px + sx * um) + kb * (px + sx * ub));
        y += f * (ka * (py + sy * ua) + 4.0 * km * (py + sy * um) + kb * (py + sy * ub));
        z += f * (ka * (pz + sz * ua) + 4.0 * km * (pz + sz * um) + kb * (pz + sz * ub));
        double area = w * km;
        vx += area * sx;
        vy += area * sy;
        vz += area * sz;
        a = b;
        if (a >= seg_end)
            i++;
    }
    t->ox = x;
    t->oy = y;
    t->oz = z;
    t->ovx = vx;
    t->ovy = vy;
    t->ovz = vz;
}

/* Acceleration to the left of the direction of travel at time t: the second difference of the route. */
static double left_accel(SwcTour *t, double time, double right_x, double right_z) {
    double d = SWC_TOUR_SMOOTH;
    raw(t, time - d);
    double ax = t->ox, az = t->oz;
    raw(t, time);
    ax -= 2.0 * t->ox;
    az -= 2.0 * t->oz;
    raw(t, time + d);
    ax += t->ox;
    az += t->oz;
    return -(ax * right_x + az * right_z) / (d * d);
}

/* The pose before the view is averaged. */
static void pose(SwcTour *t, double time, SwcTourSample *out) {
    const double *seg = t->seg;
    int origin = find(t, time) * SWC_TOUR_STRIDE;
    t->bx = seg[origin + PX];
    t->bz = seg[origin + PZ];
    smooth(t, time + SWC_TOUR_LOOK_AHEAD);
    double ax = t->ox, ay = t->oy, az = t->oz;
    smooth(t, time);
    double x = t->ox, y = t->oy, z = t->oz;
    double vx = t->ovx, vy = t->ovy, vz = t->ovz;
    double cx = ax - x, cy = ay - y, cz = az - z;
    double yaw_path = atan2(-cx, -cz);
    double pitch_path = atan2(cy, hypot(cx, cz));
    double pitch_base = 0.55 * pitch_path - 0.17 * swc_smoothstep(150.0, 600.0, y) +
                        0.05 * (1.0 - swc_smoothstep(SWC_TOUR_MIN_Y, 50.0, y));

    /* Bank with the turn, averaged over the look-ahead so it starts and ends with the yaw. */
    double speed_h = hypot(vx, vz);
    double right_x = speed_h > 1e-6 ? -vz / speed_h : 1.0;
    double right_z = speed_h > 1e-6 ? vx / speed_h : 0.0;
    double lateral = 0.25 * left_accel(t, time, right_x, right_z) +
                     0.5 * left_accel(t, time + SWC_TOUR_LOOK_AHEAD / 2.0, right_x, right_z) +
                     0.25 * left_accel(t, time + SWC_TOUR_LOOK_AHEAD, right_x, right_z);
    double roll = SWC_TOUR_MAX_ROLL * tanh(lateral / ROLL_ACCEL);

    /* Looks: each segment's share of a wide kernel pulls the view towards that segment's target. */
    double dl = SWC_TOUR_LOOK_SMOOTH;
    double px = swc_wrap_tile(swc_wrap_tile(t->bx) + x), pz = swc_wrap_tile(swc_wrap_tile(t->bz) + z);
    double yaw_off = 0.0, pitch_off = 0.0;
    double end = time + dl;
    double a = time - dl;
    int i = find(t, a);
    while (a < end) {
        const double *s = seg + i * SWC_TOUR_STRIDE;
        double seg_end = i < t->count - 1 ? s[T0] + s[DUR] : INFINITY;
        double b = fmin(end, seg_end);
        if (a < time && b > time)
            b = time;
        int kind = (int)s[LK];
        if (kind != LOOK_NONE) {
            double ka = (1.0 - fabs(a - time) / dl) / dl;
            double kb = (1.0 - fabs(b - time) / dl) / dl;
            double share = (b - a) * 0.5 * (ka + kb) * s[LS];
            double yaw_to = 0.0, pitch_to = SUN_PITCH;
            if (kind == LOOK_POINT) {
                double dx = swc_wrap_delta(s[LX], px, SWC_TILE), dz = swc_wrap_delta(s[LZ], pz, SWC_TILE);
                double dist = hypot(dx, dz);
                yaw_to = atan2(-dx, -dz);
                pitch_to =
                    atan2(swc_clamp(0.35 * (0.85 * s[LH] - y), -0.3 * dist, 0.42 * dist), fmax(dist, 1.0));
            }
            double delta = angle_delta(yaw_path, yaw_to);
            double taper = look_taper(delta);
            yaw_off += share * taper * delta;
            pitch_off += share * taper * (pitch_to - pitch_base);
        }
        a = b;
        if (a >= seg_end)
            i++;
    }

    out->x = px;
    out->y = y;
    out->z = pz;
    out->yaw = swc_wrap_delta(yaw_path + yaw_off, 0.0, SWC_TAU);
    out->pitch = swc_clamp(pitch_base + pitch_off, -1.2, 1.2);
    out->roll = roll;
    out->vx = vx;
    out->vy = vy;
    out->vz = vz;
    out->speed = sqrt(vx * vx + vy * vy + vz * vz);
}

void swc_tour_sample(SwcTour *t, double time, SwcTourSample *out) {
    ensure(t, time);
    pose(t, time, out);
    /* Average the view direction over a short window round t. Position and velocity are not touched. */
    SwcTourSample *tap = &t->tap;
    double yaw = 0.0, pitch = out->pitch / 3.0, roll = out->roll / 3.0;
    for (int k = 0; k < 5; k++) {
        double w = VIEW_WEIGHTS[k];
        if (w == 0.0)
            continue;
        pose(t, time + (double)(k - 2) * 0.5 * VIEW_SMOOTH, tap);
        yaw += w * angle_delta(out->yaw, tap->yaw);
        pitch += w * tap->pitch;
        roll += w * tap->roll;
    }
    out->yaw = swc_wrap_delta(out->yaw + yaw, 0.0, SWC_TAU);
    out->pitch = pitch;
    out->roll = roll;
}
