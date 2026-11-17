#include "arcade/playfield.h"
#include "arcade_test.h"
#include "loop_lap_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput none(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

static ArcadeInput mode_input(int mode) {
    ArcadeInput in = none();
    in.move_x = (float)mode; /* -1 = left = brake, +1 = right = accelerate */
    return in;
}

static void test_init_is_sane(void) {
    loop_lap_State s;
    loop_lap_init(&s, 1);
    CHECK(!loop_lap_is_over(&s));
    CHECK_EQ_INT(loop_lap_score(&s), 0);
    CHECK_EQ_INT(s.enemy_n, 0);
}

static void test_geometry_at_480x800(void) {
    loop_lap_State s;
    loop_lap_init_sized(&s, 1, 480, 800);
    CHECK_NEAR(s.c1r, 180.0, 0.001);
    CHECK_NEAR(s.c1y, 287.5, 0.001);
    CHECK_NEAR(s.c2y, 512.5, 0.001);
    CHECK_NEAR(s.my_pos.x, 240.0, 0.001); /* start at 12 o'clock */
    CHECK_NEAR(s.my_pos.y, 110.5, 0.001);
    s.my_a = 0.0; /* 3 o'clock */
    ArcadeInput in = none();
    loop_lap_step(&s, &in, 0.0f); /* dt 0: only refreshes the position */
    CHECK_NEAR(s.my_pos.x, 417.0, 0.001);
    CHECK_NEAR(s.my_pos.y, 287.5, 0.001);
}

static void test_geometry_fits_the_684_playfield(void) {
    loop_lap_State s;
    loop_lap_init(&s, 1);
    CHECK(s.c1y - s.c1r > 0.0f);       /* top circle starts inside the playfield */
    CHECK(s.c2y + s.c2r < (float)s.h); /* bottom circle ends inside it */
}

static void test_input_mapping_table(void) {
    ArcadeInput in = none();
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_CRUISE);
    Vec2 right = arc_pf_to_screen(400.0f, 300.0f);
    Vec2 left = arc_pf_to_screen(50.0f, 300.0f);
    in.fire = true;
    in.aim_x = right.x;
    in.aim_y = right.y;
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL); /* hold, right half */
    in.aim_x = left.x;
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_BRAKE); /* hold, left half */
    in.aim_x = 5.0f; /* pointer in the left gutter still counts as the left half */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_BRAKE);
    in.aim_x = 1270.0f; /* right gutter */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL);
    in = none(); /* the pointer position alone does nothing */
    in.aim_x = right.x;
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_CRUISE);
    in.action = true; /* Space */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL);
    in = none();
    in.move_x = 1.0f; /* Right / D */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL);
    in = none();
    in.move_y = -1.0f; /* Up / W */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL);
    in = none();
    in.move_x = -1.0f; /* Left / A */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_BRAKE);
    in = none();
    in.move_y = 1.0f; /* Down / S */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_BRAKE);
    in.action = true; /* brake key + accelerate key: accelerate wins */
    CHECK_EQ_INT(loop_lap_mode_from_input(&in, 480), LL_ACCEL);
}

/* Spec: cruise first lap = 188 ticks (3/4 of a turn), later laps 250 ticks. */
static void test_lap_times_at_50hz(void) {
    loop_lap_State s;
    loop_lap_init_sized(&s, 1, 480, 800);
    ArcadeInput in = none();
    int ticks = 0;
    while (s.score == 0 && ticks < 1000) {
        loop_lap_step(&s, &in, TICK);
        ticks++;
        s.over = false; /* ignore enemies in this test */
    }
    CHECK_EQ_INT(ticks, 188);
    ticks = 0;
    while (s.score == 1 && ticks < 1000) {
        loop_lap_step(&s, &in, TICK);
        ticks++;
        s.over = false;
    }
    CHECK_EQ_INT(ticks, 250);
}

/* At dt = 1/60: first lap ~3.76 s (225 frames), later laps 5.0 s (300 frames), +- one frame. */
static void test_lap_times_at_60hz(void) {
    loop_lap_State s;
    loop_lap_init_sized(&s, 1, 480, 800);
    ArcadeInput in = none();
    int frames = 0;
    while (s.score == 0 && frames < 2000) {
        loop_lap_step(&s, &in, ARC_DT);
        frames++;
        s.over = false;
    }
    CHECK(frames >= 224 && frames <= 226);
    frames = 0;
    while (s.score == 1 && frames < 2000) {
        loop_lap_step(&s, &in, ARC_DT);
        frames++;
        s.over = false;
    }
    CHECK(frames >= 299 && frames <= 301);
}

