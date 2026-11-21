#include <string.h>

#include "arcade_test.h"
#include "ghost_loop_logic.h"

/* ---- driving the game like a player -------------------------------------------------------------------- */

static ArcadeInput input_toward(Vec2 from, Vec2 to) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.move_x = to.x - from.x > 6.0f ? 1.0f : (to.x - from.x < -6.0f ? -1.0f : 0.0f);
    in.move_y = to.y - from.y > 6.0f ? 1.0f : (to.y - from.y < -6.0f ? -1.0f : 0.0f);
    return in;
}

/* Steers toward 'target' until the player is within 9 px (input_toward stops steering once both axes are
 * within 6 px, which can leave the player up to 8.5 px away), the round ends or the level is cleared. */
static void walk_to(ghost_loop_State *s, Vec2 target) {
    int round = s->round, level = s->level;
    for (int i = 0; i < GL_ROUND_STEPS && s->round == round && s->level == level && !s->cleared; i++) {
        if (vec2_dist(s->player, target) <= 9.0f)
            return;
        ArcadeInput in = input_toward(s->player, target);
        ghost_loop_step(s, &in, ARC_DT);
    }
}

static void stand_still(ghost_loop_State *s, int steps) {
    int round = s->round;
    ArcadeInput in;
    arcade_input_clear(&in);
    for (int i = 0; i < steps && s->round == round && !s->cleared; i++)
        ghost_loop_step(s, &in, ARC_DT);
}

static void press_space(ghost_loop_State *s) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.action_pressed = true;
    ghost_loop_step(s, &in, ARC_DT);
}

static void press_key(ghost_loop_State *s, char key) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.typed = key;
    ghost_loop_step(s, &in, ARC_DT);
}

/* ---- the intended solutions ----------------------------------------------------------------------------- *
 * One Route per round: walk to each point in turn, standing still 'wait' steps once there. A wait of RELAY
 * means "stand there as long as the test's relay hold says". Every round except the last ends with SPACE, so
 * that run becomes a ghost. The last round has to end with the player in the exit. */
#define RELAY (-1)
#define MAX_POINTS 6

typedef struct Point {
    float x, y;
    int wait;
} Point;

typedef struct Route {
    int count;
    Point pts[MAX_POINTS];
} Route;

static const Route k_solution[GL_LEVEL_COUNT][GL_MAX_GHOSTS + 1] = {
    /* 1 HOLD THE DOOR: a ghost on the pad, then walk through */
    {{1, {{320, 180, 0}}}, {1, {{1000, 380, 0}}}},
    /* 2 CHAIN REACTION: ghost 1 opens the first door, ghost 2 walks through it to the second pad */
    {{1, {{300, 560, 0}}}, {2, {{435, 380, 0}, {620, 180, 0}}}, {1, {{1040, 380, 0}}}},
    /* 3 VENN: one ghost stands where the two rings overlap */
    {{1, {{352, 200, 0}}}, {1, {{1040, 380, 0}}}},
    /* 4 RELAY: the ghost holds pad A for a while, then walks to pad B */
    {{2, {{300, 150, RELAY}, {300, 610, 0}}}, {1, {{1060, 380, 0}}}},
    /* 5 THE LONG WAY: the ghost makes the long trip, the player waits under the door */
    {{5, {{350, 620, 0}, {480, 490, 0}, {480, 215, 0}, {600, 215, 0}, {900, 420, 0}}},
     {2, {{220, 440, 0}, {220, 260, 0}}}},
    /* 6 GRAND CENTRAL: ghost 1 relays A -> B, ghost 2 walks through door A to pad C */
    {{2, {{300, 150, RELAY}, {300, 610, 0}}}, {2, {{495, 380, 0}, {640, 150, 0}}}, {1, {{1100, 380, 0}}}}};

/* Plays one round of a solution. 'delay' idle steps are inserted before a ghost-making round starts moving
 * (a slow human). Returns after SPACE (ghost rounds) or when the level is cleared (last round). */
