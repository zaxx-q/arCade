/* Temperature. Every cell has a temperature in degrees C (Sandbox.temp plus the fraction in Sandbox.tfrac),
 * and that includes empty cells: the air is a material too, it just holds almost no heat. A cell's
 * temperature travels with it when it moves, so a falling blob of lava takes its heat along and a rising
 * puff of steam takes its own.
 *
 *   - Every pair of neighbouring cells swaps heat once per tick. The flow is the temperature difference times
 *     a coefficient that is the SMALLER of two limits:
 *       * how well the pair conducts (two resistors in series: one insulating cell throttles the pair however
 *         good the other one is), and
 *       * the heat that would make the two temperatures equal. This is what makes air special: it has a
 *         capacity of 2 against 3000 for lava, so a cell of air can only take a sliver of lava's heat, it
 *         jumps to lava's temperature at once, and lava loses next to nothing. A glowing blob in free air or
 *         falling through it cools very slowly. Touch it to steel, water or stone and heat drains at a
 *         steady rate that depends on how well that material conducts and how much heat it can hold.
 *   - Air is not a bottomless sink. Hot air spreads out and only leaves the world through its edge: the
 *     rim of the grid is a good conductor held at the ambient temperature, so anything touching the edge
 *     (lava against the wall) cools fast and the whole box slowly relaxes back to room temperature.
 *   - Heat sources (Heater, Cooler, Fire, sparks) are held at their own temperature.
 *
 * The numbers are fixed point (1/65536 of a degree) and every exchange gives one cell exactly what the other
 * lost, so no heat is created or lost and no random rounding is needed. A pair never swaps more than it takes
 * to even out its two temperatures, so the model cannot overshoot or oscillate. The four sweeps (pairs along
 * x on even and odd columns, pairs along y on even and odd rows) cover disjoint pairs, so the result does not
 * depend on the order cells are visited in; which sweep goes first alternates every tick so heat has no
 * favourite direction. Rows that hold nothing but ambient-temperature cells are skipped, which keeps a calm
 * world cheap.
 *
 * On top of the temperatures sit the phase changes: water boils and freezes, ice melts, lava sets into
 * basalt, stone melts into lava, flammable things light up by themselves. They are listed in k_thermal. */
#include <stdlib.h>

#include "sandbox_internal.h"

#define BURNING_TEMP 600 /* a burning solid is a heat source at about this temperature */

/* temperatures are kept in a fixed point with this many fractional bits */
#define FRAC_BITS 16
/* one degree, in that fixed point */
#define FINE_ONE (1 << FRAC_BITS)
#define FINE_AMBIENT (SANDBOX_AMBIENT * FINE_ONE)
/* closer to ambient than this counts as ambient (so calm rows can be skipped) */
#define SNAP (FINE_ONE / 8)
/* heat shares are fractions of 1 << SHARE_BITS */
#define SHARE_BITS 24
/* the rim of the world behaves like a block of metal of this capacity */
#define EDGE_CAP 2000
/* The world is a slice of a bigger room: air also loses heat sideways, out of the picture. Every cell of air
 * gives up 1/2^AIR_LEAK_SHIFT of its excess temperature per tick, so a heater warms the air around it
 * instead of slowly filling the whole box with hot air. */
#define AIR_LEAK_SHIFT 8

#define NC INT16_MIN, EL_EMPTY /* never cold enough to change */
#define NH INT16_MAX, EL_EMPTY /* never hot enough to change */

