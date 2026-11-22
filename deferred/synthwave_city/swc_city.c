/* Synthwave City - the city plan (see swc_city.h). PURE C99, no raylib.
 *
 * This is a statement-by-statement port of src/city/{districts,massing,lanes,sites,queries}.ts of the web
 * version. Two things matter for "same seed => same city":
 *   1. The order of random draws. The web code calls rng.range(...) inside argument lists; C does not define
 *      the order in which arguments are evaluated, so every draw below is first stored in a local variable,
 *      in exactly the order the TypeScript evaluates them.
 *   2. Precision. Generation uses double, like JavaScript. The boxes are stored as float, like the web
 *      version's Float32Array. */
#include "swc_city.h"

#include <string.h>

/* =================================================================================================== */
/* Districts: a periodic skyline field over the cells, classified into districts                      */
/* =================================================================================================== */

static double lattice_hash(uint32_t seed, int i, int j, int n) {
    return swc_hash_unit3(seed, (uint32_t)(((i % n) + n) % n), (uint32_t)(((j % n) + n) % n));
}

/* Periodic value noise on a coarse lattice that divides SWC_CELLS (so the city wraps seamlessly). */
static double periodic_noise(uint32_t seed, double x, double z, int lattice) {
    int n = SWC_CELLS / lattice;
    double fx = x / (double)lattice, fz = z / (double)lattice;
    double ix = floor(fx), iz = floor(fz);
    double ux = swc_smoothstep(0.0, 1.0, fx - ix), uz = swc_smoothstep(0.0, 1.0, fz - iz);
    int i = (int)ix, j = (int)iz;
    double a = lattice_hash(seed, i, j, n), b = lattice_hash(seed, i + 1, j, n);
    double c = lattice_hash(seed, i, j + 1, n), d = lattice_hash(seed, i + 1, j + 1, n);
    return a + (b - a) * ux + (c - a) * uz + (a - b - c + d) * ux * uz;
}

/* Wrapped Chebyshev distance between two cells (in cells). */
static int cell_distance(int a, int b) {
    double dx = fabs(swc_wrap_delta((double)(a % SWC_CELLS), (double)(b % SWC_CELLS), (double)SWC_CELLS));
    double dz = fabs(swc_wrap_delta((double)(a / SWC_CELLS), (double)(b / SWC_CELLS), (double)SWC_CELLS));
    double m = dx > dz ? dx : dz;
    return (int)m;
}

/* Which neon colour a cell leans towards: a weighted pick, so magenta and cyan dominate. */
static int pick_palette(double u) {
    static const double weights[7] = {0.28, 0.28, 0.18, 0.12, 0.08, 0.04, 0.02};
    double acc = 0.0;
    for (int i = 0; i < 7; i++) {
        acc += weights[i];
        if (u < acc)
            return i;
    }
    return 0;
}

typedef struct Peak {
    double x, z, sigma, gain;
} Peak;

/* Insertion sort of the cell indices by hash key (stable, like the JavaScript sort the web version uses). */
static void sort_cells_by_key(int *order, const double *key, int n) {
    for (int i = 1; i < n; i++) {
        int cur = order[i];
        int j = i - 1;
        while (j >= 0 && key[order[j]] > key[cur]) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = cur;
    }
}

static bool try_plaza(SwcCity *c, int i) {
    int cx = i % SWC_CELLS, cz = i / SWC_CELLS;
    int d = c->cells[i].district;
    if (d == SWC_PLAZA || d == SWC_MEGA || i == c->peak_cell)
        return false;
    int around[4];
    around[0] = swc_cell_index(cx + 1, cz);
    around[1] = swc_cell_index(cx - 1, cz);
    around[2] = swc_cell_index(cx, cz + 1);
    around[3] = swc_cell_index(cx, cz - 1);
    for (int k = 0; k < 4; k++)
        if (c->cells[around[k]].district == SWC_PLAZA)
            return false; /* never two plazas side by side */
    if (c->plaza_count >= SWC_MAX_PLAZAS)
        return false;
    c->cells[i].district = SWC_PLAZA;
    c->plaza_cells[c->plaza_count++] = i;
    return true;
}

