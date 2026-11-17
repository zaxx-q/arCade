#include "arcade_test.h"
#include "spike_dart_logic.h"

#define TICK 0.02f /* one tick of the original 50 Hz game: k = 1 */

static ArcadeInput throw_input(void) {
    ArcadeInput in;
    arcade_input_clear(&in);
    in.fire_pressed = true;
    return in;
}

static void idle(spike_dart_State *s, int ticks, float dt) {
    ArcadeInput none;
    arcade_input_clear(&none);
    for (int i = 0; i < ticks; i++)
        spike_dart_step(s, &none, dt);
}

/* throws one spike and steps until it sticks (or gives up) */
static int throw_and_wait(spike_dart_State *s, float dt) {
    ArcadeInput in = throw_input();
    int n = 0;
    int before = s->stuck;
    spike_dart_step(s, &in, dt);
    n++;
    while (s->stuck == before && !s->over && n < 200) {
        idle(s, 1, dt);
        n++;
    }
    return n;
}

static void test_init_is_sane(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    CHECK(!spike_dart_is_over(&s));
    CHECK_EQ_INT(spike_dart_score(&s), 0);
    CHECK_NEAR(s.target_x, 240.0, 0.001);
}

static void test_spec_constants_at_480x800(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    CHECK_NEAR(s.target_y, 340.0, 0.001);
    CHECK_NEAR(s.target_dn, 460.0, 0.001);
    CHECK_NEAR(s.spike_y, 720.0, 0.001);
    CHECK_NEAR(s.spike_spd, 22.8, 0.001); /* KNIFEDIST 228 / 10 */
}

static void test_first_spike_sticks_in_10_ticks_at_orbit_radius(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    int ticks = throw_and_wait(&s, TICK);
    CHECK_EQ_INT(ticks, 10);
    CHECK_EQ_INT(s.stuck, 1);
    CHECK_NEAR(s.px[0], 240.0, 0.01);
    CHECK_NEAR(s.py[0], 492.0, 0.01); /* TARGETY + 152 */
}

static void test_orbit_after_one_second(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    throw_and_wait(&s, TICK);
    idle(&s, 50, TICK); /* 1 s: a quarter turn */
    CHECK_NEAR(s.px[0], 392.0, 0.05);
    CHECK_NEAR(s.py[0], 340.0, 0.05);
}

static void test_orbit_period_at_60hz(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    throw_and_wait(&s, ARC_DT);
    float x0 = s.px[0], y0 = s.py[0];
    float r0 = s.rot[0];
    idle(&s, 240, ARC_DT); /* 4 s = one full turn */
    CHECK_NEAR(s.rot[0] - r0, ARC_TAU, 0.01);
    CHECK_NEAR(s.px[0], x0, 1.5);
    CHECK_NEAR(s.py[0], y0, 1.5);
}

/* ---- the hit box is the drawn triangle -------------------------------------------------------------- */

static spike_dart_State *board(void) { /* 480 x 800: lane x = 240, target bottom y = 460, landing y = 492 */
    static spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    return &s;
}

/* Puts stuck spike i on the orbit at angle `a` (the same formula the game uses while orbiting). */
static void stick_at_angle(spike_dart_State *s, int i, float a) {
    s->rot[i] = a;
    s->px[i] = s->target_x - cosf(a) * SD_ORBIT_R;
    s->py[i] = s->target_y + sinf(a) * SD_ORBIT_R;
    if (s->stuck <= i)
        s->stuck = i + 1;
}

