/*
 * test_framework.h - a tiny unit-test harness (no external dependencies)
 *
 *   CHECK(cond)                 passes when cond is true
 *   CHECK_INT(actual, expected) compares integers and prints both on failure
 *   CHECK_NEAR(a, b, eps)       floating point comparison with a tolerance
 *   CHECK_STR(actual, expected) compares C strings
 *   RUN(test_function)          runs one test and reports its failures
 *
 * A failed CHECK does not stop the program: every problem is listed.
 */
#ifndef GG_TEST_FRAMEWORK_H
#define GG_TEST_FRAMEWORK_H

#include <math.h>
#include <stdio.h>
#include <string.h>

extern int g_checks;          /* total CHECKs executed   */
extern int g_failures;        /* total CHECKs that failed */
extern int g_tests;           /* total tests run          */
extern int g_tests_failed;    /* tests with >= 1 failure  */

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                    \
    } while (0)

#define CHECK_INT(actual, expected)                                          \
    do {                                                                     \
        long long a_ = (long long)(actual), e_ = (long long)(expected);      \
        g_checks++;                                                          \
        if (a_ != e_) {                                                      \
            g_failures++;                                                    \
            printf("    FAIL %s:%d  %s  -> got %lld, expected %lld\n",       \
                   __FILE__, __LINE__, #actual, a_, e_);                     \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(actual, expected, eps)                                    \
    do {                                                                     \
        double a_ = (double)(actual), e_ = (double)(expected);               \
        g_checks++;                                                          \
        if (!(fabs(a_ - e_) <= (double)(eps))) {                             \
            g_failures++;                                                    \
            printf("    FAIL %s:%d  %s  -> got %g, expected %g (+-%g)\n",     \
                   __FILE__, __LINE__, #actual, a_, e_, (double)(eps));      \
        }                                                                    \
    } while (0)

#define CHECK_STR(actual, expected)                                          \
    do {                                                                     \
        const char *a_ = (actual), *e_ = (expected);                         \
        g_checks++;                                                          \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                             \
            g_failures++;                                                    \
            printf("    FAIL %s:%d  %s  -> got \"%s\", expected \"%s\"\n",    \
                   __FILE__, __LINE__, #actual, a_ ? a_ : "(null)", e_);     \
        }                                                                    \
    } while (0)

#define RUN(fn)                                                              \
    do {                                                                     \
        int before_ = g_failures;                                            \
        g_tests++;                                                           \
        fn();                                                                \
        if (g_failures != before_) {                                         \
            g_tests_failed++;                                                \
            printf("  [FAILED] %s\n", #fn);                                  \
        } else {                                                             \
            printf("  [ ok ]   %s\n", #fn);                                  \
        }                                                                    \
    } while (0)

/* ---- helpers shared by several test files ---------------------------- */

/* A scratch file name in the current directory, e.g. "gg_test_save.tmp" */
const char *TmpPath(const char *name);
void        TmpRemove(const char *name);

/* deterministic pseudo-random numbers (same on every machine) */
unsigned    TestRand(void);                 /* 0 .. 2^31-1 */
void        TestSeed(unsigned seed);

/* suites */
void run_math_tests(void);
void run_util_tests(void);
void run_algo_tests(void);
void run_physics_tests(void);
void run_level_tests(void);
void run_player_tests(void);
void run_game_tests(void);
void run_files_tests(void);
void run_raster_tests(void);
void run_font_palette_tests(void);
void run_console_tests(void);
void run_playthrough_tests(void);

#endif /* GG_TEST_FRAMEWORK_H */
