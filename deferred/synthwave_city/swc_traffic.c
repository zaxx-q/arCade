/* Synthwave City - traffic (see swc_traffic.h). PURE C99, no raylib. */
#include "swc_traffic.h"

#define MIN_SPACING 28.0 /* smallest even spacing a ground lane may be packed to, metres */
#define JITTER 0.4 /* total jitter range as a share of the spacing: neighbours stay >= 0.6 spacing apart */
#define SKIP_CHANCE 0.12 /* share of slots left empty */
#define BUS_CHANCE 0.06
#define VAN_CHANCE 0.15
#define EMERGENCY_CHANCE 0.006
#define TAXI_CHANCE 0.06

/* Speed range per lane slot, in laps (1 lap = 1.5625 m/s): the inner lane is the fast one. */
static const int AVENUE_LAPS[3][2] = {{18, 21}, {14, 17}, {10, 13}};
static const int STREET_LAPS[2][2] = {{9, 11}, {6, 8}};

static int round_to_int(double v) {
    return (int)floor(v + 0.5);
}

static void init_ground_lane(SwcLaneTraffic *out, const SwcLane *lane, uint32_t seed, double density) {
    double offset = swc_wrap_delta(lane->fixed, lane->line * SWC_CELL, SWC_TILE);
    int side = offset > 0.0 ? 1 : 0;
    int row = lane->axis * 6 + side * 3 + lane->slot; /* a stable id for "this lane of this road" */
    bool avenue = lane->kind == SWC_LANE_AVENUE;
    double hu = swc_hash_unit4(seed, (uint32_t)lane->line, (uint32_t)row, 1u);
    double target = (avenue ? 30.0 + 10.0 * hu : 33.0 + 13.0 * hu) / density;
    int count = round_to_int(SWC_TILE / target);
    int max_count = (int)floor(SWC_TILE / MIN_SPACING);
    count = count < 1 ? 1 : (count > max_count ? max_count : count);
    int lo, hi;
    if (avenue) {
        lo = AVENUE_LAPS[lane->slot < 3 ? lane->slot : 2][0];
        hi = AVENUE_LAPS[lane->slot < 3 ? lane->slot : 2][1];
    } else {
        lo = STREET_LAPS[lane->slot < 2 ? lane->slot : 1][0];
        hi = STREET_LAPS[lane->slot < 2 ? lane->slot : 1][1];
    }
    uint32_t h[4];
    h[0] = seed;
    h[1] = (uint32_t)lane->line;
    h[2] = (uint32_t)row;
    h[3] = 2u;
    out->laps = lo + (int)(swc_hash_ints(h, 4) % (uint32_t)(hi - lo + 1));
    out->phase = swc_hash_unit4(seed, (uint32_t)lane->line, (uint32_t)row, 3u);
    out->count = count;
    out->spacing = SWC_TILE / (double)count;
}

/* Craft in a sky lane: higher lanes are sparser and faster. */
static void init_sky_lane(SwcLaneTraffic *out, const SwcLane *lane, int index, uint32_t seed,
                          double density) {
    double h = swc_hash_unit3(seed, (uint32_t)index, 5u);
    double spacing = 86.0 + 14.0 * lane->slot + 30.0 * h;
    int count = round_to_int((SWC_TILE / spacing) * density);
    out->count = count < 3 ? 3 : count;
    double speed = 27.0 + 13.0 * lane->slot + 5.0 * h;
    int laps = round_to_int(speed / SWC_LAP_SPEED);
    int lo = (int)ceil(25.0 / SWC_LAP_SPEED), hi = (int)floor(70.0 / SWC_LAP_SPEED);
    out->laps = laps < lo ? lo : (laps > hi ? hi : laps);
    out->phase = swc_hash_unit3(seed, (uint32_t)index, 6u);
    out->spacing = SWC_TILE / (double)out->count;
}

void swc_traffic_init(SwcTraffic *t, const SwcCity *city, double density) {
    double d = swc_clamp(density, 0.05, 4.0);
    t->seed = city->seed;
    t->lane_count = city->lane_count;
    for (int i = 0; i < city->lane_count; i++) {
        const SwcLane *lane = &city->lanes[i];
        if (lane->kind == SWC_LANE_SKY)
            init_sky_lane(&t->lane[i], lane, i, city->seed, d);
        else
            init_ground_lane(&t->lane[i], lane, city->seed, d);
    }
}

static double fract(double v) {
    return v - floor(v);
}

/* Offset of slot i from its even position, in slots: (-JITTER/2 .. +JITTER/2). */
static double jitter(const SwcTraffic *t, int lane, int i) {
    return (swc_hash_unit4(t->seed, (uint32_t)lane, (uint32_t)i, 4u) - 0.5) * JITTER;
}

double swc_vehicle_coord(const SwcTraffic *t, const SwcCity *city, int lane, int i, double u) {
    const SwcLaneTraffic *lt = &t->lane[lane];
    double dir = (double)city->lanes[lane].dir;
    double slot = ((double)i + jitter(t, lane, i)) / (double)lt->count;
    return fract(dir * (double)lt->laps * u + lt->phase + slot) * SWC_TILE;
}

int swc_vehicle_slot_near(const SwcTraffic *t, const SwcCity *city, int lane, double u, double at) {
    const SwcLaneTraffic *lt = &t->lane[lane];
    double dir = (double)city->lanes[lane].dir;
    double f = fract(at / SWC_TILE - lt->phase - dir * (double)lt->laps * u);
    int i = round_to_int(f * (double)lt->count);
    return i % lt->count;
}

bool swc_vehicle_present(const SwcTraffic *t, int lane, int i) {
    return swc_hash_unit4(t->seed, (uint32_t)lane, (uint32_t)i, 7u) >= SKIP_CHANCE;
}

int swc_vehicle_kind(const SwcTraffic *t, int lane, int i) {
    double u = swc_hash_unit4(t->seed, (uint32_t)lane, (uint32_t)i, 8u);
    if (u < EMERGENCY_CHANCE)
        return SWC_VEHICLE_EMERGENCY;
    u -= EMERGENCY_CHANCE;
    if (u < BUS_CHANCE)
        return SWC_VEHICLE_BUS;
    u -= BUS_CHANCE;
    if (u < VAN_CHANCE)
        return SWC_VEHICLE_VAN;
    u -= VAN_CHANCE;
    if (u < TAXI_CHANCE)
        return SWC_VEHICLE_TAXI;
    return SWC_VEHICLE_CAR;
}

double swc_vehicle_seed(const SwcTraffic *t, int lane, int i) {
    return swc_hash_unit4(t->seed, (uint32_t)lane, (uint32_t)i, 9u);
}
