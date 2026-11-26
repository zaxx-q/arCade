/* Synthwave City - the beams layer's instance data. See swc_beams.h. Ported from the web version
 * (src/layers/beams/pack.ts). Every random draw goes into a local, in the original's order: C leaves the
 * order of function arguments open, and JavaScript does not. */
#include "swc_beams.h"

#include <math.h>
#include <string.h>

#define DEG (3.14159265358979323846 / 180.0)
#define TAU SWC_TAU

/* The ranges the packer guarantees, whatever the sites say. */
#define SEARCHLIGHT_LENGTH_MIN 600.0
#define SEARCHLIGHT_LENGTH_MAX 1500.0
#define SEARCHLIGHT_END_RADIUS_MIN 10.0
#define SEARCHLIGHT_END_RADIUS_MAX 64.0
#define SEARCHLIGHT_TILT_MIN (20.0 * DEG)
#define SEARCHLIGHT_TILT_MAX (60.0 * DEG)
#define PILLAR_LENGTH_MIN 1500.0
#define PILLAR_LENGTH_MAX 3000.0
#define PILLAR_RADIUS_MIN 3.0
#define PILLAR_RADIUS_MAX 11.0
#define LASER_LENGTH_MIN 1200.0
#define LASER_LENGTH_MAX 2600.0

/* Least distance in metres between two lit emitters of one kind at full density. */
enum { EM_SEARCHLIGHT = 0, EM_PLAZA_PILLAR, EM_LASER, EM_KINDS };
static const double EMITTER_SPACING[EM_KINDS] = {640.0, 240.0, 650.0};

/* ---- small helpers ---- */

/* JavaScript's Math.round: halves go up. */
static double js_round(double v) {
    return floor(v + 0.5);
}

/* Whole cycles per loop for a period in seconds, at least one, so the motion wraps seamlessly. */
static double cycles(double period_seconds) {
    double c = js_round(SWC_LOOP_SECONDS / period_seconds);
    return c < 1.0 ? 1.0 : c;
}

static double sign_of(SwcRng *rng) {
    return swc_rng_chance(rng, 0.5) ? 1.0 : -1.0;
}

/* hashText() of the engine: the id of a layer becomes part of its seed. */
static int32_t hash_text(const char *text) {
    uint32_t h = 0;
    for (; *text; text++)
        h = h * 31u + (uint32_t)(unsigned char)*text;
    return (int32_t)h;
}

uint32_t swc_beam_layer_seed(uint32_t city_seed) {
    uint32_t v[2];
    v[0] = city_seed;
    v[1] = (uint32_t)hash_text("beams");
    return swc_hash_ints(v, 2);
}

/* A generator that belongs to one site: the same site gives the same beams whatever else is in the list. */
static SwcRng site_rng(uint32_t seed, const SwcSite *site, uint32_t salt) {
    uint32_t v[3];
    SwcRng r;
    v[0] = seed;
    v[1] = (uint32_t)floor(site->seed * 4294967296.0);
    v[2] = salt;
    swc_rng_seed(&r, swc_hash_ints(v, 3));
    return r;
}

static int cell_of(double v) {
    return (int)floor(v / SWC_CELL);
}

/* Searchlights on a plaza floor come in rings; the ones on roofs stand alone. */
static bool on_plaza(const SwcSite *site) {
    return site->y < 5.0;
}

/* A pillar that starts on a megatower spire is a landmark; one on a plaza floor is quieter. */
static bool is_landmark(const SwcSite *site) {
    return site->y > 5.0;
}

/* ---- writing beams ---- */

static void put4(float *a, int i, double v0, double v1, double v2, double v3) {
    a[i * 4] = (float)v0;
    a[i * 4 + 1] = (float)v1;
    a[i * 4 + 2] = (float)v2;
    a[i * 4 + 3] = (float)v3;
}

static void put_origin(SwcBeamSet *s, int i, const SwcSite *site) {
    s->origin[i * 3] = (float)site->x;
    s->origin[i * 3 + 1] = (float)site->y;
    s->origin[i * 3 + 2] = (float)site->z;
}

typedef struct Sweep {
    double tilt_mid, tilt_amp, tilt_cycles, tilt_phase;
    double turns;
} Sweep;

