/*
 * game.c  -  game controller (no drawing, no raylib)
 */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "audio.h"
#include "physics.h"
#include "util.h"
#include "vec2.h"

static const float SPLASH_SECONDS = 1.8f;

/* [arCade patch 2] An empty save_path means "do not touch the disk" (headless smoke runs, tests).
 * arCade games may only write under ctx->data_dir, and a smoke run must leave no files behind. */
static void write_save(Game *g)
{
    if (g->save_path[0] != '\0')
        Save_Write(&g->save, g->save_path);
}

/* total length of one transition phase: the LAST diagonal cell finishes at
 *   stagger * (2*(GRID-1)) + cell_time                                        */
static float transition_total(void)
{
    return TR_STAGGER * (float)(2 * (TR_GRID - 1)) + TR_CELL_TIME;
}

void Input_Clear(Input *in)
{
    memset(in, 0, sizeof *in);
    in->pick_stage = -1;                 /* -1 = "nothing picked" */
}

/* Several frames can pass between two physics ticks (and vice versa), so
 * button presses are collected here and consumed by the next tick.            */
void Input_Merge(Input *dst, const Input *src)
{
    dst->action             |= src->action;            /* [T04] |= combines flags */
    dst->reset              |= src->reset;
    dst->toggle_assist      |= src->toggle_assist;
    dst->open_stage_select  |= src->open_stage_select;
    dst->close_stage_select |= src->close_stage_select;
    if (src->pick_stage >= 0)
        dst->pick_stage = src->pick_stage;
}

static void show_toast(Game *g, const char *text)
{
    Str_Copy(g->toast, sizeof g->toast, text);
    g->toast_time = 2.0f;
}

/* ================================================================== *
 *  Setup / teardown
 * ================================================================== */
bool Game_Init(Game *g, const Settings *st, const SaveData *sv,
               const char *save_path, int start_level, bool show_splash)
{
    memset(g, 0, sizeof *g);
    g->settings = *st;                    /* struct assignment copies every field */
    g->save     = *sv;
    Str_Copy(g->save_path, sizeof g->save_path, save_path != NULL ? save_path : SAVE_PATH);

    if (start_level < 0 || start_level >= LEVEL_COUNT)
        start_level = 0;
    if (!Game_LoadLevel(g, start_level))
        return false;

    g->screen = show_splash ? SCREEN_SPLASH : SCREEN_PLAY;
    return true;
}

void Game_Shutdown(Game *g)
{
    if (g->save_dirty)
        write_save(g);                             /* flush progress to disk */
    Level_Destroy(g->level);
    g->level = NULL;
}

bool Game_LoadLevel(Game *g, int idx)
{
    Level *fresh = Level_Create(idx);
    if (fresh == NULL)
        return false;

    Level_Destroy(g->level);                       /* free the OLD level first */
    g->level     = fresh;
    g->level_idx = idx;
    Player_Init(&g->player, fresh->def->player_spawn);

    g->screen            = SCREEN_PLAY;
    g->goal_reached      = false;
    g->stage_select_open = false;
    g->level_time        = 0.0f;
    g->banner_time       = 0.0f;
    g->rest_time         = 0.0f;
    g->level_resets      = 0;
    g->preview_count     = 0;
    return true;
}

/* Reset = back to the spawn with 3 strikes, level objects restored. */
void Game_ResetLevel(Game *g, bool play_sound)
{
    Player_Reset(&g->player);
    Level_Reset(g->level);
    g->level_resets++;
    g->rest_time     = 0.0f;
    g->preview_count = 0;
    if (play_sound)
        Audio_PlaySfx(SFX_BALL_OOB, true);
}

/* ================================================================== *
 *  Diamond-wave transition
 * ================================================================== */
void Game_StartTransition(Game *g, int target)
{
    if (g->tr.phase != TR_NONE)
        return;                                    /* one at a time */
    g->tr.phase  = TR_COVER;
    g->tr.time   = 0.0f;
    g->tr.target = target;
}

