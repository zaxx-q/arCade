/* Quality tiers and the adaptive render scale: the numbers of the web version's quality.ts, the pure helpers
 * the renderer and the HUD share, the adaptive scale's behaviour, and the packs at the densities the tiers
 * use. */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_air.h"
#include "swc_beams.h"
#include "swc_city.h"
#include "swc_ground.h"
#include "swc_quality.h"
#include "swc_street.h"

static SwcCity city;
static SwcPackedBeams beams;
static SwcCraftPack craft;
static bool lit[SWC_MAX_SITES];

static void test_the_tiers_hold_the_web_versions_numbers(void) {
    const SwcQuality *lo = swc_quality(SWC_TIER_LOW);
    const SwcQuality *me = swc_quality(SWC_TIER_MEDIUM);
    const SwcQuality *hi = swc_quality(SWC_TIER_HIGH);
    CHECK(strcmp(lo->name, "low") == 0 && strcmp(me->name, "medium") == 0 && strcmp(hi->name, "high") == 0);
    CHECK_NEAR(lo->max_pixel_ratio, 1.0, 0.0);
    CHECK_NEAR(me->max_pixel_ratio, 1.5, 0.0);
    CHECK_NEAR(hi->max_pixel_ratio, 2.0, 0.0);
    CHECK_EQ_INT(lo->msaa, 0);
    CHECK_EQ_INT(me->msaa, 0);
    CHECK_EQ_INT(hi->msaa, 4);
    CHECK_NEAR(lo->reflection_scale, 0.0, 0.0);
    CHECK_NEAR(me->reflection_scale, 0.5, 0.0);
    CHECK_NEAR(hi->reflection_scale, 0.5, 0.0);
    CHECK_NEAR(lo->density, 0.35, 0.0);
    CHECK_NEAR(me->density, 0.6, 0.0);
    CHECK_NEAR(hi->density, 1.0, 0.0);
    CHECK_NEAR(lo->fade_start, 1300.0, 0.0);
    CHECK_NEAR(lo->fade_end, 1800.0, 0.0);
    CHECK_NEAR(me->fade_start, 1900.0, 0.0);
    CHECK_NEAR(me->fade_end, 2400.0, 0.0);
    CHECK_NEAR(hi->fade_start, 2500.0, 0.0);
    CHECK_NEAR(hi->fade_end, 3100.0, 0.0);
    CHECK(!lo->crowds && me->crowds && hi->crowds);
    CHECK(!lo->sun_shafts && !me->sun_shafts && hi->sun_shafts);
    CHECK_NEAR(lo->min_render_scale, 0.5, 0.0);
    CHECK_NEAR(lo->max_render_scale, 0.85, 0.0);
    CHECK_NEAR(me->min_render_scale, 0.6, 0.0);
    CHECK_NEAR(me->max_render_scale, 1.0, 0.0);
    CHECK_NEAR(hi->min_render_scale, 0.7, 0.0);
    CHECK_NEAR(hi->max_render_scale, 1.0, 0.0);
    for (int t = 0; t < SWC_TIER_COUNT; t++)
        CHECK_EQ_INT((int)swc_quality((SwcTier)t)->tier, t);
    CHECK_EQ_INT((int)swc_quality((SwcTier)99)->tier, (int)SWC_TIER_HIGH); /* nonsense is "high" */
    CHECK_EQ_INT((int)swc_quality((SwcTier)-1)->tier, (int)SWC_TIER_HIGH);
}

static void test_the_q_key_cycles_down_and_wraps(void) {
    CHECK_EQ_INT((int)swc_tier_next(SWC_TIER_HIGH), (int)SWC_TIER_MEDIUM);
    CHECK_EQ_INT((int)swc_tier_next(SWC_TIER_MEDIUM), (int)SWC_TIER_LOW);
    CHECK_EQ_INT((int)swc_tier_next(SWC_TIER_LOW), (int)SWC_TIER_HIGH);
    SwcTier t = SWC_TIER_HIGH;
    for (int i = 0; i < 3; i++)
        t = swc_tier_next(t);
    CHECK_EQ_INT((int)t, (int)SWC_TIER_HIGH);
}

