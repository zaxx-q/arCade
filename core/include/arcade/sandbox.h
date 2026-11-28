/* Falling-sand engine (pure C, no raylib). Used by the hub backdrop AND deferred/sandbox.
 *
 * The engine is split over a few files so each one stays readable:
 *   sandbox.c           grid basics (init / get / set / paint / count) and the per-tick loop
 *   sandbox_elements.c  the element table: name, colour, density, how it moves, how it reacts
 *   sandbox_move.c      movement rules for powders, liquids, gases, fire, rays and crawlers
 *   sandbox_react.c     chemistry: fire, lava, acid, plants, explosives, uranium, termites, life
 *   sandbox_heat.c      temperature: conduction, insulators, phase changes, auto-ignition
 *   sandbox_burst.c     explosions and radiation: sparks that fly out, plough through things and burn
 *   sandbox_color.c     the colour of one cell (shading, glow, heat view) so renderers stay trivial
 *
 * To add an element:
 *   1. add an ElementId before EL_COUNT                      (this file)
 *   2. add a row to k_elements in sandbox_elements.c         (name, colour, density, movement)
 *   3. if it does something special, add a case in sandbox_react() (sandbox_react.c)
 *   4. add a test in tests/test_sandbox.c
 * See docs/SANDBOX.md for the details. */
#ifndef ARCADE_SANDBOX_H
#define ARCADE_SANDBOX_H

#include "arcade/common.h"
#include "arcade/rng.h"

#define SANDBOX_MAX_W 320
#define SANDBOX_MAX_H 180

/* Order matters: it is the order of the palette and of the number keys. Keys 0-3 still pick the original
 * four (Eraser, Sand, Water, Stone). EL_EMPTY must stay 0 because empty memory means "nothing here". */
typedef enum ElementId {
    EL_EMPTY = 0, /* shown as "Eraser" in the palette */
    EL_SAND,
    EL_WATER,
    EL_STONE,
    /* powders */
    EL_SOIL,
    EL_SNOW,
    EL_GUNPOWDER,
    EL_SEED,
    EL_ASH,
    EL_THERMITE,
    EL_GRAVEL,
    /* liquids */
    EL_OIL,
    EL_ACID,
    EL_LAVA,
    EL_MERCURY,
    EL_NITROGEN,
    /* gases */
    EL_STEAM,
    EL_METHANE,
    EL_SMOKE,
    /* solids */
    EL_WOOD,
    EL_ICE,
    EL_GLASS,
    EL_WALL,
    EL_STEEL, /* conducts heat very well */
    EL_BRICK, /* hardly conducts heat at all: an insulator */
    EL_PLANT,
    EL_FUSE,
    EL_TNT,
    /* energetic and alive */
    EL_FIRE,
    EL_HEATER, /* a block that is always 700 degrees */
    EL_COOLER, /* a block that is always -80 degrees */
    EL_URANIUM,
    EL_TERMITE,
    EL_LIFE,
    /* hidden: only made by reactions, never listed in the palette */
    EL_GAMMA,          /* a ray of radiation: flies far and burns through everything but walls */
    EL_MOLTEN_URANIUM, /* uranium that got hot enough to melt */
    EL_SPARK,          /* a fragment thrown out by an explosion */
    EL_BASALT,         /* the dark rock that lava turns into when it cools */
    EL_COUNT
} ElementId;

typedef enum MoveKind {
    MOVE_NONE = 0, /* never moves on its own (solids) */
    MOVE_POWDER,   /* falls, piles up, sinks through lighter liquids */
    MOVE_LIQUID,   /* falls, then flows sideways towards the nearest drop */
    MOVE_GAS,      /* rises and drifts; the mirror image of a liquid */
    MOVE_FIRE,     /* flickers upwards and burns out */
    MOVE_RAY,      /* flies in a straight line (sparks and gamma rays) */
    MOVE_CRAWL     /* clings to surfaces and walks along them (termites) */
} MoveKind;

/* ElementDef.flags */
#define EF_HIDDEN 0x01    /* not shown in the palette, not reachable with the number keys */
#define EF_ACIDPROOF 0x02 /* acid cannot dissolve it */
#define EF_HOT 0x04       /* heat source: lights flammable neighbours and melts ice and snow */

