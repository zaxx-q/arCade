/* Tests for the arCade adapter around the Gritty Golf engine (input mapping, layout, profiles, scoring).
 * The engine itself is covered by tests/gg/ (registered as `gritty_golf.engine`). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arcade_test.h"
#include "audio_stub.h"
#include "gritty_golf_logic.h"
#include "solutions.h"

/* ---- helpers --------------------------------------------------------------------------------------- */
static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static ArcadeInput press_action(void) {
    ArcadeInput in = idle();
    in.action_pressed = true;
    return in;
}

static ArcadeInput press_key(char c) {
    ArcadeInput in = idle();
    in.typed = c;
    return in;
}

/* a left click at a position given in 256x256 canvas pixels */
static ArcadeInput click_canvas(float cx, float cy) {
    ArcadeInput in = idle();
    in.aim_x = (float)GG_VIEW_X + cx * (float)GG_VIEW_SCALE;
    in.aim_y = (float)GG_VIEW_Y + cy * (float)GG_VIEW_SCALE;
    in.fire_pressed = true;
    return in;
}

static ArcadeInput click_screen(float x, float y) {
    ArcadeInput in = idle();
    in.aim_x = x;
    in.aim_y = y;
    in.fire_pressed = true;
    return in;
}

static ArcadeInput click_rect_center(gritty_golf_Rect r) {
    return click_canvas(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
}

static void step_input(gritty_golf_State *s, ArcadeInput in) {
    gritty_golf_step(s, &in, ARC_DT);
}

static void step_idle(gritty_golf_State *s, int n) {
    for (int i = 0; i < n; i++)
        step_input(s, idle());
}

/* Plays the recorded winning run of `level` from the first tick after the level was (re)started.
 * Returns true when the flag was reached. */
static bool replay_level(gritty_golf_State *s, int level) {
    const Solution *sol = NULL;
    for (int i = 0; i < SOLUTION_COUNT; i++)
        if (SOLUTIONS[i].level == level)
            sol = &SOLUTIONS[i];
    if (sol == NULL)
        return false;
    int next = 0;
    int limit = sol->presses[sol->npress - 1] + 700;
    for (int t = 0; t < limit; t++) {
        bool press = next < sol->npress && sol->presses[next] == t;
        if (press)
            next++;
        step_input(s, press ? press_action() : idle());
        if (s->game.goal_reached)
            return true;
    }
    return false;
}

/* ---- layout ---------------------------------------------------------------------------------------- */
static void test_canvas_geometry(void) {
    CHECK(GG_VIEW_X >= 0 && GG_VIEW_Y >= 0);
    CHECK(GG_VIEW_X + GG_VIEW_SIZE <= ARC_SCREEN_W);
    CHECK(GG_VIEW_Y + GG_VIEW_SIZE <= ARC_SCREEN_H);
    CHECK_EQ_INT(GG_VIEW_SIZE, 512); /* integer 2x only: 3x (768 px) does not fit in 720 px */

    Vec2 top_left = gritty_golf_to_canvas((float)GG_VIEW_X, (float)GG_VIEW_Y);
    CHECK_NEAR(top_left.x, 0.0, 0.0001);
    CHECK_NEAR(top_left.y, 0.0, 0.0001);
    Vec2 mid =
        gritty_golf_to_canvas((float)(GG_VIEW_X + GG_VIEW_SIZE / 2), (float)(GG_VIEW_Y + GG_VIEW_SIZE / 2));
    CHECK_NEAR(mid.x, 128.0, 0.0001);
    CHECK_NEAR(mid.y, 128.0, 0.0001);

    CHECK(gritty_golf_in_canvas(vec2(0.0f, 0.0f)));
    CHECK(gritty_golf_in_canvas(vec2(255.9f, 255.9f)));
    CHECK(!gritty_golf_in_canvas(vec2(256.0f, 100.0f)));
    CHECK(!gritty_golf_in_canvas(vec2(-0.1f, 100.0f)));
    CHECK(!gritty_golf_in_canvas(vec2(100.0f, 256.0f)));
}

static bool rects_overlap(gritty_golf_Rect a, gritty_golf_Rect b) {
    return arc_rects_overlap(a.x, a.y, a.w, a.h, b.x, b.y, b.w, b.h);
}

static bool rect_inside(gritty_golf_Rect inner, gritty_golf_Rect outer) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

static void test_canvas_rects_fit(void) {
    gritty_golf_Rect canvas = {0.0f, 0.0f, (float)VIRTUAL_SIZE, (float)VIRTUAL_SIZE};
    gritty_golf_Rect stage = gritty_golf_rect_stage_button();
    gritty_golf_Rect retry = gritty_golf_rect_retry_button();
    gritty_golf_Rect dialog = gritty_golf_rect_dialog();
    CHECK(rect_inside(stage, canvas));
    CHECK(rect_inside(retry, canvas));
    CHECK(rect_inside(dialog, canvas));
    CHECK(!rects_overlap(stage, retry)); /* two icons side by side: a click picks exactly one */
    CHECK(rect_inside(gritty_golf_rect_close(), dialog));

    for (int i = 0; i < LEVEL_COUNT; i++) {
        gritty_golf_Rect a = gritty_golf_rect_stage_cell(i);
        CHECK(rect_inside(a, dialog));
        CHECK(!rects_overlap(a, gritty_golf_rect_close()));
        for (int j = i + 1; j < LEVEL_COUNT; j++)
            CHECK(!rects_overlap(a, gritty_golf_rect_stage_cell(j)));
    }
}

static void test_panel_buttons_fit(void) {
    gritty_golf_Rect canvas = {(float)GG_VIEW_X, (float)GG_VIEW_Y, (float)GG_VIEW_SIZE, (float)GG_VIEW_SIZE};
    gritty_golf_Rect screen = {0.0f, 0.0f, (float)ARC_SCREEN_W, (float)ARC_SCREEN_H};
    for (int i = 0; i < GG_BTN_COUNT; i++) {
        gritty_golf_Rect b = gritty_golf_rect_panel_button(i);
        CHECK(rect_inside(b, screen));
        CHECK(!rects_overlap(b, canvas)); /* a click on a button must never also shoot the ball */
        CHECK(b.x > canvas.x + canvas.w);
        for (int j = i + 1; j < GG_BTN_COUNT; j++)
            CHECK(!rects_overlap(b, gritty_golf_rect_panel_button(j)));
    }
}

/* ---- init and scoring ------------------------------------------------------------------------------ */
static void test_init_is_sane(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK(s.ready);
    CHECK(!gritty_golf_is_over(&s));
    CHECK_EQ_INT(gritty_golf_score(&s), 0);
    CHECK_EQ_INT(s.game.level_idx, 0);
    CHECK_EQ_INT(s.game.screen, SCREEN_PLAY); /* no splash by default: the hub is the title screen */
    CHECK_EQ_INT(s.save_path[0], 0);          /* in memory only */
    gritty_golf_show_splash(&s);
    CHECK_EQ_INT(s.game.screen, SCREEN_SPLASH);
    gritty_golf_shutdown(&s);
    CHECK(!s.ready);
    gritty_golf_shutdown(&s); /* a second call must be harmless */
}

static void test_score_formula(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    Save_RecordClear(&s.game.save, 0, 3, 10.0f);
    Save_RecordClear(&s.game.save, 1, 2, 5.0f);
    CHECK_EQ_INT(gritty_golf_score(&s), 2000 - 40 * 5 - 2 * 15); /* 1000/level - 40/stroke - 2/second */
    Save_RecordClear(&s.game.save, 2, 200, 10.0f); /* absurdly bad run: clamped at 0, never negative */
    Save_RecordClear(&s.game.save, 3, 200, 10.0f);
    CHECK(gritty_golf_score(&s) >= 0);
    CHECK_EQ_INT(gritty_golf_score(&s), 0);
    gritty_golf_shutdown(&s);
}

/* ---- input mapping --------------------------------------------------------------------------------- */
static void test_action_runs_aim_lock_shoot(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    step_input(&s, press_action());
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_AIMING);
    step_idle(&s, 2);
    step_input(&s, press_action());
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_POWER);
    step_idle(&s, 5);
    step_input(&s, press_action());
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    CHECK_EQ_INT(s.game.player.strokes, 1);
    gritty_golf_shutdown(&s);
}