static void test_names_and_labels(void) {
    SwcTier t = SWC_TIER_HIGH;
    CHECK(swc_tier_from_name("low", &t) && t == SWC_TIER_LOW);
    CHECK(swc_tier_from_name("Medium", &t) && t == SWC_TIER_MEDIUM);
    CHECK(swc_tier_from_name("HIGH", &t) && t == SWC_TIER_HIGH);
    t = SWC_TIER_MEDIUM;
    CHECK(!swc_tier_from_name("ultra", &t) && t == SWC_TIER_MEDIUM);
    CHECK(!swc_tier_from_name("hig", &t));
    CHECK(!swc_tier_from_name("highest", &t));
    CHECK(!swc_tier_from_name("", &t));
    CHECK(!swc_tier_from_name(NULL, &t));
    CHECK(strcmp(swc_tier_label(SWC_TIER_LOW), "LOW") == 0);
    CHECK(strcmp(swc_tier_label(SWC_TIER_MEDIUM), "MEDIUM") == 0);
    CHECK(strcmp(swc_tier_label(SWC_TIER_HIGH), "HIGH") == 0);
}

static void test_crowds_and_the_street_handover_follow_the_tier(void) {
    CHECK_EQ_INT(swc_quality_crowd_per_face(swc_quality(SWC_TIER_HIGH), SWC_CROWD_PER_FACE), 64);
    CHECK_EQ_INT(swc_quality_crowd_per_face(swc_quality(SWC_TIER_MEDIUM), SWC_CROWD_PER_FACE), 38); /* 38.4 */
    CHECK_EQ_INT(swc_quality_crowd_per_face(swc_quality(SWC_TIER_LOW), SWC_CROWD_PER_FACE), 0);
    /* Math.round: halves go up */
    SwcQuality q = *swc_quality(SWC_TIER_MEDIUM);
    q.density = 0.5;
    CHECK_EQ_INT(swc_quality_crowd_per_face(&q, 5), 3);
    /* real cars never reach the far fade: min(880, fadeStart * 0.8) */
    for (int t = 0; t < SWC_TIER_COUNT; t++)
        CHECK_NEAR(swc_quality_handover_end(swc_quality((SwcTier)t), SWC_STREET_HANDOVER_END),
                   SWC_STREET_HANDOVER_END, 0.0);
    q = *swc_quality(SWC_TIER_LOW);
    q.fade_start = 800.0;
    CHECK_NEAR(swc_quality_handover_end(&q, SWC_STREET_HANDOVER_END), 640.0, 1e-9);
}

static void test_the_scene_size_follows_pixel_ratio_scale_and_tier(void) {
    int w, h;
    const SwcQuality *hi = swc_quality(SWC_TIER_HIGH);
    swc_quality_render_size(hi, 1280, 720, 1280, 720, 1.0, &w, &h);
    CHECK_EQ_INT(w, 1280);
    CHECK_EQ_INT(h, 720);
    swc_quality_render_size(hi, 1920, 1080, 1920, 1080, 1.0, &w, &h);
    CHECK_EQ_INT(w, 1920);
    CHECK_EQ_INT(h, 1080);
    swc_quality_render_size(hi, 1280, 720, 1280, 720, 0.5, &w, &h);
    CHECK_EQ_INT(w, 640);
    CHECK_EQ_INT(h, 360);
    /* a retina screen (2 pixels per point): "high" keeps 2, "medium" caps at 1.5, "low" at 1 */
    swc_quality_render_size(hi, 1280, 720, 2560, 1440, 1.0, &w, &h);
    CHECK_EQ_INT(w, 2560);
    swc_quality_render_size(swc_quality(SWC_TIER_MEDIUM), 1280, 720, 2560, 1440, 1.0, &w, &h);
    CHECK_EQ_INT(w, 1920);
    CHECK_EQ_INT(h, 1080);
    swc_quality_render_size(swc_quality(SWC_TIER_LOW), 1280, 720, 2560, 1440, 1.0, &w, &h);
    CHECK_EQ_INT(w, 1280);
    CHECK_EQ_INT(h, 720);
    swc_quality_render_size(swc_quality(SWC_TIER_LOW), 1280, 720, 2560, 1440, 0.85, &w, &h);
    CHECK_EQ_INT(w, 1088);
    CHECK_EQ_INT(h, 612);
    /* never below one pixel, never a ratio under 1 from a window that reports fewer pixels than points */
    swc_quality_render_size(hi, 1, 1, 1, 1, 0.1, &w, &h);
    CHECK_EQ_INT(w, 1);
    CHECK_EQ_INT(h, 1);
    swc_quality_render_size(hi, 1280, 720, 640, 360, 1.0, &w, &h);
    CHECK_EQ_INT(w, 1280);
}