/* A tilt that stays inside the searchlight limits, and a slow steady turn. */
static Sweep sweep(SwcRng *rng, double turn_chance) {
    Sweep s;
    s.tilt_mid = swc_rng_range(rng, 33.0, 47.0) * DEG;
    double room = fmin(s.tilt_mid - SEARCHLIGHT_TILT_MIN, SEARCHLIGHT_TILT_MAX - s.tilt_mid) - 0.5 * DEG;
    s.tilt_amp = swc_rng_range(rng, 0.45, 1.0) * room;
    s.tilt_cycles = cycles(swc_rng_range(rng, 16.0, 44.0));
    s.tilt_phase = swc_rng_range(rng, 0.0, TAU);
    if (swc_rng_chance(rng, turn_chance)) {
        double sg = sign_of(rng);
        s.turns = sg * cycles(swc_rng_range(rng, 55.0, 150.0));
    } else {
        s.turns = 0.0;
    }
    return s;
}

/* The per-beam part of a searchlight: what the web version's push() closure does. */
typedef struct SearchlightCtx {
    SwcBeamSet *out;
    const SwcSite *site;
    SwcRng *rng;
    double length;
} SearchlightCtx;

static void push_searchlight(const SearchlightCtx *c, const Sweep *s, double azim_base, double sway_amp,
                             double sway_cycles, double sway_phase, double tilt_lag, double gain) {
    SwcBeamSet *out = c->out;
    if (out->count >= SWC_BEAM_MAX)
        return;
    int i = out->count++;
    double r0 = swc_rng_range(c->rng, 1.4, 2.2);
    double spread = swc_rng_range(c->rng, 0.022, 0.04);
    double r1 = swc_clamp(r0 + c->length * spread, SEARCHLIGHT_END_RADIUS_MIN, SEARCHLIGHT_END_RADIUS_MAX);
    put_origin(out, i, c->site);
    put4(out->shape, i, c->length, r0, r1, SWC_BEAM_SEARCHLIGHT);
    double seed01 = swc_rng_next(c->rng);
    double jitter = swc_rng_range(c->rng, 0.85, 1.15);
    put4(out->look, i, c->site->color, seed01, gain * jitter, 0.0);
    put4(out->tilt, i, s->tilt_mid, s->tilt_amp, s->tilt_cycles, s->tilt_phase + tilt_lag);
    put4(out->azim, i, azim_base, s->turns, sway_amp, sway_cycles);
    put4(out->fan, i, 0, 0, 0, 0);
    put4(out->roll, i, 0, 0, sway_phase, 1);
}

