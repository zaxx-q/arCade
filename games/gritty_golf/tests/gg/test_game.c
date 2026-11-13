/* test_game.c - screens, events, sounds, goal/transition flow, stage select, autosave */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "test_framework.h"
#include "audio_stub.h"
#include "game.h"
#include "physics.h"
#include "save.h"
#include "records.h"
#include "vec2.h"

/* ---- helpers ------------------------------------------------------------ */
static void new_game(Game *g, int level, bool splash)
{
    Settings st;
    SaveData sv;
    Settings_Default(&st);
    Save_Init(&sv, "Tester");
    TmpRemove("game_save");
    AudioStub_Reset();
    CHECK(Game_Init(g, &st, &sv, TmpPath("game_save"), level, splash));
}

static void tick(Game *g, bool action, bool reset)
{
    Input in;
    Input_Clear(&in);
    in.action = action;
    in.reset  = reset;
    Game_Tick(g, &in);
}

static void run(Game *g, int ticks)
{
    for (int i = 0; i < ticks; i++)
        tick(g, false, false);
}

static void teleport(Game *g, Vec2 p)
{
    g->player.ball.pos = p;
    g->player.ball.vel = V2(0, 0);
}

/* run until the running transition has completely finished (bounded) */
static void finish_transition(Game *g)
{
    for (int i = 0; i < 300 && g->tr.phase != TR_NONE; i++)
        tick(g, false, false);
}

/* a point that is certainly INSIDE polygon `p` (scan its bounding box) */
static Vec2 point_inside(const PolygonData *p)
{
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    for (int i = 0; i < p->point_count; i++) {
        if (p->points[i].x < x0) x0 = p->points[i].x;
        if (p->points[i].x > x1) x1 = p->points[i].x;
        if (p->points[i].y < y0) y0 = p->points[i].y;
        if (p->points[i].y > y1) y1 = p->points[i].y;
    }
    for (float y = y0; y <= y1; y += 2.0f)
        for (float x = x0; x <= x1; x += 2.0f)
            if (Physics_PointInPolygon(V2(x, y), p->points, p->point_count))
                return V2(x, y);
    return V2((x0 + x1) / 2, (y0 + y1) / 2);
}

/* ---- tests ----------------------------------------------------------------- */
static void test_init(void)
{
    Game g;
    new_game(&g, 5, false);
    CHECK_INT(g.level_idx, 5);
    CHECK_INT(g.screen, SCREEN_PLAY);
    CHECK(g.level != NULL);
    CHECK_STR(g.save.name, "Tester");
    CHECK_INT(g.player.strikes, 3);
    CHECK_NEAR(g.player.ball.pos.x, g_levels[5].player_spawn.x, 1e-6);
    Game_Shutdown(&g);

    new_game(&g, 99, true);                                   /* invalid start level -> level 0 */
    CHECK_INT(g.level_idx, 0);
    CHECK_INT(g.screen, SCREEN_SPLASH);
    Game_Shutdown(&g);

    new_game(&g, -4, false);
    CHECK_INT(g.level_idx, 0);
    Game_Shutdown(&g);

    Settings st; SaveData sv;                                 /* NULL path -> default save path */
    Settings_Default(&st); Save_Init(&sv, "X");
    CHECK(Game_Init(&g, &st, &sv, NULL, 0, false));
    CHECK_STR(g.save_path, SAVE_PATH);
    Game_Shutdown(&g);
}

static void test_splash(void)
{
    Game g;
    new_game(&g, 0, true);
    run(&g, 100);
    CHECK_INT(g.screen, SCREEN_SPLASH);                       /* 1.8 s = 108 ticks */
    run(&g, 12);
    CHECK_INT(g.screen, SCREEN_PLAY);
    Game_Shutdown(&g);

    new_game(&g, 0, true);
    tick(&g, true, false);                                    /* any press skips it */
    CHECK_INT(g.screen, SCREEN_PLAY);
    Game_Shutdown(&g);
}

