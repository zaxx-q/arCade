/* arCade entry point: parses the command line, then runs the screen state machine. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"
#include "online.h"
#include "arcade/str.h"
#include "arcade/version.h"

/* Menu order: lower GameDef.order first, ties broken by id (insertion sort on a pointer array).
 * Copies the (up to `cap`) games of `src` into `dst` sorted that way and returns how many. */
static int sort_games(const GameDef *const *src, int n, const GameDef **dst, int cap) {
    int count = 0;
    for (int i = 0; i < n && count < cap; i++)
        dst[count++] = src[i];
    for (int i = 1; i < count; i++) {
        const GameDef *key = dst[i];
        int j = i - 1;
        while (j >= 0 && (dst[j]->order > key->order ||
                          (dst[j]->order == key->order && strcmp(dst[j]->id, key->id) > 0))) {
            dst[j + 1] = dst[j];
            j--;
        }
        dst[j + 1] = key;
    }
    return count;
}

/* The menu starts with only the games from games/; the ones from deferred/ wait in app->deferred. */
void app_sort_games(AppState *app) {
    app->game_count =
        sort_games(g_arcade_games, g_arcade_game_count, app->games, (int)ARC_ARRAY_LEN(app->games));
    app->listed_count = app->game_count;
    app->deferred_count = sort_games(g_arcade_deferred, g_arcade_deferred_count, app->deferred,
                                     (int)ARC_ARRAY_LEN(app->deferred));
    app->archive_open = false;
}

/* Unlocks the archive: the deferred games join the end of the menu (and of the leaderboard tabs). */
void app_open_archive(AppState *app) {
    if (app->archive_open)
        return;
    for (int i = 0; i < app->deferred_count && app->game_count < (int)ARC_ARRAY_LEN(app->games); i++)
        app->games[app->game_count++] = app->deferred[i];
    app->archive_open = true;
}

int app_all_games(const AppState *app, const GameDef **out, int cap) {
    int n = 0;
    for (int i = 0; i < app->listed_count && n < cap; i++)
        out[n++] = app->games[i];
    for (int i = 0; i < app->deferred_count && n < cap; i++)
        out[n++] = app->deferred[i];
    return n;
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

/* --list shows the games on the hub menu; --all adds the deferred ones, marked as such. */
static int list_games(bool include_deferred) {
    static AppState app;
    app_sort_games(&app);
    for (int i = 0; i < app.listed_count; i++) {
        const GameDef *g = app.games[i];
        printf("%-14s %-22s %s\n", g->id, g->title, g->tagline);
    }
    if (include_deferred)
        for (int i = 0; i < app.deferred_count; i++) {
            const GameDef *g = app.deferred[i];
            printf("%-14s %-22s %s  [deferred]\n", g->id, g->title, g->tagline);
        }
    if (include_deferred)
        printf("%d game(s), %d of them deferred\n", app.listed_count + app.deferred_count,
               app.deferred_count);
    else
        printf("%d game(s)\n", app.listed_count);
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
        return list_games(opt.show_all);

    static AppState app; /* static: contains the ~190 KB leaderboard and the online client */
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
    online_init(&app);                 /* the online leaderboard: on by default, after the first-run notice */
    /* app.player stays empty: the name screen is the first one and does not accept an empty name, so nobody
     * plays under a default one */
    app_sort_games(&app);

    arcade_gfx_init(false, app.assets_dir);
    if (!IsWindowReady()) {
        fprintf(stderr, "error: could not open the game window\n");
        return 1;
    }
    backdrop_init((unsigned)time(NULL));
    if (opt.show_all) { /* --all: the archive is open from the start, no need to fill the vault */
        app_open_archive(&app);
        backdrop_vault_open_now();
    }

    if (online_first_run(&app)) /* on by default: tell the player what is shared, before anything is sent */
        screen_online_first_run(&app);

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
    online_shutdown(&app);
    backdrop_shutdown();
    arcade_gfx_shutdown();
    return 0;
}
