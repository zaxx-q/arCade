#include <string.h>

#include "arcade_test.h"
#include "laser_mirrors_logic.h"

static void clear_grid(laser_mirrors_State *s) {
    memset(s, 0, sizeof(*s));
    s->emitter_x = 0;
    s->emitter_y = 2;
    s->emitter_dir = 0;
}

/* One frame in which the player clicks the middle of tile (tx, ty). */
static void click_tile(laser_mirrors_State *s, int tx, int ty) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    in.aim_x = LM_OX + ((float)tx + 0.5f) * LM_TILE;
    in.aim_y = LM_OY + ((float)ty + 0.5f) * LM_TILE;
    laser_mirrors_step(s, &in, ARC_DT);
}

/* The same, but SPACE (continue) instead of a click. */
static void press_space(laser_mirrors_State *s) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.action_pressed = true;
    laser_mirrors_step(s, &in, ARC_DT);
}

static void idle(laser_mirrors_State *s, int steps) {
    ArcadeInput in;
    arcade_input_clear(&in);
    for (int i = 0; i < steps; i++)
        laser_mirrors_step(s, &in, ARC_DT);
}

/* ---- the level table -------------------------------------------------------------------------------- */

static void test_every_level_is_well_formed(void) {
    for (int i = 0; i < LM_LEVEL_COUNT; i++) {
        laser_mirrors_State s;
        laser_mirrors_init(&s, 1);
        laser_mirrors_load_level(&s, i);
        CHECK_EQ_INT(s.level, i);
        CHECK(s.targets >= 1);
        CHECK(!s.solved); /* a level must not start solved */
        CHECK(laser_mirrors_level_name(i)[0] != '\0');
        CHECK(laser_mirrors_level_hint(i)[0] != '\0');
        /* the laser sits on the edge of the board and shoots inwards */
        int x = s.emitter_x, y = s.emitter_y, d = s.emitter_dir;
        CHECK((d == 0 && x == 0) || (d == 1 && y == 0) || (d == 2 && x == LM_W - 1) ||
              (d == 3 && y == LM_H - 1));
        int rot = 0;
        for (int yy = 0; yy < LM_H; yy++)
            for (int xx = 0; xx < LM_W; xx++)
                if (s.rotatable[yy][xx])
                    rot++;
        CHECK(rot >= 1);
        CHECK(rot <= LM_MAX_ROT); /* the solver can only check this many */
    }
}

/* The key promise of the campaign: every level can be beaten, the stated par is really the best possible,
 * and there is exactly ONE winning arrangement, so no level can be solved by accident or by a loophole. */
static void test_every_level_has_one_solution_and_a_true_par(void) {
    for (int i = 0; i < LM_LEVEL_COUNT; i++) {
        laser_mirrors_State s;
        laser_mirrors_init(&s, 1);
        laser_mirrors_load_level(&s, i);
        LmSolve r = laser_mirrors_solve(&s);
        CHECK_EQ_INT(r.solutions, 1);
        CHECK(r.min_moves >= 1);
        CHECK_EQ_INT(r.min_moves, laser_mirrors_level_par(i));
        CHECK_EQ_INT(s.par, laser_mirrors_level_par(i));
        CHECK(laser_mirrors_can_solve(&s));
    }
}

static void test_difficulty_ramps_up(void) {
    /* the last level is much bigger than the first; par never shrinks by more than the teaching levels */
    CHECK(laser_mirrors_level_par(LM_LEVEL_COUNT - 1) >= 3 * laser_mirrors_level_par(0));
    laser_mirrors_State a, b;
    laser_mirrors_init(&a, 1);
    laser_mirrors_load_level(&b, LM_LEVEL_COUNT - 1);
    LmSolve first = laser_mirrors_solve(&a), last = laser_mirrors_solve(&b);
    CHECK(last.rotatable > first.rotatable + 5);
    CHECK(b.targets > a.targets);
}

static void test_init_ignores_the_seed_and_loading_clamps(void) {
    laser_mirrors_State a, b;
    laser_mirrors_init(&a, 1);
    laser_mirrors_init(&b, 987654);
    CHECK(memcmp(a.cell, b.cell, sizeof(a.cell)) == 0);
    laser_mirrors_load_level(&a, -5);
    CHECK_EQ_INT(a.level, 0);
    laser_mirrors_load_level(&a, 999);
    CHECK_EQ_INT(a.level, LM_LEVEL_COUNT - 1);
}

/* ---- playing the campaign through the real input path ---------------------------------------------- */

/* Beats level 'a.level' with the solver's cheapest solution, clicking tile by tile like a player would. */
static void click_solution(laser_mirrors_State *s) {
    LmSolve r = laser_mirrors_solve(s);
    int k = 0;
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++)
            if (s->rotatable[y][x]) {
                if ((r.flips >> k) & 1u)
                    click_tile(s, x, y);
                k++;
            }
}

