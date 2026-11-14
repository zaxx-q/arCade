#include <stdlib.h>

#include "arcade/cli.h"
#include "arcade/fx.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/playfield.h"
#include "arcade/rng.h"
#include "arcade/str.h"
#include "arcade_test.h"

static void test_rng_is_deterministic(void) {
    Rng a, b;
    rng_seed(&a, 42);
    rng_seed(&b, 42);
    for (int i = 0; i < 100; i++)
        CHECK_EQ_INT(rng_u32(&a), rng_u32(&b));
    rng_seed(&b, 43);
    rng_seed(&a, 42);
    CHECK(rng_u32(&a) != rng_u32(&b));
}

static void test_rng_ranges(void) {
    Rng r;
    rng_seed(&r, 7);
    int seen[6] = {0};
    for (int i = 0; i < 6000; i++) {
        int v = rng_range(&r, 1, 6);
        CHECK(v >= 1 && v <= 6);
        seen[v - 1]++;
        float f = rng_float(&r);
        CHECK(f >= 0.0f && f < 1.0f);
    }
    for (int i = 0; i < 6; i++)
        CHECK(seen[i] > 700); /* roughly uniform (expected 1000) */
    CHECK_EQ_INT(rng_range(&r, 5, 5), 5);
    CHECK_EQ_INT(rng_range(&r, 9, 3), 9); /* hi <= lo returns lo */
}

static void test_strings(void) {
    char buf[5];
    CHECK_EQ_INT(arc_strlcpy(buf, "abcdefgh", sizeof(buf)), 8);
    CHECK_STR_EQ(buf, "abcd");
    CHECK(arc_stricmp("Hello", "hELLO") == 0);
    CHECK(arc_stricmp("abc", "abd") < 0);
    CHECK(arc_stricmp("b", "abc") > 0);
    CHECK(arc_contains_nocase("Alice Wonder", "WON"));
    CHECK(!arc_contains_nocase("Alice", "bob"));
    CHECK(arc_contains_nocase("anything", ""));
}

static void test_vec2_and_collision(void) {
    Vec2 v = vec2_norm(vec2(3.0f, 4.0f));
    CHECK_NEAR(vec2_len(v), 1.0, 0.0001);
    CHECK_NEAR(vec2_len(vec2_norm(vec2(0, 0))), 0.0, 0.0001);
    CHECK_NEAR(vec2_dist(vec2(0, 0), vec2(3, 4)), 5.0, 0.0001);
    CHECK(arc_circles_overlap(vec2(0, 0), 5, vec2(8, 0), 4));
    CHECK(!arc_circles_overlap(vec2(0, 0), 5, vec2(20, 0), 4));
    CHECK(arc_rects_overlap(0, 0, 10, 10, 5, 5, 10, 10));
    CHECK(!arc_rects_overlap(0, 0, 10, 10, 11, 0, 5, 5));
    CHECK(arc_point_in_rect(vec2(5, 5), 0, 0, 10, 10));
    CHECK_EQ_INT(arc_clampi(15, 0, 10), 10);
    CHECK_NEAR(arc_clampf(-1.0f, 0.0f, 1.0f), 0.0, 0.0001);
}

static void test_monkey_is_reproducible(void) {
    ArcadeMonkey a, b;
    ArcadeInput ia, ib;
    arcade_monkey_init(&a, 5);
    arcade_monkey_init(&b, 5);
    for (int i = 0; i < 500; i++) {
        arcade_monkey_next(&a, &ia);
        arcade_monkey_next(&b, &ib);
        CHECK(memcmp(&ia, &ib, sizeof(ia)) == 0);
        CHECK(ia.move_x >= -1.0f && ia.move_x <= 1.0f);
        CHECK(ia.aim_x >= 0.0f && ia.aim_x <= (float)ARC_SCREEN_W);
    }
}

