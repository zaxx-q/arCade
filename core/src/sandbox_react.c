/* Chemistry. sandbox_react() runs once per cell per tick, right before the cell moves (see sandbox_step).
 * It may change the cell, its neighbours, or both; a cell that was changed is marked "moved" by sandbox_put()
 * and then waits for the next tick, so a reaction never chains through the whole grid in one tick.
 * Temperature (phase changes, self-ignition) lives in sandbox_heat.c and explosions in sandbox_blast.c.
 *
 * Per-cell memory (Sandbox.aux), by element:
 *   Fire      age in ticks
 *   Wood, Plant, Fuse, Oil
 *             bit 7 = burning; low 7 bits = burn timer. A Plant that is not burning keeps its growth budget
 *             there
 *   TNT       0 = idle, otherwise the countdown to the blast
 *   Lava      0 = plain lava; >0 = thermite melt power that wears off and lets it eat through solids
 *   Uranium   none: its heat is its temperature. moved bit EXCITED = hit by a gamma ray, splits next turn
 *   Gamma     direction (top 3 bits) and remaining flight (low 5 bits)
 *   Termite   hunger
 * Tuning numbers sit in the #defines so they can be found in one place. */
#include <string.h>

#include "sandbox_internal.h"

#define THERMITE_POWER 110 /* melt power of freshly ignited thermite */
#define TNT_DELAY 4        /* ticks between catching fire and blowing up */
#define TNT_POWER 56       /* a TNT cell blows with this much energy: a crater a couple of dozen cells wide */
#define FUSE_DELAY 6       /* ticks a burning fuse cell waits before it lights the next cell */
#define FUSE_LENGTH 10

#define URANIUM_CRIT 900 /* degrees at which a lump goes critical (lava gets it there; a heater does not) */
#define NUKE_MIN 12      /* a critical lump this big (cells) detonates; a smaller one only melts */
#define FISSION_HEAT 110 /* degrees a cell warms up each time it splits */
#define GAMMA_WARM 30    /* ... and when a ray hits it without making it split */
#define GAMMA_HIT 140    /* chance (/256) that a gamma ray makes uranium split */
#define GAMMA_LIFE 22    /* how many ticks a ray flies at most */

static int cell_at(const Sandbox *sb, int x, int y) {
    return y * sb->w + x;
}

static bool burning(const Sandbox *sb, int i) {
    ElementId id = (ElementId)sb->cells[i];
    return (id == EL_WOOD || id == EL_PLANT || id == EL_FUSE || id == EL_OIL) && (sb->aux[i] & BURN_BIT);
}

bool sandbox_flammable_now(const Sandbox *sb, int i) {
    ElementId id = (ElementId)sb->cells[i];
    if (sandbox_element(id)->ignite == 0)
        return false;
    if (id == EL_TNT)
        return sb->aux[i] == 0;
    return !burning(sb, i);
}

/* Is any of the 4 neighbours a heat source? A source that was only created or moved this tick does not count
 * yet, so a flame front crosses a gas cloud or a row of fuse one step per tick, not the whole row at once.
 * A fuse is never lit by the fuse next to it: that spread is timed (see react_burning). */
static bool hot_near(const Sandbox *sb, int x, int y, ElementId self) {
    for (int dir = 0; dir < 8; dir += 2) {
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (!sandbox_inside(sb, nx, ny))
            continue;
        int n = cell_at(sb, nx, ny);
        if (sb->moved[n] & MOVED_NOW)
            continue;
        ElementId id = (ElementId)sb->cells[n];
        if (sandbox_element(id)->flags & EF_HOT)
            return true;
        if (burning(sb, n) && !(self == EL_FUSE && id == EL_FUSE))
            return true;
    }
    return false;
}

static bool cold_near(const Sandbox *sb, int x, int y) {
    for (int dir = 0; dir < 8; dir += 2) {
        ElementId id = sandbox_at(sb, x + sandbox_dx(dir), y + sandbox_dy(dir));
        if (id == EL_ICE || id == EL_SNOW || id == EL_NITROGEN)
            return true;
    }
    return false;
}

