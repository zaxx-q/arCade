/* Traffic is a pure function of time: these tests check the properties the renderer relies on. */
#include "arcade_test.h"
#include "swc_traffic.h"

static SwcCity city;
static SwcTraffic traffic;

static void setup(uint32_t seed) {
    swc_city_build(&city, seed);
    swc_traffic_init(&traffic, &city, 0.77);
}

static double wrap_diff(double a, double b) {
    return swc_wrap_delta(a, b, SWC_TILE);
}

static void test_one_loop_returns_every_vehicle_to_its_start(void) {
    setup(1u);
    for (int lane = 0; lane < city.lane_count; lane += 3) {
        int count = traffic.lane[lane].count;
        for (int i = 0; i < count; i += 5)
            CHECK_NEAR(wrap_diff(swc_vehicle_coord(&traffic, &city, lane, i, 0.0),
                                 swc_vehicle_coord(&traffic, &city, lane, i, 1.0)),
                       0.0, 1e-6);
    }
}

static void test_vehicles_move_at_the_lane_speed_in_the_lane_direction(void) {
    setup(1u);
    for (int lane = 0; lane < city.lane_count; lane += 11) {
        const SwcLaneTraffic *lt = &traffic.lane[lane];
        double u = 0.25, du = 0.001;
        double a = swc_vehicle_coord(&traffic, &city, lane, 0, u);
        double b = swc_vehicle_coord(&traffic, &city, lane, 0, u + du);
        double speed = wrap_diff(b, a) / (du * SWC_LOOP_SECONDS);
        CHECK_NEAR(speed, (double)city.lanes[lane].dir * lt->laps * SWC_LAP_SPEED, 1e-6);
    }
}

static void test_speeds_are_plausible(void) {
    setup(1u);
    for (int lane = 0; lane < city.lane_count; lane++) {
        const SwcLaneTraffic *lt = &traffic.lane[lane];
        double speed = lt->laps * SWC_LAP_SPEED;
        if (city.lanes[lane].kind == SWC_LANE_SKY)
            CHECK(speed >= 25.0 && speed <= 70.0);
        else
            CHECK(lt->laps >= 6 &&
                  lt->laps <= 21); /* 9 to 33 m/s: slow side streets, fast inner avenue lanes */
        CHECK(lt->count >= 1);
        CHECK_NEAR(lt->spacing * lt->count, SWC_TILE, 1e-9);
    }
}

static void test_neighbours_never_overlap_and_never_catch_up(void) {
    setup(42u);
    for (int lane = 0; lane < city.lane_count; lane++) {
        const SwcLaneTraffic *lt = &traffic.lane[lane];
        if (lt->count < 2)
            continue;
        for (int step = 0; step < 4; step++) {
            double u = step * 0.2371;
            for (int i = 0; i < lt->count; i++) {
                double a = swc_vehicle_coord(&traffic, &city, lane, i, u);
                double b = swc_vehicle_coord(&traffic, &city, lane, (i + 1) % lt->count, u);
                double gap = wrap_diff(b, a) * (double)city.lanes[lane].dir;
                /* jitter moves each slot by at most 0.2 of a spacing: neighbours stay >= 0.6 spacing apart */
                CHECK(fabs(gap) >= 0.6 * lt->spacing - 1e-6);
            }
        }
    }
}

static void test_slot_lookup_finds_the_vehicle_at_a_position(void) {
    setup(1u);
    for (int lane = 0; lane < city.lane_count; lane += 7) {
        const SwcLaneTraffic *lt = &traffic.lane[lane];
        for (int i = 0; i < lt->count; i += 3) {
            double u = 0.6180339;
            double at = swc_vehicle_coord(&traffic, &city, lane, i, u);
            CHECK_EQ_INT(swc_vehicle_slot_near(&traffic, &city, lane, u, at), i);
        }
    }
}

static void test_mix_of_vehicles(void) {
    setup(1u);
    int present = 0, total = 0, kinds[SWC_VEHICLE_KIND_COUNT] = {0};
    for (int lane = 0; lane < city.lane_count; lane++) {
        if (city.lanes[lane].kind == SWC_LANE_SKY)
            continue;
        for (int i = 0; i < traffic.lane[lane].count; i++) {
            total++;
            if (!swc_vehicle_present(&traffic, lane, i))
                continue;
            present++;
            int k = swc_vehicle_kind(&traffic, lane, i);
            CHECK(k >= 0 && k < SWC_VEHICLE_KIND_COUNT);
            kinds[k]++;
            double s = swc_vehicle_seed(&traffic, lane, i);
            CHECK(s >= 0.0 && s < 1.0);
        }
    }
    CHECK(total > 4000);
    CHECK(present > total * 80 / 100 && present < total * 95 / 100); /* about 12% of the slots are empty */
    CHECK(kinds[SWC_VEHICLE_CAR] > present * 60 / 100);
    CHECK(kinds[SWC_VEHICLE_VAN] > 0 && kinds[SWC_VEHICLE_BUS] > 0 && kinds[SWC_VEHICLE_TAXI] > 0);
    CHECK(kinds[SWC_VEHICLE_EMERGENCY] > 0 && kinds[SWC_VEHICLE_EMERGENCY] < present / 50);
}

static void test_deterministic(void) {
    setup(5u);
    static SwcTraffic other;
    swc_traffic_init(&other, &city, 0.77);
    for (int lane = 0; lane < city.lane_count; lane++) {
        CHECK_EQ_INT(traffic.lane[lane].count, other.lane[lane].count);
        CHECK_EQ_INT(traffic.lane[lane].laps, other.lane[lane].laps);
        CHECK(traffic.lane[lane].phase == other.lane[lane].phase);
    }
}

static void test_density_changes_the_count(void) {
    setup(1u);
    static SwcTraffic sparse, dense;
    swc_traffic_init(&sparse, &city, 0.4);
    swc_traffic_init(&dense, &city, 1.6);
    long a = 0, b = 0;
    for (int lane = 0; lane < city.lane_count; lane++) {
        a += sparse.lane[lane].count;
        b += dense.lane[lane].count;
    }
    CHECK(b > a);
}

int main(void) {
    RUN(test_one_loop_returns_every_vehicle_to_its_start);
    RUN(test_vehicles_move_at_the_lane_speed_in_the_lane_direction);
    RUN(test_speeds_are_plausible);
    RUN(test_neighbours_never_overlap_and_never_catch_up);
    RUN(test_slot_lookup_finds_the_vehicle_at_a_position);
    RUN(test_mix_of_vehicles);
    RUN(test_deterministic);
    RUN(test_density_changes_the_count);
    return TEST_SUMMARY();
}
