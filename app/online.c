#include "online.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arcade/str.h"
#include "net.h"

#ifndef ARCADE_ONLINE_DEFAULT_URL
#define ARCADE_ONLINE_DEFAULT_URL ""
#endif

static char g_server[256]; /* the address in use */

static long long now_seconds(void) {
    return (long long)time(NULL);
}

/* "<data dir>/<name>", with a '/' in between when the directory does not end with one. */
static void data_path(const AppState *app, const char *name, char *out, size_t size) {
    size_t n = strlen(app->data_dir);
    bool slash = n > 0 && app->data_dir[n - 1] != '/' && app->data_dir[n - 1] != '\\';
    snprintf(out, size, "%s%s%s", app->data_dir, slash ? "/" : "", name);
}

const char *online_server(void) {
    return g_server[0] != '\0' ? g_server
                               : ARCADE_ONLINE_DEFAULT_URL; /* never blank: the consent text shows it */
}

bool online_ready(const AppState *app) {
    return app->online_ready;
}

bool online_enabled(const AppState *app) {
    return app->online_ready && app->lbn.enabled && !app->online_first_run;
}

/* online.cfg holds one line, "enabled|1" or "enabled|0". Returns 1 for exactly "enabled|1", 0 for anything
 * else that is there, a damaged file included (it must never turn back on what the player turned off), and -1
 * when there is no file at all: the first run, when the default (on) applies after the player has been told.
 */
static int load_choice(const AppState *app) {
    FILE *fp = fopen(app->online_cfg_path, "r");
    if (!fp)
        return -1;
    char line[64];
    bool on = fgets(line, sizeof(line), fp) != NULL && strncmp(line, "enabled|1", 9) == 0 &&
              (line[9] == '\0' || line[9] == '\n' || line[9] == '\r');
    fclose(fp);
    return on ? 1 : 0;
}

static void save_choice(const AppState *app, bool on) {
    FILE *fp = fopen(app->online_cfg_path, "w");
    if (!fp) {
        fprintf(stderr, "warning: could not write %s\n", app->online_cfg_path);
        return;
    }
    fprintf(fp, "enabled|%d\n", on ? 1 : 0);
    fclose(fp);
}

void online_init(AppState *app) {
    char outbox[600], cache[600], token[600];
    data_path(app, "online.cfg", app->online_cfg_path, sizeof(app->online_cfg_path));
    data_path(app, "outbox.txt", outbox, sizeof(outbox));
    data_path(app, "online_cache.txt", cache, sizeof(cache));
    data_path(app, "identity.txt", token, sizeof(token));

    const char *env = getenv("ARCADE_ONLINE_URL");
    arc_strlcpy(g_server, (env != NULL && env[0] != '\0') ? env : ARCADE_ONLINE_DEFAULT_URL,
                sizeof(g_server));
    for (size_t n = strlen(g_server); n > 0 && g_server[n - 1] == '/';
         n--) /* "https://host/" -> "https://host" */
        g_server[n - 1] = '\0';

    app->online_ready = false;
#ifdef ARCADE_HAVE_CURL
    if (g_server[0] != '\0' && net_open(g_server)) {
        LbnTransport transport = net_transport();
        lbn_init(&app->lbn, &transport);
        app->online_ready = true;
    }
#endif
    if (!app->online_ready) {
        lbn_init(&app->lbn, NULL);
        return;
    }
    lbn_set_paths(&app->lbn, outbox, cache, token);
    lbn_load(&app->lbn);
    int choice = load_choice(app);
    app->online_first_run = choice < 0; /* no answer yet: on by default, but the player is told first */
    lbn_enable(&app->lbn, choice != 0);
}

void online_shutdown(AppState *app) {
    (void)app;
#ifdef ARCADE_HAVE_CURL
    net_close();
#endif
}

void online_set_enabled(AppState *app, bool on) {
    if (!app->online_ready)
        return;
    app->online_first_run = false; /* whatever the player chose, the notice has done its job */
    lbn_enable(&app->lbn, on);
    if (!on)
        lbn_clear_outbox(&app->lbn); /* turned off: nothing may leave later without the player's say-so */
    save_choice(app, on);
}

bool online_leaderboard_start(const AppState *app, int *tab) {
    *tab = 0;
    if (!online_enabled(app))
        return false;
    if (app->game_count > 0)
        *tab = (app->selected >= 0 && app->selected < app->game_count) ? app->selected + 1 : 1;
    return true;
}

void online_tick(AppState *app) {
    if (app->online_ready && !app->online_first_run)
        lbn_update(&app->lbn, now_seconds());
}

void online_submit(AppState *app, const char *table, int score) {
    if (online_enabled(app)) {
        long long now = now_seconds();
        lbn_queue_score(&app->lbn, table, app->player, score, now, now);
    }
}

void online_want(AppState *app, const char *table) {
    if (online_enabled(app))
        lbn_want_table(&app->lbn, table, now_seconds());
}

void online_refresh(AppState *app, const char *table) {
    if (online_enabled(app))
        lbn_refresh_table(&app->lbn, table, now_seconds());
}

bool online_first_run(const AppState *app) {
    return app->online_ready && app->online_first_run;
}

void online_answer_first_run(AppState *app, bool keep_on) {
    online_set_enabled(app, keep_on); /* saves the answer, and a "no" forgets anything waiting */
    app->online_first_run = false;
}