void sandbox_ignite(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    switch ((ElementId)sb->cells[i]) {
    case EL_WOOD:
    case EL_PLANT:
    case EL_FUSE:
    case EL_OIL: /* burning oil keeps flowing, and keeps burning while it does */
        sb->aux[i] = (uint8_t)(BURN_BIT | 1u); /* a burning plant forgets its growth budget */
        sb->moved[i] |= MOVED_NOW;
        break;
    case EL_TNT:
        sb->aux[i] = 1;
        sb->moved[i] |= MOVED_NOW;
        break;
    case EL_THERMITE:
        sandbox_put(sb, x, y, EL_LAVA, THERMITE_POWER);
        sandbox_set_temp(sb, x, y, 2400); /* far hotter than ordinary lava */
        break;
    case EL_METHANE:
        sandbox_put(sb, x, y, EL_FIRE, 0);
        if (sandbox_chance(sb, 16))
            sandbox_blast(sb, x, y, 5); /* now and then a pocket of gas goes off with a pop */
        break;
    default:
        sandbox_put(sb, x, y, EL_FIRE, 0);
        break;
    }
}

/* ---- gamma rays and uranium ---- */

static bool spawn_gamma(Sandbox *sb, int x, int y, int dir) {
    int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
    if (!sandbox_inside(sb, nx, ny) || sb->cells[cell_at(sb, nx, ny)] != EL_EMPTY)
        return false;
    sandbox_put(sb, nx, ny, EL_GAMMA, (uint8_t)((dir << 5) | (GAMMA_LIFE - 3 + sandbox_rand(sb, 6))));
    return true;
}

/* A uranium atom splits: 2 or 3 rays fly out. A ray that has no room (the cell beside it is solid) goes
 * straight into that neighbour, so a solid block can pass the reaction on from the inside. */
static void fission(Sandbox *sb, int x, int y) {
    int rays = 2 + (sandbox_chance(sb, 150) ? 1 : 0);
    for (int r = 0; r < rays; r++) {
        int dir = sandbox_rand(sb, 8);
        if (spawn_gamma(sb, x, y, dir))
            continue;
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (sandbox_inside(sb, nx, ny))
            sandbox_gamma_hit(sb, nx, ny);
    }
    sandbox_add_heat(sb, x, y, FISSION_HEAT);
}

void sandbox_gamma_hit(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    switch ((ElementId)sb->cells[i]) {
    case EL_URANIUM:
        if (sandbox_chance(sb, GAMMA_HIT))
            sb->moved[i] |= EXCITED; /* splits on its own turn, so the reaction spreads one cell per tick */
        else
            sandbox_add_heat(sb, x, y, GAMMA_WARM);
        break;
    case EL_WATER:
        if (sandbox_chance(sb, 10))
            sandbox_put(sb, x, y, EL_STEAM, 0); /* water soaks up rays and slowly boils */
        break;
    case EL_TERMITE:
        sandbox_put(sb, x, y, EL_EMPTY, 0);
        break;
    default:
        break;
    }
}

/* A critical lump of uranium. Everything uranium that touches it, 8 ways, is part of the same lump. The lump
 * is gathered with a flood fill (BLASTED marks the cells already visited) and then goes up at once: all of it
 * vanishes and the blast is as strong as the lump is big, so a few dozen cells already wipe out a whole
 * corner of the world and a huge heap takes everything with it. A few cells are left behind as hot
 * molten fallout. A lump smaller than NUKE_MIN only melts. */
