#include <stdio.h>

#include "arcade_test.h"
#include "beat_dodge_logic.h"

#define TMP_MAP "arcade_test_beatmap.tmp"

static void write_file(const char *text) {
    FILE *fp = fopen(TMP_MAP, "w");
    if (fp) {
        fputs(text, fp);
        fclose(fp);
    }
}

static void test_default_map_is_valid(void) {
    bd_BeatMap m;
    bd_beatmap_default(&m, 1);
    CHECK_EQ_INT(m.bpm, 120);
    CHECK(m.length >= 8 && m.length <= BD_MAX_BEATS);
    int obstacles = 0;
    for (int i = 0; i < m.length; i++) {
        CHECK(m.lane[i] >= -1 && m.lane[i] < BD_LANES);
        if (m.lane[i] >= 0)
            obstacles++;
    }
    CHECK(obstacles > 10);
    for (int i = 0; i < 4; i++)
        CHECK_EQ_INT(m.lane[i], -1); /* warm-up */
}

static void test_beatmap_parser(void) {
    bd_BeatMap m;
    write_file("# comment\n\nbpm 100\n..0.1.2.\n 2 1 0 . \r\n");
    CHECK(bd_beatmap_load(TMP_MAP, &m));
    CHECK_EQ_INT(m.bpm, 100);
    CHECK_EQ_INT(m.length, 12);
    CHECK_EQ_INT(m.lane[2], 0);
    CHECK_EQ_INT(m.lane[4], 1);
    CHECK_EQ_INT(m.lane[8], 2);
    CHECK_EQ_INT(m.lane[11], -1);
    write_file("bpm 500\n..0.1.2.\n");
    CHECK(!bd_beatmap_load(TMP_MAP, &m)); /* tempo out of range */
    write_file("bpm 120\n..0.9.2.\n");
    CHECK(!bd_beatmap_load(TMP_MAP, &m)); /* lane 9 does not exist */
    write_file("..0.1.2.1.0.1.2.\n");
    CHECK(!bd_beatmap_load(TMP_MAP, &m)); /* missing bpm line */
    write_file("bpm 120\n.0.\n");
    CHECK(!bd_beatmap_load(TMP_MAP, &m)); /* too short */
    CHECK(!bd_beatmap_load("no_such_beatmap.tmp", &m));
    remove(TMP_MAP);
}

/* Perfect play: always stand in a lane that the next unresolved obstacle is NOT in. */
static void play_perfectly(beat_dodge_State *s, int max_steps) {
    for (int i = 0; i < max_steps && !beat_dodge_is_over(s); i++) {
        ArcadeInput in;
        arcade_input_clear(&in);
        int j = s->next_hit;
        while (j < s->map.length && s->map.lane[j] < 0)
            j++;
        if (j < s->map.length && s->map.lane[j] == s->lane)
            in.left_pressed = (s->lane > 0), in.right_pressed = (s->lane == 0);
        beat_dodge_step(s, &in, ARC_DT);
    }
}

static void test_perfect_play_wins_without_losing_a_life(void) {
    beat_dodge_State s;
    beat_dodge_init(&s, 1);
    play_perfectly(&s, 60 * 60);
    CHECK(s.won);
    CHECK_EQ_INT(s.lives, BD_START_LIVES);
    CHECK(s.best_combo > 5);
    CHECK(beat_dodge_score(&s) > 150);
}

static void test_standing_still_loses_all_lives(void) {
    beat_dodge_State s;
    beat_dodge_init(&s, 1);
    ArcadeInput in;
    arcade_input_clear(&in);
    for (int i = 0; i < 60 * 60 && !beat_dodge_is_over(&s); i++)
        beat_dodge_step(&s, &in, ARC_DT);
    CHECK(beat_dodge_is_over(&s));
    CHECK(!s.won);
    CHECK_EQ_INT(s.lives, 0);
}

static void test_obstacles_arrive_exactly_on_the_beat(void) {
    beat_dodge_State s;
    bd_BeatMap m;
    bd_beatmap_default(&m, 1);
    for (int i = 0; i < m.length; i++)
        m.lane[i] = -1;
    m.lane[10] = 1; /* one obstacle in the player's lane */
    beat_dodge_init(&s, 1);
    beat_dodge_use_map(&s, &m);
    ArcadeInput in;
    arcade_input_clear(&in);
    float arrive = (10.0f + BD_LEAD_BEATS) * s.spb;
    while (s.time < arrive - 0.02f)
        beat_dodge_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.lives, BD_START_LIVES); /* not there yet */
    CHECK_NEAR(beat_dodge_obstacle_y(&s, 10), BD_PLAYER_Y, 12.0);
    while (s.time < arrive + 0.03f)
        beat_dodge_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.lives, BD_START_LIVES - 1);
}

static void test_beat_event_fires_once_per_beat(void) {
    beat_dodge_State s;
    beat_dodge_init(&s, 1);
    ArcadeInput in;
    arcade_input_clear(&in);
    int events = 0;
    for (int i = 0; i < 120; i++) { /* 2 seconds at 120 bpm = 4 beats (plus the very first one at t=0) */
        beat_dodge_step(&s, &in, ARC_DT);
        if (s.beat_event)
            events++;
    }
    CHECK(events >= 4 && events <= 5);
}

static void test_lane_is_clamped_and_monkey_is_safe(void) {
    beat_dodge_State s;
    beat_dodge_init(&s, 2);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 7);
    for (int i = 0; i < 5000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        beat_dodge_step(&s, &in, ARC_DT);
        CHECK(s.lane >= 0 && s.lane < BD_LANES);
        CHECK(s.lives >= 0 && s.lives <= BD_START_LIVES);
        CHECK(beat_dodge_score(&s) >= 0);
    }
}

int main(void) {
    RUN(test_default_map_is_valid);
    RUN(test_beatmap_parser);
    RUN(test_perfect_play_wins_without_losing_a_life);
    RUN(test_standing_still_loses_all_lives);
    RUN(test_obstacles_arrive_exactly_on_the_beat);
    RUN(test_beat_event_fires_once_per_beat);
    RUN(test_lane_is_clamped_and_monkey_is_safe);
    return TEST_SUMMARY();
}
