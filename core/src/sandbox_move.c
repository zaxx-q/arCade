/* Movement rules. Every cell is visited once per tick (bottom row first, see sandbox_step) and may move.
 *
 *   powder  falls up to `speed` cells through open air, sinks through lighter fluids, splashes sideways when
 *           it lands on cells that are still moving, and slides down slopes.
 *   liquid  falls the same way, then looks sideways for the NEAREST place it could fall again and goes there.
 *           A cell only moves when that gets it lower, so a settled pool stands perfectly still, and water
 *           poured on a table runs to the edge and drips off, which is what makes pools level out quickly.
 *   gas     rises with some chance, otherwise drifts sideways.
 *
 * Moving always goes through sandbox_swap(): the cell trades places with what is there (empty air or a
 * lighter fluid), so a heavy cell sinking through water pushes the water up automatically. */
#include <stdlib.h>

#include "sandbox_internal.h"

#define LIQUID_RUN 4 /* a liquid slides at most this many cells sideways per tick */

static int cell_index(const Sandbox *sb, int x, int y) {
    return y * sb->w + x;
}

/* Can `mover` fall into (x, y)? Open air, or a fluid that is lighter than the mover. */
static bool can_sink(const Sandbox *sb, int x, int y, ElementId mover) {
    if (!sandbox_inside(sb, x, y))
        return false;
    ElementId t = (ElementId)sb->cells[cell_index(sb, x, y)];
    if (t == EL_EMPTY)
        return true;
    return sandbox_is_fluid(t) && sandbox_element(t)->density < sandbox_element(mover)->density;
}

/* Can a gas or flame rise into (x, y)? Open air, or a heavier gas (so light gases end up on top). */
static bool can_rise(const Sandbox *sb, int x, int y, ElementId mover) {
    if (!sandbox_inside(sb, x, y))
        return false;
    ElementId t = (ElementId)sb->cells[cell_index(sb, x, y)];
    if (t == EL_EMPTY)
        return true;
    MoveKind k = sandbox_element(t)->kind;
    return (k == MOVE_GAS || k == MOVE_FIRE) && sandbox_element(t)->density > sandbox_element(mover)->density;
}

/* Fall straight down: through up to `budget` cells of open air, or one cell through a lighter fluid.
 * Returns how much of the budget was used (0 = could not move). Updates *y. */
static int fall(Sandbox *sb, int x, int *y, int budget) {
    ElementId me = (ElementId)sb->cells[cell_index(sb, x, *y)];
    int n = 0;
    while (n < budget && sandbox_at(sb, x, *y + n + 1) == EL_EMPTY)
        n++;
    if (n == 0) {
        if (!can_sink(sb, x, *y + 1, me))
            return 0;
        n = 1; /* sinking through a fluid is slower than falling through air */
    }
    sandbox_swap(sb, cell_index(sb, x, *y), cell_index(sb, x, *y + n));
    *y += n;
    return n;
}

/* ---- powders ---- */

