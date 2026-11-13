#include "gritty_golf_logic.h"

#include <stdio.h>
#include <string.h>

#include "arcade/str.h"
#include "gg/save.h"
#include "gg/util.h"

/* A typed key only counts when nothing was typed during the last few frames. Holding a key makes the OS
 * repeat it every ~2 frames after a short delay; this keeps "hold P" from flickering the aim assist. */
#define TYPED_QUIET_FRAMES 6

/* ================================================================== *
 *  Layout (pure numbers, no drawing)
 * ================================================================== */
gritty_golf_Rect gritty_golf_rect_stage_button(void) {
    gritty_golf_Rect r = {185.0f, 220.0f, 26.0f, 24.0f};
    return r;
}

gritty_golf_Rect gritty_golf_rect_retry_button(void) {
    gritty_golf_Rect r = {216.0f, 220.0f, 27.0f, 24.0f};
    return r;
}

gritty_golf_Rect gritty_golf_rect_dialog(void) {
    gritty_golf_Rect r = {32.0f, 32.0f, 192.0f, 192.0f};
    return r;
}

gritty_golf_Rect gritty_golf_rect_close(void) {
    gritty_golf_Rect r = {202.0f, 36.0f, 16.0f, 16.0f};
    return r;
}

/* 25 level buttons in a 5 x 5 grid: index i -> column i % 5, row i / 5 */
gritty_golf_Rect gritty_golf_rect_stage_cell(int i) {
    const float w = 28.0f, h = 22.0f, pitch_x = 34.0f, pitch_y = 29.0f;
    float x0 = 128.0f - (4.0f * pitch_x + w) * 0.5f;
    float y0 = 66.0f;
    gritty_golf_Rect r;
    r.x = x0 + (float)(i % 5) * pitch_x;
    r.y = y0 + (float)(i / 5) * pitch_y;
    r.w = w;
    r.h = h;
    return r;
}

/* The right-hand side panel holds four big buttons stacked under its statistics. */
gritty_golf_Rect gritty_golf_rect_panel_button(int i) {
    gritty_golf_Rect r = {936.0f, 360.0f + 56.0f * (float)i, 296.0f, 44.0f};
    return r;
}

Vec2 gritty_golf_to_canvas(float screen_x, float screen_y) {
    return vec2((screen_x - (float)GG_VIEW_X) / (float)GG_VIEW_SCALE,
                (screen_y - (float)GG_VIEW_Y) / (float)GG_VIEW_SCALE);
}

bool gritty_golf_in_rect(gritty_golf_Rect r, Vec2 p) {
    return arc_point_in_rect(p, r.x, r.y, r.w, r.h);
}

bool gritty_golf_in_canvas(Vec2 p) {
    return p.x >= 0.0f && p.y >= 0.0f && p.x < (float)VIRTUAL_SIZE && p.y < (float)VIRTUAL_SIZE;
}

/* ================================================================== *
 *  Life cycle
 * ================================================================== */
void gritty_golf_init(gritty_golf_State *s, unsigned seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed;
    s->quiet_frames = TYPED_QUIET_FRAMES; /* the very first key press must count */

    Settings settings;
    Settings_Default(&settings);
    SaveData save;
    Save_Init(&save, "Player");
    /* An empty save path tells the engine never to touch the disk (see gg/VENDORED.md, patch 2). */
    s->ready = Game_Init(&s->game, &settings, &save, "", 0, false);
}

void gritty_golf_shutdown(gritty_golf_State *s) {
    if (s->ready)
        Game_Shutdown(&s->game); /* frees the level; writes the save only if a path is set */
    s->ready = false;
}

/* "<dir>/<file>", adding the slash only when it is missing. Both separators are accepted (Windows paths). */
static void join_path(char *out, size_t cap, const char *dir, const char *file) {
    size_t n = strlen(dir);
    bool slash = n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\');
    snprintf(out, cap, "%s%s%s", dir, (n > 0 && !slash) ? "/" : "", file);
}

SaveResult gritty_golf_open_profile(gritty_golf_State *s, const char *player, const char *data_dir) {
    char path[512] = "";
    SaveData save;
    SaveResult result = SAVE_NOT_FOUND;

    if (data_dir != NULL && data_dir[0] != '\0') {
        char file[128];
        Save_PathFor(player, file, sizeof(file)); /* "Andi Putra" -> "gritty_save_andi_putra.dat" */
        join_path(path, sizeof(path), data_dir, file);
        result = Save_Load(&save, path);
        if (result == SAVE_CORRUPT || result == SAVE_BAD_VERSION) {
            char bad[560];
            snprintf(bad, sizeof(bad), "%s.bad", path);
            remove(bad);
            rename(path, bad); /* keep the damaged file instead of silently deleting it */
        }
    }
    if (result != SAVE_OK)
        Save_Init(&save, player);
    else
        Str_Copy(save.name, sizeof(save.name), player); /* the name may differ in letter case */

    /* Restart the engine on this profile. Settings (aim assist) carry over from the old game. */
    Settings settings = s->game.settings;
    unsigned seed = s->seed;
    int frames = s->frames;
    gritty_golf_shutdown(s);
    memset(s, 0, sizeof(*s));
    s->seed = seed;
    s->frames = frames;
    s->quiet_frames = TYPED_QUIET_FRAMES;
    arc_strlcpy(s->save_path, path, sizeof(s->save_path));

    int start = Save_FirstUncleared(&save); /* continue where the player stopped */
    if (start >= LEVEL_COUNT)
        start = LEVEL_COUNT - 1; /* everything cleared: show the last level, stage select reaches the rest */
    s->ready = Game_Init(&s->game, &settings, &save, path, start, false);
    return result;
}

