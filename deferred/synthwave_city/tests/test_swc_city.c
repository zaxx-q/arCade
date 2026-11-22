/* The city plan: the numbers marked "web version" were produced by the original TypeScript generator
 * (src/city/index.ts) for the same seed, so they pin the port to it. */
#include <string.h>

#include "arcade_test.h"
#include "swc_city.h"

static SwcCity city_a, city_b;

typedef struct Golden {
    uint32_t seed;
    int boxes, buildings, lanes, sites, landmarks;
    int districts[5];
    double h1, h2; /* height_at the probe points k = 1 and k = 2 (see probe_x / probe_z) */
} Golden;

static const Golden GOLDEN[] = {
    {1u, 16772, 11052, 520, 2239, 93, {1078, 254, 176, 80, 12}, 29.1889705657959, 35.40012741088867},
    {42u, 16572, 11419, 520, 1938, 92, {1187, 209, 113, 80, 11}, 40.95697021484375, 0.0},
};

static double probe_x(int k) {
    return fmod((double)k * 977.3 + 13.7, 6400.0);
}
static double probe_z(int k) {
    return fmod((double)k * 1511.9 + 71.1, 6400.0);
}

static void test_matches_the_web_version(void) {
    for (size_t g = 0; g < sizeof GOLDEN / sizeof GOLDEN[0]; g++) {
        const Golden *e = &GOLDEN[g];
        swc_city_build(&city_a, e->seed);
        CHECK_EQ_INT(city_a.box_count, e->boxes);
        CHECK_EQ_INT(city_a.building_count, e->buildings);
        CHECK_EQ_INT(city_a.lane_count, e->lanes);
        CHECK_EQ_INT(city_a.site_count, e->sites);
        CHECK_EQ_INT(city_a.landmark_count, e->landmarks);
        int counts[5] = {0, 0, 0, 0, 0};
        for (int i = 0; i < SWC_CELL_COUNT; i++)
            counts[city_a.cells[i].district]++;
        for (int d = 0; d < 5; d++)
            CHECK_EQ_INT(counts[d], e->districts[d]);
        CHECK_NEAR(swc_city_height_at(&city_a, probe_x(1), probe_z(1)), e->h1, 1e-4);
        CHECK_NEAR(swc_city_height_at(&city_a, probe_x(2), probe_z(2)), e->h2, 1e-4);
    }
}

static void test_deterministic_and_seed_dependent(void) {
    swc_city_build(&city_a, 7u);
    swc_city_build(&city_b, 7u);
    CHECK(city_a.box_count == city_b.box_count);
    CHECK(memcmp(city_a.boxes, city_b.boxes, (size_t)city_a.box_count * sizeof(SwcBox)) == 0);
    CHECK(memcmp(city_a.cells, city_b.cells, sizeof city_a.cells) == 0);
    swc_city_build(&city_b, 8u);
    CHECK(city_a.box_count != city_b.box_count ||
          memcmp(city_a.boxes, city_b.boxes, (size_t)city_a.box_count * sizeof(SwcBox)) != 0);
}

static void test_every_seed_fits_and_is_sane(void) {
    for (uint32_t seed = 0; seed < 12u; seed++) {
        swc_city_build(&city_a, seed * 977u + 3u);
        CHECK(city_a.box_count > 12000 && city_a.box_count < SWC_MAX_BOXES);
        CHECK(city_a.site_count > 1000 && city_a.site_count < SWC_MAX_SITES);
        CHECK(city_a.mega_count >= 6);
        for (int i = 0; i < city_a.box_count; i++) {
            const SwcBox *b = &city_a.boxes[i];
            CHECK(b->sx > 0.0f && b->sy > 0.0f && b->sz > 0.0f);
            CHECK(b->y0 >= 0.0f &&
                  b->y0 + b->sy <
                      1200.0f); /* tallest spires reach about 1060 m, under the 1500 m flight ceiling */
            CHECK(b->building >= 0 && b->building < city_a.building_count);
            CHECK(b->style <= SWC_STYLE_KIOSK);
        }
        for (int i = 0; i < city_a.site_count; i++) {
            const SwcSite *s = &city_a.sites[i];
            CHECK(isfinite(s->x) && isfinite(s->y) && isfinite(s->z) && isfinite(s->w));
            CHECK(s->kind < SWC_SITE_KIND_COUNT);
        }
        for (int i = 0; i < city_a.lane_count; i++)
            CHECK(city_a.lanes[i].fixed >= 0.0 && city_a.lanes[i].fixed < SWC_TILE);
    }
}

