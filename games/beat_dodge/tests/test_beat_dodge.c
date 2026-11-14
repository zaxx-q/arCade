#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"
#include "arcade_test.h"
#include "beat_dodge_convert.h"
#include "beat_dodge_logic.h"

#define TMP_SONG "arcade_test_song.tmp"

static beat_dodge_State g_s; /* ~34 KB: keep off the stack */
static bd_Song g_song, g_back;
static bd_SongList g_list; /* ~544 KB */

static void write_file(const char *text) {
    FILE *fp = fopen(TMP_SONG, "w");
    if (fp) {
        fputs(text, fp);
        fclose(fp);
    }
}

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

/* The next bead that has not been judged yet (chart->length when there is none). */
static int next_bead(const beat_dodge_State *s) {
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    int j = s->run.next_hit;
    while (j < s->run.length && c->lane[j] < 0)
        j++;
    return j;
}

/* Teleporting bot: stands in a lane nobody is about to hit (direct lane keys), so it never loses a life.
 * close_in > 0 makes it a CLOSE-CALL bot: it waits in the lane of the next object until close_in seconds
 * before the object arrives, then leaves. */
static void teleport_bot(const beat_dodge_State *s, float close_in, ArcadeInput *in) {
    const bd_Run *r = &s->run;
    const bd_Chart *c = &r->song.charts[r->chart];
    bool danger[BD_MAX_LANES] = {false};
    float horizon = r->time + (close_in > 0.0f ? close_in : 1.5f * ARC_DT);
    for (int j = r->next_hit; j < r->length && bd_run_arrival(r, j) <= horizon; j++)
        if (c->lane[j] >= 0)
            danger[c->lane[j]] = true;
    int want = r->lane;
    int j = next_bead(s);
    if (close_in > 0.0f && j < r->length && !c->link[j] && !danger[c->lane[j]])
        want = c->lane[j]; /* wait right where the object will come down */
    if (danger[want])
        for (int l = r->lanes - 1; l >= 0; l--) /* any lane that is not about to be hit */
            if (!danger[l])
                want = l;
    if (want != r->lane)
        in->typed = (char)('1' + want);
}

/* Perfect play: never loses a life (but is not close on purpose). */
static void play_perfectly(beat_dodge_State *s, int max_steps) {
    for (int i = 0; i < max_steps && !beat_dodge_is_over(s); i++) {
        ArcadeInput in = idle();
        teleport_bot(s, 0.0f, &in);
        beat_dodge_step(s, &in, ARC_DT);
    }
}

/* Close-call play: every object is entered as late as possible. */
static void play_close_calls(beat_dodge_State *s, int max_steps) {
    for (int i = 0; i < max_steps && !beat_dodge_is_over(s); i++) {
        ArcadeInput in = idle();
        teleport_bot(s, 0.1f, &in);
        beat_dodge_step(s, &in, ARC_DT);
    }
}

/* Human-speed bot: follows the path of bd_chart_plan with one arrow tap per 0.1 s (the model allows 0.12 s;
 * the 0.02 s pay for rounding to the 1/60 s steps of the game). */
static void play_the_plan(beat_dodge_State *s, const int8_t *plan, int max_steps) {
    float last_tap = -10.0f;
    for (int i = 0; i < max_steps && !beat_dodge_is_over(s); i++) {
        ArcadeInput in = idle();
        int j = next_bead(s);
        if (j < s->run.length && plan[j] != s->run.lane && s->run.time - last_tap >= 0.1f) {
            in.left_pressed = plan[j] < s->run.lane;
            in.right_pressed = plan[j] > s->run.lane;
            last_tap = s->run.time;
        }
        beat_dodge_step(s, &in, ARC_DT);
    }
}

static void test_default_song_is_valid(void) {
    bd_song_default(&g_song, 1);
    CHECK_EQ_INT(g_song.bpm, 120);
    CHECK_EQ_INT(g_song.chart_count, 1);
    CHECK_EQ_INT(g_song.charts[0].lanes, 3);
    CHECK(bd_chart_beads(&g_song.charts[0]) > 10);
    for (int i = 0; i < 4; i++)
        CHECK_EQ_INT(g_song.charts[0].lane[i], -1); /* warm-up */
    for (int i = 0; i < g_song.charts[0].length; i++)
        CHECK(g_song.charts[0].lane[i] >= -1 && g_song.charts[0].lane[i] < 3);
}

