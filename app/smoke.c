/* `arCade --smoke <id|all>`: runs games with scripted random input for N frames and exports
 * screenshots. Used by CI (headless build) and handy locally to check that nothing crashes. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"

/* The Online tab and the online line of the result screen, drawn by a client that has no network: it is
 * switched on, "received" two tables earlier (one fresh, one hours old) and was told how the shown run did.
 */
static void sample_online(AppState *app) {
    static const char *const online_names[14] = {"Zul",  "Rina", "Budi",  "Sari", "Tono", "Wati", "Yoga",
                                                 "Dewi", "Eko",  "Fajar", "Gita", "Ana",  "Hadi", "Ivan"};
    lbn_init(&app->lbn, NULL);
    app->online_ready = true;
    app->lbn.enabled = true; /* set directly: lbn_enable() refuses a client without a transport */
    const long long now = (long long)time(NULL);
    static OnlineTable t;
    for (int k = 0; k < 2; k++) {
        memset(&t, 0, sizeof(t));
        snprintf(t.key, sizeof(t.key), "%s", k == 0 ? "different_color" : "loop_lap");
        t.fetched = now - (k == 0 ? 125 : 3 * 3600);
        t.players = k == 0 ? 87 : 40;
        t.count = 14;
        for (int i = 0; i < t.count; i++) { /* Ana is 12th: below the ten rows on screen */
            t.rows[i].rank = i + 1;
            snprintf(t.rows[i].player, sizeof(t.rows[i].player), "%s", online_names[i]);
            t.rows[i].score = (k == 0 ? 380 : 60) - i * (k == 0 ? 17 : 3);
            t.rows[i].stamp = 1700000000LL + i;
        }
        lbn_cache_put(&app->lbn.cache, &t);
    }
    if (app->last_game != NULL) { /* the server's answer to the run on the result screen */
        LbnAck *ack = &app->lbn.ack;
        lb_board_key(ack->table, sizeof(ack->table), app->last_game->id, app->last_board);
        snprintf(ack->player, sizeof(ack->player), "%s", app->player);
        ack->valid = true;
        ack->score = app->last_result.score;
        ack->rank = 3;
        ack->players = 87;
    }
}

/* Renders each hub screen (name, menu, archive, result, leaderboard) with sample data, so CI and the README
 * screenshots cover the hub UI too. Most of them show the default backdrop (the city); one shows the menu on
 * the sand with the vault, and one shows the menu hidden while the player flies. Returns the number of failed
 * screenshots. */
