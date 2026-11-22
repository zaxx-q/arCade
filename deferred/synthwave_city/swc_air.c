/* Synthwave City - the air traffic layer's data. See swc_air.h. Ported from the web version
 * (src/layers/airtraffic/pack.ts and shapes.ts); the order of every hash and arithmetic step follows it. */
#include "swc_air.h"

#include <math.h>
#include <string.h>

#define LAP_SPEED (SWC_TILE / SWC_LOOP_SECONDS)
#define PHASE_JITTER 0.3

/* Palette indices (the web version's NEON). */
enum {
    N_MAGENTA = 0,
    N_CYAN = 1,
    N_VIOLET = 2,
    N_ORANGE = 3,
    N_YELLOW = 4,
    N_GREEN = 5,
    N_RED = 6,
    N_WHITE = 7
};

/* ---------------------------------------------------------------------------------------------------- */
/* Small craft                                                                                          */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct ShapeLimits {
    double min_scale, max_scale;
    double lateral;  /* largest |constant sideways offset| + weave amplitude */
    double vertical; /* largest |constant vertical offset| + bob amplitude */
} ShapeLimits;

static const ShapeLimits LIMITS[SWC_CRAFT_SHAPES] = {
    {0.9, 1.15, SWC_SKY_LANE_MAX_WEAVE, 6.5}, /* car */
    {0.9, 1.1, 4.5, 5.0},                     /* hauler */
    {0.8, 1.2, SWC_SKY_LANE_MAX_WEAVE, 6.5},  /* drone */
};

/* Engine colour each lane level leans towards, lowest level first: the streams read as layers. */
static const int LEVEL_COLOR[4] = {N_ORANGE, N_MAGENTA, N_CYAN, N_VIOLET};
static const int ANY_ENGINE[6] = {N_MAGENTA, N_CYAN, N_VIOLET, N_ORANGE, N_YELLOW, N_GREEN};
static const int ANY_ACCENT[8] = {N_MAGENTA, N_CYAN, N_VIOLET, N_YELLOW, N_GREEN, N_WHITE, N_CYAN, N_MAGENTA};

/* The engine gives every layer the seed hashInts(city seed, hashText(layer id)); the layer id is
 * "airtraffic". */
uint32_t swc_air_layer_seed(uint32_t city_seed) {
    return swc_layer_seed(city_seed, "airtraffic");
}

/* JavaScript's Math.round: halves go up. */
static double js_round(double v) {
    return floor(v + 0.5);
}

int swc_air_laps_for_lane(int slot, double h) {
    double speed = 27.0 + 13.0 * slot + 5.0 * h;
    double hi = floor(70.0 / LAP_SPEED);
    double lo = ceil(25.0 / LAP_SPEED);
    double laps = js_round(speed / LAP_SPEED);
    if (laps < lo)
        laps = lo;
    if (laps > hi)
        laps = hi;
    return (int)laps;
}

int swc_air_craft_in_lane(int slot, double h, double density) {
    double spacing = 86.0 + 14.0 * slot + 30.0 * h;
    double n = js_round((SWC_TILE / spacing) * density);
    return n < 3.0 ? 3 : (int)n;
}

/* The shape of craft i of a sky lane, which only the pick hash decides. */
static int pick_shape(const SwcLane *lane, uint32_t seed, int li, int i) {
    double pick = swc_hash_unit4(seed, (uint32_t)li, (uint32_t)i, 1);
    double haulers = 0.2 - 0.04 * lane->slot;
    return pick < haulers ? SWC_CRAFT_HAULER : (pick < haulers + 0.2 ? SWC_CRAFT_DRONE : SWC_CRAFT_CAR);
}

