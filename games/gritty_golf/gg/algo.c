/*
 * algo.c  -  sorting and searching
 */
#include <stdlib.h>
#include "algo.h"
#include "util.h"

/* ================================================================== *
 *  Comparators                                         [T10 Pointer]
 *  A comparator returns  <0 if a comes first, 0 if equal, >0 if b first.
 *  Passing the comparator as a FUNCTION POINTER lets ONE sort routine
 *  order the same data in many different ways.
 * ================================================================== */
int Cmp_ByRank(const ScoreRecord *a, const ScoreRecord *b)
{
    if (a->levels  != b->levels)  return b->levels  - a->levels;   /* more levels first   */
    if (a->strokes != b->strokes) return a->strokes - b->strokes;  /* fewer strokes first */
    if (a->time < b->time) return -1;                              /* shorter time first  */
    if (a->time > b->time) return  1;
    return 0;
}

int Cmp_ByName(const ScoreRecord *a, const ScoreRecord *b)
{
    return Str_CompareNoCase(a->name, b->name);
}

int Cmp_ByTime(const ScoreRecord *a, const ScoreRecord *b)
{
    if (a->time < b->time) return -1;
    if (a->time > b->time) return  1;
    return 0;
}

/* ================================================================== *
 *  SORTING
 * ================================================================== */

/*
 * BUBBLE SORT  - repeatedly swap neighbours that are in the wrong order.
 * After pass k the k biggest values have "bubbled" to the end.
 * If a whole pass makes no swap the array is sorted: stop early.
 * Time O(n^2) worst case, O(n) if already sorted.
 *
 *   PSEUDOCODE
 *     REPEAT
 *        swapped <- false
 *        FOR i <- 0 TO n-2
 *           IF a[i] > a[i+1] THEN swap them ; swapped <- true
 *        n <- n - 1                       // last element is final
 *     UNTIL swapped = false
 */
void Sort_BubbleFloat(float *a, int n)
{
    bool swapped;
    do {                                            /* [T06] do-while: runs at least once */
        swapped = false;
        for (int i = 0; i < n - 1; i++) {
            if (a[i] > a[i + 1]) {
                Swap_Float(&a[i], &a[i + 1]);
                swapped = true;
            }
        }
        n--;
    } while (swapped);
}

/*
 * INSERTION SORT - take the next element and slide it left into the already
 * sorted part, like sorting playing cards in your hand.  Very fast for
 * SMALL or NEARLY SORTED arrays, which is exactly what the rasteriser has.
 */
void Sort_InsertionFloat(float *a, int n)
{
    for (int i = 1; i < n; i++) {
        float key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > key) {              /* shift bigger values right */
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

/*
 * SELECTION SORT on INDEXES.
 * We do not move the data at all, we only sort an array of positions
 * (idx[]) by looking up key[idx[i]].  Useful when the real data should stay
 * in place (here: level numbers 0..24 ranked by best strokes).
 *   each round: find the best remaining element, swap it into position i.
 */
void Sort_SelectionByKey(int *idx, const int *key, int n, bool descending)
{
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++) {
            bool better = descending ? key[idx[j]] > key[idx[best]]    /* [T05] ternary */
                                     : key[idx[j]] < key[idx[best]];
            if (better)
                best = j;
        }
        if (best != i)
            Swap_Int(&idx[i], &idx[best]);
    }
}