static void test_shape_is_the_drawn_triangle(void) {
    Vec2 t[3];
    spike_dart_shape(240.0f, 492.0f, ARC_PI / 2.0f, t); /* hanging below the target, tip up */
    CHECK_NEAR(t[0].x, 240.0, 0.01);
    CHECK_NEAR(t[0].y, 452.0, 0.01); /* tip: 40 px above the centre */
    CHECK_NEAR(t[1].x, 244.0, 0.01);
    CHECK_NEAR(t[1].y, 532.0, 0.01); /* flat end: 40 px below, 8 px wide */
    CHECK_NEAR(t[2].x, 236.0, 0.01);
    CHECK_NEAR(t[2].y, 532.0, 0.01);
    spike_dart_shape(100.0f, 100.0f, 0.0f, t); /* angle 0: tip points to +x */
    CHECK_NEAR(t[0].x, 140.0, 0.01);
    CHECK_NEAR(t[0].y, 100.0, 0.01);
    CHECK_NEAR(t[1].x, 60.0, 0.01);
    CHECK_NEAR(fabs(t[1].y - t[2].y), 8.0, 0.01);
    for (int i = 0; i < 100; i++) { /* any angle: 80 px long, 8 px wide flat end */
        float a = (float)i * 0.0628f;
        spike_dart_shape(200.0f, 300.0f, a, t);
        Vec2 mid = vec2_scale(vec2_add(t[1], t[2]), 0.5f);
        CHECK_NEAR(vec2_dist(t[0], mid), SD_SPIKE_LEN, 0.01);
        CHECK_NEAR(vec2_dist(t[1], t[2]), SD_SPIKE_W, 0.01);
        CHECK_NEAR(vec2_dist(vec2(200.0f, 300.0f), vec2_scale(vec2_add(t[0], mid), 0.5f)), 0.0,
                   0.01); /* centred */
    }
}

/* A stuck spike hanging exactly in the lane (the worst case) against a spike at different heights. */
static void test_hit_box_follows_the_drawn_triangle_vertically(void) {
    spike_dart_State *s = board();
    stick_at_angle(s, 0, ARC_PI / 2.0f); /* centre (240, 492): tip y 452, flat end y 532 */
    /* the flying spike's tip is (cur_y - 40); it touches once that is above the flat end (532) */
    CHECK(!spike_dart_would_touch(s, 532.0f + 40.0f + 12.0f)); /* tip 12 px short of the flat end */
    CHECK(!spike_dart_would_touch(s, 532.0f + 40.0f));         /* tip exactly at the flat end */
    CHECK(spike_dart_would_touch(s, 532.0f + 40.0f - 10.0f));  /* tip 10 px inside */
    CHECK(spike_dart_would_touch(s, 492.0f));                  /* on top of it: the landing pose */
    /* REGRESSION: the original rule already said "hit" when the tip was 12 px short of the flat end
     * (it started checking at 68 px below the target, the spike's flat end is at 72). */
    CHECK(spike_dart_lane_blocked(s));
}

static void test_hit_box_follows_the_drawn_triangle_sideways(void) {
    spike_dart_State *s = board();
    Vec2 t[3];
    /* two vertical spikes at the same height whose centres are dx apart overlap by about 8 - dx px */
    static const float dx[6] = {0.0f, 3.0f, 6.0f, 7.0f, 8.0f, 14.0f};
    static const bool touch[6] = {true, true, true, false, false, false};
    for (int i = 0; i < 6; i++) {
        s->stuck = 1;
        s->rot[0] = ARC_PI / 2.0f;
        s->px[0] = 240.0f + dx[i];
        s->py[0] = 492.0f;
        CHECK(spike_dart_would_touch(s, 492.0f) == touch[i]);
        spike_dart_shape(s->px[0], s->py[0], s->rot[0], t); /* (keeps the shape helper in the loop) */
    }
    /* mirrored: the other side of the lane behaves the same */
    s->px[0] = 240.0f - 7.0f;
    CHECK(!spike_dart_would_touch(s, 492.0f));
    s->px[0] = 240.0f - 6.0f;
    CHECK(spike_dart_would_touch(s, 492.0f));
}

/* REGRESSION for the reported bug. A stuck spike 3 degrees off the bottom has its centre 7.96 px from the
 * lane: the original test (|centre x - lane| < 8) called it a hit, but the drawn spikes are about 2 px
 * apart. It must now be a clean pass, while a spike only 1 degree off really does touch. */