static void test_load_level(void)
{
    Game g;
    new_game(&g, 3, false);
    CHECK(!Game_LoadLevel(&g, 25));                           /* invalid: refused, old level kept */
    CHECK(!Game_LoadLevel(&g, -1));
    CHECK_INT(g.level_idx, 3);
    CHECK(g.level != NULL);
    CHECK(Game_LoadLevel(&g, 9));
    CHECK_INT(g.level_idx, 9);
    CHECK_NEAR(g.player.ball.pos.x, g_levels[9].player_spawn.x, 1e-6);
    CHECK_NEAR(g.level_time, 0.0, 0);
    Game_Shutdown(&g);
}

static void test_input_helpers(void)
{
    Input a, b;
    Input_Clear(&a);
    CHECK(!a.action && !a.reset && !a.toggle_assist && !a.open_stage_select && !a.close_stage_select);
    CHECK_INT(a.pick_stage, -1);

    Input_Clear(&b);
    b.action = true; b.pick_stage = 7;
    Input_Merge(&a, &b);
    CHECK(a.action);
    CHECK_INT(a.pick_stage, 7);
    Input c; Input_Clear(&c);
    c.reset = true;
    Input_Merge(&a, &c);                                      /* flags accumulate, pick stays */
    CHECK(a.action && a.reset);
    CHECK_INT(a.pick_stage, 7);
}

static void test_sounds_for_a_shot(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 30);                                              /* let the ball settle */
    AudioStub_Reset();

    tick(&g, true, false);                                    /* aim */
    CHECK_INT(g.player.state, BALL_STATE_AIMING);
    CHECK_INT(g_sfx_count[SFX_POWER], 1);
    CHECK_INT(g_sfx_pitched[SFX_POWER], 1);                   /* random pitch */
    run(&g, 5);
    tick(&g, true, false);                                    /* lock */
    CHECK_INT(g_sfx_count[SFX_POWER], 2);
    run(&g, 5);
    tick(&g, true, false);                                    /* shoot */
    CHECK_INT(g_sfx_count[SFX_POWER], 3);
    CHECK_INT(g_sfx_count[SFX_BALL_HIT], 1);
    CHECK_INT(g.player.strikes, 2);
    CHECK_INT(g.player.state, BALL_STATE_MOVING);

    AudioStub_Reset();                                        /* power meter hitting 100 -> sound 2 */
    tick(&g, true, false); run(&g, 2); tick(&g, true, false);
    run(&g, 40);                                              /* 40 ticks * 3.33 = 133: reached the top */
    CHECK(g_sfx_count[SFX_POWER_2] >= 1);
    Game_Shutdown(&g);
}

static void test_reset_input(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 20);
    tick(&g, true, false); run(&g, 2); tick(&g, true, false); run(&g, 2); tick(&g, true, false);
    CHECK_INT(g.player.strikes, 2);
    g.level->strike_count = g.level->strike_count;            /* (no pickups on level 0) */

    AudioStub_Reset();
    tick(&g, false, true);                                    /* press R */
    CHECK_INT(g.player.strikes, 3);
    CHECK_INT(g.player.strokes, 0);
    CHECK_INT(g.level_resets, 1);
    CHECK_INT(g_sfx_count[SFX_BALL_OOB], 1);
    CHECK_NEAR(g.player.ball.pos.x, g_levels[0].player_spawn.x, 3.0);
    Game_Shutdown(&g);
}

static void test_out_of_bounds_resets(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 10);
    AudioStub_Reset();
    teleport(&g, V2(-500, 100));
    tick(&g, false, false);
    CHECK_INT(g.level_resets, 1);
    CHECK_INT(g_sfx_count[SFX_BALL_OOB], 1);
    CHECK(g.player.ball.pos.x > 0.0f);                        /* back at the spawn */
    Game_Shutdown(&g);
}

static void test_hazard_resets(void)
{
    int lvl = -1;
    for (int i = 0; i < LEVEL_COUNT && lvl < 0; i++)
        if (g_levels[i].death_polyland_count > 0) lvl = i;
    CHECK(lvl >= 0);

    Game g;
    new_game(&g, lvl, false);
    AudioStub_Reset();
    teleport(&g, point_inside(&g_levels[lvl].death_polylands[0]));
    tick(&g, false, false);
    CHECK_INT(g.level_resets, 1);
    CHECK(g.last_events == EVT_NONE || (g.last_events & EVT_HAZARD));
    CHECK_INT(g_sfx_count[SFX_BALL_OOB], 1);
    CHECK_NEAR(g.player.ball.pos.x, g_levels[lvl].player_spawn.x, 3.0);
    Game_Shutdown(&g);
}