static void test_the_mirror_is_a_share_of_the_frame_and_low_has_none(void) {
    int mw, mh;
    swc_quality_mirror_size(swc_quality(SWC_TIER_HIGH), 1280, 720, &mw, &mh);
    CHECK_EQ_INT(mw, 640);
    CHECK_EQ_INT(mh, 360);
    swc_quality_mirror_size(swc_quality(SWC_TIER_MEDIUM), 1087, 611, &mw, &mh);
    CHECK_EQ_INT(mw, 543);
    CHECK_EQ_INT(mh, 305);
    swc_quality_mirror_size(swc_quality(SWC_TIER_LOW), 1280, 720, &mw, &mh);
    CHECK_EQ_INT(mw, 0);
    CHECK_EQ_INT(mh, 0);
}

/* ---- the adaptive scale ---- */

static void run(SwcAdaptiveScale *a, double seconds, double frame_ms, int *changes) {
    double dt = 1.0 / 60.0;
    for (double t = 0.0; t < seconds; t += dt)
        if (swc_adaptive_update(a, frame_ms, dt) && changes != NULL)
            (*changes)++;
}

static void test_the_scale_starts_at_the_top_and_ignores_the_first_seconds(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.7, 1.0);
    CHECK_NEAR(a.scale, 1.0, 0.0);
    int changes = 0;
    run(&a, 2.9, 80.0, &changes); /* slow frames during the grace: ignored */
    CHECK_EQ_INT(changes, 0);
    CHECK_NEAR(a.scale, 1.0, 0.0);
    CHECK(!swc_adaptive_update(&a, 0.0, 0.016)); /* nonsense input changes nothing */
    CHECK(!swc_adaptive_update(&a, 16.0, 0.0));
    CHECK(!swc_adaptive_update(&a, -3.0, 0.016));
}

static void test_slow_frames_step_the_scale_down_to_the_floor(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.7, 1.0);
    run(&a, 3.0, 40.0, NULL); /* the grace */
    int changes = 0;
    double last = a.scale;
    for (int i = 0; i < 40; i++) {
        run(&a, 1.0, 40.0, &changes);
        CHECK(a.scale <= last + 1e-12);
        CHECK(a.scale >= 0.7 - 1e-12);
        last = a.scale;
    }
    CHECK_NEAR(a.scale, 0.7, 1e-9);
    CHECK(changes >= 3); /* 1.0 -> 0.9 -> 0.8 -> 0.7 */
    CHECK(changes <= 4);
}

static void test_the_first_step_down_comes_after_a_second_of_slow_frames(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.5, 1.0);
    run(&a, 3.0, 40.0, NULL);
    int changes = 0;
    run(&a, 0.8, 40.0, &changes);
    CHECK_EQ_INT(changes, 0);
    run(&a, 0.5, 40.0, &changes);
    CHECK_EQ_INT(changes, 1);
    CHECK_NEAR(a.scale, 0.9, 1e-12);
}

static void test_calm_frames_never_change_a_scale_that_is_at_the_top(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.7, 1.0);
    int changes = 0;
    run(&a, 120.0, 16.7, &changes);
    CHECK_EQ_INT(changes, 0);
    CHECK_NEAR(a.scale, 1.0, 0.0);
}

/* Slow frames until the scale steps down once; returns the seconds that took. */
static double until_step_down(SwcAdaptiveScale *a, double frame_ms) {
    double dt = 1.0 / 60.0, t = 0.0;
    while (t < 30.0) {
        t += dt;
        if (swc_adaptive_update(a, frame_ms, dt))
            break;
    }
    return t;
}

static void test_a_calm_spell_steps_up_by_half_a_step(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.7, 1.0);
    run(&a, 3.0, 16.0, NULL); /* the grace */
    until_step_down(&a, 40.0);
    CHECK_NEAR(a.scale, 0.9, 1e-12);
    run(&a, 1.5, 16.0, NULL); /* the cooldown, and the frame time settling: nothing happens */
    CHECK_NEAR(a.scale, 0.9, 1e-12);
    double dt = 1.0 / 60.0, calm_s = 0.0;
    bool stepped = false;
    while (calm_s < 20.0 && !stepped) {
        calm_s += dt;
        stepped = swc_adaptive_update(&a, 16.0, dt);
    }
    CHECK(stepped);
    CHECK(calm_s >= 4.0); /* the 6 s wait counts from the end of the cooldown, which ended already */
    CHECK(calm_s <= 6.5);
    CHECK_NEAR(a.scale, 0.95, 1e-9);
}