static void test_a_near_miss_the_old_rule_punished_is_now_a_pass(void) {
    spike_dart_State *s = board();
    stick_at_angle(s, 0, ARC_PI / 2.0f + 3.0f * ARC_PI / 180.0f);
    CHECK(fabsf(s->px[0] - s->spike_x) < 8.0f); /* the old rule's condition holds ...                 */
    CHECK(s->py[0] > s->target_dn);             /* ... and so does its "below the target" condition   */
    CHECK(!spike_dart_would_touch(s, 492.0f));  /* but the drawn spikes do not touch                  */
    CHECK(!spike_dart_lane_blocked(s));
    stick_at_angle(s, 0, ARC_PI / 2.0f + 1.0f * ARC_PI / 180.0f);
    CHECK(spike_dart_would_touch(s, 492.0f)); /* a real overlap is still a hit */
    stick_at_angle(s, 0, ARC_PI / 2.0f - 1.0f * ARC_PI / 180.0f);
    CHECK(spike_dart_would_touch(s, 492.0f)); /* on either side */
}

static void test_spikes_far_from_the_lane_never_touch(void) {
    spike_dart_State *s = board();
    for (int deg = 10; deg <= 350; deg += 5) { /* everywhere else on the orbit: no contact at landing */
        stick_at_angle(s, 0, ARC_PI / 2.0f + (float)deg * ARC_PI / 180.0f);
        CHECK(!spike_dart_lane_blocked(s));
    }
    s->stuck = 0;
    CHECK(!spike_dart_lane_blocked(s)); /* nothing stuck: nothing to hit */
    CHECK(!spike_dart_would_touch(s, 300.0f));
}

/* Only the stuck spike that is in the way matters, wherever it sits in the array. */
static void test_any_stuck_spike_in_the_way_counts(void) {
    spike_dart_State *s = board();
    for (int i = 0; i < 5; i++)
        stick_at_angle(s, i, (float)(i + 1) * 1.0f); /* parked on the far side of the ring */
    CHECK(!spike_dart_lane_blocked(s));
    stick_at_angle(s, 4, ARC_PI / 2.0f); /* the last one swings into the lane */
    CHECK(spike_dart_lane_blocked(s));
    stick_at_angle(s, 4, 3.0f);
    stick_at_angle(s, 0, ARC_PI / 2.0f); /* or the first one */
    CHECK(spike_dart_lane_blocked(s));
}

/* A stuck spike keeps orbiting while the new spike flies (10 ticks), so aim its start angle so that it
 * is exactly in the lane when the new spike arrives. */
static void test_throw_into_a_passing_spike_ends_game(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.rot[0] = ARC_PI / 2.0f - 10.0f * (ARC_PI / 2.0f) * TICK;
    s.px[0] = 392.0f; /* parked out of the lane until the first orbit update */
    s.py[0] = 340.0f;
    throw_and_wait(&s, TICK);
    CHECK(s.over);
    CHECK(!s.capped);
    CHECK_EQ_INT(s.stuck, 1); /* the losing spike did not stick */
    idle(&s, 70, TICK);
    CHECK(spike_dart_is_over(&s));
}

/* The losing spike is drawn where it touched: on the landing spot if it arrived, never past the target. */
static void test_losing_spike_stops_where_it_touched(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.rot[0] = ARC_PI / 2.0f - 10.0f * (ARC_PI / 2.0f) * TICK;
    throw_and_wait(&s, TICK);
    CHECK(s.over);
    CHECK(s.cur_y >= s.target_y + SD_ORBIT_R - 0.01f); /* not past the target */
    CHECK(s.cur_y <= s.spike_y);
}

static void test_throw_with_clear_lane_survives(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = 1;
    s.rot[0] = ARC_PI / 2.0f + 1.0f; /* far from the lane when the new spike arrives */
    s.px[0] = 392.0f;
    s.py[0] = 340.0f;
    throw_and_wait(&s, TICK);
    CHECK(!s.over);
    CHECK_EQ_INT(s.stuck, 2);
}

/* No waiting at all: a spike eventually meets one passing the lane. The player ends the game by
 * playing badly, so no test relies on the monkey. */