static int run_hub_previews(const CliOptions *opt, AppState *app, const char *data_dir,
                            const char *assets_dir, int *ran) {
    static const char *const names[] = {"Ana", "Budi", "Citra", "Dewi", "Eko", "Fajar", "Gita"};
    memset(&app->lb, 0, sizeof(app->lb));
    for (int i = 0; i < 9 && app->game_count > 0; i++)
        lb_add(&app->lb, app->games[i % app->game_count]->id, names[i % 7], 3200 - i * 290, 1700000000LL + i);
    /* One game with separate tables (mode / song / difficulty), so the board screens have rows to show. */
    static const char *const boards[] = {"Catch / Satin Panties / normal", "Dodge / Bopeebo / normal",
                                         "Dodge / Bopeebo / hard", "Catch / Bopeebo / normal"};
    for (int b = 0; b < 4; b++) {
        char key[LB_GAME_LEN];
        lb_board_key(key, sizeof(key), "beat_dodge", boards[b]);
        for (int k = 0; k < 4; k++)
            lb_add(&app->lb, key, names[(b + k) % 7], 1650 - b * 120 - k * 210, 1700001000LL + b * 10 + k);
    }
    /* A long board: 14 players, more than the ten rows on screen, and the current player (Ana) is the last of
     * them, so the leaderboard shows her row pinned under the visible ones. */
    static const char *const more[] = {"Hana", "Indra", "Joko", "Kiki", "Lina", "Made", "Nina"};
    char long_key[LB_GAME_LEN];
    lb_board_key(long_key, sizeof(long_key), "beat_dodge", "Dodge / Thorns / easy");
    for (int k = 0; k < 14; k++)
        lb_add(&app->lb, long_key, k < 7 ? names[k] : more[k - 7], 1500 - ((k + 13) % 14) * 90,
               1700002000LL + k);
    snprintf(app->player, sizeof(app->player), "Ana");
    app->selected = app->game_count > 1 ? 1 : 0;
    app->last_game = app->game_count > 0 ? app->games[0] : NULL;
    app->last_result.score = 1280;
    app->last_result.finished = true;
    app->last_rank = 1;
    app->last_best = 1280;
    app->last_improved = true;
    /* the result screen names the table when the game used one */
    snprintf(app->last_board, sizeof(app->last_board), "%s",
             app->last_game != NULL && strcmp(app->last_game->id, "beat_dodge") == 0
                 ? "Dodge / Bopeebo / normal"
                 : "");
    sample_online(app);

    backdrop_init(opt->seed);
    /* 15 s of sand and water, then 15 s of the drone's tour, so the screenshots show settled scenes */
    backdrop_set_mode(BACKDROP_SANDBOX);
    for (int i = 0; i < 900; i++)
        backdrop_update(false, false, 0, 0);
    backdrop_set_mode(BACKDROP_CITY);
    for (int i = 0; i < 900; i++)
        backdrop_update(false, false, 0, 0);
    int failed = 0;
    for (int which = 0; which < PREVIEW_COUNT; which++) {
        if (which == PREVIEW_RESULT && app->last_game == NULL)
            continue;
        backdrop_set_mode(which == PREVIEW_MENU_SANDBOX ? BACKDROP_SANDBOX : BACKDROP_CITY);
        backdrop_set_flying(which == PREVIEW_FLY);
        if (which == PREVIEW_ARCHIVE) { /* from here on the vault is open, as if the player had filled it */
            app_open_archive(app);
            backdrop_vault_open_now();
            app->archive_toast = 3.0f;
        }
        if (which == PREVIEW_ARCHIVE_END)
            app->selected = app->game_count - 1;
        if (which == PREVIEW_RESULT)
            app->selected = 1 < app->game_count ? 1 : 0;
        char shot[600];
        snprintf(shot, sizeof(shot), "%s/%s.png", opt->out_dir, screens_preview_name(which));
        GameContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.player = "SMOKE";
        ctx.seed = opt->seed;
        ctx.headless = true;
        /* the sand needs a moment to settle; the city is slow on the CPU rasteriser and one frame is enough
         */
        int preview_frames = which == PREVIEW_MENU_SANDBOX ? 150 : 60;
        ctx.max_frames = opt->frames < preview_frames ? opt->frames : preview_frames;
        ctx.data_dir = data_dir;
        ctx.assets_dir = assets_dir;
        ctx.screenshot_path = shot;
        arcade_loop_begin(&ctx);
        float t = 0.0f;
        while (arcade_loop_running()) {
            t += ARC_DT;
            if (which == PREVIEW_FLY) { /* the player holds W: it leaves the tour and flies */
                ArcadeInput in;
                arcade_input_clear(&in);
                in.move_y = -1.0f;
                backdrop_fly(&in);
            } else {
                backdrop_update(false, false, 0, 0);
            }
            arcade_frame_begin();
            screens_draw_preview(app, which, t);
            arcade_frame_end();
        }
        bool ok = arcade_screenshot_ok();
        printf("SMOKE %-14s frames<=%d screenshot=%s -> %s\n", screens_preview_name(which), ctx.max_frames,
               shot, ok ? "OK" : "FAIL");
        if (!ok)
            failed++;
        (*ran)++;
    }
    backdrop_shutdown();
    return failed;
}

/* End-to-end check of the archive unlock, with the real scene: (1) nobody touches the hub for a long time and
 * the vault must stay closed (the ambient sand and water never fill it by accident); (2) a player holding the
 * right mouse button over the vault opens it within a few seconds. Returns 1 if a check failed. */
static int run_vault_check(const CliOptions *opt, int *ran) {
    backdrop_init(opt->seed);
    backdrop_set_mode(BACKDROP_SANDBOX); /* the vault is part of the sand scene */
    for (int i = 0; i < 6000; i++)       /* 100 seconds of an idle hub */
        backdrop_update(false, false, 0, 0);
    bool idle_ok = !backdrop_vault_is_open() && backdrop_vault_progress() < 0.2f;

    int mx = 0, my = 0;
    backdrop_vault_mouth(&mx, &my);
    int frames = 0;
    while (frames < 600 && !backdrop_vault_is_open()) {
        backdrop_update(false, true, mx, my); /* right mouse button held above the vault */
        frames++;
    }
    bool pour_ok = backdrop_vault_is_open();
    backdrop_shutdown();
    printf("SMOKE %-14s idle 6000 frames stays closed=%s, pouring opens it after %d frames=%s -> %s\n",
           "hub_vault", idle_ok ? "yes" : "NO", frames, pour_ok ? "yes" : "NO",
           idle_ok && pour_ok ? "OK" : "FAIL");
    (*ran)++;
    return idle_ok && pour_ok ? 0 : 1;
}

/* End-to-end check of who holds the city's camera, through the same calls the menu makes: (1) an idle hub
 * keeps the drone flying for a long time; (2) a player who takes the camera and holds W leaves the tour; (3)
 * giving the camera back (Esc) puts the drone in charge again; (4) the same when the backdrop is switched to
 * the sand and back while flying; (5) the archive lists Synthwave City too, so it can be started on its own
 * while the backdrop's copy exists: afterwards the backdrop must still draw (the two share the renderer's
 * textures). Returns 1 if a check failed. */