static void meltdown(Sandbox *sb, int x0, int y0) {
    static int stack[SANDBOX_MAX_W * SANDBOX_MAX_H];
    int top = 0, n = 0;
    long sx = 0, sy = 0;
    int start = cell_at(sb, x0, y0);
    stack[top++] = start;
    sb->moved[start] |= BLASTED;
    int head = 0;
    while (head < top) {
        int i = stack[head++];
        n++;
        sx += i % sb->w;
        sy += i / sb->w;
        for (int dir = 0; dir < 8; dir++) {
            int nx = i % sb->w + sandbox_dx(dir), ny = i / sb->w + sandbox_dy(dir);
            if (!sandbox_inside(sb, nx, ny))
                continue;
            int j = cell_at(sb, nx, ny);
            ElementId id = (ElementId)sb->cells[j];
            if ((id == EL_URANIUM || id == EL_MOLTEN_URANIUM) && !(sb->moved[j] & BLASTED) &&
                top < (int)(sizeof stack / sizeof stack[0])) {
                sb->moved[j] |= BLASTED;
                stack[top++] = j;
            }
        }
    }
    if (n < NUKE_MIN) {
        sandbox_put(sb, x0, y0, EL_MOLTEN_URANIUM, 0);
        return;
    }
    int cx = (int)(sx / n), cy = (int)(sy / n);
    int fallout = 0;
    for (int k = 0; k < n; k++) {
        int i = stack[k];
        int x = i % sb->w, y = i / sb->w;
        if (sandbox_chance(sb, 30) && fallout < 60) {
            fallout++;
            sandbox_put(sb, x, y, EL_MOLTEN_URANIUM, 0);
        } else {
            sandbox_put(sb, x, y, EL_FIRE, 0);
        }
    }
    int power = 18;
    int root = 0;
    while ((root + 1) * (root + 1) <= n)
        root++;
    power += 7 * root;
    sandbox_blast(sb, cx, cy, power);
}

static void react_uranium(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    if (sb->moved[i] & EXCITED) {
        sb->moved[i] &= (uint8_t)~EXCITED;
        fission(sb, x, y);
    } else if ((rng_u32(&sb->rng) & 1023u) == 0u) {
        fission(sb, x, y); /* spontaneous decay: a lump of uranium crackles quietly on its own */
    }
    if (sb->temp[i] >= URANIUM_CRIT)
        meltdown(sb, x, y);
}

static void react_molten(Sandbox *sb, int x, int y) {
    if (sandbox_chance(sb, 4))
        spawn_gamma(sb, x, y, sandbox_rand(sb, 8));
    for (int dir = 0; dir < 8; dir += 2) {
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (sandbox_at(sb, nx, ny) == EL_WATER && sandbox_chance(sb, 90)) {
            sandbox_put(sb, nx, ny, EL_STEAM, 0);
            sandbox_put(sb, x, y, EL_URANIUM, 0); /* quenched: it freezes back into a lump */
            sandbox_set_temp(sb, x, y, 150);
            return;
        }
    }
}

/* ---- lava ---- */

/* Things thermite lava can eat through: solids and heaps, but not walls. */
static bool meltable(ElementId id) {
    const ElementDef *d = sandbox_element(id);
    return id != EL_EMPTY && id != EL_LAVA && d->density < 200 &&
           (d->kind == MOVE_NONE || d->kind == MOVE_POWDER);
}

static void react_lava(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    int power = sb->aux[i];
    int start = sandbox_rand(sb, 4);
    for (int k = 0; k < 4; k++) {
        int dir = ((start + k) & 3) * 2;
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        ElementId t = sandbox_at(sb, nx, ny);
        if (t == EL_WATER && sandbox_chance(sb, 110)) {
            sandbox_put(sb, nx, ny, EL_STEAM, 0);
            sandbox_put(sb, x, y, EL_STONE, 0); /* lava that meets water cools into rock */
            return;
        }
        if (t == EL_SAND && sandbox_chance(sb, 28)) {
            sandbox_put(sb, nx, ny, EL_GLASS, 0); /* hot lava fuses sand into glass */
        } else if (power > 24 && meltable(t) && t != EL_WOOD && sandbox_chance(sb, 60)) {
            sandbox_put(sb, nx, ny, EL_LAVA,
                        (uint8_t)(power - 24)); /* thermite burns on through the material */
        }
    }
    if (power > 0) {
        sb->aux[i] = (uint8_t)(power - 1);
        if (sandbox_chance(sb, 30) && sandbox_at(sb, x, y - 1) == EL_EMPTY)
            sandbox_put(sb, x, y - 1, EL_FIRE, 0);
    }
}