/*
 * Scale (0..1) of the diamond at grid cell (cx, cy).
 * Cells on the same diagonal (cx + cy equal) start at the same moment, so
 * the wave sweeps from the top-left to the bottom-right corner.
 */
float Game_TransitionScale(const Game *g, int cx, int cy)
{
    if (g->tr.phase == TR_NONE)
        return 0.0f;
    float delay = TR_STAGGER * (float)(cx + cy);
    float p = Clampf((g->tr.time - delay) / TR_CELL_TIME, 0.0f, 1.0f);
    float e = p * p * p;                           /* cubic ease-in */
    return (g->tr.phase == TR_COVER) ? e : 1.0f - e;
}

static void update_transition(Game *g)
{
    if (g->tr.phase == TR_NONE)
        return;

    g->tr.time += PHYSICS_DT;
    if (g->tr.time < transition_total())
        return;

    if (g->tr.phase == TR_COVER) {                 /* screen fully hidden: swap */
        if (g->tr.target >= LEVEL_COUNT)
            g->screen = SCREEN_ENDING;
        else
            Game_LoadLevel(g, g->tr.target);
        g->tr.phase = TR_REVEAL;
        g->tr.time  = 0.0f;
    } else {
        g->tr.phase = TR_NONE;
    }
}

/* ================================================================== *
 *  Level finished
 * ================================================================== */
static void on_goal(Game *g)
{
    g->goal_reached = true;
    g->player.ball.vel = V2_Scale(g->player.ball.vel, 0.1f);     /* ball slows to 1/10 */
    Audio_PlaySfx(SFX_LEVEL_BEAT, true);

    bool best = Save_RecordClear(&g->save, g->level_idx, g->player.strokes, g->level_time);
    g->save_dirty = true;
    write_save(g);                                 /* autosave right away */
    g->save_dirty = false;

    char msg[40];
    snprintf(msg, sizeof msg, best ? "NEW BEST: %d STROKES" : "CLEARED: %d STROKES",
             g->player.strokes < 1 ? 1 : g->player.strokes);
    show_toast(g, msg);

    Game_StartTransition(g, g->level_idx + 1);     /* 25 means "ending" */
}

/* Translate the event bit mask into sounds and rule changes.
 * Each `if (ev & FLAG)` tests ONE bit of the mask.                          */
static void handle_events(Game *g, unsigned ev)
{
    if (ev & EVT_AIM_START)    Audio_PlaySfx(SFX_POWER, true);
    if (ev & EVT_ANGLE_LOCK)   Audio_PlaySfx(SFX_POWER, true);
    if (ev & EVT_POWER_BOUNCE) Audio_PlaySfx(SFX_POWER_2, true);
    if (ev & EVT_LAUNCH) {
        Audio_PlaySfx(SFX_POWER, true);
        Audio_PlaySfx(SFX_BALL_HIT, true);
    }
    if ((ev & EVT_BOUNCE) && g->bounce_cooldown <= 0.0f) {
        Audio_PlaySfx(SFX_BALL_LANDED, true);
        g->bounce_cooldown = 0.08f;                /* avoid machine-gun sound */
    }
    if (ev & EVT_PICKUP) {
        Player_RefillStrikes(&g->player);
        Audio_PlaySfx(SFX_POWER, false);           /* pickup: exact pitch */
    }
    if (ev & EVT_WALL_OPEN)
        Audio_PlaySfx(SFX_BALL_HIT, false);        /* gate: exact pitch   */

    /* priority: dying beats winning on the same tick */
    if (ev & (EVT_HAZARD | EVT_OOB)) {
        Game_ResetLevel(g, true);
    } else if ((ev & EVT_GOAL) && !g->goal_reached) {
        on_goal(g);
    }
}

/* ================================================================== *
 *  One PLAY tick
 * ================================================================== */