/* Columns:  temp0, cond, cap, autoignite, (cold_t, cold_to), (hot_t, hot_to), rate, fixed, latent_hot,
 * latent_cold
 *
 * cond / cap : how well it conducts and how much heat it holds per degree. Steel 3000/3800 gives its heat up
 *              and takes heat in quickly, water 800/4200 holds a lot and shares it well, brick 30/1500 and
 *              wood 12/1000 hardly pass anything on, and air 2 holds almost nothing at all.
 * autoignite : flammable things light up by themselves above this temperature
 * rate       : phase changes happen with this chance (/256) per tick once the temperature is reached
 * fixed      : 1 = the cell is a heat source or sink and always sits at temp0
 * latent_*   : water that reaches 100 degrees does not boil at once: it stays at 100 while it soaks up heat
 *              (kept in aux, in steps of two degrees of its own temperature) and only turns to steam when it
 *              has taken enough. Ice melting and water freezing work the same way at 0 degrees. 0 = the
 *              change is immediate. */
static const ThermalDef k_thermal[EL_COUNT] = {
    [EL_EMPTY] = {SANDBOX_AMBIENT, 400, 2, 0, NC, NH, 0, 0, 0, 0},

    [EL_SAND] = {20, 12, 1600, 0, NC, 1100, EL_GLASS, 40, 0, 0, 0},
    [EL_WATER] = {20, 300, 4200, 0, 0, EL_ICE, 100, EL_STEAM, 255, 0, 110, 60},
    [EL_STONE] = {20, 220, 2400, 0, NC, 1300, EL_LAVA, 20, 0, 0, 0},

    [EL_SOIL] = {20, 15, 1800, 0, NC, NH, 0, 0, 0, 0},
    [EL_SNOW] = {-5, 8, 300, 0, NC, 0, EL_WATER, 255, 0, 80, 0},
    [EL_GUNPOWDER] = {20, 10, 1200, 260, NC, NH, 0, 0, 0, 0},
    [EL_SEED] = {20, 10, 800, 260, NC, NH, 0, 0, 0, 0},
    [EL_ASH] = {20, 6, 600, 0, NC, NH, 0, 0, 0, 0},
    [EL_THERMITE] = {20, 40, 1800, 700, NC, NH, 0, 0, 0, 0},
    [EL_GRAVEL] = {20, 50, 1800, 0, NC, 1300, EL_LAVA, 20, 0, 0, 0},

    [EL_OIL] = {20, 40, 1900, 280, NC, NH, 0, 0, 0, 0},
    [EL_ACID] = {20, 300, 3600, 0, NC, NH, 0, 0, 0, 0},
    /* cools through red and sets into basalt */
    [EL_LAVA] = {1150, 900, 3000, 0, 500, EL_BASALT, NH, 30, 0, 0, 0},
    [EL_MERCURY] = {20, 700, 1900, 0, NC, NH, 0, 0, 0, 0},
    [EL_NITROGEN] = {-196, 60, 2000, 0, NC, -150, EL_EMPTY, 255, 0, 150, 0},

    [EL_STEAM] = {110, 400, 30, 0, 96, EL_WATER, NH, 40, 0, 0, 100},
    [EL_METHANE] = {20, 400, 3, 480, NC, NH, 0, 0, 0, 0},
    [EL_SMOKE] = {20, 400, 4, 0, NC, NH, 0, 0, 0, 0},

    [EL_WOOD] = {20, 12, 1000, 300, NC, NH, 0, 0, 0, 0},
    [EL_ICE] = {-10, 200, 1900, 0, NC, 0, EL_WATER, 255, 0, 120, 0},
    [EL_GLASS] = {20, 80, 2000, 0, NC, 1700, EL_LAVA, 10, 0, 0, 0},
    [EL_WALL] = {20, 8, 1400, 0, NC, NH, 0, 0, 0, 0},
    [EL_STEEL] = {20, 3000, 3800, 0, NC, 1500, EL_LAVA, 20, 0, 0, 0},
    [EL_BRICK] = {20, 25, 1500, 0, NC, 1900, EL_LAVA, 10, 0, 0, 0},
    [EL_PLANT] = {20, 25, 1500, 320, NC, NH, 0, 0, 0, 0},
    [EL_FUSE] = {20, 10, 800, 250, NC, NH, 0, 0, 0, 0},
    [EL_TNT] = {20, 15, 1500, 330, NC, NH, 0, 0, 0, 0},

    [EL_FIRE] = {800, 400, 3000, 0, NC, NH, 0, 1, 0, 0},
    [EL_HEATER] = {700, 3000, 20000, 0, NC, NH, 0, 1, 0, 0},
    [EL_COOLER] = {-80, 3000, 20000, 0, NC, NH, 0, 1, 0, 0},
    /* heats itself (see react_uranium), melts at 1000. A poor conductor in this game: that is what keeps the
     * heat of a big lump inside it */
    [EL_URANIUM] = {20, 40, 2200, 0, NC, 1000, EL_MOLTEN_URANIUM, 60, 0, 0, 0},
    [EL_TERMITE] = {20, 40, 1500, 300, NC, NH, 0, 0, 0, 0},
    [EL_LIFE] = {20, 20, 1000, 0, NC, NH, 0, 0, 0, 0},

    [EL_GAMMA] = {20, 400, 2, 0, NC, NH, 0, 0, 0, 0},
    [EL_MOLTEN_URANIUM] = {1000, 40, 2200, 0, 800, EL_URANIUM, NH, 20, 0, 0, 0},
    [EL_SPARK] = {1200, 400, 3000, 0, NC, NH, 0, 1, 0, 0},
    [EL_BASALT] = {20, 200, 2400, 0, NC, 1100, EL_LAVA, 20, 0, 0, 0},
};

