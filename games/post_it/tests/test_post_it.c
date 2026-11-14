#include "arcade/playfield.h"
#include "arcade_test.h"
#include "post_it_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

/* input with the pointer at a playfield-LOCAL position (sent as screen pixels like the real input) */
static ArcadeInput pointer_at(float lx, float ly) {
    ArcadeInput in;
    arcade_input_clear(&in);
    Vec2 p = arc_pf_to_screen(lx, ly);
    in.aim_x = p.x;
    in.aim_y = p.y;
    return in;
}

/* a state at 480 x 800 with every free item parked far away, so a test controls what happens */
static void parked(post_it_State *s, unsigned seed) {
    post_it_init_sized(s, seed, 480, 800);
    for (int i = 0; i < PI_GREENS; i++) {
        s->gx[i] = 10.0f + (float)i;
        s->gy[i] = 10.0f;
        s->grot[i] = 0.0f;
    }
    for (int i = 0; i < PI_REDS; i++) {
        s->rx[i] = 470.0f - (float)i;
        s->ry[i] = 790.0f;
        s->rrot[i] = 0.0f;
    }
}

static ArcadeInput centre(void) {
    return pointer_at(240.0f, 400.0f); /* no tilt */
}

static void test_init_is_sane(void) {
    post_it_State s;
    post_it_init(&s, 1);
    CHECK(!post_it_is_over(&s));
    CHECK_EQ_INT(post_it_score(&s), 1);
    CHECK_NEAR(s.head.x, 240.0, 0.001);
    CHECK_NEAR(s.head.y, 342.0, 0.001);
}

static void test_viruses_start_at_least_144_px_from_the_head(void) {
    for (unsigned seed = 1; seed <= 40; seed++) {
        post_it_State s;
        post_it_init_sized(&s, seed, 480, 800);
        for (int i = 0; i < PI_REDS; i++)
            CHECK(vec2_dist(vec2(s.rx[i], s.ry[i]), s.head) >= 144.0f);
    }
}

/* spec vectors: acc (0.3, -0.2) -> drift (2.4, 1.6); acc 1.0 clamps to 0.5 -> 4 */
static void test_tilt_to_drift_vectors(void) {
    float sx, sy;
    post_it_drift_from_tilt(0.3f, -0.2f, &sx, &sy);
    CHECK_NEAR(sx, 2.4, 0.0001);
    CHECK_NEAR(sy, 1.6, 0.0001);
    CHECK_NEAR(sqrtf(sx * sx + sy * sy), 2.884, 0.001);
    post_it_drift_from_tilt(1.0f, 0.0f, &sx, &sy);
    CHECK_NEAR(sx, 4.0, 0.0001);
    post_it_drift_from_tilt(-1.0f, 1.0f, &sx, &sy);
    CHECK_NEAR(sx, -4.0, 0.0001);
    CHECK_NEAR(sy, -4.0, 0.0001); /* pointer above the centre: world drifts up (y decreases) */
}

static void test_pointer_and_keys_to_tilt(void) {
    float ax, ay;
    ArcadeInput in = pointer_at(240.0f, 342.0f); /* centre of the 480 x 684 playfield */
    post_it_tilt_from_input(&in, 480, 684, &ax, &ay);
    CHECK_NEAR(ax, 0.0, 0.0001);
    CHECK_NEAR(ay, 0.0, 0.0001);
    in = pointer_at(240.0f + 120.0f, 342.0f - 120.0f); /* 120 px right and above the centre */
    post_it_tilt_from_input(&in, 480, 684, &ax, &ay);
    CHECK_NEAR(ax, 0.5, 0.0001);
    CHECK_NEAR(ay, 0.5, 0.0001);
    in = pointer_at(-500.0f, 342.0f); /* far outside: saturates at -1 */
    post_it_tilt_from_input(&in, 480, 684, &ax, &ay);
    CHECK_NEAR(ax, -1.0, 0.0001);
    in = pointer_at(240.0f, 342.0f);
    in.move_x = 1.0f; /* held keys override the pointer: right + down */
    in.move_y = 1.0f;
    post_it_tilt_from_input(&in, 480, 684, &ax, &ay);
    CHECK_NEAR(ax, 0.5, 0.0001);
    CHECK_NEAR(ay, -0.5, 0.0001);
}

static void test_scale_index_vectors(void) {
    CHECK_EQ_INT(post_it_scale_index(2), 2);
    CHECK_EQ_INT(post_it_scale_index(8), 8);
    CHECK_EQ_INT(post_it_scale_index(9), 8);
    CHECK_EQ_INT(post_it_scale_index(10), 7);
    CHECK_EQ_INT(post_it_scale_index(15), 2);
    CHECK_EQ_INT(post_it_scale_index(16), 1);
    CHECK_EQ_INT(post_it_scale_index(17), 1);
    CHECK_EQ_INT(post_it_scale_index(18), 2);
}

