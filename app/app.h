/* Hub-internal declarations shared by main.c, screens.c, backdrop.c and smoke.c. */
#ifndef ARCADE_APP_H
#define ARCADE_APP_H

#include "arcade/cli.h"
#include "arcade/game.h"
#include "arcade/leaderboard.h"
#include "arcade/sandbox.h"

typedef enum Screen { SCR_NAME, SCR_MENU, SCR_LAUNCH, SCR_RESULT, SCR_LEADERBOARD, SCR_QUIT } Screen;

typedef struct AppState {
    char player[LB_NAME_LEN];
    char data_dir[512];
    char assets_dir[512];
    char scores_path[600];
    Leaderboard lb;
    const GameDef *games[64]; /* registry sorted for the menu: by order, then id */
    int game_count;
    int selected;             /* menu selection */
    const GameDef *launch;    /* game to start in SCR_LAUNCH */
    const GameDef *last_game; /* shown by SCR_RESULT */
    GameResult last_result;
    int last_rank; /* 1-based rank of the last score within its game, 0 if none */
    unsigned launch_counter;
} AppState;

/* main.c */
void app_sort_games(AppState *app);

/* screens.c */
Screen screen_name(AppState *app);
Screen screen_menu(AppState *app);
Screen screen_launch(AppState *app);
Screen screen_result(AppState *app);
Screen screen_leaderboard(AppState *app);

typedef enum ScreenPreview {
    PREVIEW_NAME,
    PREVIEW_MENU,
    PREVIEW_RESULT,
    PREVIEW_LEADERBOARD,
    PREVIEW_COUNT
} ScreenPreview;
const char *screens_preview_name(int which);
/* Draws one hub screen with the state in `app` (inside an already begun frame). */
void screens_draw_preview(AppState *app, int which, float t);

/* backdrop.c: the live sand simulation drawn behind the hub screens */
void backdrop_init(unsigned seed);
void backdrop_shutdown(void);
void backdrop_update(bool paint_sand, bool paint_water, int mouse_x, int mouse_y);
void backdrop_draw(float alpha);

/* smoke.c */
int smoke_run(const CliOptions *opt, const char *data_dir, const char *assets_dir);

#endif
