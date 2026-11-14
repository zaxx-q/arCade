/* BW Reflex - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * A black or white block falls. Click to flip the colour of the base strip. When the block lands,
 * the base must have the same colour: +1 point and the next block falls faster. Otherwise game over.
 * All coordinates are playfield-local (origin = top-left of the playfield). */
#ifndef BW_REFLEX_LOGIC_H
#define BW_REFLEX_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define BW_BLOCK 80.0f     /* the falling block is 80 x 80 px                            */
#define BW_OVER_DELAY 0.9f /* seconds the front-end may show the mismatch before is_over() */

typedef struct bw_reflex_State {
    int w, h;        /* playfield size (pixels)                                   */
    int base_h;      /* height of the base strip = round(h / 6)                   */
    int base_white;  /* 1 = the base is white, 0 = black                          */
    int block_white; /* 1 = the falling block is white, 0 = black                 */
    float block_y;   /* centre y of the block (starts above the screen at -40)    */
    float speed;     /* fall speed factor: 4.0 at the start, grows on every match */
    float mult;      /* growth rate of speed, shrinks by x0.9625 on every match   */
    int score;       /* blocks matched                                            */
    bool over;       /* the rules say game over (colours differed)                */
    float over_t;    /* seconds since `over` became true                          */
    int flips;       /* event counter: base flips (front-end plays a click)       */
    int matches;     /* event counter: blocks matched (front-end plays a note)    */
    int last_note;   /* scale note 0..7 of the latest match = old score % 8       */
    Rng rng;
} bw_reflex_State;

void bw_reflex_init(bw_reflex_State *s, unsigned seed);
void bw_reflex_init_sized(bw_reflex_State *s, unsigned seed, int w, int h);
void bw_reflex_step(bw_reflex_State *s, const ArcadeInput *in, float dt);
bool bw_reflex_is_over(const bw_reflex_State *s);
int bw_reflex_score(const bw_reflex_State *s);

/* Y of the line the block's bottom edge must cross to "land". */
float bw_reflex_contact_y(const bw_reflex_State *s);

#endif