static void build_districts(SwcCity *c, SwcRng *rng) {
    const uint32_t seed = c->seed;

    /* Downtown peaks, kept apart. */
    Peak peaks[3];
    int peak_count = 0;
    for (int tries = 0; peak_count < 3 && tries < 200; tries++) {
        double x = swc_rng_range(rng, 0.0, (double)SWC_CELLS);
        double z = swc_rng_range(rng, 0.0, (double)SWC_CELLS);
        bool far_enough = true;
        for (int p = 0; p < peak_count; p++) {
            double dx = swc_wrap_delta(x, peaks[p].x, (double)SWC_CELLS);
            double dz = swc_wrap_delta(z, peaks[p].z, (double)SWC_CELLS);
            if (!(hypot(dx, dz) > 13.0))
                far_enough = false;
        }
        if (far_enough) {
            double sigma = swc_rng_range(rng, 3.2, 4.6);
            double gain = peak_count == 0 ? 1.0 : swc_rng_range(rng, 0.78, 0.95);
            peaks[peak_count].x = x;
            peaks[peak_count].z = z;
            peaks[peak_count].sigma = sigma;
            peaks[peak_count].gain = gain;
            peak_count++;
        }
    }

    /* The skyline field: gaussian hills around the peaks, plus low-frequency noise and a little grain. */
    int peak_cell = 0;
    for (int cz = 0; cz < SWC_CELLS; cz++) {
        for (int cx = 0; cx < SWC_CELLS; cx++) {
            double f = 0.0;
            for (int p = 0; p < peak_count; p++) {
                double dx = swc_wrap_delta(cx + 0.5, peaks[p].x, (double)SWC_CELLS);
                double dz = swc_wrap_delta(cz + 0.5, peaks[p].z, (double)SWC_CELLS);
                double hill =
                    peaks[p].gain * exp(-(dx * dx + dz * dz) / (2.0 * peaks[p].sigma * peaks[p].sigma));
                if (hill > f)
                    f = hill;
            }
            f = f * 0.82 + 0.3 * periodic_noise(seed ^ 0x51u, cx + 0.5, cz + 0.5, 5) * (0.35 + f) +
                0.06 * swc_hash_unit4(seed, (uint32_t)cx, (uint32_t)cz, 11u);
            int i = swc_cell_index(cx, cz);
            c->field[i] = (float)(f < 1.0 ? f : 1.0);
            if (c->field[i] > c->field[peak_cell])
                peak_cell = i;
        }
    }
    c->peak_cell = peak_cell;

    for (int i = 0; i < SWC_CELL_COUNT; i++) {
        double f = (double)c->field[i];
        c->cells[i].district = (uint8_t)(f > 0.6 ? SWC_DOWNTOWN : (f > 0.3 ? SWC_MIDTOWN : SWC_LOWRISE));
    }

    /* Megatowers: on the shoulders of the skyline, far apart, never the very peak. */
    int order[SWC_CELL_COUNT];
    double key[SWC_CELL_COUNT];
    for (int i = 0; i < SWC_CELL_COUNT; i++) {
        order[i] = i;
        key[i] = swc_hash_unit3(seed, (uint32_t)i, 21u);
    }
    sort_cells_by_key(order, key, SWC_CELL_COUNT);

    c->mega_count = 0;
    for (int k = 0; k < SWC_CELL_COUNT; k++) {
        if (c->mega_count >= 12)
            break;
        int i = order[k];
        double f = (double)c->field[i];
        if (f < 0.18 || f > 0.85 || i == c->peak_cell)
            continue;
        bool apart = true;
        for (int m = 0; m < c->mega_count; m++)
            if (cell_distance(i, c->mega_cells[m]) < 7)
                apart = false;
        if (apart)
            c->mega_cells[c->mega_count++] = i;
    }
    for (int m = 0; m < c->mega_count; m++)
        c->cells[c->mega_cells[m]].district = SWC_MEGA;

    /* Plazas: one beside every megatower, plus a scatter; never two side by side. */
    c->plaza_count = 0;
    for (int m = 0; m < c->mega_count; m++) {
        int mc = c->mega_cells[m];
        int cx = mc % SWC_CELLS, cz = mc / SWC_CELLS;
        int options[4];
        options[0] = swc_cell_index(cx + 1, cz);
        options[1] = swc_cell_index(cx - 1, cz);
        options[2] = swc_cell_index(cx, cz + 1);
        options[3] = swc_cell_index(cx, cz - 1);
        int start = (int)floor(swc_hash_unit3(seed, (uint32_t)mc, 31u) * 4.0);
        for (int k = 0; k < 4; k++)
            if (try_plaza(c, options[(start + k) % 4]))
                break;
    }
    for (int k = 0; k < SWC_CELL_COUNT; k++) {
        if (c->plaza_count >= 80)
            break;
        int i = order[k];
        if (swc_hash_unit3(seed, (uint32_t)i, 41u) < 0.06)
            try_plaza(c, i);
    }

    /* Colour neighbourhoods: a slow noise picks each cell's main neon colour. */
    for (int cz = 0; cz < SWC_CELLS; cz++) {
        for (int cx = 0; cx < SWC_CELLS; cx++) {
            int i = swc_cell_index(cx, cz);
            double u = fmod(periodic_noise(seed ^ 0x77u, cx + 0.5, cz + 0.5, 4) * 0.75 +
                                swc_hash_unit4(seed, (uint32_t)cx, (uint32_t)cz, 51u) * 0.25,
                            1.0);
            c->cells[i].palette = (uint8_t)pick_palette(u);
            c->cells[i].seed = (uint8_t)floor(swc_hash_unit4(seed, (uint32_t)cx, (uint32_t)cz, 61u) * 256.0);
        }
    }
}

/* =================================================================================================== */
/* Massing: every block is cut into lots and every lot gets a stack of boxes                          */
/* =================================================================================================== */

SwcRect swc_block_rect(int cx, int cz) {
    SwcRect r;
    r.x0 = cx * SWC_CELL + swc_road_spec(cx)->corridor_half;
    r.x1 = (cx + 1) * SWC_CELL - swc_road_spec(cx + 1)->corridor_half;
    r.z0 = cz * SWC_CELL + swc_road_spec(cz)->corridor_half;
    r.z1 = (cz + 1) * SWC_CELL - swc_road_spec(cz + 1)->corridor_half;
    return r;
}

typedef struct Mass {
    SwcCity *c;
    SwcRng *rng;
    double cell_top; /* tallest point pushed in the current cell */
} Mass;

/* Adds one box. The box's own random number is drawn here, AFTER the caller evaluated its arguments. */
static void push_box(Mass *m, double x, double y0, double z, double sx, double sy, double sz, int style,
                     int color_a, int color_b, int flags, int building) {
    SwcCity *c = m->c;
    double seed = swc_rng_next(m->rng);
    if (c->box_count < SWC_MAX_BOXES) {
        SwcBox *b = &c->boxes[c->box_count++];
        b->cx = (float)x;
        b->y0 = (float)y0;
        b->cz = (float)z;
        b->sx = (float)sx;
        b->sy = (float)sy;
        b->sz = (float)sz;
        b->style = (uint8_t)style;
        b->color_a = (uint8_t)color_a;
        b->color_b = (uint8_t)color_b;
        b->seed = (float)seed;
        b->flags = (uint8_t)flags;
        b->building = building;
    }
    if (y0 + sy > m->cell_top)
        m->cell_top = y0 + sy;
}

/* Two neon colours for a building: mostly the neighbourhood's, sometimes any of the first five. */
static void pick_colours(SwcRng *rng, int cell_palette, int *a, int *b) {
    int first = swc_rng_chance(rng, 0.65) ? cell_palette : swc_rng_int(rng, 0, 4);
    int second = swc_rng_int(rng, 0, 5);
    if (second == first)
        second = (second + 1) % 5;
    *a = first;
    *b = second;
}

/* Cuts a length into n pieces of uneven size with a fixed gap between them. */
static void split_length(double length, int n, double gap, SwcRng *rng, double *start, double *size) {
    double weights[4];
    double total = 0.0;
    for (int i = 0; i < n; i++) {
        weights[i] = swc_rng_range(rng, 0.8, 1.25);
        total += weights[i];
    }
    double usable = length - gap * (double)(n - 1);
    double at = 0.0;
    for (int i = 0; i < n; i++) {
        double sz = (usable * weights[i]) / total;
        start[i] = at;
        size[i] = sz;
        at += sz + gap;
    }
}

