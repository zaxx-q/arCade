/* Post It - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A yellow post-it (the head) sits fixed in the middle. 8 coloured post-its and 16 viruses drift
 * around it; the pointer offset from the centre (or the arrow keys) "tilts" the world.
 * A post-it that touches ANY block of your cluster is glued on (+1). A virus that touches ANY block
 * ends the game. Score = blocks in the cluster (starts at 1). Coordinates are playfield-local. */
#ifndef POST_IT_LOGIC_H
#define POST_IT_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define PI_MAX_BLOCKS 256  /* cluster cap: reaching it ends the game (the original would crash) */
#define PI_GREENS 8        /* free post-its  */
#define PI_REDS 16         /* viruses        */
#define PI_HIT_R 24.0f     /* grab / death distance, centre to centre */
#define PI_NEAR_R 48.0f    /* a virus closer than this blinks and sounds the alarm */
#define PI_COLORS 8        /* post-it colour indices 0..7 (the front-end picks the actual colours) */
#define PI_OVER_DELAY 1.0f /* seconds the front-end may show the killer virus before is_over() */

typedef struct post_it_State {
    int w, h;
    Vec2 head;            /* centre of the playfield: block 0 never moves */
    float spx, spy;       /* world drift in px per original tick (|v| <= 4) */
    float tilt_x, tilt_y; /* last tilt (-1..1), for the front-end display   */
    int blockn;           /* blocks in the cluster, head included = score   */
    float bx[PI_MAX_BLOCKS], by[PI_MAX_BLOCKS];
    float brot[PI_MAX_BLOCKS]; /* degrees */
    int bcol[PI_MAX_BLOCKS];   /* colour index 0..7 (block 0 is the head: front-end draws it yellow) */
    float gx[PI_GREENS], gy[PI_GREENS], grot[PI_GREENS];
    int gcol[PI_GREENS];
    float rx[PI_REDS], ry[PI_REDS], rrot[PI_REDS];
    bool rnear[PI_REDS]; /* virus is within 48 px of a block */
    int near_count;      /* how many viruses are near: the alarm sounds while > 0 */
    bool paused;
    bool over, capped;
    int killer; /* index of the virus that ended the game, -1 if none */
    float over_t;
    int grabs, last_note; /* event counters for sound: last_note 0..7 */
    Rng rng;
} post_it_State;

void post_it_init(post_it_State *s, unsigned seed);
void post_it_init_sized(post_it_State *s, unsigned seed, int w, int h);
void post_it_step(post_it_State *s, const ArcadeInput *in, float dt);
bool post_it_is_over(const post_it_State *s);
int post_it_score(const post_it_State *s);

/* Pure helpers (tested on their own). */
/* Pointer / keys -> tilt (ax, ay in -1..1). `in->aim_*` are SCREEN pixels; keys override the pointer. */
void post_it_tilt_from_input(const ArcadeInput *in, int w, int h, float *ax, float *ay);
/* Tilt -> world drift: each axis is clamped to +-0.5 and scaled by 8 (y flipped: pointer above centre = up).
 */
void post_it_drift_from_tilt(float ax, float ay, float *spx, float *spy);
/* Scale note (1..8) for a cluster size: 1,2..8,8,7..2,1,1,2... */
int post_it_scale_index(int blockn);

#endif
