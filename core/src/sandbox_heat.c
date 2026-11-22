/* Temperature. Every non-empty cell has a temperature in degrees C (Sandbox.temp) that travels with it when
 * it moves. Each tick, heat flows between neighbouring cells, and from the surface of solids into the open
 * air:
 *
 *   - flow between two cells ~ (difference in temperature) x (how well the pair conducts)
 *     The pair conducts like two resistors in series, so one insulating cell (brick) throttles the whole
 *     pair however good the other one is (steel).
 *   - a cell warms by (heat that arrived) / (its heat capacity): water heats slowly, gas quickly.
 *   - empty cells are the open air and always sit at the ambient temperature; they soak up (or give) heat
 *     through every face that touches them, which is how a hot piece of steel slowly cools down.
 *
 * The conduction pass reads the old temperatures of the whole grid and only then applies the changes, so
 * heat cannot race along a row in one tick. Whole rows that hold nothing but ambient-temperature cells are
 * skipped, which keeps a calm world cheap. Results are rounded at random (with the right odds), so even a
 * one degree difference keeps flowing instead of freezing at "close enough".
 *
 * On top of the temperatures sit the phase changes: water boils and freezes, ice melts, lava sets into
 * stone, stone melts into lava, flammable things light up by themselves. They are listed in k_thermal. */
#include <stdlib.h>

#include "sandbox_internal.h"

#define THERMAL_PASSES 2 /* conduction passes per tick: heat travels faster than one cell per tick */
#define BURNING_TEMP 600 /* a burning solid is a heat source at about this temperature */

#define NC INT16_MIN, EL_EMPTY /* never cold enough to change */
#define NH INT16_MAX, EL_EMPTY /* never hot enough to change */

/* Columns:  temp0, cond, cap, loss, autoignite, (cold_t, cold_to), (hot_t, hot_to), rate, fixed, latent_hot,
 * latent_cold
 *
 * cond / cap : conductivity and heat capacity. Steel 62/1 heats and cools very fast, water 20/6 slowly, brick
 *              3/6 hardly passes heat on at all.
 * loss       : heat lost to open air per exposed face, 1/16384 of the temperature difference per tick
 * autoignite : flammable things light up by themselves above this temperature
 * rate       : phase changes happen with this chance (/256) per tick once the temperature is reached
 * fixed      : 1 = the cell is a heat source or sink and always sits at temp0
 * latent_*   : water that reaches 100 degrees does not boil at once: it stays at 100 while it soaks up heat
 *              (kept in aux, in units of 16) and only turns to steam when it has taken enough. Ice melting
 *              and water freezing work the same way at 0 degrees. 0 = the change is immediate. */