static void test_parser_v2(void) {
    write_file("# comment\ntitle My Song\nbpm 140\nsteps 4\nchart easy 3\n..0.1.2.\n2.1.0...\n"
               "chart hard 4\n0.1.2.3.0.1.2.3.\nmelody\n0 12 60 0\n24 6 72 1\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    CHECK_STR_EQ(g_song.title, "My Song");
    CHECK_EQ_INT(g_song.bpm, 140);
    CHECK_EQ_INT(g_song.steps, 4);
    CHECK_EQ_INT(g_song.chart_count, 2);
    CHECK_STR_EQ(g_song.charts[0].name, "easy");
    CHECK_EQ_INT(g_song.charts[0].length, 16); /* two pattern lines are joined */
    CHECK_EQ_INT(g_song.charts[0].lane[2], 0);
    CHECK_EQ_INT(g_song.charts[0].lane[8], 2);
    CHECK_EQ_INT(g_song.charts[1].lanes, 4);
    CHECK_EQ_INT(g_song.charts[1].lane[6], 3);
    CHECK_EQ_INT(g_song.note_count, 2);
    CHECK_EQ_INT(g_song.notes[1].start, 24);
    CHECK_EQ_INT(g_song.notes[1].pitch, 72);
    CHECK_EQ_INT(g_song.notes[1].voice, 1);
    remove(TMP_SONG);
}

static void test_parser_still_reads_the_first_file_format(void) {
    write_file("# old format\nbpm 100\n....0.1.2.\n 2 1 0 . \r\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    CHECK_EQ_INT(g_song.chart_count, 1);
    CHECK_STR_EQ(g_song.charts[0].name, "normal");
    CHECK_EQ_INT(g_song.steps, 1);
    CHECK_EQ_INT(g_song.charts[0].length, 14);
    CHECK_EQ_INT(g_song.charts[0].lane[4], 0);
    CHECK_EQ_INT(g_song.charts[0].lane[13], -1);
    remove(TMP_SONG);
}

static void test_parser_rejects_bad_files(void) {
    bd_Song keep;
    memset(&keep, 7, sizeof(keep));
    g_song = keep;
    const char *bad[] = {
        "bpm 500\n..0.1.2.\n",                              /* tempo out of range */
        "bpm 120\n..0.9.2.\n",                              /* lane 9 does not exist */
        "bpm 120\nchart x 3\n..0.3...\n",                   /* lane 3 on a 3-lane chart */
        "..0.1.2.1.0.1.2.\n",                               /* missing bpm */
        "bpm 120\n.0.\n",                                   /* chart too short */
        "bpm 120\nwhat is this\n..0.1.2.\n",                /* unknown directive */
        "bpm 120\nsteps 9\n..0.1.2.\n",                     /* steps out of range */
        "bpm 120\nchart a 3\n........\nsteps 4\n",          /* steps after a chart */
        "bpm 120\nchart a 1\n........\n",                   /* one lane is not a game */
        "bpm 120\nchart a 3\n........\nmelody\n1 2 3\n",    /* incomplete melody line */
        "bpm 120\nchart a 3\n........\nmelody\n0 4 10 0\n", /* pitch out of range */
        "bpm 120\nchart a 3\n........\nmelody\n0 4 60 9\n", /* voice out of range */
        "bpm 120\nchart a 3\nchart b 3\nchart c 3\nchart d 3\nchart e 3\nchart f 3\nchart g 3\n", /* 7 charts
                                                                                                   */
        /* new in format version 3 */
        "bpm 120\nchart a 3\na.......\n",            /* a ribbon letter as the first step of a chart */
        "bpm 120\nchart a 3\n...a....\n",            /* a ribbon letter with no bead before it */
        "bpm 120\nchart a 3\n0.d.....\n",            /* letter d = lane 3 on a 3-lane chart */
        "bpm 120\nchart a 3 hologram\n........\n",   /* unknown third word */
        "bpm 120\nchart a 3 catch more\n........\n", /* too many words */
        "bpm 120\nchart a catch\n........\n",        /* the lane count must be a number */
        "bpm 120\na.0.1.2.\n",                       /* a first-version file cannot start a ribbon */
    };
    for (size_t i = 0; i < ARC_ARRAY_LEN(bad); i++) {
        write_file(bad[i]);
        CHECK(!bd_song_load(TMP_SONG, &g_song));
    }
    CHECK(!bd_song_load("no_such_song.tmp", &g_song));
    CHECK(memcmp(&g_song, &keep, sizeof(keep)) == 0); /* a failed load leaves the output untouched */
    remove(TMP_SONG);
}

static void test_save_and_load_round_trip(void) {
    bd_song_default(&g_song, 3);
    g_song.steps = 2;
    arc_strlcpy(g_song.title, "Round Trip", sizeof(g_song.title));
    g_song.chart_count = 2;
    g_song.charts[1] = g_song.charts[0];
    arc_strlcpy(g_song.charts[1].name, "hard", sizeof(g_song.charts[1].name));
    g_song.charts[1].lanes = 4;
    g_song.charts[1].lane[10] = 3;
    g_song.note_count = 3;
    g_song.notes[0] = (bd_Note){0, 12, 60, 0};
    g_song.notes[1] = (bd_Note){12, 6, 64, 1};
    g_song.notes[2] = (bd_Note){600, 24, 72, 2};
    CHECK(bd_song_save(TMP_SONG, &g_song));
    CHECK(bd_song_load(TMP_SONG, &g_back));
    CHECK_STR_EQ(g_back.title, "Round Trip");
    CHECK_EQ_INT(g_back.bpm, g_song.bpm);
    CHECK_EQ_INT(g_back.steps, 2);
    CHECK_EQ_INT(g_back.chart_count, 2);
    for (int c = 0; c < 2; c++) {
        CHECK_EQ_INT(g_back.charts[c].lanes, g_song.charts[c].lanes);
        CHECK_EQ_INT(g_back.charts[c].length, g_song.charts[c].length);
        CHECK(memcmp(g_back.charts[c].lane, g_song.charts[c].lane, (size_t)g_song.charts[c].length) == 0);
    }
    CHECK_EQ_INT(g_back.note_count, 3);
    CHECK(memcmp(g_back.notes, g_song.notes, 3 * sizeof(bd_Note)) == 0);
    remove(TMP_SONG);
}

static void test_parser_v3_ribbons_and_catch_charts(void) {
    write_file("title V3\nbpm 150\nsteps 4\nchart normal 3\n0abc.1\n..b.\n"
               "chart hard 4 catch\n..0b..c.3d\nchart easy 3 dodge\n1.1.1.1.\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    CHECK_EQ_INT(g_song.chart_count, 3);
    const bd_Chart *d = &g_song.charts[0], *k = &g_song.charts[1], *e = &g_song.charts[2];
    CHECK_EQ_INT(d->kind, BD_CHART_DODGE);
    CHECK_EQ_INT(k->kind, BD_CHART_CATCH);
    CHECK_EQ_INT(e->kind, BD_CHART_DODGE); /* "dodge" is the default spelled out */
    CHECK_EQ_INT(d->length, 10);
    CHECK_EQ_INT(d->lane[1], 0); /* 'a' = lane 0, continues the object that starts at step 0 */
    CHECK_EQ_INT(d->link[1], 1);
    CHECK_EQ_INT(d->lane[3], 2);
    CHECK_EQ_INT(d->link[0], 0);
    CHECK_EQ_INT(d->link[5], 0); /* '1' starts a new object */
    CHECK_EQ_INT(d->lane[8], 1);
    CHECK_EQ_INT(d->link[8], 1); /* a rest at step 7 may sit inside a ribbon (fast songs have 2-step gaps) */
    CHECK_EQ_INT(k->lanes, 4);
    CHECK_EQ_INT(k->lane[2],
                 0); /* "..0b..c.3d": 0 starts, b and c continue (across a rest), 3 starts, d continues */
    CHECK_EQ_INT(k->link[2], 0);
    CHECK_EQ_INT(k->lane[3], 1);
    CHECK_EQ_INT(k->link[3], 1);
    CHECK_EQ_INT(k->lane[6], 2);
    CHECK_EQ_INT(k->link[6], 1);
    CHECK_EQ_INT(k->lane[8], 3);
    CHECK_EQ_INT(k->link[8], 0);
    CHECK_EQ_INT(k->lane[9], 3);
    CHECK_EQ_INT(k->link[9], 1);
    CHECK_EQ_INT(bd_chart_beads(d), 6);
    CHECK_EQ_INT(bd_chart_objects(d), 2); /* the ribbon 0abc, and the ribbon 1 . . b (steps 5 and 8) */
    CHECK_EQ_INT(bd_chart_objects(k), 2); /* 0bc and 3d */
    CHECK_EQ_INT(bd_chart_next_bead(d, 0), 1);
    CHECK_EQ_INT(bd_chart_next_bead(d, 5), 8);
    CHECK_EQ_INT(bd_chart_next_bead(d, 8), d->length);
    CHECK(bd_chart_in_ribbon(d, 0));  /* the head of a ribbon */
    CHECK(bd_chart_in_ribbon(d, 3));  /* the tail */
    CHECK(bd_chart_in_ribbon(d, 5));  /* a head whose ribbon continues after a rest */
    CHECK(!bd_chart_in_ribbon(e, 0)); /* a lone bead */
    CHECK(!bd_chart_in_ribbon(d, 4)); /* a rest is no bead */
    remove(TMP_SONG);
}

static void test_parser_v1_and_v2_files_have_no_ribbons_and_dodge_kind(void) {
    write_file("bpm 100\n..0.1.2.\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    CHECK_EQ_INT(g_song.charts[0].kind, BD_CHART_DODGE);
    CHECK_EQ_INT(bd_chart_objects(&g_song.charts[0]), bd_chart_beads(&g_song.charts[0]));
    write_file(
        "title Old\nbpm 120\nsteps 4\nchart easy 3\n..0.1.2.\nchart hard 4\n0.1.2.3.\nmelody\n0 12 60 0\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    for (int c = 0; c < 2; c++) {
        CHECK_EQ_INT(g_song.charts[c].kind, BD_CHART_DODGE);
        for (int i = 0; i < g_song.charts[c].length; i++)
            CHECK_EQ_INT(g_song.charts[c].link[i], 0);
    }
    remove(TMP_SONG);
}

static void test_ribbon_save_round_trip(void) {
    write_file("title Ribbons\nbpm 130\nsteps 4\nchart easy 3\n0ab.1.b.\nchart hard 4 catch\n0.b.c.d.\n");
    CHECK(bd_song_load(TMP_SONG, &g_song));
    CHECK(bd_song_save(TMP_SONG, &g_song));
    CHECK(bd_song_load(TMP_SONG, &g_back));
    CHECK_EQ_INT(g_back.chart_count, 2);
    for (int c = 0; c < 2; c++) {
        CHECK_EQ_INT(g_back.charts[c].kind, g_song.charts[c].kind);
        CHECK_EQ_INT(g_back.charts[c].length, g_song.charts[c].length);
        CHECK(memcmp(g_back.charts[c].lane, g_song.charts[c].lane, (size_t)g_song.charts[c].length) == 0);
        CHECK(memcmp(g_back.charts[c].link, g_song.charts[c].link, (size_t)g_song.charts[c].length) == 0);
    }
    CHECK_EQ_INT(g_back.charts[1].kind, BD_CHART_CATCH);
    remove(TMP_SONG);
}

/* ---- the reachable-lane check on hand-made charts. 120 BPM, 4 steps per beat: a step lasts 0.125 s, so a
 * human (0.12 s per lane step) manages 1 lane step between neighbouring steps. At 220 BPM a step lasts only
 * 0.068 s: no lane step fits between neighbouring steps. */
static void make_chart(bd_Song *song, int bpm, bd_ChartKind kind, int lanes, const char *pattern) {
    memset(song, 0, sizeof(*song));
    arc_strlcpy(song->title, "Hand made", sizeof(song->title));
    song->bpm = bpm;
    song->steps = 4;
    song->chart_count = 1;
    bd_Chart *c = &song->charts[0];
    arc_strlcpy(c->name, "normal", sizeof(c->name));
    c->kind = kind;
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

static void test_reach_steps_per_time(void) {
    CHECK_EQ_INT(bd_reach_steps(0.0f), 0);
    CHECK_EQ_INT(bd_reach_steps(0.119f), 0);
    CHECK_EQ_INT(bd_reach_steps(BD_HUMAN_STEP_SEC), 1);
    CHECK_EQ_INT(bd_reach_steps(0.24f), 2);
    CHECK_EQ_INT(bd_reach_steps(0.36f), 3); /* 0.36 / 0.12 is 2.9999 in floating point */
    CHECK_EQ_INT(bd_reach_steps(-1.0f), 0);
}

static void test_reach_dodge_playable(void) {
    int fail = -7;
    make_chart(&g_song, 120, BD_CHART_DODGE, 3, "....0...1...2...1...0...");
    CHECK(bd_chart_playable(&g_song, 0, &fail));
    CHECK_EQ_INT(fail, -7); /* untouched when the chart is playable */
    make_chart(&g_song, 120, BD_CHART_DODGE, 4, "..0ab.1.2.3.");
    CHECK(bd_chart_playable(&g_song, 0, NULL)); /* a ribbon over 3 of 4 lanes leaves one free lane */
}

static void test_reach_dodge_unplayable_when_a_ribbon_covers_every_lane(void) {
    int fail = -1;
    make_chart(&g_song, 220, BD_CHART_DODGE, 3, "..0bc....");
    CHECK(!bd_chart_playable(&g_song, 0, &fail));
    CHECK_EQ_INT(fail, 4); /* the bead in lane 2 takes the last free lane */
    make_chart(&g_song, 220, BD_CHART_DODGE, 3, "..0.......b.......c...");
    CHECK(bd_chart_playable(&g_song, 0,
                            NULL)); /* the same lanes, far apart in time: there is time to step aside */
}

static void test_reach_catch_unplayable_by_distance(void) {
    int fail = -1;
    make_chart(&g_song, 120, BD_CHART_CATCH, 3, "........02");
    CHECK(!bd_chart_playable(&g_song, 0, &fail));
    CHECK_EQ_INT(fail, 9); /* lane 0 then lane 2 one step (0.125 s) later: only one lane step fits */
    make_chart(&g_song, 120, BD_CHART_CATCH, 3, "........0.2");
    CHECK(bd_chart_playable(&g_song, 0, NULL)); /* two steps = 0.25 s = two lane steps */
    make_chart(&g_song, 220, BD_CHART_CATCH, 3, "........0bc.");
    CHECK(bd_chart_playable(&g_song, 0, NULL)); /* a ribbon is caught from the middle lane: tolerance 1 */
    make_chart(&g_song, 220, BD_CHART_CATCH, 3, "........0.2");
    CHECK(!bd_chart_playable(&g_song, 0, NULL)); /* two LONE beads 0.136 s apart, 2 lanes away: no way */
}

static void test_object_span_and_catch_tolerance(void) {
    make_chart(&g_song, 120, BD_CHART_CATCH, 4, "0.1a.b..2..3c.0bc");
    const bd_Chart *c = &g_song.charts[0];
    CHECK_EQ_INT(bd_chart_object_span(c, 0), 0);  /* a lone bead */
    CHECK_EQ_INT(bd_chart_object_span(c, 2), 1);  /* lane 1 then 0: the ribbon touches lanes 0..1 */
    CHECK_EQ_INT(bd_chart_object_span(c, 3), 1);  /* the same ribbon seen from its second bead */
    CHECK_EQ_INT(bd_chart_object_span(c, 5), 1);  /* ... and from its third (after a rest) */
    CHECK_EQ_INT(bd_chart_object_span(c, 4), 0);  /* a rest has no object */
    CHECK_EQ_INT(bd_chart_object_span(c, 8), 0);  /* a lone bead in lane 2 */
    CHECK_EQ_INT(bd_chart_object_span(c, 14), 2); /* lanes 0, 1, 2 */
    CHECK_EQ_INT(bd_chart_object_span(c, 16), 2);
    make_chart(&g_song, 120, BD_CHART_CATCH, 3, "0a.a");
    CHECK_EQ_INT(bd_chart_object_span(&g_song.charts[0], 3), 0); /* a ribbon that stays in one lane */
    CHECK_EQ_INT(bd_catch_tolerance(0), 0);
    CHECK_EQ_INT(bd_catch_tolerance(1), 1);
    CHECK_EQ_INT(bd_catch_tolerance(2), 1);
    /* a ribbon in ONE lane needs that exact lane, a ribbon that changes lane is caught from its middle: at
     * 220 BPM (steps of 0.068 s) the next bead, two lanes away, is only reachable from the second ribbon */
    make_chart(&g_song, 220, BD_CHART_CATCH, 3, "........0a.2");
    CHECK(!bd_chart_playable(&g_song, 0, NULL));
    make_chart(&g_song, 220, BD_CHART_CATCH, 3, "........0b.2");
    CHECK(bd_chart_playable(&g_song, 0, NULL));
}

static void test_plan_is_a_path_a_human_can_follow(void) {
    static int8_t plan[BD_MAX_STEPS];
    make_chart(&g_song, 120, BD_CHART_DODGE, 3, "....0.1.2.1ab.0...2.1.");
    CHECK(bd_chart_plan(&g_song, 0, plan));
    const bd_Chart *c = &g_song.charts[0];
    float step_sec = 60.0f / 120.0f / 4.0f, lead = BD_LEAD_BEATS * 0.5f;
    int before = -1;
    for (int i = 0; i < c->length; i++) {
        if (c->lane[i] < 0) {
            CHECK_EQ_INT(plan[i], -1);
            continue;
        }
        CHECK(plan[i] >= 0 && plan[i] < c->lanes);
        CHECK(plan[i] != c->lane[i]); /* dodge: never on the bead's lane */
        int reach = before < 0 ? bd_reach_steps(lead + (float)i * step_sec)
                               : bd_reach_steps((float)(i - before) * step_sec);
        int from = before < 0 ? c->lanes / 2 : plan[before];
        CHECK(abs(plan[i] - from) <= reach);
        before = i;
    }
    make_chart(&g_song, 120, BD_CHART_CATCH, 4, "......0..3...1b.c...0");
    CHECK(bd_chart_plan(&g_song, 0, plan));
    CHECK(abs(plan[6] - 0) == 0);
    CHECK_EQ_INT(plan[9], 3);
    CHECK(abs(plan[13] - g_song.charts[0].lane[13]) <=
          1); /* a bead of a ribbon that changes lane: tolerance 1 */
    CHECK(abs(plan[16] - g_song.charts[0].lane[16]) <= 1);
    make_chart(&g_song, 220, BD_CHART_DODGE, 3, "..0bc....");
    CHECK(!bd_chart_plan(&g_song, 0, plan));
}

static void test_obstacles_arrive_exactly_on_the_beat(void) {
    bd_song_default(&g_song, 1);
    g_song.steps = 4; /* 16th-note steps */
    g_song.bpm = 120;
    for (int i = 0; i < g_song.charts[0].length; i++)
        g_song.charts[0].lane[i] = -1;
    g_song.charts[0].lane[10] = 1; /* in the player's starting lane */
    beat_dodge_use_song(&g_s, &g_song, 0);
    float arrive = BD_LEAD_BEATS * 0.5f + 10.0f * (0.5f / 4.0f); /* lead + 10 steps of 0.125 s */
    CHECK_NEAR(beat_dodge_arrival(&g_s, 10), arrive, 0.0001);
    ArcadeInput in = idle();
    while (g_s.run.time < arrive - 0.02f)
        beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES);
    CHECK_NEAR(beat_dodge_obstacle_y(&g_s, 10), BD_PLAYER_Y, 12.0);
    while (g_s.run.time < arrive + 0.03f)
        beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES - 1);
}

/* A song made from a pattern, started on its chart 0; `bpm` 120 gives 16th steps of 0.125 s. */
static void start_chart(int lanes, const char *pattern) {
    make_chart(&g_song, 120, BD_CHART_DODGE, lanes, pattern);
    beat_dodge_use_song(&g_s, &g_song, 0);
}

/* Steps the game without any input until `seconds` after the arrival of step `step`. */
static void run_past(int step, float seconds) {
    ArcadeInput in = idle();
    float until = bd_run_arrival(&g_s.run, step) + seconds;
    while (g_s.run.time < until && !beat_dodge_is_over(&g_s))
        beat_dodge_step(&g_s, &in, ARC_DT);
}

static void test_perfect_play_never_loses_a_life(void) {
    beat_dodge_init(&g_s, 1);
    play_perfectly(&g_s, 60 * 90);
    CHECK(g_s.run.won);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES);
    CHECK_EQ_INT(g_s.hits, 0);
    CHECK_EQ_INT(g_s.dodged, bd_chart_objects(&g_s.run.song.charts[0]));
    CHECK(g_s.points > 0 && g_s.points <= g_s.max_points);
}

static void test_a_ribbon_costs_exactly_one_life(void) {
    start_chart(3, "........1bbb"); /* a bar of four beads in the middle lane, where the player stands */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES - 1); /* not four lives */
    CHECK_EQ_INT(g_s.hits, 1);
    CHECK_EQ_INT(g_s.dodged, 0);
    start_chart(3, "........0b.b"); /* lane 0, then two beads in lane 1: the player dodges only the first */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES - 1);
    CHECK_EQ_INT(g_s.dodged, 0); /* a hit object does not count as dodged and pays nothing */
    CHECK_EQ_INT(g_s.points, 0);
    start_chart(3, "........1b.....1b"); /* two ribbons: two lives */
    run_past(17, 0.1f);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES - 2);
}

