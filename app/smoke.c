/* `arCade --smoke <id|all>`: runs games with scripted random input for N frames and exports
 * screenshots. Used by CI (headless build) and handy locally to check that nothing crashes. */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "arcade/gfx.h"

/* Renders each hub screen (name, menu, result, leaderboard) with sample data, so CI and the README
 * screenshots cover the hub UI too. Returns the number of failed screenshots. */
static int run_hub_previews(const CliOptions *opt, AppState *app, const char *data_dir,
                            const char *assets_dir, int *ran) {
    static const char *const names[] = {"Ana", "Budi", "Citra", "Dewi", "Eko", "Fajar", "Gita"};
    memset(&app->lb, 0, sizeof(app->lb));
    for (int i = 0; i < 9 && app->game_count > 0; i++)
        lb_add(&app->lb, app->games[i % app->game_count]->id, names[i % 7], 3200 - i * 290, 1700000000LL + i);
    snprintf(app->player, sizeof(app->player), "Ana");
    app->selected = app->game_count > 1 ? 1 : 0;
    app->last_game = app->game_count > 0 ? app->games[0] : NULL;
    app->last_result.score = 1280;
    app->last_result.finished = true;
    app->last_rank = 1;

    backdrop_init(opt->seed);
    int failed = 0;
    for (int which = 0; which < PREVIEW_COUNT; which++) {
        if (which == PREVIEW_RESULT && app->last_game == NULL)
            continue;
        char shot[600];
        snprintf(shot, sizeof(shot), "%s/%s.png", opt->out_dir, screens_preview_name(which));
        GameContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.player = "SMOKE";
        ctx.seed = opt->seed;
        ctx.headless = true;
        ctx.max_frames = opt->frames < 150 ? opt->frames : 150; /* let the sand settle a little */
        ctx.data_dir = data_dir;
        ctx.assets_dir = assets_dir;
        ctx.screenshot_path = shot;
        arcade_loop_begin(&ctx);
        float t = 0.0f;
        while (arcade_loop_running()) {
            t += ARC_DT;
            backdrop_update(false, false, 0, 0);
            BeginDrawing();
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

int smoke_run(const CliOptions *opt, const char *data_dir, const char *assets_dir) {
    arcade_gfx_init(true, assets_dir);
    if (!IsWindowReady()) {
        fprintf(stderr, "smoke: could not open a window / software framebuffer\n");
        return 1;
    }
    MakeDirectory(opt->out_dir);

    AppState *app = NULL;
    static AppState storage; /* AppState holds a 28 KB leaderboard: static, not stack */
    app = &storage;
    app_sort_games(app);

    int ran = 0, failed = 0;
    for (int i = 0; i < app->game_count; i++) {
        const GameDef *g = app->games[i];
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
        failed += run_hub_previews(opt, app, data_dir, assets_dir, &ran);
    arcade_gfx_shutdown();
    if (ran == 0) {
        fprintf(stderr, "smoke: no registered game matches '%s' (try --list)\n", opt->smoke_target);
        return 1;
    }
    printf("smoke: %d game(s) run, %d failed\n", ran, failed);
    return failed ? 1 : 0;
}