static void play_route(ghost_loop_State *s, const Route *r, bool last_round, int relay_hold, int delay) {
    int round = s->round;
    if (!last_round)
        stand_still(s, delay);
    for (int i = 0; i < r->count; i++) {
        walk_to(s, vec2(r->pts[i].x, r->pts[i].y));
        stand_still(s, r->pts[i].wait == RELAY ? relay_hold : r->pts[i].wait);
    }
    if (last_round) {
        ArcadeInput idle;
        arcade_input_clear(&idle);
        for (int i = 0; i < GL_ROUND_STEPS && s->round == round && !s->cleared; i++)
            ghost_loop_step(s, &idle, ARC_DT);
    } else if (s->round == round) {
        press_space(s);
    }
}

/* Plays the intended solution of the level the state is on. Returns with the level cleared (or not). */
static void play_solution(ghost_loop_State *s, int relay_hold, int delay) {
    int rounds = ghost_loop_level(s->level)->par_ghosts + 1;
    for (int r = 0; r < rounds; r++)
        play_route(s, &k_solution[s->level][r], r == rounds - 1, relay_hold, delay);
}

/* ---- the level table -------------------------------------------------------------------------------------
 */

static float dist_point_rect(Vec2 p, gl_Rect r) {
    float cx = arc_clampf(p.x, r.x0, r.x1), cy = arc_clampf(p.y, r.y0, r.y1);
    return vec2_dist(p, vec2(cx, cy));
}

static void test_every_level_is_well_formed(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        const gl_Level *lv = ghost_loop_level(i);
        CHECK(lv->name[0] != '\0');
        CHECK(lv->hint[0] != '\0');
        CHECK(lv->max_ghosts >= 1 && lv->max_ghosts <= GL_MAX_GHOSTS);
        CHECK(lv->par_ghosts >= 1 && lv->par_ghosts <= lv->max_ghosts);
        CHECK(lv->pad_count >= 1 && lv->pad_count <= GL_MAX_PADS);
        CHECK(lv->door_count >= 1 && lv->door_count <= GL_MAX_DOORS);
        CHECK(lv->wall_count <= GL_MAX_WALLS);
        for (int d = 0; d < lv->door_count; d++)
            CHECK(lv->doors[d].pad >= 0 && lv->doors[d].pad < lv->pad_count);
        /* start and exit are inside the room, with room for the actor to stand */
        CHECK(lv->start.x > GL_ROOM_X0 + GL_R && lv->start.x < GL_ROOM_X1 - GL_R);
        CHECK(lv->start.y > GL_ROOM_Y0 + GL_R && lv->start.y < GL_ROOM_Y1 - GL_R);
        CHECK(lv->exit.x > GL_ROOM_X0 + GL_R && lv->exit.x < GL_ROOM_X1 - GL_R);
        CHECK(lv->exit.y > GL_ROOM_Y0 + GL_R && lv->exit.y < GL_ROOM_Y1 - GL_R);
        /* nobody starts stuck inside a wall or a door */
        for (int w = 0; w < lv->wall_count; w++)
            CHECK(dist_point_rect(lv->start, lv->walls[w]) > GL_R);
        for (int d = 0; d < lv->door_count; d++)
            CHECK(dist_point_rect(lv->start, lv->doors[d].box) > GL_R);
    }
}

/* This is the regression test for "you can walk around the door": with the doors shut, the exit must be
 * walled off, and no single door may be skippable (open every door but one: the exit must stay out of reach).
 */
static void test_no_door_can_be_walked_around(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        const gl_Level *lv = ghost_loop_level(i);
        unsigned all = (1u << lv->door_count) - 1u;
        CHECK(!ghost_loop_can_reach(i, 0, lv->start, lv->exit, GL_EXIT_R)); /* all shut */
        CHECK(ghost_loop_can_reach(i, all, lv->start, lv->exit,
                                   GL_EXIT_R)); /* all open: the level is passable */
        for (int d = 0; d < lv->door_count; d++)
            CHECK(!ghost_loop_can_reach(i, all & ~(1u << d), lv->start, lv->exit, GL_EXIT_R));
    }
}