/* ---- fire and burning solids ---- */

static void react_fire(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    bool fuel_near = false;
    for (int dir = 0; dir < 8; dir += 2) {
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (!sandbox_inside(sb, nx, ny))
            continue;
        ElementId t = (ElementId)sb->cells[cell_at(sb, nx, ny)];
        if (t == EL_WATER && sandbox_chance(sb, 150)) {
            if (sandbox_chance(sb, 60))
                sandbox_put(sb, nx, ny, EL_STEAM, 0); /* a little of the water boils off */
            sandbox_put(sb, x, y, EL_EMPTY, 0);
            return;
        }
        if (sandbox_flammable_now(sb, cell_at(sb, nx, ny)))
            fuel_near = true;
    }
    int age = sb->aux[i];
    if (age < 255)
        sb->aux[i] = (uint8_t)(age + 1);
    if (age >= 3 && sandbox_chance(sb, 44)) {
        sandbox_put(sb, x, y, sandbox_chance(sb, 60) ? EL_SMOKE : EL_EMPTY, 0);
        return;
    }
    if (fuel_near && sandbox_chance(sb, 170))
        sb->moved[i] |= MOVED_NOW; /* a flame clings to what it is about to burn instead of drifting away */
}

static void react_burning(Sandbox *sb, int x, int y, ElementId id) {
    int i = cell_at(sb, x, y);
    int timer = BURN_TIMER(sb->aux[i]);
    int flame = id == EL_WOOD ? 40 : id == EL_PLANT ? 80 : id == EL_OIL ? 90 : 100;

    if (sandbox_chance(sb, flame)) { /* throw a flame, preferably upwards */
        int nx = x, ny = y - 1;
        if (sandbox_at(sb, nx, ny) != EL_EMPTY || sandbox_chance(sb, 80)) {
            int dir = sandbox_rand(sb, 4) * 2;
            nx = x + sandbox_dx(dir);
            ny = y + sandbox_dy(dir);
        }
        if (sandbox_at(sb, nx, ny) == EL_EMPTY)
            sandbox_put(sb, nx, ny, EL_FIRE, 0);
    }
    if (id != EL_FUSE && sandbox_chance(sb, 10) && sandbox_at(sb, x, y - 1) == EL_EMPTY)
        sandbox_put(sb, x, y - 1, EL_SMOKE, 0);

    if (id == EL_FUSE && timer == FUSE_DELAY) { /* the flame creeps on to the next piece of fuse or charge */
        for (int dir = 0; dir < 8; dir += 2) {
            int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
            if (sandbox_inside(sb, nx, ny) && sandbox_flammable_now(sb, cell_at(sb, nx, ny)) &&
                sandbox_element(sandbox_at(sb, nx, ny))->ignite >= 40)
                sandbox_ignite(sb, nx, ny);
        }
    }
    if (timer < 127)
        timer++;
    sb->aux[i] = (uint8_t)(BURN_BIT | timer);

    bool done = false;
    switch (id) {
    case EL_WOOD:
        done = timer >= 30 && sandbox_chance(sb, 3);
        break;
    case EL_PLANT:
        done = timer >= 6 && sandbox_chance(sb, 40);
        break;
    case EL_OIL:
        done = timer >= 10 && sandbox_chance(sb, 16);
        break;
    default:
        done = timer >= FUSE_LENGTH;
        break;
    }
    if (!done)
        return;
    if (id == EL_WOOD)
        sandbox_put(sb, x, y, sandbox_chance(sb, 150) ? EL_ASH : EL_FIRE, 0);
    else if (id == EL_PLANT)
        sandbox_put(sb, x, y, sandbox_chance(sb, 110) ? EL_ASH : EL_SMOKE, 0);
    else if (id == EL_OIL)
        sandbox_put(sb, x, y, sandbox_chance(sb, 128) ? EL_FIRE : EL_SMOKE, 0);
    else
        sandbox_put(sb, x, y, EL_FIRE, 0);
}