/* Appends a building record. Returns it so the caller can fill in what only it knows. */
static SwcBuilding *begin_building(SwcCity *c, int cell, int district) {
    static SwcBuilding scratch; /* receives the data of a building that no longer fits (never in practice) */
    if (c->building_count >= SWC_MAX_BUILDINGS)
        return &scratch;
    SwcBuilding *b = &c->buildings[c->building_count++];
    memset(b, 0, sizeof(*b));
    b->id = c->building_count - 1;
    b->cell = cell;
    b->district = (uint8_t)district;
    b->mega_slot = -1;
    return b;
}

static const int SHAFT_STYLES[4] = {SWC_STYLE_SHAFT, SWC_STYLE_GLASS, SWC_STYLE_SLAB, SWC_STYLE_STRIP};
static const int LOW_STYLES[4] = {SWC_STYLE_PODIUM, SWC_STYLE_SLAB, SWC_STYLE_SHAFT, SWC_STYLE_PODIUM};

/* An open square, sometimes with kiosks in the corners. The centre always stays clear. */
static void mass_plaza(Mass *m, int cell, const SwcRect *rect, int cell_palette) {
    SwcRng *rng = m->rng;
    double bw = rect->x1 - rect->x0, bd = rect->z1 - rect->z0;
    if (!swc_rng_chance(rng, 0.6))
        return;
    int a, b;
    pick_colours(rng, cell_palette, &a, &b);
    for (int sxi = 0; sxi < 2; sxi++) {
        for (int szi = 0; szi < 2; szi++) {
            double sxn = sxi == 0 ? -1.0 : 1.0, szn = szi == 0 ? -1.0 : 1.0;
            if (swc_rng_chance(rng, 0.3))
                continue;
            double size = swc_rng_range(rng, 8.0, 14.0);
            double inset_x = swc_rng_range(rng, 3.0, 9.0);
            double x = (rect->x0 + rect->x1) / 2.0 + sxn * (bw / 2.0 - size / 2.0 - inset_x);
            double inset_z = swc_rng_range(rng, 3.0, 9.0);
            double z = (rect->z0 + rect->z1) / 2.0 + szn * (bd / 2.0 - size / 2.0 - inset_z);
            double h = swc_rng_range(rng, 4.0, 8.0);
            SwcBuilding *bl = begin_building(m->c, cell, SWC_PLAZA);
            push_box(m, x, 0.0, z, size, h, size, SWC_STYLE_KIOSK, a, b, SWC_FLAG_ROOF | SWC_FLAG_STREET,
                     bl->id);
            bl->x = x;
            bl->z = z;
            bl->base_x = size;
            bl->base_z = size;
            bl->top_x = size;
            bl->top_z = size;
            bl->top = h;
            bl->color_a = (uint8_t)a;
            bl->color_b = (uint8_t)b;
        }
    }
}

/* One megatower filling the block, stepping in as it rises, with a lit crown and a spire. */
static void mass_mega(Mass *m, int cell, const SwcRect *rect, int cell_palette) {
    SwcRng *rng = m->rng;
    SwcCity *c = m->c;
    double bw = rect->x1 - rect->x0, bd = rect->z1 - rect->z0;
    SwcBuilding *bl = begin_building(c, cell, SWC_MEGA);
    int id = bl->id;
    int a, b;
    pick_colours(rng, cell_palette, &a, &b);
    double x = (rect->x0 + rect->x1) / 2.0, z = (rect->z0 + rect->z1) / 2.0;
    double total = swc_rng_range(rng, 600.0, 900.0);
    int n = swc_rng_int(rng, 5, 7);
    double sx = bw - 8.0, sz = bd - 8.0;
    double base_x = sx, base_z = sz;
    double y = 0.0;
    int slot = c->mega_slots_used < SWC_MAX_MEGA ? c->mega_slots_used++ : -1;
    /* Segment heights fall off geometrically. */
    const double ratio = 0.8;
    double first = (total * (1.0 - ratio)) / (1.0 - pow(ratio, (double)n));
    for (int s = 0; s < n; s++) {
        double h = first * pow(ratio, (double)s);
        int flags = SWC_FLAG_LANDMARK | (s == 0 ? SWC_FLAG_STREET : 0);
        push_box(m, x, y, z, sx, h, sz, SWC_STYLE_MEGA, a, b, flags, id);
        if (slot >= 0 && s < SWC_MAX_SEGMENTS) {
            c->segments[slot][s].y0 = y;
            c->segments[slot][s].h = h;
            c->segments[slot][s].sx = sx;
            c->segments[slot][s].sz = sz;
        }
        y += h;
        double kx = swc_rng_range(rng, 0.78, 0.88);
        sx *= kx;
        double kz = swc_rng_range(rng, 0.78, 0.88);
        sz *= kz;
    }
    double crown_h = swc_rng_range(rng, 10.0, 18.0);
    push_box(m, x, y, z, sx, crown_h, sz, SWC_STYLE_CROWN, a, b, SWC_FLAG_LANDMARK | SWC_FLAG_ROOF, id);
    double top = y + crown_h;
    double spire_h = swc_rng_range(rng, 90.0, 160.0);
    push_box(m, x, top, z, 6.0, spire_h, 6.0, SWC_STYLE_SPIRE, a, b, SWC_FLAG_LANDMARK, id);
    bl->x = x;
    bl->z = z;
    bl->base_x = base_x;
    bl->base_z = base_z;
    bl->top_x = sx;
    bl->top_z = sz;
    bl->top = top;
    bl->spire_top = top + spire_h;
    bl->has_crown = 1;
    bl->color_a = (uint8_t)a;
    bl->color_b = (uint8_t)b;
    bl->mega_slot = slot;
    bl->segment_count = n < SWC_MAX_SEGMENTS ? n : SWC_MAX_SEGMENTS;
}

/* A low-rise lot: one block, or a podium with a narrower block on top. */
static double lot_lowrise(Mass *m, double x, double z, double *w, double *d, int a, int b, int id) {
    SwcRng *rng = m->rng;
    double h = 12.0 + pow(swc_rng_next(rng), 1.6) * 50.0;
    if (swc_rng_chance(rng, 0.7)) {
        int style = LOW_STYLES[swc_rng_pick(rng, 4)];
        push_box(m, x, 0.0, z, *w, h, *d, style, a, b, SWC_FLAG_ROOF | SWC_FLAG_STREET, id);
    } else {
        double base_h = h * swc_rng_range(rng, 0.35, 0.55);
        push_box(m, x, 0.0, z, *w, base_h, *d, SWC_STYLE_PODIUM, a, b, SWC_FLAG_STREET, id);
        double kw = swc_rng_range(rng, 0.6, 0.85);
        *w *= kw;
        double kd = swc_rng_range(rng, 0.6, 0.85);
        *d *= kd;
        int style = LOW_STYLES[swc_rng_pick(rng, 4)];
        push_box(m, x, base_h, z, *w, h - base_h, *d, style, a, b, SWC_FLAG_ROOF, id);
    }
    return h;
}