static void test_a_failed_step_up_makes_the_next_attempt_wait_longer(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.5, 1.0);
    run(&a, 3.0, 40.0, NULL);
    run(&a, 6.0, 40.0, NULL); /* well down */
    CHECK_NEAR(a.retry_after, 6.0, 0.0);
    /* calm until it steps up, then slow again within five seconds: that was a mistake */
    for (int i = 0; i < 4000 && a.since_step_up > 1e20; i++)
        run(&a, 0.1, 16.0, NULL);
    CHECK(a.since_step_up < 1.0);
    run(&a, 3.0, 40.0, NULL);
    CHECK_NEAR(a.retry_after, 12.0, 0.0);
}

static void test_the_wait_between_attempts_is_capped(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.5, 1.0);
    a.age = 10.0; /* past the grace */
    a.scale = 0.9;
    a.over = 0.99;         /* one more slow frame is a second of them */
    a.since_step_up = 1.0; /* it stepped up a second ago: that was a mistake */
    a.retry_after = 200.0;
    CHECK(swc_adaptive_update(&a, 40.0, 0.016));
    CHECK_NEAR(a.retry_after, 240.0, 0.0); /* 400 capped */
    CHECK_NEAR(a.scale, 0.8, 1e-12);
    /* a step down long after the last step up leaves the wait alone */
    a.cooldown = 0.0;
    a.over = 0.99;
    a.since_step_up = 100.0;
    a.retry_after = 12.0;
    CHECK(swc_adaptive_update(&a, 40.0, 0.016));
    CHECK_NEAR(a.retry_after, 12.0, 0.0);
}

static void test_a_new_tier_starts_at_its_best_scale(void) {
    SwcAdaptiveScale a;
    swc_adaptive_init(&a, 0.7, 1.0);
    swc_adaptive_set_range(&a, 0.5, 0.85); /* "low" */
    CHECK_NEAR(a.scale, 0.85, 0.0);
    CHECK_NEAR(a.min, 0.5, 0.0);
    CHECK_NEAR(a.max, 0.85, 0.0);
    a.scale = 0.5;                        /* the loop had stepped all the way down ... */
    swc_adaptive_set_range(&a, 0.7, 1.0); /* ... and the player goes back to "high" */
    CHECK_NEAR(a.scale, 1.0, 0.0);
    CHECK_NEAR(a.cooldown, 0.0, 0.0);
    CHECK_NEAR(a.retry_after, 6.0, 0.0);
}

/* ---- the packs at the densities the tiers use (reference numbers dumped from the web version) ---- */

typedef struct TierGolden {
    uint32_t seed;
    double density;
    int lit_count;
    double lit_sum;
    int used[3];
    int sky_count, pillar_count;
    double sky_sum, pillar_sum; /* sum of the origin floats */
    int craft;
    int shape_start[4];
    double craft_sum0;
    double lane_sum;
    double spacing;
} TierGolden;

static const TierGolden GOLDEN[] = {
    {1984u,
     0.35,
     101,
     173780.0,
     {50, 48, 3},
     69,
     48,
     472195.7925567627,
     333938.68829345703,
     3015,
     {0, 1974, 2405, 3015},
     9699924.46988535,
     239357.0,
     80.0},
    {1984u,
     0.6,
     123,
     207023.0,
     {62, 56, 5},
     99,
     56,
     666654.3564758301,
     385606.68829345703,
     5172,
     {0, 3413, 4145, 5172},
     16631764.338114738,
     410088.0,
     80.0},
    {7u,
     0.35,
     96,
     140771.0,
     {46, 47, 3},
     69,
     47,
     500595.40518188477,
     299371.05615234375,
     3008,
     {0, 2003, 2415, 3008},
     9671191.989299297,
     239580.0,
     80.0},
    {7u,
     0.6,
     125,
     184951.0,
     {69, 53, 3},
     96,
     53,
     716938.1006851196,
     335367.05615234375,
     5172,
     {0, 3416, 4146, 5172},
     16611481.499453545,
     411685.0,
     80.0},
};

static double sum_floats(const float *p, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++)
        s += p[i];
    return s;
}

