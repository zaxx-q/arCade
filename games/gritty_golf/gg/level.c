/*
 * level.c  -  runtime level: allocation, reset, wall animation, trigger zones
 */
#include <stdlib.h>
#include <math.h>
#include "level.h"
#include "vec2.h"

/* ------------------------------------------------------------------ *
 * [T09 Array + String]  an ARRAY OF STRINGS: 25 pointers to text.
 * `const char *const` = pointer to constant text, and the pointer
 * itself can never be re-pointed either.
 * ------------------------------------------------------------------ */
static const char *const LEVEL_NAMES[LEVEL_COUNT] = {
    "Tutorial",        "Gentle Slope",   "Drop & Rise",     "U-Valley",
    "Stepped Ascent",  "Low Ceiling",    "Chasm",           "Pillar Run",
    "First Gate",      "Wall Hop",       "High Gate",       "First Pickup",
    "Ricochet Strike", "Deep Well",      "Dual Gates",      "Wide Vista 1",
    "Wide Vista 2",    "Hazard Intro",   "Red Trench",      "Super Wide Course",
    "Hazard Slopes",   "Hazard Funnel",  "Long Gate Trench","Precision Bounces",
    "The Gauntlet"
};

const char *Level_Name(int index)
{
    /* [T05 Percabangan] range check before indexing an array */
    if (index < 0 || index >= LEVEL_COUNT)
        return "?";
    return LEVEL_NAMES[index];
}

/* ================================================================== *
 *  Easing functions                                      [T12 Math]
 *  Input t runs 0 -> 1 (time), output runs 0 -> 1 (distance travelled)
 * ================================================================== */

/* "Back out": overshoots the target a little, then settles.
 *   PSEUDOCODE
 *     s <- 1.70158
 *     u <- t - 1
 *     return u*u*((s+1)*u + s) + 1
 */
float Ease_BackOut(float t)
{
    const float s = 1.70158f;
    float u = t - 1.0f;
    return u * u * ((s + 1.0f) * u + s) + 1.0f;
}

/* "Cubic in-out": slow start, fast middle, slow end.
 *   PSEUDOCODE
 *     IF t < 0.5 THEN return 4 t^3
 *     ELSE            return 1 - (-2t + 2)^3 / 2
 */
float Ease_CubicInOut(float t)
{
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float q = -2.0f * t + 2.0f;
    return 1.0f - (q * q * q) / 2.0f;
}

/* ------------------------------------------------------------------ *
 * [T10 Pointer]  FUNCTION POINTERS
 * `EaseFn` is "a pointer to a function taking float and returning float".
 * The table is indexed by the TweenType enum, so selecting the right curve
 * is ONE array lookup instead of an if/else chain:   EASE_TABLE[type](t)
 * ------------------------------------------------------------------ */
typedef float (*EaseFn)(float);
static const EaseFn EASE_TABLE[] = {
    [TWEEN_BACK_OUT]     = Ease_BackOut,
    [TWEEN_CUBIC_IN_OUT] = Ease_CubicInOut
};

/* ================================================================== *
 *  Life cycle                                          [T10 Pointer]
 * ================================================================== */

/*
 * Level_Create   (allocates memory on the heap)
 *   PSEUDOCODE
 *     IF index is outside 0..24 THEN return NULL
 *     lv <- calloc(1 Level)                    // zero-filled
 *     copy counts from the constant description
 *     allocate one array for walls, buttons, pickup flags
 *     IF any allocation failed THEN free everything, return NULL
 *     Level_Reset(lv); return lv
 */
Level *Level_Create(int index)
{
    if (index < 0 || index >= LEVEL_COUNT)
        return NULL;
    return Level_CreateFromData(&g_levels[index]);  /* & = "address of"            */
}

Level *Level_CreateFromData(const LevelData *def)
{
    if (def == NULL)
        return NULL;

    Level *lv = calloc(1, sizeof *lv);              /* sizeof *lv == sizeof(Level) */
    if (lv == NULL)
        return NULL;

    lv->def          = def;
    lv->wall_count   = lv->def->wall_count;
    lv->button_count = lv->def->button_count;
    lv->strike_count = lv->def->strike_count;

    /* calloc(0, ...) may legally return NULL, so only allocate when needed */
    if (lv->wall_count > 0)
        lv->walls = calloc((size_t)lv->wall_count, sizeof *lv->walls);
    if (lv->button_count > 0)
        lv->buttons = calloc((size_t)lv->button_count, sizeof *lv->buttons);
    if (lv->strike_count > 0)
        lv->strike_active = calloc((size_t)lv->strike_count, sizeof *lv->strike_active);

    bool failed = (lv->wall_count   > 0 && lv->walls         == NULL) ||
                  (lv->button_count > 0 && lv->buttons       == NULL) ||
                  (lv->strike_count > 0 && lv->strike_active == NULL);
    if (failed) {
        Level_Destroy(lv);
        return NULL;
    }

    /* link every runtime object to its constant definition */
    for (int i = 0; i < lv->wall_count; i++)
        lv->walls[i].def = &lv->def->walls[i];
    for (int i = 0; i < lv->button_count; i++)
        lv->buttons[i].def = &lv->def->buttons[i];

    Level_Reset(lv);
    return lv;
}