/* A midtown or downtown lot: optional podium, setback tower, optional crown and spire. Fills `bl`. */
static double lot_tower(Mass *m, SwcBuilding *bl, double f, bool downtown, double x, double z, double *w,
                        double *d, int a, int b, int id) {
    SwcRng *rng = m->rng;
    double fn = downtown ? swc_clamp((f - 0.6) / 0.4, 0.0, 1.0) : swc_clamp((f - 0.3) / 0.3, 0.0, 1.0);
    double h;
    if (downtown) {
        double r = pow(swc_rng_next(rng), 1.2);
        h = 170.0 + (0.5 * fn + 0.5 * r) * 320.0;
    } else {
        double r = pow(swc_rng_next(rng), 1.3);
        h = 55.0 + (0.35 * fn + 0.65 * r) * 150.0;
    }
    int style = SHAFT_STYLES[swc_rng_pick(rng, 4)];
    bool round = swc_rng_chance(rng, downtown ? 0.12 : 0.06);
    int street = SWC_FLAG_STREET;
    double y = 0.0;
    if (swc_rng_chance(rng, downtown ? 0.7 : 0.6)) {
        double pod_h = downtown ? swc_rng_range(rng, 12.0, 24.0) : swc_rng_range(rng, 8.0, 18.0);
        push_box(m, x, 0.0, z, *w, pod_h, *d, SWC_STYLE_PODIUM, a, b, SWC_FLAG_STREET, id);
        y = pod_h;
        street = 0;
        double inset = downtown ? swc_rng_range(rng, 4.0, 10.0) : swc_rng_range(rng, 3.0, 8.0);
        *w = fmax(10.0, *w - inset * 2.0);
        *d = fmax(10.0, *d - inset * 2.0);
    }
    if (round) {
        *w = fmin(*w, *d);
        *d = *w;
    }
    int round_flag = round ? SWC_FLAG_ROUND : 0;
    bool crown = swc_rng_chance(rng, downtown ? 0.7 : 0.35);
    double crown_h = crown ? swc_rng_range(rng, 4.0, 8.0) : 0.0;
    int setbacks;
    if (downtown)
        setbacks = swc_rng_int(rng, 0, 3);
    else
        setbacks = swc_rng_chance(rng, 0.3) ? 1 : 0;
    double remaining = h - y - crown_h;
    for (int s = 0; s <= setbacks; s++) {
        bool last = s == setbacks;
        double part = remaining;
        if (!last)
            part = remaining * swc_rng_range(rng, 0.45, 0.65);
        int flags = round_flag | (s == 0 ? street : 0) | (last && !crown ? SWC_FLAG_ROOF : 0);
        push_box(m, x, y, z, *w, part, *d, style, a, b, flags, id);
        y += part;
        remaining -= part;
        if (!last) {
            double k = swc_rng_range(rng, 0.74, 0.88);
            *w = fmax(8.0, *w * k);
            *d = fmax(8.0, *d * k);
        }
    }
    bl->has_crown = 0;
    if (crown) {
        double cw = fmax(6.0, *w - 3.0), cd = fmax(6.0, *d - 3.0);
        push_box(m, x, y, z, cw, crown_h, cd, SWC_STYLE_CROWN, a, b, round_flag | SWC_FLAG_ROOF, id);
        y += crown_h;
        bl->has_crown = 1;
        *w = cw;
        *d = cd;
    }
    bl->spire_top = 0.0;
    if (swc_rng_chance(rng, downtown ? 0.3 : 0.1)) {
        double sw = swc_rng_range(rng, 2.5, 4.0);
        double sh = swc_rng_range(rng, 30.0, 90.0);
        push_box(m, x, y, z, sw, sh, sw, SWC_STYLE_SPIRE, a, b, 0, id);
        bl->spire_top = y + sh;
    }
    bl->round = round ? 1 : 0;
    return y;
}

/* Cuts a low/mid/downtown block into lots and gives each lot a stack of boxes. */
static void mass_lots(Mass *m, int cell, int district, double f, int cell_palette, const SwcRect *rect) {
    SwcRng *rng = m->rng;
    double bw = rect->x1 - rect->x0, bd = rect->z1 - rect->z0;
    int nx, nz;
    double gap;
    if (district == SWC_LOWRISE) {
        nx = swc_rng_int(rng, 2, 4);
        nz = swc_rng_int(rng, 2, 4);
        gap = swc_rng_range(rng, 4.0, 8.0);
    } else if (district == SWC_MIDTOWN) {
        nx = swc_rng_int(rng, 1, 3);
        nz = swc_rng_int(rng, 1, 3);
        gap = swc_rng_range(rng, 5.0, 10.0);
        if (nx == 1 && nz == 1)
            nx = 2;
    } else {
        double u = swc_rng_next(rng);
        nx = u < 0.2 ? 1 : 2;
        nz = u < 0.2 ? 1 : (u < 0.6 ? 1 : 2);
        if (u >= 0.2 && u < 0.6 && swc_rng_chance(rng, 0.5)) {
            nx = 1;
            nz = 2;
        }
        gap = swc_rng_range(rng, 8.0, 14.0);
    }
    double xs[4], xw[4], zs[4], zd[4];
    split_length(bw, nx, gap, rng, xs, xw);
    split_length(bd, nz, gap, rng, zs, zd);

    for (int ix = 0; ix < nx; ix++) {
        for (int iz = 0; iz < nz; iz++) {
            double lx = xs[ix], lw = xw[ix], lz = zs[iz], ld = zd[iz];
            if (district == SWC_LOWRISE && swc_rng_chance(rng, 0.04))
                continue; /* a gap in the street wall */
            double margin = swc_rng_range(rng, 0.5, 3.0);
            double w = lw - margin * 2.0, d = ld - margin * 2.0;
            if (w < 10.0 || d < 10.0)
                continue;
            double x = rect->x0 + lx + lw / 2.0, z = rect->z0 + lz + ld / 2.0;
            SwcBuilding *bl = begin_building(m->c, cell, district);
            int id = bl->id;
            int a, b;
            pick_colours(rng, cell_palette, &a, &b);
            double base_x = w, base_z = d;
            double top;
            if (district == SWC_LOWRISE) {
                top = lot_lowrise(m, x, z, &w, &d, a, b, id);
            } else {
                top = lot_tower(m, bl, f, district == SWC_DOWNTOWN, x, z, &w, &d, a, b, id);
            }
            bl->x = x;
            bl->z = z;
            bl->base_x = base_x;
            bl->base_z = base_z;
            bl->top_x = w;
            bl->top_z = d;
            bl->top = top;
            bl->color_a = (uint8_t)a;
            bl->color_b = (uint8_t)b;
        }
    }
}