static void test_pads_are_reachable_and_round_one_has_something_to_do(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        const gl_Level *lv = ghost_loop_level(i);
        unsigned all = (1u << lv->door_count) - 1u;
        int free_pads = 0;
        for (int p = 0; p < lv->pad_count; p++) {
            CHECK(ghost_loop_can_reach(i, all, lv->start, lv->pads[p], GL_PAD_R));
            if (ghost_loop_can_reach(i, 0, lv->start, lv->pads[p], GL_PAD_R))
                free_pads++;
        }
        CHECK(free_pads >= 1); /* the first round is played alone: at least one pad must be reachable then */
    }
}

/* Without help nobody may ride a door: a pad has to be too far from every door it opens for an actor to be on
 * the pad and in the doorway at the same time. */
static void test_a_pad_cannot_open_its_own_door_for_the_one_standing_on_it(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        const gl_Level *lv = ghost_loop_level(i);
        for (int d = 0; d < lv->door_count; d++)
            CHECK(dist_point_rect(lv->pads[lv->doors[d].pad], lv->doors[d].box) > GL_PAD_R + GL_R + 10.0f);
    }
}

static void test_flood_fill_basics(void) {
    const gl_Level *lv = ghost_loop_level(0);
    CHECK(!ghost_loop_can_reach(0, 0, vec2(-50.0f, -50.0f), lv->exit, GL_EXIT_R)); /* outside the room */
    CHECK(ghost_loop_can_reach(0, 0, lv->start, lv->start, 5.0f));                 /* already there */
    CHECK(ghost_loop_can_reach(0, 0, lv->start, lv->pads[0], GL_PAD_R));
}

/* ---- the campaign: every level can be beaten ----------------------------------------------------------- */

static void test_every_level_can_be_beaten_with_its_par_ghosts(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        ghost_loop_State s;
        ghost_loop_init(&s, 1);
        ghost_loop_load_level(&s, i);
        play_solution(&s, 130, 0);
        CHECK(s.cleared);
        CHECK_EQ_INT(s.ghost_count, ghost_loop_level(i)->par_ghosts);
        CHECK(s.level_points > 1000); /* par ghosts, and well inside the 8 seconds */
        CHECK(s.level_points <= 1500);
    }
}

static void test_the_whole_campaign_can_be_played_through(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    int expected = 0;
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        CHECK_EQ_INT(s.level, i);
        CHECK(!ghost_loop_is_over(&s));
        play_solution(&s, 130, 0);
        CHECK(s.cleared);
        expected += s.level_points;
        CHECK_EQ_INT(ghost_loop_score(&s), expected);
        ArcadeInput idle;
        arcade_input_clear(&idle);
        for (int k = 0; k < GL_CLEAR_HOLD; k++)
            ghost_loop_step(&s, &idle, ARC_DT);
        press_space(&s);
    }
    CHECK(ghost_loop_is_over(&s));
    CHECK_EQ_INT(ghost_loop_score(&s), expected);
}

/* The levels are meant for people: the two relay levels must work for a wide range of hold times, and every
 * level must forgive a slow start of a second. */
static void test_relay_levels_have_a_forgiving_timing_window(void) {
    const int holds[3] = {40, 130, 200};
    const int relay_levels[2] = {3, 5};
    for (int l = 0; l < 2; l++)
        for (int h = 0; h < 3; h++) {
            ghost_loop_State s;
            ghost_loop_init(&s, 1);
            ghost_loop_load_level(&s, relay_levels[l]);
            play_solution(&s, holds[h], 0);
            CHECK(s.cleared);
        }
}

