/* test_algo.c - every sorting and searching algorithm against a reference */
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "algo.h"
#include "util.h"

#define MAXN 200

static ScoreRecord make(const char *name, int levels, int strokes, float time)
{
    ScoreRecord r;
    memset(&r, 0, sizeof r);
    Str_Copy(r.name, sizeof r.name, name);
    r.levels = levels;
    r.strokes = strokes;
    r.time = time;
    Str_Copy(r.date, sizeof r.date, "2026-01-01");
    return r;
}

static ScoreRecord random_record(void)
{
    char name[12];
    int len = 1 + (int)(TestRand() % 8);
    for (int i = 0; i < len; i++) {
        char c = (char)('a' + TestRand() % 26);
        name[i] = (TestRand() % 3 == 0) ? (char)(c - 32) : c;      /* mixed case */
    }
    name[len] = '\0';
    return make(name, (int)(TestRand() % 26), (int)(TestRand() % 300), (float)(TestRand() % 5000) / 10.0f);
}

static int is_sorted(const ScoreRecord *a, int n, RecordCmp cmp)
{
    for (int i = 1; i < n; i++)
        if (cmp(&a[i - 1], &a[i]) > 0)
            return 0;
    return 1;
}

/* wrappers so the C library qsort() can act as the reference implementation */
static int ref_rank(const void *a, const void *b) { return Cmp_ByRank(a, b); }
static int ref_name(const void *a, const void *b) { return Cmp_ByName(a, b); }
static int ref_time(const void *a, const void *b) { return Cmp_ByTime(a, b); }

/* two arrays are "equivalent" when they have the same key at every position */
static int same_order(const ScoreRecord *a, const ScoreRecord *b, int n, RecordCmp cmp)
{
    for (int i = 0; i < n; i++)
        if (cmp(&a[i], &b[i]) != 0)
            return 0;
    return 1;
}

/* ---- comparators ---------------------------------------------------- */
static void test_comparators(void)
{
    ScoreRecord a = make("A", 10, 50, 100.0f), b = make("B", 12, 90, 300.0f);
    CHECK(Cmp_ByRank(&b, &a) < 0);                    /* more levels ranks first */
    ScoreRecord c = make("C", 10, 40, 500.0f);
    CHECK(Cmp_ByRank(&c, &a) < 0);                    /* same levels: fewer strokes first */
    ScoreRecord d = make("D", 10, 50, 90.0f);
    CHECK(Cmp_ByRank(&d, &a) < 0);                    /* same strokes: faster first */
    CHECK_INT(Cmp_ByRank(&a, &a), 0);
    CHECK(Cmp_ByTime(&d, &a) < 0);
    CHECK(Cmp_ByTime(&a, &d) > 0);
    CHECK_INT(Cmp_ByTime(&a, &a), 0);
    ScoreRecord lo = make("alice", 0, 0, 0), up = make("ALICE", 0, 0, 0), z = make("Zoe", 0, 0, 0);
    CHECK_INT(Cmp_ByName(&lo, &up), 0);               /* case-insensitive */
    CHECK(Cmp_ByName(&lo, &z) < 0);
}

/* ---- float sorts ------------------------------------------------------ */
static int cmp_float_ref(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

static void check_float_sort(void (*sort)(float *, int))
{
    static const int sizes[] = { 0, 1, 2, 3, 5, 10, 64, 150 };
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        int n = sizes[s];
        float a[MAXN], ref[MAXN];
        for (int i = 0; i < n; i++)
            a[i] = ref[i] = (float)((int)(TestRand() % 2001) - 1000) / 10.0f;   /* with duplicates */
        sort(a, n);
        /* cppcheck-suppress uninitvar ; false positive: ref[0..n-1] was filled by the loop above */
        qsort(ref, (size_t)n, sizeof ref[0], cmp_float_ref);
        CHECK(n == 0 || memcmp(a, ref, (size_t)n * sizeof a[0]) == 0);
    }
    float sorted[6] = { 1, 2, 3, 4, 5, 6 }, rev[6] = { 6, 5, 4, 3, 2, 1 }, same[4] = { 7, 7, 7, 7 };
    sort(sorted, 6); sort(rev, 6); sort(same, 4);
    CHECK_NEAR(sorted[0], 1, 0); CHECK_NEAR(sorted[5], 6, 0);
    CHECK_NEAR(rev[0], 1, 0);    CHECK_NEAR(rev[5], 6, 0);
    CHECK_NEAR(same[0], 7, 0);   CHECK_NEAR(same[3], 7, 0);
}

static void test_bubble_float(void)    { TestSeed(1); check_float_sort(Sort_BubbleFloat); }
static void test_insertion_float(void) { TestSeed(2); check_float_sort(Sort_InsertionFloat); }