static int run_city_check(const CliOptions *opt, AppState *app, const char *data_dir, const char *assets_dir,
                          int *ran) {
    backdrop_init(opt->seed);
    ArcadeInput in;
    arcade_input_clear(&in);

    for (int i = 0; i < 3600; i++) /* a minute of an idle hub */
        backdrop_update(false, false, 0, 0);
    bool idle_ok = backdrop_city_touring();

    backdrop_set_flying(true);
    in.move_y = -1.0f;
    for (int i = 0; i < 30; i++)
        backdrop_fly(&in);
    bool flies_ok = !backdrop_city_touring();

    backdrop_set_flying(false); /* Esc */
    backdrop_update(false, false, 0, 0);
    bool back_ok = backdrop_city_touring();

    backdrop_set_flying(true);
    for (int i = 0; i < 30; i++)
        backdrop_fly(&in);
    bool again_ok = !backdrop_city_touring();
    backdrop_set_mode(BACKDROP_SANDBOX); /* the button, while flying: the camera must come back too */
    backdrop_set_mode(BACKDROP_CITY);
    backdrop_update(false, false, 0, 0);
    bool switch_ok = backdrop_city_touring();

    bool shared_ok = false;
    const GameDef *all[128];
    int all_count = app_all_games(app, all, (int)ARC_ARRAY_LEN(all));
    for (int i = 0; i < all_count; i++) {
        if (strcmp(all[i]->id, "synthwave_city") != 0)
            continue;
        GameContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.player = "SMOKE";
        ctx.seed = opt->seed;
        ctx.headless = true;
        ctx.max_frames = 10;
        ctx.data_dir = data_dir;
        ctx.assets_dir = assets_dir;
        (void)all[i]->play(&ctx); /* its own copy of the game runs on top of the backdrop's */
        char shot[600];
        snprintf(shot, sizeof(shot), "%s/hub_city_after_game.png", opt->out_dir);
        ctx.max_frames = 1;
        ctx.screenshot_path = shot;
        arcade_loop_begin(&ctx);
        while (
            arcade_loop_running()) { /* one frame of the backdrop: a flat frame means its textures are gone */
            arcade_frame_begin();
            backdrop_draw(1.0f);
            arcade_frame_end();
        }
        shared_ok = arcade_screenshot_ok();
    }
    backdrop_shutdown();

    bool ok = idle_ok && flies_ok && back_ok && again_ok && switch_ok && shared_ok;
    printf("SMOKE %-14s idle keeps the tour=%s, W takes the camera=%s, Esc gives it back=%s, "
           "taken again=%s, sandbox round trip gives it back=%s, still draws after the game ran=%s -> %s\n",
           "hub_city", idle_ok ? "yes" : "NO", flies_ok ? "yes" : "NO", back_ok ? "yes" : "NO",
           again_ok ? "yes" : "NO", switch_ok ? "yes" : "NO", shared_ok ? "yes" : "NO", ok ? "OK" : "FAIL");
    (*ran)++;
    return ok ? 0 : 1;
}

int smoke_run(const CliOptions *opt, const char *data_dir, const char *assets_dir) {
    arcade_gfx_init_sized(true, assets_dir, opt->width, opt->height, false); /* --size: the frame size */
    if (!IsWindowReady()) {
        fprintf(stderr, "smoke: could not open a window / software framebuffer\n");
        return 1;
    }
    MakeDirectory(opt->out_dir);

    AppState *app = NULL;
    static AppState storage; /* AppState holds a ~190 KB leaderboard: static, not stack */
    app = &storage;
    app_sort_games(app);

    /* every game is smoke-tested, the ones parked in deferred/ included: deferring hides a game, not its bugs
     */
    const GameDef *all[128];
    int all_count = app_all_games(app, all, (int)ARC_ARRAY_LEN(all));

    int ran = 0, failed = 0;
    for (int i = 0; i < all_count; i++) {
        const GameDef *g = all[i];
        if (strcmp(opt->smoke_target, "all") != 0 && strcmp(opt->smoke_target, g->id) != 0)
            continue;

        char shot[600];
        snprintf(shot, sizeof(shot), "%s/%s.png", opt->out_dir, g->id);
        GameContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.player = "SMOKE";
        ctx.seed = opt->seed;
        ctx.headless = true;
        ctx.max_frames = opt->frames;
        ctx.data_dir = data_dir;
        ctx.assets_dir = assets_dir;
        ctx.screenshot_path = shot;

        GameResult r = g->play(&ctx);
        bool ok = r.score >= 0 && arcade_screenshot_ok();
        printf("SMOKE %-14s frames<=%d score=%d finished=%d screenshot=%s -> %s\n", g->id, opt->frames,
               r.score, r.finished ? 1 : 0, shot, ok ? "OK" : "FAIL");
        if (!ok)
            failed++;
        ran++;
    }
    if (strcmp(opt->smoke_target, "all") == 0 || strcmp(opt->smoke_target, "hub") == 0)
        failed += run_hub_previews(opt, app, data_dir, assets_dir, &ran) + run_vault_check(opt, &ran) +
                  run_city_check(opt, app, data_dir, assets_dir, &ran);
    arcade_gfx_shutdown();
    if (ran == 0) {
        fprintf(stderr, "smoke: no registered game matches '%s' (try --list)\n", opt->smoke_target);
        return 1;
    }
    printf("smoke: %d game(s) run, %d failed\n", ran, failed);
    return failed ? 1 : 0;
}
