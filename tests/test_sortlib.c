#include <string.h>

#include "arcade/rng.h"
#include "arcade/sortlib.h"
#include "arcade_test.h"

static void test_all_sorts_sort(void) {
    Rng r;
    rng_seed(&r, 99);
    for (int round = 0; round < 50; round++) {
        int base[16] = {0}, a[16], b[16], c[16];
        size_t n = (size_t)rng_range(&r, 0, 16);
        for (size_t i = 0; i < n; i++)
            base[i] = rng_range(&r, -20, 20);
        memcpy(a, base, sizeof(base));
        memcpy(b, base, sizeof(base));
        memcpy(c, base, sizeof(base));
        sort_bubble(a, n, NULL);
        sort_selection(b, n, NULL);
        sort_insertion(c, n, NULL);
        CHECK(sort_is_sorted(a, n));
        CHECK(sort_is_sorted(b, n));
        CHECK(sort_is_sorted(c, n));
        CHECK(memcmp(a, b, n * sizeof(int)) == 0);
        CHECK(memcmp(a, c, n * sizeof(int)) == 0);
    }
}

static void test_stats(void) {
    int a[5] = {5, 4, 3, 2, 1};
    SortStats st = {0, 0};
    sort_bubble(a, 5, &st);
    CHECK_EQ_INT(st.swaps, 10); /* reversed list: n(n-1)/2 swaps */
    int b[5] = {1, 2, 3, 4, 5};
    SortStats s2 = {0, 0};
    sort_bubble(b, 5, &s2);
    CHECK_EQ_INT(s2.swaps, 0);
    CHECK_EQ_INT(s2.comparisons, 4); /* early exit after one clean pass */
    int c[5] = {5, 4, 3, 2, 1};
    SortStats s3 = {0, 0};
    sort_selection(c, 5, &s3);
    CHECK(s3.swaps <= 4);
}

static void test_swap_and_sorted(void) {
    int x = 1, y = 2;
    swap_int(&x, &y);
    CHECK_EQ_INT(x, 2);
    CHECK_EQ_INT(y, 1);
    int e[1] = {7};
    CHECK(sort_is_sorted(e, 1));
    CHECK(sort_is_sorted(e, 0));
}

static void test_min_swaps(void) {
    int sorted[4] = {1, 2, 3, 4};
    CHECK_EQ_INT(sort_min_swaps(sorted, 4), 0);
    int one[4] = {2, 1, 3, 4};
    CHECK_EQ_INT(sort_min_swaps(one, 4), 1);
    int cycle[4] = {2, 3, 1, 4}; /* a 3-cycle needs 2 swaps */
    CHECK_EQ_INT(sort_min_swaps(cycle, 4), 2);
    int rev[5] = {5, 4, 3, 2, 1};
    CHECK_EQ_INT(sort_min_swaps(rev, 5), 2);
    int dup[4] = {2, 1, 2, 1}; /* repeated values: only an upper bound (the true minimum is 2) */
    CHECK(sort_min_swaps(dup, 4) <= 3);
    /* for permutations the minimum never exceeds what selection sort uses */
    Rng r;
    rng_seed(&r, 5);
    for (int k = 0; k < 100; k++) {
        int a[8], b[8];
        for (int i = 0; i < 8; i++)
            a[i] = i;
        for (int i = 7; i > 0; i--)
            swap_int(&a[i], &a[rng_range(&r, 0, i)]); /* Fisher-Yates shuffle */
        memcpy(b, a, sizeof(a));
        SortStats st = {0, 0};
        sort_selection(b, 8, &st);
        CHECK(sort_min_swaps(a, 8) <= (int)st.swaps);
    }
}

int main(void) {
    RUN(test_all_sorts_sort);
    RUN(test_stats);
    RUN(test_swap_and_sorted);
    RUN(test_min_swaps);
    return TEST_SUMMARY();
}
