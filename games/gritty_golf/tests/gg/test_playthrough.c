/*
 * test_playthrough.c - end-to-end tests
 *
 * A "robot player" replays one recorded winning run for every level through the
 * REAL game logic (Game_Tick), pressing the action button on exact ticks.  If
 * physics, collisions, walls, buttons, pickups, strikes, goal detection or the
 * state machine were broken in ANY way, some level would no longer be completable.
 *
 * The runs in solutions.h were found by tools/solve_levels.c (a beam-search robot).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "audio_stub.h"
#include "game.h"
#include "records.h"
#include "save.h"
#include "vec2.h"
#include "solutions.h"

typedef struct {
    bool won;
    bool died;
    int  goal_tick;
    int  strokes;
} RunResult;

static void start_game(Game *g, int level)
{
    Settings st;
    SaveData sv;
    Settings_Default(&st);
    Save_Init(&sv, "Robot");
    TmpRemove("play_save");
    AudioStub_Reset();
    CHECK(Game_Init(g, &st, &sv, TmpPath("play_save"), level, false));
}

/* Press the action button on the listed ticks; stop at the goal, on death, or 700 ticks after the last press. */
static RunResult play(Game *g, const Solution *s, float dx, float dy)
{
    RunResult r = { false, false, -1, 0 };
    g->player.ball.pos.x += dx;                      /* optional nudge (robustness check) */
    g->player.ball.pos.y += dy;

    int next = 0;
    int limit = s->presses[s->npress - 1] + 700;
    for (int t = 0; t < limit; t++) {
        bool press = (next < s->npress && s->presses[next] == t);
        if (press) next++;

        int resets = g->level_resets;
        Input in;
        Input_Clear(&in);
        in.action = press;
        Game_Tick(g, &in);

        if (g->level_resets != resets) { r.died = true; break; }
        if (g->goal_reached) { r.won = true; r.goal_tick = t; r.strokes = g->player.strokes; break; }
    }
    return r;
}

static const Solution *find_solution(int level)
{
    for (int i = 0; i < SOLUTION_COUNT; i++)
        if (SOLUTIONS[i].level == level)
            return &SOLUTIONS[i];
    return NULL;
}

/* ------------------------------------------------------------------------------ */
static void test_solution_table(void)
{
    CHECK_INT(SOLUTION_COUNT, LEVEL_COUNT);
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        const Solution *s = find_solution(lvl);
        CHECK(s != NULL);
        if (!s) continue;
        CHECK(s->npress >= 3);
        CHECK_INT(s->npress % 3, 0);                              /* whole shots: aim, lock, shoot */
        CHECK(s->npress <= MAX_SOLUTION_PRESSES);
        for (int i = 1; i < s->npress; i++)
            CHECK(s->presses[i] > s->presses[i - 1]);             /* strictly increasing ticks */
    }
}

static void test_every_level_can_be_won(void)
{
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        const Solution *s = find_solution(lvl);
        if (!s) { CHECK(0); continue; }
        Game g;
        start_game(&g, lvl);
        RunResult r = play(&g, s, 0.0f, 0.0f);
        if (!r.won)
            printf("    level %d (%s) was NOT completed (died=%d)\n", lvl, Level_Name(lvl), r.died);
        CHECK(r.won);
        CHECK(!r.died);
        CHECK(r.strokes >= 1);
        CHECK(r.strokes <= s->npress / 3);                        /* never more shots than recorded */
        CHECK_INT(g.tr.phase, TR_COVER);                          /* the level-complete transition started */
        CHECK_INT(g.tr.target, lvl + 1);
        CHECK(Save_IsCleared(&g.save, lvl));
        CHECK_INT(g.save.best_strokes[lvl], r.strokes);
        CHECK_INT(g_sfx_count[SFX_LEVEL_BEAT], 1);
        Game_Shutdown(&g);
    }
    TmpRemove("play_save");
}

/* the same runs with the start position nudged by a hair: results must not depend on
 * floating-point noise (this is what makes the replays safe on other CPUs / compilers) */
static void test_runs_are_robust(void)
{
    static const float nudge[][2] = { { 0.02f, 0.0f }, { -0.02f, 0.0f }, { 0.0f, 0.02f }, { 0.0f, -0.02f } };
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        const Solution *s = find_solution(lvl);
        if (!s) continue;
        for (int k = 0; k < 4; k++) {
            Game g;
            start_game(&g, lvl);
            RunResult r = play(&g, s, nudge[k][0], nudge[k][1]);
            if (!r.won)
                printf("    level %d with nudge %d failed\n", lvl, k);
            CHECK(r.won);
            Game_Shutdown(&g);
        }
    }
    TmpRemove("play_save");
}

