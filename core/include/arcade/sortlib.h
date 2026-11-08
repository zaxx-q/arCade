/* Instrumented sorting algorithms (count comparisons and swaps).
 * Used by the leaderboard-style demos and by the "Sort It" game to compare you against the algorithms. */
#ifndef ARCADE_SORTLIB_H
#define ARCADE_SORTLIB_H

#include "arcade/common.h"

typedef struct SortStats {
    unsigned long comparisons;
    unsigned long swaps;
} SortStats;

void swap_int(int *a, int *b); /* classic pointer swap */
void sort_bubble(int *a, size_t n, SortStats *st);
void sort_selection(int *a, size_t n, SortStats *st);
void sort_insertion(int *a, size_t n, SortStats *st);
bool sort_is_sorted(const int *a, size_t n); /* ascending */
/* Minimum number of arbitrary swaps needed to sort a, via cycle counting (n <= 64).
 * Exact when all values are distinct (a permutation). With repeated values it is an upper bound. */
int sort_min_swaps(const int *a, size_t n);

#endif
