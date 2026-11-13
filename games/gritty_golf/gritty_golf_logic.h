/* Gritty Golf - PURE game logic adapter (no raylib, no drawing, no keyboard). Owner: zaxx-q (upstream author)
 *
 * The rules (physics, levels, save files) live in the vendored engine `gg/` (see gg/VENDORED.md).
 * This file only does four things: translate ArcadeInput into the engine's Input, step the engine,
 * load/save the player profile, and expose the arCade score. Everything is deterministic: the same
 * ArcadeInput sequence always gives the same result. */
#ifndef GRITTY_GOLF_LOGIC_H
#define GRITTY_GOLF_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h" /* Vec2: the engine reuses arCade's (see GG_VEC2_FROM_ARCADE in CMakeLists.txt) */
#include "gg/game.h"      /* the vendored engine: Game, Input, SaveData, ... */

/* ---- Where the 256x256 game canvas sits on the 1280x720 screen ----
 * Only whole-number scales keep the pixel art sharp, and 3x (768 px) would not fit in 720 px, so: 2x. */
#define GG_VIEW_SCALE 2
#define GG_VIEW_SIZE (VIRTUAL_SIZE * GG_VIEW_SCALE) /* 512 */
#define GG_VIEW_X ((ARC_SCREEN_W - GG_VIEW_SIZE) / 2)
#define GG_VIEW_Y ((ARC_SCREEN_H - GG_VIEW_SIZE) / 2)

/* Finishing ends the run and sends the score to the hub, so it needs two presses: the first one "arms" it for
 * this many steps (3 s), the second one inside that window confirms. A stray click does nothing. */
#define GG_FINISH_CONFIRM_FRAMES 180

/* ---- Clickable rectangles: ONE function per rectangle, used by drawing AND by clicking ----
 * (so what you see is exactly what you can click). Canvas rectangles are in 256x256 canvas pixels,
 * panel buttons are in screen pixels. */
typedef struct gritty_golf_Rect {
    float x, y, w, h;
} gritty_golf_Rect;

typedef enum gritty_golf_PanelButton {
    GG_BTN_STAGES, /* open / close the stage select dialog (same as the L key)   */
    GG_BTN_RESTART,
    GG_BTN_ASSIST,
    GG_BTN_SUBMIT, /* finish and send the score to the leaderboard (F key)       */
    GG_BTN_COUNT
} gritty_golf_PanelButton;

gritty_golf_Rect gritty_golf_rect_stage_button(void);  /* canvas: the small "stages" icon        */
gritty_golf_Rect gritty_golf_rect_retry_button(void);  /* canvas: the small "retry" icon         */
gritty_golf_Rect gritty_golf_rect_dialog(void);        /* canvas: the stage select window        */
gritty_golf_Rect gritty_golf_rect_close(void);         /* canvas: the X of the stage select      */
gritty_golf_Rect gritty_golf_rect_stage_cell(int i);   /* canvas: level button i (0..24)         */
gritty_golf_Rect gritty_golf_rect_panel_button(int i); /* screen: button i of the right-hand panel */

Vec2 gritty_golf_to_canvas(float screen_x, float screen_y); /* screen pixels -> canvas pixels       */
bool gritty_golf_in_rect(gritty_golf_Rect r, Vec2 p);
bool gritty_golf_in_canvas(Vec2 canvas_point);

/* ---- State ---- */
typedef struct gritty_golf_State {
    Game game;           /* the engine's controller: level, ball, save data, screens (owns a heap Level) */
    unsigned seed;       /* kept for the contract; the engine has no randomness, so it never changes play */
    bool ready;          /* game.level is allocated: call gritty_golf_shutdown() exactly once            */
    bool finished;       /* the player finished (ending screen + action, or confirmed submit)            */
    int finish_armed;    /* steps left in which a second "finish" press confirms (0 = not armed)         */
    bool mute_toggled;   /* set when M was pressed; the front-end reads and clears it                    */
    int frames;          /* steps taken                                                                  */
    int quiet_frames;    /* frames since the last typed key: debounces OS key repeat                     */
    char save_path[512]; /* "" = never write a file (tests, headless smoke runs)                         */
} gritty_golf_State;

/* A fresh in-memory game on level 0 with a blank "Player" profile. No file is read or written. */
void gritty_golf_init(gritty_golf_State *s, unsigned seed);
/* Frees the heap level and flushes pending progress to disk when a save path is set. */
void gritty_golf_shutdown(gritty_golf_State *s);

/* Loads (or creates) this player's profile `<data_dir>/gritty_save_<player>.dat` and continues at the first
 * level not yet cleared. A damaged file is renamed to *.bad and a fresh profile starts. data_dir NULL or ""
 * keeps the profile in memory. Returns what happened to an existing file (SAVE_NOT_FOUND = new player). */
SaveResult gritty_golf_open_profile(gritty_golf_State *s, const char *player, const char *data_dir);

/* One fixed step (ARC_DT = 1/60 s = one engine tick). `dt` is accepted for the contract: the engine always
 * advances exactly one tick. After gritty_golf_is_over() further calls do nothing. */
void gritty_golf_step(gritty_golf_State *s, const ArcadeInput *in, float dt);
bool gritty_golf_is_over(const gritty_golf_State *s);
/* Engine score of the player's profile: 1000 per cleared level - 40 per best stroke - 2 per best second,
 * never below 0. Higher is better; it only grows when the player clears levels or improves a best. */
int gritty_golf_score(const gritty_golf_State *s);

/* What the submit button and the F key do: the first call arms, a second call within
 * GG_FINISH_CONFIRM_FRAMES steps finishes the run. */
void gritty_golf_request_finish(gritty_golf_State *s);
void gritty_golf_show_splash(gritty_golf_State *s); /* interactive runs only: the 1.8 s title card */

#endif
