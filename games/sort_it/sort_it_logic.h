/* Sort It - PURE game logic (no raylib). Owner: Leader
 * Hook: you are the sorting algorithm. Sort the bars with as few swaps as possible and beat
 * bubble sort and selection sort (their swap counts are computed with core/sortlib). */
#ifndef SORT_IT_LOGIC_H
#define SORT_IT_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/rng.h"

#define SI_MAX_N 12
#define SI_ROUNDS 3
#define SI_X0 140.0f /* left edge of the bar area (pixels) */
#define SI_AREA_W 1000.0f

typedef struct sort_it_State {
    int values[SI_MAX_N]; /* a permutation of 1..n */
    int n;
    int cursor;                        /* highlighted bar */
    int mark;                          /* first bar picked for a swap, -1 = none */
    int round;                         /* 0-based */
    int swaps;                         /* swaps used this round */
    int par;                           /* fewest swaps that can possibly sort this round (sort_min_swaps) */
    int budget;                        /* swap limit: running out ends the game */
    int bubble_swaps, selection_swaps; /* what the algorithms would need for this round */
    int last_round_score;              /* points earned by the most recently finished round (for the HUD) */
    int score;
    bool over;
    bool lost; /* true when the swap budget ran out */
    Rng rng;
} sort_it_State;

void sort_it_init(sort_it_State *s, unsigned seed);
void sort_it_step(sort_it_State *s, const ArcadeInput *in, float dt);
bool sort_it_is_over(const sort_it_State *s);
int sort_it_score(const sort_it_State *s);
/* Which bar is under pixel x? -1 when outside the bar area. */
int sort_it_index_at(const sort_it_State *s, float x);

#endif