static void test_spamming_throws_eventually_loses(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    for (int i = 0; i < 3000 && !spike_dart_is_over(&s); i++)
        spike_dart_step(&s, &in, ARC_DT);
    CHECK(spike_dart_is_over(&s));
    CHECK(spike_dart_score(&s) >= 1 && spike_dart_score(&s) < SD_MAX_SPIKES);
}

static void test_throw_ignored_while_flying(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    spike_dart_step(&s, &in, ARC_DT);
    spike_dart_step(&s, &in, ARC_DT);
    spike_dart_step(&s, &in, ARC_DT);
    CHECK_EQ_INT(s.throws, 1);
}

static void test_cap_of_64_ends_the_game(void) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    s.stuck = SD_MAX_SPIKES - 1;
    for (int i = 0; i < s.stuck; i++) { /* park the old spikes out of the lane */
        s.px[i] = 392.0f;
        s.py[i] = 340.0f;
        s.rot[i] = 0.0f;
    }
    throw_and_wait(&s, TICK);
    CHECK_EQ_INT(s.stuck, SD_MAX_SPIKES);
    CHECK(s.over && s.capped);
    idle(&s, 70, TICK);
    CHECK(spike_dart_is_over(&s));
    CHECK_EQ_INT(spike_dart_score(&s), SD_MAX_SPIKES);
}

static void test_big_step_cannot_tunnel(void) {
    spike_dart_State s;
    spike_dart_init(&s, 1);
    ArcadeInput in = throw_input();
    spike_dart_step(&s, &in, 0.2f); /* a huge dt: the spike must still stick, not fly past */
    CHECK(s.stuck == 1 || s.flying);
    for (int i = 0; i < 5 && s.stuck == 0; i++)
        idle(&s, 1, 0.2f);
    CHECK_EQ_INT(s.stuck, 1);
}

/* The spike snaps onto the orbit when it arrives, so the landing spot itself must be tested: at the edge of
 * the touching window a stuck spike can touch the landing pose without touching the pose 3 px before it. */
static void test_the_landing_spot_is_checked_before_sticking(void) {
    spike_dart_State *s = board();
    float land = s->target_y + SD_ORBIT_R;
    float angle = 0.0f;
    bool found = false;
    for (int i = 0; i <= 300 && !found; i++) { /* 0 to 3 degrees off the bottom, in 0.01 degree steps */
        float a = ARC_PI / 2.0f + (float)i * 0.01f * ARC_PI / 180.0f;
        stick_at_angle(s, 0, a);
        if (spike_dart_would_touch(s, land) && !spike_dart_would_touch(s, land + 3.0f)) {
            angle = a;
            found = true;
        }
    }
    CHECK(found); /* such an edge case exists */
    float dt =
        3.5f / (s->spike_spd * 50.0f); /* one tiny step: the flight ends 0.5 px past the landing spot */
    stick_at_angle(s, 0, angle - (ARC_PI / 2.0f) * dt); /* it orbits by (pi/2) * dt during that step */
    s->flying = true;
    s->cur_y = land + 3.0f;
    ArcadeInput none;
    arcade_input_clear(&none);
    spike_dart_step(s, &none, dt);
    CHECK(s->over && !s->capped); /* it touched where it landed: a hit, not a stuck spike */
    CHECK_EQ_INT(s->stuck, 1);
    CHECK_NEAR(s->cur_y, land, 0.001); /* and it is shown on the landing spot */
}

/* A throw is decided by the geometry, not by the frame rate: the same start angle gives the same
 * result at 30, 50, 60 and 120 Hz and in one single giant step (so nothing can be skipped over). */
static bool throw_loses(float rot0, float dt) {
    spike_dart_State s;
    spike_dart_init_sized(&s, 1, 480, 800);
    stick_at_angle(&s, 0, rot0);
    ArcadeInput fire = throw_input();
    spike_dart_step(&s, &fire, dt);
    ArcadeInput none;
    arcade_input_clear(&none);
    for (int i = 0; i < 2000 && s.flying && !s.over; i++)
        spike_dart_step(&s, &none, dt);
    return s.over;
}