typedef struct ElementDef {
    const char *name;
    uint8_t r, g, b; /* base colour; sandbox_pixel() adds shading and glow on top */
    int density;     /* heavier things sink through lighter fluids */
    MoveKind kind;
    uint8_t flags;  /* EF_* */
    uint8_t speed;  /* powders/liquids: cells per tick when falling; gases: chance (out of 256) to rise */
    uint8_t spread; /* liquids: how far sideways to look for a place to flow to */
    uint8_t slip;   /* liquids: chance (out of 256) to move at all each tick (low = thick, like lava) */
    uint8_t grip;   /* powders: 0 slides off any edge; 1 needs a 2 cell drop; higher = steeper piles */
    uint8_t ignite; /* chance (out of 256) per tick to catch fire next to a hot cell; 0 = not flammable */
    uint8_t shade;  /* amount of per-cell brightness noise, so big areas do not look flat */
    uint8_t spray;  /* brush density: chance (out of 256) that a painted cell is filled */
} ElementDef;

/* Temperatures are degrees Celsius. Every cell has one, empty air included: air holds almost no heat, so
 * it follows whatever touches it, but a hot object still warms the air around it. */
#define SANDBOX_AMBIENT 20
#define SANDBOX_TEMP_MIN (-273)
#define SANDBOX_TEMP_MAX 9000

/* Per-cell memory. `aux` means something different for each element (fire: remaining life, plant: growth
 * budget, wood: burn timer, ...). It is 0 for a freshly painted cell. See sandbox_react.c. `temp` is the
 * cell's temperature in whole degrees and `tfrac` the part of a degree on top of it, in 1/65536 (the heat
 * model needs the fraction: lava in free air loses well under a degree per tick). Both travel with the cell
 * when it moves (see sandbox_heat.c). */
typedef struct Sandbox {
    int w, h;
    uint8_t cells[SANDBOX_MAX_W * SANDBOX_MAX_H];
    uint8_t aux[SANDBOX_MAX_W * SANDBOX_MAX_H];
    /* scratch: bit 0 = moved this tick, bit 1 = moved last tick, bits 3-6 = the heading of a flying spark or
     * ray (see sandbox_internal.h); all of it travels with the cell */
    uint8_t moved[SANDBOX_MAX_W * SANDBOX_MAX_H];
    int16_t temp[SANDBOX_MAX_W * SANDBOX_MAX_H];
    uint16_t tfrac[SANDBOX_MAX_W * SANDBOX_MAX_H];
    uint8_t heat_rows[SANDBOX_MAX_H]; /* 1 = this row may hold something not at ambient */
    bool heat_view;                   /* render as a thermal map instead of the normal colours */
    unsigned tick;
    Rng rng;
} Sandbox;

const ElementDef *sandbox_element(ElementId id);
void sandbox_init(Sandbox *sb, int w, int h, unsigned seed); /* w,h are clamped to the max size */
void sandbox_clear(Sandbox *sb);
ElementId sandbox_get(const Sandbox *sb, int x, int y); /* EL_STONE outside the grid */
void sandbox_set(Sandbox *sb, int x, int y, ElementId id);
/* Circular brush. EL_EMPTY erases everything; other elements only fill empty cells. */
void sandbox_paint(Sandbox *sb, int cx, int cy, int radius, ElementId id);
void sandbox_step(Sandbox *sb); /* advance the simulation one tick */
int sandbox_count(const Sandbox *sb, ElementId id);
/* Like sandbox_count, but only inside the w x h rectangle whose top-left cell is (x, y).
 * The rectangle is clipped to the grid. Used by the hub's vault to measure how full it is. */
int sandbox_count_in(const Sandbox *sb, int x, int y, int w, int h, ElementId id);

/* Temperature of a cell in whole degrees Celsius, air included (SANDBOX_AMBIENT outside the grid). */
int sandbox_temp(const Sandbox *sb, int x, int y);
/* Set or change the temperature of a cell (clamped to the allowed range). */
void sandbox_set_temp(Sandbox *sb, int x, int y, int degrees);
void sandbox_add_heat(Sandbox *sb, int x, int y, int degrees);
/* Throw `sparks` sparks out from (x, y) in random directions; they start in the cells around that point,
 * replacing whatever is there except walls and steel. A spark flies one cell per tick for 32 to 159
 * ticks, leaves flames behind it and ploughs through nearly everything in its way; only walls and steel stop
 * it. This is what an exploding charge is made of (see sandbox_burst.c); the call is public so tools and
 * tests can set off a blast without building a charge. */
void sandbox_burst(Sandbox *sb, int x, int y, int sparks);

/* The on-screen colour of cell (x, y) as 0xRRGGBB. Pure: same cell, same tick => same colour. */
uint32_t sandbox_pixel(const Sandbox *sb, int x, int y);
/* Number of elements the palette shows (hidden ones skipped) and the id of the i-th one. */
int sandbox_palette_count(void);
ElementId sandbox_palette_id(int index);
/* Index of `id` in the palette, or -1 for a hidden element. */
int sandbox_palette_index(ElementId id);

#endif
