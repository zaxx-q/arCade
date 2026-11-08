/* `arCade --smoke <id|all>`: runs games with scripted random input for N frames and exports
 * screenshots. Used by CI (headless build) and handy locally to check that nothing crashes. */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "arcade/gfx.h"

int smoke_run(const CliOptions *opt, const char *data_dir, const char *assets_dir) {
    arcade_gfx_init(true);
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
    arcade_gfx_shutdown();
    if (ran == 0) {
        fprintf(stderr, "smoke: no registered game matches '%s' (try --list)\n", opt->smoke_target);
        return 1;
    }
    printf("smoke: %d game(s) run, %d failed\n", ran, failed);
    return failed ? 1 : 0;
}