static void test_the_next_object_is_judged_again_after_a_hit(void) {
    start_chart(3, "........1b.1"); /* ribbon, then a lone bead 0.25 s later in the same lane */
    run_past(11, 0.1f);
    CHECK_EQ_INT(g_s.lives, BD_START_LIVES - 2); /* the lone bead is a new object: a second life */
}

static void test_the_close_call_window(void) {
    /* a lone bead in lane 0; the player waits there and leaves `before` seconds before it arrives */
    const float before[2] = {0.20f, 0.50f}; /* inside / outside BD_CLOSE_WINDOW_SEC (0.30 s) */
    for (int k = 0; k < 2; k++) {
        start_chart(3, "..............0");
        float arrival = bd_run_arrival(&g_s.run, 14);
        while (!beat_dodge_is_over(&g_s) && g_s.run.time < arrival + 0.1f) {
            ArcadeInput in = idle();
            in.typed = g_s.run.time < arrival - before[k] ? '1' : '3'; /* lane 0 until then, lane 2 after */
            beat_dodge_step(&g_s, &in, ARC_DT);
        }
        CHECK_EQ_INT(g_s.hits, 0);
        CHECK_EQ_INT(g_s.dodged, 1);
        CHECK_EQ_INT(g_s.close_calls, k == 0 ? 1 : 0);
        CHECK_EQ_INT(g_s.points, k == 0 ? 20 : 10);
    }
}

