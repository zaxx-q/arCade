#include <string.h>

#include "arcade/sortlib.h"
#include "sort_it_logic.h"

static const int k_round_sizes[SI_ROUNDS] = {6, 8, 10};

static void start_round(sort_it_State *s) {
    s->n = k_round_sizes[s->round];
    do { /* Fisher-Yates shuffle, repeated until the bars are NOT already sorted */
        for (int i = 0; i < s->n; i++)
            s->values[i] = i + 1;
        for (int i = s->n - 1; i > 0; i--)
            swap_int(&s->values[i], &s->values[rng_range(&s->rng, 0, i)]);
    } while (sort_is_sorted(s->values, (size_t)s->n));

    s->cursor = 0;
    s->mark = -1;
    s->swaps = 0;
    s->par = sort_min_swaps(s->values, (size_t)s->n);
    s->budget = s->par + 2 + s->n / 2;

    int copy[SI_MAX_N];
    SortStats st;
    memcpy(copy, s->values, sizeof(copy));
    st.comparisons = st.swaps = 0;
    sort_bubble(copy, (size_t)s->n, &st);
    s->bubble_swaps = (int)st.swaps;
    memcpy(copy, s->values, sizeof(copy));
    st.comparisons = st.swaps = 0;
    sort_selection(copy, (size_t)s->n, &st);
    s->selection_swaps = (int)st.swaps;
}

void sort_it_init(sort_it_State *s, unsigned seed) {
    memset(s, 0, sizeof(*s));
    rng_seed(&s->rng, seed);
    start_round(s);
}

int sort_it_index_at(const sort_it_State *s, float x) {
    float slot = SI_AREA_W / (float)s->n;
    int i = (int)((x - SI_X0) / slot);
    if (x < SI_X0 || i < 0 || i >= s->n)
        return -1;
    return i;
}

/* 300 points for a perfect round, 40 less per swap above par, never below 100. */
static int round_points(const sort_it_State *s) {
    return arc_maxi(100, 300 - 40 * (s->swaps - s->par));
}

/* TODO(Leader): record the algorithms' swaps (core/sortlib trace) and replay them as an animated opponent
 * (GAME.md step 1). */
static void act_on_cursor(sort_it_State *s) {
    if (s->mark < 0) { /* first pick */
        s->mark = s->cursor;
        return;
    }
    if (s->mark == s->cursor) { /* picked the same bar again: cancel */
        s->mark = -1;
        return;
    }
    swap_int(&s->values[s->mark], &s->values[s->cursor]);
    s->swaps++;
    s->mark = -1;
    if (sort_is_sorted(s->values, (size_t)s->n)) {
        s->last_round_score = round_points(s);
        s->score += s->last_round_score;
        s->round++;
        if (s->round >= SI_ROUNDS)
            s->over = true;
        else
            start_round(s);
    } else if (s->swaps >= s->budget) {
        s->over = true;
        s->lost = true;
    }
}

void sort_it_step(sort_it_State *s, const ArcadeInput *in, float dt) {
    (void)dt; /* turn-based: time does not matter */
    if (s->over)
        return;
    if (in->left_pressed)
        s->cursor = arc_maxi(0, s->cursor - 1);
    if (in->right_pressed)
        s->cursor = arc_mini(s->n - 1, s->cursor + 1);
    bool act = in->action_pressed;
    if (in->fire_pressed) {
        int i = sort_it_index_at(s, in->aim_x);
        if (i >= 0) {
            s->cursor = i;
            act = true;
        }
    }
    if (act)
        act_on_cursor(s);
}

bool sort_it_is_over(const sort_it_State *s) {
    return s->over;
}

int sort_it_score(const sort_it_State *s) {
    return s->score;
}
