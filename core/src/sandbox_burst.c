/* Explosions and radiation. Both are made of particles that live in the grid as ordinary cells (EL_SPARK and
 * EL_GAMMA) and fly in a straight line, so a blast is not one calculation but hundreds of little fragments
 * that each decide for themselves what to do. That is why it looks like an explosion: a ragged starburst,
 * every spark with its own length and its own luck, and a crater whose shape depends on what was in the way.
 *
 * A charge (TNT, gunpowder) goes off all at once: sandbox_detonate() gathers every charge cell touching the
 * one that was lit with a flood fill and turns each of them into a spark that flies off in a random
 * direction. One cell, one spark.
 *
 * A spark flies five cells per tick, for 30 to 99 cells, and then burns out. Every cell it crosses it
 * leaves a flame behind it, so a blast is over in about a quarter of a second but leaves a fat ball of fire.
 * What is in front of it decides what happens:
 *   - open air or gas: it flies on (gas is pushed out of the way);
 *   - a wall, steel, a heater or a cooler: it stops and goes out;
 *   - another spark: it waits, or sometimes both burn out, so a dense charge does not pile up;
 *   - anything else (sand, water, stone, wood ...): the spark ploughs through it, deleting the cell, but
 *     each cell it breaks through has about a 3 in 10 chance of using the spark up;
 *   - a charge: it sets that one off too.
 * Now and then a spark throws off a twin at a right angle. A heading is one of 16 (8 directions, each with a
 * "bent" variant that alternates between two neighbouring directions), so the starburst has no gaps.
 *
 * Gamma rays are the same idea for radiation (see sandbox_react.c for where they come from): they fly three
 * cells per tick for up to about 150 cells, and nothing but a wall stops them. Whatever they cross is burnt
 * away or turned to rubble, and any uranium they meet goes critical in turn, which is the chain reaction. A
 * ray also scatters now and then: it leaves a second ray behind it that flies off in another direction (only
 * rays that came straight from the uranium do this, the scattered ones do not), so a big lump does not send
 * out a few thin lines but sweeps a whole region clean. */
#include "sandbox_internal.h"

#define SPARK_STEPS 5      /* cells a spark flies per tick */
#define SPARK_LIFE_MIN 30  /* a spark lives this many steps at least ... */
#define SPARK_LIFE_SPAN 70 /* ... and up to this many more */
#define POWDER_LIFE_MIN 15 /* gunpowder is a weaker charge: its sparks do not fly as far */
#define POWDER_LIFE_SPAN 30
#define SPARK_FORK 6 /* chance (/256) per step that a spark throws off a twin */
#define SPARK_DIE 80 /* chance (/256) that a cell it breaks through, or a spark it bumps, uses it up */

/* aux of a ray: bits 6-7 = generation (0 = straight from uranium, 1 = scattered), bits 0-5 = ticks left */
#define GAMMA_LIFE_MIN 40
#define GAMMA_LIFE_SPAN 24
#define GAMMA_STEPS 3    /* cells per tick */
#define GAMMA_SCATTER 16 /* chance (/256) per step that a first-generation ray throws off a scattered one */

static int heading_of(const Sandbox *sb, int i) {
    return (sb->moved[i] & HEADING_MASK) >> HEADING_SHIFT;
}

static void set_heading(Sandbox *sb, int i, int heading) {
    sb->moved[i] = (uint8_t)((sb->moved[i] & ~HEADING_MASK) | (((unsigned)heading & 15u) << HEADING_SHIFT));
}

/* The direction a particle steps in. Headings 8..15 alternate between direction h and h + 1 as `phase`
 * counts up, which gives the 8 directions halfway between the compass points. */
static int step_dir(int heading, unsigned phase) {
    int dir = heading & 7;
    return ((heading & 8) && (phase & 1u)) ? (dir + 1) & 7 : dir;
}

static int cell_of(const Sandbox *sb, int x, int y) {
    return y * sb->w + x;
}

static void make_particle(Sandbox *sb, int x, int y, ElementId id, int life, int heading) {
    sandbox_put(sb, x, y, id, (uint8_t)life);
    set_heading(sb, cell_of(sb, x, y), heading);
}

bool sandbox_explosive(ElementId id) {
    return id == EL_TNT || id == EL_GUNPOWDER;
}

/* ---- charges ---- */

