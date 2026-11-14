/* Spike Dart - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A round target spins in the middle. Every throw sends a spike up from the bottom; it sticks into
 * the target and then orbits it. A throw loses when it would hit a stuck spike that is passing
 * the launch lane. Score = spikes stuck. Coordinates are playfield-local (see arcade/playfield.h). */
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

/* True when a stuck spike is passing below the target inside the launch lane (spec: y > target_dn and
 * |x - lane| < 8). A spike thrown while this is true during its last 68 px of flight loses. */
bool spike_dart_lane_blocked(const spike_dart_State *s);

#endif
