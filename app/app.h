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
    const GameDef *games[64]; /* menu rows: games/ sorted by order then id, then (archive open) deferred/ */
    int game_count;           /* rows currently on the menu */
    int listed_count;         /* how many leading rows come from games/; the rest are the archive */
    const GameDef *deferred[64]; /* games parked in deferred/, sorted the same way */
    int deferred_count;
    bool archive_open;        /* the vault was filled (or --all): deferred games are appended to the menu */
    float archive_toast;      /* seconds left of the "archive opened" message */
    int selected;             /* menu selection */
    const GameDef *launch;    /* game to start in SCR_LAUNCH */
    const GameDef *last_game; /* shown by SCR_RESULT */
    GameResult last_result;
    int last_rank; /* 1-based rank of the last score within its game, 0 if none */
    unsigned launch_counter;
} AppState;

/* main.c */
void app_sort_games(
    AppState *app); /* fills games[] from games/ and deferred[] from deferred/ (archive closed) */
void app_open_archive(AppState *app); /* appends the deferred games to the menu; safe to call twice */
/* Every registered game, listed first and then deferred (smoke runs and --list --all). Returns how many. */
int app_all_games(const AppState *app, const GameDef **out, int cap);

/* screens.c */
Screen screen_name(AppState *app);
Screen screen_menu(AppState *app);
Screen screen_launch(AppState *app);
Screen screen_result(AppState *app);
Screen screen_leaderboard(AppState *app);

typedef enum ScreenPreview {
    PREVIEW_NAME,
    PREVIEW_MENU,        /* archive closed: only the games from games/ */
    PREVIEW_ARCHIVE,     /* archive open, top of the list */
    PREVIEW_ARCHIVE_END, /* archive open, scrolled to the last game */
    PREVIEW_RESULT,
    PREVIEW_LEADERBOARD,
    PREVIEW_COUNT
} ScreenPreview;
const char *screens_preview_name(int which);
/* Draws one hub screen with the state in `app` (inside an already begun frame). */
void screens_draw_preview(AppState *app, int which, float t);

/* backdrop.c: the live sand simulation drawn behind the hub screens, with the vault that opens the archive */
void backdrop_init(unsigned seed);
void backdrop_shutdown(void);
void backdrop_update(bool paint_sand, bool paint_water, int mouse_x, int mouse_y);
void backdrop_draw(float alpha);
bool backdrop_vault_is_open(void);         /* the vault was filled (latched) */
float backdrop_vault_progress(void);       /* 0..1, how close the vault is to opening */
void backdrop_vault_open_now(void);        /* open it without filling it (--all) */
void backdrop_vault_mouth(int *x, int *y); /* screen position just above the vault's open top (smoke test) */
/* Draws the vault's lock + caption on top of the sand (menu only). `archive_open` picks the icon state. */
void backdrop_vault_draw(bool archive_open, float t);

/* smoke.c */
int smoke_run(const CliOptions *opt, const char *data_dir, const char *assets_dir);

#endif
