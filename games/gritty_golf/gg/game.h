/*
 * game.h  -  the game controller: screens, level flow, scoring, transitions
 *
 * Game_Tick() is called 60 times per second with the player's Input.
 * It contains NO drawing code: render.c / ui.c read the Game struct and
 * draw it.  (This split is what makes the game logic unit-testable.)
 */
#ifndef GG_GAME_H
#define GG_GAME_H

#include "common.h"
#include "level.h"
#include "player.h"
#include "save.h"

/* What the player did during this tick (filled by the input layer). */
typedef struct {
    bool action;               /* aim / lock / shoot button was just pressed */
    bool reset;                /* restart the level                          */
    bool toggle_assist;
    bool open_stage_select;
    bool close_stage_select;
    int  pick_stage;           /* 0..24 when a stage button was clicked, else -1 */
} Input;

typedef struct {
    TransitionPhase phase;
    float time;                /* seconds since the phase started            */
    int   target;              /* level to load when covered (25 = ending)   */
} Transition;

/* Diamond-wave transition tuning */
#define TR_CELL        32      /* grid spacing in pixels                     */
#define TR_GRID        9       /* 9 x 9 cells cover 256 + 32 pixels          */
static const float TR_CELL_TIME = 0.25f;    /* seconds for one diamond       */
static const float TR_STAGGER   = 0.015f;   /* delay per diagonal step       */

typedef struct {
    ScreenId   screen;
    Level     *level;                 /* heap object, owned by Game          */
    int        level_idx;
    Player     player;
    Transition tr;
    bool       stage_select_open;
    bool       goal_reached;          /* flag touched, waiting for transition */

    SaveData   save;
    Settings   settings;
    char       save_path[260];
    bool       save_dirty;

    float splash_time;
    float level_time;                 /* seconds spent on this level         */
    float banner_time;                /* drives the logo animation / level name fade */
    float rest_time;                  /* ball still + no strikes: show hint  */
    float bounce_cooldown;
    int   play_ticks;                 /* ticks since the last whole second (exact integer count) */
    int   level_resets;

    Vec2  preview[MAX_PREVIEW_POINTS];/* aim-assist dots                     */
    int   preview_count;

    char  toast[40];                  /* short message, e.g. "NEW BEST!"     */
    float toast_time;
    unsigned last_events;             /* GameEvent flags of the last tick    */
} Game;

bool  Game_Init(Game *g, const Settings *st, const SaveData *sv,
                const char *save_path, int start_level, bool show_splash);
void  Game_Shutdown(Game *g);
void  Game_Tick(Game *g, const Input *in);
bool  Game_LoadLevel(Game *g, int idx);
void  Game_ResetLevel(Game *g, bool play_sound);
void  Game_StartTransition(Game *g, int target);
float Game_TransitionScale(const Game *g, int cell_x, int cell_y);  /* 0..1 */
void  Input_Clear(Input *in);
void  Input_Merge(Input *dst, const Input *src);   /* OR the flags together */

#endif /* GG_GAME_H */