static void test_goal_flow_and_autosave(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 10);
    AudioStub_Reset();

    teleport(&g, g.level->def->flagpole_pos);
    tick(&g, false, false);
    CHECK(g.goal_reached);
    CHECK_INT(g.tr.phase, TR_COVER);
    CHECK_INT(g.tr.target, 1);
    CHECK_INT(g_sfx_count[SFX_LEVEL_BEAT], 1);
    CHECK(Save_IsCleared(&g.save, 0));
    CHECK_INT(g.save.best_strokes[0], 1);                     /* 0 shots counts as 1 */
    CHECK(strstr(g.toast, "NEW BEST") != NULL);
    CHECK(g.toast_time > 0.0f);

    SaveData onDisk;                                          /* autosaved immediately */
    CHECK_INT(Save_Load(&onDisk, TmpPath("game_save")), SAVE_OK);
    CHECK(Save_IsCleared(&onDisk, 0));

    run(&g, 10);                                              /* standing on the flag: only ONE win */
    CHECK_INT(g_sfx_count[SFX_LEVEL_BEAT], 1);

    float t = g.level_time;                                   /* the clock stopped at the flag */
    run(&g, 5);
    CHECK_NEAR(g.level_time, t, 1e-6);

    Input in; Input_Clear(&in); in.action = true;             /* input is ignored while winning */
    BallState before = g.player.state;
    Game_Tick(&g, &in);
    CHECK_INT(g.player.state, before);
    CHECK_INT(g.tr.phase, TR_COVER);                          /* (still covering: ~30 ticks in total) */

    finish_transition(&g);
    CHECK_INT(g.level_idx, 1);                                /* next level loaded */
    CHECK(!g.goal_reached);
    CHECK_INT(g.screen, SCREEN_PLAY);
    CHECK_INT(g.tr.phase, TR_NONE);
    CHECK(g.level_time < 0.7f);                               /* the clock restarted with the new level */

    /* a worse run on level 0 must not overwrite the best one */
    Game_LoadLevel(&g, 0);
    g.player.strokes = 3;
    g.level_time = 50.0f;
    teleport(&g, g.level->def->flagpole_pos);
    tick(&g, false, false);
    CHECK(strstr(g.toast, "CLEARED: 3") != NULL);
    CHECK_INT(g.save.best_strokes[0], 1);
    Game_Shutdown(&g);
    TmpRemove("game_save");
}

static void test_ending_and_replay(void)
{
    Game g;
    new_game(&g, 24, false);
    teleport(&g, g.level->def->flagpole_pos);
    tick(&g, false, false);
    CHECK_INT(g.tr.target, 25);
    finish_transition(&g);
    CHECK_INT(g.screen, SCREEN_ENDING);

    tick(&g, true, false);                                    /* press: back to level 0 */
    CHECK_INT(g.tr.phase, TR_COVER);
    CHECK_INT(g.tr.target, 0);
    finish_transition(&g);
    CHECK_INT(g.screen, SCREEN_PLAY);
    CHECK_INT(g.level_idx, 0);
    Game_Shutdown(&g);
    TmpRemove("game_save");
}

static void test_stage_select(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 30);

    Input in;
    Input_Clear(&in); in.open_stage_select = true;
    Game_Tick(&g, &in);
    CHECK(g.stage_select_open);

    Vec2 where = g.player.ball.pos;                           /* the world is paused */
    float clock = g.level_time;
    run(&g, 30);
    CHECK_NEAR(g.player.ball.pos.y, where.y, 1e-6);
    CHECK_NEAR(g.level_time, clock, 1e-6);

    Input_Clear(&in); in.pick_stage = 25;                     /* out of range: ignored */
    Game_Tick(&g, &in);
    CHECK(g.stage_select_open);
    CHECK_INT(g.tr.phase, TR_NONE);

    Input_Clear(&in); in.close_stage_select = true;
    Game_Tick(&g, &in);
    CHECK(!g.stage_select_open);

    Input_Clear(&in); in.open_stage_select = true; Game_Tick(&g, &in);
    Input_Clear(&in); in.pick_stage = 7;                      /* choose level 7 */
    Game_Tick(&g, &in);
    CHECK(!g.stage_select_open);
    CHECK_INT(g.tr.phase, TR_COVER);
    CHECK_INT(g.tr.target, 7);

    Input_Clear(&in); in.open_stage_select = true;            /* cannot reopen mid-transition */
    Game_Tick(&g, &in);
    CHECK(!g.stage_select_open);

    finish_transition(&g);
    CHECK_INT(g.level_idx, 7);
    Game_Shutdown(&g);
}