static void test_every_level_forgives_a_slow_start(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        ghost_loop_State s;
        ghost_loop_init(&s, 1);
        ghost_loop_load_level(&s, i);
        play_solution(&s, 90, 60); /* one second late in every ghost round */
        CHECK(s.cleared);
    }
}

/* Walking straight at the exit, round after round (each failed round becomes a ghost that does the same),
 * never clears any level: the doors have to be opened on purpose. */
static void test_walking_straight_at_the_exit_never_clears_a_level(void) {
    for (int i = 0; i < GL_LEVEL_COUNT; i++) {
        ghost_loop_State s;
        ghost_loop_init(&s, 1);
        ghost_loop_load_level(&s, i);
        const gl_Level *lv = ghost_loop_level(i);
        for (int attempt = 0; attempt < 4; attempt++)
            walk_to(&s, lv->exit);
        CHECK(!s.cleared);
    }
}

/* ---- rules --------------------------------------------------------------------------------------------- */

static void test_clear_banner_waits_before_moving_on(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    play_solution(&s, 130, 0);
    CHECK(s.cleared);
    int points = s.level_points;
    press_space(&s); /* too early: the banner must be seen */
    CHECK_EQ_INT(s.level, 0);
    CHECK(s.cleared);
    ArcadeInput idle;
    arcade_input_clear(&idle);
    for (int k = 0; k < GL_CLEAR_HOLD; k++)
        ghost_loop_step(&s, &idle, ARC_DT);
    press_space(&s);
    CHECK_EQ_INT(s.level, 1);
    CHECK(!s.cleared);
    CHECK_EQ_INT(s.round, 0);
    CHECK_EQ_INT(s.ghost_count, 0);
    CHECK_EQ_INT(ghost_loop_score(&s), points); /* points stay banked */
}

static void test_a_finished_run_ignores_input(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    s.won = true;
    Vec2 where = s.player;
    ArcadeInput in;
    arcade_input_clear(&in);
    in.move_x = 1.0f;
    ghost_loop_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.player.x, where.x, 0.0001);
    CHECK(ghost_loop_is_over(&s));
}

static void test_points_follow_par(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    ghost_loop_load_level(&s, 1); /* par 2 */
    s.round = 2;
    s.step = 100;
    CHECK_EQ_INT(ghost_loop_level_points(&s), 1400); /* at par: only the time counts */
    s.round = 3;
    CHECK_EQ_INT(ghost_loop_level_points(&s), 1100); /* one ghost above par: 300 less */
    s.round = 1;
    CHECK_EQ_INT(ghost_loop_level_points(&s), 1400); /* fewer ghosts than par is just full marks */
    s.round = 40;
    s.step = 479;
    CHECK_EQ_INT(ghost_loop_level_points(&s), 100); /* never below 100 */
}

static void test_space_ends_the_round_early_and_the_ghost_stays_put(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    walk_to(&s, vec2(320.0f, 180.0f));
    CHECK(s.step > 20 && s.step < GL_ROUND_STEPS - 1);
    Vec2 where = s.player;
    press_space(&s);
    CHECK_EQ_INT(s.round, 1);
    CHECK_EQ_INT(s.ghost_count, 1);
    CHECK_EQ_INT(s.step, 0);
    stand_still(&s, 300);
    CHECK_NEAR(s.ghost_pos[0].x, where.x, 0.01); /* the recording ended there: the ghost idles on the spot */
    CHECK_NEAR(s.ghost_pos[0].y, where.y, 0.01);
    CHECK(s.pad_down[0]);
    CHECK(s.door_open[0]);
}

static void test_the_timer_ends_a_round_too(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    stand_still(&s, GL_ROUND_STEPS + 10);
    CHECK_EQ_INT(s.round, 1);
    CHECK_EQ_INT(s.ghost_count, 1);
    CHECK_EQ_INT(s.notice, GL_NOTICE_NONE); /* a ghost slot was free, so nothing special happened */
}