const ThermalDef *sandbox_thermal(ElementId id) {
    if ((int)id < 0 || id >= EL_COUNT)
        return &k_thermal[EL_EMPTY];
    return &k_thermal[id];
}

/* ---- fixed-point temperatures ---- */

static int clamp_temp(int t) {
    return arc_clampi(t, SANDBOX_TEMP_MIN, SANDBOX_TEMP_MAX);
}

/* The temperature of cell i in 1/65536 degrees. (Multiplied, not shifted: temp can be negative.) */
static inline int32_t fine_get(const Sandbox *sb, int i) {
    return (int32_t)sb->temp[i] * FINE_ONE + (int32_t)sb->tfrac[i];
}

/* Store a temperature. The arithmetic shift rounds towards minus infinity, which is exactly what the split
 * into "whole degrees + a positive fraction" needs (-0.25 is -1 plus 0.75). */
static inline void fine_set(Sandbox *sb, int i, int32_t v) {
    sb->temp[i] = (int16_t)(v >> FRAC_BITS);
    sb->tfrac[i] = (uint16_t)(v & (FINE_ONE - 1));
}

/* ---- how much heat two kinds of cell swap ---- */

/* For a pair (a, b) the heat that flows is  diff * K  with K = min(series conductance, equalising limit).
 * Cell a changes by diff * K / cap_a and cell b by diff * K / cap_b. The tables hold those two fractions
 * (in 1 / 2^24), so the hot loop is two multiplications. */
typedef struct HeatShare {
    uint32_t a, b;
} HeatShare;

static HeatShare g_share[EL_COUNT][EL_COUNT];
static uint32_t g_edge_share[EL_COUNT]; /* the same for a cell and the rim of the world */
static bool g_tables_ready;

static void build_tables(void) {
    const int64_t one = (int64_t)1 << SHARE_BITS;
    for (int p = 0; p < EL_COUNT; p++) {
        int64_t ca = k_thermal[p].cap, ga = k_thermal[p].cond;
        for (int q = 0; q < EL_COUNT; q++) {
            int64_t cb = k_thermal[q].cap, gb = k_thermal[q].cond;
            int64_t even_out = ca * cb * one / (ca + cb); /* the heat that makes both the same */
            int64_t pipe = ga + gb == 0 ? 0 : ga * gb * one / (ga + gb);
            int64_t k = pipe < even_out ? pipe : even_out;
            g_share[p][q].a = (uint32_t)(k / ca);
            g_share[p][q].b = (uint32_t)(k / cb);
        }
        int64_t even_out = ca * EDGE_CAP * one / (ca + EDGE_CAP);
        int64_t pipe = ga * one; /* the rim itself conducts perfectly: only the cell's own side limits it */
        g_edge_share[p] = (uint32_t)((pipe < even_out ? pipe : even_out) / ca);
    }
    g_tables_ready = true;
}

