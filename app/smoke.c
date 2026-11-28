/* `arCade --smoke <id|all>`: runs games with scripted random input for N frames and exports
 * screenshots. Used by CI (headless build) and handy locally to check that nothing crashes. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/osk.h"
#include "arcade/touch.h"

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
        arcade_flush_input(); /* these are hub screens: with ARCADE_TOUCH_UI=1 they get no game controls */
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

/* ---- touch screens ----------------------------------------------------------------------------------------
 * The rules of the virtual controls and of the on-screen keyboard are unit-tested (tests/test_touch.c,
 * test_osk.c). This checks what only the real front-end can: that fingers reach them through the same calls
 * the hub and the games make, with scripted fingers numbered like a phone numbers them. */

#define TOUCH_MAX_CHECKS 40
static const char *g_check_name[TOUCH_MAX_CHECKS];
static bool g_check_ok[TOUCH_MAX_CHECKS];
static int g_check_n;

static void expect(const char *name, bool ok) {
    if (g_check_n < TOUCH_MAX_CHECKS) {
        g_check_name[g_check_n] = name;
        g_check_ok[g_check_n] = ok;
        g_check_n++;
    }
}

/* One drawn frame with the fingers `pts` on the glass. The hub's draw code registers its tap zones while it
 * draws: this registers `zone` (if any) the same way. */
static void touch_frame(const TouchPoint *pts, int n, const Rectangle *zone, int zone_key) {
    arcade_touch_test_fingers(pts, n);
    arcade_frame_begin();
    if (zone != NULL)
        arcade_touch_key_zone(*zone, zone_key);
    arcade_frame_end();
}

static TouchPoint finger(int id, float x, float y) {
    TouchPoint p = {id, x, y};
    return p;
}

/* A finger down on (x, y) for one frame, then lifted for one. */
static void tap(int id, float x, float y, const Rectangle *zone, int zone_key) {
    TouchPoint p = finger(id, x, y);
    touch_frame(&p, 1, zone, zone_key);
}

static void lift(const Rectangle *zone, int zone_key) {
    touch_frame(NULL, 0, zone, zone_key);
}

