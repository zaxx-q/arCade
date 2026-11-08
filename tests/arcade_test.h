/* Minimal unit-test helper (no external dependency).
 *
 *   #include "arcade_test.h"
 *   static void test_something(void) { CHECK(1 + 1 == 2); CHECK_EQ_INT(sum(2, 3), 5); }
 *   int main(void) { RUN(test_something); return TEST_SUMMARY(); }
 *
 * CHECK* macros `return` from the test function on failure, so use them only inside
 * `static void test_xxx(void)` functions (not inside helpers that return a value).
 */
#ifndef ARCADE_TEST_H
#define ARCADE_TEST_H

#include <math.h>
#include <stdio.h>
#include <string.h>

static int arc_t_failed = 0;
static int arc_t_run = 0;
static int arc_t_cur_failed = 0;

#define CHECK(cond)                                                                                          \
    do {                                                                                                     \
        if (!(cond)) {                                                                                       \
            printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                       \
            arc_t_cur_failed = 1;                                                                            \
            return;                                                                                          \
        }                                                                                                    \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                                                   \
    do {                                                                                                     \
        long long arc_va = (long long)(a), arc_vb = (long long)(b);                                          \
        if (arc_va != arc_vb) {                                                                              \
            printf("    FAIL %s:%d: %s == %s  (got %lld, expected %lld)\n", __FILE__, __LINE__, #a, #b,      \
                   arc_va, arc_vb);                                                                          \
            arc_t_cur_failed = 1;                                                                            \
            return;                                                                                          \
        }                                                                                                    \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                                \
    do {                                                                                                     \
        double arc_va = (double)(a), arc_vb = (double)(b);                                                   \
        if (fabs(arc_va - arc_vb) > (double)(eps)) {                                                         \
            printf("    FAIL %s:%d: %s ~= %s  (got %f, expected %f)\n", __FILE__, __LINE__, #a, #b, arc_va,  \
                   arc_vb);                                                                                  \
            arc_t_cur_failed = 1;                                                                            \
            return;                                                                                          \
        }                                                                                                    \
    } while (0)

#define CHECK_STR_EQ(a, b)                                                                                   \
    do {                                                                                                     \
        if (strcmp((a), (b)) != 0) {                                                                         \
            printf("    FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b));                      \
            arc_t_cur_failed = 1;                                                                            \
            return;                                                                                          \
        }                                                                                                    \
    } while (0)

#define RUN(fn)                                                                                              \
    do {                                                                                                     \
        arc_t_cur_failed = 0;                                                                                \
        arc_t_run++;                                                                                         \
        fn();                                                                                                \
        if (arc_t_cur_failed) {                                                                              \
            arc_t_failed++;                                                                                  \
            printf("[FAIL] %s\n", #fn);                                                                      \
        } else {                                                                                             \
            printf("[ ok ] %s\n", #fn);                                                                      \
        }                                                                                                    \
    } while (0)

#define TEST_SUMMARY() (printf("%d tests, %d failed\n", arc_t_run, arc_t_failed), arc_t_failed ? 1 : 0)

#endif