static void test_close_call_bot_scores_the_maximum(void) {
    /* lone beads and ribbons, always at least half a second apart, in 4 lanes */
    start_chart(4, "........0.......1.......2b.c....3.......0a.......1.......3...2");
    play_close_calls(&g_s, 60 * 30);
    CHECK(g_s.run.won);
    CHECK_EQ_INT(g_s.hits, 0);
    CHECK_EQ_INT(g_s.close_calls, g_s.dodged);
    CHECK_EQ_INT(g_s.dodged, bd_chart_objects(&g_song.charts[0]));
    CHECK_EQ_INT(g_s.points, g_s.max_points);
    CHECK_EQ_INT(beat_dodge_score(&g_s), 1500); /* 1000 x the "normal" multiplier 1.5: the ceiling */
}

static void test_a_camper_scores_about_half(void) {
    char pattern[260];
    int n = 0;
    for (int i = 0; i < 60; i++) { /* 60 lone beads, 0.5 s apart, only in lanes 0..2 */
        pattern[n++] = (char)('0' + i % 3);
        pattern[n++] = '.';
        pattern[n++] = '.';
        pattern[n++] = '.';
    }
    pattern[n] = '\0';
    start_chart(4, pattern);
    for (int i = 0; i < 60 * 60 && !beat_dodge_is_over(&g_s); i++) {
        ArcadeInput in = idle();
        in.typed = '4'; /* lane 3 is never used by the chart: stand there and never be close */
        beat_dodge_step(&g_s, &in, ARC_DT);
    }
    CHECK(g_s.run.won);
    CHECK_EQ_INT(g_s.hits, 0);
    CHECK_EQ_INT(g_s.close_calls, 0);
    float ratio = (float)g_s.points / (float)g_s.max_points;
    CHECK(ratio > 0.5f &&
          ratio < 0.62f); /* clean objects pay half of close calls, the life bonus is the rest */
    CHECK(beat_dodge_score(&g_s) < 1500 * 62 / 100);
}