static const ThermalDef k_thermal[EL_COUNT] = {
    [EL_EMPTY] = {SANDBOX_AMBIENT, 0, 1, 0, 0, NC, NH, 0, 0, 0, 0},

    [EL_SAND] = {20, 8, 4, 24, 0, NC, 1100, EL_GLASS, 40, 0, 0, 0},
    [EL_WATER] = {20, 6, 8, 20, 0, 0, EL_ICE, 100, EL_STEAM, 255, 0, 110, 60},
    [EL_STONE] = {20, 14, 5, 20, 0, NC, 1300, EL_LAVA, 20, 0, 0, 0},

    [EL_SOIL] = {20, 6, 5, 24, 0, NC, NH, 0, 0, 0, 0},
    [EL_SNOW] = {-5, 4, 3, 24, 0, NC, 0, EL_WATER, 255, 0, 30, 0},
    [EL_GUNPOWDER] = {20, 6, 3, 24, 260, NC, NH, 0, 0, 0, 0},
    [EL_SEED] = {20, 5, 3, 24, 260, NC, NH, 0, 0, 0, 0},
    [EL_ASH] = {20, 4, 2, 24, 0, NC, NH, 0, 0, 0, 0},
    [EL_THERMITE] = {20, 10, 4, 24, 700, NC, NH, 0, 0, 0, 0},
    [EL_GRAVEL] = {20, 10, 4, 24, 0, NC, 1300, EL_LAVA, 20, 0, 0, 0},

    [EL_OIL] = {20, 8, 4, 24, 280, NC, NH, 0, 0, 0, 0},
    [EL_ACID] = {20, 14, 5, 24, 0, NC, NH, 0, 0, 0, 0},
    [EL_LAVA] = {1150, 22, 4, 250, 0, 500, EL_BASALT, NH, 30, 0, 0,
                 0}, /* cools through red and sets into basalt */
    [EL_MERCURY] = {20, 56, 3, 20, 0, NC, NH, 0, 0, 0, 0},
    [EL_NITROGEN] = {-196, 12, 4, 40, 0, NC, -150, EL_EMPTY, 90, 0, 0, 0},

    [EL_STEAM] = {110, 6, 1, 6, 0, 96, EL_WATER, NH, 40, 0, 0, 0},
    [EL_METHANE] = {20, 6, 1, 20, 480, NC, NH, 0, 0, 0, 0},
    [EL_SMOKE] = {20, 6, 1, 20, 0, NC, NH, 0, 0, 0, 0},

    [EL_WOOD] = {20, 4, 4, 20, 300, NC, NH, 0, 0, 0, 0},
    [EL_ICE] = {-10, 10, 4, 20, 0, NC, 0, EL_WATER, 255, 0, 60, 0},
    [EL_GLASS] = {20, 10, 4, 20, 0, NC, 1700, EL_LAVA, 10, 0, 0, 0},
    [EL_WALL] = {20, 2, 8, 4, 0, NC, NH, 0, 0, 0, 0},
    [EL_STEEL] = {20, 62, 1, 30, 0, NC, 1500, EL_LAVA, 20, 0, 0, 0},
    [EL_BRICK] = {20, 3, 6, 4, 0, NC, 1900, EL_LAVA, 10, 0, 0, 0},
    [EL_PLANT] = {20, 8, 5, 24, 320, NC, NH, 0, 0, 0, 0},
    [EL_FUSE] = {20, 5, 2, 24, 250, NC, NH, 0, 0, 0, 0},
    [EL_TNT] = {20, 6, 4, 24, 330, NC, NH, 0, 0, 0, 0},

    [EL_FIRE] = {800, 24, 1, 30, 0, NC, NH, 0, 1, 0, 0},
    [EL_HEATER] = {700, 40, 10, 4, 0, NC, NH, 0, 1, 0, 0},
    [EL_COOLER] = {-80, 40, 10, 4, 0, NC, NH, 0, 1, 0, 0},
    [EL_URANIUM] = {20, 30, 4, 160, 0, NC, 1000, EL_MOLTEN_URANIUM, 60, 0, 0,
                    0}, /* heats itself, melts at 1000 */
    [EL_TERMITE] = {20, 6, 3, 24, 300, NC, NH, 0, 0, 0, 0},
    [EL_LIFE] = {20, 6, 2, 24, 0, NC, NH, 0, 0, 0, 0},

    [EL_GAMMA] = {20, 0, 1, 0, 0, NC, NH, 0, 0, 0, 0},
    [EL_MOLTEN_URANIUM] = {1000, 30, 4, 160, 0, 800, EL_URANIUM, NH, 20, 0, 0, 0},
    [EL_SPARK] = {1200, 24, 1, 0, 0, NC, NH, 0, 1, 0, 0},
    [EL_BASALT] = {20, 14, 5, 80, 0, NC, 1100, EL_LAVA, 20, 0, 0, 0},
};

const ThermalDef *sandbox_thermal(ElementId id) {
    if ((int)id < 0 || id >= EL_COUNT)
        return &k_thermal[EL_EMPTY];
    return &k_thermal[id];
}

/* ---- bookkeeping ---- */

static int clamp_temp(int t) {
    return arc_clampi(t, SANDBOX_TEMP_MIN, SANDBOX_TEMP_MAX);
}

void sandbox_heat_clear(Sandbox *sb) {
    for (int i = 0; i < SANDBOX_MAX_W * SANDBOX_MAX_H; i++)
        sb->temp[i] = SANDBOX_AMBIENT;
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
 * again). */
void sandbox_heat_on_put(Sandbox *sb, int i, ElementId id) {
    int t = sb->temp[i];
    if (id == EL_EMPTY) {
        sb->temp[i] = SANDBOX_AMBIENT;
        return;
    }
    const ThermalDef *d = sandbox_thermal(id);
    if (d->fixed)
        t = d->temp0;
    else if (d->temp0 > 100)
        t = arc_maxi(t, d->temp0);
    else if (d->temp0 < 0)
        t = arc_mini(t, d->temp0);
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
    sb->temp[i] = (int16_t)clamp_temp(t);
    if (sb->temp[i] != SANDBOX_AMBIENT)
        sandbox_heat_mark(sb, i / sb->w);
}