static void test_brake_and_accelerate_lap_lengths(void) {
    for (int m = -1; m <= 1; m += 2) {
        loop_lap_State s;
        loop_lap_init_sized(&s, 1, 480, 800);
        s.my_a = 0.001; /* start a full turn */
        ArcadeInput in = mode_input(m);
        int ticks = 0;
        while (s.score == 0 && ticks < 2000) {
            loop_lap_step(&s, &in, TICK);
            ticks++;
        }
        CHECK(m == LL_ACCEL ? (ticks >= 99 && ticks <= 101) : (ticks >= 624 && ticks <= 626));
        CHECK_NEAR(s.my_w, m == LL_ACCEL ? 36.0 : 60.0, 0.001);
    }
}

static void test_hit_distances(void) {
    /* the spec's 48 px enemy */
    CHECK_NEAR(loop_lap_hit_distance(60.0f, LL_ENEMY_W), 52.0, 0.001);
    CHECK_NEAR(loop_lap_hit_distance(48.0f, LL_ENEMY_W), 46.0, 0.001);
    CHECK_NEAR(loop_lap_hit_distance(36.0f, LL_ENEMY_W), 40.0, 0.001);
    /* the smallest and the largest random enemy: my_w / 2 + enemy_w / 2 - 2 */
    CHECK_NEAR(loop_lap_hit_distance(48.0f, LL_ENEMY_W_MIN), 38.0, 0.001);
    CHECK_NEAR(loop_lap_hit_distance(48.0f, LL_ENEMY_W_MAX), 54.0, 0.001);
}

/* Concentric circles of the same radius put both balls on the same path, so the ball distance is
 * 2 R sin(delta / 2): that lets the test place the enemy at an exact distance. */
static bool collides_at_size(int mode, float enemy_w, float dist) {
    loop_lap_State s;
    loop_lap_init_sized(&s, 1, 480, 800);
    s.c2x = s.c1x;
    s.c2y = s.c1y;
    double r = s.c1r - 3.0;
    double target = 1.0; /* player angle after the step */
    ArcadeInput in = mode_input(mode);
    double speed =
        mode == LL_BRAKE ? ARC_TAU / 625.0 : (mode == LL_ACCEL ? ARC_TAU / 100.0 : ARC_TAU / 250.0);
    if (mode == LL_CRUISE)
        in = none();
    s.my_a = target - speed;
    s.enemy_n = 1;
    s.ew[0] = 0.0; /* a standing enemy */
    s.esize[0] = enemy_w;
    s.ea[0] = target + 2.0 * asin((double)dist / (2.0 * r));
    loop_lap_step(&s, &in, TICK);
    return s.over;
}

static bool collides_at(int mode, float dist) {
    return collides_at_size(mode, LL_ENEMY_W, dist);
}

static void test_collision_thresholds(void) {
    CHECK(collides_at(LL_BRAKE, 51.9f));
    CHECK(!collides_at(LL_BRAKE, 52.1f));
    CHECK(collides_at(LL_CRUISE, 45.9f));
    CHECK(!collides_at(LL_CRUISE, 46.1f));
    CHECK(collides_at(LL_ACCEL, 39.9f));
    CHECK(!collides_at(LL_ACCEL, 40.1f));
}

static void test_enemies_join_at_the_spec_laps(void) {
    static const int laps[8] = {1, 3, 8, 17, 31, 51, 78, 113};
    for (int i = 0; i < LL_MAX_ENEMIES; i++) {
        CHECK_EQ_INT(loop_lap_join_lap(i), laps[i]);
        loop_lap_State s;
        loop_lap_init(&s, 4);
        s.enemy_n = i;
        s.score = laps[i] - 1;
        s.my_a = (double)ARC_TAU - 0.001;
        ArcadeInput in = none();
        loop_lap_step(&s, &in, TICK);
        CHECK_EQ_INT(s.score, laps[i]);
        CHECK_EQ_INT(s.enemy_n, i + 1); /* the join happens even if the new enemy then crashes into us */
    }
    loop_lap_State s;
    loop_lap_init(&s, 4);
    s.enemy_n = 1;
    s.score = 1; /* lap 2 is not a join lap */
    s.my_a = (double)ARC_TAU - 0.001;
    ArcadeInput in = none();
    loop_lap_step(&s, &in, TICK);
    CHECK_EQ_INT(s.enemy_n, 1);
    s.enemy_n = LL_MAX_ENEMIES; /* never more than 8 */
    s.score = 112;
    s.my_a = (double)ARC_TAU - 0.001;
    loop_lap_step(&s, &in, TICK);
    CHECK_EQ_INT(s.enemy_n, LL_MAX_ENEMIES);
}

