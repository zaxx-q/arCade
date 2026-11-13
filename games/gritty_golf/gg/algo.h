/*
 * algo.h  -  SORTING and SEARCHING algorithms, written by hand
 *
 *   algorithm        where it is used in the program
 *   ---------------  ------------------------------------------------------
 *   bubble sort      console "Shot statistics" (sort the numbers you typed)
 *   selection sort   "hardest levels" ranking (sorts an array of INDEXES)
 *   insertion sort   raster.c: x-crossings of a scan line; leaderboard rank
 *   merge sort  (R)  leaderboard sorted by name
 *   quick sort  (R)  leaderboard sorted by time
 *   linear search    find a player / first uncleared level
 *   binary search    find a player in the name-sorted list (iterative + (R))
 *   (R) = recursive
 */
#ifndef GG_ALGO_H
#define GG_ALGO_H

#include <stdbool.h>
#include "records.h"

/* [T10] Function pointer type: "a function that compares two records" */
typedef int (*RecordCmp)(const ScoreRecord *a, const ScoreRecord *b);

int Cmp_ByRank(const ScoreRecord *a, const ScoreRecord *b); /* levels desc, strokes asc, time asc */
int Cmp_ByName(const ScoreRecord *a, const ScoreRecord *b); /* A..Z, ignoring case                */
int Cmp_ByTime(const ScoreRecord *a, const ScoreRecord *b); /* fastest first                      */

/* ---- sorting ------------------------------------------------------ */
void Sort_BubbleFloat(float *a, int n);
void Sort_InsertionFloat(float *a, int n);
void Sort_SelectionByKey(int *idx, const int *key, int n, bool descending);
void Sort_InsertionRecords(ScoreRecord *a, int n, RecordCmp cmp);
bool Sort_MergeRecords(ScoreRecord *a, int n, RecordCmp cmp);          /* false = out of memory */
void Sort_QuickRecords(ScoreRecord *a, int lo, int hi, RecordCmp cmp); /* sorts a[lo..hi]       */

/* ---- searching ---------------------------------------------------- */
int Search_LinearName(const ScoreRecord *a, int n, const char *name);  /* index or -1 */
int Search_BinaryNameIter(const ScoreRecord *a, int n, const char *name);
int Search_BinaryNameRec(const ScoreRecord *a, int lo, int hi, const char *name);
int Search_BinaryInt(const int *a, int n, int target);                 /* a ascending */

#endif /* GG_ALGO_H */