int sandbox_temp(const Sandbox *sb, int x, int y) {
    if (!sandbox_inside(sb, x, y))
        return SANDBOX_AMBIENT;
    int i = y * sb->w + x;
    return sb->cells[i] == EL_EMPTY ? SANDBOX_AMBIENT : sb->temp[i];
}

void sandbox_set_temp(Sandbox *sb, int x, int y, int degrees) {
    if (!sandbox_inside(sb, x, y))
        return;
    int i = y * sb->w + x;
    if (sb->cells[i] == EL_EMPTY)
        return;
    sb->temp[i] = (int16_t)clamp_temp(degrees);
    sandbox_heat_mark(sb, y);
}

void sandbox_add_heat(Sandbox *sb, int x, int y, int degrees) {
    sandbox_set_temp(sb, x, y, sandbox_temp(sb, x, y) + degrees);
}

/* ---- conduction ---- */

/* How well two neighbouring elements conduct, 0..62: like two resistors in series. */
static int pair_cond(ElementId a, ElementId b) {
    static uint8_t table[EL_COUNT][EL_COUNT];
    static bool ready = false;
    if (!ready) {
        for (int p = 0; p < EL_COUNT; p++) {
            for (int q = 0; q < EL_COUNT; q++) {
                int ca = k_thermal[p].cond, cb = k_thermal[q].cond;
                table[p][q] = (uint8_t)(ca + cb == 0 ? 0 : 2 * ca * cb / (ca + cb));
            }
        }
        ready = true;
    }
    return table[a][b];
}

/* Heat that flows from a cell at ta to one at tb through a connection of conductivity g, rounded at random so
 * that small differences still move heat on average. */
static int flow(Sandbox *sb, int ta, int tb, int g) {
    int diff = ta - tb;
    if (diff == 0 || g == 0)
        return 0;
    int num = diff * g;
    int q = num / 256;
    int rem = abs(num) % 256;
    if (rem != 0 && (int)(rng_u32(&sb->rng) & 255u) < rem)
        q += diff > 0 ? 1 : -1;
    return q;
}

/* temperature change for a cell that received `heat` units, rounded at random */
static int warm(Sandbox *sb, int heat, int cap) {
    if (heat == 0)
        return 0;
    int dt = heat / cap;
    int rem = heat - dt * cap;
    if (rem != 0 && (int)(rng_u32(&sb->rng) % (uint32_t)cap) < abs(rem))
        dt += rem > 0 ? 1 : -1;
    return dt;
}

static bool is_burning_solid(const Sandbox *sb, int i) {
    ElementId id = (ElementId)sb->cells[i];
    return (id == EL_WOOD || id == EL_PLANT || id == EL_FUSE || id == EL_OIL) && (sb->aux[i] & BURN_BIT);
}