/* ---- every enemy gets its own random size ------------------------------------------------------- */

/* Makes enemy `i` join by finishing a lap, the same way a real run does. */
static void join_next_enemy(loop_lap_State *s) {
    s->score = loop_lap_join_lap(s->enemy_n) - 1;
    s->my_a = (double)ARC_TAU - 0.001;
    ArcadeInput in = none();
    loop_lap_step(s, &in, TICK);
    CHECK(!s->over); /* a joining enemy is ~109 px from the player (3 o'clock), so it cannot crash at once */
}

static void test_enemy_sizes_are_random_within_the_range(void) {
    float lo = 1.0e9f, hi = 0.0f, sum = 0.0f;
    int count = 0;
    for (unsigned seed = 1; seed <= 60; seed++) {
        loop_lap_State s;
        loop_lap_init(&s, seed);
        for (int i = 0; i < LL_MAX_ENEMIES; i++) {
            join_next_enemy(&s);
            CHECK_EQ_INT(s.enemy_n, i + 1);
            CHECK(s.esize[i] >= LL_ENEMY_W_MIN && s.esize[i] <= LL_ENEMY_W_MAX);
            lo = s.esize[i] < lo ? s.esize[i] : lo;
            hi = s.esize[i] > hi ? s.esize[i] : hi;
            sum += s.esize[i];
            count++;
        }
    }
    CHECK(lo < 36.0f && hi > 60.0f);                 /* the whole range is really used */
    CHECK_NEAR(sum / (float)count, LL_ENEMY_W, 1.5); /* mean 48 px: not harder on average */
}

static void test_enemies_of_one_run_differ_in_size(void) {
    loop_lap_State s;
    loop_lap_init(&s, 5);
    for (int i = 0; i < LL_MAX_ENEMIES; i++)
        join_next_enemy(&s);
    int distinct = 0; /* enemies whose size differs by more than half a pixel from every earlier one */
    for (int i = 0; i < LL_MAX_ENEMIES; i++) {
        bool unique = true;
        for (int j = 0; j < i; j++)
            if (fabsf(s.esize[i] - s.esize[j]) <= 0.5f)
                unique = false;
        if (unique)
            distinct++;
    }
    CHECK(distinct >= 6); /* 8 enemies, almost all different sizes */
}

/* Unused slots keep the default 48 px, so code (and tests) that raise enemy_n by hand see the spec's enemy.
 */
static void test_unused_slots_default_to_the_spec_size(void) {
    loop_lap_State s;
    loop_lap_init(&s, 3);
    for (int i = 0; i < LL_MAX_ENEMIES; i++)
        CHECK_NEAR(s.esize[i], 48.0, 0.0001);
}

/* The size is drawn after the angle and the speed, so those are exactly what they were before sizes existed.
 */
static void test_adding_sizes_did_not_change_angle_or_speed(void) {
    for (unsigned seed = 1; seed <= 10; seed++) {
        loop_lap_State s;
        loop_lap_init(&s, seed);
        join_next_enemy(&s);
        Rng r;
        rng_seed(&r, seed);
        double angle = (double)rng_float(&r) * (double)ARC_TAU;
        double speed = (double)rng_float(&r) * 0.03518584 + 0.00670206;
        CHECK_NEAR(s.ew[0], speed, 1e-12);
        CHECK_NEAR(s.ea[0], angle + speed, 1e-9); /* the joining step also moved it by one tick (k = 1) */
    }
}

