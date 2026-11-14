/* Tower Stacker - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A bar slides left and right above a tower. Drop it: the overhang is cut off (it falls away and
 * fades) and the rest becomes the new top floor, so the tower gets narrower. A perfect landing
 * keeps the full width. Missing the tower completely ends the game. Score = floors stacked.
 * Coordinates are playfield-local (y grows downward).
 *
 * D6 EXCEPTION: the bar moves in whole 5 px steps on a 50 Hz sub-clock (not scaled by dt), so its x
 * always has the same remainder (mod 5) as the floor edge below it and a perfect is always reachable. */
#ifndef TOWER_STACKER_LOGIC_H
#define TOWER_STACKER_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define TS_MAX_FLOORS 1023       /* arrays hold 1024 floors; reaching 1023 ends the game      */
#define TS_WINDOWS 3             /* up to 3 random windows per floor                          */
#define TS_SCROLL_FLOORS 10      /* from this floor on the tower scrolls instead of rising    */
#define TS_ZOOM_TIME 2.5f        /* seconds of the zoom-to-fit after a game over with >= 10 floors */
#define TS_OVER_DELAY_SHORT 0.8f /* reveal delay when the tower is short                      */

/* What the bar is doing. */
#define TS_SLIDING 0 /* moving sideways, waiting for the drop                */
#define TS_FALLING 1 /* frozen in x, falling onto the tower                  */
#define TS_FADING 2  /* the cut-off piece falls slowly and fades (0.5 s)     */
#define TS_MISSED 3  /* landed with no overlap: game over                    */

typedef struct tower_stacker_State {
    int w, h;
    /* constants derived from the playfield size */
    float base_w0, base_h0, base_x0, bar_y0, speed_x, speed_y;
    /* the tower's top floor */
    float base_x, base_w, base_h;
    float base_y;      /* landing line = top edge of the top floor (stops rising after floor 9) */
    float base_y_move; /* where the tower's top is drawn now (scrolls down after floor 9)      */
    int score;         /* floors above the base floor                                           */
    int bases_x[TS_MAX_FLOORS + 1]; /* every floor's left edge and width, rounded half up                   */
    int bases_w[TS_MAX_FLOORS + 1];
    int window[TS_MAX_FLOORS + 1][TS_WINDOWS]; /* x of each window, 0 = none */
    /* the bar (or the cut-off piece while fading) */
    int state;
    float bar_x, bar_y; /* left edge and BOTTOM edge                                             */
    float bar_w, bar_h;
    float bar_spx, bar_spy; /* spx in px per tick (+-5 or 0), spy in px per tick                     */
    float bar_alpha;
    float bar_acc; /* 50 Hz sub-clock for the 5 px steps                                    */
    bool over, capped;
    float over_t;
    int perfects, cuts, landings; /* event counters for sound                                              */
    int last_cut_class;           /* 0: d < 10, 1: d < 30, 2: else (which "lost part" tone to play)        */
    float last_cut_d;
    Rng rng;
} tower_stacker_State;

void tower_stacker_init(tower_stacker_State *s, unsigned seed);
void tower_stacker_init_sized(tower_stacker_State *s, unsigned seed, int w, int h);
void tower_stacker_step(tower_stacker_State *s, const ArcadeInput *in, float dt);
bool tower_stacker_is_over(const tower_stacker_State *s);
int tower_stacker_score(const tower_stacker_State *s);

/* Zoom-to-fit after a game over with >= 10 floors: top y and floor height at `over_t` seconds.
 * With fewer floors (or over_t = 0) it returns the normal drawing: top = base_y_move, height = base_h. */
void tower_stacker_zoom(const tower_stacker_State *s, float *top_y, float *floor_h);

#endif