void sandbox_heat_step(Sandbox *sb) {
    int w = sb->w, h = sb->h;
    bool near_hot[SANDBOX_MAX_H]; /* the row, or one right next to it, may hold heat */
    bool any = false;
    for (int y = 0; y < h; y++) {
        near_hot[y] =
            sb->heat_rows[y] || (y > 0 && sb->heat_rows[y - 1]) || (y + 1 < h && sb->heat_rows[y + 1]);
        any = any || near_hot[y];
    }
    if (!any)
        return;

    for (int pass = 0; pass < THERMAL_PASSES; pass++) {
        for (int y = 0; y < h; y++) {
            if (!near_hot[y])
                continue;
            for (int x = 0; x < w; x++)
                sb->heat_flow[y * w + x] = 0;
        }
        for (int y = 0; y < h; y++) {
            if (!near_hot[y])
                continue;
            for (int x = 0; x < w; x++) {
                int i = y * w + x;
                ElementId a = (ElementId)sb->cells[i];
                if (a == EL_EMPTY)
                    continue;
                int ta = sb->temp[i];
                for (int k = 0; k < 2; k++) { /* the neighbour to the right, then the one below */
                    int j;
                    if (k == 0) {
                        if (x + 1 >= w)
                            continue;
                        j = i + 1;
                    } else {
                        if (y + 1 >= h || !near_hot[y + 1])
                            continue;
                        j = i + w;
                    }
                    ElementId b = (ElementId)sb->cells[j];
                    if (b == EL_EMPTY)
                        continue;
                    int q = flow(sb, ta, sb->temp[j], pair_cond(a, b));
                    if (q != 0) {
                        sb->heat_flow[i] = (int16_t)(sb->heat_flow[i] - q);
                        sb->heat_flow[j] = (int16_t)(sb->heat_flow[j] + q);
                    }
                }
            }
        }
        /* apply the flows, cool through the open air, and hold the sources at their temperature */
        for (int y = 0; y < h; y++) {
            if (!near_hot[y])
                continue;
            bool row_hot = false;
            for (int x = 0; x < w; x++) {
                int i = y * w + x;
                ElementId id = (ElementId)sb->cells[i];
                if (id == EL_EMPTY) {
                    sb->temp[i] = SANDBOX_AMBIENT;
                    continue;
                }
                const ThermalDef *d = &k_thermal[id];
                int t = sb->temp[i] + warm(sb, sb->heat_flow[i], d->cap);
                int faces = 0; /* sides open to the air */
                if (x > 0 && sb->cells[i - 1] == EL_EMPTY)
                    faces++;
                if (x + 1 < w && sb->cells[i + 1] == EL_EMPTY)
                    faces++;
                if (y > 0 && sb->cells[i - w] == EL_EMPTY)
                    faces++;
                if (y + 1 < h && sb->cells[i + w] == EL_EMPTY)
                    faces++;
                if (faces > 0 && t != SANDBOX_AMBIENT) {
                    int num = (SANDBOX_AMBIENT - t) * d->loss * faces;
                    int den = 16384 * d->cap;
                    int dt = num / den;
                    int rem = num - dt * den;
                    if (rem != 0 && (int)(rng_u32(&sb->rng) % (uint32_t)den) < abs(rem))
                        dt += rem > 0 ? 1 : -1;
                    t += dt;
                }
                if (d->fixed)
                    t = d->temp0;
                else if (is_burning_solid(sb, i))
                    t = arc_maxi(t, BURNING_TEMP);
                t = clamp_temp(t);
                sb->temp[i] = (int16_t)t;
                if (t != SANDBOX_AMBIENT)
                    row_hot = true;
            }
            sb->heat_rows[y] = row_hot ? 1 : 0;
        }
        /* a row that just became hot is re-checked next pass through its neighbours */
        for (int y = 0; y < h; y++)
            near_hot[y] =
                sb->heat_rows[y] || (y > 0 && sb->heat_rows[y - 1]) || (y + 1 < h && sb->heat_rows[y + 1]);
    }
}

/* ---- phase changes and self-ignition ---- */

/* Phase change at a threshold with latent heat: the cell is held at the threshold temperature and everything
 * that would have heated it further goes into aux until the cell has soaked up `latent` units. */
static bool soak(Sandbox *sb, int i, int over, int cap, int latent, int *aux_acc) {
    int units = over * cap; /* heat beyond the threshold */
    int add = units / 16, rem = units % 16;
    if (rem != 0 && (int)(rng_u32(&sb->rng) & 15u) < rem)
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
    if (has_hot && (d->latent_hot ? t > d->hot_t : t >= d->hot_t)) {
        if (d->latent_hot) {
            int acc;
            bool done = soak(sb, i, t - d->hot_t, d->cap, d->latent_hot, &acc);
            sb->temp[i] = d->hot_t; /* the extra heat went into the change, not into the temperature */
            sb->aux[i] = (uint8_t)acc;
            if (done) {
                sandbox_put(sb, x, y, d->hot_to, 0);
                return true;
            }
        } else if ((int)(rng_u32(&sb->rng) & 255u) < d->rate) {
            sandbox_put(sb, x, y, d->hot_to, 0);
            return true;
        }
    } else if (has_cold && (d->latent_cold ? t < d->cold_t : t <= d->cold_t)) {
        if (d->latent_cold) {
            int acc;
            bool done = soak(sb, i, d->cold_t - t, d->cap, d->latent_cold, &acc);
            sb->temp[i] = d->cold_t;
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
