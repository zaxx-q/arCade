/* Explosions. A blast is a shock wave: energy sent out along many straight rays from one point, one ray for
 * each cell on the border of a square around it, so the rays are never more than a cell apart where the wave
 * ends.
 *
 * Walking out along a ray the energy drops a little with every step through open air and by the cell's
 * `resist` (see k_thermal in sandbox_heat.c) for every cell it passes through, so walls cast shadows and a
 * wave dies quickly inside a pile of sand. What a cell does depends on the energy that reaches it:
 *
 *   - sand, water, gas and other loose things are thrown outwards: they swap places with the first free cell
 *     further along the ray, so a buried charge digs a crater and the material lands as a rim around it.
 *     With a lot of energy liquids flash into steam and loose things burn up instead;
 *   - brittle solids (stone, brick, glass, wood, ice, plants) shatter once the energy reaches their
 *     `tough` value: stone and brick become gravel, which is thrown out like sand. Steel and heaters need far
 *     more. Walls never break and stop the wave completely;
 *   - what survives is heated, and flammable things catch fire; TNT is set off (a rolling chain of blasts)
 *     and uranium is heated, so a blast next to a pile of it can start a meltdown;
 *   - in open air the strongest part of the wave leaves flames: the fireball.
 *
 * Small blasts finish inside the tick that made them. Big ones (BIG_BLAST and up) expand SHOCK_SPEED cells
 * per tick so the ring can be watched; only one such wave travels at a time (a second one finishes the first
 * at once). A cell is hit at most once per tick (BLASTED). */
#include <stdlib.h>

#include "sandbox_internal.h"

#define BIG_BLAST 40   /* blasts at least this strong travel outwards over several ticks */
#define FLASH_BLAST 60 /* ... and these also white out the screen for a moment */
#define SHOCK_SPEED 4  /* cells per tick */
#define MAX_POWER ((SANDBOX_MAX_RAYS / 8) - 1)
#define THROW_RANGE 24 /* how far along the ray a thrown cell looks for a free landing place */

typedef struct RayCtx {
    int cx, cy, reach, power;
    int tx, ty; /* where the ray points: a cell on the border of the square, relative to the centre */
    int step8;  /* energy lost per step in open air, in 1/8 units (a diagonal step is longer) */
} RayCtx;

static int isqrt(int v) {
    int r = 0;
    while ((r + 1) * (r + 1) <= v)
        r++;
    return r;
}

/* The ray's target: ray k of 8 * reach walks round the square. */
static void ray_target(int reach, int k, int *tx, int *ty) {
    int side = 2 * reach, n = k % side;
    switch (k / side) {
    case 0:
        *tx = -reach + n;
        *ty = -reach;
        break;
    case 1:
        *tx = reach;
        *ty = -reach + n;
        break;
    case 2:
        *tx = reach - n;
        *ty = reach;
        break;
    default:
        *tx = -reach;
        *ty = reach - n;
        break;
    }
}

static void ray_pos(const RayCtx *rc, int s, int *px, int *py) {
    int half = rc->reach / 2;
    *px = rc->cx + (rc->tx * s + (rc->tx >= 0 ? half : -half)) / rc->reach;
    *py = rc->cy + (rc->ty * s + (rc->ty >= 0 ? half : -half)) / rc->reach;
}

/* Throw the loose cell at step s along the ray: it swaps places with the first free cell further out.
 * Returns false when there is none in range (it stays where it is). */
static bool throw_out(Sandbox *sb, const RayCtx *rc, int s, int i) {
    for (int m = 1; m <= THROW_RANGE; m++) {
        int px, py;
        ray_pos(rc, s + m, &px, &py);
        if (!sandbox_inside(sb, px, py))
            return false;
        int j = py * sb->w + px;
        if (sb->cells[j] == EL_EMPTY) {
            sandbox_swap(sb, i, j);
            sb->moved[i] |= BLASTED;
            sb->moved[j] |= BLASTED;
            return true;
        }
    }
    return false;
}

static bool is_loose(MoveKind k) {
    return k == MOVE_POWDER || k == MOVE_LIQUID || k == MOVE_GAS || k == MOVE_FIRE || k == MOVE_CRAWL;
}

/* What happens to cell (px, py) when a wave with energy e8 (in 1/8 units) reaches it; returns the energy that
 * is left to carry on outwards. */
