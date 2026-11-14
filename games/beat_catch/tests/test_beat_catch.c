#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"
#include "arcade_test.h"
#include "beat_catch_logic.h"

static beat_catch_State g_s; /* ~34 KB: keep off the stack */
static bd_Song g_song;
static bd_SongList g_list; /* ~544 KB */

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

/* The next bead that has not been judged yet (chart length when there is none). */
static int next_bead(const beat_catch_State *s) {
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    int j = s->run.next_hit;
    while (j < s->run.length && c->lane[j] < 0)
        j++;
    return j;
}

/* Perfect bot: jumps (direct lane keys) to the lane of the next bead, so it catches everything. */
static void play_perfectly(beat_catch_State *s, int max_steps) {
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    for (int i = 0; i < max_steps && !beat_catch_is_over(s); i++) {
        ArcadeInput in = idle();
        int j = next_bead(s);
        if (j < s->run.length)
            in.typed = (char)('1' + c->lane[j]);
        beat_catch_step(s, &in, ARC_DT);
    }
}

/* Human-speed bot: follows the path of bd_chart_plan with one arrow tap per 0.1 s (the model allows 0.12 s;
 * the 0.02 s pay for rounding to the 1/60 s steps of the game). */
static void play_the_plan(beat_catch_State *s, const int8_t *plan, int max_steps) {
    float last_tap = -10.0f;
    for (int i = 0; i < max_steps && !beat_catch_is_over(s); i++) {
        ArcadeInput in = idle();
        int j = next_bead(s);
        if (j < s->run.length && plan[j] != s->run.lane && s->run.time - last_tap >= 0.1f) {
            in.left_pressed = plan[j] < s->run.lane;
            in.right_pressed = plan[j] > s->run.lane;
            last_tap = s->run.time;
        }
        beat_catch_step(s, &in, ARC_DT);
    }
}

/* A song made from a pattern: 120 BPM and 4 steps per beat give steps of 0.125 s. '.' = rest, '0'..'4' = a
 * bead in that lane that starts an object, 'a'..'e' = a bead in lane 0..4 that continues the object. */
static void make_chart(bd_Song *song, const char *name, int lanes, const char *pattern) {
    memset(song, 0, sizeof(*song));
    arc_strlcpy(song->title, "Hand made", sizeof(song->title));
    song->bpm = 120;
    song->steps = 4;
    song->chart_count = 1;
    bd_Chart *c = &song->charts[0];
    arc_strlcpy(c->name, name, sizeof(c->name));
    c->kind = BD_CHART_CATCH;
    c->lanes = lanes;
    for (const char *p = pattern; *p; p++) {
        if (*p == '.')
            c->lane[c->length] = -1;
        else if (*p >= 'a') {
            c->lane[c->length] = (int8_t)(*p - 'a');
            c->link[c->length] = 1;
        } else {
            c->lane[c->length] = (int8_t)(*p - '0');
        }
        c->length++;
    }
}

static void start_chart(int lanes, const char *pattern) {
    make_chart(&g_song, "normal", lanes, pattern);
    beat_catch_use_song(&g_s, &g_song, 0);
}

/* Steps the game without any input until `seconds` after the arrival of step `step`. */
static void run_past(int step, float seconds) {
    ArcadeInput in = idle();
    float until = bd_run_arrival(&g_s.run, step) + seconds;
    while (g_s.run.time < until && !beat_catch_is_over(&g_s))
        beat_catch_step(&g_s, &in, ARC_DT);
}

/* ------------------------------------------------------------------ the rules on hand-made charts */
static void test_init_is_sane(void) {
    beat_catch_init(&g_s, 1);
    CHECK(!beat_catch_is_over(&g_s));
    CHECK_EQ_INT(beat_catch_score(&g_s), 0);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH);
    CHECK_EQ_INT(g_s.combo, 0);
    CHECK(g_s.run.song.charts[0].kind == BD_CHART_CATCH);
    CHECK(g_s.max_points > 0);
}

static void test_a_lone_bead_needs_the_exact_lane(void) {
    start_chart(3, "........1"); /* the player starts in the middle lane: lane 1 */
    run_past(8, 0.1f);
    CHECK_EQ_INT(g_s.caught, 1);
    CHECK_EQ_INT(g_s.points, BC_LONE_POINTS);
    start_chart(3, "........0"); /* one lane away: a lone bead has tolerance 0 */
    run_past(8, 0.1f);
    CHECK_EQ_INT(g_s.caught, 0);
    CHECK_EQ_INT(g_s.missed, 1);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS);
}