static void pack_searchlights(SwcBeamSet *out, const SwcSite *site, uint32_t seed) {
    SwcRng rng = site_rng(seed, site, 1u);
    SearchlightCtx c;
    c.out = out;
    c.site = site;
    c.rng = &rng;
    c.length = swc_clamp(site->w, SEARCHLIGHT_LENGTH_MIN, SEARCHLIGHT_LENGTH_MAX);

    if (on_plaza(site)) {
        /* A ring of lights round a plaza moves in loose formation: the motion comes from the cell the plaza
         * is in, so every light of the ring shares it, and each one lags a little behind the next. */
        int cx = cell_of(site->x), cz = cell_of(site->z);
        uint32_t v[4];
        v[0] = seed;
        v[1] = (uint32_t)cx;
        v[2] = (uint32_t)cz;
        v[3] = 77u;
        SwcRng group;
        swc_rng_seed(&group, swc_hash_ints(v, 4));
        Sweep s = sweep(&group, 1.0);
        double centre_x = ((double)cx + 0.5) * SWC_CELL, centre_z = ((double)cz + 0.5) * SWC_CELL;
        double radial = atan2(site->z - centre_z, site->x - centre_x);
        double sway_cycles = cycles(swc_rng_range(&group, 20.0, 40.0));
        push_searchlight(&c, &s, radial, 0.22, sway_cycles, radial, radial * 0.35, 1.0);
        return;
    }

    int n = (int)swc_clamp(js_round(site->h), 1.0, 3.0);
    double base = site->yaw;
    if (n == 1) {
        Sweep s = sweep(&rng, 0.5);
        /* Without a steady turn it sways through a wide arc instead. */
        double sway_amp = s.turns == 0.0 ? swc_rng_range(&rng, 0.6, 1.3) : swc_rng_range(&rng, 0.1, 0.3);
        double sway_cycles = cycles(swc_rng_range(&rng, 26.0, 60.0));
        double sway_phase = swc_rng_range(&rng, 0.0, TAU);
        push_searchlight(&c, &s, base, sway_amp, sway_cycles, sway_phase, 0.0, 1.0);
    } else if (n == 2) {
        Sweep s = sweep(&rng, 0.35);
        double sway_cycles = cycles(swc_rng_range(&rng, 22.0, 48.0));
        double sway_phase = swc_rng_range(&rng, 0.0, TAU);
        if (swc_rng_chance(&rng, 0.6)) {
            /* A crossing pair: the same sway with opposite sign, so the beams scissor through each other. */
            double amp = swc_rng_range(&rng, 0.55, 1.1);
            push_searchlight(&c, &s, base, amp, sway_cycles, sway_phase, 0.0, 0.85);
            push_searchlight(&c, &s, base, -amp, sway_cycles, sway_phase, 0.4, 0.85);
        } else {
            /* A V: opposite sides, opening and closing together while the pair turns. */
            Sweep turning = s;
            if (s.turns == 0.0) {
                double sg = sign_of(&rng);
                turning.turns = sg * cycles(swc_rng_range(&rng, 60.0, 130.0));
            }
            push_searchlight(&c, &turning, base, 0.12, sway_cycles, sway_phase, 0.0, 0.85);
            push_searchlight(&c, &turning, base + 3.14159265358979323846, 0.12, sway_cycles, sway_phase, 0.0,
                             0.85);
        }
    } else {
        /* A tripod, turning, its legs breathing in a wave. */
        Sweep s = sweep(&rng, 1.0);
        double sway_cycles = cycles(swc_rng_range(&rng, 24.0, 50.0));
        double sway_phase = swc_rng_range(&rng, 0.0, TAU);
        for (int k = 0; k < 3; k++)
            push_searchlight(&c, &s, base + ((double)k * TAU) / 3.0, 0.1, sway_cycles, sway_phase,
                             (double)k * 0.7, 0.75);
    }
}

static void pack_pillar(SwcBeamSet *out, const SwcSite *site, uint32_t seed) {
    if (out->count >= SWC_BEAM_MAX)
        return;
    int i = out->count++;
    SwcRng rng = site_rng(seed, site, 2u);
    double radius = swc_clamp(site->w, PILLAR_RADIUS_MIN, PILLAR_RADIUS_MAX);
    bool landmark = is_landmark(site);
    put_origin(out, i, site);
    put4(out->shape, i, swc_clamp(site->h, PILLAR_LENGTH_MIN, PILLAR_LENGTH_MAX), radius, radius,
         SWC_BEAM_PILLAR);
    double seed01 = swc_rng_next(&rng);
    put4(out->look, i, site->color, seed01, landmark ? 1.3 : 0.65, 0.0);
    put4(out->tilt, i, 0, 0, 0, 0);
    put4(out->azim, i, 0, 0, 0, 0);
    put4(out->fan, i, 0, 0, 0, 0);
    put4(out->roll, i, 0, 0, 0, 1);
}