/* ---- bookkeeping ---- */

void sandbox_heat_clear(Sandbox *sb) {
    for (int i = 0; i < SANDBOX_MAX_W * SANDBOX_MAX_H; i++) {
        sb->temp[i] = SANDBOX_AMBIENT;
        sb->tfrac[i] = 0;
    }
    for (int y = 0; y < SANDBOX_MAX_H; y++)
        sb->heat_rows[y] = 0;
}

void sandbox_heat_mark(Sandbox *sb, int y) {
    if (y >= 0 && y < SANDBOX_MAX_H)
        sb->heat_rows[y] = 1;
}

/* The temperature a cell gets when it is turned into `id` by a reaction (or painted). It keeps the heat it
 * had, except that things that are made hot or cold start at least that hot or cold, and a few materials are
 * kept inside the range where they can exist (so liquid water is never made at -10 degrees, only to freeze
 * again). Air made by a reaction (a flame going out) keeps its heat; erasing with the brush resets it. */
void sandbox_heat_on_put(Sandbox *sb, int i, ElementId id) {
    int old = sb->temp[i];
    int t = old;
    const ThermalDef *d = sandbox_thermal(id);
    if (id != EL_EMPTY) {
        if (d->fixed)
            t = d->temp0;
        else if (d->temp0 > 100)
            t = arc_maxi(t, d->temp0);
        else if (d->temp0 < 0)
            t = arc_mini(t, d->temp0);
    }
    switch (id) {
    case EL_WATER:
        t = arc_clampi(t, 2, 98);
        break;
    case EL_STONE: /* lava that sets is hot rock, but not hot enough to melt again at once */
        t = arc_mini(t, 900);
        break;
    default:
        break;
    }
    t = clamp_temp(t);
    if (t != old)
        sb->tfrac[i] = 0;
    sb->temp[i] = (int16_t)t;
    if (t != SANDBOX_AMBIENT || sb->tfrac[i] != 0)
        sandbox_heat_mark(sb, i / sb->w);
}

int sandbox_temp(const Sandbox *sb, int x, int y) {
    if (!sandbox_inside(sb, x, y))
        return SANDBOX_AMBIENT;
    return sb->temp[y * sb->w + x];
}

void sandbox_set_temp(Sandbox *sb, int x, int y, int degrees) {
    if (!sandbox_inside(sb, x, y))
        return;
    int i = y * sb->w + x;
    sb->temp[i] = (int16_t)clamp_temp(degrees);
    sb->tfrac[i] = 0;
    sandbox_heat_mark(sb, y);
}

void sandbox_add_heat(Sandbox *sb, int x, int y, int degrees) {
    sandbox_set_temp(sb, x, y, sandbox_temp(sb, x, y) + degrees);
}

void sandbox_heat_inject(Sandbox *sb, int i, int energy) {
    int64_t dt = (int64_t)energy * FINE_ONE / k_thermal[sb->cells[i]].cap;
    int64_t t = (int64_t)fine_get(sb, i) + dt;
    int64_t lo = (int64_t)SANDBOX_TEMP_MIN * FINE_ONE, hi = (int64_t)SANDBOX_TEMP_MAX * FINE_ONE;
    fine_set(sb, i, (int32_t)(t < lo ? lo : t > hi ? hi : t));
    sandbox_heat_mark(sb, i / sb->w);
}

/* ---- heat flow ---- */