/* ---- selection sort on indexes ---------------------------------------- */
static void test_selection_by_key(void)
{
    int key[10] = { 5, 9, 1, 9, 3, 7, 0, 2, 8, 4 };
    int idx[10];
    for (int i = 0; i < 10; i++) idx[i] = i;
    Sort_SelectionByKey(idx, key, 10, true);                  /* descending */
    for (int i = 1; i < 10; i++)
        CHECK(key[idx[i - 1]] >= key[idx[i]]);
    CHECK_INT(key[idx[0]], 9);
    CHECK_INT(key[idx[9]], 0);
    int seen[10] = { 0 };                                     /* idx is still a permutation */
    for (int i = 0; i < 10; i++) seen[idx[i]]++;
    for (int i = 0; i < 10; i++) CHECK_INT(seen[i], 1);
    CHECK_INT(key[3], 9);                                     /* the DATA was not moved */

    for (int i = 0; i < 10; i++) idx[i] = i;
    Sort_SelectionByKey(idx, key, 10, false);                 /* ascending */
    for (int i = 1; i < 10; i++)
        CHECK(key[idx[i - 1]] <= key[idx[i]]);

    int idx2[3] = { 2, 0, 1 };                                /* only the first m entries are sorted */
    int key2[3] = { 30, 10, 20 };
    Sort_SelectionByKey(idx2, key2, 2, false);                /* sorts idx2[0..1] = keys 20, 30 */
    CHECK_INT(idx2[0], 2);
    CHECK_INT(idx2[1], 0);
    CHECK_INT(idx2[2], 1);

    Sort_SelectionByKey(idx2, key2, 0, true);                 /* n = 0 must not crash */
    Sort_SelectionByKey(idx2, key2, 1, true);
}

/* ---- record sorts ----------------------------------------------------- */
typedef struct { const char *name; RecordCmp cmp; int (*ref)(const void *, const void *); } Order;

static void run_record_sort(void (*sort)(ScoreRecord *, int, RecordCmp))
{
    static const Order orders[] = {
        { "rank", Cmp_ByRank, ref_rank }, { "name", Cmp_ByName, ref_name }, { "time", Cmp_ByTime, ref_time }
    };
    static const int sizes[] = { 0, 1, 2, 3, 7, 16, 50, 199 };

    for (size_t o = 0; o < 3; o++) {
        for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
            int n = sizes[s];
            ScoreRecord a[MAXN], ref[MAXN];
            for (int i = 0; i < n; i++) a[i] = ref[i] = random_record();
            sort(a, n, orders[o].cmp);
            /* cppcheck-suppress uninitvar ; false positive: ref[0..n-1] was filled by the loop above */
            qsort(ref, (size_t)n, sizeof ref[0], orders[o].ref);
            CHECK(is_sorted(a, n, orders[o].cmp));
            CHECK(same_order(a, ref, n, orders[o].cmp));
        }
    }
    /* already sorted / reversed / all equal */
    ScoreRecord a[50];
    for (int i = 0; i < 50; i++) a[i] = make("x", 1, i, (float)i);
    sort(a, 50, Cmp_ByTime);                    CHECK(is_sorted(a, 50, Cmp_ByTime));
    for (int i = 0; i < 50; i++) a[i] = make("x", 1, i, (float)(50 - i));
    sort(a, 50, Cmp_ByTime);                    CHECK(is_sorted(a, 50, Cmp_ByTime));
    for (int i = 0; i < 50; i++) a[i] = make("x", 1, 5, 5.0f);
    sort(a, 50, Cmp_ByTime);                    CHECK(is_sorted(a, 50, Cmp_ByTime));
}

static void ins(ScoreRecord *a, int n, RecordCmp c) { Sort_InsertionRecords(a, n, c); }
static void mer(ScoreRecord *a, int n, RecordCmp c) { CHECK(Sort_MergeRecords(a, n, c)); }
static void qui(ScoreRecord *a, int n, RecordCmp c) { Sort_QuickRecords(a, 0, n - 1, c); }

static void test_insertion_records(void) { TestSeed(3); run_record_sort(ins); }
static void test_merge_records(void)     { TestSeed(4); run_record_sort(mer); }
static void test_quick_records(void)     { TestSeed(5); run_record_sort(qui); }

/* insertion and merge sort are STABLE: equal keys keep their original order */
static void check_stable(void (*sort)(ScoreRecord *, int, RecordCmp))
{
    ScoreRecord a[60];
    for (int i = 0; i < 60; i++)
        a[i] = make("n", 1, i /* remember original position */, (float)(i % 4));
    sort(a, 60, Cmp_ByTime);
    for (int i = 1; i < 60; i++)
        if (a[i - 1].time == a[i].time)
            CHECK(a[i - 1].strokes < a[i].strokes);
}
static void test_stability(void) { check_stable(ins); check_stable(mer); }