static void test_ghost_replays_the_recorded_run_exactly(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    Vec2 path[GL_ROUND_STEPS];
    for (int i = 0; i < GL_ROUND_STEPS; i++) { /* a scripted zig-zag that stays in the first chamber */
        ArcadeInput in;
        arcade_input_clear(&in);
        in.move_x = ((i / 60) % 2) ? 1.0f : -1.0f;
        in.move_y = ((i / 90) % 2) ? 1.0f : -1.0f;
        ghost_loop_step(&s, &in, ARC_DT);
        path[i] = s.player;
        if (s.round == 1) { /* the last step ended the round: the player was reset, keep the final position */
            path[i] = s.ghost_pos[0];
            break;
        }
    }
    CHECK_EQ_INT(s.round, 1);
    ArcadeInput idle;
    arcade_input_clear(&idle);
    for (int i = 0; i < GL_ROUND_STEPS - 1; i++) {
        ghost_loop_step(&s, &idle, ARC_DT);
        CHECK_NEAR(s.ghost_pos[0].x, path[i].x, 0.001);
        CHECK_NEAR(s.ghost_pos[0].y, path[i].y, 0.001);
    }
}

static void test_redo_undo_and_restart_keys(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    ghost_loop_load_level(&s, 1); /* two ghost slots */
    walk_to(&s, vec2(300.0f, 560.0f));
    press_space(&s); /* ghost 1 */
    walk_to(&s, vec2(435.0f, 380.0f));
    press_space(&s); /* ghost 2 */
    CHECK_EQ_INT(s.ghost_count, 2);
    walk_to(&s, vec2(500.0f, 380.0f));
    CHECK(s.step > 0);
    press_key(&s, 'r'); /* redo: this round starts again, the ghosts stay */
    CHECK_EQ_INT(s.step, 0);
    CHECK_EQ_INT(s.ghost_count, 2);
    CHECK_EQ_INT(s.round, 2);
    press_key(&s, 'u'); /* undo: the newest ghost goes away and its round is played again */
    CHECK_EQ_INT(s.ghost_count, 1);
    CHECK_EQ_INT(s.round, 1);
    CHECK_EQ_INT(s.step, 0);
    press_key(&s, 'C'); /* restart the level: no ghosts at all */
    CHECK_EQ_INT(s.ghost_count, 0);
    CHECK_EQ_INT(s.round, 0);
    press_key(&s, 'u'); /* nothing to undo: harmless */
    CHECK_EQ_INT(s.ghost_count, 0);
    CHECK_EQ_INT(s.round, 0);
}

static void test_running_out_of_ghost_slots_replays_the_round_instead_of_losing(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    int slots = ghost_loop_level(0)->max_ghosts;
    for (int g = 0; g < slots; g++)
        press_space(&s); /* fill every slot with a do-nothing ghost */
    CHECK_EQ_INT(s.ghost_count, slots);
    press_space(&s); /* nothing left to record into */
    CHECK_EQ_INT(s.ghost_count, slots);
    CHECK_EQ_INT(s.notice, GL_NOTICE_NO_GHOSTS);
    stand_still(&s, GL_ROUND_STEPS + 5); /* the timer runs out in the last round */
    CHECK_EQ_INT(s.ghost_count, slots);  /* the ghosts survive... */
    CHECK_EQ_INT(s.step, 5);             /* ...and the same round is running again, 5 steps in */
    CHECK_EQ_INT(s.notice, GL_NOTICE_TIME_UP);
    CHECK(!ghost_loop_is_over(&s)); /* there is no losing */
}

