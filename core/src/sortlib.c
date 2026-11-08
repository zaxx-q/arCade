#include "arcade/sortlib.h"

#include <string.h>

void swap_int(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

void sort_bubble(int *a, size_t n, SortStats *st) {
    for (size_t i = 0; i + 1 < n; i++) {
        bool swapped = false;
        for (size_t j = 0; j + 1 < n - i; j++) {
            if (st)
                st->comparisons++;
            if (a[j] > a[j + 1]) {
                swap_int(&a[j], &a[j + 1]);
                if (st)
                    st->swaps++;
                swapped = true;
            }
        }
        if (!swapped)
            break;
    }
}

void sort_selection(int *a, size_t n, SortStats *st) {
    for (size_t i = 0; i + 1 < n; i++) {
        size_t min = i;
        for (size_t j = i + 1; j < n; j++) {
            if (st)
                st->comparisons++;
            if (a[j] < a[min])
                min = j;
        }
        if (min != i) {
            swap_int(&a[i], &a[min]);
            if (st)
                st->swaps++;
        }
    }
}

void sort_insertion(int *a, size_t n, SortStats *st) {
    for (size_t i = 1; i < n; i++) {
        size_t j = i;
        while (j > 0) {
            if (st)
                st->comparisons++;
            if (a[j - 1] <= a[j])
                break;
            swap_int(&a[j - 1], &a[j]);
            if (st)
                st->swaps++;
            j--;
        }
    }
}

bool sort_is_sorted(const int *a, size_t n) {
    for (size_t i = 0; i + 1 < n; i++)
        if (a[i] > a[i + 1])
            return false;
    return true;
}

/* Sort a copy with selection sort-like cycle counting: build the target permutation, count cycles. */
int sort_min_swaps(const int *a, size_t n) {
    if (n > 64)
        n = 64;
    int sorted[64], target[64];
    bool seen[64];
    memcpy(sorted, a, n * sizeof(int));
    sort_insertion(sorted, n, NULL);
    /* map every element to its final index (equal values take the lowest unused slot) */
    bool used[64];
    memset(used, 0, sizeof(used));
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (!used[j] && sorted[j] == a[i]) {
                used[j] = true;
                target[i] = (int)j;
                break;
            }
        }
    }
    memset(seen, 0, sizeof(seen));
    int swaps = 0;
    for (size_t i = 0; i < n; i++) {
        if (seen[i] || target[i] == (int)i)
            continue;
        int len = 0;
        for (int j = (int)i; !seen[j]; j = target[j]) {
            seen[j] = true;
            len++;
        }
        swaps += len - 1;
    }
    return swaps;
}