/* ---- plants ---- */

static int plant_neighbours(const Sandbox *sb, int x, int y) {
    int n = 0;
    for (int dir = 0; dir < 8; dir += 2)
        if (sandbox_at(sb, x + sandbox_dx(dir), y + sandbox_dy(dir)) == EL_PLANT)
            n++;
    return n;
}

/* A plant has a growth budget (aux). It drinks water to get more, and spends it growing a new cell upwards or
 * sideways. A child takes half of what is left, so a seed grows a bush of a limited size, not a forest. */
static void react_plant(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    int budget = sb->aux[i] & 0x7F;

    if (sandbox_chance(sb, 18)) {
        for (int dir = 0; dir < 8; dir += 2) {
            int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
            if (sandbox_at(sb, nx, ny) == EL_WATER) {
                sandbox_put(sb, nx, ny, EL_EMPTY, 0);
                budget = arc_mini(100, budget + 6);
                sb->aux[i] = (uint8_t)budget;
                break;
            }
        }
    }
    if (budget <= 0 || !sandbox_chance(sb, 22))
        return;
    int pick = sandbox_rand(sb, 10);
    int dir = pick < 6 ? 6 : pick < 8 ? 4 : 0; /* mostly up, sometimes sideways */
    int tx = x + sandbox_dx(dir), ty = y + sandbox_dy(dir);
    if (sandbox_at(sb, tx, ty) != EL_EMPTY || plant_neighbours(sb, tx, ty) != 1)
        return; /* keeps the stems thin */
    int child = (budget - 1) / 2;
    sandbox_put(sb, tx, ty, EL_PLANT, (uint8_t)child);
    sb->aux[i] = (uint8_t)(budget - 1 - child);
}

/* ---- the termite ---- */

static bool is_food(ElementId id) {
    return id == EL_WOOD || id == EL_PLANT || id == EL_FUSE;
}

/* Termites walk on surfaces: a free cell is only worth stepping into when something solid touches it. */
static bool has_anchor(const Sandbox *sb, int x, int y, int from_x, int from_y) {
    for (int dir = 0; dir < 8; dir++) {
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (nx == from_x && ny == from_y)
            continue;
        ElementId id = sandbox_at(sb, nx, ny);
        if (id != EL_EMPTY && !sandbox_is_fluid(id))
            return true;
    }
    return false;
}

static void react_termite(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    int hunger = sb->aux[i];
    if (hunger < 255 && sandbox_chance(sb, 12))
        hunger++;
    if (hunger >= 255) {
        sandbox_put(sb, x, y, EL_EMPTY, 0); /* starved */
        return;
    }
    sb->aux[i] = (uint8_t)hunger;

    int start = sandbox_rand(sb, 8);
    for (int k = 0; k < 8; k++) {
        int dir = (start + k) & 7;
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        ElementId t = sandbox_at(sb, nx, ny);
        if (t == EL_WATER && (dir & 1) == 0 && sandbox_chance(sb, 4)) {
            sandbox_put(sb, x, y, EL_EMPTY, 0); /* drowned */
            return;
        }
        if (is_food(t) && sandbox_chance(sb, 60)) { /* chew: sometimes the meal becomes another termite */
            sandbox_put(sb, nx, ny, sandbox_chance(sb, 55) ? EL_TERMITE : EL_EMPTY, 0);
            sb->aux[i] = 0;
            return;
        }
    }
    if (!sandbox_chance(sb, 170))
        return;
    for (int k = 0; k < 8; k++) {
        int dir = (start + k) & 7;
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (sandbox_at(sb, nx, ny) == EL_EMPTY && sandbox_inside(sb, nx, ny) &&
            has_anchor(sb, nx, ny, x, y)) {
            sandbox_swap(sb, i, cell_at(sb, nx, ny));
            return;
        }
    }
}

