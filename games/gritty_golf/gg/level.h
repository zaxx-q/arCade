/*
 * level.h  -  the RUNTIME state of one level
 *
 * data/levels.c holds the READ-ONLY description of all 25 levels (const).
 * A `Level` is the mutable copy of what changes while you play: which buttons
 * are pressed, how far each wall has slid, which pickups were collected.
 *
 *   [T10 Pointer]   Level* is created on the HEAP with calloc() and must be
 *                   released with free() -> Level_Destroy()
 *   [T13 Struct]    WallState / ButtonState / Level
 */
#ifndef GG_LEVEL_H
#define GG_LEVEL_H

#include "common.h"

typedef struct {
    const WallData *def;     /* pointer to the constant definition           */
    Vec2   offset;           /* how far the wall has moved from its start    */
    float  elapsed;          /* seconds since the button was pressed         */
    bool   opening;          /* true while the tween is running              */
} WallState;

typedef struct {
    const ButtonData *def;
    bool pressed;
} ButtonState;

typedef struct {
    const LevelData *def;    /* read-only description (spawn, polygons ...)  */
    int          wall_count;
    int          button_count;
    int          strike_count;
    WallState   *walls;          /* arrays allocated at run time (heap)      */
    ButtonState *buttons;
    bool        *strike_active;  /* true = pickup is still lying there       */
} Level;

/* ---- life cycle --------------------------------------------------- */
Level *Level_Create(int index);            /* one of the 25 built-in levels; NULL if invalid */
Level *Level_CreateFromData(const LevelData *def);   /* any description (used by the tests) */
void   Level_Destroy(Level *lv);
void   Level_Reset(Level *lv);             /* buttons, walls, pickups -> start */

/* ---- per-tick behaviour ------------------------------------------- */
void     Level_Update(Level *lv, float dt);              /* animate the walls  */
unsigned Level_CheckTriggers(Level *lv, Vec2 ball_pos); /* returns EVT_* mask */

/* ---- queries ------------------------------------------------------ */
void        Level_WallCorners(const WallState *w, Vec2 out[4]);
float       Level_WorldSize(const Level *lv);   /* visible world = 256/zoom   */
const char *Level_Name(int index);

/* ---- easing curves (used through a table of function pointers) ---- */
float Ease_BackOut(float t);
float Ease_CubicInOut(float t);

#endif /* GG_LEVEL_H */
