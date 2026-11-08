#include <math.h>

#include "absorb_grow_logic.h"
#include "arcade_test.h"

static ArcadeInput idle(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    return in;
}

/* Park every blob in a harmless corner so a test controls exactly what the player touches. */
static void park_all(absorb_grow_State *s) {
    for (int i = 0; i < AG_MAX_BLOBS; i++) {
        s->blobs[i].r = 5.0f;
        s->blobs[i].pos = vec2(20.0f, 70.0f);
        s->blobs[i].vel = vec2(0.0f, 0.0f);
    }
}

static void put_blob(absorb_grow_State *s, float r) {
    s->blobs[0].r = r;
    s->blobs[0].pos = s->pos;
    s->blobs[0].vel = vec2(0.0f, 0.0f);
}

static void test_init_is_sane(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    CHECK_NEAR(s.r, AG_START_R, 0.0001);
    CHECK(!absorb_grow_is_over(&s));
    for (int i = 0; i < AG_MAX_BLOBS; i++) {
        CHECK(s.blobs[i].r >= 5.0f && s.blobs[i].r <= 200.0f);
        CHECK(vec2_dist(s.blobs[i].pos, s.pos) > s.r * 0.5f); /* nothing spawns on top of the player */
    }
}

static void test_swallowing_a_small_blob_adds_area(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    park_all(&s);
    put_blob(&s, 8.0f);
    float before = s.r;
    ArcadeInput in = idle();
    absorb_grow_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.absorbed, 1);
    CHECK_NEAR(s.r, sqrtf(before * before + 0.7f * 8.0f * 8.0f), 0.001);
    CHECK_EQ_INT(s.absorb_events, 1);
    CHECK(s.blobs[0].r >= 5.0f); /* the swallowed blob respawned somewhere else */
}

static void test_a_big_blob_ends_the_game(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    park_all(&s);
    put_blob(&s, 60.0f);
    ArcadeInput in = idle();
    absorb_grow_step(&s, &in, ARC_DT);
    CHECK(absorb_grow_is_over(&s));
    CHECK(!s.won);
}

static void test_similar_size_is_harmless(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    park_all(&s);
    put_blob(&s, s.r); /* same size: neither side can swallow the other */
    ArcadeInput in = idle();
    absorb_grow_step(&s, &in, ARC_DT);
    CHECK(!absorb_grow_is_over(&s));
    CHECK_EQ_INT(s.absorbed, 0);
}

static void test_reaching_the_win_size(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    park_all(&s);
    s.r = AG_WIN_R - 0.1f;
    put_blob(&s, 20.0f);
    ArcadeInput in = idle();
    absorb_grow_step(&s, &in, ARC_DT);
    CHECK(s.won);
    CHECK(absorb_grow_is_over(&s));
    CHECK(absorb_grow_score(&s) > 500 + (int)(AG_WIN_R * 10.0f) - 1);
}

static void test_time_runs_out(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 1);
    park_all(&s);
    ArcadeInput in = idle();
    for (int i = 0; i < 60 * 91 && !absorb_grow_is_over(&s); i++)
        absorb_grow_step(&s, &in, ARC_DT);
    CHECK(absorb_grow_is_over(&s));
    CHECK(!s.won);
    CHECK_NEAR(s.time_left, 0.0, 0.0001);
}

static void test_monkey_never_shrinks_or_leaves_the_arena(void) {
    absorb_grow_State s;
    absorb_grow_init(&s, 3);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 9);
    float prev = s.r;
    for (int i = 0; i < 6000 && !absorb_grow_is_over(&s); i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        absorb_grow_step(&s, &in, ARC_DT);
        CHECK(s.r >= prev); /* a blob only ever grows */
        prev = s.r;
        CHECK(s.pos.x >= 0 && s.pos.x <= ARC_SCREEN_W && s.pos.y >= 0 && s.pos.y <= ARC_SCREEN_H);
    }
    CHECK(absorb_grow_score(&s) >= 0);
}

static void test_deterministic(void) {
    absorb_grow_State a, b;
    absorb_grow_init(&a, 11);
    absorb_grow_init(&b, 11);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 2);
    arcade_monkey_init(&mb, 2);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        absorb_grow_step(&a, &ia, ARC_DT);
        absorb_grow_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(absorb_grow_score(&a), absorb_grow_score(&b));
    CHECK_EQ_INT(a.absorbed, b.absorbed);
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_swallowing_a_small_blob_adds_area);
    RUN(test_a_big_blob_ends_the_game);
    RUN(test_similar_size_is_harmless);
    RUN(test_reaching_the_win_size);
    RUN(test_time_runs_out);
    RUN(test_monkey_never_shrinks_or_leaves_the_arena);
    RUN(test_deterministic);
    return TEST_SUMMARY();
}