static void test_held_action_is_not_a_press(void) {
    /* `action` is "held", only `action_pressed` is the edge: holding Space must not advance the cycle */
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    for (int i = 0; i < 30; i++) {
        ArcadeInput in = idle();
        in.action = true;
        in.fire = true;
        step_input(&s, in);
    }
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    CHECK_EQ_INT(s.game.player.strokes, 0);
    gritty_golf_shutdown(&s);
}

static void test_click_on_canvas_is_action_but_panels_are_not(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    step_input(&s, click_screen(1000.0f, 200.0f)); /* right panel, not a button */
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    step_input(&s, click_screen(100.0f, 300.0f)); /* left panel */
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    step_input(&s, click_canvas(128.0f, 100.0f)); /* on the canvas */
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_AIMING);
    gritty_golf_shutdown(&s);
}

static void test_canvas_icons(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    step_input(&s, click_rect_center(gritty_golf_rect_stage_button()));
    CHECK(s.game.stage_select_open);
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING); /* the click opened the dialog, it did not shoot */
    step_input(&s, click_rect_center(gritty_golf_rect_close()));
    CHECK(!s.game.stage_select_open);

    int resets = s.game.level_resets;
    step_input(&s, click_rect_center(gritty_golf_rect_retry_button()));
    CHECK_EQ_INT(s.game.level_resets, resets + 1);
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    gritty_golf_shutdown(&s);
}

