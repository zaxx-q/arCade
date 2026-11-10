/* arCade entry point: parses the command line, then runs the screen state machine. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/str.h"
#include "arcade/version.h"

/* Menu order: lower GameDef.order first, ties broken by id (insertion sort on the pointer array). */
void app_sort_games(AppState *app) {
    app->game_count = 0;
    for (int i = 0; i < g_arcade_game_count && i < (int)ARC_ARRAY_LEN(app->games); i++)
        app->games[app->game_count++] = g_arcade_games[i];
    for (int i = 1; i < app->game_count; i++) {
        const GameDef *key = app->games[i];
        int j = i - 1;
        while (j >= 0 && (app->games[j]->order > key->order ||
                          (app->games[j]->order == key->order && strcmp(app->games[j]->id, key->id) > 0))) {
            app->games[j + 1] = app->games[j];
            j--;
        }
        app->games[j + 1] = key;
    }
}

static void resolve_dirs(const CliOptions *opt, char *data_dir, size_t dn, char *assets_dir, size_t an) {
    const char *exe_dir = GetApplicationDirectory();
    arc_strlcpy(data_dir, opt->data_dir[0] ? opt->data_dir : exe_dir, dn);
    if (opt->assets_dir[0]) {
        arc_strlcpy(assets_dir, opt->assets_dir, an);
        return;
    }
    char candidate[600];
    snprintf(candidate, sizeof(candidate), "%sassets", exe_dir);
    if (DirectoryExists(candidate)) {
        arc_strlcpy(assets_dir, candidate, an);
    } else {
#ifdef ARCADE_SOURCE_ASSETS_DIR
        arc_strlcpy(assets_dir, ARCADE_SOURCE_ASSETS_DIR, an); /* running from the build tree */
#else
        arc_strlcpy(assets_dir, "assets", an);
#endif
    }
}

static int list_games(void) {
    static AppState app;
    app_sort_games(&app);
    for (int i = 0; i < app.game_count; i++) {
        const GameDef *g = app.games[i];
        printf("%-14s %-22s %-8s %s\n", g->id, g->title, g->ranked ? "ranked" : "unranked", g->tagline);
    }
    printf("%d game(s)\n", app.game_count);
    return 0;
}

int main(int argc, char **argv) {
    CliOptions opt;
    if (!cli_parse(argc, argv, &opt)) {
        fprintf(stderr, "error: %s\n\n%s", opt.error, cli_usage());
        return 2;
    }
    if (opt.show_help) {
        fputs(cli_usage(), stdout);
        return 0;
    }
    if (opt.show_version) {
        printf("arCade %s (raylib %s)\n", ARCADE_VERSION_STRING, ARCADE_RAYLIB_TAG_STRING);
        return 0;
    }
    if (opt.list_games)
        return list_games();

    static AppState app; /* static: contains the 28 KB leaderboard */
    memset(&app, 0, sizeof(app));
    resolve_dirs(&opt, app.data_dir, sizeof(app.data_dir), app.assets_dir, sizeof(app.assets_dir));
    if (opt.smoke)
        return smoke_run(&opt, app.data_dir, app.assets_dir);

    snprintf(app.scores_path, sizeof(app.scores_path), "%s%sscores.txt", app.data_dir,
             (app.data_dir[0] && app.data_dir[strlen(app.data_dir) - 1] != '/' &&
              app.data_dir[strlen(app.data_dir) - 1] != '\\')
                 ? "/"
                 : "");
    lb_load(&app.lb, app.scores_path); /* a missing file just means "no scores yet" */
    arc_strlcpy(app.player, "PLAYER", sizeof(app.player));
    app_sort_games(&app);

    arcade_gfx_init(false, app.assets_dir);
    if (!IsWindowReady()) {
        fprintf(stderr, "error: could not open the game window\n");
        return 1;
    }
    backdrop_init((unsigned)time(NULL));

    Screen s = SCR_NAME;
    while (s != SCR_QUIT && !arcade_quit_requested()) {
        switch (s) {
        case SCR_NAME:
            s = screen_name(&app);
            break;
        case SCR_MENU:
            s = screen_menu(&app);
            break;
        case SCR_LAUNCH:
            s = screen_launch(&app);
            break;
        case SCR_RESULT:
            s = screen_result(&app);
            break;
        case SCR_LEADERBOARD:
            s = screen_leaderboard(&app);
            break;
        case SCR_QUIT:
            break;
        }
    }
    backdrop_shutdown();
    arcade_gfx_shutdown();
    return 0;
}
