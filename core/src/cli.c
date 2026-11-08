#include "arcade/cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

static bool parse_uint(const char *s, long max, long *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < 0 || v > max)
        return false;
    *out = v;
    return true;
}

static bool fail(CliOptions *o, const char *msg, const char *arg) {
    snprintf(o->error, sizeof(o->error), "%s: %s", msg, arg ? arg : "");
    return false;
}

bool cli_parse(int argc, char **argv, CliOptions *o) {
    memset(o, 0, sizeof(*o));
    o->frames = 300;
    o->seed = 1234;
    arc_strlcpy(o->out_dir, "smoke-out", sizeof(o->out_dir));

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        bool has_value = i + 1 < argc;
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            o->show_help = true;
        } else if (strcmp(a, "--version") == 0 || strcmp(a, "-v") == 0) {
            o->show_version = true;
        } else if (strcmp(a, "--list") == 0) {
            o->list_games = true;
        } else if (strcmp(a, "--smoke") == 0) {
            if (!has_value)
                return fail(o, "missing value for", a);
            o->smoke = true;
            arc_strlcpy(o->smoke_target, argv[++i], sizeof(o->smoke_target));
        } else if (strcmp(a, "--frames") == 0) {
            long v;
            if (!has_value || !parse_uint(argv[i + 1], 1000000, &v) || v < 1)
                return fail(o, "invalid value for --frames", has_value ? argv[i + 1] : NULL);
            o->frames = (int)v;
            i++;
        } else if (strcmp(a, "--seed") == 0) {
            long v;
            if (!has_value || !parse_uint(argv[i + 1], 2000000000L, &v))
                return fail(o, "invalid value for --seed", has_value ? argv[i + 1] : NULL);
            o->seed = (unsigned)v;
            i++;
        } else if (strcmp(a, "--out") == 0 || strcmp(a, "--data-dir") == 0 ||
                   strcmp(a, "--assets-dir") == 0) {
            if (!has_value)
                return fail(o, "missing value for", a);
            char *dst = a[2] == 'o' ? o->out_dir : (a[2] == 'd' ? o->data_dir : o->assets_dir);
            size_t cap = a[2] == 'o' ? sizeof(o->out_dir)
                                     : (a[2] == 'd' ? sizeof(o->data_dir) : sizeof(o->assets_dir));
            arc_strlcpy(dst, argv[++i], cap);
        } else {
            return fail(o, "unknown option", a);
        }
    }
    return true;
}

const char *cli_usage(void) {
    return "arCade\n"
           "\n"
           "Usage: arCade [options]\n"
           "  --version, -v          print the version and exit\n"
           "  --list                 list the registered games and exit\n"
           "  --smoke <id|all>       automated run with scripted input (CI); needs --frames etc.\n"
           "  --frames N             frames per game in a smoke run (default 300)\n"
           "  --seed N               RNG seed for a smoke run (default 1234)\n"
           "  --out DIR              folder for smoke screenshots (default smoke-out)\n"
           "  --data-dir DIR         folder for scores.txt (default: next to the executable)\n"
           "  --assets-dir DIR       folder containing assets/ (default: auto-detect)\n"
           "  --help, -h             show this help\n";
}
