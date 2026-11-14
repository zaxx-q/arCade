/* New Star - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * Round 1 shows one star. Tap it: the screen fades to a dark overlay, a NEW star is added, the overlay
 * fades away, and you must tap the NEW star. Every round adds one more star. Tapping a star you
 * already confirmed ends the game. From the 10th star on there is a time limit per round
 * (alarm after 10 s, game over after 15 s). Score = new stars found. Coordinates are playfield-local. */
#ifndef NEW_STAR_LOGIC_H
#define NEW_STAR_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define NS_MAX_STARS 512       /* array size; confirming the 512th star ends the game        */
#define NS_TAP_TOLERANCE 10.0f /* a tap may miss the star's box by 10 px on every side       */
#define NS_TIMED_FROM 10       /* from this many stars on, every round has a time limit      */
#define NS_ALARM_TIME 10.001f  /* seconds after a star appears: shaking and the alarm tone   */
#define NS_LIMIT_TIME 15.001f  /* seconds after which the game is over                       */
#define NS_OVER_DELAY 1.5f     /* seconds the star you missed blinks before is_over()        */
#define NS_ROT_SPEED 150.0f    /* degrees per second, clockwise, every star                  */
#define NS_TYPES 6

typedef struct new_star_State {
    int w, h;
    Vec2 score_pos; /* centre of the score text zone (w/2, h/8): no star is placed near it */
    int n;          /* stars created so far                                    */
    int score;      /* stars confirmed = index of the star to tap next        */
    float x[NS_MAX_STARS], y[NS_MAX_STARS]; /* home centre of each star                      */
    float size[NS_MAX_STARS];               /* width = height                                          */
    float rot[NS_MAX_STARS];                /* rotation in degrees (positive = clockwise)              */
    float shake[NS_MAX_STARS];              /* sideways offset while the alarm shakes the stars        */
    int type[NS_MAX_STARS];                 /* 0..5: shape and face                                    */
    bool overlay;                           /* the dark overlay is visible                             */
    float phase_t;                          /* seconds since the last correct tap (overlay clock)      */
    float new_t;                            /* seconds since the newest star appeared or was confirmed */
    bool alarm;
    bool over, capped, timed_out;
    float over_t;
    int corrects, alarms, created; /* event counters for sound                                */
    Rng rng;
} new_star_State;

void new_star_init(new_star_State *s, unsigned seed);
void new_star_init_sized(new_star_State *s, unsigned seed, int w, int h);
void new_star_step(new_star_State *s, const ArcadeInput *in, float dt);
bool new_star_is_over(const new_star_State *s);
int new_star_score(const new_star_State *s);

/* A tap at a playfield-LOCAL position (step calls this with the converted pointer). */
void new_star_tap(new_star_State *s, float lx, float ly);

/* Pure helpers (tested on their own). */
/* Overlay alpha at `t` seconds after a correct tap: sine fade-in (to 0.999 s), full (to 1.199 s), sine
 * fade-out. */
float new_star_overlay_alpha(float t);
/* The overlay is visible until 2.199 s. */
bool new_star_overlay_visible(float t);
/* Star size in px for a type (size factors 1.0, 0.8, 1.4, 1.2, 1.2, 1.4 times 40). */
float new_star_type_size(int type);

#endif
