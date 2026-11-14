/* A Different Color - PURE game logic (no raylib, no drawing, no keyboard). Owner: Leader
 * Two boards of randomly coloured cells. The bottom board is a copy of the top one except for ONE
 * cell with a slightly different colour. Click it on either board: +1 and a bigger grid. A click on
 * any other cell of the field ends the game. The first board is untimed, later boards have 30 s.
 * Coordinates are playfield-local; colours are plain float triplets 0..1 (the front-end converts). */
#ifndef DIFFERENT_COLOR_LOGIC_H
#define DIFFERENT_COLOR_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define DC_MAX_CELLS 400    /* array cap: score 399 (a 20 x 20 board solved) ends the game */
#define DC_X1 16.0f         /* left margin of the boards                                  */
#define DC_Y1 80.0f         /* top of the top board                                       */
#define DC_TIME_LIMIT 30.0f /* seconds per board (from the second board on)               */
#define DC_WARN_TIME 9.999f /* tick-tack starts when this many seconds are left           */
#define DC_OVER_DELAY 1.2f  /* seconds the rings on the odd cells stay before is_over()   */

typedef struct different_color_State {
    int w, h;
    float y2;                     /* top of the bottom board = h / 2 + 8              */
    int n, cols, rows;            /* real cells, grid columns and rows                */
    float rectw, recth;           /* size of one cell                                 */
    int dif;                      /* index of the odd cell, row-major                 */
    float cell[DC_MAX_CELLS][3];  /* colour of each cell (top board, and bottom board except `dif`) */
    float odd[3];                 /* the odd cell's colour on the bottom board        */
    bool timed;                   /* false on the first board                         */
    float time_left;              /* seconds left on the current board                */
    float tick_t;                 /* seconds since the last tick-tack                 */
    int score;                    /* boards solved                                    */
    bool over, capped, timed_out; /* why the game ended                               */
    float over_t;
    int solved, ticks, misses; /* event counters for sound                         */
    Rng rng;
} different_color_State;

void different_color_init(different_color_State *s, unsigned seed);
void different_color_init_sized(different_color_State *s, unsigned seed, int w, int h);
void different_color_step(different_color_State *s, const ArcadeInput *in, float dt);
bool different_color_is_over(const different_color_State *s);
int different_color_score(const different_color_State *s);

/* A click at a playfield-LOCAL position (step calls this with the converted pointer). */
void different_color_click(different_color_State *s, float lx, float ly);
/* (Re)builds the board for the current score; init and every correct click call it. */
void different_color_new_board(different_color_State *s);
/* Centre of the odd cell on the TOP board; the bottom one is `y + (y2 - DC_Y1)`. */
Vec2 different_color_odd_center(const different_color_State *s);

/* Pure rules, exported for the tests. */
void different_color_grid_for_score(int score, int *cols, int *rows);
bool different_color_random_color_ok(float r, float g, float b);    /* not too grey, not too dark */
bool different_color_diff_ok(const float c1[3], const float c2[3]); /* different enough to be fair */

#endif