static void test_the_whole_campaign_can_be_beaten(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    for (int i = 0; i < LM_LEVEL_COUNT; i++) {
        CHECK_EQ_INT(s.level, i);
        CHECK(!laser_mirrors_is_over(&s));
        click_solution(&s);
        CHECK(s.solved);
        CHECK_EQ_INT(s.moves, laser_mirrors_level_par(i));
        CHECK_EQ_INT(s.level_points, 1000); /* par = full marks */
        CHECK_EQ_INT(laser_mirrors_score(&s), 1000 * (i + 1));
        idle(&s, LM_CLEAR_HOLD);
        press_space(&s);
    }
    CHECK(laser_mirrors_is_over(&s));
    CHECK_EQ_INT(laser_mirrors_score(&s), 1000 * LM_LEVEL_COUNT);
}

static void test_clear_banner_waits_before_moving_on(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    click_solution(&s);
    CHECK(s.solved);
    click_tile(&s, 4, 4); /* a double-click right after the last mirror must not skip the banner */
    press_space(&s);
    CHECK_EQ_INT(s.level, 0);
    CHECK(s.solved);
    idle(&s, LM_CLEAR_HOLD);
    click_tile(&s, 0, 0); /* any click moves on once the banner has been shown */
    CHECK_EQ_INT(s.level, 1);
    CHECK(!s.solved);
    CHECK_EQ_INT(s.moves, 0);
    CHECK_EQ_INT(laser_mirrors_score(&s), 1000); /* points stay banked */
}

static void test_a_finished_run_ignores_input(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    s.finished = true;
    int level = s.level;
    click_tile(&s, 4, 4);
    press_space(&s);
    CHECK_EQ_INT(s.level, level);
    CHECK_EQ_INT(s.moves, 0);
}

static void test_clicks_count_moves_and_ignore_the_wrong_tiles(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    click_tile(&s, 0, 0); /* an empty tile */
    click_tile(&s, 4, 4); /* the rotatable mirror of level 0 */
    CHECK_EQ_INT(s.moves, 1);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    in.aim_x = 5.0f; /* left of the board */
    in.aim_y = LM_OY + 1.5f * LM_TILE;
    laser_mirrors_step(&s, &in, ARC_DT);
    in.aim_x = LM_OX + 5 * LM_TILE;
    in.aim_y = 3.0f; /* above the board */
    laser_mirrors_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.moves, 1);
}

static void test_points_per_level_follow_par(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    CHECK_EQ_INT(laser_mirrors_level_points(&s), 0); /* not solved: no points */
    s.solved = true;
    s.par = 3;
    s.moves = 3;
    CHECK_EQ_INT(laser_mirrors_level_points(&s), 1000);
    s.moves = 1; /* better than par is still just full marks */
    CHECK_EQ_INT(laser_mirrors_level_points(&s), 1000);
    s.moves = 5;
    CHECK_EQ_INT(laser_mirrors_level_points(&s), 1000 - 2 * 60);
    s.moves = 500;
    CHECK_EQ_INT(laser_mirrors_level_points(&s), 100); /* never below 100 */
}

static void test_reset_key_restores_mirrors_but_keeps_the_clicks(void) {
    laser_mirrors_State s;
    laser_mirrors_init(&s, 1);
    laser_mirrors_load_level(&s, 1); /* ZIG-ZAG: three mirrors, clicking two of them does not solve it */
    uint8_t before[LM_H][LM_W];
    memcpy(before, s.cell, sizeof(before));
    click_tile(&s, 4, 0);
    click_tile(&s, 4, 5);
    CHECK_EQ_INT(s.moves, 2);
    CHECK(!s.solved);
    CHECK(memcmp(before, s.cell, sizeof(before)) != 0);
    ArcadeInput in;
    arcade_input_clear(&in);
    in.typed = 'r';
    laser_mirrors_step(&s, &in, ARC_DT);
    CHECK(memcmp(before, s.cell, sizeof(before)) == 0); /* every mirror is back as the level started */
    CHECK_EQ_INT(s.moves, 2);                           /* spent clicks are not refunded */
    CHECK_EQ_INT(s.level, 1);
}

/* ---- the beam physics ---------------------------------------------------------------------------- */

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

static void test_emitters_shoot_in_all_four_directions(void) {
    const char *right[LM_H] = {"..", "..", ">...T"};
    const char *down[LM_H] = {"...v", "", "", "", "", "...T"};
    const char *left[LM_H] = {"", "", "....T.....<"};
    const char *up[LM_H] = {"", "...T", "", "", "", "", "", "...^"};
    const char *const *boards[4] = {right, down, left, up};
    for (int i = 0; i < 4; i++) {
        laser_mirrors_State s;
        memset(&s, 0, sizeof(s));
        laser_mirrors_load_rows(&s, boards[i]);
        CHECK_EQ_INT(s.emitter_dir, i == 0 ? 0 : (i == 1 ? 1 : (i == 2 ? 2 : 3)));
        CHECK(s.solved);
    }
}