/* Same distance, different enemies: the big one touches, the small one does not. */
static void test_collision_uses_the_enemys_own_size(void) {
    /* cruise (48 px ball): hit when distance <= 24 + enemy / 2 - 2 */
    CHECK(!collides_at_size(LL_CRUISE, 32.0f, 40.0f)); /* limit 38 */
    CHECK(collides_at_size(LL_CRUISE, 64.0f, 40.0f));  /* limit 54 */
    CHECK(collides_at_size(LL_CRUISE, 32.0f, 37.9f));
    CHECK(!collides_at_size(LL_CRUISE, 32.0f, 38.1f));
    CHECK(collides_at_size(LL_CRUISE, 64.0f, 53.9f));
    CHECK(!collides_at_size(LL_CRUISE, 64.0f, 54.1f));
    /* a small accelerating ball against the biggest enemy, and a big braking ball against the smallest */
    CHECK(collides_at_size(LL_ACCEL, 64.0f, 47.9f)); /* 18 + 32 - 2 = 48 */
    CHECK(!collides_at_size(LL_ACCEL, 64.0f, 48.1f));
    CHECK(collides_at_size(LL_BRAKE, 32.0f, 43.9f)); /* 30 + 16 - 2 = 44 */
    CHECK(!collides_at_size(LL_BRAKE, 32.0f, 44.1f));
}

static void test_near_miss_distance_uses_the_enemys_own_size(void) {
    for (int big = 0; big < 2; big++) {
        loop_lap_State s;
        loop_lap_init_sized(&s, 1, 480, 800);
        s.c2x = s.c1x;
        s.c2y = s.c1y;
        s.enemy_n = 1;
        s.ew[0] = 0.0;
        s.esize[0] = big ? 64.0f : 32.0f;
        s.ea[0] = 1.0 + 2.0 * asin(66.0 / (2.0 * 177.0)); /* 66 px away */
        s.my_a = 1.0 - ARC_TAU / 250.0;
        ArcadeInput in = none();
        loop_lap_step(&s, &in, TICK);
        CHECK(!s.over);
        /* cruise: near when <= 24 + enemy / 2 + 16 -> 56 for the small enemy, 72 for the big one */
        CHECK_EQ_INT(s.whooshes, big ? 1 : 0);
    }
}

static void test_same_seed_same_enemy_sizes(void) {
    loop_lap_State a, b;
    loop_lap_init(&a, 21);
    loop_lap_init(&b, 21);
    for (int i = 0; i < 4; i++) {
        join_next_enemy(&a);
        join_next_enemy(&b);
        CHECK_NEAR(a.esize[i], b.esize[i], 0.0f);
    }
}

static void test_enemy_speed_range(void) {
    for (unsigned seed = 1; seed <= 50; seed++) {
        loop_lap_State s;
        loop_lap_init(&s, seed);
        s.score = 0;
        s.my_a = (double)ARC_TAU - 0.001;
        ArcadeInput in = none();
        loop_lap_step(&s, &in, TICK);
        CHECK_EQ_INT(s.enemy_n, 1);
        CHECK(s.ew[0] >= 0.00670206 - 1e-9 && s.ew[0] <= 0.00670206 + 0.03518584 + 1e-9);
    }
}

static void test_near_miss_whoosh_is_rate_limited(void) {
    loop_lap_State s;
    loop_lap_init_sized(&s, 1, 480, 800);
    s.c2x = s.c1x;
    s.c2y = s.c1y;
    s.enemy_n = 1;
    s.ew[0] = 0.0;
    s.ea[0] = 1.0 + 2.0 * asin(60.0 / (2.0 * 177.0)); /* 60 px away: near (<= 64) but not touching */
    s.my_a = 1.0 - ARC_TAU / 250.0;
    ArcadeInput in = none();
    loop_lap_step(&s, &in, TICK);
    CHECK(!s.over);
    CHECK_EQ_INT(s.whooshes, 1);
    s.my_a -= ARC_TAU / 250.0; /* keep it near */
    loop_lap_step(&s, &in, TICK);
    CHECK_EQ_INT(s.whooshes, 1); /* cool-down */
    for (int i = 0; i < 60; i++) {
        s.my_a = 1.0 - ARC_TAU / 250.0;
        loop_lap_step(&s, &in, TICK);
    }
    CHECK(s.whooshes >= 2);
}