static void test_transition_scale(void)
{
    Game g;
    new_game(&g, 0, false);
    CHECK_NEAR(Game_TransitionScale(&g, 3, 3), 0.0, 0);       /* no transition: invisible */

    Game_StartTransition(&g, 2);
    CHECK_INT(g.tr.phase, TR_COVER);
    CHECK_NEAR(Game_TransitionScale(&g, 0, 0), 0.0, 1e-6);    /* just started: nothing visible */
    Game_StartTransition(&g, 9);                              /* ignored: one at a time */
    CHECK_INT(g.tr.target, 2);

    float prev = 0.0f;
    bool diagonal_checked = false;
    for (int i = 0; i < 300 && g.tr.phase == TR_COVER; i++) {
        tick(&g, false, false);
        if (g.tr.phase != TR_COVER) break;
        float s = Game_TransitionScale(&g, 0, 0);
        CHECK(s >= prev - 1e-6f);                             /* grows monotonically */
        CHECK(s >= 0.0f && s <= 1.0f);
        prev = s;
        float far = Game_TransitionScale(&g, 8, 8);
        CHECK(s >= far - 1e-6f);                              /* wave starts top-left, reaches bottom-right last */
        if (s > 0.2f && far < 0.9f) diagonal_checked = true;
    }
    CHECK(diagonal_checked);
    CHECK_INT(g.tr.phase, TR_REVEAL);                         /* cover finished: level swapped */
    CHECK_INT(g.level_idx, 2);
    CHECK_NEAR(Game_TransitionScale(&g, 5, 5), 1.0, 1e-6);    /* fully covered at the start of the reveal */

    finish_transition(&g);
    CHECK_INT(g.tr.phase, TR_NONE);
    CHECK_NEAR(Game_TransitionScale(&g, 5, 5), 0.0, 0);
    Game_Shutdown(&g);
}

static void test_aim_assist(void)
{
    Game g;
    new_game(&g, 3, false);
    run(&g, 30);
    CHECK(!g.settings.aim_assist);

    Input in; Input_Clear(&in); in.toggle_assist = true;
    Game_Tick(&g, &in);
    CHECK(g.settings.aim_assist);
    CHECK_INT(g.preview_count, 0);                            /* nothing while the ball moves */

    tick(&g, true, false);                                    /* AIMING */
    run(&g, 10);
    CHECK(g.preview_count > 0);
    CHECK(g.preview_count <= MAX_PREVIEW_POINTS);

    tick(&g, true, false);                                    /* POWER: preview follows the meter */
    int n1 = g.preview_count;
    run(&g, 20);
    CHECK(g.preview_count > 0);
    (void)n1;

    Input_Clear(&in); in.toggle_assist = true;                /* off again */
    Game_Tick(&g, &in);
    CHECK(!g.settings.aim_assist);
    CHECK_INT(g.preview_count, 0);
    Game_Shutdown(&g);
}

static void test_pickup_refills_strikes(void)
{
    int lvl = -1;
    for (int i = 0; i < LEVEL_COUNT && lvl < 0; i++)
        if (g_levels[i].strike_count > 0) lvl = i;
    CHECK(lvl >= 0);

    Game g;
    new_game(&g, lvl, false);
    g.player.strikes = 0;
    AudioStub_Reset();
    teleport(&g, g.level->def->strikes[0]);
    tick(&g, false, false);
    CHECK_INT(g.player.strikes, 3);
    CHECK_INT(g_sfx_count[SFX_POWER], 1);
    CHECK_INT(g_sfx_pitched[SFX_POWER], 0);                   /* pickup uses the exact pitch */
    CHECK(!g.level->strike_active[0]);

    tick(&g, false, true);                                    /* a reset puts the pickup back */
    CHECK(g.level->strike_active[0]);
    Game_Shutdown(&g);
}