static void test_the_human_speed_bot_clears_every_shipped_dodge_chart(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    static int8_t plan[BD_MAX_STEPS];
    int played = 0;
    for (int i = 0; i < n; i++)
        for (int c = 0; c < g_list.songs[i].chart_count; c++) {
            if (g_list.songs[i].charts[c].kind != BD_CHART_DODGE)
                continue;
            CHECK(bd_chart_plan(&g_list.songs[i], c, plan));
            beat_dodge_use_song(&g_s, &g_list.songs[i], c);
            play_the_plan(&g_s, plan, 60 * 400);
            CHECK(g_s.run.won);
            CHECK_EQ_INT(g_s.hits, 0);
            CHECK_EQ_INT(g_s.lives, BD_START_LIVES);
            played++;
        }
    CHECK(played >= 3);
}

static void test_the_teleport_bot_is_never_hit_on_shipped_charts(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    for (int i = 0; i < n; i++)
        for (int c = 0; c < g_list.songs[i].chart_count; c++) {
            if (g_list.songs[i].charts[c].kind != BD_CHART_DODGE)
                continue;
            beat_dodge_use_song(&g_s, &g_list.songs[i], c);
            play_close_calls(&g_s, 60 * 400);
            CHECK(g_s.run.won);
            CHECK_EQ_INT(g_s.hits, 0);
            CHECK(g_s.points <= g_s.max_points);
            CHECK(g_s.close_calls > g_s.dodged / 2); /* waiting in the lane of the next object is close */
        }
}