static void build_massing(SwcCity *c, SwcRng *rng) {
    Mass m;
    m.c = c;
    m.rng = rng;
    for (int cz = 0; cz < SWC_CELLS; cz++) {
        for (int cx = 0; cx < SWC_CELLS; cx++) {
            int cell = cz * SWC_CELLS + cx;
            int district = c->cells[cell].district;
            double f = (double)c->field[cell];
            int cell_palette = c->cells[cell].palette;
            SwcRect rect = swc_block_rect(cx, cz);
            m.cell_top = 0.0;
            if (district == SWC_PLAZA)
                mass_plaza(&m, cell, &rect, cell_palette);
            else if (district == SWC_MEGA)
                mass_mega(&m, cell, &rect, cell_palette);
            else
                mass_lots(&m, cell, district, f, cell_palette, &rect);
            double units = floor(m.cell_top / 4.0 + 0.5);
            c->cells[cell].height = (uint8_t)(units > 255.0 ? 255.0 : units);
        }
    }
}

/* =================================================================================================== */
/* Spatial queries over the boxes. Every box lies inside one block, so a point needs only its cell.   */
/* =================================================================================================== */

/* Counting sort of the boxes by cell: start[] holds the running totals. */
static void build_box_index(SwcCity *c) {
    static int32_t cell_of_box[SWC_MAX_BOXES];
    memset(c->box_start, 0, sizeof(c->box_start));
    for (int b = 0; b < c->box_count; b++) {
        const SwcBox *box = &c->boxes[b];
        int cx = (int)floor(swc_wrap((double)box->cx, SWC_TILE) / SWC_CELL);
        int cz = (int)floor(swc_wrap((double)box->cz, SWC_TILE) / SWC_CELL);
        cell_of_box[b] = cz * SWC_CELLS + cx;
        c->box_start[cell_of_box[b] + 1]++;
    }
    for (int i = 0; i < SWC_CELL_COUNT; i++)
        c->box_start[i + 1] += c->box_start[i];
    int32_t fill[SWC_CELL_COUNT];
    for (int i = 0; i < SWC_CELL_COUNT; i++)
        fill[i] = c->box_start[i];
    for (int b = 0; b < c->box_count; b++)
        c->box_list[fill[cell_of_box[b]]++] = b;
}

double swc_city_height_at(const SwcCity *c, double x, double z) {
    double px = swc_wrap(x, SWC_TILE), pz = swc_wrap(z, SWC_TILE);
    int cell = (int)floor(pz / SWC_CELL) * SWC_CELLS + (int)floor(px / SWC_CELL);
    double top = 0.0;
    for (int k = c->box_start[cell]; k < c->box_start[cell + 1]; k++) {
        const SwcBox *box = &c->boxes[c->box_list[k]];
        double dx = px - (double)box->cx, dz = pz - (double)box->cz;
        double hx = (double)box->sx / 2.0, hz = (double)box->sz / 2.0;
        bool inside;
        if (box->flags & SWC_FLAG_ROUND) {
            double rad = fmin(hx, hz);
            inside = dx * dx + dz * dz <= rad * rad;
        } else {
            inside = fabs(dx) <= hx && fabs(dz) <= hz;
        }
        if (inside) {
            double t = (double)box->y0 + (double)box->sy;
            if (t > top)
                top = t;
        }
    }
    return top;
}

bool swc_city_volume_blocked(const SwcCity *c, double x, double y, double z, double hx, double hy,
                             double hz) {
    double px = swc_wrap(x, SWC_TILE), pz = swc_wrap(z, SWC_TILE);
    /* Check the volume's own cell and any neighbour it reaches into. */
    int c0x = (int)floor((px - hx) / SWC_CELL), c1x = (int)floor((px + hx) / SWC_CELL);
    int c0z = (int)floor((pz - hz) / SWC_CELL), c1z = (int)floor((pz + hz) / SWC_CELL);
    for (int cz = c0z; cz <= c1z; cz++) {
        for (int cx = c0x; cx <= c1x; cx++) {
            int cell = ((cz + SWC_CELLS) % SWC_CELLS) * SWC_CELLS + ((cx + SWC_CELLS) % SWC_CELLS);
            /* Shift the query into that cell's copy of the tile. */
            double qx = px - floor((double)cx / SWC_CELLS) * SWC_TILE;
            double qz = pz - floor((double)cz / SWC_CELLS) * SWC_TILE;
            for (int k = c->box_start[cell]; k < c->box_start[cell + 1]; k++) {
                const SwcBox *box = &c->boxes[c->box_list[k]];
                double bsx = (double)box->sx, bsy = (double)box->sy, bsz = (double)box->sz;
                if (fabs(qx - (double)box->cx) > hx + bsx / 2.0)
                    continue;
                if (fabs(qz - (double)box->cz) > hz + bsz / 2.0)
                    continue;
                double by = (double)box->y0 + bsy / 2.0;
                if (fabs(y - by) > hy + bsy / 2.0)
                    continue;
                return true;
            }
        }
    }
    return false;
}

#define MAX_COLLIDE_RADIUS 11.0 /* STREET corridor half-width (12 m) minus 1 */

