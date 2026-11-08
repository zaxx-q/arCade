/* Command-line parsing for the hub (argc/argv). Pure C so it can be unit-tested. */
#ifndef ARCADE_CLI_H
#define ARCADE_CLI_H

#include "arcade/common.h"

typedef struct CliOptions {
    bool show_help;
    bool show_version;
    bool list_games;
    bool smoke;            /* --smoke <id|all>: headless-style automated run */
    char smoke_target[48]; /* game id or "all" */
    int frames;            /* --frames N (smoke), default 300 */
    unsigned seed;         /* --seed N, default 1234 */
    char out_dir[256];     /* --out DIR (screenshots), default "smoke-out" */
    char data_dir[256];    /* --data-dir DIR, default "" = next to the executable */
    char assets_dir[256];  /* --assets-dir DIR, default "" = auto-detect */
    char error[96];        /* filled when cli_parse returns false */
} CliOptions;

bool cli_parse(int argc, char **argv, CliOptions *opt);
const char *cli_usage(void);

#endif
