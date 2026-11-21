/* Private helpers shared by the core/src/sandbox*.c files. Not part of the public API: games and the hub
 * only include arcade/sandbox.h. */
#ifndef ARCADE_SANDBOX_INTERNAL_H
#define ARCADE_SANDBOX_INTERNAL_H

#include "arcade/sandbox.h"

/* Bits of Sandbox.moved. A cell that already acted this tick is skipped, so nothing moves twice. */
#define MOVED_NOW 1u
#define MOVED_LAST 2u
#define EXCITED 4u /* uranium hit by a gamma ray: it splits on its next turn (kept across ticks) */
#define BLASTED 8u /* already hit by a shock wave this tick: a cell is pushed at most once per tick */
#define SANDBOX_BLASTS_PER_TICK                                                                              \
    12 /* TNT cells that may go off in one tick; the rest of a big stack waits                               \
        */

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

/* How an element deals with heat and with blasts (sandbox_heat.c). */
typedef struct ThermalDef {
    int16_t temp0; /* temperature of a freshly made cell, in degrees C */
    uint8_t cond;  /* conductivity: 0 insulates, 62 is a very good conductor */
    uint8_t cap;   /* heat capacity: how much heat it takes to warm it by one degree (1..16) */
    uint8_t loss; /* heat lost to open air through each exposed face, in 1/16384 of the difference per tick */
    int16_t autoignite; /* catches fire by itself above this temperature; 0 = never */
    uint8_t resist;     /* blast: energy a shock wave loses crossing the cell */
    uint8_t tough;      /* blast: energy needed to break the cell (255 = nothing can) */
    int16_t cold_t;     /* at or below this temperature it turns into cold_to ... */
    ElementId cold_to;
    int16_t hot_t; /* ... and at or above this one into hot_to */
    ElementId hot_to;
    uint8_t rate;  /* chance (/256) per tick that a phase change happens once its temperature is reached */
    uint8_t fixed; /* 1 = a heat source or sink: it always sits at temp0 */
    uint8_t latent_hot;  /* heat (in units of 16) absorbed at hot_t before the change happens; 0 = at once */
    uint8_t latent_cold; /* the same at cold_t */
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
void sandbox_heat_on_put(Sandbox *sb, int i, ElementId id); /* temperature of a cell that just became id */
void sandbox_heat_step(Sandbox *sb);                        /* conduction + cooling for the whole grid */
bool sandbox_heat_react(Sandbox *sb, int x, int y); /* phase change / self-ignition; true = cell changed */

/* ---- sandbox_blast.c ---- */
void sandbox_blast_step(Sandbox *sb); /* advance a travelling shock wave and fade the flash */

/* ---- sandbox_react.c ---- */
void sandbox_react(Sandbox *sb, int x, int y);
void sandbox_life_step(Sandbox *sb);
void sandbox_gamma_hit(Sandbox *sb, int x, int y); /* a gamma ray just reached cell (x, y) */
void sandbox_ignite(Sandbox *sb, int x, int y);    /* set a flammable cell alight */
bool sandbox_flammable_now(const Sandbox *sb, int i);

#endif
