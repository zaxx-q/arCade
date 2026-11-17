/* Spike Dart - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A round target spins in the middle. Every throw sends a spike up from the bottom; it sticks into
 * the target and then orbits it. A throw loses when the spike it throws TOUCHES a stuck spike: the hit
 * box is exactly the triangle that is drawn (spike_dart_shape), so the game never says "hit" while the
 * spikes look apart. Score = spikes stuck. Coordinates are playfield-local (see arcade/playfield.h). */
#ifndef SPIKE_DART_LOGIC_H
#define SPIKE_DART_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define SD_MAX_SPIKES 64   /* array cap; the 64th stuck spike ends the game */
#define SD_TARGET_R 120.0f /* radius of the target disc                      */
#define SD_ORBIT_R 152.0f  /* distance of a stuck spike's centre from the target centre */
#define SD_SPIKE_LEN 80.0f /* spike is 80 px long and 8 px wide                         */
#define SD_SPIKE_W 8.0f
#define SD_OVER_DELAY 1.2f /* seconds the losing spike stays on screen before is_over() */
#define SD_HIT_SLACK                                                                                         \
    1.5f /* two spikes must overlap by MORE than this many px to count as a hit,                             \
            so a graze that looks like a near miss is forgiven                   */

typedef struct spike_dart_State {
    int w, h;
    /* derived from the playfield height in init_sized */
    float target_x, target_y, target_dn;        /* target centre and the y of its bottom edge */
    float spike_x, spike_y;                     /* where every new spike waits (centre)       */
    float spike_spd;                            /* flight speed in px per original tick       */
    int stuck;                                  /* spikes stuck in the target = score         */
    float px[SD_MAX_SPIKES], py[SD_MAX_SPIKES]; /* centre of each stuck spike          */
    float rot[SD_MAX_SPIKES];                   /* orbit angle in radians (pi/2 = bottom)     */
    float cur_y;                                /* y of the spike that waits or flies         */
    bool flying;
    bool over;   /* lane hit or cap reached                    */
    bool capped; /* over because the 64th spike stuck          */
    float over_t;
    int throws, sticks; /* event counters for sound                   */
    Rng rng;            /* unused (no randomness) but kept for the contract */
} spike_dart_State;

void spike_dart_init(spike_dart_State *s, unsigned seed);
void spike_dart_init_sized(spike_dart_State *s, unsigned seed, int w, int h);
void spike_dart_step(spike_dart_State *s, const ArcadeInput *in, float dt);
bool spike_dart_is_over(const spike_dart_State *s);
int spike_dart_score(const spike_dart_State *s);

/* The drawn (and hit-tested) shape of a spike: a triangle whose tip points at the target centre.
 * (cx, cy) is the spike's centre and `angle` its orbit angle (pi/2 = hanging below the target).
 * tri[0] = tip, tri[1] and tri[2] = the two corners of the 8 px wide flat end. The front-end draws
 * exactly these three points, so what you see is what can hit you. */
void spike_dart_shape(float cx, float cy, float angle, Vec2 tri[3]);

/* True when the spike we throw, with its centre at (spike_x, y), would touch a stuck spike right now.
 * This is the one hit test of the game: step() calls it all along the flight. */
bool spike_dart_would_touch(const spike_dart_State *s, float y);

/* True when a spike that reached the target right now would touch a stuck spike: the landing spot
 * (just below the target) is occupied. It is spike_dart_would_touch() at the landing pose. */
bool spike_dart_lane_blocked(const spike_dart_State *s);

#endif