static void pack_laser_fan(SwcBeamSet *out, const SwcSite *site, uint32_t seed) {
    SwcRng rng = site_rng(seed, site, 3u);
    int n = (int)swc_clamp(js_round(site->h), 5.0, 9.0);
    double length = swc_clamp(site->w, LASER_LENGTH_MIN, LASER_LENGTH_MAX);
    /* The centre line of the fan leans at most 45 degrees and the fan opens at most 1.2 rad, so no beam drops
     * to the horizon (45 + 34 degrees, under the 84 degree limit). */
    double tilt_mid = swc_rng_range(&rng, 24.0, 35.0) * DEG;
    double tilt_amp = swc_rng_range(&rng, 4.0, 10.0) * DEG;
    double tilt_cycles = cycles(swc_rng_range(&rng, 28.0, 70.0));
    double tilt_phase = swc_rng_range(&rng, 0.0, TAU);
    double sg = sign_of(&rng);
    double turns = sg * cycles(swc_rng_range(&rng, 80.0, 200.0));
    double open_mid = swc_rng_range(&rng, 0.5, 0.75);
    double open_amp = fmin(swc_rng_range(&rng, 0.25, 0.45), open_mid - 0.12);
    double open_cycles = cycles(swc_rng_range(&rng, 9.0, 22.0));
    double open_phase = swc_rng_range(&rng, 0.0, TAU);
    double roll_base = swc_rng_range(&rng, 0.0, TAU);
    double roll_turns = 0.0;
    if (swc_rng_chance(&rng, 0.7)) {
        double sg2 = sign_of(&rng);
        roll_turns = sg2 * cycles(swc_rng_range(&rng, 90.0, 240.0));
    }
    double radius = swc_rng_range(&rng, 0.15, 0.24);
    double gain = swc_rng_range(&rng, 0.85, 1.1);
    for (int k = 0; k < n; k++) {
        double seed01 = swc_rng_next(&rng);
        if (out->count >= SWC_BEAM_MAX)
            return;
        int i = out->count++;
        put_origin(out, i, site);
        put4(out->shape, i, length, radius, radius, SWC_BEAM_LASER);
        put4(out->look, i, site->color, seed01, gain, (double)k / (double)(n - 1) - 0.5);
        put4(out->tilt, i, tilt_mid, tilt_amp, tilt_cycles, tilt_phase);
        put4(out->azim, i, site->yaw, turns, 0, 0);
        put4(out->fan, i, open_mid, open_amp, open_cycles, open_phase);
        put4(out->roll, i, roll_base, roll_turns, 0, 1.0 / (double)n);
    }
}

/* ---- choosing the emitters ---- */

typedef struct Emitter {
    double x, z;
    double rank; /* lower ranks are placed first */
    bool kept;
} Emitter;

/* Scratch of swc_choose_emitters: one pass handles one kind. Not reentrant. */
static Emitter g_emitters[SWC_MAX_SITES];
static int g_order[SWC_MAX_SITES];
static int g_owner[SWC_MAX_SITES]; /* per site: its emitter in this pass, or -1 */

static int new_emitter(int *count, double x, double z, double rank) {
    int e = (*count)++;
    g_emitters[e].x = x;
    g_emitters[e].z = z;
    g_emitters[e].rank = rank;
    g_emitters[e].kept = false;
    return e;
}

void swc_choose_emitters(const SwcCity *city, double density, uint32_t layer_seed, bool *lit) {
    int n = city->site_count;
    memset(lit, 0, sizeof(bool) * (size_t)n);

    for (int kind = 0; kind < EM_KINDS; kind++) {
        int count = 0;
        int ring_of_cell[SWC_CELL_COUNT];
        for (int i = 0; i < SWC_CELL_COUNT; i++)
            ring_of_cell[i] = -1;
        for (int i = 0; i < n; i++) {
            const SwcSite *site = &city->sites[i];
            g_owner[i] = -1;
            if (kind == EM_PLAZA_PILLAR && site->kind == SWC_SITE_PILLAR) {
                if (is_landmark(site)) {
                    lit[i] = true; /* megatower pillars are landmarks: always lit */
                } else {
                    SwcRng r = site_rng(layer_seed, site, 0u);
                    g_owner[i] = new_emitter(&count, site->x, site->z, swc_rng_next(&r));
                }
            } else if (kind == EM_LASER && site->kind == SWC_SITE_LASER) {
                SwcRng r = site_rng(layer_seed, site, 0u);
                g_owner[i] = new_emitter(&count, site->x, site->z, swc_rng_next(&r));
            } else if (kind == EM_SEARCHLIGHT && site->kind == SWC_SITE_SEARCHLIGHT) {
                if (!on_plaza(site)) {
                    SwcRng r = site_rng(layer_seed, site, 0u);
                    g_owner[i] = new_emitter(&count, site->x, site->z, 0.5 + 0.5 * swc_rng_next(&r));
                    continue;
                }
                /* A plaza ring counts as one emitter at the centre of its cell, kept or dropped whole. */
                int cx = cell_of(site->x), cz = cell_of(site->z);
                int cell = swc_cell_index(cx, cz);
                if (ring_of_cell[cell] < 0) {
                    uint32_t v[4];
                    SwcRng r;
                    v[0] = layer_seed;
                    v[1] = (uint32_t)cx;
                    v[2] = (uint32_t)cz;
                    v[3] = 78u;
                    swc_rng_seed(&r, swc_hash_ints(v, 4));
                    ring_of_cell[cell] = new_emitter(&count, ((double)cx + 0.5) * SWC_CELL,
                                                     ((double)cz + 0.5) * SWC_CELL, 0.5 * swc_rng_next(&r));
                }
                g_owner[i] = ring_of_cell[cell];
            }
        }

        /* Stable insertion sort by rank (the original's Array.sort is stable). */
        for (int k = 0; k < count; k++) {
            int e = k;
            int j = k;
            while (j > 0 && g_emitters[g_order[j - 1]].rank > g_emitters[e].rank) {
                g_order[j] = g_order[j - 1];
                j--;
            }
            g_order[j] = e;
        }

        double spacing = EMITTER_SPACING[kind] / sqrt(swc_clamp(density, 0.05, 1.0));
        for (int k = 0; k < count; k++) {
            Emitter *e = &g_emitters[g_order[k]];
            bool clear = true;
            for (int m = 0; m < k && clear; m++) {
                const Emitter *o = &g_emitters[g_order[m]];
                if (!o->kept)
                    continue;
                double d = hypot(swc_wrap_delta(e->x, o->x, SWC_TILE), swc_wrap_delta(e->z, o->z, SWC_TILE));
                if (d < spacing)
                    clear = false;
            }
            e->kept = clear;
        }
        for (int i = 0; i < n; i++)
            if (g_owner[i] >= 0 && g_emitters[g_owner[i]].kept)
                lit[i] = true;
    }
}