static void test_a_ribbon_that_changes_lane_is_caught_from_its_middle(void) {
    start_chart(3, "........1a.c"); /* lane 1, 0, (rest), 2: standing in lane 1 is within 1 of every bead */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.caught, 3);
    CHECK_EQ_INT(g_s.missed, 0);
    CHECK_EQ_INT(g_s.points, 3 * BC_RIBBON_POINTS); /* ribbon beads pay half of a lone bead */
    start_chart(
        4, "........0b.c"); /* starts in lane 2: lane 0 is two away from it, so the first bead is missed */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.missed, 1);
    CHECK_EQ_INT(g_s.caught, 2);
}

static void test_a_ribbon_in_one_lane_needs_that_exact_lane(void) {
    start_chart(3, "........0a.a"); /* a ribbon in lane 0 only; the player stands in lane 1 */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.caught, 0); /* no tolerance: this ribbon does not move, so neither may the player */
    CHECK_EQ_INT(g_s.missed, 3);
    start_chart(
        3, "........1a.a"); /* the same shape in the player's own lane: lane 1, then lane 0 (changes lane) */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.caught, 3);
    start_chart(3, "........1b.b");
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.caught, 3); /* lane 1 only, standing in lane 1 */
    CHECK_EQ_INT(g_s.points, 3 * BC_RIBBON_POINTS);
}

static void test_a_ribbon_costs_exactly_one_penalty(void) {
    start_chart(
        5, "........0bcd"); /* a ribbon from lane 0 to 3; the player stands in lane 2: bead 0 is 2 away */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.missed, 1);
    CHECK_EQ_INT(g_s.missed_objects, 1);
    start_chart(5, "........0.......4.......0b.c"); /* three objects with a miss: three penalties */
    run_past(27, 0.1f);
    CHECK_EQ_INT(g_s.missed, 3); /* the lone beads and the first bead of the ribbon (lane 0 is 2 away) */
    CHECK_EQ_INT(g_s.caught, 2);
    CHECK_EQ_INT(g_s.missed_objects, 3);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - 3 * BC_HEALTH_PER_MISS);
    /* the same object missed in all of its beads still costs one penalty */
    start_chart(5, "........0a.a.a"); /* a ribbon of four beads in lane 0: the player stays in lane 2 */
    run_past(13, 0.1f);
    CHECK_EQ_INT(g_s.missed, 4);
    CHECK_EQ_INT(g_s.missed_objects, 1);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS);
}

static void test_the_combo_multiplier_steps_at_10_20_and_30(void) {
    CHECK_EQ_INT(beat_catch_combo_multiplier(0), 1);
    CHECK_EQ_INT(beat_catch_combo_multiplier(9), 1);
    CHECK_EQ_INT(beat_catch_combo_multiplier(10), 2);
    CHECK_EQ_INT(beat_catch_combo_multiplier(19), 2);
    CHECK_EQ_INT(beat_catch_combo_multiplier(20), 3);
    CHECK_EQ_INT(beat_catch_combo_multiplier(30), 4);
    CHECK_EQ_INT(beat_catch_combo_multiplier(500), 4); /* it stops at x4 */
    /* 35 lone beads in the middle lane, 0.5 s apart, caught one after the other */
    char pattern[200];
    int n = 0;
    for (int i = 0; i < 4; i++) /* two beats of warm-up */
        pattern[n++] = '.';
    for (int i = 0; i < 35; i++) {
        pattern[n++] = '1';
        pattern[n++] = '.';
        pattern[n++] = '.';
        pattern[n++] = '.';
    }
    pattern[n] = '\0';
    start_chart(3, pattern);
    run_past(4 + 9 * 4, 0.05f); /* the 10th bead */
    CHECK_EQ_INT(g_s.combo, 10);
    CHECK_EQ_INT(g_s.points, 10 * BC_LONE_POINTS);
    run_past(4 + 10 * 4, 0.05f); /* the 11th bead is worth x2 */
    CHECK_EQ_INT(g_s.points, 10 * BC_LONE_POINTS + 2 * BC_LONE_POINTS);
    run_past(4 + 29 * 4, 0.05f); /* the 30th bead */
    CHECK_EQ_INT(g_s.points, 10 * 10 + 10 * 20 + 10 * 30);
    run_past(4 + 30 * 4, 0.05f); /* the 31st: x4 */
    CHECK_EQ_INT(g_s.points, 10 * 10 + 10 * 20 + 10 * 30 + 40);
    CHECK_EQ_INT(g_s.best_combo, 31);
}