static void test_keys(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);

    int resets = s.game.level_resets;
    AudioStub_Reset();
    step_input(&s, press_key('r'));
    CHECK_EQ_INT(s.game.level_resets, resets + 1);
    CHECK_EQ_INT(g_sfx_count[SFX_BALL_OOB], 1); /* the engine's sounds still reach the audio seam */
    step_idle(&s, 10);
    step_input(&s, press_key('R')); /* Shift or Caps Lock must not matter */
    CHECK_EQ_INT(s.game.level_resets, resets + 2);

    step_idle(&s, 10);
    CHECK(!s.game.settings.aim_assist);
    step_input(&s, press_key('p'));
    CHECK(s.game.settings.aim_assist);

    step_idle(&s, 10);
    step_input(&s, press_key('l'));
    CHECK(s.game.stage_select_open);
    step_idle(&s, 10);
    step_input(&s, press_key('l'));
    CHECK(!s.game.stage_select_open);

    step_idle(&s, 10);
    CHECK(!s.mute_toggled);
    step_input(&s, press_key('m'));
    CHECK(s.mute_toggled);
    s.mute_toggled = false;

    step_idle(&s, 10);
    step_input(&s, press_key('x')); /* unmapped keys do nothing (Z, X, C were dropped on purpose) */
    step_input(&s, press_key('z'));
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);

    CHECK(!gritty_golf_is_over(&s));
    step_idle(&s, 10);
    step_input(&s, press_key('f')); /* the first press only arms: finishing needs a confirmation */
    CHECK(!gritty_golf_is_over(&s));
    CHECK(s.finish_armed > 0);
    step_idle(&s, 10);
    step_input(&s, press_key('f'));
    CHECK(gritty_golf_is_over(&s));
    gritty_golf_shutdown(&s);
}

static void test_finish_needs_a_confirmation(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    gritty_golf_request_finish(&s);
    CHECK(!gritty_golf_is_over(&s));
    CHECK_EQ_INT(s.finish_armed, GG_FINISH_CONFIRM_FRAMES);

    step_idle(&s, GG_FINISH_CONFIRM_FRAMES); /* the arming runs out: a late second press only re-arms */
    CHECK_EQ_INT(s.finish_armed, 0);
    gritty_golf_request_finish(&s);
    CHECK(!gritty_golf_is_over(&s));

    step_idle(&s, GG_FINISH_CONFIRM_FRAMES - 1); /* still armed on the very last step of the window */
    CHECK_EQ_INT(s.finish_armed, 1);
    gritty_golf_request_finish(&s);
    CHECK(gritty_golf_is_over(&s));
    gritty_golf_shutdown(&s);
}

static void test_typed_key_repeat_is_debounced(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    for (int i = 0; i < 5; i++) /* the OS repeats a held key every frame or two */
        step_input(&s, press_key('p'));
    CHECK(s.game.settings.aim_assist); /* toggled once, not five times */

    step_idle(&s, 3);
    step_input(&s, press_key('p')); /* too soon after the last key: still the same press */
    CHECK(s.game.settings.aim_assist);

    step_idle(&s, 8); /* a real pause: the next key is a new press */
    step_input(&s, press_key('p'));
    CHECK(!s.game.settings.aim_assist);
    gritty_golf_shutdown(&s);
}