void swc_air_pack_craft(const SwcCity *city, double density, uint32_t seed, SwcCraftPack *out) {
    /* The sky lanes in plan order, and what each one holds. */
    int sky[SWC_MAX_LANES];
    int sky_count = 0;
    for (int i = 0; i < city->lane_count; i++)
        if (city->lanes[i].kind == SWC_LANE_SKY)
            sky[sky_count++] = i;

    /* Pass 1: how many craft of each shape, so the data can be written sorted by shape. */
    int per_shape[SWC_CRAFT_SHAPES] = {0, 0, 0};
    for (int li = 0; li < sky_count; li++) {
        const SwcLane *lane = &city->lanes[sky[li]];
        int n = swc_air_craft_in_lane(lane->slot, swc_hash_unit3(seed, (uint32_t)li, 2), density);
        for (int i = 0; i < n; i++)
            per_shape[pick_shape(lane, seed, li, i)]++;
    }
    int total = per_shape[0] + per_shape[1] + per_shape[2];
    if (total > SWC_CRAFT_MAX) { /* cannot happen at density <= 1; keep the buffers whole if it ever does */
        total = SWC_CRAFT_MAX;
    }
    out->shape_start[0] = 0;
    for (int s = 0; s < SWC_CRAFT_SHAPES; s++)
        out->shape_start[s + 1] = out->shape_start[s] + per_shape[s];
    int fill[SWC_CRAFT_SHAPES] = {out->shape_start[0], out->shape_start[1], out->shape_start[2]};
    out->count = total;

    /* Pass 2: fill. */
    for (int li = 0; li < sky_count; li++) {
        const SwcLane *lane = &city->lanes[sky[li]];
        int laps = swc_air_laps_for_lane(lane->slot, swc_hash_unit3(seed, (uint32_t)li, 1));
        int n = swc_air_craft_in_lane(lane->slot, swc_hash_unit3(seed, (uint32_t)li, 2), density);
        double speed = laps * LAP_SPEED;
        int level_color = LEVEL_COLOR[lane->slot % 4];
        double lane_phase = swc_hash_unit3(seed, (uint32_t)li, 3);
        for (int i = 0; i < n; i++) {
#define H(k) swc_hash_unit4(seed, (uint32_t)li, (uint32_t)i, (k))
            int shape = pick_shape(lane, seed, li, i);
            const ShapeLimits *lim = &LIMITS[shape];
            double scale = lim->min_scale + (lim->max_scale - lim->min_scale) * H(2);
            /* Sideways: a constant place in the tube plus a weave, together inside the shape's limit. */
            double weave_amp = lim->lateral * (0.25 + 0.3 * H(3));
            double side = (lim->lateral - weave_amp) * (2.0 * H(4) - 1.0) * 0.95;
            double bob_amp = shape == SWC_CRAFT_HAULER ? 0.4 + 0.5 * H(5) : 0.6 + 1.1 * H(5);
            double lift = (lim->vertical - bob_amp) * (2.0 * H(6) - 1.0) * 0.95;
            double phase = fmod(lane_phase + (i + 0.5 + PHASE_JITTER * (2.0 * H(7) - 1.0)) / n, 1.0);
            /* Periods: weave 9 to 16 s, bob 5 to 9 s, as whole cycles per loop. */
            double weave_cycles = js_round(SWC_LOOP_SECONDS / (9.0 + 7.0 * H(8)));
            double bob_cycles = js_round(SWC_LOOP_SECONDS / (5.0 + 4.0 * H(9)));
            double trail = speed * (1.0 + 0.9 * H(10)) * (shape == SWC_CRAFT_DRONE ? 0.7 : 1.0);
            trail = trail < 30.0 ? 30.0 : (trail > 120.0 ? 120.0 : trail);
            int engine_color = H(11) < 0.68 ? level_color : ANY_ENGINE[(int)floor(H(12) * 6.0)];
            int accent_color = ANY_ACCENT[(int)floor(H(13) * 8.0)];
            double seed_q = floor(H(14) * 4096.0) / 4096.0;
#undef H
            if (fill[shape] >= out->shape_start[shape + 1])
                continue; /* only when the capacity cut the total short */
            int at = fill[shape]++;
            float *r = &out->data[at * SWC_CRAFT_STRIDE];
            r[SWC_CRAFT_FIXED] = (float)(lane->fixed + side);
            r[SWC_CRAFT_Y] = (float)(lane->y + lift);
            r[SWC_CRAFT_AXIS] = (float)lane->axis;
            r[SWC_CRAFT_DIR] = (float)lane->dir;
            r[SWC_CRAFT_LAPS] = (float)laps;
            r[SWC_CRAFT_PHASE] = (float)phase;
            r[SWC_CRAFT_WEAVE_AMP] = (float)weave_amp;
            r[SWC_CRAFT_WEAVE_CYCLES] = (float)weave_cycles;
            r[SWC_CRAFT_BOB_AMP] = (float)bob_amp;
            r[SWC_CRAFT_BOB_CYCLES] = (float)bob_cycles;
            r[SWC_CRAFT_TRAIL] = (float)trail;
            r[SWC_CRAFT_SCALE] = (float)scale;
            r[SWC_CRAFT_SEED] = (float)seed_q;
            r[SWC_CRAFT_SHAPE] = (float)shape;
            r[SWC_CRAFT_ENGINE_COLOR] = (float)engine_color;
            r[SWC_CRAFT_ACCENT_COLOR] = (float)accent_color;
            out->lane_of[at] = (uint16_t)li;
        }
    }
}

static double fract_d(double v) {
    return v - floor(v);
}