/* A splitter lets the beam through AND sends a copy away as if it were a '/' mirror, so which two ways the
 * light leaves depends on the side it arrives from. In every board the splitter S is at (5,3) and three
 * targets sit around it: two on the directions it should feed, and one 'decoy' that must stay dark. */
static void test_splitter_branches_for_every_entry_side(void) {
    const char *from_left[LM_H] = {".....T", "", "", ">....S...T", "", "", ".....T"};      /* right + up   */
    const char *from_top[LM_H] = {".....v", "", "", ".T...S...T", "", "", ".....T"};       /* down + left  */
    const char *from_right[LM_H] = {".....T", "", "", ".T...S.......<", "", "", ".....T"}; /* left + down  */
    const char *from_below[LM_H] = {".....T", "", "", ".T...S...T", "", "", "", ".....^"}; /* up + right   */
    const char *const *boards[4] = {from_left, from_top, from_right, from_below};
    const int decoy_x[4] = {5, 9, 5, 1}, decoy_y[4] = {6, 3, 0, 3};
    for (int i = 0; i < 4; i++) {
        laser_mirrors_State s;
        memset(&s, 0, sizeof(s));
        laser_mirrors_load_rows(&s, boards[i]);
        CHECK_EQ_INT(s.targets, 3);
        CHECK_EQ_INT(s.targets_lit, 2);
        CHECK(!s.lit[decoy_y[i]][decoy_x[i]]);
    }
}

static void test_targets_absorb_the_beam(void) {
    const char *rows[LM_H] = {"", "", ">.T.T"};
    laser_mirrors_State s;
    memset(&s, 0, sizeof(s));
    laser_mirrors_load_rows(&s, rows);
    CHECK_EQ_INT(s.targets, 2);
    CHECK_EQ_INT(s.targets_lit, 1); /* the first target eats the light: nothing reaches the one behind it */
    CHECK(!s.solved);
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

/* ---- the solver --------------------------------------------------------------------------------- */

static void test_solver_counts_every_combination(void) {
    /* One mirror that matters (flip it to send the beam down onto T) and one far away that does not. */
    const char *rows[LM_H] = {"", "", ">..r", "", "", "...T", "", "..........r"};
    laser_mirrors_State s;
    memset(&s, 0, sizeof(s));
    laser_mirrors_load_rows(&s, rows);
    LmSolve r = laser_mirrors_solve(&s);
    CHECK_EQ_INT(r.rotatable, 2);
    CHECK_EQ_INT(r.solutions, 2); /* the idle mirror can be either way: two winning combinations */
    CHECK_EQ_INT(r.min_moves, 1); /* ...but the cheapest one never touches it */
    CHECK_EQ_INT(r.flips, 1);     /* bit 0 = the first mirror in row-by-row order */
}

static void test_solver_reports_impossible_and_oversized_boards(void) {
    const char *solvable[LM_H] = {"", "", ">..r", "", "", "...T"};
    const char *blocked[LM_H] = {"", "", ">#.r", "", "", "...T"};
    laser_mirrors_State s;
    memset(&s, 0, sizeof(s));
    laser_mirrors_load_rows(&s, blocked);
    LmSolve r = laser_mirrors_solve(&s);
    CHECK_EQ_INT(r.solutions, 0);
    CHECK_EQ_INT(r.min_moves, -1);
    CHECK(!laser_mirrors_can_solve(&s));
    laser_mirrors_load_rows(&s, solvable); /* reloading replaces the old board completely */
    CHECK(laser_mirrors_can_solve(&s));
    /* 17 rotatable mirrors are too many to try every combination */
    const char *many[LM_H] = {"rrrrrrrrrrrrrr", "rrr"};
    laser_mirrors_load_rows(&s, many);
    r = laser_mirrors_solve(&s);
    CHECK_EQ_INT(r.rotatable, 17);
    CHECK_EQ_INT(r.solutions, -1);
}

int main(void) {
    RUN(test_every_level_is_well_formed);
    RUN(test_every_level_has_one_solution_and_a_true_par);
    RUN(test_difficulty_ramps_up);
    RUN(test_init_ignores_the_seed_and_loading_clamps);
    RUN(test_the_whole_campaign_can_be_beaten);
    RUN(test_clear_banner_waits_before_moving_on);
    RUN(test_a_finished_run_ignores_input);
    RUN(test_clicks_count_moves_and_ignore_the_wrong_tiles);
    RUN(test_points_per_level_follow_par);
    RUN(test_reset_key_restores_mirrors_but_keeps_the_clicks);
    RUN(test_mirror_reflections);
    RUN(test_emitters_shoot_in_all_four_directions);
    RUN(test_splitter_branches_for_every_entry_side);
    RUN(test_targets_absorb_the_beam);
    RUN(test_mirror_loops_terminate);
    RUN(test_random_grids_never_hang_or_leave_the_board);
    RUN(test_solver_counts_every_combination);
    RUN(test_solver_reports_impossible_and_oversized_boards);
    return TEST_SUMMARY();
}