static void test_item_moves_with_world_plus_half_speed_heading(void) {
    post_it_State s;
    parked(&s, 1);
    s.gx[0] = 100.0f;
    s.gy[0] = 100.0f;
    s.grot[0] = 0.0f;                                            /* heading along +x */
    ArcadeInput in = pointer_at(240.0f + 72.0f, 400.0f + 48.0f); /* tilt (0.3, -0.2) */
    post_it_step(&s, &in, TICK);
    CHECK_NEAR(s.gx[0], 100.0 + 2.4 + 1.442, 0.001);
    CHECK_NEAR(s.gy[0], 100.0 + 1.6, 0.001);
}

static void test_wrap_uses_a_new_heading(void) {
    post_it_State s;
    parked(&s, 2);
    s.gx[0] = 497.0f; /* > W + 16 */
    s.gy[0] = 100.0f;
    s.grot[0] = 33.0f;
    ArcadeInput in = centre(); /* no drift: the wrap alone moves it */
    post_it_step(&s, &in, TICK);
    CHECK_NEAR(s.gx[0], 497.0 - 511.0, 0.001); /* -14 */
    CHECK(fabsf(s.grot[0] - 33.0f) > 0.0001f);
    s.gx[1] = -17.0f; /* < -16 wraps the other way */
    post_it_step(&s, &in, TICK);
    CHECK_NEAR(s.gx[1], -17.0 + 511.0, 0.001);
    s.gy[2] = 817.0f; /* > H + 16 */
    post_it_step(&s, &in, TICK);
    CHECK_NEAR(s.gy[2], 817.0 - 831.0, 0.001);
}

static void test_grab_radius_23_9_versus_24(void) {
    post_it_State s;
    parked(&s, 3);
    s.gx[0] = 240.0f + 24.0f; /* exactly 24.0: not grabbed */
    s.gy[0] = 400.0f;
    ArcadeInput in = centre();
    post_it_step(&s, &in, TICK);
    CHECK_EQ_INT(s.blockn, 1);
    s.gx[0] = 240.0f + 23.9f;
    post_it_step(&s, &in, TICK);
    CHECK(s.blockn >= 2);
    CHECK(s.grabs >= 1);
    CHECK_NEAR(s.bx[1], 263.9, 0.01); /* the block is glued where the post-it touched */
}

static void test_any_block_counts_not_only_the_head(void) {
    post_it_State s;
    parked(&s, 4);
    s.blockn = 2;
    s.bx[1] = 100.0f;
    s.by[1] = 300.0f;
    s.gx[0] = 100.0f + 20.0f; /* touches block 1 only */
    s.gy[0] = 300.0f;
    ArcadeInput in = centre();
    post_it_step(&s, &in, TICK);
    CHECK(s.blockn >= 3);
}

static void test_virus_kill_and_warning_radii(void) {
    post_it_State s;
    parked(&s, 5);
    s.rx[0] = 240.0f + 47.9f; /* near but not touching */
    s.ry[0] = 400.0f;
    ArcadeInput in = centre();
    post_it_step(&s, &in, TICK);
    CHECK(!s.over);
    CHECK(s.rnear[0]);
    CHECK_EQ_INT(s.near_count, 1);
    s.rx[0] = 240.0f + 48.0f; /* exactly 48.0: not near */
    post_it_step(&s, &in, TICK);
    CHECK(!s.rnear[0]);
    CHECK_EQ_INT(s.near_count, 0);
    s.rx[0] = 240.0f + 24.0f; /* exactly 24.0: still alive */
    post_it_step(&s, &in, TICK);
    CHECK(!s.over);
    s.rx[0] = 240.0f + 23.9f;
    post_it_step(&s, &in, TICK);
    CHECK(s.over);
    CHECK_EQ_INT(s.killer, 0);
    CHECK(!post_it_is_over(&s)); /* the reveal delay is running */
    for (int i = 0; i < 80 && !post_it_is_over(&s); i++)
        post_it_step(&s, &in, ARC_DT);
    CHECK(post_it_is_over(&s));
    CHECK_EQ_INT(post_it_score(&s), 1);
}

static void test_cluster_cap_ends_the_game(void) {
    post_it_State s;
    parked(&s, 6);
    s.blockn = PI_MAX_BLOCKS - 1;
    for (int j = 1; j < s.blockn; j++) { /* old blocks far from everything */
        s.bx[j] = 1000.0f;
        s.by[j] = 1000.0f;
    }
    s.gx[0] = 240.0f + 10.0f;
    s.gy[0] = 400.0f;
    ArcadeInput in = centre();
    post_it_step(&s, &in, TICK);
    CHECK_EQ_INT(s.blockn, PI_MAX_BLOCKS);
    CHECK(s.over && s.capped);
    post_it_step(&s, &in, TICK);
    CHECK(s.blockn <= PI_MAX_BLOCKS);
    for (int i = 0; i < 80; i++)
        post_it_step(&s, &in, ARC_DT);
    CHECK(post_it_is_over(&s));
    CHECK_EQ_INT(post_it_score(&s), PI_MAX_BLOCKS);
}