void sandbox_detonate(Sandbox *sb, int x0, int y0) {
    static int stack[SANDBOX_MAX_W * SANDBOX_MAX_H];
    const int cap = (int)(sizeof stack / sizeof stack[0]);
    int start = cell_of(sb, x0, y0);
    if (!sandbox_explosive((ElementId)sb->cells[start]))
        return;
    int top = 0;
    stack[top++] = start;
    sb->moved[start] |= VISITED;
    for (int head = 0; head < top; head++) { /* breadth-first: the list doubles as the queue */
        int x = stack[head] % sb->w, y = stack[head] / sb->w;
        for (int dir = 0; dir < 8; dir++) {
            int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
            if (!sandbox_inside(sb, nx, ny))
                continue;
            int j = cell_of(sb, nx, ny);
            if (sandbox_explosive((ElementId)sb->cells[j]) && !(sb->moved[j] & VISITED) && top < cap) {
                sb->moved[j] |= VISITED;
                stack[top++] = j;
            }
        }
    }
    for (int k = 0; k < top; k++) {
        int x = stack[k] % sb->w, y = stack[k] / sb->w;
        bool powder = sb->cells[stack[k]] == EL_GUNPOWDER;
        int life = powder ? POWDER_LIFE_MIN + sandbox_rand(sb, POWDER_LIFE_SPAN)
                          : SPARK_LIFE_MIN + sandbox_rand(sb, SPARK_LIFE_SPAN);
        make_particle(sb, x, y, EL_SPARK, life, sandbox_rand(sb, 16));
    }
}

/* Things a spark cannot get through. */
static bool stops_spark(ElementId id) {
    return id == EL_WALL || id == EL_STEEL || id == EL_HEATER || id == EL_COOLER;
}

void sandbox_burst(Sandbox *sb, int x, int y, int sparks) {
    int r = 1;
    while ((2 * r + 1) * (2 * r + 1) < sparks)
        r++;
    for (int k = 0; k < sparks; k++) {
        int px = x + sandbox_rand(sb, 2 * r + 1) - r, py = y + sandbox_rand(sb, 2 * r + 1) - r;
        if (sandbox_inside(sb, px, py) && !stops_spark((ElementId)sb->cells[cell_of(sb, px, py)]))
            make_particle(sb, px, py, EL_SPARK, SPARK_LIFE_MIN + sandbox_rand(sb, SPARK_LIFE_SPAN),
                          sandbox_rand(sb, 16));
    }
}

/* ---- sparks ---- */

static void spark_out(Sandbox *sb, int x, int y) {
    sandbox_put(sb, x, y, sandbox_chance(sb, 60) ? EL_FIRE : EL_EMPTY, 0);
}

/* One step of a spark. Returns true if it moved on (and *x, *y are its new cell), false if it is gone or has
 * to wait where it is. */
static bool spark_step(Sandbox *sb, int *px, int *py, int sub) {
    int x = *px, y = *py;
    int i = cell_of(sb, x, y);
    int life = sb->aux[i], heading = heading_of(sb, i);
    if (life <= 1) {
        sandbox_put(sb, x, y, EL_EMPTY, 0);
        return false;
    }
    life--;
    sb->aux[i] = (uint8_t)life;

    if (sandbox_chance(sb, SPARK_FORK)) { /* a twin leaves at a right angle */
        int side = ((heading & 7) + (sandbox_rand(sb, 2) ? 2 : 6)) & 7;
        int tx = x + sandbox_dx(side), ty = y + sandbox_dy(side);
        if (sandbox_inside(sb, tx, ty) && sb->cells[cell_of(sb, tx, ty)] == EL_EMPTY)
            make_particle(sb, tx, ty, EL_SPARK, life, side | (heading & 8));
    }

    int dir = step_dir(heading, sb->tick * SPARK_STEPS + (unsigned)sub);
    int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
    if (!sandbox_inside(sb, nx, ny)) {
        sandbox_put(sb, x, y, EL_EMPTY, 0);
        return false;
    }
    int j = cell_of(sb, nx, ny);
    ElementId t = (ElementId)sb->cells[j];
    MoveKind k = sandbox_element(t)->kind;
    if (t != EL_EMPTY && k != MOVE_GAS && k != MOVE_FIRE) {
        if (stops_spark(t)) {
            spark_out(sb, x, y);
            return false;
        }
        if (t == EL_SPARK || t == EL_GAMMA) {
            if (t == EL_SPARK && sandbox_chance(sb, SPARK_DIE))
                spark_out(sb, x, y);
            return false; /* otherwise it waits for the way to clear */
        }
        if (sandbox_explosive(t)) { /* it hit another charge: that goes off as well */
            sandbox_detonate(sb, nx, ny);
            return false;
        }
        if (sandbox_chance(sb, SPARK_DIE)) { /* it breaks through, but it costs the spark */
            spark_out(sb, x, y);
            return false;
        }
    }
    make_particle(sb, nx, ny, EL_SPARK, life, heading); /* advance, deleting whatever was in the cell ... */
    sandbox_put(sb, x, y, EL_FIRE, 0);                  /* ... and leave a flame where it was */
    *px = nx;
    *py = ny;
    return true;
}