static int run_touch_check(int *ran) {
    g_check_n = 0;
    GameContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.player = "SMOKE";
    ctx.seed = 1;
    ctx.headless = true;
    arcade_touch_test_begin();

    const Rectangle view = arcade_view_bounds(); /* the window in design units */
    const float left = view.x, top = view.y, right = view.x + view.width, bottom = view.y + view.height;
    TouchLayout pad;
    touch_layout_fit(&pad, left, top, right, bottom);
    Osk kb;
    osk_init(&kb);
    osk_layout(&kb, (left + right) / 2.0f, bottom, arc_minf(1000.0f, right - left - 40.0f)); /* the hub's */

    /* -- the hub: a footer hint is a button -- */
    const Rectangle zone = {100.0f, 600.0f, 200.0f, 60.0f};
    arcade_flush_input();
    lift(&zone, KEY_L);
    expect("no finger, no key", !arcade_key_pressed(KEY_L));
    tap(3, 150.0f, 630.0f, &zone, KEY_L);
    expect("a tap on a zone presses its key", arcade_key_pressed(KEY_L));
    tap(3, 150.0f, 630.0f, &zone, KEY_L);
    expect("a finger that stays does not press again", !arcade_key_pressed(KEY_L));
    lift(&zone, KEY_L);
    tap(3, 150.0f, 630.0f, &zone, KEY_L);
    expect("the next tap presses again", arcade_key_pressed(KEY_L));
    lift(&zone, KEY_L);
    tap(4, 900.0f, 300.0f, &zone, KEY_L);
    expect("a tap outside the zone presses nothing", !arcade_key_pressed(KEY_L));
    tap(4, 150.0f, 630.0f, &zone, KEY_L);
    expect("a finger that slides into a zone does not press it", !arcade_key_pressed(KEY_L));
    lift(&zone, KEY_L);

    /* a finger that is already down when the screen changes belongs to the old screen */
    TouchPoint old = finger(7, 150.0f, 630.0f);
    arcade_touch_test_fingers(&old, 1);
    arcade_flush_input();
    touch_frame(&old, 1, &zone, KEY_L);
    expect("a finger down at a screen change is ignored", !arcade_key_pressed(KEY_L));
    lift(&zone, KEY_L);
    tap(7, 150.0f, 630.0f, &zone, KEY_L);
    expect("... until it is lifted and lands again", arcade_key_pressed(KEY_L));
    lift(&zone, KEY_L);

    /* -- the hub: the on-screen keyboard -- */
    arcade_flush_input();
    arcade_osk_open(true);
    lift(NULL, 0);
    int q = -1, shift = -1, enter = -1, del = -1;
    for (int i = 0; i < kb.count; i++) {
        if (kb.keys[i].kind == OSK_CHAR && kb.keys[i].ch == 'q')
            q = i;
        if (kb.keys[i].kind == OSK_SHIFT)
            shift = i;
        if (kb.keys[i].kind == OSK_ENTER)
            enter = i;
        if (kb.keys[i].kind == OSK_BACKSPACE)
            del = i;
    }
    if (q < 0 || shift < 0 || enter < 0 || del < 0) {
        expect("the keyboard has q, shift, Enter and Del", false);
    } else {
#define KEY_CENTRE(i) kb.keys[i].x + kb.keys[i].w / 2.0f, kb.keys[i].y + kb.keys[i].h / 2.0f
        tap(1, KEY_CENTRE(q), NULL, 0);
        expect("a key of the keyboard types its letter", arcade_text_char() == 'q');
        expect("... and only once", arcade_text_char() == 0);
        tap(1, KEY_CENTRE(q), NULL, 0);
        expect("a finger that stays on a key types it once", arcade_text_char() == 0);
        lift(NULL, 0);
        tap(1, KEY_CENTRE(shift), NULL, 0);
        lift(NULL, 0);
        tap(1, KEY_CENTRE(q), NULL, 0);
        expect("shift makes a capital", arcade_text_char() == 'Q');
        lift(NULL, 0);
        tap(1, KEY_CENTRE(del), NULL, 0);
        expect("Del is the Backspace key", arcade_key_pressed(KEY_BACKSPACE));
        lift(NULL, 0);
        tap(1, KEY_CENTRE(enter), NULL, 0);
        expect("Enter is the Enter key", arcade_key_pressed(KEY_ENTER));
        lift(NULL, 0);
    }
    expect("the keyboard is open until the screen changes", arcade_osk_is_open());
    arcade_flush_input();
    expect("the next screen starts without it", !arcade_osk_is_open());

    /* -- a game: stick, action button, pointer -- */
    arcade_loop_begin(&ctx);
    ArcadeInput in;
    lift(NULL, 0);
    tap(0, pad.stick.x + pad.stick.r, pad.stick.y, NULL, 0);
    arcade_poll(&in);
    expect("the stick moves the player", in.move_x > 0.9f && in.move_y == 0.0f && !in.fire);
    TouchPoint two[3] = {finger(0, pad.stick.x + pad.stick.r, pad.stick.y),
                         finger(1, pad.action.x, pad.action.y), finger(2, 640.0f, 360.0f)};
    touch_frame(two, 3, NULL, 0);
    arcade_poll(&in);
    expect("the action button is Space", in.action && in.action_pressed);
    expect("another finger is the pointer",
           in.fire && in.fire_pressed && in.aim_x == 640.0f && in.aim_y == 360.0f);
    expect("all three work at once", in.move_x > 0.9f);
    lift(NULL, 0);
    arcade_poll(&in);
    expect("lifting every finger releases everything", in.move_x == 0.0f && !in.action && !in.fire);

    /* -- a game: back asks twice -- */
    tap(5, pad.back.x, pad.back.y, NULL, 0);
    expect("the first tap on back only asks", !arcade_key_pressed(KEY_ESCAPE));
    lift(NULL, 0);
    tap(5, pad.back.x, pad.back.y, NULL, 0);
    expect("the second one leaves the game", arcade_key_pressed(KEY_ESCAPE));
    lift(NULL, 0);
    tap(5, pad.back.x, pad.back.y, NULL, 0);
    lift(NULL, 0);
    for (int i = 0; i < 2 * ARC_FPS + 5; i++)
        lift(NULL, 0);
    tap(5, pad.back.x, pad.back.y, NULL, 0);
    expect("... but not when the question has timed out", !arcade_key_pressed(KEY_ESCAPE));
    lift(NULL, 0);

    /* -- a game: the keyboard button, and typing in a game -- */
    tap(6, pad.keys.x, pad.keys.y, NULL, 0);
    expect("the keyboard button opens the keyboard", arcade_osk_is_open());
    lift(NULL, 0);
    osk_layout(&kb, (left + right) / 2.0f, bottom,
               arc_minf(840.0f, right - left - 40.0f)); /* in a game: narrower */
    if (q >= 0) {
        tap(6, KEY_CENTRE(q), NULL, 0);
        arcade_poll(&in);
        expect("a key typed in a game arrives as ArcadeInput.typed", in.typed == 'q');
        expect("... and is not a shot", !in.fire_pressed && !in.fire);
        lift(NULL, 0);
    }
    tap(6, pad.keys.x, pad.keys.y, NULL, 0);
    expect("the button closes it again", !arcade_osk_is_open());
    lift(NULL, 0);

    /* a finger down on the menu when the game starts is not a shot */
    TouchPoint menu = finger(9, 640.0f, 360.0f);
    arcade_touch_test_fingers(&menu, 1);
    arcade_flush_input();
    arcade_loop_begin(&ctx);
    touch_frame(&menu, 1, NULL, 0);
    arcade_poll(&in);
    expect("the finger that started the game is not a shot", !in.fire && !in.fire_pressed);
#undef KEY_CENTRE

    lift(NULL, 0);
    arcade_flush_input();
    arcade_touch_test_end();

    int failed = 0;
    for (int i = 0; i < g_check_n; i++)
        failed += g_check_ok[i] ? 0 : 1;
    printf("SMOKE %-14s %d scripted-finger checks, %d failed -> %s\n", "hub_touch", g_check_n, failed,
           failed == 0 ? "OK" : "FAIL");
    for (int i = 0; i < g_check_n; i++)
        if (!g_check_ok[i])
            printf("  FAILED: %s\n", g_check_name[i]);
    (*ran)++;
    return failed == 0 ? 0 : 1;
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
                  run_city_check(opt, app, data_dir, assets_dir, &ran) + run_touch_check(&ran);
    arcade_gfx_shutdown();
    if (ran == 0) {
        fprintf(stderr, "smoke: no registered game matches '%s' (try --list)\n", opt->smoke_target);
        return 1;
    }
    printf("smoke: %d game(s) run, %d failed\n", ran, failed);
    return failed ? 1 : 0;
}
