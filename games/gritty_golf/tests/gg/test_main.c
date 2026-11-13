/*
 * test_main.c - runs every suite and returns a non-zero exit code on failure
 *               (this is what CTest and GitHub Actions look at).
 */
#include <stdio.h>
#include <string.h>
#include "test_framework.h"

int g_checks = 0, g_failures = 0, g_tests = 0, g_tests_failed = 0;

static char s_path[256];
const char *TmpPath(const char *name)
{
    snprintf(s_path, sizeof s_path, "gg_test_%s.tmp", name);
    return s_path;
}
void TmpRemove(const char *name)
{
    char p[256];
    snprintf(p, sizeof p, "gg_test_%s.tmp", name);
    remove(p);
    snprintf(p, sizeof p, "gg_test_%s.tmp.tmp", name);   /* leftovers of atomic writes */
    remove(p);
    snprintf(p, sizeof p, "gg_test_%s.tmp.bad", name);
    remove(p);
}

static unsigned s_rng = 12345u;
void TestSeed(unsigned seed) { s_rng = seed; }
unsigned TestRand(void)       /* linear congruential generator */
{
    s_rng = s_rng * 1103515245u + 12345u;
    return (s_rng >> 1) & 0x7FFFFFFFu;
}

typedef struct { const char *name; void (*run)(void); } Suite;

int main(int argc, char **argv)
{
    static const Suite suites[] = {
        { "math & vectors",        run_math_tests        },
        { "strings & utilities",   run_util_tests        },
        { "sorting & searching",   run_algo_tests        },
        { "physics",               run_physics_tests     },
        { "level runtime",         run_level_tests       },
        { "player state machine",  run_player_tests      },
        { "game controller",       run_game_tests        },
        { "files (save/csv/cfg)",  run_files_tests       },
        { "rasteriser",            run_raster_tests      },
        { "font & palette",        run_font_palette_tests },
        { "playthrough (replays)", run_playthrough_tests }
    };
    const char *only = (argc > 1) ? argv[1] : NULL;       /* ./test_logic physics */

    for (size_t i = 0; i < sizeof suites / sizeof suites[0]; i++) {
        if (only != NULL && strstr(suites[i].name, only) == NULL)
            continue;
        printf("\n== %s\n", suites[i].name);
        suites[i].run();
    }

    printf("\n--------------------------------------------------------\n");
    printf("%d tests (%d failed), %d checks (%d failed)\n",
           g_tests, g_tests_failed, g_checks, g_failures);
    puts(g_failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return g_failures == 0 ? 0 : 1;
}