static void test_pause_with_p(void) {
    loop_lap_State s;
    loop_lap_init(&s, 1);
    ArcadeInput in = none();
    in.typed = 'p';
    loop_lap_step(&s, &in, TICK);
    CHECK(s.paused);
    double a = s.my_a;
    in.typed = 0;
    loop_lap_step(&s, &in, TICK);
    CHECK_NEAR(s.my_a, a, 1e-12);
    in.typed = 'P';
    loop_lap_step(&s, &in, TICK);
    CHECK(!s.paused);
    loop_lap_step(&s, &in, TICK);
    CHECK(s.my_a > a);
}

static void test_crash_ends_the_game_after_the_reveal(void) {
    loop_lap_State s;
    loop_lap_init(&s, 1);
    s.c2x = s.c1x;
    s.c2y = s.c1y;
    s.enemy_n = 1;
    s.ew[0] = 0.0;
    s.ea[0] = s.my_a + 0.03; /* right in front of the ball */
    ArcadeInput in = none();
    loop_lap_step(&s, &in, ARC_DT);
    CHECK(s.over);
    CHECK(!loop_lap_is_over(&s));
    for (int i = 0; i < 70 && !loop_lap_is_over(&s); i++)
        loop_lap_step(&s, &in, ARC_DT);
    CHECK(loop_lap_is_over(&s));
}

/* A scripted player that only accelerates meets an enemy sooner or later: no test relies on the monkey. */
static void test_accelerating_player_ends_the_game(void) {
    for (unsigned seed = 1; seed <= 5; seed++) {
        loop_lap_State s;
        loop_lap_init(&s, seed);
        ArcadeInput in = mode_input(LL_ACCEL);
        int frames = 0;
        while (!loop_lap_is_over(&s) && frames++ < 60 * 3600)
            loop_lap_step(&s, &in, ARC_DT);
        CHECK(loop_lap_is_over(&s));
        CHECK(loop_lap_score(&s) >= 1);
    }
}

static void test_same_seed_same_result(void) {
    loop_lap_State a, b;
    loop_lap_init(&a, 9);
    loop_lap_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        loop_lap_step(&a, &ia, ARC_DT);
        loop_lap_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.score, b.score);
    CHECK_NEAR(a.my_a, b.my_a, 1e-9);
    CHECK_EQ_INT(a.over, b.over);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        loop_lap_State s;
        loop_lap_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 5);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            loop_lap_step(&s, &in, ARC_DT);
            CHECK(s.score >= 0);
            CHECK(s.enemy_n >= 0 && s.enemy_n <= LL_MAX_ENEMIES);
            CHECK(s.mode >= LL_BRAKE && s.mode <= LL_ACCEL);
            CHECK(isfinite(s.my_pos.x) && isfinite(s.my_pos.y));
            CHECK(s.my_a >= 0.0 && s.my_a <= (double)ARC_TAU + 0.1);
            for (int j = 0; j < s.enemy_n; j++) {
                CHECK(isfinite(s.enemy_pos[j].x) && isfinite(s.enemy_pos[j].y));
                CHECK(s.esize[j] >= LL_ENEMY_W_MIN && s.esize[j] <= LL_ENEMY_W_MAX);
            }
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_geometry_at_480x800);
    RUN(test_geometry_fits_the_684_playfield);
    RUN(test_input_mapping_table);
    RUN(test_lap_times_at_50hz);
    RUN(test_lap_times_at_60hz);
    RUN(test_brake_and_accelerate_lap_lengths);
    RUN(test_hit_distances);
    RUN(test_collision_thresholds);
    RUN(test_enemies_join_at_the_spec_laps);
    RUN(test_enemy_sizes_are_random_within_the_range);
    RUN(test_enemies_of_one_run_differ_in_size);
    RUN(test_unused_slots_default_to_the_spec_size);
    RUN(test_adding_sizes_did_not_change_angle_or_speed);
    RUN(test_collision_uses_the_enemys_own_size);
    RUN(test_near_miss_distance_uses_the_enemys_own_size);
    RUN(test_same_seed_same_enemy_sizes);
    RUN(test_enemy_speed_range);
    RUN(test_near_miss_whoosh_is_rate_limited);
    RUN(test_pause_with_p);
    RUN(test_crash_ends_the_game_after_the_reveal);
    RUN(test_accelerating_player_ends_the_game);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