void swc_pack_beams(const SwcCity *city, double density, uint32_t layer_seed, SwcPackedBeams *out) {
    static bool lit[SWC_MAX_SITES];
    out->sky.count = 0;
    out->pillars.count = 0;
    out->sites_used[SWC_BEAM_SEARCHLIGHT] = 0;
    out->sites_used[SWC_BEAM_PILLAR] = 0;
    out->sites_used[SWC_BEAM_LASER] = 0;
    swc_choose_emitters(city, density, layer_seed, lit);
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *site = &city->sites[i];
        if (!lit[i])
            continue;
        if (site->kind == SWC_SITE_SEARCHLIGHT) {
            pack_searchlights(&out->sky, site, layer_seed);
            out->sites_used[SWC_BEAM_SEARCHLIGHT]++;
        } else if (site->kind == SWC_SITE_PILLAR) {
            pack_pillar(&out->pillars, site, layer_seed);
            out->sites_used[SWC_BEAM_PILLAR]++;
        } else if (site->kind == SWC_SITE_LASER) {
            pack_laser_fan(&out->sky, site, layer_seed);
            out->sites_used[SWC_BEAM_LASER]++;
        }
    }
}

/* ---- the CPU mirror of bm_dir() ---- */

static double wave(double cycles_per_loop, double phase, double loop) {
    double turn = cycles_per_loop * loop;
    return sin(TAU * (turn - floor(turn)) + phase);
}

void swc_beam_direction(const SwcBeamSet *set, int i, double loop, double out[3]) {
    int o = i * 4;
    double th = set->tilt[o] + set->tilt[o + 1] * wave(set->tilt[o + 2], set->tilt[o + 3], loop);
    double spin = (double)set->azim[o + 1] * loop;
    double ph = set->azim[o] + TAU * (spin - floor(spin)) +
                set->azim[o + 2] * wave(set->azim[o + 3], set->roll[o + 2], loop);
    double st = sin(th), ct = cos(th), sp = sin(ph), cp = cos(ph);
    double a =
        set->look[o + 3] * (set->fan[o] + set->fan[o + 1] * wave(set->fan[o + 2], set->fan[o + 3], loop));
    double roll_turn = (double)set->roll[o + 1] * loop;
    double ro = set->roll[o] + TAU * (roll_turn - floor(roll_turn));
    double cr = cos(ro), sr = sin(ro);
    /* e1 and e2 span the plane across the centre line c; neither degenerates when c is vertical. */
    double ux = cr * -sp + sr * ct * cp;
    double uy = sr * -st;
    double uz = cr * cp + sr * ct * sp;
    double ca = cos(a), sa = sin(a);
    out[0] = ca * st * cp + sa * ux;
    out[1] = ca * ct + sa * uy;
    out[2] = ca * st * sp + sa * uz;
}
