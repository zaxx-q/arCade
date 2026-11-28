/* Private helpers shared by the core/src/sandbox*.c files. Not part of the public API: games and the hub
 * only include arcade/sandbox.h. */
#ifndef ARCADE_SANDBOX_INTERNAL_H
#define ARCADE_SANDBOX_INTERNAL_H

#include "arcade/sandbox.h"

/* Bits of Sandbox.moved. A cell that already acted this tick is skipped, so nothing moves twice. */
#define MOVED_NOW 1u
#define MOVED_LAST 2u
#define HEADING_SHIFT 3
#define HEADING_MASK 0x78u /* sparks and rays: 4 bits that say which way the particle flies */
#define VISITED 0x80u      /* scratch mark for the flood fill that gathers a charge; cleared next tick */

/* Wood, plants, fuse and oil burn in place: bit 7 of aux says "burning", the low 7 bits are a timer. */
#define BURN_BIT 0x80u
#define BURN_TIMER(a) ((a) & 0x7Fu)

/* The Life cellular automaton only advances every this many ticks, so patterns can be watched. */
#define LIFE_PERIOD 6

static inline bool sandbox_inside(const Sandbox *sb, int x, int y) {
    return x >= 0 && y >= 0 && x < sb->w && y < sb->h;
}

/* Element at (x, y); EL_STONE outside the grid, so the edges of the world behave like walls. */
static inline ElementId sandbox_at(const Sandbox *sb, int x, int y) {
    return sandbox_inside(sb, x, y) ? (ElementId)sb->cells[y * sb->w + x] : EL_STONE;
}

/* The 8 directions, clockwise from east: 0 = E, 2 = S, 4 = W, 6 = N. Even numbers are the 4 straight ones. */
static inline int sandbox_dx(int dir) {
    static const int t[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    return t[dir & 7];
}
static inline int sandbox_dy(int dir) {
    static const int t[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    return t[dir & 7];
}

/* Fluids can be pushed aside by heavier things: liquids, gases and fire. */
static inline bool sandbox_is_fluid(ElementId id) {
    MoveKind k = sandbox_element(id)->kind;
    return k == MOVE_LIQUID || k == MOVE_GAS || k == MOVE_FIRE;
}

/* How an element deals with heat: conduction, phase changes, self-ignition (sandbox_heat.c). */
typedef struct ThermalDef {
    int16_t temp0; /* temperature of a freshly made cell, in degrees C */
    uint16_t cond; /* how well it passes heat on: 8 = an insulator, 3000 = a very good conductor */
    uint16_t cap;  /* heat capacity: how much heat it takes to warm it by one degree. Air is 2, water 4200 */
    int16_t autoignite; /* catches fire by itself above this temperature; 0 = never */
    int16_t cold_t;     /* at or below this temperature it turns into cold_to ... */
    ElementId cold_to;
    int16_t hot_t; /* ... and at or above this one into hot_to */
    ElementId hot_to;
    uint8_t rate;  /* chance (/256) per tick that a phase change happens once its temperature is reached */
    uint8_t fixed; /* 1 = a heat source or sink: it always sits at temp0 */
    /* extra heat (in steps of two degrees of its own) absorbed at hot_t before the change */
    uint8_t latent_hot;
    /* the same at cold_t. 0 = the change happens at once */
    uint8_t latent_cold;
} ThermalDef;

/* ---- sandbox.c ---- */
int sandbox_rand(Sandbox *sb, int n);         /* 0 .. n-1 */
bool sandbox_chance(Sandbox *sb, int n);      /* true with probability n / 256 */
void sandbox_swap(Sandbox *sb, int a, int b); /* swap two cells with all their state; both count as moved */
void sandbox_put(
    Sandbox *sb, int x, int y, ElementId id,
    uint8_t aux); /* overwrite a cell; it waits a tick. It keeps its temperature (see heat_on_put) */

/* ---- sandbox_move.c ---- */
void sandbox_move(Sandbox *sb, int x, int y);

/* ---- sandbox_heat.c ---- */
const ThermalDef *sandbox_thermal(ElementId id);
void sandbox_heat_clear(Sandbox *sb);       /* every cell back to ambient */
void sandbox_heat_mark(Sandbox *sb, int y); /* row y may now hold something that is not ambient */
/* Put `energy` units of heat into cell i (its temperature rises by energy / capacity). Used by things that
 * make heat themselves, like uranium. */
void sandbox_heat_inject(Sandbox *sb, int i, int energy);
void sandbox_heat_on_put(Sandbox *sb, int i, ElementId id); /* temperature of a cell that just became id */
void sandbox_heat_step(Sandbox *sb);                        /* heat flow for the whole grid */
bool sandbox_heat_react(Sandbox *sb, int x, int y); /* phase change / self-ignition; true = cell changed */

/* ---- sandbox_burst.c ---- */
void sandbox_fly(Sandbox *sb, int x, int y);         /* one tick of a spark or a gamma ray */
void sandbox_detonate(Sandbox *sb, int x, int y);    /* set off the charge at (x, y) and all touching it */
void sandbox_go_critical(Sandbox *sb, int x, int y); /* a uranium cell flies apart as a gamma ray */
bool sandbox_explosive(ElementId id);

/* ---- sandbox_react.c ---- */
void sandbox_react(Sandbox *sb, int x, int y);
void sandbox_life_step(Sandbox *sb);
void sandbox_ignite(Sandbox *sb, int x, int y); /* set a flammable cell alight */
bool sandbox_flammable_now(const Sandbox *sb, int i);

#endif
