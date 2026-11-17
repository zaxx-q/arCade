/* Synthwave City - traffic as a pure function of time. PURE C99, no raylib.
 *
 * Ported from the web version's street-life and air-traffic packers. Nothing about a vehicle is stored:
 * every lane holds `count` slots at even spacing, and where slot i is at loop fraction u is
 *
 *     coordinate(i) = fract(dir * laps * u + phase + (i + jitter(i)) / count) * SWC_TILE
 *
 * `laps` is a whole number of laps of the tile per loop, so the motion is exactly periodic (the loop has no
 * seam) and the vehicles of one lane share a speed, so they never catch each other. The renderer asks only
 * for the few slots near the camera. */
#ifndef SWC_TRAFFIC_H
#define SWC_TRAFFIC_H

#include "swc_city.h"

#define SWC_LAP_SPEED (SWC_TILE / SWC_LOOP_SECONDS) /* metres per second of one lap per loop = 1.5625 */

typedef enum SwcVehicleKind {
    SWC_VEHICLE_CAR = 0,
    SWC_VEHICLE_VAN,
    SWC_VEHICLE_BUS,
    SWC_VEHICLE_TAXI,
    SWC_VEHICLE_EMERGENCY,
    SWC_VEHICLE_KIND_COUNT
} SwcVehicleKind;

typedef struct SwcLaneTraffic {
    int count;      /* slots in this lane (some are left empty) */
    int laps;       /* whole laps of the tile per loop: the speed, in steps of SWC_LAP_SPEED */
    double phase;   /* phase of slot 0 at loop 0, 0..1 */
    double spacing; /* SWC_TILE / count, metres */
} SwcLaneTraffic;

typedef struct SwcTraffic {
    uint32_t seed;
    SwcLaneTraffic lane[SWC_MAX_LANES];
    int lane_count;
} SwcTraffic;

/* Fills one entry per lane of the city. `density` 1 packs ground lanes to 30-46 m per car; smaller values
 * spread them out in proportion. */
void swc_traffic_init(SwcTraffic *t, const SwcCity *city, double density);

/* Where slot i of lane `lane` is, as a tile coordinate in [0, SWC_TILE), at loop fraction u in [0, 1).
 * The vehicle sits at that coordinate along the lane's axis and at lane.fixed across it. */
double swc_vehicle_coord(const SwcTraffic *t, const SwcCity *city, int lane, int i, double u);

/* The slot of the lane whose even position is nearest to coordinate `at` at loop fraction u. */
int swc_vehicle_slot_near(const SwcTraffic *t, const SwcCity *city, int lane, double u, double at);

/* A slot may be empty, so streams have gaps and clumps. */
bool swc_vehicle_present(const SwcTraffic *t, int lane, int i);

/* What a vehicle is (cars, vans, buses, taxis, emergency cars on the ground lanes; ignored in the sky). */
int swc_vehicle_kind(const SwcTraffic *t, int lane, int i);

/* A random number 0..1 fixed per vehicle (weave, bob, colour). */
double swc_vehicle_seed(const SwcTraffic *t, int lane, int i);

#endif