bool swc_city_collide(const SwcCity *c, SwcVec3 *pos, double radius) {
    double r = radius < MAX_COLLIDE_RADIUS ? radius : MAX_COLLIDE_RADIUS;
    bool moved = false;
    if (pos->y < r) {
        pos->y = r;
        moved = true;
    }
    double px = swc_wrap(pos->x, SWC_TILE), pz = swc_wrap(pos->z, SWC_TILE);
    int cell = (int)floor(pz / SWC_CELL) * SWC_CELLS + (int)floor(px / SWC_CELL);
    double x = px, y = pos->y, z = pz;
    for (int k = c->box_start[cell]; k < c->box_start[cell + 1]; k++) {
        const SwcBox *box = &c->boxes[c->box_list[k]];
        double bx = (double)box->cx, bz = (double)box->cz;
        double hx = (double)box->sx / 2.0, hz = (double)box->sz / 2.0;
        double by0 = (double)box->y0, by1 = by0 + (double)box->sy;
        if (y - r >= by1 || y + r <= by0)
            continue;
        if (box->flags & SWC_FLAG_ROUND) {
            /* A vertical cylinder inscribed in the box. */
            double rad = fmin(hx, hz);
            double dx = x - bx, dz = z - bz;
            double d = hypot(dx, dz);
            if (d >= rad + r)
                continue;
            if (y >= by1) {
                /* Over the top face: only the cap can touch. */
                if (d <= rad) {
                    y = by1 + r;
                    moved = true;
                }
                continue;
            }
            double over_top = by1 - y;
            double out_side = rad + r - d;
            if (y > by0 && over_top + r < out_side) {
                y = by1 + r;
            } else if (d > 1e-6) {
                x = bx + (dx / d) * (rad + r);
                z = bz + (dz / d) * (rad + r);
            } else {
                x = bx + rad + r;
            }
            moved = true;
            continue;
        }
        /* Closest point on the box to the sphere centre. */
        double qx = fmax(bx - hx, fmin(x, bx + hx));
        double qy = fmax(by0, fmin(y, by1));
        double qz = fmax(bz - hz, fmin(z, bz + hz));
        double dx = x - qx, dy = y - qy, dz = z - qz;
        double d2 = dx * dx + dy * dy + dz * dz;
        if (d2 >= r * r)
            continue;
        if (d2 > 1e-10) {
            double d = sqrt(d2);
            double push = (r - d) / d;
            x += dx * push;
            y += dy * push;
            z += dz * push;
        } else {
            /* Centre is inside: leave through the nearest face (never downwards). */
            double to_xn = x - (bx - hx), to_xp = bx + hx - x, to_zn = z - (bz - hz), to_zp = bz + hz - z;
            double to_top = by1 - y;
            double mn = fmin(fmin(fmin(to_xn, to_xp), fmin(to_zn, to_zp)), to_top);
            if (mn == to_top)
                y = by1 + r;
            else if (mn == to_xn)
                x = bx - hx - r;
            else if (mn == to_xp)
                x = bx + hx + r;
            else if (mn == to_zn)
                z = bz - hz - r;
            else
                z = bz + hz + r;
        }
        moved = true;
    }
    if (moved) {
        pos->x += x - px;
        pos->y = y;
        pos->z += z - pz;
    }
    return moved;
}

/* =================================================================================================== */
/* Traffic lanes                                                                                      */
/* =================================================================================================== */

/* Traffic keeps right. With y up, the right of +x travel is +z, and the right of +z travel is -x.
 * Returns the fixed coordinate of a lane `offset` metres to the right of the centre line. */
double swc_lane_coordinate(int axis, int line, int dir, double offset) {
    double side = axis == 0 ? (double)dir : (double)-dir;
    return swc_wrap((double)line * SWC_CELL + side * offset, SWC_TILE);
}

static void add_lane(SwcCity *c, int kind, int axis, double fixed, double y, int dir, int line, int slot) {
    if (c->lane_count >= SWC_MAX_LANES)
        return;
    SwcLane *l = &c->lanes[c->lane_count++];
    l->kind = (uint8_t)kind;
    l->axis = (uint8_t)axis;
    l->fixed = fixed;
    l->y = y;
    l->dir = (int8_t)dir;
    l->line = line;
    l->slot = slot;
}

static void build_lanes(SwcCity *c) {
    static const int dirs[2] = {1, -1};
    c->lane_count = 0;
    for (int axis = 0; axis < 2; axis++) {
        for (int line = 0; line < SWC_CELLS; line++) {
            const SwcRoad *spec = swc_road_spec(line);
            int kind = swc_is_avenue(line) ? SWC_LANE_AVENUE : SWC_LANE_STREET;
            for (int slot = 0; slot < spec->lane_count; slot++)
                for (int di = 0; di < 2; di++)
                    add_lane(c, kind, axis,
                             swc_lane_coordinate(axis, line, dirs[di], spec->lane_offset[slot]), 0.0,
                             dirs[di], line, slot);
            if (!swc_is_avenue(line))
                continue;
            const double *altitudes = axis == 0 ? SWC_SKY_LANE_Y_X : SWC_SKY_LANE_Y_Z;
            for (int slot = 0; slot < SWC_SKY_LEVELS; slot++)
                for (int di = 0; di < 2; di++)
                    add_lane(c, SWC_LANE_SKY, axis,
                             swc_lane_coordinate(axis, line, dirs[di], SWC_SKY_LANE_OFFSET), altitudes[slot],
                             dirs[di], line, slot);
        }
    }
}

/* =================================================================================================== */
/* Sites: where holograms, searchlights, pillars, lasers, tickers and roof lights go                  */
/* =================================================================================================== */

/* The site's own random number is drawn here, AFTER the caller evaluated its arguments. */
static void add_site(SwcCity *c, SwcRng *rng, int kind, double x, double y, double z, double w, double h,
                     double d, double yaw, int color) {
    double seed = swc_rng_next(rng);
    if (c->site_count >= SWC_MAX_SITES)
        return;
    SwcSite *s = &c->sites[c->site_count++];
    s->kind = (uint8_t)kind;
    s->x = swc_wrap(x, SWC_TILE);
    s->y = y;
    s->z = swc_wrap(z, SWC_TILE);
    s->w = w;
    s->h = h;
    s->d = d;
    s->yaw = yaw;
    s->color = (uint8_t)color;
    s->seed = seed;
}

/* A panel is a thin slab: wide along x when it faces +-z (yaw 0 or pi), along z otherwise. */
static bool panel_clear(const SwcCity *c, double x, double y, double z, double w, double h, double yaw) {
    bool along_x = fabs(sin(yaw)) < 0.5;
    return !swc_city_volume_blocked(c, x, y, z, along_x ? w / 2.0 + 1.0 : 2.5, h / 2.0 + 1.0,
                                    along_x ? 2.5 : w / 2.0 + 1.0);
}