/* ---- liquids and gases with chemistry ---- */

static bool dissolvable(ElementId id) {
    const ElementDef *d = sandbox_element(id);
    if (id == EL_EMPTY || id == EL_ACID || (d->flags & EF_ACIDPROOF))
        return false;
    return d->kind == MOVE_NONE || d->kind == MOVE_POWDER || d->kind == MOVE_CRAWL;
}

static void react_acid(Sandbox *sb, int x, int y) {
    int dir = sandbox_rand(sb, 4) * 2;
    int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
    ElementId t = sandbox_at(sb, nx, ny);
    if (dissolvable(t) && sandbox_inside(sb, nx, ny) && sandbox_chance(sb, 60)) {
        sandbox_put(sb, nx, ny, sandbox_chance(sb, 30) ? EL_SMOKE : EL_EMPTY, 0);
        if (sandbox_chance(sb, 64))
            sandbox_put(sb, x, y, EL_EMPTY, 0); /* the acid is used up in the process */
    } else if (t == EL_WATER && sandbox_chance(sb, 8)) {
        sandbox_put(sb, x, y, EL_WATER, 0); /* diluted */
    }
}

static void react_nitrogen(Sandbox *sb, int x, int y) {
    for (int dir = 0; dir < 8; dir += 2) {
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        ElementId t = sandbox_at(sb, nx, ny);
        bool hit = true;
        if (t == EL_WATER)
            sandbox_put(sb, nx, ny, EL_ICE, 0);
        else if (t == EL_LAVA)
            sandbox_put(sb, nx, ny, EL_STONE, 0);
        else if (t == EL_MOLTEN_URANIUM)
            sandbox_put(sb, nx, ny, EL_URANIUM, 0);
        else if (t == EL_FIRE)
            sandbox_put(sb, nx, ny, EL_EMPTY, 0);
        else if (t == EL_STEAM)
            sandbox_put(sb, nx, ny, EL_WATER, 0);
        else
            hit = false;
        if (hit) {
            if (sandbox_chance(sb, 70))
                sandbox_put(sb, x, y, EL_EMPTY, 0); /* it boils away as it cools things */
            return;
        }
    }
}

/* Convection: in a liquid, a warmer cell rises through the cooler cell of the same liquid above it, so a pot
 * heated from below warms up all the way to the top. */
static void convect(Sandbox *sb, int x, int y) {
    if (y == 0)
        return;
    int i = cell_at(sb, x, y), up = cell_at(sb, x, y - 1);
    if (sb->cells[up] == sb->cells[i] && sb->temp[up] + 3 <= sb->temp[i] && sandbox_chance(sb, 70))
        sandbox_swap(sb, i, up);
}

/* ---- the dispatcher ---- */