static void test_button_opens_wall(void)
{
    int lvl = -1, btn = -1;
    for (int i = 0; i < LEVEL_COUNT && lvl < 0; i++)
        for (int b = 0; b < g_levels[i].button_count; b++) {
            int t = g_levels[i].buttons[b].target_wall_index;
            if (t >= 0 && g_levels[i].walls[t].play_hit_sfx) { lvl = i; btn = b; break; }
        }
    CHECK(lvl >= 0);

    Game g;
    new_game(&g, lvl, false);
    AudioStub_Reset();
    teleport(&g, g.level->def->buttons[btn].position);
    tick(&g, false, false);
    int t = g.level->def->buttons[btn].target_wall_index;
    CHECK(g.level->buttons[btn].pressed);
    CHECK(g.level->walls[t].opening);
    CHECK_INT(g_sfx_count[SFX_BALL_HIT], 1);
    CHECK_INT(g_sfx_pitched[SFX_BALL_HIT], 0);                /* gate sound: exact pitch */

    teleport(&g, g.level->def->player_spawn);                 /* let the gate finish moving */
    run(&g, 30);
    CHECK_NEAR(g.level->walls[t].offset.y, g.level->def->walls[t].open_displacement.y, 1e-2);
    Game_Shutdown(&g);
}

static void test_out_of_strikes_hint(void)
{
    Game g;
    new_game(&g, 0, false);
    g.player.strikes = 0;
    run(&g, 20);
    CHECK_NEAR(g.rest_time, 0.0, 0.6);                        /* still settling */
    run(&g, 120);
    CHECK(g.rest_time > 0.8f);                                /* standing still, no strikes: show the hint */
    g.player.strikes = 2;
    run(&g, 2);
    CHECK_NEAR(g.rest_time, 0.0, 0);
    Game_Shutdown(&g);
}

static void test_timers_and_play_time(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 60);
    CHECK_NEAR(g.level_time, 1.0, 0.05);
    CHECK_INT(g.save.play_seconds, 1);                        /* one whole second played */
    CHECK(g.save_dirty);
    run(&g, 60);
    CHECK_INT(g.save.play_seconds, 2);
    CHECK(g.banner_time > 1.9f);
    Game_Shutdown(&g);
    TmpRemove("game_save");
}

static void test_shutdown_flushes_save(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 125);                                             /* 2 seconds -> dirty */
    CHECK(g.save_dirty);
    Game_Shutdown(&g);
    CHECK(g.level == NULL);                                   /* freed and cleared */

    SaveData sv;
    CHECK_INT(Save_Load(&sv, TmpPath("game_save")), SAVE_OK);
    CHECK_INT(sv.play_seconds, 2);
    TmpRemove("game_save");
}

static void test_events_recorded(void)
{
    Game g;
    new_game(&g, 0, false);
    run(&g, 5);
    tick(&g, true, false);
    CHECK(g.last_events & EVT_AIM_START);
    tick(&g, false, false);
    CHECK_INT(g.last_events & EVT_AIM_START, 0);              /* flags describe ONE tick only */
    Game_Shutdown(&g);
}

void run_game_tests(void)
{
    RUN(test_init);
    RUN(test_splash);
    RUN(test_load_level);
    RUN(test_input_helpers);
    RUN(test_sounds_for_a_shot);
    RUN(test_reset_input);
    RUN(test_out_of_bounds_resets);
    RUN(test_hazard_resets);
    RUN(test_goal_flow_and_autosave);
    RUN(test_ending_and_replay);
    RUN(test_stage_select);
    RUN(test_transition_scale);
    RUN(test_aim_assist);
    RUN(test_pickup_refills_strikes);
    RUN(test_button_opens_wall);
    RUN(test_out_of_strikes_hint);
    RUN(test_timers_and_play_time);
    RUN(test_shutdown_flushes_save);
    RUN(test_events_recorded);
}