/* The rule that the old placeholder door got wrong: a closed door is solid on every side. */
static void test_a_closed_door_stops_an_actor_but_lets_one_inside_walk_out(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    const gl_Level *lv = ghost_loop_level(0);
    gl_Rect door = lv->doors[0].box;
    float mid_y = (door.y0 + door.y1) / 2.0f;
    /* outside, pushing at the closed door */
    s.player = vec2(door.x0 - GL_R - 2.0f, mid_y);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.move_x = 1.0f;
    for (int i = 0; i < 60; i++)
        ghost_loop_step(&s, &in, ARC_DT);
    CHECK(s.player.x + GL_R <= door.x0 + 0.001f);
    /* pushing diagonally up along the wall above the door: slides, never gets through */
    in.move_y = -1.0f;
    for (int i = 0; i < 120; i++)
        ghost_loop_step(&s, &in, ARC_DT);
    CHECK(s.player.x + GL_R <= door.x0 + 0.001f);
    CHECK(s.player.y < mid_y); /* it did slide up the wall */
    /* inside the doorway when the door is shut: free to walk out the far side */
    ghost_loop_load_level(&s, 0);
    s.player = vec2((door.x0 + door.x1) / 2.0f, mid_y);
    arcade_input_clear(&in);
    in.move_x = 1.0f;
    for (int i = 0; i < 30; i++)
        ghost_loop_step(&s, &in, ARC_DT);
    CHECK(s.player.x - GL_R >= door.x1 - 0.001f);
}

static void test_monkey_never_leaves_the_room_or_enters_a_wall(void) {
    for (int lv = 0; lv < GL_LEVEL_COUNT; lv++) {
        ghost_loop_State s;
        ghost_loop_init(&s, 1);
        ghost_loop_load_level(&s, lv);
        ArcadeMonkey mk;
        arcade_monkey_init(&mk, 77u + (unsigned)lv);
        for (int i = 0; i < 6000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&mk, &in);
            ghost_loop_step(&s, &in, ARC_DT);
            if (s.level != lv) /* the monkey got lucky and cleared the level: keep checking the one we know */
                ghost_loop_load_level(&s, lv);
            const gl_Level *l = ghost_loop_level(lv);
            CHECK(s.player.x >= GL_ROOM_X0 + GL_R - 0.01f && s.player.x <= GL_ROOM_X1 - GL_R + 0.01f);
            CHECK(s.player.y >= GL_ROOM_Y0 + GL_R - 0.01f && s.player.y <= GL_ROOM_Y1 - GL_R + 0.01f);
            for (int w = 0; w < l->wall_count; w++)
                CHECK(dist_point_rect(s.player, l->walls[w]) >= GL_R - 0.01f); /* walls are never entered */
            for (int g = 0; g < s.ghost_count; g++)
                for (int w = 0; w < l->wall_count; w++)
                    CHECK(dist_point_rect(s.ghost_pos[g], l->walls[w]) >= GL_R - 0.01f);
        }
    }
}

int main(void) {
    RUN(test_every_level_is_well_formed);
    RUN(test_no_door_can_be_walked_around);
    RUN(test_pads_are_reachable_and_round_one_has_something_to_do);
    RUN(test_a_pad_cannot_open_its_own_door_for_the_one_standing_on_it);
    RUN(test_flood_fill_basics);
    RUN(test_every_level_can_be_beaten_with_its_par_ghosts);
    RUN(test_the_whole_campaign_can_be_played_through);
    RUN(test_relay_levels_have_a_forgiving_timing_window);
    RUN(test_every_level_forgives_a_slow_start);
    RUN(test_walking_straight_at_the_exit_never_clears_a_level);
    RUN(test_clear_banner_waits_before_moving_on);
    RUN(test_a_finished_run_ignores_input);
    RUN(test_points_follow_par);
    RUN(test_space_ends_the_round_early_and_the_ghost_stays_put);
    RUN(test_the_timer_ends_a_round_too);
    RUN(test_ghost_replays_the_recorded_run_exactly);
    RUN(test_redo_undo_and_restart_keys);
    RUN(test_running_out_of_ghost_slots_replays_the_round_instead_of_losing);
    RUN(test_a_closed_door_stops_an_actor_but_lets_one_inside_walk_out);
    RUN(test_monkey_never_leaves_the_room_or_enters_a_wall);
    return TEST_SUMMARY();
}