static void test_outcome_does_not_depend_on_the_frame_rate(void) {
    static const float dts[5] = {0.02f, 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f, 0.2f};
    int losses = 0;
    for (int i = 0; i < 360; i++) {
        float rot0 = (float)i / 360.0f * ARC_TAU;
        bool reference = throw_loses(rot0, 1.0f / 240.0f);
        losses += reference ? 1 : 0;
        for (int d = 0; d < 5; d++)
            CHECK(throw_loses(rot0, dts[d]) == reference);
    }
    CHECK(losses >= 5 && losses <= 30); /* a narrow window of start angles loses: about 8 degrees of 360 */
}

/* ---- ground truth that does not share code with the game: brute-force point sampling ----------------- */

static bool in_triangle(const Vec2 t[3], float x, float y) {
    float d0 = (t[1].x - t[0].x) * (y - t[0].y) - (t[1].y - t[0].y) * (x - t[0].x);
    float d1 = (t[2].x - t[1].x) * (y - t[1].y) - (t[2].y - t[1].y) * (x - t[1].x);
    float d2 = (t[0].x - t[2].x) * (y - t[2].y) - (t[0].y - t[2].y) * (x - t[2].x);
    bool neg = d0 < 0.0f || d1 < 0.0f || d2 < 0.0f;
    bool pos = d0 > 0.0f || d1 > 0.0f || d2 > 0.0f;
    return !(neg && pos);
}

/* Area (px^2) of the part of triangle a that lies inside triangle b, counted on a 0.5 px grid. */
static float common_area(const Vec2 a[3], const Vec2 b[3]) {
    float x0 = fminf(a[0].x, fminf(a[1].x, a[2].x)), x1 = fmaxf(a[0].x, fmaxf(a[1].x, a[2].x));
    float y0 = fminf(a[0].y, fminf(a[1].y, a[2].y)), y1 = fmaxf(a[0].y, fmaxf(a[1].y, a[2].y));
    int count = 0;
    for (float y = y0; y <= y1; y += 0.5f)
        for (float x = x0; x <= x1; x += 0.5f)
            if (in_triangle(a, x, y) && in_triangle(b, x, y))
                count++;
    return (float)count * 0.25f;
}

/* Biggest overlap between the thrown spike (at height y) and any stuck spike. */
static float biggest_overlap(const spike_dart_State *s, float y) {
    Vec2 mine[3], other[3];
    float best = 0.0f;
    spike_dart_shape(s->spike_x, y, ARC_PI / 2.0f, mine);
    for (int i = 0; i < s->stuck; i++) {
        if (vec2_dist(vec2(s->px[i], s->py[i]), vec2(s->spike_x, y)) > SD_SPIKE_LEN)
            continue; /* two 80 px spikes whose centres are further apart than that cannot touch */
        spike_dart_shape(s->px[i], s->py[i], s->rot[i], other);
        best = fmaxf(best, common_area(mine, other));
    }
    return best;
}

/* The bug, as a property: over many random games, every "SPIKE HIT!" is a visible overlap, and no frame
 * where the drawn spikes clearly overlap goes by without ending the game. The original rule failed both:
 * about 2 of 3 of its losses showed a 2-5 px gap, and it let visible overlaps pass. */