/* Panel heights over an avenue that stay clear of that avenue's own sky lanes. Returns how many. */
static int panel_altitudes(int axis, double *out) {
    const double *own = axis == 0 ? SWC_SKY_LANE_Y_X : SWC_SKY_LANE_Y_Z;
    double candidates[10];
    int n = 0;
    candidates[n++] = 55.0;
    for (int i = 0; i < SWC_SKY_LEVELS; i++)
        candidates[n++] = SWC_SKY_LANE_Y_X[i];
    for (int i = 0; i < SWC_SKY_LEVELS; i++)
        candidates[n++] = SWC_SKY_LANE_Y_Z[i];
    candidates[n++] = 470.0;
    int count = 0;
    for (int k = 0; k < n; k++) {
        bool ok = true;
        for (int i = 0; i < SWC_SKY_LEVELS; i++)
            if (!(fabs(own[i] - candidates[k]) >= 38.0))
                ok = false;
        if (ok)
            out[count++] = candidates[k];
    }
    return count;
}

static void sites_for_buildings(SwcCity *c, SwcRng *rng) {
    for (int bi = 0; bi < c->building_count; bi++) {
        const SwcBuilding *b = &c->buildings[bi];
        if (b->district == SWC_PLAZA)
            continue;
        bool mega = b->district == SWC_MEGA;
        if (b->spire_top > 0.0)
            add_site(c, rng, SWC_SITE_ROOF_LIGHT, b->x, b->spire_top + 0.8, b->z, mega ? 3.0 : 1.6, 0, 0, 0,
                     6);
        else if (b->top >= 140.0)
            add_site(c, rng, SWC_SITE_ROOF_LIGHT, b->x, b->top + 1.5, b->z, 1.2, 0, 0, 0, 6);
        if (mega) {
            double radius = swc_rng_range(rng, 7.0, 10.0);
            add_site(c, rng, SWC_SITE_PILLAR, b->x, b->spire_top, b->z, radius, 2600.0, 0, 0, b->color_a);
            for (int s = 1; s < b->segment_count; s += 2) {
                const SwcSegment *seg = &c->segments[b->mega_slot][s];
                double band = swc_rng_range(rng, 9.0, 14.0);
                add_site(c, rng, SWC_SITE_TICKER, b->x, seg->y0 + seg->h * 0.6, b->z, seg->sx / 2.0 + 2.0,
                         band, seg->sz / 2.0 + 2.0, 0, s % 4 == 1 ? b->color_a : b->color_b);
            }
            continue;
        }
        bool tall = b->district == SWC_MIDTOWN || b->district == SWC_DOWNTOWN;
        if (tall && b->spire_top == 0.0 && b->top >= 60.0 && swc_rng_chance(rng, 0.07)) {
            double length = swc_rng_range(rng, 700.0, 1400.0);
            int beams = swc_rng_int(rng, 1, 3);
            double yaw = swc_rng_range(rng, 0.0, SWC_TAU);
            int color = swc_rng_chance(rng, 0.5) ? 7 : b->color_a;
            add_site(c, rng, SWC_SITE_SEARCHLIGHT, b->x, b->top + 0.5, b->z, length, beams, 0, yaw, color);
        }
        if (b->district == SWC_DOWNTOWN && b->has_crown && b->spire_top == 0.0 && swc_rng_chance(rng, 0.1)) {
            double length = swc_rng_range(rng, 1400.0, 2400.0);
            int beams = swc_rng_int(rng, 5, 9);
            double yaw = swc_rng_range(rng, 0.0, SWC_TAU);
            int color = swc_rng_chance(rng, 0.5) ? b->color_a : 5;
            add_site(c, rng, SWC_SITE_LASER, b->x, b->top + 1.0, b->z, length, beams, 0, yaw, color);
        }
        /* A billboard floating over a roof: common over low roofs, and over the tall towers too. A spire
         * in the way fails the clearance check. */
        double foot = fmin(b->base_x, b->base_z);
        if (foot >= 24.0 && swc_rng_chance(rng, b->top <= 130.0 ? 0.08 : 0.2)) {
            double k1 = swc_rng_range(rng, 0.9, 1.5);
            double w = fmin(70.0, fmax(24.0, foot * k1));
            double h = w * swc_rng_range(rng, 0.4, 0.6);
            double y = b->top + 8.0 + h / 2.0 + swc_rng_range(rng, 0.0, 20.0);
            double yaw = swc_rng_int(rng, 0, 3) * (SWC_PI / 2.0);
            if (panel_clear(c, b->x, y, b->z, w, h, yaw)) {
                int color = swc_rng_chance(rng, 0.6) ? b->color_a : b->color_b;
                add_site(c, rng, SWC_SITE_HOLO_PANEL, b->x, y, b->z, w, h, 0, yaw, color);
            }
        }
    }
}

static void sites_for_plazas(SwcCity *c, SwcRng *rng) {
    for (int k = 0; k < c->plaza_count; k++) {
        int i = c->plaza_cells[k];
        /* The centre of the block, not of the cell: they differ by 4 m beside an avenue. */
        SwcRect rect = swc_block_rect(i % SWC_CELLS, i / SWC_CELLS);
        double x = (rect.x0 + rect.x1) / 2.0, z = (rect.z0 + rect.z1) / 2.0;
        int color = c->cells[i].palette;
        if (swc_rng_chance(rng, 0.75)) {
            double y = swc_rng_range(rng, 55.0, 110.0);
            double radius = swc_rng_range(rng, 20.0, 38.0);
            add_site(c, rng, SWC_SITE_HOLO_OBJECT, x, y, z, radius, 0, 0, 0, color);
        }
        if (swc_rng_chance(rng, 0.55)) {
            double radius = swc_rng_range(rng, 3.5, 6.0);
            double height = swc_rng_range(rng, 1600.0, 2200.0);
            add_site(c, rng, SWC_SITE_PILLAR, x, 0, z, radius, height, 0, 0, color);
        }
        if (swc_rng_chance(rng, 0.5)) {
            int n = swc_rng_int(rng, 2, 4);
            double phase = swc_rng_range(rng, 0.0, SWC_TAU);
            for (int j = 0; j < n; j++) {
                double a = phase + ((double)j / (double)n) * SWC_TAU;
                double length = swc_rng_range(rng, 900.0, 1500.0);
                add_site(c, rng, SWC_SITE_SEARCHLIGHT, x + cos(a) * 18.0, 0.5, z + sin(a) * 18.0, length, 1,
                         0, a, 7);
            }
        }
    }
}