static void test_pause_by_key_and_click(void) {
    post_it_State s;
    parked(&s, 7);
    s.gx[0] = 100.0f;
    s.gy[0] = 100.0f;
    ArcadeInput in = pointer_at(400.0f, 400.0f); /* strong tilt */
    in.typed = 'p';
    post_it_step(&s, &in, TICK);
    CHECK(s.paused);
    float x = s.gx[0];
    in.typed = 0;
    post_it_step(&s, &in, TICK);
    CHECK_NEAR(s.gx[0], x, 0.0001); /* nothing moves while paused */
    in.fire_pressed = true;         /* a click resumes */
    post_it_step(&s, &in, TICK);
    CHECK(!s.paused);
    CHECK(s.gx[0] != x);
}

/* The same second of movement at 60 Hz and at 50 Hz ends at the same place. */
static void test_dt_consistency(void) {
    post_it_State a, b;
    parked(&a, 8);
    parked(&b, 8);
    a.gx[0] = b.gx[0] = 200.0f;
    a.gy[0] = b.gy[0] = 200.0f;
    a.grot[0] = b.grot[0] = 40.0f;
    ArcadeInput in = pointer_at(240.0f + 30.0f, 400.0f - 30.0f); /* tilt (0.125, 0.125) */
    for (int i = 0; i < 50; i++)
        post_it_step(&a, &in, TICK);
    for (int i = 0; i < 60; i++)
        post_it_step(&b, &in, ARC_DT);
    CHECK_NEAR(a.gx[0], b.gx[0], 0.05);
    CHECK_NEAR(a.gy[0], b.gy[0], 0.05);
}

static void test_same_seed_same_result(void) {
    post_it_State a, b;
    post_it_init(&a, 9);
    post_it_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        post_it_step(&a, &ia, ARC_DT);
        post_it_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.blockn, b.blockn);
    CHECK_NEAR(a.rx[3], b.rx[3], 0.0001);
    CHECK_EQ_INT(a.over, b.over);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        post_it_State s;
        post_it_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 30);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            post_it_step(&s, &in, ARC_DT);
            CHECK(s.blockn >= 1 && s.blockn <= PI_MAX_BLOCKS);
            CHECK(post_it_score(&s) >= 1);
            CHECK(s.near_count >= 0 && s.near_count <= PI_REDS);
            for (int j = 0; j < PI_GREENS; j++)
                CHECK(isfinite(s.gx[j]) && s.gx[j] >= -16.01f && s.gx[j] <= (float)s.w + 16.01f &&
                      s.gy[j] >= -16.01f && s.gy[j] <= (float)s.h + 16.01f);
            for (int j = 0; j < PI_REDS; j++)
                CHECK(isfinite(s.rx[j]) && s.rx[j] >= -16.01f && s.rx[j] <= (float)s.w + 16.01f &&
                      s.ry[j] >= -16.01f && s.ry[j] <= (float)s.h + 16.01f);
            CHECK(s.last_note >= 0 && s.last_note < 8);
        }
    }
}

/* With zero tilt the speed v is 0, and an item's own heading is scaled by v: the whole world freezes. */
static void test_zero_tilt_freezes_the_world(void) {
    post_it_State s;
    post_it_init(&s, 3);
    float gx = s.gx[0], rx = s.rx[0];
    ArcadeInput in = pointer_at(240.0f, 342.0f);
    for (int i = 0; i < 600; i++)
        post_it_step(&s, &in, ARC_DT);
    CHECK_NEAR(s.gx[0], gx, 0.0001);
    CHECK_NEAR(s.rx[0], rx, 0.0001);
    CHECK(!s.over);
}

/* A scripted player that keeps a constant tilt: viruses keep sweeping across the playfield, so a
 * virus eventually touches the cluster. The round ends by itself (no test relies on the monkey). */
static void test_constant_tilt_player_ends_the_game(void) {
    for (unsigned seed = 1; seed <= 5; seed++) {
        post_it_State s;
        post_it_init(&s, seed);
        ArcadeInput in = pointer_at(240.0f + 150.0f, 342.0f - 60.0f);
        int frames = 0;
        while (!post_it_is_over(&s) && frames++ < 60 * 600)
            post_it_step(&s, &in, ARC_DT);
        CHECK(post_it_is_over(&s));
        CHECK(s.killer >= 0 || s.capped);
        CHECK(post_it_score(&s) >= 1);
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_viruses_start_at_least_144_px_from_the_head);
    RUN(test_tilt_to_drift_vectors);
    RUN(test_pointer_and_keys_to_tilt);
    RUN(test_scale_index_vectors);
    RUN(test_item_moves_with_world_plus_half_speed_heading);
    RUN(test_wrap_uses_a_new_heading);
    RUN(test_grab_radius_23_9_versus_24);
    RUN(test_any_block_counts_not_only_the_head);
    RUN(test_virus_kill_and_warning_radii);
    RUN(test_cluster_cap_ends_the_game);
    RUN(test_pause_by_key_and_click);
    RUN(test_dt_consistency);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    RUN(test_zero_tilt_freezes_the_world);
    RUN(test_constant_tilt_player_ends_the_game);
    return TEST_SUMMARY();
}
