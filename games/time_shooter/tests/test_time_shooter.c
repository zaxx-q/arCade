#include "arcade_test.h"
#include "time_shooter_logic.h"

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static void test_world_is_frozen_when_idle(void) {
    time_shooter_State s;
    time_shooter_init(&s, 1);
    s.enemies[0].alive = true;
    s.enemies[0].pos = vec2(100, 100);
    s.enemies[0].speed = 100.0f;
    Vec2 before = s.enemies[0].pos;
    ArcadeInput in = idle();
    for (int i = 0; i < 120; i++)
        time_shooter_step(&s, &in, ARC_DT);
    CHECK(vec2_dist(before, s.enemies[0].pos) < 12.0f); /* 2 s idle: crawls instead of covering ~200 px */
    CHECK(s.time_scale < 0.1f);
}

static void test_world_runs_when_moving(void) {
    time_shooter_State s;
    time_shooter_init(&s, 1);
    s.enemies[0].alive = true;
    s.enemies[0].pos = vec2(100, 100);
    s.enemies[0].speed = 100.0f;
    Vec2 before = s.enemies[0].pos;
    ArcadeInput in = idle();
    in.move_x = 1.0f;
    for (int i = 0; i < 60; i++)
        time_shooter_step(&s, &in, ARC_DT);
    CHECK(vec2_dist(before, s.enemies[0].pos) > 60.0f);
    CHECK_NEAR(s.time_scale, 1.0, 0.0001);
}

static void test_bullet_kills_enemy(void) {
    time_shooter_State s;
    time_shooter_init(&s, 1);
    s.player = vec2(300, 300);
    s.enemies[0].alive = true;
    s.enemies[0].pos = vec2(600, 300);
    s.enemies[0].speed = 0.0f;
    ArcadeInput in = idle();
    in.fire_pressed = true;
    in.aim_x = 600;
    in.aim_y = 300;
    time_shooter_step(&s, &in, ARC_DT);
    in = idle();
    in.move_y = 1.0f; /* keep moving so time keeps running while the bullet flies */
    for (int i = 0; i < 90 && s.kills == 0; i++)
        time_shooter_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.kills, 1);
    CHECK(!s.enemies[0].alive);
    CHECK(time_shooter_score(&s) >= 100);
}

static void test_touching_an_enemy_ends_the_game(void) {
    time_shooter_State s;
    time_shooter_init(&s, 1);
    s.enemies[0].alive = true;
    s.enemies[0].pos = s.player;
    s.enemies[0].speed = 0.0f;
    ArcadeInput in = idle();
    time_shooter_step(&s, &in, ARC_DT);
    CHECK(time_shooter_is_over(&s));
    int score = time_shooter_score(&s);
    time_shooter_step(&s, &in, ARC_DT); /* no changes after game over */
    CHECK_EQ_INT(time_shooter_score(&s), score);
}

static void test_cooldown_limits_fire_rate(void) {
    time_shooter_State s;
    time_shooter_init(&s, 1);
    ArcadeInput in = idle();
    in.fire = true;
    in.aim_x = 900;
    in.aim_y = 300;
    for (int i = 0; i < 6; i++)
        time_shooter_step(&s, &in, ARC_DT);
    int bullets = 0;
    for (int i = 0; i < TS_MAX_BULLETS; i++)
        bullets += s.bullets[i].alive ? 1 : 0;
    CHECK_EQ_INT(bullets, 1); /* 0.1 s of holding fire = one bullet (cooldown 0.25 s) */
}

static void test_deterministic_and_bounded_with_monkey(void) {
    time_shooter_State a, b;
    time_shooter_init(&a, 7);
    time_shooter_init(&b, 7);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 4);
    arcade_monkey_init(&mb, 4);
    for (int i = 0; i < 4000; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        time_shooter_step(&a, &ia, ARC_DT);
        time_shooter_step(&b, &ib, ARC_DT);
        CHECK(a.player.x >= 0 && a.player.x <= ARC_SCREEN_W && a.player.y >= 0 && a.player.y <= ARC_SCREEN_H);
    }
    CHECK_EQ_INT(time_shooter_score(&a), time_shooter_score(&b));
    CHECK_EQ_INT(a.over, b.over);
}

int main(void) {
    RUN(test_world_is_frozen_when_idle);
    RUN(test_world_runs_when_moving);
    RUN(test_bullet_kills_enemy);
    RUN(test_touching_an_enemy_ends_the_game);
    RUN(test_cooldown_limits_fire_rate);
    RUN(test_deterministic_and_bounded_with_monkey);
    return TEST_SUMMARY();
}
