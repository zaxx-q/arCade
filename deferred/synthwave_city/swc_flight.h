/* Synthwave City - free flight: a sphere with velocity that eases towards what the controls ask for and
 * slides along building boxes. PURE C99, no raylib. Ported from src/camera/flight.ts of the web version. */
#ifndef SWC_FLIGHT_H
#define SWC_FLIGHT_H

#include <stdbool.h>

#include "swc_city.h"
#include "swc_core.h"

#define SWC_CAMERA_RADIUS 1.8 /* collision sphere, metres; also the lowest eye height */
#define SWC_MAX_ALTITUDE 1500.0
#define SWC_BASE_SPEED 45.0      /* cruise speed at street level, m/s */
#define SWC_ALTITUDE_SPEED 300.0 /* speed grows by 1x for every this many metres of altitude */
#define SWC_BOOST 3.5
#define SWC_MAX_PITCH 1.5

/* Free flight: a sphere with velocity. Position x and z are kept wrapped into 0..SWC_TILE. */
typedef struct SwcFlight {
    double x, y, z;
    double vx, vy, vz;
    double yaw, pitch; /* yaw 0 faces -z; positive turns towards -x. Pitch up is positive. */
} SwcFlight;

/* Top speed at an altitude, m/s. `speed_mul` is 1 for cruising, SWC_BOOST when boosting. */
double swc_top_speed(double y, double speed_mul);

/* One step of flight (axes -1..1 in the camera's frame). Returns true if it touched a building. */
bool swc_flight_step(const SwcCity *city, SwcFlight *f, double forward, double right, double up,
                     double speed_mul, double dt);
/* Slides by (dx, dy, dz), never entering a building. Returns true if it touched one. */
bool swc_flight_move(const SwcCity *city, SwcFlight *f, double dx, double dy, double dz);
/* One step of a critically damped spring pulling the sphere towards a point, with collision. `reach` caps
 * how far away the target is taken to be, which caps the speed at about reach * omega / 2. */
bool swc_flight_step_towards(const SwcCity *city, SwcFlight *f, double tx, double ty, double tz, double omega,
                             double reach, double dt);

/* One step of a critically damped spring on a single value: returns the new value and writes the new rate.
 * `offset` is value - target; the caller passes it so angles can take the short way round. */
double swc_spring_value(double value, double *rate, double offset, double omega, double dt);

#endif