static void update_aim_assist(Game *g)
{
    bool aiming = g->player.state == BALL_STATE_AIMING ||
                  g->player.state == BALL_STATE_POWER;
    if (!g->settings.aim_assist || !aiming) {
        g->preview_count = 0;
        return;
    }
    Ball probe = g->player.ball;                       /* COPY of the real ball */
    float power = (g->player.state == BALL_STATE_POWER) ? g->player.power : 50.0f;
    probe.vel = Physics_LaunchVelocity(g->player.aim_angle, power);
    g->preview_count = Physics_PredictPath(g->level, probe, MAX_PREVIEW_POINTS * 3,
                                           g->preview, 0, MAX_PREVIEW_POINTS);
}

static void play_tick(Game *g, const Input *in)
{
    const float dt = PHYSICS_DT;
    update_transition(g);

    /* ---- modal stage-select dialog: the world is paused ---- */
    if (g->stage_select_open) {
        if (in->close_stage_select) {
            g->stage_select_open = false;
        } else if (in->pick_stage >= 0 && in->pick_stage < LEVEL_COUNT) {
            g->stage_select_open = false;
            Game_StartTransition(g, in->pick_stage);
        }
        return;
    }
    if (in->open_stage_select && g->tr.phase == TR_NONE) {
        g->stage_select_open = true;
        return;
    }

    if (in->reset && !g->goal_reached)
        Game_ResetLevel(g, true);

    /* ---- world update ---- */
    Level_Update(g->level, dt);

    bool can_act = in->action && !g->goal_reached;
    unsigned ev = Player_Update(&g->player, g->level, can_act, dt);
    if (!g->goal_reached)
        ev |= Level_CheckTriggers(g->level, g->player.ball.pos);
    g->last_events = ev;
    handle_events(g, ev);

    update_aim_assist(g);

    /* ---- timers ---- */
    if (!g->goal_reached)
        g->level_time += dt;
    g->banner_time += dt;
    if (g->bounce_cooldown > 0.0f) g->bounce_cooldown -= dt;
    if (g->toast_time > 0.0f)      g->toast_time -= dt;

    /* count TICKS, not float seconds: adding 1/60 sixty times gives 0.99999...
     * (floating point is approximate), but 60 integer ticks are exactly 1 s.   */
    g->play_ticks++;
    if (g->play_ticks >= PHYSICS_HZ) {
        g->play_ticks -= PHYSICS_HZ;
        g->save.play_seconds++;                    /* whole seconds -> save file */
        g->save_dirty = true;
    }

    /* out of strikes and standing still?  tell the player to restart */
    bool stuck = g->player.strikes == 0 && g->player.state == BALL_STATE_MOVING &&
                 V2_Len(g->player.ball.vel) < REST_VELOCITY;
    g->rest_time = stuck ? g->rest_time + dt : 0.0f;
}

/* ================================================================== *
 *  Game_Tick : the public entry point, 60 calls per second
 *
 *   [T05 Percabangan]  switch on the ScreenId enum
 * ================================================================== */
void Game_Tick(Game *g, const Input *in)
{
    g->last_events = EVT_NONE;
    if (in->toggle_assist)
        g->settings.aim_assist = !g->settings.aim_assist;

    switch (g->screen) {

    case SCREEN_SPLASH:
        g->splash_time += PHYSICS_DT;
        if (in->action || g->splash_time >= SPLASH_SECONDS)
            g->screen = SCREEN_PLAY;
        break;

    case SCREEN_PLAY:
        play_tick(g, in);
        break;

    case SCREEN_ENDING:
        update_transition(g);
        g->banner_time += PHYSICS_DT;
        if (g->toast_time > 0.0f)
            g->toast_time -= PHYSICS_DT;
        if (in->action && g->tr.phase == TR_NONE)
            Game_StartTransition(g, 0);            /* play again from level 0 */
        break;
    }
}