/* Cells i and j swap heat. */
static inline void exchange(Sandbox *sb, int i, int j) {
    int32_t ti = fine_get(sb, i), tj = fine_get(sb, j);
    int32_t diff = ti - tj;
    if (diff == 0)
        return;
    const HeatShare *s = &g_share[sb->cells[i]][sb->cells[j]];
    uint32_t mag = (uint32_t)(diff < 0 ? -diff : diff);
    int32_t da = (int32_t)(((uint64_t)mag * s->a) >> SHARE_BITS);
    int32_t db = (int32_t)(((uint64_t)mag * s->b) >> SHARE_BITS);
    if (diff < 0) {
        da = -da;
        db = -db;
    }
    fine_set(sb, i, ti - da);
    fine_set(sb, j, tj + db);
}

/* A cell on the rim swaps heat with the outside, once for each side of it that faces out. */
static void exchange_with_rim(Sandbox *sb, int i, int sides) {
    int32_t t = fine_get(sb, i);
    uint32_t share = g_edge_share[sb->cells[i]];
    for (int k = 0; k < sides; k++) {
        int32_t diff = t - FINE_AMBIENT;
        uint32_t mag = (uint32_t)(diff < 0 ? -diff : diff);
        int32_t moved = (int32_t)(((uint64_t)mag * share) >> SHARE_BITS);
        t += diff < 0 ? moved : -moved;
    }
    fine_set(sb, i, t);
}

static bool is_burning_solid(const Sandbox *sb, int i) {
    ElementId id = (ElementId)sb->cells[i];
    return (id == EL_WOOD || id == EL_PLANT || id == EL_FUSE || id == EL_OIL) && (sb->aux[i] & BURN_BIT);
}

void sandbox_heat_step(Sandbox *sb) {
    int w = sb->w, h = sb->h;
    if (!g_tables_ready)
        build_tables();
    bool near_hot[SANDBOX_MAX_H]; /* the row, or one right next to it, may hold heat */
    bool any = false;
    for (int y = 0; y < h; y++) {
        near_hot[y] =
            sb->heat_rows[y] || (y > 0 && sb->heat_rows[y - 1]) || (y + 1 < h && sb->heat_rows[y + 1]);
        any = any || near_hot[y];
    }
    if (!any)
        return;

    /* pairs side by side: first the pairs (0,1) (2,3) ..., then (1,2) (3,4) ... (the other way round on
     * odd ticks) */
    for (int pass = 0; pass < 2; pass++) {
        int start = (int)((sb->tick + (unsigned)pass) & 1u);
        for (int y = 0; y < h; y++) {
            if (!near_hot[y])
                continue;
            for (int x = start; x + 1 < w; x += 2)
                exchange(sb, y * w + x, y * w + x + 1);
        }
    }
    /* pairs on top of each other, the same way */
    for (int pass = 0; pass < 2; pass++) {
        int start = (int)(((sb->tick >> 1) + (unsigned)pass) & 1u);
        for (int y = start; y + 1 < h; y += 2) {
            if (!near_hot[y] || !near_hot[y + 1])
                continue;
            for (int x = 0; x < w; x++)
                exchange(sb, y * w + x, (y + 1) * w + x);
        }
    }
    /* the rim of the world */
    for (int y = 0; y < h; y++) {
        if (!near_hot[y])
            continue;
        if (y == 0 || y == h - 1) {
            int sides = (y == 0) + (y == h - 1);
            for (int x = 0; x < w; x++)
                exchange_with_rim(sb, y * w + x, sides + (x == 0) + (x == w - 1));
        } else {
            exchange_with_rim(sb, y * w, 1 + (w == 1));
            if (w > 1)
                exchange_with_rim(sb, y * w + w - 1, 1);
        }
    }

    /* hold the sources at their temperature, tidy up, and note which rows still hold heat */
    for (int y = 0; y < h; y++) {
        if (!near_hot[y])
            continue;
        bool row_hot = false;
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            const ThermalDef *d = &k_thermal[sb->cells[i]];
            if (d->fixed) {
                sb->temp[i] = d->temp0;
                sb->tfrac[i] = 0;
                row_hot = true;
                continue;
            }
            int32_t t = fine_get(sb, i);
            if (is_burning_solid(sb, i) && t < BURNING_TEMP * FINE_ONE)
                t = BURNING_TEMP * FINE_ONE;
            int32_t dev = t - FINE_AMBIENT;
            if (sb->cells[i] == EL_EMPTY) {
                dev -= dev / (1 << AIR_LEAK_SHIFT);
                t = FINE_AMBIENT + dev;
            }
            if (dev < SNAP && dev > -SNAP) {
                sb->temp[i] = SANDBOX_AMBIENT; /* close enough to ambient: say so, so the row can rest */
                sb->tfrac[i] = 0;
                continue;
            }
            fine_set(sb, i, t);
            row_hot = true;
        }
        sb->heat_rows[y] = row_hot ? 1 : 0;
    }
}