static void test_particles_and_shake(void) {
    ParticleSystem ps;
    fx_init(&ps, 1);
    CHECK_EQ_INT(fx_alive(&ps), 0);
    fx_burst(&ps, 100, 100, 50, 200, 0.5f, 3, 255, 0, 0);
    CHECK_EQ_INT(fx_alive(&ps), 50);
    for (int i = 0; i < 60; i++)
        fx_update(&ps, ARC_DT);
    CHECK_EQ_INT(fx_alive(&ps), 0);                                     /* all expired after 1 second */
    fx_burst(&ps, 0, 0, FX_MAX_PARTICLES + 100, 100, 1.0f, 2, 1, 2, 3); /* ring buffer wraps, no overflow */
    CHECK_EQ_INT(fx_alive(&ps), FX_MAX_PARTICLES);

    Shake s;
    Rng r;
    rng_seed(&r, 3);
    memset(&s, 0, sizeof(s));
    CHECK_NEAR(shake_offset(&s, &r).x, 0.0, 0.0001);
    shake_start(&s, 10.0f, 0.5f);
    Vec2 o = shake_offset(&s, &r);
    CHECK(fabsf(o.x) <= 10.0f && fabsf(o.y) <= 10.0f);
    for (int i = 0; i < 40; i++)
        shake_update(&s, ARC_DT);
    CHECK_NEAR(shake_offset(&s, &r).x, 0.0, 0.0001);
}

static void test_cli(void) {
    CliOptions o;
    char *ok[] = {"hub", "--smoke", "all", "--frames", "120", "--seed", "9", "--out", "shots"};
    CHECK(cli_parse(9, ok, &o));
    CHECK(o.smoke);
    CHECK_STR_EQ(o.smoke_target, "all");
    CHECK_EQ_INT(o.frames, 120);
    CHECK_EQ_INT(o.seed, 9);
    CHECK_STR_EQ(o.out_dir, "shots");

    char *none[] = {"hub"};
    CHECK(cli_parse(1, none, &o));
    CHECK(!o.smoke && !o.show_help && !o.show_version && !o.list_games);
    CHECK_EQ_INT(o.frames, 300);

    char *flags[] = {"hub", "--version", "--list", "-h"};
    CHECK(cli_parse(4, flags, &o));
    CHECK(o.show_version && o.list_games && o.show_help);

    char *bad1[] = {"hub", "--nope"};
    CHECK(!cli_parse(2, bad1, &o));
    CHECK(strlen(o.error) > 0);
    char *bad2[] = {"hub", "--frames", "abc"};
    CHECK(!cli_parse(3, bad2, &o));
    char *bad3[] = {"hub", "--smoke"};
    CHECK(!cli_parse(2, bad3, &o));
    char *bad4[] = {"hub", "--frames", "0"};
    CHECK(!cli_parse(3, bad4, &o));
}

static void test_playfield_helpers(void) {
    CHECK_EQ_INT(ARC_PF_X, 400);
    CHECK_EQ_INT(ARC_PF_X + ARC_PF_W, 880);
    CHECK_EQ_INT(ARC_PF_Y + ARC_PF_H, ARC_SCREEN_H);
    Vec2 local = arc_pf_from_screen(640.0f, 378.0f); /* centre of the playfield */
    CHECK_NEAR(local.x, ARC_PF_W / 2.0f, 0.001);
    CHECK_NEAR(local.y, ARC_PF_H / 2.0f, 0.001);
    Vec2 back = arc_pf_to_screen(local.x, local.y);
    CHECK_NEAR(back.x, 640.0f, 0.001);
    CHECK_NEAR(back.y, 378.0f, 0.001);
    CHECK(arc_pf_contains(vec2(0.0f, 0.0f), ARC_PF_W, ARC_PF_H));
    CHECK(!arc_pf_contains(vec2(-1.0f, 10.0f), ARC_PF_W, ARC_PF_H));           /* left gutter */
    CHECK(!arc_pf_contains(vec2((float)ARC_PF_W, 10.0f), ARC_PF_W, ARC_PF_H)); /* right edge is outside */
}

int main(void) {
    RUN(test_rng_is_deterministic);
    RUN(test_rng_ranges);
    RUN(test_strings);
    RUN(test_vec2_and_collision);
    RUN(test_monkey_is_reproducible);
    RUN(test_particles_and_shake);
    RUN(test_cli);
    RUN(test_playfield_helpers);
    return TEST_SUMMARY();
}
