#include <string.h>

#include "arcade_test.h"
#include "echo_maze_logic.h"

static echo_maze_State g_s;

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

/* flood fill over floor tiles; returns how many tiles were reached */
static int reachable_floor(const echo_maze_State *s, int sx, int sy, bool *reached_exit) {
    static bool seen[EM_H][EM_W];
    static int stack_x[EM_W * EM_H], stack_y[EM_W * EM_H];
    memset(seen, 0, sizeof(seen));
    int top = 0, count = 0;
    stack_x[top] = sx;
    stack_y[top++] = sy;
    seen[sy][sx] = true;
    *reached_exit = false;
    while (top > 0) {
        int x = stack_x[--top], y = stack_y[top];
        count++;
        if (x == EM_W - 2 && y == EM_H - 2)
            *reached_exit = true;
        static const int dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
        for (int d = 0; d < 4; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= EM_W || ny >= EM_H || seen[ny][nx] || s->wall[ny][nx])
                continue;
            seen[ny][nx] = true;
            stack_x[top] = nx;
            stack_y[top++] = ny;
        }
    }
    return count;
}

static void test_maze_is_a_perfect_maze(void) {
    for (unsigned seed = 1; seed <= 25; seed++) {
        echo_maze_init(&g_s, seed);
        int floor_tiles = 0;
        for (int y = 0; y < EM_H; y++)
            for (int x = 0; x < EM_W; x++) {
                if (!g_s.wall[y][x])
                    floor_tiles++;
                if (x == 0 || y == 0 || x == EM_W - 1 || y == EM_H - 1)
                    CHECK(g_s.wall[y][x] == 1); /* solid border */
            }
        bool exit_found = false;
        int reached = reachable_floor(&g_s, 1, 1, &exit_found);
        CHECK_EQ_INT(reached, floor_tiles); /* everything is connected */
        CHECK(exit_found);
        CHECK_EQ_INT(floor_tiles,
                     2 * ((EM_W / 2) * (EM_H / 2)) - 1); /* 120 rooms + 119 passages: a tree, no loops */
    }
}

static void test_same_seed_same_maze(void) {
    static echo_maze_State other;
    echo_maze_init(&g_s, 5);
    echo_maze_init(&other, 5);
    CHECK(memcmp(g_s.wall, other.wall, sizeof(g_s.wall)) == 0);
    echo_maze_init(&other, 6);
    CHECK(memcmp(g_s.wall, other.wall, sizeof(g_s.wall)) != 0);
}

static void test_player_cannot_enter_walls(void) {
    echo_maze_init(&g_s, 3);
    ArcadeInput in = idle();
    for (int k = 0; k < 4; k++) {
        in.move_x = (k == 0) ? 1.0f : (k == 2 ? -1.0f : 0.0f);
        in.move_y = (k == 1) ? 1.0f : (k == 3 ? -1.0f : 0.0f);
        for (int i = 0; i < 240; i++)
            echo_maze_step(&g_s, &in, ARC_DT);
        int tx = (int)((g_s.player.x - EM_OX) / EM_TILE), ty = (int)((g_s.player.y - EM_OY) / EM_TILE);
        CHECK(g_s.wall[ty][tx] == 0);
    }
}

static void test_ping_reveals_visible_walls_but_not_hidden_ones(void) {
    echo_maze_init(&g_s, 1);
    for (int y = 0; y < EM_H; y++)
        for (int x = 0; x < EM_W; x++)
            g_s.wall[y][x] = (x == 0 || y == 0 || x == EM_W - 1 || y == EM_H - 1) ? 1 : 0;
    g_s.wall[1][5] = 1;
    g_s.wall[1][6] = 1; /* hidden behind (5,1) when seen from (3,1) */
    g_s.player = echo_maze_tile_center(3, 1);
    ArcadeInput in = idle();
    in.action_pressed = true;
    echo_maze_step(&g_s, &in, ARC_DT);
    in = idle();
    for (int i = 0; i < 90; i++)
        echo_maze_step(&g_s, &in, ARC_DT);
    CHECK(g_s.reveal[1][5] > 0.0f);
    CHECK_NEAR(g_s.reveal[1][6], 0.0, 0.0001);
    CHECK_EQ_INT(g_s.pings, 1);
    CHECK(echo_maze_los_clear(&g_s, echo_maze_tile_center(3, 1), 5, 1));
    CHECK(!echo_maze_los_clear(&g_s, echo_maze_tile_center(3, 1), 6, 1));
}

static void test_ping_cooldown(void) {
    echo_maze_init(&g_s, 2);
    ArcadeInput in = idle();
    in.action_pressed = true;
    for (int i = 0; i < 10; i++)
        echo_maze_step(&g_s, &in, ARC_DT);
    CHECK_EQ_INT(g_s.pings, 1); /* spamming within the cooldown does nothing */
    for (int i = 0; i < 70; i++)
        echo_maze_step(&g_s, &in, ARC_DT);
    CHECK(g_s.pings >= 2);
}

static void test_reaching_the_exit_wins_and_scores(void) {
    echo_maze_init(&g_s, 4);
    CHECK_EQ_INT(echo_maze_score(&g_s), 0);
    g_s.player = echo_maze_tile_center(EM_W - 2, EM_H - 2);
    ArcadeInput in = idle();
    echo_maze_step(&g_s, &in, ARC_DT);
    CHECK(echo_maze_is_over(&g_s));
    CHECK(echo_maze_score(&g_s) >= 100);
}

static void test_monkey_is_safe(void) {
    echo_maze_init(&g_s, 8);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 6);
    for (int i = 0; i < 6000 && !echo_maze_is_over(&g_s); i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        echo_maze_step(&g_s, &in, ARC_DT);
        int tx = (int)((g_s.player.x - EM_OX) / EM_TILE), ty = (int)((g_s.player.y - EM_OY) / EM_TILE);
        CHECK(tx >= 0 && ty >= 0 && tx < EM_W && ty < EM_H);
        CHECK(g_s.wall[ty][tx] == 0);
    }
}

int main(void) {
    RUN(test_maze_is_a_perfect_maze);
    RUN(test_same_seed_same_maze);
    RUN(test_player_cannot_enter_walls);
    RUN(test_ping_reveals_visible_walls_but_not_hidden_ones);
    RUN(test_ping_cooldown);
    RUN(test_reaching_the_exit_wins_and_scores);
    RUN(test_monkey_is_safe);
    return TEST_SUMMARY();
}
