/* Falling-sand engine (pure C, no raylib). Used by the hub backdrop AND games/sandbox.
 *
 * To add an element:
 *   1. add an ElementId before EL_COUNT          (this file)
 *   2. add a row to k_elements in sandbox.c       (name, colour, density, behaviour)
 *   3. if it needs a NEW behaviour, add a MoveKind and a case in sandbox_step()
 *   4. add a test in tests/test_sandbox.c
 * See docs/SANDBOX.md for ready-made ideas (fire, oil, lava, plant, acid). */
#ifndef ARCADE_SANDBOX_H
#define ARCADE_SANDBOX_H

#include "arcade/common.h"
#include "arcade/rng.h"

#define SANDBOX_MAX_W 320
#define SANDBOX_MAX_H 180

typedef enum ElementId { EL_EMPTY = 0, EL_SAND, EL_WATER, EL_STONE, EL_COUNT } ElementId;
typedef enum MoveKind { MOVE_NONE = 0, MOVE_POWDER, MOVE_LIQUID } MoveKind;

typedef struct ElementDef {
    const char *name;
    uint8_t r, g, b;
    int density; /* heavier things sink through lighter liquids */
    MoveKind kind;
} ElementDef;

typedef struct Sandbox {
    int w, h;
    uint8_t cells[SANDBOX_MAX_W * SANDBOX_MAX_H];
    uint8_t moved[SANDBOX_MAX_W * SANDBOX_MAX_H]; /* scratch: cell already moved this tick */
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

#endif