void swc_air_craft_state(const float *data, int index, double loop, SwcCraftState *out) {
    const float *o = &data[index * SWC_CRAFT_STRIDE];
    double along =
        fract_d((double)o[SWC_CRAFT_DIR] * (double)o[SWC_CRAFT_LAPS] * loop + (double)o[SWC_CRAFT_PHASE]) *
        SWC_TILE;
    double seed = o[SWC_CRAFT_SEED];
    double lateral =
        (double)o[SWC_CRAFT_WEAVE_AMP] * sin(SWC_TAU * ((double)o[SWC_CRAFT_WEAVE_CYCLES] * loop + seed));
    double vertical =
        (double)o[SWC_CRAFT_BOB_AMP] * sin(SWC_TAU * ((double)o[SWC_CRAFT_BOB_CYCLES] * loop + seed * 7.0));
    double cross = (double)o[SWC_CRAFT_FIXED] + lateral;
    bool along_z = o[SWC_CRAFT_AXIS] > 0.5f;
    out->x = along_z ? cross : along;
    out->z = along_z ? along : cross;
    out->y = (double)o[SWC_CRAFT_Y] + vertical;
    out->along = along;
    out->lateral = lateral;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Hero ships                                                                                           */
/* ---------------------------------------------------------------------------------------------------- */

static const SwcHeroLimits HERO_LIMITS[SWC_HERO_TYPES] = {
    {27, 102, 16, 42, 6, 1.5, 128},  /* freighter */
    {42, 94, 32, 42, 4, 3.0, 96},    /* airship */
    {13, 32, 4.5, 8.5, 10, 0.8, 256} /* patrol */
};

const SwcHeroLimits *swc_air_hero_limits(int type) {
    return &HERO_LIMITS[type];
}

static const double PATROL_ALTITUDES[2] = {190.0, 235.0};
#define LANE_TUBE_HALF_HEIGHT 10.0
#define ROOF_CLEARANCE 28.0
#define TALL_LIMIT 560.0

typedef struct LineInfo {
    int axis;
    int line;
    double tallest; /* tallest cell on either side of the avenue, metres */
    double score;   /* the sum of those heights: how much city the route passes */
} LineInfo;

static double cell_height(const SwcCity *city, int cx, int cz) {
    return (double)city->cells[swc_cell_index(cx, cz)].height * 4.0;
}

/* Every avenue line of both axes with the roof heights beside it. A lane along x has z fixed. */
static int avenue_lines(const SwcCity *city, LineInfo *out) {
    int n = 0;
    for (int axis = 0; axis < 2; axis++) {
        for (int line = 0; line < SWC_CELLS; line++) {
            if (!swc_is_avenue(line))
                continue;
            double tallest = 0.0, score = 0.0;
            for (int i = 0; i < SWC_CELLS; i++) {
                double a = axis == 0 ? cell_height(city, i, line - 1) : cell_height(city, line - 1, i);
                double b = axis == 0 ? cell_height(city, i, line) : cell_height(city, line, i);
                double m = a > b ? a : b;
                if (m > tallest)
                    tallest = m;
                score += m;
            }
            out[n].axis = axis;
            out[n].line = line;
            out[n].tallest = tallest;
            out[n].score = score;
            n++;
        }
    }
    return n;
}

/* The order the web version sorts the lines of one axis in: no megatower beside the avenue first, then the
 * busiest, then the lower line number. */
static bool ranks_before(const LineInfo *a, const LineInfo *b) {
    int ta = a->tallest > TALL_LIMIT ? 1 : 0, tb = b->tallest > TALL_LIMIT ? 1 : 0;
    if (ta != tb)
        return ta < tb;
    if (a->score != b->score)
        return a->score > b->score;
    return a->line < b->line;
}

static int ranked(const LineInfo *lines, int n, int axis, LineInfo *out) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        if (lines[i].axis != axis)
            continue;
        int at = m++;
        while (at > 0 && ranks_before(&lines[i], &out[at - 1])) {
            out[at] = out[at - 1];
            at--;
        }
        out[at] = lines[i];
    }
    return m;
}

static double rank_value(const LineInfo *list, int n) {
    double v = 0.0;
    for (int i = 0; i < n && i < 4; i++)
        v += list[i].tallest > TALL_LIMIT ? -1e6 : list[i].score;
    return v;
}

static LineInfo nearest_line(const LineInfo *lines, int n, int axis, double coord) {
    LineInfo best = lines[0];
    double best_d = INFINITY;
    for (int i = 0; i < n; i++) {
        if (lines[i].axis != axis)
            continue;
        double d = fabs(swc_wrap_delta(lines[i].line * SWC_CELL, coord, SWC_TILE));
        if (d < best_d) {
            best_d = d;
            best = lines[i];
        }
    }
    return best;
}

static const SwcLandmark *find_landmark(const SwcCity *city, int kind) {
    for (int i = 0; i < city->landmark_count; i++)
        if (city->landmarks[i].kind == kind)
            return &city->landmarks[i];
    return NULL;
}

static double coord_on(const SwcLandmark *l, int axis) {
    return l ? (axis == 0 ? l->z : l->x) : SWC_TILE / 2.0;
}

static void push_hero(SwcHeroPack *out, uint32_t seed, int type, const LineInfo *info, double y, int dir,
                      double phase, int k) {
    float *r = &out->data[out->count * SWC_HERO_STRIDE];
    r[SWC_HERO_FIXED] = (float)(info->line * SWC_CELL);
    r[SWC_HERO_Y] = (float)y;
    r[SWC_HERO_AXIS] = (float)info->axis;
    r[SWC_HERO_DIR] = (float)dir;
    r[SWC_HERO_LAPS] = (float)HERO_LIMITS[type].laps;
    r[SWC_HERO_PHASE] = (float)phase;
    r[SWC_HERO_SEED] = (float)(floor(swc_hash_unit3(seed, 77, (uint32_t)k) * 4096.0) / 4096.0);
    r[SWC_HERO_TYPE] = (float)type;
    out->count++;
}

void swc_air_plan_heroes(const SwcCity *city, uint32_t seed, SwcHeroPack *out) {
    LineInfo lines[2 * SWC_CELLS];
    LineInfo rx[SWC_CELLS], rz[SWC_CELLS];
    int n = avenue_lines(city, lines);
    int nx = ranked(lines, n, 0, rx), nz = ranked(lines, n, 1, rz);
    out->count = 0;

    /* Wide ships: pick the axis whose four best lines carry more city, lowest ceilings first. */
    const LineInfo *wide = rank_value(rz, nz) >= rank_value(rx, nx) ? rz : rx;
    int wide_n = wide == rz ? nz : nx;
    static const int wide_types[4] = {SWC_HERO_FREIGHTER, SWC_HERO_AIRSHIP, SWC_HERO_FREIGHTER,
                                      SWC_HERO_AIRSHIP};
    double big_floor = 410.0 + LANE_TUBE_HALF_HEIGHT + 12.0; /* clear of the highest sky lane */
    for (int k = 0; k < 4; k++) {
        int type = wide_types[k];
        const SwcHeroLimits *lim = &HERO_LIMITS[type];
        const LineInfo *info = &wide[k % wide_n];
        /* Ships that have to share a line (fewer than four lines) differ in altitude band and phase. */
        int lap_on_line = k / wide_n;
        double clear = info->tallest + ROOF_CLEARANCE;
        double underside = (big_floor > clear ? big_floor : clear) + lap_on_line * 110.0;
        double y = ceil(underside + lim->below + lim->bob_amp);
        push_hero(out, seed, type, info, y, k % 2 == 0 ? 1 : -1, swc_hash_unit3(seed, 78, (uint32_t)k), k);
    }

    /* Patrol craft: inside the avenue canyon, on the axis the wide ships do not use where possible. */
    const SwcLandmark *downtown = find_landmark(city, SWC_LM_DOWNTOWN);
    const SwcLandmark *mega = find_landmark(city, SWC_LM_MEGATOWER);
    int wide_axis = wide[0].axis;
    int other_axis = wide_axis == 0 ? 1 : 0;
    LineInfo p0 = nearest_line(lines, n, other_axis, coord_on(downtown, other_axis));
    LineInfo p1 = nearest_line(lines, n, wide_axis, coord_on(mega, wide_axis) + SWC_CELL);
    if (p1.axis == p0.axis && p1.line == p0.line)
        p1 = nearest_line(lines, n, wide_axis, coord_on(mega, wide_axis) + SWC_TILE / 2.0);
    push_hero(out, seed, SWC_HERO_PATROL, &p0, PATROL_ALTITUDES[0], 1, swc_hash_unit3(seed, 78, 10), 10);
    push_hero(out, seed, SWC_HERO_PATROL, &p1, PATROL_ALTITUDES[1], -1, swc_hash_unit3(seed, 78, 11), 11);
}