void sandbox_react(Sandbox *sb, int x, int y) {
    int i = cell_at(sb, x, y);
    ElementId id = (ElementId)sb->cells[i];
    const ElementDef *d = sandbox_element(id);

    if (sandbox_heat_react(sb, x, y))
        return; /* it boiled, froze, melted or caught fire because of its temperature */

    if (d->kind == MOVE_LIQUID) {
        convect(sb, x, y);
        if (sb->moved[i] & MOVED_NOW)
            return;
    }

    if (d->ignite && sandbox_flammable_now(sb, i) && hot_near(sb, x, y, id) &&
        sandbox_chance(sb, d->ignite)) {
        sandbox_ignite(sb, x, y);
        return;
    }

    switch (id) {
    case EL_FIRE:
        react_fire(sb, x, y);
        break;
    case EL_WOOD:
    case EL_FUSE:
    case EL_OIL:
        if (sb->aux[i] & BURN_BIT)
            react_burning(sb, x, y, id);
        break;
    case EL_PLANT:
        if (sb->aux[i] & BURN_BIT)
            react_burning(sb, x, y, id);
        else
            react_plant(sb, x, y);
        break;
    case EL_TNT:
        if (sb->aux[i] > 0) {
            if (sb->aux[i] < TNT_DELAY)
                sb->aux[i]++;
            if (sb->aux[i] >= TNT_DELAY &&
                sb->blast_budget > 0) { /* a huge stack goes off over a few ticks */
                sb->blast_budget--;
                sandbox_put(sb, x, y, EL_FIRE, 0);
                sandbox_blast(sb, x, y, TNT_POWER);
            }
        }
        break;
    case EL_LAVA:
        react_lava(sb, x, y);
        break;
    case EL_MOLTEN_URANIUM:
        react_molten(sb, x, y);
        break;
    case EL_URANIUM:
        react_uranium(sb, x, y);
        break;
    case EL_ACID:
        react_acid(sb, x, y);
        break;
    case EL_NITROGEN:
        react_nitrogen(sb, x, y);
        break;
    case EL_TERMITE:
        react_termite(sb, x, y);
        break;
    case EL_SEED:
        if (sandbox_at(sb, x, y + 1) == EL_SOIL && sandbox_chance(sb, 6))
            sandbox_put(sb, x, y, EL_PLANT, (uint8_t)(36 + sandbox_rand(sb, 32)));
        break;
    case EL_ICE:
        if (hot_near(sb, x, y, id) && sandbox_chance(sb, 10))
            sandbox_put(sb, x, y, EL_WATER, 0);
        break;
    case EL_SNOW:
        if (hot_near(sb, x, y, id) && sandbox_chance(sb, 60))
            sandbox_put(sb, x, y, EL_WATER, 0);
        break;
    case EL_STEAM:
        if (cold_near(sb, x, y) && sandbox_chance(sb, 60))
            sandbox_put(sb, x, y, EL_WATER,
                        0); /* touching ice condenses it at once; otherwise it cools on its own */
        break;
    case EL_SMOKE:
        if (sandbox_chance(sb, 3))
            sandbox_put(sb, x, y, EL_EMPTY, 0);
        break;
    default:
        break;
    }
}

/* ---- Conway's Game of Life, for the pink "Life" cells ---- */

void sandbox_life_step(Sandbox *sb) {
    int total = sb->w * sb->h;
    if (memchr(sb->cells, EL_LIFE, (size_t)total) == NULL)
        return; /* nothing alive: skip the whole scan */

    /* Pass 1 marks the cells that change in aux (1 = dies, 2 = is born); pass 2 applies the marks. */
    for (int y = 0; y < sb->h; y++) {
        for (int x = 0; x < sb->w; x++) {
            int i = cell_at(sb, x, y);
            ElementId id = (ElementId)sb->cells[i];
            if (id != EL_LIFE && id != EL_EMPTY)
                continue;
            int n = 0;
            for (int dir = 0; dir < 8; dir++) {
                int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
                if (sandbox_inside(sb, nx, ny) && sb->cells[cell_at(sb, nx, ny)] == EL_LIFE)
                    n++;
            }
            if (id == EL_LIFE && (n < 2 || n > 3))
                sb->aux[i] = 1;
            else if (id == EL_EMPTY && n == 3)
                sb->aux[i] = 2;
        }
    }
    for (int y = 0; y < sb->h; y++) {
        for (int x = 0; x < sb->w; x++) {
            int i = cell_at(sb, x, y);
            if (sb->aux[i] == 0)
                continue;
            if (sb->cells[i] == EL_LIFE && sb->aux[i] == 1)
                sandbox_put(sb, x, y, EL_EMPTY, 0);
            else if (sb->cells[i] == EL_EMPTY && sb->aux[i] == 2)
                sandbox_put(sb, x, y, EL_LIFE, 0);
        }
    }
}