static void test_panel_buttons(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    gritty_golf_Rect b = gritty_golf_rect_panel_button(GG_BTN_ASSIST);
    step_input(&s, click_screen(b.x + b.w * 0.5f, b.y + b.h * 0.5f));
    CHECK(s.game.settings.aim_assist);
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);

    b = gritty_golf_rect_panel_button(GG_BTN_STAGES);
    step_input(&s, click_screen(b.x + 5.0f, b.y + 5.0f));
    CHECK(s.game.stage_select_open);
    step_input(&s, click_screen(b.x + 5.0f, b.y + 5.0f)); /* the same button closes it again */
    CHECK(!s.game.stage_select_open);

    int resets = s.game.level_resets;
    b = gritty_golf_rect_panel_button(GG_BTN_RESTART);
    step_input(&s, click_screen(b.x + 5.0f, b.y + 5.0f));
    CHECK_EQ_INT(s.game.level_resets, resets + 1);

    CHECK(!gritty_golf_is_over(&s));
    b = gritty_golf_rect_panel_button(GG_BTN_SUBMIT);
    step_input(&s, click_screen(b.x + 5.0f, b.y + 5.0f));
    CHECK(!gritty_golf_is_over(&s)); /* one misclick must not end the run */
    CHECK(s.finish_armed > 0);
    step_input(&s, click_screen(b.x + 5.0f, b.y + 5.0f));
    CHECK(gritty_golf_is_over(&s));
    gritty_golf_shutdown(&s);
}

static void test_stage_select_dialog(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    step_input(&s, press_key('l'));
    CHECK(s.game.stage_select_open);

    step_input(&s, press_action()); /* game keys are ignored while the dialog is open */
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);
    step_input(&s, click_canvas(5.0f, 5.0f)); /* a click outside every button does nothing either */
    CHECK(s.game.stage_select_open);
    CHECK_EQ_INT(s.game.player.state, BALL_STATE_MOVING);

    step_input(&s, click_rect_center(gritty_golf_rect_stage_cell(7)));
    CHECK(!s.game.stage_select_open);
    CHECK_EQ_INT(s.game.tr.target, 7);
    step_idle(&s, 70); /* the diamond transition covers the screen (30 ticks), swaps the level, reveals it */
    CHECK_EQ_INT(s.game.level_idx, 7);
    CHECK_EQ_INT(s.game.tr.phase, TR_NONE);

    /* every one of the 25 buttons selects its own level */
    for (int i = 0; i < LEVEL_COUNT; i += 6) {
        step_input(&s, press_key('l'));
        step_idle(&s, 10);
        step_input(&s, click_rect_center(gritty_golf_rect_stage_cell(i)));
        step_idle(&s, 70);
        CHECK_EQ_INT(s.game.level_idx, i);
        step_idle(&s, 10);
    }
    gritty_golf_shutdown(&s);
}

/* ---- playing through the adapter ------------------------------------------------------------------- */
static void test_replay_of_level_zero_scores(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK(replay_level(&s, 0));
    CHECK(Save_IsCleared(&s.game.save, 0));
    CHECK_EQ_INT(s.game.save.best_strokes[0], 1);
    int score = gritty_golf_score(&s);
    CHECK(score > 900 && score <= 960); /* 1000 - 40 - 2 * (about 1.5 s) */
    CHECK(!gritty_golf_is_over(&s));    /* clearing a level is not the end of the run */
    gritty_golf_shutdown(&s);
}

