#include "arcade_test.h"
#include "ghost_loop_logic.h"

static ArcadeInput input_toward(Vec2 from, Vec2 to) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.move_x = to.x - from.x > 6.0f ? 1.0f : (to.x - from.x < -6.0f ? -1.0f : 0.0f);
    in.move_y = to.y - from.y > 6.0f ? 1.0f : (to.y - from.y < -6.0f ? -1.0f : 0.0f);
    return in;
}

static void drive_to(ghost_loop_State *s, Vec2 target, float radius) {
    int round = s->round;
    for (int i = 0; i < GL_ROUND_STEPS && s->round == round && !ghost_loop_is_over(s); i++) {
        if (vec2_dist(s->player, target) < radius)
            return;
        ArcadeInput in = input_toward(s->player, target);
        ghost_loop_step(s, &in, ARC_DT);
    }
}

static void idle_until_next_round(ghost_loop_State *s) {
    int round = s->round;
    ArcadeInput in;
    arcade_input_clear(&in);
    for (int i = 0; i < GL_ROUND_STEPS + 5 && s->round == round && !ghost_loop_is_over(s); i++)
        ghost_loop_step(s, &in, ARC_DT);
}

static void test_closed_door_blocks_a_solo_run(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    drive_to(&s, vec2(GL_EXIT_X, GL_EXIT_Y), 20.0f); /* pushes against the closed door until the round ends */
    CHECK(!s.won);
    CHECK(s.player.x < GL_DOOR_X);
    CHECK_EQ_INT(s.round, 1); /* the failed round became a ghost */
    CHECK_EQ_INT(s.ghost_count, 1);
}

static void test_ghost_holds_the_pad_and_player_wins_in_round_two(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    drive_to(&s, vec2(GL_PAD_X, GL_PAD_Y), 20.0f); /* round 1: walk to the pad and wait there */
    idle_until_next_round(&s);
    CHECK_EQ_INT(s.round, 1);
    drive_to(&s, vec2(GL_EXIT_X, GL_EXIT_Y), 20.0f); /* round 2: the ghost opens the door for you */
    CHECK(ghost_loop_is_over(&s));
    CHECK(s.won);
    CHECK(ghost_loop_score(&s) >= 100);
    CHECK(ghost_loop_score(&s) < 1500);
}

static void test_ghost_replays_the_recorded_run_exactly(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    Vec2 path[GL_ROUND_STEPS];
    for (int i = 0; i < GL_ROUND_STEPS; i++) { /* a scripted zig-zag that stays left of the door */
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

static void test_running_out_of_ghosts_loses(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 1);
    for (int r = 0; r <= GL_MAX_GHOSTS && !ghost_loop_is_over(&s); r++)
        idle_until_next_round(&s);
    CHECK(s.lost);
    CHECK(ghost_loop_is_over(&s));
    CHECK_EQ_INT(ghost_loop_score(&s), 0);
}

static void test_monkey_stays_in_the_room(void) {
    ghost_loop_State s;
    ghost_loop_init(&s, 3);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 5);
    for (int i = 0; i < 6000 && !ghost_loop_is_over(&s); i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        ghost_loop_step(&s, &in, ARC_DT);
        CHECK(s.player.x >= GL_ROOM_X0 && s.player.x <= GL_ROOM_X1);
        CHECK(s.player.y >= GL_ROOM_Y0 && s.player.y <= GL_ROOM_Y1);
        CHECK(s.step >= 0 && (ghost_loop_is_over(&s) || s.step < GL_ROUND_STEPS));
    }
}

int main(void) {
    RUN(test_closed_door_blocks_a_solo_run);
    RUN(test_ghost_holds_the_pad_and_player_wins_in_round_two);
    RUN(test_ghost_replays_the_recorded_run_exactly);
    RUN(test_running_out_of_ghosts_loses);
    RUN(test_monkey_stays_in_the_room);
    return TEST_SUMMARY();
}