/* Banners across streets and billboards over avenues, mid-block. */
static void sites_for_roads(SwcCity *c, SwcRng *rng) {
    for (int axis = 0; axis < 2; axis++) {
        for (int line = 0; line < SWC_CELLS; line++) {
            bool avenue = swc_is_avenue(line);
            double altitudes[10];
            int alt_count = avenue ? panel_altitudes(axis, altitudes) : 0;
            for (int cc = 0; cc < SWC_CELLS; cc++) {
                /* Denser where the skyline is taller, so downtown canyons are hung with them. */
                int here = axis == 0 ? swc_cell_index(cc, line) : swc_cell_index(line, cc);
                double boost = 1.0 + 2.5 * (double)c->field[here];
                if (!swc_rng_chance(rng, (avenue ? 0.09 : 0.04) * boost))
                    continue;
                double along = (cc + 0.5) * SWC_CELL, fixed = line * SWC_CELL;
                double x = axis == 0 ? along : fixed, z = axis == 0 ? fixed : along;
                /* The panel faces along the road, so it spans the corridor. */
                double flip = swc_rng_chance(rng, 0.5) ? SWC_PI : 0.0;
                double yaw = (axis == 0 ? SWC_PI / 2.0 : 0.0) + flip;
                double w = avenue ? 34.0 : 18.0;
                double h = avenue ? 15.0 : swc_rng_range(rng, 28.0, 46.0);
                double y;
                if (avenue)
                    y = altitudes[swc_rng_pick(rng, alt_count)];
                else
                    y = swc_rng_range(rng, 30.0 + h / 2.0, 260.0);
                if (panel_clear(c, x, y, z, w, h, yaw)) {
                    int color = swc_rng_chance(rng, 0.5) ? c->cells[here].palette : swc_rng_int(rng, 0, 4);
                    add_site(c, rng, SWC_SITE_HOLO_PANEL, x, y, z, w, h, 0, yaw, color);
                }
            }
        }
    }
}

/* Big panels beside megatowers. */
static void sites_beside_megatowers(SwcCity *c, SwcRng *rng) {
    static const int options[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int bi = 0; bi < c->building_count; bi++) {
        const SwcBuilding *b = &c->buildings[bi];
        if (b->district != SWC_MEGA)
            continue;
        int cx = b->cell % SWC_CELLS, cz = b->cell / SWC_CELLS;
        int start = swc_rng_int(rng, 0, 3);
        for (int k = 0; k < 2; k++) {
            int dx = options[(start + k * 2 + (k == 1 ? 1 : 0)) % 4][0];
            int dz = options[(start + k * 2 + (k == 1 ? 1 : 0)) % 4][1];
            int n = swc_cell_index(cx + dx, cz + dz);
            double w = swc_rng_range(rng, 90.0, 120.0);
            double h = swc_rng_range(rng, 40.0, 52.0);
            double x = (cx + dx + 0.5) * SWC_CELL, z = (cz + dz + 0.5) * SWC_CELL;
            double y = (double)c->cells[n].height * 4.0 + 45.0 + h / 2.0;
            double yaw = dx != 0 ? SWC_PI / 2.0 : 0.0;
            if (panel_clear(c, x, y, z, w, h, yaw))
                add_site(c, rng, SWC_SITE_HOLO_PANEL, x, y, z, w, h, 0, yaw,
                         k == 0 ? b->color_a : b->color_b);
        }
    }
}

static void build_sites(SwcCity *c, SwcRng *rng) {
    c->site_count = 0;
    sites_for_buildings(c, rng);
    sites_for_plazas(c, rng);
    sites_for_roads(c, rng);
    sites_beside_megatowers(c, rng);
}

/* =================================================================================================== */
/* Landmarks and the public entry point                                                               */
/* =================================================================================================== */

static void add_landmark(SwcCity *c, int kind, double x, double z, double height, int cell_x, int cell_z) {
    if (c->landmark_count >= SWC_MAX_LANDMARKS)
        return;
    SwcLandmark *l = &c->landmarks[c->landmark_count++];
    l->kind = (uint8_t)kind;
    l->x = x;
    l->z = z;
    l->height = height;
    l->cell_x = cell_x;
    l->cell_z = cell_z;
}

/* Megatowers first (tallest first), then plazas, then the densest downtown cell. */
static void build_landmarks(SwcCity *c) {
    c->landmark_count = 0;
    for (int bi = 0; bi < c->building_count; bi++) {
        const SwcBuilding *b = &c->buildings[bi];
        if (b->district != SWC_MEGA)
            continue;
        add_landmark(c, SWC_LM_MEGATOWER, b->x, b->z, b->spire_top, b->cell % SWC_CELLS, b->cell / SWC_CELLS);
    }
    /* Insertion sort, tallest first (stable). */
    for (int i = 1; i < c->landmark_count; i++) {
        SwcLandmark cur = c->landmarks[i];
        int j = i - 1;
        while (j >= 0 && c->landmarks[j].height < cur.height) {
            c->landmarks[j + 1] = c->landmarks[j];
            j--;
        }
        c->landmarks[j + 1] = cur;
    }
    for (int k = 0; k < c->plaza_count; k++) {
        int i = c->plaza_cells[k];
        int cx = i % SWC_CELLS, cz = i / SWC_CELLS;
        SwcRect rect = swc_block_rect(cx, cz);
        add_landmark(c, SWC_LM_PLAZA, (rect.x0 + rect.x1) / 2.0, (rect.z0 + rect.z1) / 2.0, 0.0, cx, cz);
    }
    int pc = c->peak_cell;
    add_landmark(c, SWC_LM_DOWNTOWN, ((pc % SWC_CELLS) + 0.5) * SWC_CELL, ((pc / SWC_CELLS) + 0.5) * SWC_CELL,
                 (double)c->cells[pc].height * 4.0, pc % SWC_CELLS, pc / SWC_CELLS);
}

void swc_city_build(SwcCity *c, uint32_t seed) {
    memset(c, 0, sizeof(*c));
    c->seed = seed;
    SwcRng rng;
    swc_rng_seed(&rng, seed);
    /* Each stage draws from its own fork, so changing one stage never shifts the others. */
    SwcRng r_districts = swc_rng_fork(&rng, 1u);
    SwcRng r_massing = swc_rng_fork(&rng, 2u);
    SwcRng r_sites = swc_rng_fork(&rng, 3u);
    build_districts(c, &r_districts);
    build_massing(c, &r_massing);
    build_box_index(c);
    build_sites(c, &r_sites);
    build_lanes(c);
    build_landmarks(c);
}