static void test_full_campaign_ends_with_the_action_button(void) {
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        CHECK_EQ_INT(s.game.level_idx, lvl);
        CHECK(replay_level(&s, lvl));
        for (int t = 0; t < 400 && s.game.tr.phase != TR_NONE; t++)
            step_idle(&s, 1);
        CHECK_EQ_INT(s.game.tr.phase, TR_NONE);
        if (lvl + 1 < LEVEL_COUNT)
            Game_ResetLevel(&s.game, false); /* the next recorded run starts from the exact initial state */
    }
    CHECK_EQ_INT(s.game.screen, SCREEN_ENDING);
    CHECK_EQ_INT(Save_CountCleared(&s.game.save), LEVEL_COUNT);
    CHECK(gritty_golf_score(&s) > 15000);

    step_idle(&s, 30);
    CHECK(!gritty_golf_is_over(&s)); /* the ending screen waits for the player */
    step_input(&s, press_action());
    CHECK(gritty_golf_is_over(&s)); /* ... and "play again" became "finish" */
    CHECK_EQ_INT(s.game.screen, SCREEN_ENDING);
    CHECK_EQ_INT(s.game.tr.phase, TR_NONE); /* the engine did not start a new campaign */

    int frames = s.frames, score = gritty_golf_score(&s);
    step_idle(&s, 20); /* after the end, steps are no-ops */
    CHECK_EQ_INT(s.frames, frames);
    CHECK_EQ_INT(gritty_golf_score(&s), score);
    gritty_golf_shutdown(&s);
}

/* ---- profiles -------------------------------------------------------------------------------------- */
static bool file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return false;
    fclose(f);
    return true;
}

static void test_profile_roundtrip(void) {
    const char *file = "./gritty_save_gg_adapter_test.dat";
    remove(file);
    remove("./gritty_save_gg_adapter_test.dat.bad");

    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK_EQ_INT(gritty_golf_open_profile(&s, "GG Adapter Test", "."), SAVE_NOT_FOUND);
    CHECK_STR_EQ(s.save_path, file);
    CHECK_STR_EQ(s.game.save.name, "GG Adapter Test");
    CHECK_EQ_INT(s.game.level_idx, 0);
    CHECK(replay_level(&s, 0));
    int score = gritty_golf_score(&s);
    CHECK(score > 0);
    CHECK(file_exists(file)); /* the engine autosaves the moment a level is cleared */
    gritty_golf_shutdown(&s);

    gritty_golf_State again;
    gritty_golf_init(&again, 2);
    CHECK_EQ_INT(gritty_golf_open_profile(&again, "gg adapter test", "./"),
                 SAVE_OK); /* case and trailing slash */
    CHECK_STR_EQ(again.save_path, file);
    CHECK_EQ_INT(gritty_golf_score(&again), score);
    CHECK_EQ_INT(again.game.level_idx, 1); /* continues at the first level not yet cleared */
    gritty_golf_shutdown(&again);

    /* a damaged file is kept as *.bad and the player starts fresh instead of crashing */
    FILE *f = fopen(file, "wb");
    CHECK(f != NULL);
    if (f == NULL)
        return;
    fputs("this is not a save file", f);
    fclose(f);
    gritty_golf_State broken;
    gritty_golf_init(&broken, 3);
    CHECK_EQ_INT(gritty_golf_open_profile(&broken, "GG Adapter Test", "."), SAVE_CORRUPT);
    CHECK_EQ_INT(gritty_golf_score(&broken), 0);
    CHECK_EQ_INT(broken.game.level_idx, 0);
    CHECK(file_exists("./gritty_save_gg_adapter_test.dat.bad"));
    gritty_golf_shutdown(&broken);

    remove(file);
    remove("./gritty_save_gg_adapter_test.dat.bad");
}

static void test_all_cleared_profile_starts_on_the_last_level(void) {
    const char *file = "./gritty_save_gg_adapter_done.dat";
    SaveData done;
    Save_Init(&done, "GG Adapter Done");
    for (int i = 0; i < LEVEL_COUNT; i++)
        Save_RecordClear(&done, i, 1, 1.0f);
    CHECK(Save_Write(&done, file));

    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK_EQ_INT(gritty_golf_open_profile(&s, "GG Adapter Done", "."), SAVE_OK);
    CHECK_EQ_INT(s.game.level_idx, LEVEL_COUNT - 1);
    gritty_golf_shutdown(&s);
    remove(file);
}

static void test_in_memory_run_writes_no_file(void) {
    remove("gritty_save.dat");
    remove("gritty_save_player.dat");
    gritty_golf_State s;
    gritty_golf_init(&s, 1);
    CHECK(replay_level(&s, 0)); /* clearing a level autosaves; with no path that must not touch the disk */
    step_idle(&s, 120);         /* running seconds mark the save dirty: shutdown must not flush it either */
    gritty_golf_shutdown(&s);
    CHECK(!file_exists("gritty_save.dat"));
    CHECK(!file_exists("gritty_save_player.dat"));
}