void gritty_golf_show_splash(gritty_golf_State *s) {
    if (s->ready)
        s->game.screen = SCREEN_SPLASH;
}

void gritty_golf_request_finish(gritty_golf_State *s) {
    if (s->finish_armed > 0)
        s->finished = true; /* second press in time: confirmed */
    else
        s->finish_armed = GG_FINISH_CONFIRM_FRAMES;
}

/* ================================================================== *
 *  Input: ArcadeInput -> engine Input
 * ================================================================== */
static char lower_key(int c) {
    return (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);
}

/* The typed key of this frame (lower case) or 0. Updates the debounce counter. */
static char take_key(gritty_golf_State *s, const ArcadeInput *in) {
    char key = 0;
    if (in->typed != 0) {
        if (s->quiet_frames >= TYPED_QUIET_FRAMES)
            key = lower_key(in->typed);
        s->quiet_frames = 0;
    } else if (s->quiet_frames < TYPED_QUIET_FRAMES) {
        s->quiet_frames++;
    }
    return key;
}

/* Which side panel button is under the pointer? -1 for none. */
static int panel_button_at(float screen_x, float screen_y) {
    Vec2 p = vec2(screen_x, screen_y);
    for (int i = 0; i < GG_BTN_COUNT; i++)
        if (gritty_golf_in_rect(gritty_golf_rect_panel_button(i), p))
            return i;
    return -1;
}

/* Fills `out` with what the engine should do this tick. Mirrors the upstream UI_PollInput (gg/ui.c), with
 * the keys arCade can deliver: Space/Enter/click = action, R = restart, L = stages, P = aim assist,
 * F = finish, M = mute. ESC never reaches the game: the arCade loop uses it to leave to the hub. */
static void translate_input(gritty_golf_State *s, const ArcadeInput *in, Input *out) {
    const Game *g = &s->game;
    char key = take_key(s, in);
    bool click = in->fire_pressed;
    Vec2 m = gritty_golf_to_canvas(in->aim_x, in->aim_y);
    int btn = click ? panel_button_at(in->aim_x, in->aim_y) : -1;

    Input_Clear(out);
    if (key == 'm')
        s->mute_toggled = true;
    if (key == 'f' || btn == GG_BTN_SUBMIT)
        gritty_golf_request_finish(s);
    if (key == 'p' || btn == GG_BTN_ASSIST)
        out->toggle_assist = true;

    if (g->stage_select_open) {
        if (key == 'l' || btn == GG_BTN_STAGES)
            out->close_stage_select = true;
        if (click && btn < 0) {
            if (gritty_golf_in_rect(gritty_golf_rect_close(), m)) {
                out->close_stage_select = true;
            } else {
                for (int i = 0; i < LEVEL_COUNT; i++) {
                    if (gritty_golf_in_rect(gritty_golf_rect_stage_cell(i), m)) {
                        out->pick_stage = i;
                        break;
                    }
                }
            }
        }
        return; /* the world is paused behind the dialog: game keys are ignored */
    }

    if (key == 'l' || btn == GG_BTN_STAGES)
        out->open_stage_select = true;
    if (key == 'r' || btn == GG_BTN_RESTART)
        out->reset = true;
    if (in->action_pressed)
        out->action = true;
    if (click && btn < 0) {
        if (gritty_golf_in_rect(gritty_golf_rect_stage_button(), m))
            out->open_stage_select = true;
        else if (gritty_golf_in_rect(gritty_golf_rect_retry_button(), m))
            out->reset = true;
        else if (gritty_golf_in_canvas(m))
            out->action = true; /* a click on the canvas is the action; a click on the panels is not */
    }
}

/* ================================================================== *
 *  One step
 * ================================================================== */
void gritty_golf_step(gritty_golf_State *s, const ArcadeInput *in, float dt) {
    (void)dt; /* the engine is fixed-step: one call = one 1/60 s tick, exactly like ARC_DT */
    if (!s->ready || s->finished)
        return;

    Input cmd;
    translate_input(s, in, &cmd);

    /* On the ending screen the upstream "play again" becomes "finish": the run is over and the score goes to
     * the hub. The engine never sees this action, so it never restarts the campaign by itself. */
    if (s->game.screen == SCREEN_ENDING) {
        if (cmd.action && s->game.tr.phase == TR_NONE)
            s->finished = true; /* the ending screen is its own confirmation: no second press needed */
        cmd.action = false;
    }

    Game_Tick(&s->game, &cmd);
    s->frames++;
    if (s->finish_armed > 0)
        s->finish_armed--;
}

bool gritty_golf_is_over(const gritty_golf_State *s) {
    return s->finished;
}

int gritty_golf_score(const gritty_golf_State *s) {
    return Save_Score(&s->game.save);
}