void swc_air_hero_state(const float *data, int index, double loop, SwcHeroState *out) {
    const float *o = &data[index * SWC_HERO_STRIDE];
    const SwcHeroLimits *lim = &HERO_LIMITS[(int)o[SWC_HERO_TYPE]];
    double along =
        fract_d((double)o[SWC_HERO_DIR] * (double)o[SWC_HERO_LAPS] * loop + (double)o[SWC_HERO_PHASE]) *
        SWC_TILE;
    bool along_z = o[SWC_HERO_AXIS] > 0.5f;
    out->x = along_z ? (double)o[SWC_HERO_FIXED] : along;
    out->z = along_z ? along : (double)o[SWC_HERO_FIXED];
    out->y = (double)o[SWC_HERO_Y] +
             lim->bob_amp * sin(SWC_TAU * (lim->bob_cycles * loop + (double)o[SWC_HERO_SEED]));
    out->along = along;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Hull meshes: lofts of closed rings                                                                   */
/* ---------------------------------------------------------------------------------------------------- */

#define RING_MAX 16

typedef struct Ring {
    int n;
    double p[RING_MAX][2];
} Ring;

typedef struct Section {
    double z;
    Ring ring;
} Section;

/* All rings are counter-clockwise seen from +z, x to the right, y up. */
static Ring ring_hex(double hw, double yb, double ym, double yt, double kb, double kt) {
    Ring r = {6, {{hw * kb, yb}, {hw, ym}, {hw * kt, yt}, {-hw * kt, yt}, {-hw, ym}, {-hw * kb, yb}}};
    return r;
}
static Ring ring_rect(double x0, double x1, double y0, double y1) {
    Ring r = {4, {{x1, y0}, {x1, y1}, {x0, y1}, {x0, y0}}};
    return r;
}
/* Symmetric trapezoid: half-width w0 at y0 (bottom), w1 at y1 (top). */
static Ring ring_trap(double w0, double w1, double y0, double y1) {
    Ring r = {4, {{w0, y0}, {w1, y1}, {-w1, y1}, {-w0, y0}}};
    return r;
}
static Ring ring_diamond(double cx, double w, double h) {
    Ring r = {4, {{cx + w, 0.0}, {cx, h}, {cx - w, 0.0}, {cx, -h}}};
    return r;
}
static Ring ring_ellipse(double rx, double ry, int n) {
    Ring r;
    r.n = n;
    for (int i = 0; i < n; i++) {
        double a = ((i + 0.5) / n) * SWC_PI * 2.0 - SWC_PI / 2.0;
        r.p[i][0] = cos(a) * rx;
        r.p[i][1] = sin(a) * ry;
    }
    return r;
}
static Ring ring_point(double x, double y, int n) {
    Ring r;
    r.n = n;
    for (int i = 0; i < n; i++) {
        r.p[i][0] = x;
        r.p[i][1] = y;
    }
    return r;
}
/* Mirror in x and reverse so the ring stays counter-clockwise. */
static Ring ring_mirrored(const Ring *in) {
    Ring r;
    r.n = in->n;
    for (int i = 0; i < in->n; i++) {
        r.p[i][0] = -in->p[in->n - 1 - i][0];
        r.p[i][1] = in->p[in->n - 1 - i][1];
    }
    return r;
}

static double ring_area(const Ring *r) {
    double a = 0.0;
    for (int i = 0; i < r->n; i++) {
        const double *p = r->p[i];
        const double *q = r->p[(i + 1) % r->n];
        a += p[0] * q[1] - q[0] * p[1];
    }
    return a / 2.0;
}

typedef struct Builder {
    int vertex_count, index_count, piece_count;
    double pos[SWC_HULL_MAX_VERTICES * 3];
    float part[SWC_HULL_MAX_VERTICES];
    float type[SWC_HULL_MAX_VERTICES];
    uint16_t idx[SWC_HULL_MAX_INDICES];
    int piece_start[SWC_HULL_MAX_PIECES], piece_length[SWC_HULL_MAX_PIECES];
    bool piece_closed[SWC_HULL_MAX_PIECES];
    int current_type; /* hero type stamped on the vertices added from now on */
    bool overflow;
} Builder;

/* A triangle, unless it has no area (a ring collapsed to a point or a line). */
static void tri(Builder *b, int a, int c, int d) {
    const double *p = b->pos;
    double ux = p[c * 3] - p[a * 3], uy = p[c * 3 + 1] - p[a * 3 + 1], uz = p[c * 3 + 2] - p[a * 3 + 2];
    double vx = p[d * 3] - p[a * 3], vy = p[d * 3 + 1] - p[a * 3 + 1], vz = p[d * 3 + 2] - p[a * 3 + 2];
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    if (nx * nx + ny * ny + nz * nz < 1e-12)
        return;
    if (b->index_count + 3 > SWC_HULL_MAX_INDICES) {
        b->overflow = true;
        return;
    }
    b->idx[b->index_count++] = (uint16_t)a;
    b->idx[b->index_count++] = (uint16_t)c;
    b->idx[b->index_count++] = (uint16_t)d;
}

/* Join rings (same point count, increasing z) into a closed piece. The ends are capped unless told not to:
 * a fin whose root is buried in the body leaves one open on purpose. */
static void loft(Builder *b, int part, const Section *sections, int count, bool cap_start, bool cap_end) {
    int start = b->index_count;
    int n = sections[0].ring.n;
    int base = b->vertex_count;
    if (base + n * count > SWC_HULL_MAX_VERTICES || b->piece_count >= SWC_HULL_MAX_PIECES) {
        b->overflow = true;
        return;
    }
    for (int s = 0; s < count; s++)
        for (int j = 0; j < n; j++) {
            int v = b->vertex_count++;
            b->pos[v * 3] = sections[s].ring.p[j][0];
            b->pos[v * 3 + 1] = sections[s].ring.p[j][1];
            b->pos[v * 3 + 2] = sections[s].z;
            b->part[v] = (float)part;
            b->type[v] = (float)b->current_type;
        }
    for (int s = 0; s + 1 < count; s++)
        for (int j = 0; j < n; j++) {
            int a0 = base + s * n + j;
            int b0 = base + s * n + ((j + 1) % n);
            int a1 = a0 + n;
            int b1 = b0 + n;
            tri(b, a0, b0, a1);
            tri(b, b0, b1, a1);
        }
    int last = base + (count - 1) * n;
    if (cap_start)
        for (int j = 1; j + 1 < n; j++)
            tri(b, base, base + j + 1, base + j);
    if (cap_end)
        for (int j = 1; j + 1 < n; j++)
            tri(b, last, last + j, last + j + 1);
    bool closed = (cap_start || fabs(ring_area(&sections[0].ring)) < 1e-9) &&
                  (cap_end || fabs(ring_area(&sections[count - 1].ring)) < 1e-9);
    int p = b->piece_count++;
    b->piece_start[p] = start;
    b->piece_length[p] = b->index_count - start;
    b->piece_closed[p] = closed;
}

static void box(Builder *b, int part, double x0, double x1, double y0, double y1, double z0, double z1) {
    Section s[2] = {{z0, ring_rect(x0, x1, y0, y1)}, {z1, ring_rect(x0, x1, y0, y1)}};
    loft(b, part, s, 2, true, true);
}

/* The same loft on both sides of the centre line. */
static void loft_pair(Builder *b, int part, const Section *sections, int count, bool cap_start,
                      bool cap_end) {
    Section m[8];
    loft(b, part, sections, count, cap_start, cap_end);
    for (int i = 0; i < count; i++) {
        m[i].z = sections[i].z;
        m[i].ring = ring_mirrored(&sections[i].ring);
    }
    loft(b, part, m, count, cap_start, cap_end);
}

static void finish(Builder *b, SwcHull *out) {
    out->vertex_count = b->vertex_count;
    out->index_count = b->index_count;
    out->piece_count = b->piece_count;
    for (int i = 0; i < b->vertex_count; i++) {
        out->position[i * 3] = (float)b->pos[i * 3];
        out->position[i * 3 + 1] = (float)b->pos[i * 3 + 1];
        out->position[i * 3 + 2] = (float)b->pos[i * 3 + 2];
        out->part[i] = b->part[i];
        out->type[i] = b->type[i];
    }
    memcpy(out->index, b->idx, (size_t)b->index_count * sizeof(uint16_t));
    memcpy(out->piece_start, b->piece_start, sizeof(b->piece_start));
    memcpy(out->piece_length, b->piece_length, sizeof(b->piece_length));
    memcpy(out->piece_closed, b->piece_closed, sizeof(b->piece_closed));
}

/* The builder is large: one static copy, so the hull functions are not reentrant. */
static Builder g_builder;

static Builder *builder_begin(void) {
    memset(&g_builder, 0, sizeof(g_builder));
    return &g_builder;
}

/* ---- small craft: kept lean, several thousand of each are drawn, twice (scene and reflection) ---- */

static void build_car(Builder *b) {
    /* A low wedge: a knife-edge nose widening to the shoulders, then a short tail. */
    Section body[3] = {{-4.0, ring_hex(0.55, -0.16, -0.16, -0.16, 0.6, 0.55)},
                       {1.2, ring_hex(1.7, -0.52, -0.06, 0.42, 0.6, 0.55)},
                       {3.3, ring_hex(1.5, -0.36, 0.0, 0.34, 0.6, 0.55)}};
    loft(b, SWC_PART_BODY, body, 3, true, true);
    /* Canopy: rises out of the hood and is cut off square at the back. */
    Section canopy[2] = {{-1.9, ring_trap(0.5, 0.5, 0.05, 0.05)}, {1.1, ring_trap(0.9, 0.6, 0.36, 0.92)}};
    loft(b, SWC_PART_GLASS, canopy, 2, true, true);
    /* Two tail fins growing out of the shoulders. */
    Ring tip = {3, {{1.08, 0.3}, {1.36, 0.3}, {1.3, 1.26}}};
    Section fin[2] = {{1.5, ring_point(1.22, 0.36, 3)}, {3.6, tip}};
    loft_pair(b, SWC_PART_FIN, fin, 2, true, true);
}

static void build_hauler(Builder *b) {
    /* Cab with a raked windscreen; its back is hidden by the container. */
    Section cab[3] = {{-9.0, ring_trap(1.5, 1.25, -1.35, 0.1)},
                      {-6.6, ring_trap(2.05, 1.9, -1.65, 1.55)},
                      {-4.7, ring_trap(2.05, 1.9, -1.65, 1.55)}};
    loft(b, SWC_PART_BODY, cab, 3, true, false);
    box(b, SWC_PART_CARGO, -2.35, 2.35, -1.75, 2.15, -4.7, 8.4);
    /* Engine pods on the flanks at the back. */
    Section pods[2] = {{5.0, ring_rect(2.35, 3.55, -0.75, 0.75)}, {9.2, ring_rect(2.35, 3.55, -0.75, 0.75)}};
    loft_pair(b, SWC_PART_ENGINE, pods, 2, true, true);
}

static void build_drone(Builder *b) {
    Section body[3] = {{-1.6, ring_point(0.0, -0.05, 4)},
                       {-0.35, ring_diamond(0.0, 0.62, 0.42)},
                       {0.95, ring_diamond(0.0, 0.4, 0.3)}};
    loft(b, SWC_PART_BODY, body, 3, true, true);
    /* Two rotor pods: flat double wedges touching the body. */
    Section pods[3] = {{-0.75, ring_point(1.1, 0.0, 4)},
                       {0.05, ring_diamond(1.1, 0.6, 0.16)},
                       {0.85, ring_point(1.1, 0.0, 4)}};
    loft_pair(b, SWC_PART_ENGINE, pods, 3, true, true);
}

void swc_air_build_craft_hull(int shape, SwcHull *out) {
    Builder *b = builder_begin();
    if (shape == SWC_CRAFT_CAR)
        build_car(b);
    else if (shape == SWC_CRAFT_HAULER)
        build_hauler(b);
    else
        build_drone(b);
    finish(b, out);
}

/* ---- hero ships: the three types merged into one mesh, each vertex carrying its type ---- */

static void build_freighter(Builder *b) {
    b->current_type = SWC_HERO_FREIGHTER;
    Section hull[4] = {{-98.0, ring_hex(3.0, -5.0, -2.0, 1.5, 0.6, 0.9)},
                       {-72.0, ring_hex(17.0, -13.0, -3.0, 5.0, 0.6, 0.9)},
                       {62.0, ring_hex(20.0, -14.0, -3.0, 5.0, 0.6, 0.9)},
                       {96.0, ring_hex(15.0, -9.0, -2.0, 5.0, 0.6, 0.9)}};
    loft(b, SWC_PART_BODY, hull, 4, true, true);
    /* Container stacks on the deck. */
    static const double tops[6] = {15.0, 19.5, 13.0, 19.5, 16.0, 12.0};
    for (int i = 0; i < 6; i++) {
        double z0 = -64.0 + i * 17.5;
        box(b, SWC_PART_CARGO, -15.0, 15.0, 5.0, tops[i], z0, z0 + 16.0);
    }
    /* Superstructure and bridge. */
    box(b, SWC_PART_GLASS, -13.0, 13.0, 5.0, 22.0, 46.0, 72.0);
    box(b, SWC_PART_GLASS, -17.0, 17.0, 22.0, 27.5, 47.0, 60.0);
    box(b, SWC_PART_FIN, -0.8, 0.8, 22.0, 41.0, 62.0, 64.0);
    box(b, SWC_PART_FIN, -1.5, 1.5, -16.0, -13.0, -40.0, 50.0);
    /* Engine nacelles. */
    Section nacelle[2] = {{56.0, ring_rect(19.0, 27.0, -9.0, -1.0)},
                          {102.0, ring_rect(19.0, 27.0, -9.0, -1.0)}};
    loft_pair(b, SWC_PART_ENGINE, nacelle, 2, true, true);
}

static void build_airship(Builder *b) {
    b->current_type = SWC_HERO_AIRSHIP;
    static const double stations[9][2] = {{-92, 1.5}, {-84, 10}, {-66, 18.5}, {-38, 24.5}, {0, 26},
                                          {36, 24},   {62, 18},  {80, 10.5},  {92, 3}};
    Section env[9];
    for (int i = 0; i < 9; i++) {
        env[i].z = stations[i][0];
        env[i].ring = ring_ellipse(stations[i][1], stations[i][1] * 0.9, 14);
    }
    loft(b, SWC_PART_BODY, env, 9, true, true);
    box(b, SWC_PART_GLASS, -5.0, 5.0, -31.0, -22.5, -28.0, 14.0);
    /* Tail fins: top, bottom (shorter), and the two sides. */
    Section top[2] = {{56.0, ring_rect(-0.7, 0.7, 15.0, 17.0)}, {90.0, ring_rect(-0.7, 0.7, 3.0, 40.0)}};
    loft(b, SWC_PART_FIN, top, 2, true, true);
    Section bottom[2] = {{56.0, ring_rect(-0.7, 0.7, -17.0, -15.0)},
                         {90.0, ring_rect(-0.7, 0.7, -30.0, -3.0)}};
    loft(b, SWC_PART_FIN, bottom, 2, true, true);
    Section sides[2] = {{56.0, ring_rect(17.0, 19.0, -0.7, 0.7)}, {90.0, ring_rect(3.5, 40.0, -0.7, 0.7)}};
    loft_pair(b, SWC_PART_FIN, sides, 2, true, true);
    Section pods[2] = {{10.0, ring_rect(25.0, 31.5, -12.0, -6.0)},
                       {26.0, ring_rect(25.0, 31.5, -12.0, -6.0)}};
    loft_pair(b, SWC_PART_ENGINE, pods, 2, true, true);
}

static void build_patrol(Builder *b) {
    b->current_type = SWC_HERO_PATROL;
    Section hull[4] = {{-31.0, ring_hex(1.2, -0.9, -0.2, 0.5, 0.6, 0.55)},
                       {-13.0, ring_hex(5.5, -2.4, -0.3, 2.0, 0.6, 0.55)},
                       {12.0, ring_hex(7.5, -2.8, 0.0, 2.8, 0.6, 0.55)},
                       {29.0, ring_hex(6.0, -2.0, 0.0, 2.2, 0.6, 0.55)}};
    loft(b, SWC_PART_BODY, hull, 4, true, true);
    Section glass[3] = {{-23.0, ring_trap(1.8, 1.4, 0.9, 1.3)},
                        {-14.0, ring_trap(3.0, 2.2, 1.6, 3.6)},
                        {-4.0, ring_trap(2.6, 1.8, 2.0, 3.4)}};
    loft(b, SWC_PART_GLASS, glass, 3, true, true);
    /* Swept wings. */
    Section wings[3] = {{-6.0, ring_rect(5.3, 5.9, -0.5, 0.1)},
                        {18.0, ring_rect(6.5, 12.5, -0.5, 0.1)},
                        {27.0, ring_rect(6.0, 11.5, -0.4, 0.0)}};
    loft_pair(b, SWC_PART_FIN, wings, 3, true, true);
    /* Twin tail fins. */
    Section tails[2] = {{14.0, ring_rect(4.6, 5.2, 2.4, 2.9)}, {28.0, ring_rect(4.6, 5.2, 2.0, 8.0)}};
    loft_pair(b, SWC_PART_FIN, tails, 2, true, true);
    box(b, SWC_PART_LIGHTBAR, -4.2, 4.2, 2.3, 3.5, 0.0, 2.4);
    box(b, SWC_PART_LAMP, -1.3, 1.3, -4.2, -2.5, -9.0, -6.0);
    Section pods[2] = {{26.0, ring_rect(2.0, 5.0, -1.4, 1.4)}, {31.5, ring_rect(2.0, 5.0, -1.4, 1.4)}};
    loft_pair(b, SWC_PART_ENGINE, pods, 2, true, true);
}

void swc_air_build_hero_hull(SwcHull *out) {
    Builder *b = builder_begin();
    build_freighter(b);
    build_airship(b);
    build_patrol(b);
    finish(b, out);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Lights                                                                                               */
/* ---------------------------------------------------------------------------------------------------- */

static const float CRAFT_LIGHT_TABLE[SWC_CRAFT_LIGHT_FLOATS] = {
    /* car */
    -0.8f, 0.0f, 3.5f, 1.9f, 0.8f, 0.0f, 3.5f, 1.9f, 0.0f, -0.14f, -4.1f, 1.2f, 0.0f, 1.05f, 0.7f, 0.9f, 0.0f,
    -0.66f, 0.6f, 0.9f,
    /* hauler */
    -2.95f, 0.0f, 9.4f, 2.8f, 2.95f, 0.0f, 9.4f, 2.8f, 0.0f, -0.9f, -9.15f, 1.7f, 0.0f, 2.35f, -3.6f, 1.2f,
    0.0f, -1.95f, 2.0f, 1.2f,
    /* drone */
    -1.1f, 0.0f, 0.95f, 1.0f, 1.1f, 0.0f, 0.95f, 1.0f, 0.0f, -0.05f, -1.65f, 0.8f, 0.0f, 0.55f, 0.0f, 0.6f,
    0.0f, 0.0f, 0.0f, 0.0f};

static const float CRAFT_TRAIL_TABLE[SWC_CRAFT_TRAIL_FLOATS] = {
    /* y, z, half separation, line half-width */
    0.0f, 3.5f, 0.8f, 0.3f, 0.0f, 9.4f, 2.95f, 0.5f, 0.0f, 0.95f, 1.1f, 0.2f};

const float *swc_air_craft_light_table(void) {
    return CRAFT_LIGHT_TABLE;
}
const float *swc_air_craft_trail_table(void) {
    return CRAFT_TRAIL_TABLE;
}

static void add_light(SwcHeroLight *out, int *n, int type, double x, double y, double z, double size,
                      double color, int mode, double phase) {
    SwcHeroLight *l = &out[(*n)++];
    l->type = type;
    l->x = (float)x;
    l->y = (float)y;
    l->z = (float)z;
    l->size = (float)size;
    l->color = (float)color;
    l->mode = mode;
    l->phase = (float)phase;
}

int swc_air_hero_lights(SwcHeroLight *out) {
    int n = 0;
    /* ---- freighter: rows of lights down both gunwales and along the keel */
    const int F = SWC_HERO_FREIGHTER;
    for (int i = 0; i < 10; i++) {
        double z = -84.0 + i * 18.5;
        double hw = z < -72.0 ? 3.0 + ((z + 98.0) / 26.0) * 14.0 : 17.0 + ((z + 72.0) / 134.0) * 3.0;
        add_light(out, &n, F, -(hw + 0.8), -3.0, z, 3.2, 4.0, SWC_GLOW_STEADY, 0.0);
        add_light(out, &n, F, hw + 0.8, -3.0, z, 3.2, 4.0, SWC_GLOW_STEADY, 0.0);
    }
    for (int i = 0; i < 8; i++)
        add_light(out, &n, F, 0.0, -17.2, -38.0 + i * 12.4, 3.6, 1.0, SWC_GLOW_PULSE, i / 8.0);
    add_light(out, &n, F, -21.5, -3.0, -10.0, 5.0, 6.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, F, 21.5, -3.0, -10.0, 5.0, 5.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, F, 0.0, 2.5, -99.5, 7.0, 7.0, SWC_GLOW_STROBE, 0.0);
    add_light(out, &n, F, 0.0, 42.0, 63.0, 6.0, 6.0, SWC_GLOW_PULSE, 0.3);
    add_light(out, &n, F, 0.0, 7.0, 97.5, 6.0, 7.0, SWC_GLOW_STROBE, 0.5);
    add_light(out, &n, F, -17.8, 25.0, 46.5, 4.0, 7.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, F, 17.8, 25.0, 46.5, 4.0, 7.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, F, -23.0, -5.0, 103.5, 19.0, 3.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, F, 23.0, -5.0, 103.5, 19.0, 3.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, F, -23.0, -5.0, 102.5, 90.0, 3.0, SWC_GLOW_PLUME, 0.0);
    add_light(out, &n, F, 23.0, -5.0, 102.5, 90.0, 3.0, SWC_GLOW_PLUME, 0.0);
    /* ---- airship */
    const int A = SWC_HERO_AIRSHIP;
    add_light(out, &n, A, 0.0, 0.0, -94.0, 6.0, 7.0, SWC_GLOW_STROBE, 0.2);
    add_light(out, &n, A, 0.0, 0.5, 93.5, 6.0, 6.0, SWC_GLOW_PULSE, 0.0);
    add_light(out, &n, A, 0.0, 41.5, 90.0, 5.0, 6.0, SWC_GLOW_PULSE, 0.5);
    add_light(out, &n, A, 0.0, -31.5, 90.0, 5.0, 6.0, SWC_GLOW_PULSE, 0.5);
    add_light(out, &n, A, -41.5, 0.0, 90.0, 5.0, 7.0, SWC_GLOW_STROBE, 0.35);
    add_light(out, &n, A, 41.5, 0.0, 90.0, 5.0, 7.0, SWC_GLOW_STROBE, 0.85);
    for (int i = 0; i < 12; i++) {
        double a = (i / 12.0) * SWC_PI * 2.0;
        add_light(out, &n, A, sin(a) * 12.5, -23.6, -7.0 + cos(a) * 40.0, 3.4, 0.0, SWC_GLOW_PULSE, i / 12.0);
    }
    add_light(out, &n, A, 0.0, -32.2, -7.0, 7.0, 4.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, A, -28.2, -9.0, 27.0, 9.0, 2.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, A, 28.2, -9.0, 27.0, 9.0, 2.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, A, -28.2, -9.0, 26.5, 40.0, 2.0, SWC_GLOW_PLUME, 0.0);
    add_light(out, &n, A, 28.2, -9.0, 26.5, 40.0, 2.0, SWC_GLOW_PLUME, 0.0);
    /* ---- patrol craft */
    const int P = SWC_HERO_PATROL;
    add_light(out, &n, P, -3.3, 3.9, 1.2, 5.0, 6.0, SWC_GLOW_POLICE_A, 0.0);
    add_light(out, &n, P, -1.3, 3.9, 1.2, 5.0, 6.0, SWC_GLOW_POLICE_A, 0.0);
    add_light(out, &n, P, 1.3, 3.9, 1.2, 5.0, 1.62, SWC_GLOW_POLICE_B, 0.0);
    add_light(out, &n, P, 3.3, 3.9, 1.2, 5.0, 1.62, SWC_GLOW_POLICE_B, 0.0);
    add_light(out, &n, P, -1.0, -0.3, -31.4, 3.0, 7.0, SWC_GLOW_HEAD, 0.0);
    add_light(out, &n, P, 1.0, -0.3, -31.4, 3.0, 7.0, SWC_GLOW_HEAD, 0.0);
    add_light(out, &n, P, -12.9, -0.2, 21.0, 2.6, 6.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, P, 12.9, -0.2, 21.0, 2.6, 5.0, SWC_GLOW_STEADY, 0.0);
    add_light(out, &n, P, -3.5, 0.0, 32.0, 7.0, 1.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, P, 3.5, 0.0, 32.0, 7.0, 1.0, SWC_GLOW_ENGINE, 0.0);
    add_light(out, &n, P, 0.0, -3.1, 10.0, 3.0, 7.0, SWC_GLOW_STROBE, 0.6);
    /* The searchlight leaves the hull at (0, -4.4, -7.5): its lamp, its cone and its pool on the road. */
    add_light(out, &n, P, 0.0, -4.4, -7.5, 8.0, 7.0, SWC_GLOW_SEARCHLAMP, 0.0);
    add_light(out, &n, P, 0.0, -4.4, -7.5, 1.0, 7.0, SWC_GLOW_CONE, 0.0);
    add_light(out, &n, P, 0.0, -4.4, -7.5, 1.0, 7.0, SWC_GLOW_POOL, 0.0);
    return n;
}