/* ---- monkey ---------------------------------------------------------------------------------------- */
static void test_same_input_same_result(void) {
    gritty_golf_State a, b;
    gritty_golf_init(&a, 9);
    gritty_golf_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 3000; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        gritty_golf_step(&a, &ia, ARC_DT);
        gritty_golf_step(&b, &ib, ARC_DT);
        a.game.settings.aim_assist = false; /* both treated alike; assist is slow in an unoptimised build */
        b.game.settings.aim_assist = false;
    }
    CHECK_EQ_INT(a.game.level_idx, b.game.level_idx);
    CHECK_EQ_INT(gritty_golf_score(&a), gritty_golf_score(&b));
    CHECK_EQ_INT(a.game.player.strokes, b.game.player.strokes);
    CHECK(memcmp(&a.game.player.ball, &b.game.player.ball, sizeof(a.game.player.ball)) ==
          0); /* bit identical */
    gritty_golf_shutdown(&a);
    gritty_golf_shutdown(&b);
}

/* Random input for `frames` steps; the engine invariants must hold after every single step.
 * Aim assist predicts the whole shot on every step, which is about 20x slower in an unoptimised build, so the
 * long run keeps it off (the monkey can switch it on by clicking the panel button) and a short run lets it
 * be. */
static void run_monkey(unsigned seed, int frames, bool keep_assist_off) {
    gritty_golf_State s;
    gritty_golf_init(&s, seed);
    s.game.settings.aim_assist = !keep_assist_off;
    ArcadeMonkey m;
    arcade_monkey_init(&m, seed + 6);
    int finishes = 0;
    for (int i = 0; i < frames; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        gritty_golf_step(&s, &in, ARC_DT);
        if (keep_assist_off)
            s.game.settings.aim_assist = false;
        if (gritty_golf_is_over(
                &s)) { /* random clicks sometimes hit "submit" twice: resume to keep the run long */
            finishes++;
            s.finished = false;
        }
        const Ball *ball = &s.game.player.ball;
        CHECK(isfinite(ball->pos.x) && isfinite(ball->pos.y));
        CHECK(isfinite(ball->vel.x) && isfinite(ball->vel.y));
        CHECK(s.game.level_idx >= 0 && s.game.level_idx < LEVEL_COUNT);
        CHECK(s.game.player.strikes >= 0 && s.game.player.strikes <= STRIKES_MAX);
        CHECK(s.game.preview_count >= 0 && s.game.preview_count <= MAX_PREVIEW_POINTS);
        CHECK(gritty_golf_score(&s) >= 0);
    }
    CHECK_EQ_INT(s.frames, frames); /* every step ran: nothing stopped the run early */
    printf("    (seed %u: level %d, score %d, \"finish\" confirmed %d times)\n", seed, s.game.level_idx,
           gritty_golf_score(&s), finishes);
    gritty_golf_shutdown(&s);
}

static void test_monkey_never_breaks_invariants(void) {
    run_monkey(5, 20000, true);
}

static void test_monkey_with_aim_assist(void) {
    run_monkey(8, 800, false);
}

int main(void) {
    RUN(test_canvas_geometry);
    RUN(test_canvas_rects_fit);
    RUN(test_panel_buttons_fit);
    RUN(test_init_is_sane);
    RUN(test_score_formula);
    RUN(test_action_runs_aim_lock_shoot);
    RUN(test_held_action_is_not_a_press);
    RUN(test_click_on_canvas_is_action_but_panels_are_not);
    RUN(test_canvas_icons);
    RUN(test_keys);
    RUN(test_finish_needs_a_confirmation);
    RUN(test_typed_key_repeat_is_debounced);
    RUN(test_panel_buttons);
    RUN(test_stage_select_dialog);
    RUN(test_replay_of_level_zero_scores);
    RUN(test_full_campaign_ends_with_the_action_button);
    RUN(test_profile_roundtrip);
    RUN(test_all_cleared_profile_starts_on_the_last_level);
    RUN(test_in_memory_run_writes_no_file);
    RUN(test_same_input_same_result);
    RUN(test_monkey_never_breaks_invariants);
    RUN(test_monkey_with_aim_assist);
    return TEST_SUMMARY();
}