static void test_replay_is_deterministic(void)
{
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl += 4) {
        const Solution *s = find_solution(lvl);
        Game a, b;
        start_game(&a, lvl);
        RunResult ra = play(&a, s, 0, 0);
        start_game(&b, lvl);
        RunResult rb = play(&b, s, 0, 0);
        CHECK_INT(ra.goal_tick, rb.goal_tick);                    /* same ticks ... */
        CHECK(memcmp(&a.player.ball, &b.player.ball, sizeof a.player.ball) == 0);   /* ... and bit-identical ball */
        Game_Shutdown(&a);
        Game_Shutdown(&b);
    }
    TmpRemove("play_save");
}

/* Without any input the ball must never produce NaN/inf values or leave the world unnoticed. */
static void test_idle_simulation_is_sane(void)
{
    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        Game g;
        start_game(&g, lvl);
        float world = (float)VIRTUAL_SIZE / g.level->def->camera_zoom;
        for (int t = 0; t < 1200; t++) {                          /* 20 seconds of doing nothing */
            Input in;
            Input_Clear(&in);
            Game_Tick(&g, &in);
            Vec2 p = g.player.ball.pos;
            if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(g.player.ball.vel.x) || !isfinite(g.player.ball.vel.y)) {
                CHECK(0);
                break;
            }
            CHECK(p.x > -40.0f && p.x < world + 40.0f);           /* out-of-bounds is caught and reset */
            CHECK(p.y > -40.0f && p.y < world + 40.0f);
        }
        Game_Shutdown(&g);
    }
    TmpRemove("play_save");
}

/* The complete game: play level 0..24 one after another, finish with the ending screen,
 * check the save file and the leaderboard.                                                */
static void test_full_campaign(void)
{
    Game g;
    start_game(&g, 0);
    int total_shots = 0;

    for (int lvl = 0; lvl < LEVEL_COUNT; lvl++) {
        const Solution *s = find_solution(lvl);
        CHECK_INT(g.level_idx, lvl);
        RunResult r = play(&g, s, 0, 0);
        CHECK(r.won);
        if (!r.won) break;
        total_shots += r.strokes;

        /* let the transition run: it must end on the next level (or on the ending screen) */
        for (int t = 0; t < 400 && g.tr.phase != TR_NONE; t++) {
            Input in; Input_Clear(&in);
            Game_Tick(&g, &in);
        }
        CHECK_INT(g.tr.phase, TR_NONE);
        if (lvl + 1 < LEVEL_COUNT) {
            CHECK_INT(g.level_idx, lvl + 1);
            CHECK_INT(g.screen, SCREEN_PLAY);
            Game_ResetLevel(&g, false);          /* back to the exact start state, so the next recorded run lines up */
        }
    }
    CHECK_INT(g.screen, SCREEN_ENDING);

    CHECK_INT(Save_CountCleared(&g.save), LEVEL_COUNT);
    CHECK_INT(g.save.cleared_mask, (1u << LEVEL_COUNT) - 1u);
    CHECK_INT(Save_FirstUncleared(&g.save), LEVEL_COUNT);
    CHECK_INT(Save_TotalBestStrokes(&g.save), total_shots);
    CHECK(Save_TotalBestTime(&g.save) > 10.0f);
    CHECK(Save_Score(&g.save) > 15000);                           /* 25000 minus a few strokes and seconds */

    SaveData onDisk;                                              /* the autosave has everything too */
    CHECK_INT(Save_Load(&onDisk, TmpPath("play_save")), SAVE_OK);
    CHECK_INT(onDisk.cleared_mask, g.save.cleared_mask);

    /* leaderboard submission */
    TmpRemove("play_scores");
    CHECK(Records_SubmitProfile(TmpPath("play_scores"), &g.save));
    ScoreRecord *list = NULL; int n = 0;
    CHECK(Records_Load(TmpPath("play_scores"), &list, &n));
    CHECK_INT(n, 1);
    if (n == 1) {
        CHECK_STR(list[0].name, "Robot");
        CHECK_INT(list[0].levels, LEVEL_COUNT);
        CHECK_INT(list[0].strokes, total_shots);
    }
    free(list);
    TmpRemove("play_scores");

    Game_Shutdown(&g);
    TmpRemove("play_save");
}

void run_playthrough_tests(void)
{
    RUN(test_solution_table);
    RUN(test_every_level_can_be_won);
    RUN(test_runs_are_robust);
    RUN(test_replay_is_deterministic);
    RUN(test_idle_simulation_is_sane);
    RUN(test_full_campaign);
}
