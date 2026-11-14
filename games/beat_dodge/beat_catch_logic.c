#include "beat_catch_logic.h"

#include <stdlib.h>

/* All scoring numbers live in these three functions so the live game and the "perfect run" that defines
 * max_points can never disagree. */
int beat_catch_combo_multiplier(int combo) {
    return 1 + arc_mini(combo / BC_COMBO_STEP, BC_MAX_COMBO_BONUS);
}

static int award_bead(bool in_ribbon,
                      int combo) { /* a caught bead; `combo` = beads caught in a row before it */
    return (in_ribbon ? BC_RIBBON_POINTS : BC_LONE_POINTS) * beat_catch_combo_multiplier(combo);
}

static int award_finish(int health) { /* bonus for surviving the whole song */
    return health * BC_FINISH_PER_HEALTH;
}

static void reset_rules(beat_catch_State *s) { /* everything except the shared run, which bd_run_start did */
    const bd_Chart *c = &s->run.song.charts[s->run.chart];
    s->health = BC_START_HEALTH;
    s->combo = s->best_combo = 0;
    s->caught = s->missed = s->missed_objects = s->points = 0;
    s->multiplier = bd_chart_multiplier(c->name);
    s->obj_missed = false;
    s->catch_flash = 0.0f;
    s->catch_lane = 0;
    /* a perfect run: every bead caught, health never below the start. It is replayed through the same award
     * functions as the real game. Humans land below it: the maximum is a ceiling, not a target. */
    s->max_points = award_finish(BC_START_HEALTH);
    int combo = 0;
    for (int i = 0; i < c->length; i++)
        if (c->lane[i] >= 0)
            s->max_points += award_bead(bd_chart_in_ribbon(c, i), combo++);
}

void beat_catch_init(beat_catch_State *s, unsigned seed) {
    bd_song_default(&s->run.song, seed);
    s->run.song.charts[0].kind = BD_CHART_CATCH; /* the practice song is a catch chart here */
    bd_run_start(&s->run, &s->run.song, 0);
    reset_rules(s);
}

void beat_catch_use_song(beat_catch_State *s, const bd_Song *song, int chart) {
    bd_run_start(&s->run, song, chart);
    reset_rules(s);
}

/* Judges the bead of `step`, which has just reached the player line. */
static void judge_bead(beat_catch_State *s, const bd_Chart *c, int step) {
    bd_Run *r = &s->run;
    if (!c->link[step]) /* a new object starts: forget the last one */
        s->obj_missed = false;
    bool in_ribbon = bd_chart_in_ribbon(c, step);
    int tolerance = bd_catch_tolerance(bd_chart_object_span(c, step));
    if (abs(r->lane - c->lane[step]) <= tolerance) { /* caught */
        s->points += award_bead(in_ribbon, s->combo);
        s->combo++;
        s->best_combo = arc_maxi(s->best_combo, s->combo);
        s->caught++;
        s->catch_lane = c->lane[step];
        s->catch_flash = 0.3f;
    } else {          /* missed */
        s->combo = 0; /* any missed bead breaks the combo */
        s->missed++;
        r->hit_flash = 0.35f;
        if (!s->obj_missed) { /* one penalty per object, at its first missed bead */
            s->obj_missed = true;
            s->missed_objects++;
            s->health = arc_maxi(0, s->health - BC_HEALTH_PER_MISS);
            if (s->health == 0)
                r->over = true;
        }
    }
    /* the last bead of an object that had no miss pays the health back. Health is counted per OBJECT, not
     * per bead: a ribbon of six beads must not heal six times as much as a lone bead. */
    int next = bd_chart_next_bead(c, step);
    bool last_of_object = next >= c->length || !c->link[next];
    if (last_of_object && !s->obj_missed)
        s->health = arc_mini(BC_START_HEALTH, s->health + BC_HEALTH_PER_CLEAN_OBJECT);
}

void beat_catch_step(beat_catch_State *s, const ArcadeInput *in, float dt) {
    bd_Run *r = &s->run;
    if (r->over)
        return;
    bd_run_tick(r, in, dt);
    s->catch_flash = arc_maxf(0.0f, s->catch_flash - dt);

    /* resolve every bead that has now arrived at the player line */
    const bd_Chart *c = &r->song.charts[r->chart];
    while (r->next_hit < r->length && r->time >= bd_run_arrival(r, r->next_hit) && !r->over) {
        if (c->lane[r->next_hit] >= 0)
            judge_bead(s, c, r->next_hit);
        r->next_hit++;
    }
    if (!r->over && r->time >= r->end_time) { /* survived the whole song */
        r->won = true;
        r->over = true;
        s->points += award_finish(s->health);
    }
}

void beat_catch_sync(beat_catch_State *s, float clock_seconds) {
    bd_run_sync(&s->run, clock_seconds);
}

bool beat_catch_is_over(const beat_catch_State *s) {
    return s->run.over;
}

int beat_catch_score(const beat_catch_State *s) {
    if (s->max_points <= 0)
        return 0;
    long long ceiling = (long long)(1000.0f * s->multiplier + 0.5f); /* 1000, 1500 or 2000 */
    return (int)(ceiling * s->points /
                 s->max_points); /* integer maths: a perfect run is exactly the ceiling */
}