static void test_standing_still_loses_all_lives(void) {
    beat_dodge_init(&g_s, 1);
    ArcadeInput in = idle();
    for (int i = 0; i < 60 * 90 && !beat_dodge_is_over(&g_s); i++)
        beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK(beat_dodge_is_over(&g_s));
    CHECK(!g_s.run.won);
    CHECK_EQ_INT(g_s.lives, 0);
    CHECK(beat_dodge_score(&g_s) < 600);
}

static void test_lanes_keys_and_clamping(void) {
    bd_song_default(&g_song, 1);
    g_song.chart_count = 1;
    g_song.charts[0].lanes = 4;
    beat_dodge_use_song(&g_s, &g_song, 0);
    CHECK_EQ_INT(g_s.run.lanes, 4);
    ArcadeInput in = idle();
    in.typed = '4'; /* direct lane key */
    beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.run.lane, 3);
    in = idle();
    in.typed = '5'; /* no such lane: ignored */
    beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.run.lane, 3);
    in = idle();
    in.right_pressed = true;
    beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.run.lane, 3); /* clamped at the right edge */
    in = idle();
    in.typed = '1';
    beat_dodge_step(&g_s, &in, ARC_DT);
    in = idle();
    in.left_pressed = true;
    beat_dodge_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.run.lane, 0);
}

static void test_sync_only_fixes_large_drift(void) {
    beat_dodge_init(&g_s, 1);
    ArcadeInput in = idle();
    for (int i = 0; i < 60; i++)
        beat_dodge_step(&g_s, &in, ARC_DT);
    float t = g_s.run.time;
    beat_dodge_sync(&g_s, t + 0.05f); /* small drift: ignored */
    CHECK_NEAR(g_s.run.time, t, 0.0001);
    beat_dodge_sync(&g_s, t + 0.5f); /* big drift: follow the clock */
    CHECK_NEAR(g_s.run.time, t + 0.5f, 0.0001);
}

static void test_beat_event_fires_once_per_beat(void) {
    beat_dodge_init(&g_s, 1);
    ArcadeInput in = idle();
    int events = 0;
    for (int i = 0; i < 120; i++) { /* 2 seconds at 120 bpm = 4 beats (plus the very first one at t=0) */
        beat_dodge_step(&g_s, &in, ARC_DT);
        if (g_s.run.beat_event)
            events++;
    }
    CHECK(events >= 4 && events <= 5);
}

static void test_song_to_synth_uses_beats_not_seconds(void) {
    make_chart(&g_song, 120, BD_CHART_DODGE, 3, "0...1");
    g_song.note_count = 2;                      /* 0.5 s per beat */
    g_song.notes[0] = (bd_Note){0, 24, 60, 0};  /* beat 0, one beat long: step 0 holds a bead */
    g_song.notes[1] = (bd_Note){96, 12, 72, 1}; /* beat 4 (step 16): no bead there */
    static SynthNote out[BD_MAX_NOTES];
    CHECK_EQ_INT(bd_song_to_synth(&g_song, 0, 1.5f, out), 2);
    CHECK_NEAR(out[0].start, 1.5, 0.0001); /* shifted by the lead-in */
    CHECK_NEAR(out[0].dur, 0.5, 0.0001);
    CHECK_NEAR(out[1].start, 3.5, 0.0001);
    CHECK_NEAR(out[1].dur, 0.25, 0.0001);
    CHECK_EQ_INT(out[1].pitch, 72);
}