static void test_a_miss_resets_the_combo(void) {
    start_chart(3, "........1...1...1...0...1...1"); /* beads at steps 8, 12, 16, 20 (missed), 24, 28 */
    run_past(16, 0.05f);
    CHECK_EQ_INT(g_s.combo, 3);
    run_past(20, 0.05f);
    CHECK_EQ_INT(g_s.combo, 0);
    CHECK_EQ_INT(g_s.best_combo, 3);
    run_past(28, 0.05f);
    CHECK_EQ_INT(g_s.combo, 2);
    CHECK_EQ_INT(g_s.points,
                 3 * BC_LONE_POINTS + 2 * BC_LONE_POINTS); /* the multiplier started again at x1 */
}

static void test_health_is_counted_per_object_and_never_above_the_start(void) {
    start_chart(3, "........0...1...1...1"); /* a miss, then three lone beads caught */
    run_past(8, 0.05f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS);
    run_past(20, 0.05f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS + 3 * BC_HEALTH_PER_CLEAN_OBJECT);
    start_chart(3, "........1...1...1");
    run_past(16, 0.05f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH); /* 100 is the ceiling */
    /* a ribbon of six beads caught completely heals ONCE, like a lone bead */
    start_chart(3, "........0...1b.b.b.b.b");
    run_past(8, 0.05f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS);
    run_past(22, 0.05f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS + BC_HEALTH_PER_CLEAN_OBJECT);
    /* a ribbon with one missed bead costs health and gives none back */
    start_chart(3, "........1a.a");
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH); /* all three caught (the ribbon changes lane): clean */
    start_chart(4, "........0b.c");
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.health, BC_START_HEALTH - BC_HEALTH_PER_MISS);
}

static void test_zero_health_ends_the_run(void) {
    char pattern[200];
    int n = 0;
    for (int i = 0; i < 4; i++)
        pattern[n++] = '.';
    for (int i = 0; i < 40; i++) { /* lone beads in lane 0 that the player (lane 1) never catches */
        pattern[n++] = '0';
        pattern[n++] = '.';
        pattern[n++] = '.';
        pattern[n++] = '.';
    }
    pattern[n] = '\0';
    start_chart(3, pattern);
    ArcadeInput in = idle();
    int misses_to_die = (BC_START_HEALTH + BC_HEALTH_PER_MISS - 1) / BC_HEALTH_PER_MISS; /* 7 */
    for (int i = 0; i < 60 * 60 && !beat_catch_is_over(&g_s); i++)
        beat_catch_step(&g_s, &in, ARC_DT);
    CHECK(beat_catch_is_over(&g_s));
    CHECK(!g_s.run.won);
    CHECK_EQ_INT(g_s.health, 0);
    CHECK_EQ_INT(g_s.missed, misses_to_die);
    CHECK(beat_catch_score(&g_s) == 0); /* it earned nothing and the finish bonus needs a won run */
}

static void test_surviving_the_song_pays_for_the_health_left(void) {
    start_chart(3, "........1...1");
    play_perfectly(&g_s, 60 * 30);
    CHECK(g_s.run.won);
    CHECK_EQ_INT(g_s.points, 2 * BC_LONE_POINTS + BC_START_HEALTH * BC_FINISH_PER_HEALTH);
    CHECK_EQ_INT(g_s.points, g_s.max_points);
}