static void fly_spark(Sandbox *sb, int x, int y) {
    for (int sub = 0; sub < SPARK_STEPS; sub++)
        if (!spark_step(sb, &x, &y, sub))
            return;
}

/* ---- gamma rays ---- */

static void make_ray(Sandbox *sb, int x, int y, int generation) {
    int life = GAMMA_LIFE_MIN + sandbox_rand(sb, GAMMA_LIFE_SPAN);
    make_particle(sb, x, y, EL_GAMMA, (generation << 6) | life, sandbox_rand(sb, 16));
}

void sandbox_go_critical(Sandbox *sb, int x, int y) {
    make_ray(sb, x, y, 0);
}

/* What a ray leaves in the cell it has burnt through (EL_EMPTY = nothing at all). */
static ElementId burnt(Sandbox *sb, ElementId t) {
    const ElementDef *d = sandbox_element(t);
    switch (t) {
    case EL_WATER:
    case EL_ICE:
    case EL_SNOW:
        return EL_STEAM;
    case EL_SAND:
        return sandbox_chance(sb, 180) ? EL_GLASS : EL_EMPTY; /* sand is fused into glass */
    default:
        break;
    }
    if (d->ignite > 0)
        return EL_FIRE;
    if (d->kind == MOVE_LIQUID || d->kind == MOVE_POWDER)
        return sandbox_chance(sb, 128) ? EL_SMOKE : EL_EMPTY;
    return sandbox_chance(sb, 64) ? EL_GRAVEL : EL_EMPTY; /* solids mostly vanish, some crumble */
}

static void fly_gamma(Sandbox *sb, int x, int y) {
    for (int sub = 0; sub < GAMMA_STEPS; sub++) {
        int i = cell_of(sb, x, y);
        int generation = sb->aux[i] >> 6, life = sb->aux[i] & 63, heading = heading_of(sb, i);
        if (life == 0) {
            sandbox_put(sb, x, y, EL_EMPTY, 0);
            return;
        }
        if (sub == 0) /* the flight is counted in ticks, not in steps */
            sb->aux[i] = (uint8_t)((generation << 6) | (life - 1));
        int dir = step_dir(heading, sb->tick);
        int nx = x + sandbox_dx(dir), ny = y + sandbox_dy(dir);
        if (!sandbox_inside(sb, nx, ny)) {
            sandbox_put(sb, x, y, EL_EMPTY, 0);
            return;
        }
        int j = cell_of(sb, nx, ny);
        ElementId t = (ElementId)sb->cells[j];
        MoveKind k = sandbox_element(t)->kind;
        ElementId left = EL_EMPTY; /* what stays behind where the ray was */
        if (t == EL_WALL) {
            sandbox_put(sb, x, y, EL_EMPTY, 0);
            return;
        }
        if (t == EL_URANIUM || t == EL_MOLTEN_URANIUM) {
            /* the chain reaction: the ray goes on, and so does the new one */
            sandbox_go_critical(sb, nx, ny);
        } else if (sandbox_explosive(t)) {
            sandbox_detonate(sb, nx, ny);
        } else if (t != EL_EMPTY && t != EL_LAVA && k != MOVE_GAS && k != MOVE_FIRE && k != MOVE_RAY) {
            left = burnt(sb, t);
            sandbox_put(sb, nx, ny, EL_EMPTY, 0);
        }
        sandbox_swap(sb, i, j);
        x = nx;
        y = ny;
        int bx = x - sandbox_dx(dir), by = y - sandbox_dy(dir); /* the cell the ray just left */
        if (left != EL_EMPTY)
            sandbox_put(sb, bx, by, left, 0);
        else if (generation == 0 && sandbox_chance(sb, GAMMA_SCATTER))
            make_ray(sb, bx, by, 1);
    }
}

/* One tick of a flying particle; called from sandbox_move for everything of kind MOVE_RAY. */
void sandbox_fly(Sandbox *sb, int x, int y) {
    if (sb->cells[cell_of(sb, x, y)] == EL_GAMMA)
        fly_gamma(sb, x, y);
    else
        fly_spark(sb, x, y);
}