static int hit(Sandbox *sb, const RayCtx *rc, int s, int px, int py, int e8) {
    int i = py * sb->w + px;
    ElementId c = (ElementId)sb->cells[i];
    int e = e8 / 8;
    int f = e * 256 / rc->power; /* how much of the original strength is left, 0..256 */

    if (c == EL_EMPTY) { /* open air: the fireball burns where the wave is still strong */
        if (f > 60 && sandbox_chance(sb, (f - 60) * 2 / 3)) {
            sandbox_put(sb, px, py, EL_FIRE, 0);
            sb->moved[i] |= BLASTED;
        }
        return e8 - rc->step8;
    }

    const ThermalDef *t = sandbox_thermal(c);
    const ElementDef *d = sandbox_element(c);
    if (t->tough == 255)
        return 0; /* a wall: the wave stops dead */
    int cost = rc->step8 + t->resist * 8;
    if (sb->moved[i] & BLASTED)
        return e8 - cost; /* already pushed this tick */
    sb->moved[i] |= BLASTED;

    if (c == EL_TNT) { /* neighbouring charges go off a moment later: a rolling chain */
        if (sb->aux[i] == 0)
            sb->aux[i] = 1;
        return e8 - cost;
    }
    if (c == EL_URANIUM) {
        sandbox_add_heat(sb, px, py, e * 50); /* a blast can push a pile of it over the edge */
        return e8 - cost;
    }

    if (is_loose(d->kind)) {
        if (f >= 190 || c == EL_METHANE) { /* so much energy that it just burns up */
            if (c == EL_WATER || c == EL_ICE)
                sandbox_put(sb, px, py, EL_STEAM, 0);
            else if (c == EL_METHANE || sandbox_element(c)->ignite > 0 || sandbox_chance(sb, 128))
                sandbox_put(sb, px, py, EL_FIRE, 0);
            else
                sandbox_put(sb, px, py, sandbox_chance(sb, 90) ? EL_SMOKE : EL_EMPTY, 0);
            return e8 - cost / 2;
        }
        if (sandbox_flammable_now(sb, i) && e >= 4) { /* powder, oil and gas catch fire in the blast */
            sandbox_ignite(sb, px, py);
            return e8 - cost / 2;
        }
        sandbox_add_heat(sb, px, py, e * 8);
        if (!throw_out(sb, rc, s, i) && e >= 6) {
            /* nowhere to go: a strong wave still crushes it */
            sandbox_put(sb, px, py, EL_EMPTY, 0);
        }
        return e8 - cost;
    }

    /* a solid */
    if (e >= t->tough) {
        ElementId rubble = EL_EMPTY;
        switch (c) {
        case EL_STONE:
        case EL_BRICK:
        case EL_GRAVEL:
            rubble = EL_GRAVEL;
            break;
        case EL_WOOD:
            rubble = sandbox_chance(sb, 120) ? EL_FIRE : EL_ASH;
            break;
        case EL_ICE:
            rubble = f >= 120 ? EL_STEAM : EL_WATER;
            break;
        case EL_PLANT:
        case EL_FUSE:
            rubble = sandbox_chance(sb, 100) ? EL_FIRE : EL_SMOKE;
            break;
        case EL_GLASS:
            rubble = sandbox_chance(sb, 70) ? EL_EMPTY : EL_SAND;
            break;
        default: /* steel, heaters and the rest only give way to a huge blast */
            rubble = sandbox_chance(sb, 128) ? EL_FIRE : EL_EMPTY;
            break;
        }
        sandbox_put(sb, px, py, rubble, 0);
        sb->moved[i] |= BLASTED;
        if (rubble == EL_GRAVEL || rubble == EL_ASH || rubble == EL_WATER || rubble == EL_SAND)
            throw_out(sb, rc, s, i);
        return e8 - cost / 2;
    }
    sandbox_add_heat(sb, px, py, e * 12); /* it holds, but it gets hot */
    if (sandbox_flammable_now(sb, i) && e >= 3)
        sandbox_ignite(sb, px, py);
    return e8 - cost;
}

/* Walk one ray from step s0 + 1 to s1 with energy *e8. */
static void march(Sandbox *sb, RayCtx *rc, int16_t *e8, int s0, int s1) {
    int e = *e8;
    for (int s = s0 + 1; s <= s1 && e > 0; s++) {
        int px, py;
        ray_pos(rc, s, &px, &py);
        if (!sandbox_inside(sb, px, py)) {
            e = 0;
            break;
        }
        e = hit(sb, rc, s, px, py, e);
    }
    *e8 = (int16_t)arc_maxi(e, 0);
}

static void ray_setup(RayCtx *rc, int cx, int cy, int reach, int power, int k) {
    rc->cx = cx;
    rc->cy = cy;
    rc->reach = reach;
    rc->power = power;
    ray_target(reach, k, &rc->tx, &rc->ty);
    rc->step8 = (isqrt(rc->tx * rc->tx + rc->ty * rc->ty) * 8 + reach / 2) / reach;
}

static void shock_advance(Sandbox *sb, Shock *sh, int to) {
    to = arc_mini(to, sh->reach);
    bool alive = false;
    for (int k = 0; k < 8 * sh->reach; k++) {
        if (sh->energy[k] <= 0)
            continue;
        RayCtx rc;
        ray_setup(&rc, sh->cx, sh->cy, sh->reach, sh->power, k);
        march(sb, &rc, &sh->energy[k], sh->front, to);
        alive = alive || sh->energy[k] > 0;
    }
    sh->front = to;
    if (!alive || to >= sh->reach)
        sh->active = false;
}

void sandbox_blast(Sandbox *sb, int x, int y, int power) {
    power = arc_clampi(power, 2, MAX_POWER);
    if (power >= FLASH_BLAST)
        sb->flash = 255;
    if (power >= BIG_BLAST) {
        if (sb->shock.active)
            shock_advance(sb, &sb->shock, sb->shock.reach); /* the old wave finishes at once */
        Shock *sh = &sb->shock;
        sh->active = true;
        sh->cx = x;
        sh->cy = y;
        sh->reach = power;
        sh->power = power;
        sh->front = 0;
        for (int k = 0; k < 8 * power; k++)
            sh->energy[k] = (int16_t)(power * 8);
        shock_advance(sb, sh, SHOCK_SPEED);
        return;
    }
    int16_t energy[SANDBOX_MAX_RAYS];
    for (int k = 0; k < 8 * power; k++) {
        energy[k] = (int16_t)(power * 8);
        RayCtx rc;
        ray_setup(&rc, x, y, power, power, k);
        march(sb, &rc, &energy[k], 0, power);
    }
}

void sandbox_blast_step(Sandbox *sb) {
    sb->blast_budget = SANDBOX_BLASTS_PER_TICK;
    if (sb->flash > 0)
        sb->flash = arc_maxi(0, sb->flash - 9);
    if (sb->shock.active)
        shock_advance(sb, &sb->shock, sb->shock.front + SHOCK_SPEED);
}