static void test_notes_with_a_bead_are_lead_and_the_rest_is_backing(void) {
    make_chart(&g_song, 120, BD_CHART_DODGE, 3, "0..1.2"); /* beads on steps 0, 3 and 5 */
    g_song.note_count = 5;
    g_song.notes[0] = (bd_Note){0, 6, 60, 1};  /* step 0: bead */
    g_song.notes[1] = (bd_Note){6, 6, 62, 0};  /* step 1: no bead (opponent voice) */
    g_song.notes[2] = (bd_Note){18, 6, 64, 0}; /* step 3: bead (opponent voice is lead too) */
    g_song.notes[3] = (bd_Note){18, 6, 67, 1}; /* step 3 again: two notes on one step are both lead */
    g_song.notes[4] = (bd_Note){60, 6, 65, 1}; /* step 10: beyond the chart's beads */
    static SynthNote out[BD_MAX_NOTES];
    CHECK_EQ_INT(bd_song_to_synth(&g_song, 0, 0.0f, out), 5);
    CHECK_EQ_INT(out[0].voice, 0);
    CHECK_EQ_INT(out[1].voice, 3);
    CHECK_EQ_INT(out[2].voice, 0);
    CHECK_EQ_INT(out[3].voice, 0);
    CHECK_EQ_INT(out[4].voice, 3);
}

static void test_every_lead_note_has_a_bead_in_every_shipped_chart(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    static SynthNote out[BD_MAX_NOTES];
    for (int i = 0; i < n; i++) {
        const bd_Song *song = &g_list.songs[i];
        for (int c = 0; c < song->chart_count; c++) {
            const bd_Chart *ch = &song->charts[c];
            int count = bd_song_to_synth(song, c, 0.0f, out), lead = 0;
            CHECK_EQ_INT(count, song->note_count);
            for (int k = 0; k < count; k++) {
                int step = bd_note_step(song->notes[k].start, song->steps);
                bool bead = step < ch->length && ch->lane[step] >= 0;
                CHECK_EQ_INT(out[k].voice == 0, bead);         /* lead exactly when there is a bead ... */
                CHECK(out[k].voice == 0 || out[k].voice == 3); /* ... and backing otherwise */
                lead += bead ? 1 : 0;
            }
            if (ch->kind == BD_CHART_DODGE && song->note_count > 0)
                CHECK(lead * 2 > count); /* dodge charts show most of what is heard */
        }
    }
}

static void test_menu(void) {
    memset(&g_list, 0, sizeof(g_list));
    g_list.count = 2;
    bd_song_default(&g_list.songs[0], 1);
    bd_song_default(&g_list.songs[1], 2);
    g_list.songs[1].chart_count = 3;
    for (int c = 1; c < 3; c++)
        g_list.songs[1].charts[c] = g_list.songs[1].charts[0];
    bd_Menu m;
    bd_menu_init(&m, BD_CHART_DODGE);
    ArcadeInput in = idle();
    in.down_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.song, 1);
    CHECK_EQ_INT(m.chart, 1); /* a new song starts on its second chart */
    in = idle();
    in.right_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.chart, 2); /* clamped to the last chart */
    in = idle();
    in.down_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.song, 0);  /* wraps around */
    CHECK_EQ_INT(m.chart, 0); /* song 0 has only one chart */
    CHECK(!m.chosen);
    in = idle();
    in.action_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    CHECK(m.chosen);
}

static void test_menu_only_offers_charts_of_its_own_kind(void) {
    memset(&g_list, 0, sizeof(g_list));
    g_list.count = 3;
    bd_song_default(&g_list.songs[0], 1); /* dodge only: a catch menu skips it */
    for (int k = 1; k < 3; k++) {
        bd_Song *song = &g_list.songs[k];
        bd_song_default(song, (unsigned)k);
        song->chart_count = 6; /* easy, normal, hard of each kind */
        for (int c = 1; c < 6; c++)
            song->charts[c] = song->charts[0];
        for (int c = 0; c < 6; c++)
            song->charts[c].kind = c < 3 ? BD_CHART_DODGE : BD_CHART_CATCH;
    }
    bd_Menu m;
    ArcadeInput none = idle();
    bd_menu_init(&m, BD_CHART_CATCH);
    bd_menu_step(&m, &none, &g_list); /* starts on song 0, which has no catch chart: moves on */
    CHECK_EQ_INT(m.song, 1);
    CHECK_EQ_INT(m.chart, 4); /* the second catch chart */
    ArcadeInput in = idle();
    in.left_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.chart, 3); /* never into the dodge charts 0..2 */
    in = idle();
    in.right_pressed = true;
    for (int i = 0; i < 4; i++)
        bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.chart, 5);
    in = idle();
    in.down_pressed = true;
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.song, 2);
    bd_menu_step(&m, &in, &g_list);
    CHECK_EQ_INT(m.song, 1); /* song 0 is skipped on the way round */
    bd_menu_init(&m, BD_CHART_DODGE);
    bd_menu_step(&m, &none, &g_list);
    CHECK_EQ_INT(m.song, 0);
    CHECK_EQ_INT(m.chart, 0);
    bd_menu_move_song(&m, &g_list, 1);
    CHECK_EQ_INT(m.chart, 1);           /* the second dodge chart */
    bd_menu_pick_chart(&m, &g_list, 4); /* a catch chart: refused */
    CHECK_EQ_INT(m.chart, 1);
    bd_menu_pick_chart(&m, &g_list, 2);
    CHECK_EQ_INT(m.chart, 2);
}

