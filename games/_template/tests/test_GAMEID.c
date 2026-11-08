#include "@ID@_logic.h"
#include "arcade_test.h"

static void test_init_is_sane(void) {
    @ID@_State s;
    @ID@_init(&s, 1);
    CHECK(!@ID@_is_over(&s));
    CHECK_EQ_INT(@ID@_score(&s), 0);
}

static void test_same_seed_same_result(void) {
    @ID@_State a, b;
    @ID@_init(&a, 9);
    @ID@_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 600; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        @ID@_step(&a, &ia, ARC_DT);
        @ID@_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(@ID@_score(&a), @ID@_score(&b));
    CHECK_NEAR(a.pos.x, b.pos.x, 0.0001);
}

static void test_monkey_never_breaks_invariants(void) {
    @ID@_State s;
    @ID@_init(&s, 5);
    ArcadeMonkey m;
    arcade_monkey_init(&m, 11);
    for (int i = 0; i < 5000; i++) {
        ArcadeInput in;
        arcade_monkey_next(&m, &in);
        @ID@_step(&s, &in, ARC_DT);
        CHECK(s.pos.x >= 0.0f && s.pos.x <= (float)ARC_SCREEN_W);
        CHECK(s.pos.y >= 0.0f && s.pos.y <= (float)ARC_SCREEN_H);
        CHECK(@ID@_score(&s) >= 0);
    }
    CHECK(@ID@_is_over(&s)); /* the round ends by itself after ROUND_SECONDS */
}

/* TODO(@OWNER@): add a test for every rule you implement (collisions, scoring, win/lose...). */

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