static void test_the_perfect_bot_scores_the_ceiling(void) {
    const char *names[3] = {"easy", "normal", "hard"};
    const int ceiling[3] = {1000, 1500, 2000};
    for (int k = 0; k < 3; k++) {
        make_chart(&g_song, names[k], 4, "........0.......1..2b.c...3a......2.......1.......0.....3");
        beat_catch_use_song(&g_s, &g_song, 0);
        play_perfectly(&g_s, 60 * 60);
        CHECK(g_s.run.won);
        CHECK_EQ_INT(g_s.missed, 0);
        CHECK_EQ_INT(g_s.caught, bd_chart_beads(&g_song.charts[0]));
        CHECK_EQ_INT(g_s.points, g_s.max_points);
        CHECK_EQ_INT(beat_catch_score(&g_s), ceiling[k]); /* exactly: the score uses integer maths */
    }
}

static void test_standing_in_a_corner_loses_and_scores_almost_nothing(void) {
    char pattern[300];
    int n = 0;
    for (int i = 0; i < 4; i++)
        pattern[n++] = '.';
    for (int i = 0; i < 60; i++) { /* lanes 0, 1, 2, 3, 0, 1, ... one bead every 0.5 s */
        pattern[n++] = (char)('0' + i % 4);
        pattern[n++] = '.';
        pattern[n++] = '.';
        pattern[n++] = '.';
    }
    pattern[n] = '\0';
    start_chart(4, pattern);
    ArcadeInput in = idle();
    in.typed = '1'; /* a camper: lane 0 only */
    for (int i = 0; i < 60 * 40 && !beat_catch_is_over(&g_s); i++) {
        beat_catch_step(&g_s, &in, ARC_DT);
        in.typed = 0;
    }
    CHECK(beat_catch_is_over(&g_s));
    CHECK(!g_s.run.won);
    CHECK(g_s.caught < 8);
    CHECK(beat_catch_score(&g_s) < 1500 / 10);
}

static void test_default_song_perfect_bot_wins(void) {
    beat_catch_init(&g_s, 3);
    play_perfectly(&g_s, 60 * 90);
    CHECK(g_s.run.won);
    CHECK_EQ_INT(g_s.missed, 0);
    CHECK_EQ_INT(g_s.points, g_s.max_points);
}

static void test_sync_only_fixes_large_drift(void) {
    beat_catch_init(&g_s, 1);
    ArcadeInput in = idle();
    for (int i = 0; i < 60; i++)
        beat_catch_step(&g_s, &in, ARC_DT);
    float t = g_s.run.time;
    beat_catch_sync(&g_s, t + 0.05f); /* small drift: left alone */
    CHECK_NEAR(g_s.run.time, t, 0.0001);
    beat_catch_sync(&g_s, t + 0.5f);
    CHECK_NEAR(g_s.run.time, t + 0.5f, 0.0001);
}

static void test_the_clock_and_the_arrival_match_beat_dodge(void) {
    start_chart(3, "........1");
    CHECK_NEAR(bd_run_arrival(&g_s.run, 8), 1.5f + 8 * 0.125f, 0.0001); /* lead = 3 beats of 0.5 s */
    CHECK(bd_run_obstacle_y(&g_s.run, 8) < BD_TOP_Y);                   /* not spawned at time 0 */
    CHECK_EQ_INT(bd_catch_tolerance(0), 0);                             /* an object that stays in one lane */
    CHECK_EQ_INT(bd_catch_tolerance(2), 1);                             /* an object that changes lane */
}

/* ------------------------------------------------------------------ the shipped songs */
static void test_the_human_speed_bot_clears_every_shipped_catch_chart(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    static int8_t plan[BD_MAX_STEPS];
    int played = 0;
    for (int i = 0; i < n; i++)
        for (int c = 0; c < g_list.songs[i].chart_count; c++) {
            if (g_list.songs[i].charts[c].kind != BD_CHART_CATCH)
                continue;
            CHECK(bd_chart_plan(&g_list.songs[i], c, plan));
            beat_catch_use_song(&g_s, &g_list.songs[i], c);
            play_the_plan(&g_s, plan, 60 * 400);
            CHECK(g_s.run.won);
            CHECK(g_s.health > 0);
            CHECK(g_s.missed_objects * 10 <
                  bd_chart_objects(&g_list.songs[i].charts[c])); /* nearly all caught */
            played++;
        }
    CHECK(played >= 3);
}