static void test_quick_sub_range(void)
{
    ScoreRecord a[6];
    float t[6] = { 9, 5, 4, 3, 2, 0 };
    for (int i = 0; i < 6; i++) a[i] = make("q", 1, i, t[i]);
    Sort_QuickRecords(a, 1, 4, Cmp_ByTime);                   /* sort positions 1..4 only */
    CHECK_NEAR(a[0].time, 9, 0);
    CHECK_NEAR(a[5].time, 0, 0);
    for (int i = 2; i <= 4; i++) CHECK(a[i - 1].time <= a[i].time);
    Sort_QuickRecords(a, 3, 2, Cmp_ByTime);                   /* lo > hi: no-op */
}

/* ---- searching -------------------------------------------------------- */
static void test_linear_search(void)
{
    ScoreRecord a[4] = { make("Ann", 1, 1, 1), make("bob", 1, 1, 1), make("Cy", 1, 1, 1), make("Di", 1, 1, 1) };
    CHECK_INT(Search_LinearName(a, 4, "Ann"), 0);
    CHECK_INT(Search_LinearName(a, 4, "BOB"), 1);             /* case-insensitive */
    CHECK_INT(Search_LinearName(a, 4, "di"), 3);
    CHECK_INT(Search_LinearName(a, 4, "Eve"), -1);
    CHECK_INT(Search_LinearName(a, 0, "Ann"), -1);
    CHECK_INT(Search_LinearName(a, 4, ""), -1);
}

static void test_binary_search_names(void)
{
    TestSeed(6);
    for (int round = 0; round < 20; round++) {
        int n = 1 + (int)(TestRand() % 120);
        ScoreRecord a[MAXN];
        for (int i = 0; i < n; i++) {
            char name[24];
            snprintf(name, sizeof name, "P%03u_%d", TestRand() % 1000, i);   /* unique */
            a[i] = make(name, 1, 1, 1);
        }
        Sort_MergeRecords(a, n, Cmp_ByName);
        for (int i = 0; i < n; i++) {                                         /* every element is found */
            CHECK_INT(Search_BinaryNameIter(a, n, a[i].name), i);
            CHECK_INT(Search_BinaryNameRec(a, 0, n - 1, a[i].name), i);
            char upper[24];
            Str_Copy(upper, sizeof upper, a[i].name);
            Str_ToUpper(upper);                                               /* case-insensitive */
            CHECK_INT(Search_BinaryNameIter(a, n, upper), i);
        }
        CHECK_INT(Search_BinaryNameIter(a, n, "ZZZ_missing"), -1);
        CHECK_INT(Search_BinaryNameRec(a, 0, n - 1, "ZZZ_missing"), -1);
        CHECK_INT(Search_BinaryNameIter(a, n, "A_before_all"), -1);
    }
    ScoreRecord one[1] = { make("solo", 1, 1, 1) };
    CHECK_INT(Search_BinaryNameIter(one, 1, "solo"), 0);
    CHECK_INT(Search_BinaryNameRec(one, 0, 0, "solo"), 0);
    CHECK_INT(Search_BinaryNameIter(one, 0, "solo"), -1);
    CHECK_INT(Search_BinaryNameRec(one, 0, -1, "solo"), -1);                  /* empty range */
}

static void test_binary_search_int(void)
{
    int a[] = { 1, 3, 5, 7, 9, 11, 13 };
    for (int i = 0; i < 7; i++)
        CHECK_INT(Search_BinaryInt(a, 7, a[i]), i);
    CHECK_INT(Search_BinaryInt(a, 7, 0), -1);
    CHECK_INT(Search_BinaryInt(a, 7, 14), -1);
    CHECK_INT(Search_BinaryInt(a, 7, 6), -1);
    CHECK_INT(Search_BinaryInt(a, 0, 1), -1);
    int one[1] = { 42 };
    CHECK_INT(Search_BinaryInt(one, 1, 42), 0);
    CHECK_INT(Search_BinaryInt(one, 1, 41), -1);
    int big[1000];                                                            /* large array */
    for (int i = 0; i < 1000; i++) big[i] = i * 2;
    CHECK_INT(Search_BinaryInt(big, 1000, 1998), 999);
    CHECK_INT(Search_BinaryInt(big, 1000, 999), -1);
}

void run_algo_tests(void)
{
    RUN(test_comparators);
    RUN(test_bubble_float);
    RUN(test_insertion_float);
    RUN(test_selection_by_key);
    RUN(test_insertion_records);
    RUN(test_merge_records);
    RUN(test_quick_records);
    RUN(test_stability);
    RUN(test_quick_sub_range);
    RUN(test_linear_search);
    RUN(test_binary_search_names);
    RUN(test_binary_search_int);
}