static void test_the_packs_match_the_web_version_at_the_tier_densities(void) {
    for (size_t c = 0; c < sizeof(GOLDEN) / sizeof(GOLDEN[0]); c++) {
        const TierGolden *g = &GOLDEN[c];
        swc_city_build(&city, g->seed);
        uint32_t bs = swc_beam_layer_seed(g->seed);
        swc_choose_emitters(&city, g->density, bs, lit);
        int count = 0;
        double sum = 0.0;
        for (int i = 0; i < city.site_count; i++)
            if (lit[i]) {
                count++;
                sum += (double)(i + 1);
            }
        CHECK_EQ_INT(count, g->lit_count);
        CHECK_NEAR(sum, g->lit_sum, 0.5);
        swc_pack_beams(&city, g->density, bs, &beams);
        for (int k = 0; k < 3; k++)
            CHECK_EQ_INT(beams.sites_used[k], g->used[k]);
        CHECK_EQ_INT(beams.sky.count, g->sky_count);
        CHECK_EQ_INT(beams.pillars.count, g->pillar_count);
        CHECK_NEAR(sum_floats(beams.sky.origin, beams.sky.count * 3), g->sky_sum, fabs(g->sky_sum) * 4e-7);
        CHECK_NEAR(sum_floats(beams.pillars.origin, beams.pillars.count * 3), g->pillar_sum,
                   fabs(g->pillar_sum) * 4e-7);

        swc_air_pack_craft(&city, g->density, swc_air_layer_seed(g->seed), &craft);
        CHECK_EQ_INT(craft.count, g->craft);
        for (int s = 0; s < 4; s++)
            CHECK_EQ_INT(craft.shape_start[s], g->shape_start[s]);
        double s0 = 0.0, lane = 0.0;
        for (int i = 0; i < craft.count; i++) {
            s0 += craft.data[i * SWC_CRAFT_STRIDE];
            lane += craft.lane_of[i];
        }
        CHECK_NEAR(s0, g->craft_sum0, fabs(g->craft_sum0) * 8e-7);
        CHECK_NEAR(lane, g->lane_sum, 0.5);
        CHECK_NEAR(swc_lamp_spacing(g->density), g->spacing, 0.0);
    }
}

static void test_a_lower_tier_never_draws_more_than_a_higher_one(void) {
    swc_city_build(&city, 1984u);
    int craft_at[SWC_TIER_COUNT], lit_at[SWC_TIER_COUNT];
    for (int t = 0; t < SWC_TIER_COUNT; t++) {
        const SwcQuality *q = swc_quality((SwcTier)t);
        swc_air_pack_craft(&city, q->density, swc_air_layer_seed(1984u), &craft);
        craft_at[t] = craft.count;
        swc_choose_emitters(&city, q->density, swc_beam_layer_seed(1984u), lit);
        lit_at[t] = 0;
        for (int i = 0; i < city.site_count; i++)
            lit_at[t] += lit[i] ? 1 : 0;
    }
    CHECK(craft_at[SWC_TIER_LOW] < craft_at[SWC_TIER_MEDIUM] &&
          craft_at[SWC_TIER_MEDIUM] < craft_at[SWC_TIER_HIGH]);
    CHECK(lit_at[SWC_TIER_LOW] < lit_at[SWC_TIER_MEDIUM] && lit_at[SWC_TIER_MEDIUM] < lit_at[SWC_TIER_HIGH]);
}

int main(void) {
    RUN(test_the_tiers_hold_the_web_versions_numbers);
    RUN(test_the_q_key_cycles_down_and_wraps);
    RUN(test_names_and_labels);
    RUN(test_crowds_and_the_street_handover_follow_the_tier);
    RUN(test_the_scene_size_follows_pixel_ratio_scale_and_tier);
    RUN(test_the_mirror_is_a_share_of_the_frame_and_low_has_none);
    RUN(test_the_scale_starts_at_the_top_and_ignores_the_first_seconds);
    RUN(test_slow_frames_step_the_scale_down_to_the_floor);
    RUN(test_the_first_step_down_comes_after_a_second_of_slow_frames);
    RUN(test_calm_frames_never_change_a_scale_that_is_at_the_top);
    RUN(test_a_calm_spell_steps_up_by_half_a_step);
    RUN(test_a_failed_step_up_makes_the_next_attempt_wait_longer);
    RUN(test_the_wait_between_attempts_is_capped);
    RUN(test_a_new_tier_starts_at_its_best_scale);
    RUN(test_the_packs_match_the_web_version_at_the_tier_densities);
    RUN(test_a_lower_tier_never_draws_more_than_a_higher_one);
    return TEST_SUMMARY();
}