static void test_the_perfect_bot_scores_the_ceiling_on_shipped_charts(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    int played = 0;
    for (int i = 0; i < n; i++)
        for (int c = 0; c < g_list.songs[i].chart_count; c++) {
            if (g_list.songs[i].charts[c].kind != BD_CHART_CATCH)
                continue;
            beat_catch_use_song(&g_s, &g_list.songs[i], c);
            play_perfectly(&g_s, 60 * 400);
            CHECK(g_s.run.won);
            CHECK_EQ_INT(g_s.missed, 0);
            CHECK_EQ_INT(g_s.points, g_s.max_points);
            CHECK_EQ_INT(beat_catch_score(&g_s),
                         (int)(1000.0f * bd_chart_multiplier(g_list.songs[i].charts[c].name)));
            played++;
        }
    CHECK(played >= 3);
}

static void test_standing_still_loses_on_every_shipped_catch_chart(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    int played = 0;
    for (int i = 0; i < n; i++)
        for (int c = 0; c < g_list.songs[i].chart_count; c++) {
            if (g_list.songs[i].charts[c].kind != BD_CHART_CATCH)
                continue;
            beat_catch_use_song(&g_s, &g_list.songs[i], c);
            ArcadeInput in = idle();
            for (int k = 0; k < 60 * 400 && !beat_catch_is_over(&g_s); k++)
                beat_catch_step(&g_s, &in, ARC_DT);
            CHECK(beat_catch_is_over(&g_s));
            CHECK(!g_s.run.won); /* health runs out before the song ends */
            played++;
        }
    CHECK(played >= 3);
}

static void test_monkey_is_safe(void) {
    beat_catch_init(&g_s, 2);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 7);
    for (int i = 0; i < 5000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        beat_catch_step(&g_s, &in, ARC_DT);
        CHECK(g_s.run.lane >= 0 && g_s.run.lane < g_s.run.lanes);
        CHECK(g_s.health >= 0 && g_s.health <= BC_START_HEALTH);
        CHECK(beat_catch_score(&g_s) >= 0);
    }
    /* a real song full of ribbons: the invariants of the scoring hold for any input */
    CHECK(bd_song_load(ARCADE_ASSETS_DIR "/beatmaps/fnf/thorns.txt", &g_song));
    for (int c = 0; c < g_song.chart_count; c++) {
        if (g_song.charts[c].kind != BD_CHART_CATCH)
            continue;
        beat_catch_use_song(&g_s, &g_song, c);
        arcade_monkey_init(&m, (uint64_t)(11 + c));
        for (int i = 0; i < 60 * 200 && !beat_catch_is_over(&g_s); i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            beat_catch_step(&g_s, &in, ARC_DT);
            CHECK(g_s.run.lane >= 0 && g_s.run.lane < g_s.run.lanes);
            CHECK(g_s.health >= 0 && g_s.health <= BC_START_HEALTH);
            CHECK(g_s.points >= 0 && g_s.points <= g_s.max_points);
            CHECK(g_s.combo <= g_s.caught && g_s.best_combo >= g_s.combo);
            CHECK(g_s.missed_objects <= g_s.missed);
        }
        CHECK(beat_catch_score(&g_s) <= (int)(1000.0f * g_s.multiplier));
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_a_lone_bead_needs_the_exact_lane);
    RUN(test_a_ribbon_that_changes_lane_is_caught_from_its_middle);
    RUN(test_a_ribbon_in_one_lane_needs_that_exact_lane);
    RUN(test_a_ribbon_costs_exactly_one_penalty);
    RUN(test_the_combo_multiplier_steps_at_10_20_and_30);
    RUN(test_a_miss_resets_the_combo);
    RUN(test_health_is_counted_per_object_and_never_above_the_start);
    RUN(test_zero_health_ends_the_run);
    RUN(test_surviving_the_song_pays_for_the_health_left);
    RUN(test_the_perfect_bot_scores_the_ceiling);
    RUN(test_standing_in_a_corner_loses_and_scores_almost_nothing);
    RUN(test_default_song_perfect_bot_wins);
    RUN(test_sync_only_fixes_large_drift);
    RUN(test_the_clock_and_the_arrival_match_beat_dodge);
    RUN(test_the_human_speed_bot_clears_every_shipped_catch_chart);
    RUN(test_the_perfect_bot_scores_the_ceiling_on_shipped_charts);
    RUN(test_standing_still_loses_on_every_shipped_catch_chart);
    RUN(test_monkey_is_safe);
    return TEST_SUMMARY();
}