static void test_landmarks(void) {
    swc_city_build(&city_a, 1u);
    CHECK_EQ_INT(city_a.landmarks[0].kind, SWC_LM_MEGATOWER);
    CHECK_EQ_INT(city_a.landmarks[city_a.landmark_count - 1].kind, SWC_LM_DOWNTOWN);
    for (int i = 1; i < city_a.mega_count; i++) /* tallest first */
        CHECK(city_a.landmarks[i - 1].height >= city_a.landmarks[i].height);
    CHECK(city_a.landmarks[0].height > 150.0);
}

static void test_height_wraps_round_the_tile(void) {
    swc_city_build(&city_a, 1u);
    for (int k = 0; k < 40; k++) {
        double x = probe_x(k), z = probe_z(k);
        CHECK_NEAR(swc_city_height_at(&city_a, x, z), swc_city_height_at(&city_a, x + SWC_TILE, z - SWC_TILE),
                   1e-9);
    }
}

static void test_collide_pushes_out_of_a_box(void) {
    swc_city_build(&city_a, 1u);
    int tested = 0;
    for (int i = 0; i < city_a.box_count && tested < 200; i += 37) {
        const SwcBox *b = &city_a.boxes[i];
        if (b->flags & SWC_FLAG_ROUND || b->sx < 8.0f || b->sz < 8.0f || b->sy < 8.0f)
            continue;
        /* a point just inside the box, near its middle height */
        SwcVec3 p;
        p.x = b->cx + 0.3 * b->sx;
        p.y = b->y0 + 0.5 * b->sy;
        p.z = b->cz;
        const double r = 1.8;
        if (!swc_city_volume_blocked(&city_a, p.x, p.y, p.z, r, r, r))
            continue;
        CHECK(swc_city_collide(&city_a, &p, r));
        /* resolved: the sphere no longer overlaps ANY box (up to a small slack for the push-out iterations)
         */
        CHECK(!swc_city_volume_blocked(&city_a, p.x, p.y, p.z, r - 0.1, r - 0.1, r - 0.1));
        tested++;
    }
    CHECK(tested > 50);
}

static void test_collide_leaves_open_air_alone(void) {
    swc_city_build(&city_a, 1u);
    SwcVec3 p = {0.0, 40.0, 0.0}; /* the crossing of two avenues, 40 m up */
    CHECK(!swc_city_collide(&city_a, &p, 1.8));
    CHECK_NEAR(p.x, 0.0, 1e-12);
    CHECK_NEAR(p.y, 40.0, 1e-12);
    SwcVec3 low = {0.0, 0.2, 0.0}; /* below the lowest eye height: lifted onto it */
    CHECK(swc_city_collide(&city_a, &low, 1.8));
    CHECK_NEAR(low.y, 1.8, 1e-9);
}

static void test_lane_coordinates(void) {
    for (int line = 0; line < SWC_CELLS; line += 7) {
        double c = swc_lane_coordinate(0, line, 1, 0.0);
        CHECK(c >= 0.0 && c < SWC_TILE);
    }
}

int main(void) {
    RUN(test_matches_the_web_version);
    RUN(test_deterministic_and_seed_dependent);
    RUN(test_every_seed_fits_and_is_sane);
    RUN(test_landmarks);
    RUN(test_height_wraps_round_the_tile);
    RUN(test_collide_pushes_out_of_a_box);
    RUN(test_collide_leaves_open_air_alone);
    RUN(test_lane_coordinates);
    return TEST_SUMMARY();
}
