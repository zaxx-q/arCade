#include <string.h>

#include "arcade_test.h"
#include "laser_mirrors_logic.h"

static void clear_grid(laser_mirrors_State *s) {
    memset(s, 0, sizeof(*s));
    s->emitter_x = 0;
    s->emitter_y = 2;
    s->emitter_dir = 0;
}

static void test_initial_level_is_unsolved_but_solvable(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    CHECK_EQ_INT(s.targets, 2);
    CHECK_EQ_INT(s.targets_lit, 0);
    CHECK(!s.solved);
    CHECK(laser_mirrors_can_solve(&s));
}

static void test_toggling_both_mirrors_solves(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    CHECK(laser_mirrors_toggle(&s, 4, 3));
    CHECK_EQ_INT(s.targets_lit, 1);
    CHECK(laser_mirrors_toggle(&s, 9, 4));
    CHECK_EQ_INT(s.targets_lit, 2);
    CHECK(s.solved);
    CHECK(!laser_mirrors_toggle(&s, 4, 4)); /* the splitter is not rotatable */
    CHECK(!laser_mirrors_toggle(&s, -1, 0));
}

static void test_clicks_count_moves_and_score(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    in.aim_x = LM_OX + 4.5f * LM_TILE;
    in.aim_y = LM_OY + 3.5f * LM_TILE;
    laser_mirrors_step(&s, &in, ARC_DT);
    in.aim_x = LM_OX + 9.5f * LM_TILE;
    in.aim_y = LM_OY + 4.5f * LM_TILE;
    laser_mirrors_step(&s, &in, ARC_DT);
    CHECK(laser_mirrors_is_over(&s));
    CHECK_EQ_INT(s.moves, 2);
    CHECK_EQ_INT(laser_mirrors_score(&s), 1000); /* par */
    in.aim_x = 5.0f;                             /* clicks left of the board are ignored */
    laser_mirrors_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.moves, 2);
}

static void test_mirror_reflections(void) {
    laser_mirrors_State s;
    clear_grid(&s);
    s.cell[2][3] = LM_SLASH; /* right -> up */
    s.cell[0][3] = LM_TARGET;
    s.targets = 1;
    laser_mirrors_recompute(&s);
    CHECK(s.solved);
    s.cell[2][3] = LM_BACK; /* right -> down */
    s.cell[0][3] = LM_EMPTY;
    s.cell[5][3] = LM_TARGET;
    laser_mirrors_recompute(&s);
    CHECK(s.solved);
    s.cell[2][3] = LM_WALL; /* walls block the beam */
    laser_mirrors_recompute(&s);
    CHECK(!s.solved);
}

static void test_splitter_branches(void) {
    laser_mirrors_State s;
    clear_grid(&s);
    s.cell[2][3] = LM_SPLIT;
    s.cell[2][8] = LM_TARGET; /* straight on */
    s.cell[0][3] = LM_TARGET; /* the branch goes up like off a '/' */
    s.targets = 2;
    laser_mirrors_recompute(&s);
    CHECK(s.solved);
}

static void test_mirror_loops_terminate(void) {
    laser_mirrors_State s;
    clear_grid(&s);
    s.cell[2][3] = LM_SLASH; /* a closed square of mirrors traps the beam */
    s.cell[0][3] = LM_BACK;
    s.cell[0][6] = LM_SLASH;
    s.cell[2][6] = LM_BACK;
    laser_mirrors_recompute(&s); /* must return (visited bits stop the loop) */
    CHECK(!s.solved);
}

static void test_random_grids_never_hang_or_leave_the_board(void) {
    Rng r;
    rng_seed(&r, 1234);
    for (int n = 0; n < 300; n++) {
        laser_mirrors_State s;
        clear_grid(&s);
        for (int y = 0; y < LM_H; y++)
            for (int x = 0; x < LM_W; x++) {
                if (x == s.emitter_x && y == s.emitter_y)
                    continue;
                s.cell[y][x] = (uint8_t)rng_range(&r, 0, LM_TARGET);
                if (s.cell[y][x] == LM_TARGET)
                    s.targets++;
            }
        laser_mirrors_recompute(&s);
        CHECK(s.targets_lit <= s.targets);
    }
}

int main(void) {
    RUN(test_initial_level_is_unsolved_but_solvable);
    RUN(test_toggling_both_mirrors_solves);
    RUN(test_clicks_count_moves_and_score);
    RUN(test_mirror_reflections);
    RUN(test_splitter_branches);
    RUN(test_mirror_loops_terminate);
    RUN(test_random_grids_never_hang_or_leave_the_board);
    return TEST_SUMMARY();
}