static void test_every_loss_is_visible_and_every_visible_overlap_is_a_loss(void) {
    int losses = 0, flight_frames = 0;
    for (unsigned seed = 1; seed <= 120; seed++) {
        spike_dart_State s;
        spike_dart_init(&s, seed);
        Rng r;
        rng_seed(&r, seed * 7919u);
        float p = 0.03f + 0.25f * rng_float(&r); /* each game has its own impatience */
        for (int f = 0; f < 4000 && !s.over; f++) {
            ArcadeInput in;
            arcade_input_clear(&in);
            if (!s.flying && rng_chance(&r, p))
                in.fire_pressed = true;
            bool was_in_flight = s.flying || in.fire_pressed;
            int stuck_before = s.stuck;
            spike_dart_step(&s, &in, ARC_DT);
            if (s.stuck > stuck_before && stuck_before > 0) { /* a spike just stuck: not inside another one */
                Vec2 me[3], other[3];
                spike_dart_shape(s.px[s.stuck - 1], s.py[s.stuck - 1], s.rot[s.stuck - 1], me);
                for (int j = 0; j < s.stuck - 1; j++) {
                    spike_dart_shape(s.px[j], s.py[j], s.rot[j], other);
                    CHECK(common_area(me, other) < 6.0f);
                }
            }
            if (!was_in_flight || s.stuck == 0)
                continue;
            if (s.over && !s.capped) { /* a loss: the red spike really overlaps a stuck one */
                losses++;
                CHECK(biggest_overlap(&s, s.cur_y) > 0.0f);
            } else if (s.flying) { /* still flying: no clear overlap may be going unpunished */
                flight_frames++;
                CHECK(biggest_overlap(&s, s.cur_y) < 6.0f);
            }
        }
    }
    CHECK(losses >= 100 && flight_frames >= 1000); /* the check really ran */
}

static void test_same_seed_same_result(void) {
    spike_dart_State a, b;
    spike_dart_init(&a, 9);
    spike_dart_init(&b, 9);
    ArcadeMonkey ma, mb;
    arcade_monkey_init(&ma, 3);
    arcade_monkey_init(&mb, 3);
    for (int i = 0; i < 1500; i++) {
        ArcadeInput ia, ib;
        arcade_monkey_next(&ma, &ia);
        arcade_monkey_next(&mb, &ib);
        spike_dart_step(&a, &ia, ARC_DT);
        spike_dart_step(&b, &ib, ARC_DT);
    }
    CHECK_EQ_INT(a.stuck, b.stuck);
    CHECK_NEAR(a.cur_y, b.cur_y, 0.0001);
}

static void test_monkey_never_breaks_invariants(void) {
    for (unsigned seed = 1; seed <= 20; seed++) {
        spike_dart_State s;
        spike_dart_init(&s, seed);
        ArcadeMonkey m;
        arcade_monkey_init(&m, seed + 50);
        for (int i = 0; i < 5000; i++) {
            ArcadeInput in;
            arcade_monkey_next(&m, &in);
            spike_dart_step(&s, &in, ARC_DT);
            CHECK(s.stuck >= 0 && s.stuck <= SD_MAX_SPIKES);
            CHECK(isfinite(s.cur_y) && s.cur_y <= s.spike_y + 0.01f);
            for (int j = 0; j < s.stuck; j++)
                CHECK(isfinite(s.px[j]) && isfinite(s.py[j]));
            CHECK(spike_dart_score(&s) >= 0);
        }
    }
}

int main(void) {
    RUN(test_init_is_sane);
    RUN(test_spec_constants_at_480x800);
    RUN(test_first_spike_sticks_in_10_ticks_at_orbit_radius);
    RUN(test_orbit_after_one_second);
    RUN(test_orbit_period_at_60hz);
    RUN(test_shape_is_the_drawn_triangle);
    RUN(test_hit_box_follows_the_drawn_triangle_vertically);
    RUN(test_hit_box_follows_the_drawn_triangle_sideways);
    RUN(test_a_near_miss_the_old_rule_punished_is_now_a_pass);
    RUN(test_spikes_far_from_the_lane_never_touch);
    RUN(test_any_stuck_spike_in_the_way_counts);
    RUN(test_throw_into_a_passing_spike_ends_game);
    RUN(test_losing_spike_stops_where_it_touched);
    RUN(test_throw_with_clear_lane_survives);
    RUN(test_spamming_throws_eventually_loses);
    RUN(test_throw_ignored_while_flying);
    RUN(test_cap_of_64_ends_the_game);
    RUN(test_big_step_cannot_tunnel);
    RUN(test_the_landing_spot_is_checked_before_sticking);
    RUN(test_outcome_does_not_depend_on_the_frame_rate);
    RUN(test_every_loss_is_visible_and_every_visible_overlap_is_a_loss);
    RUN(test_same_seed_same_result);
    RUN(test_monkey_never_breaks_invariants);
    return TEST_SUMMARY();
}