static void move_powder(Sandbox *sb, int x, int y) {
    int i = cell_index(sb, x, y);
    ElementId me = (ElementId)sb->cells[i];
    const ElementDef *d = sandbox_element(me);

    /* Buoyancy: a light powder under a heavier liquid is pushed up (snow in water, sand on mercury). */
    const ElementDef *above = sandbox_element(sandbox_at(sb, x, y - 1));
    if (above->kind == MOVE_LIQUID && above->density > d->density) {
        sandbox_swap(sb, i, cell_index(sb, x, y - 1));
        return;
    }

    if (fall(sb, x, &y, d->speed) > 0)
        return;

    int dir = (rng_u32(&sb->rng) & 1u) ? 1 : -1;

    /* Splash: a grain that was falling last tick and lands on a cell that was ALSO falling (a thick
     * stream or a dropped lump) bounces sideways. A thin trickle never splashes, so slow pours still
     * make clean cones while a big dump spreads into a wide heap. */
    if ((sb->moved[i] & MOVED_LAST) && sandbox_inside(sb, x, y + 1)) {
        int b = cell_index(sb, x, y + 1);
        if ((sb->moved[b] & MOVED_LAST) && !sandbox_is_fluid((ElementId)sb->cells[b])) {
            int reach = 1 + sandbox_rand(sb, 4);
            for (int k = 0; k < 2; k++) {
                int sd = k == 0 ? dir : -dir;
                int s = 0;
                while (s < reach && sandbox_at(sb, x + sd * (s + 1), y) == EL_EMPTY)
                    s++;
                if (s > 0) {
                    sandbox_swap(sb, i, cell_index(sb, x + sd * s, y));
                    return;
                }
            }
        }
    }

    /* Slide down a slope. The side cell must be free too, or grains would squeeze through diagonal gaps. */
    for (int k = 0; k < 2; k++) {
        int sx = k == 0 ? x + dir : x - dir;
        if (!can_sink(sb, sx, y, me) || !can_sink(sb, sx, y + 1, me))
            continue;
        bool drop_ok = true; /* `grip` extra free cells below are needed: sticky grains pile up steeper */
        for (int g = 1; g <= d->grip && drop_ok; g++)
            drop_ok = can_sink(sb, sx, y + 1 + g, me);
        if (!drop_ok)
            continue;
        sandbox_swap(sb, i, cell_index(sb, sx, y + 1));
        return;
    }
}

/* ---- liquids ---- */

/* Looks left and right along the row for the nearest cell that has free space below it. Returns the signed
 * distance to walk (negative = left), or 0 when there is nowhere lower to go.
 * The search may pass over cells of the same liquid: a surface cell inside a plateau can then hop over its
 * neighbours to where the water is needed, so a whole pool levels out quickly instead of one cell at a time.
 * *over is set when the path crosses such cells (the move must then be made in one go). */
static int find_drop(const Sandbox *sb, int x, int y, ElementId me, int reach, int first, bool *over) {
    bool open[2] = {true, true}; /* [0] = first direction, [1] = the other one: is the path still clear? */
    bool wet[2] = {false, false};
    for (int dist = 1; dist <= reach; dist++) {
        for (int k = 0; k < 2; k++) {
            if (!open[k])
                continue;
            int dir = k == 0 ? first : -first;
            int nx = x + dir * dist;
            if (sandbox_at(sb, nx, y) == me) {
                wet[k] = true;
                continue;
            }
            if (!can_sink(sb, nx, y, me)) {
                open[k] = false;
                continue;
            }
            if (can_sink(sb, nx, y + 1, me)) {
                *over = wet[k];
                return dir * dist;
            }
        }
        if (!open[0] && !open[1])
            break;
    }
    return 0;
}

#define BAR_CAP 64

/* Length of the run of `me` cells in row y that starts at x and extends in direction dir. */
static int bar_len(const Sandbox *sb, int x, int y, ElementId me, int dir) {
    int n = 0;
    while (n < BAR_CAP && sandbox_at(sb, x + dir * n, y) == me)
        n++;
    return n;
}

/* A flat surface with a few leftover cells on it (studs and short bars) has nowhere lower to flow, but it
 * looks noisy. So the cell at the end of a bar hands itself over to the nearest bar that is at least as long
 * as its own, in the direction where the surface is free. Every such move makes the bars more uneven, so it
 * always ends: the leftovers of a spill gather into one tidy step. Returns the distance to walk, or 0. */
static int find_mate(const Sandbox *sb, int x, int y, ElementId me, int reach, int first) {
    bool free_side[2];
    for (int k = 0; k < 2; k++)
        free_side[k] = can_sink(sb, x + (k == 0 ? first : -first), y, me);
    if (!free_side[0] && !free_side[1])
        return 0; /* in the middle of a bar */
    int own = 1;
    if (!free_side[0] && sandbox_at(sb, x + first, y) == me)
        own = bar_len(sb, x, y, me, first);
    else if (!free_side[1] && sandbox_at(sb, x - first, y) == me)
        own = bar_len(sb, x, y, me, -first);
    for (int k = 0; k < 2; k++) {
        if (!free_side[k])
            continue;
        int dir = k == 0 ? first : -first;
        for (int dist = 2; dist <= reach; dist++) {
            ElementId t = sandbox_at(sb, x + dir * dist, y);
            if (t == me) {
                if (bar_len(sb, x + dir * dist, y, me, dir) >= own)
                    return dir * (dist - 1);
                break;
            }
            if (!can_sink(sb, x + dir * dist, y, me))
                break;
        }
    }
    return 0;
}