/* ---- phase changes and self-ignition ---- */

/* Phase change at a threshold with latent heat: the cell is held at the threshold temperature and everything
 * that would have heated it further goes into aux until the cell has soaked up `latent` units (a unit is
 * two degrees of its own temperature). `over` is how far past the threshold it got, in 1/65536 degrees. */
static bool soak(Sandbox *sb, int i, int32_t over, int latent, int *aux_acc) {
    const int32_t unit = 2 * FINE_ONE;
    int32_t add = over / unit, rem = over % unit;
    if (rem != 0 && (int32_t)(rng_u32(&sb->rng) % (uint32_t)unit) < rem)
        add++;
    int acc = sb->aux[i] + add;
    *aux_acc = arc_mini(acc, 255);
    return acc >= latent;
}

bool sandbox_heat_react(Sandbox *sb, int x, int y) {
    int i = y * sb->w + x;
    ElementId id = (ElementId)sb->cells[i];
    const ThermalDef *d = &k_thermal[id];
    int t = sb->temp[i];

    if (d->autoignite != 0 && t >= d->autoignite && sandbox_flammable_now(sb, i)) {
        sandbox_ignite(sb, x, y);
        return true;
    }
    bool has_hot = d->hot_t != INT16_MAX, has_cold = d->cold_t != INT16_MIN;
    int32_t fine = fine_get(sb, i);
    if (has_hot && (d->latent_hot ? fine > d->hot_t * FINE_ONE : t >= d->hot_t)) {
        if (d->latent_hot) {
            int acc;
            bool done = soak(sb, i, fine - d->hot_t * FINE_ONE, d->latent_hot, &acc);
            sb->temp[i] = d->hot_t; /* the extra heat went into the change, not into the temperature */
            sb->tfrac[i] = 0;
            sb->aux[i] = (uint8_t)acc;
            if (done) {
                sandbox_put(sb, x, y, d->hot_to, 0);
                return true;
            }
        } else if ((int)(rng_u32(&sb->rng) & 255u) < d->rate) {
            sandbox_put(sb, x, y, d->hot_to, 0);
            return true;
        }
    } else if (has_cold && (d->latent_cold ? fine < d->cold_t * FINE_ONE : t <= d->cold_t)) {
        if (d->latent_cold) {
            int acc;
            bool done = soak(sb, i, d->cold_t * FINE_ONE - fine, d->latent_cold, &acc);
            sb->temp[i] = d->cold_t;
            sb->tfrac[i] = 0;
            sb->aux[i] = (uint8_t)acc;
            if (done) {
                sandbox_put(sb, x, y, d->cold_to, 0);
                return true;
            }
        } else if ((int)(rng_u32(&sb->rng) & 255u) < d->rate) {
            sandbox_put(sb, x, y, d->cold_to, 0);
            return true;
        }
    } else if ((d->latent_hot || d->latent_cold) && sb->aux[i] != 0 && t < d->hot_t - 3 &&
               t > d->cold_t + 3) {
        sb->aux[i] = 0; /* it cooled down again before the change was finished: the soaked-up heat is gone */
    }
    return false;
}