/* Insertion sort for records; stable (equal items keep their order). */
void Sort_InsertionRecords(ScoreRecord *a, int n, RecordCmp cmp)
{
    for (int i = 1; i < n; i++) {
        ScoreRecord key = a[i];                     /* structs can be copied with = */
        int j = i - 1;
        while (j >= 0 && cmp(&a[j], &key) > 0) {    /* call through the pointer     */
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

/*
 * MERGE SORT  (recursive, "divide and conquer")
 *   1. split the range in two halves
 *   2. sort each half  <-- the SAME function calls itself
 *   3. merge two sorted halves into one sorted range
 * Base case: a range of 0 or 1 elements is already sorted.
 * Always O(n log n), stable, but needs a temporary array.
 *
 *   PSEUDOCODE
 *     PROCEDURE msort(lo, hi)
 *        IF lo >= hi THEN return
 *        mid <- (lo + hi) / 2
 *        msort(lo, mid) ; msort(mid+1, hi)
 *        merge a[lo..mid] and a[mid+1..hi] into tmp, copy back
 */
static void merge_rec(ScoreRecord *a, ScoreRecord *tmp, int lo, int hi, RecordCmp cmp)
{
    if (lo >= hi)
        return;                                     /* base case */

    int mid = lo + (hi - lo) / 2;
    merge_rec(a, tmp, lo, mid, cmp);                /* sort left half  */
    merge_rec(a, tmp, mid + 1, hi, cmp);            /* sort right half */

    int i = lo, j = mid + 1, k = lo;
    while (i <= mid && j <= hi)                     /* take the smaller head */
        tmp[k++] = (cmp(&a[i], &a[j]) <= 0) ? a[i++] : a[j++];
    while (i <= mid) tmp[k++] = a[i++];             /* leftovers             */
    while (j <= hi)  tmp[k++] = a[j++];
    for (k = lo; k <= hi; k++)
        a[k] = tmp[k];
}

bool Sort_MergeRecords(ScoreRecord *a, int n, RecordCmp cmp)
{
    if (n < 2)
        return true;
    ScoreRecord *tmp = malloc((size_t)n * sizeof *tmp);   /* scratch space */
    if (tmp == NULL)
        return false;
    merge_rec(a, tmp, 0, n - 1, cmp);
    free(tmp);
    return true;
}

/*
 * QUICK SORT (recursive)
 *   1. pick a PIVOT, move everything smaller to its left, bigger to its right
 *   2. the pivot is now in its final place
 *   3. recursively sort the left part and the right part
 * Average O(n log n); sorts in place (no temporary array).
 * We pick the MIDDLE element as pivot so already-sorted input is fast.
 */
void Sort_QuickRecords(ScoreRecord *a, int lo, int hi, RecordCmp cmp)
{
    if (lo >= hi)
        return;                                     /* base case */

    int mid = lo + (hi - lo) / 2;
    ScoreRecord tmp = a[mid]; a[mid] = a[hi]; a[hi] = tmp;   /* pivot -> end */
    ScoreRecord pivot = a[hi];

    int store = lo;                                 /* boundary of the "smaller" part */
    for (int i = lo; i < hi; i++) {
        if (cmp(&a[i], &pivot) < 0) {
            tmp = a[i]; a[i] = a[store]; a[store] = tmp;
            store++;
        }
    }
    tmp = a[store]; a[store] = a[hi]; a[hi] = tmp;  /* pivot to its final slot */

    Sort_QuickRecords(a, lo, store - 1, cmp);       /* left of the pivot  */
    Sort_QuickRecords(a, store + 1, hi, cmp);       /* right of the pivot */
}

/* ================================================================== *
 *  SEARCHING
 * ================================================================== */

/* LINEAR SEARCH: look at every element until found.  Works on ANY list. O(n) */
int Search_LinearName(const ScoreRecord *a, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (Str_CompareNoCase(a[i].name, name) == 0)
            return i;
    return -1;                                      /* convention: -1 = not found */
}

/*
 * BINARY SEARCH: the list MUST be sorted.  Look at the middle element and
 * throw away the half that cannot contain the target.  O(log n):
 * 1,000,000 names need only ~20 comparisons.
 */
int Search_BinaryNameIter(const ScoreRecord *a, int n, const char *name)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;               /* (lo+hi)/2 could overflow */
        int c = Str_CompareNoCase(name, a[mid].name);
        if (c == 0)      return mid;
        else if (c < 0)  hi = mid - 1;              /* target is in the left half  */
        else             lo = mid + 1;              /* target is in the right half */
    }
    return -1;
}

/* The same idea written recursively: each call searches a smaller range. */
int Search_BinaryNameRec(const ScoreRecord *a, int lo, int hi, const char *name)
{
    if (lo > hi)
        return -1;                                  /* base case: empty range */
    int mid = lo + (hi - lo) / 2;
    int c = Str_CompareNoCase(name, a[mid].name);
    if (c == 0)
        return mid;                                 /* base case: found */
    if (c < 0)
        return Search_BinaryNameRec(a, lo, mid - 1, name);
    return Search_BinaryNameRec(a, mid + 1, hi, name);
}

/* Binary search on plain ints (ascending array). */
int Search_BinaryInt(const int *a, int n, int target)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == target) return mid;
        if (a[mid] < target)  lo = mid + 1;
        else                  hi = mid - 1;
    }
    return -1;
}