static void move_liquid(Sandbox *sb, int x, int y) {
    ElementId me = (ElementId)sb->cells[cell_index(sb, x, y)];
    const ElementDef *d = sandbox_element(me);
    int slip = d->slip;
    if (me == EL_LAVA) /* lava thickens as it cools: runny at 1200 degrees, nearly stiff by 500 */
        slip = slip * arc_clampi(sb->temp[cell_index(sb, x, y)] - 450, 4, 800) / 800;
    if (slip < 255 && !sandbox_chance(sb, slip))
        return; /* thick liquids (lava) only act now and then */

    int budget = d->speed;     /* cells it may still fall this tick */
    int run_left = LIQUID_RUN; /* cells it may still slide sideways this tick */
    for (int step = 0; step < 4; step++) {
        int used = fall(sb, x, &y, budget);
        if (used > 0) {
            budget -= used;
            if (budget <= 0)
                return;
            continue;
        }
        /* Only the surface flows sideways: something on top (water, sand...) pins a cell down. */
        if (run_left <= 0 || !can_sink(sb, x, y - 1, me))
            return;
        int first = (rng_u32(&sb->rng) & 1u) ? 1 : -1;
        bool over = false;
        int walk = find_drop(sb, x, y, me, d->spread, first, &over);
        if (walk == 0) {
            walk = find_mate(sb, x, y, me, d->spread, first);
            if (walk == 0)
                return; /* settled */
            over = false;
        }
        int run = over ? abs(walk) : arc_mini(abs(walk), run_left);
        run_left = over ? 0 : run_left - run;
        int nx = x + (walk > 0 ? run : -run);
        sandbox_swap(sb, cell_index(sb, x, y), cell_index(sb, nx, y));
        x = nx;
    }
}

/* ---- gases and fire ---- */

static void move_gas(Sandbox *sb, int x, int y) {
    int i = cell_index(sb, x, y);
    ElementId me = (ElementId)sb->cells[i];
    int dir = (rng_u32(&sb->rng) & 1u) ? 1 : -1;
    if (sandbox_chance(sb, sandbox_element(me)->speed)) {
        if (can_rise(sb, x, y - 1, me)) {
            sandbox_swap(sb, i, cell_index(sb, x, y - 1));
            return;
        }
        if (can_rise(sb, x + dir, y - 1, me) && can_rise(sb, x + dir, y, me)) {
            sandbox_swap(sb, i, cell_index(sb, x + dir, y - 1));
            return;
        }
    }
    if (sandbox_at(sb, x + dir, y) == EL_EMPTY)
        sandbox_swap(sb, i, cell_index(sb, x + dir, y));
}

/* ---- crawlers: they only fall when nothing is near to hold on to; the walking is in sandbox_react.c ---- */

static void move_crawl(Sandbox *sb, int x, int y) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0)
                continue;
            ElementId t = sandbox_at(sb, x + dx, y + dy);
            if (t != EL_EMPTY && !sandbox_is_fluid(t))
                return; /* something to cling to */
        }
    fall(sb, x, &y, 1);
}

void sandbox_move(Sandbox *sb, int x, int y) {
    switch (sandbox_element((ElementId)sb->cells[cell_index(sb, x, y)])->kind) {
    case MOVE_POWDER:
        move_powder(sb, x, y);
        break;
    case MOVE_LIQUID:
        move_liquid(sb, x, y);
        break;
    case MOVE_GAS:
    case MOVE_FIRE:
        move_gas(sb, x, y);
        break;
    case MOVE_RAY:
        sandbox_fly(sb, x, y); /* sparks and gamma rays: see sandbox_burst.c */
        break;
    case MOVE_CRAWL:
        move_crawl(sb, x, y);
        break;
    case MOVE_NONE:
        break;
    }
}