/* Every malloc/calloc needs exactly one free.  free(NULL) is harmless. */
void Level_Destroy(Level *lv)
{
    if (lv == NULL)
        return;
    free(lv->walls);
    free(lv->buttons);
    free(lv->strike_active);
    free(lv);
}

void Level_Reset(Level *lv)
{
    /* [T06 Perulangan] three independent for-loops over three arrays */
    for (int i = 0; i < lv->wall_count; i++) {
        lv->walls[i].offset  = V2(0.0f, 0.0f);
        lv->walls[i].elapsed = 0.0f;
        lv->walls[i].opening = false;
    }
    for (int i = 0; i < lv->button_count; i++)
        lv->buttons[i].pressed = false;
    for (int i = 0; i < lv->strike_count; i++)
        lv->strike_active[i] = true;
}

/* ================================================================== *
 *  Per-tick behaviour
 * ================================================================== */

/*
 * Level_Update : slide every opening wall along its path.
 *   PSEUDOCODE
 *     FOR each wall
 *        IF wall is opening THEN
 *            elapsed <- elapsed + dt
 *            t       <- clamp(elapsed / duration, 0, 1)
 *            offset  <- displacement * EASE[type](t)
 *            IF t = 1 THEN opening <- false
 */
void Level_Update(Level *lv, float dt)
{
    for (int i = 0; i < lv->wall_count; i++) {
        WallState *w = &lv->walls[i];               /* pointer to ONE element */
        if (!w->opening)
            continue;                                /* [T06] skip this iteration */

        w->elapsed += dt;                            /* [T04] compound assignment */
        float t = Clampf(w->elapsed / w->def->tween_duration, 0.0f, 1.0f);
        float f = EASE_TABLE[w->def->tween_type](t); /* call through the table    */
        w->offset = V2_Scale(w->def->open_displacement, f);
        if (t >= 1.0f)
            w->opening = false;
    }
}

/*
 * Level_WallCorners : the 4 world-space corners of a wall's collision box.
 *   A wall is stored as   pivot + rotation + width + height
 *   with its collider centred at local (0, 78).  So the local corners are
 *        (-W/2, cy-H/2) (+W/2, cy-H/2) (+W/2, cy+H/2) (-W/2, cy+H/2)
 *   and each is rotated, then moved to (pivot + current slide offset).
 *
 *   [T09 Array]   `Vec2 out[4]` is really a POINTER to the caller's array,
 *                 so the function can fill it (an "output parameter").
 */
void Level_WallCorners(const WallState *w, Vec2 out[4])
{
    const WallData *d = w->def;
    float hw = d->width  * 0.5f;
    float hh = d->height * 0.5f;
    float cx = d->collider_offset.x;
    float cy = d->collider_offset.y;

    Vec2 local[4] = {
        { cx - hw, cy - hh }, { cx + hw, cy - hh },
        { cx + hw, cy + hh }, { cx - hw, cy + hh }
    };
    Vec2 origin = V2_Add(d->position, w->offset);

    for (int i = 0; i < 4; i++)
        out[i] = V2_Add(origin, V2_Rotate(local[i], d->rotation));
}

float Level_WorldSize(const Level *lv)
{
    return (float)VIRTUAL_SIZE / lv->def->camera_zoom;   /* 256 / zoom */
}

/*
 * Level_CheckTriggers : flag, strike pickups, buttons.
 *   Every trigger is a CIRCLE; the ball touches it when the distance between
 *   the two centres is <= ball radius + trigger radius.
 *   Returns the events as a bit mask (see GameEvent in common.h).
 */
unsigned Level_CheckTriggers(Level *lv, Vec2 ball_pos)
{
    unsigned ev = EVT_NONE;

    /* --- flag pole ------------------------------------------------- */
    if (V2_Dist(ball_pos, lv->def->flagpole_pos) <= BALL_RADIUS + FLAG_RADIUS)
        ev |= EVT_GOAL;                                   /* [T04] OR sets the bit */

    /* --- strike pickups ------------------------------------------- */
    for (int i = 0; i < lv->strike_count; i++) {
        if (!lv->strike_active[i])
            continue;
        if (V2_Dist(ball_pos, lv->def->strikes[i]) <= BALL_RADIUS + STRIKE_RADIUS) {
            lv->strike_active[i] = false;                 /* disappears until reset */
            ev |= EVT_PICKUP;
        }
    }

    /* --- buttons (each one may open a wall) ------------------------ */
    for (int i = 0; i < lv->button_count; i++) {
        ButtonState *b = &lv->buttons[i];
        if (b->pressed)
            continue;
        if (V2_Dist(ball_pos, b->def->position) <= BALL_RADIUS + BUTTON_RADIUS) {
            b->pressed = true;
            ev |= EVT_BUTTON;

            int target = b->def->target_wall_index;
            if (target >= 0 && target < lv->wall_count) {
                WallState *w = &lv->walls[target];
                w->opening = true;
                w->elapsed = 0.0f;
                if (w->def->play_hit_sfx)
                    ev |= EVT_WALL_OPEN;
            }
        }
    }
    return ev;
}