/* Every shipped song must load and be playable: lanes in range, sensible density, and a perfect run wins. */
static void test_every_shipped_song_is_playable(void) {
    int n = bd_songlist_load(ARCADE_ASSETS_DIR "/beatmaps", &g_list);
    CHECK(n >= 1);                   /* at least default.txt */
    CHECK_EQ_INT(g_list.skipped, 0); /* nothing listed in songs.txt may be broken */
    static int8_t plan[BD_MAX_STEPS];
    float empty5_sum = 0.0f;
    int dodge_charts = 0;
    for (int i = 0; i < n; i++) {
        const bd_Song *song = &g_list.songs[i];
        CHECK(song->chart_count >= 1);
        for (int c = 0; c < song->chart_count; c++) {
            const bd_Chart *ch = &song->charts[c];
            CHECK(bd_chart_beads(ch) >= 20);
            CHECK(bd_song_seconds(song, c) < 300.0f);
            for (int k = 0; k < ch->length; k++)
                CHECK(ch->lane[k] < ch->lanes);      /* -1 (rest) or a real lane */
            CHECK(bd_chart_playable(song, c, NULL)); /* the player always has somewhere safe to stand ... */
            CHECK(bd_chart_plan(song, c, plan));     /* ... and a path a human can walk */
            bd_ChartMetrics m;
            bd_chart_metrics(song, c, &m);
            CHECK_EQ_INT(m.bursts_mismatched, 0); /* no run of rapid notes without its ribbon */
            if (ch->kind == BD_CHART_DODGE && song->note_count > 100) {
                empty5_sum += m.empty5;
                dodge_charts++;
                CHECK(m.longest_idle < 40.0f); /* no lane is ignored for most of a song */
            }
        }
    }
    /* anti-camping: before ribbons 57% of the 5 s windows (mean of the three difficulties) had an empty lane
     */
    CHECK(dodge_charts > 0);
    CHECK(empty5_sum / (float)dodge_charts < 0.45f);
}

static void test_monkey_is_safe(void) {
    beat_dodge_init(&g_s, 2);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 7);
    for (int i = 0; i < 5000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        beat_dodge_step(&g_s, &in, ARC_DT);
        CHECK(g_s.run.lane >= 0 && g_s.run.lane < g_s.run.lanes);
        CHECK(g_s.lives >= 0 && g_s.lives <= BD_START_LIVES);
        CHECK(beat_dodge_score(&g_s) >= 0);
    }
    /* a real song full of ribbons: the invariants of the scoring hold for any input */
    CHECK(bd_song_load(ARCADE_ASSETS_DIR "/beatmaps/fnf/thorns.txt", &g_song));
    for (int c = 0; c < g_song.chart_count; c++) {
        if (g_song.charts[c].kind != BD_CHART_DODGE)
            continue;
        beat_dodge_use_song(&g_s, &g_song, c);
        arcade_monkey_init(&m, (uint64_t)(11 + c));
        for (int i = 0; i < 60 * 200 && !beat_dodge_is_over(&g_s); i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            beat_dodge_step(&g_s, &in, ARC_DT);
            CHECK(g_s.run.lane >= 0 && g_s.run.lane < g_s.run.lanes);
            CHECK(g_s.lives >= 0 && g_s.lives <= BD_START_LIVES);
            CHECK(g_s.points >= 0 && g_s.points <= g_s.max_points);
            CHECK(g_s.close_calls <= g_s.dodged);
            CHECK_EQ_INT(g_s.hits, BD_START_LIVES - g_s.lives);
        }
        CHECK(beat_dodge_score(&g_s) <= (int)(1000.0f * g_s.multiplier));
    }
}

int main(void) {
    RUN(test_default_song_is_valid);
    RUN(test_parser_v2);
    RUN(test_parser_still_reads_the_first_file_format);
    RUN(test_parser_rejects_bad_files);
    RUN(test_save_and_load_round_trip);
    RUN(test_parser_v3_ribbons_and_catch_charts);
    RUN(test_parser_v1_and_v2_files_have_no_ribbons_and_dodge_kind);
    RUN(test_ribbon_save_round_trip);
    RUN(test_reach_steps_per_time);
    RUN(test_reach_dodge_playable);
    RUN(test_reach_dodge_unplayable_when_a_ribbon_covers_every_lane);
    RUN(test_reach_catch_unplayable_by_distance);
    RUN(test_object_span_and_catch_tolerance);
    RUN(test_plan_is_a_path_a_human_can_follow);
    RUN(test_obstacles_arrive_exactly_on_the_beat);
    RUN(test_perfect_play_never_loses_a_life);
    RUN(test_a_ribbon_costs_exactly_one_life);
    RUN(test_the_next_object_is_judged_again_after_a_hit);
    RUN(test_the_close_call_window);
    RUN(test_close_call_bot_scores_the_maximum);
    RUN(test_a_camper_scores_about_half);
    RUN(test_the_human_speed_bot_clears_every_shipped_dodge_chart);
    RUN(test_the_teleport_bot_is_never_hit_on_shipped_charts);
    RUN(test_standing_still_loses_all_lives);
    RUN(test_lanes_keys_and_clamping);
    RUN(test_sync_only_fixes_large_drift);
    RUN(test_beat_event_fires_once_per_beat);
    RUN(test_song_to_synth_uses_beats_not_seconds);
    RUN(test_notes_with_a_bead_are_lead_and_the_rest_is_backing);
    RUN(test_every_lead_note_has_a_bead_in_every_shipped_chart);
    RUN(test_menu);
    RUN(test_menu_only_offers_charts_of_its_own_kind);
    RUN(test_every_shipped_song_is_playable);
    RUN(test_monkey_is_safe);
    return TEST_SUMMARY();
}
