/*
 * common.h  -  constants, enums and small shared types
 *
 * Every other module includes this file.  It contains NO functions, only
 * declarations that the compiler needs to know about everywhere.
 *
 *   [T01 Pengantar]  header file + include guard, #include, #define
 *   [T03 Tipe Data]  int / float / unsigned / bool / size_t / const / typedef
 *   [T13 Enums]      every enum of the game lives here
 *   [T04 Operator]   the "1u << n" bit-flag idiom (bitwise shift)
 */
#ifndef GG_COMMON_H          /* include guard: stops the file being read twice */
#define GG_COMMON_H

#include <stdbool.h>         /* bool, true, false                         */
#include <stdint.h>          /* uint8_t, uint32_t ... (exact-width ints)  */
#include <stddef.h>          /* size_t, NULL                              */

#include "levels.h"          /* Vec2, LevelData, WallData ... (provided)  */
#include "palette_colors.h"   /* PAL_BG, PAL_LAND, PAL_INK (generated)     */

/* ------------------------------------------------------------------ *
 * [T01/T03] Compile-time constants
 *   #define : textual substitution done by the preprocessor, no type.
 *   const   : a real typed value the compiler can check.
 * ------------------------------------------------------------------ */
#define VIRTUAL_SIZE        256     /* the game is drawn on a 256x256 canvas   */
#define PHYSICS_HZ          60      /* fixed simulation rate                   */
#define LEVEL_COUNT         GOLF_LEVEL_COUNT          /* 25, from levels.h     */
#define STRIKES_MAX         3
#define TRAIL_LEN           8       /* motion-trail history (ring buffer size) */
#define PLAYER_NAME_MAX     24      /* char name[24] -> max 23 letters + '\0'  */
#define MAX_PREVIEW_POINTS  40      /* aim-assist dots                         */
#define MAX_POLY_POINTS     128     /* biggest polygon in data is 64 points    */
#define PHYSICS_SUBSTEPS    4       /* collision sub-steps per 1/60 s tick     */

#define SAVE_PATH           "gritty_save.dat"
#define SETTINGS_PATH       "gritty_settings.cfg"
#define SCORES_PATH         "gritty_scores.csv"

/* typed constants (floating point values are better as const than #define) */
static const float GG_PI           = 3.14159265358979323846f;
static const float PHYSICS_DT      = 1.0f / (float)PHYSICS_HZ;   /* [T04] float division + cast */
static const float GRAVITY         = 980.0f;     /* px / s^2, pointing down      */
static const float BALL_RADIUS     = 4.0f;
static const float BOUNCE          = 0.33f;      /* coefficient of restitution   */
static const float LAUNCH_SCALE    = 6.0f;       /* power (0..100) -> px/s       */
static const float AIM_SPEED       = 1.5f * 3.14159265358979323846f; /* rad/s    */
static const float POWER_SPEED     = 200.0f;     /* power units / s              */
static const float FLAG_RADIUS     = 8.0f;
static const float STRIKE_RADIUS   = 16.0312f;
static const float BUTTON_RADIUS   = 18.0f;
static const float REST_VELOCITY   = 30.0f;      /* below this impact: no bounce */
static const float SFX_IMPACT_MIN  = 70.0f;      /* below this impact: silent    */

/* ------------------------------------------------------------------ *
 * [T13 Enums] named integer constants.  Far more readable than "if (s==2)".
 * ------------------------------------------------------------------ */
typedef enum {
    BALL_STATE_MOVING = 0,      /* free flight under gravity                  */
    BALL_STATE_AIMING = 1,      /* frozen, direction arrow is sweeping        */
    BALL_STATE_POWER  = 2       /* frozen, angle locked, power bar oscillates */
} BallState;

typedef enum {
    TWEEN_BACK_OUT      = 0,    /* wall slides with a little overshoot        */
    TWEEN_CUBIC_IN_OUT  = 1     /* smooth start and smooth stop               */
} TweenType;

typedef enum {                  /* the six sound effects                      */
    SFX_BALL_HIT = 0,
    SFX_BALL_LANDED,
    SFX_BALL_OOB,
    SFX_LEVEL_BEAT,
    SFX_POWER,
    SFX_POWER_2,
    SFX_COUNT                   /* trick: last entry == number of entries     */
} SfxId;

typedef enum {
    SCREEN_SPLASH = 0,
    SCREEN_PLAY,
    SCREEN_ENDING
} ScreenId;

typedef enum {
    TR_NONE = 0,                /* no transition running                      */
    TR_COVER,                   /* diamonds growing, hiding the old level     */
    TR_REVEAL                   /* diamonds shrinking, showing the new level  */
} TransitionPhase;

/* ------------------------------------------------------------------ *
 * [T04 Operator / T13 Enums]  BIT FLAGS
 * Each event is one bit, so many events can be packed in ONE unsigned int:
 *     unsigned ev = EVT_BOUNCE | EVT_GOAL;      // bitwise OR  sets bits
 *     if (ev & EVT_GOAL) ...                    // bitwise AND tests a bit
 *     ev &= ~EVT_BOUNCE;                        // AND + NOT   clears a bit
 * ------------------------------------------------------------------ */
typedef enum {
    EVT_NONE         = 0,
    EVT_BOUNCE       = 1u << 0,   /* ball hit terrain/wall hard enough to hear */
    EVT_HAZARD       = 1u << 1,   /* ball touched a death polygon              */
    EVT_OOB          = 1u << 2,   /* ball left the visible area                */
    EVT_BUTTON       = 1u << 3,   /* a button was pressed                      */
    EVT_WALL_OPEN    = 1u << 4,   /* ... and its wall wants the "hit" sound    */
    EVT_PICKUP       = 1u << 5,   /* strike pickup collected                   */
    EVT_GOAL         = 1u << 6,   /* ball reached the flag                     */
    EVT_AIM_START    = 1u << 7,   /* MOVING -> AIMING                          */
    EVT_ANGLE_LOCK   = 1u << 8,   /* AIMING -> POWER                           */
    EVT_POWER_BOUNCE = 1u << 9,   /* power bar reached 0 or 100                */
    EVT_LAUNCH       = 1u << 10   /* POWER  -> MOVING (ball was hit)           */
} GameEvent;

/* Colours used by the CPU rasteriser (no raylib needed for logic/tests).
 * The three palette colours are 0xRRGGBB numbers; these macros split them into bytes.
 *   [T04 Operator]  >> shifts the wanted byte down, & 0xFF keeps only that byte      */
typedef struct { uint8_t r, g, b, a; } Rgba;

#define PAL_R(hex)  ((uint8_t)(((hex) >> 16) & 0xFFu))
#define PAL_G(hex)  ((uint8_t)(((hex) >>  8) & 0xFFu))
#define PAL_B(hex)  ((uint8_t)( (hex)        & 0xFFu))

#define COL_BG      { PAL_R(PAL_BG),   PAL_G(PAL_BG),   PAL_B(PAL_BG),   255 }  /* sky, text outline */
#define COL_LAND    { PAL_R(PAL_LAND), PAL_G(PAL_LAND), PAL_B(PAL_LAND), 255 }  /* terrain, accents  */
#define COL_INK     { PAL_R(PAL_INK),  PAL_G(PAL_INK),  PAL_B(PAL_INK),  255 }  /* ball, flag, text  */
#define COL_OUTLINE COL_INK                                                     /* hazard outline    */

#endif /* GG_COMMON_H */
