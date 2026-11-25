/* Synthwave City - free flight (see swc_flight.h). PURE C99, no raylib. Ported from src/camera/flight.ts. */
#include "swc_flight.h"

#include <math.h>

#define MAX_PHYSICS_STEP (1.0 / 120.0) /* a longer frame is split, so 30 Hz and 60 Hz fly the same path */
#define SUB_STEP 1.5                   /* longest move between collision tests: thinner than any wall */
#define MAX_SUB_STEPS 96
#define RESPONSE 3.0 /* how fast velocity approaches what the controls ask for, per second */

static double hyp3(double a, double b, double c) {
    return sqrt(a * a + b * b + c * c);
}

double swc_top_speed(double y, double speed_mul) {
    return SWC_BASE_SPEED * (1.0 + swc_clamp(y, 0.0, SWC_MAX_ALTITUDE) / SWC_ALTITUDE_SPEED) * speed_mul;
}

/* Moves in steps short enough not to skip a wall. Where the sphere is pushed out of a box, the rest of the
 * move and the velocity lose their component into that wall, so the camera slides along it. */
bool swc_flight_move(const SwcCity *city, SwcFlight *f, double dx, double dy, double dz) {
    double dist = hyp3(dx, dy, dz);
    int steps = (int)ceil(dist / SUB_STEP);
    steps = steps < 1 ? 1 : (steps > MAX_SUB_STEPS ? MAX_SUB_STEPS : steps);
    double sx = dx / steps, sy = dy / steps, sz = dz / steps;
    bool touched = false;
    for (int i = 0; i < steps; i++) {
        SwcVec3 probe;
        probe.x = f->x + sx;
        probe.y = f->y + sy;
        probe.z = f->z + sz;
        double bx = probe.x, by = probe.y, bz = probe.z;
        if (swc_city_collide(city, &probe, SWC_CAMERA_RADIUS)) {
            double nx = probe.x - bx, ny = probe.y - by, nz = probe.z - bz;
            double n = hyp3(nx, ny, nz);
            if (n > 1e-9) {
                double ux = nx / n, uy = ny / n, uz = nz / n;
                double into = f->vx * ux + f->vy * uy + f->vz * uz;
                if (into < 0.0) {
                    f->vx -= into * ux;
                    f->vy -= into * uy;
                    f->vz -= into * uz;
                }
                double step_into = sx * ux + sy * uy + sz * uz;
                if (step_into < 0.0) {
                    sx -= step_into * ux;
                    sy -= step_into * uy;
                    sz -= step_into * uz;
                }
            }
            touched = true;
        }
        f->x = probe.x;
        f->y = probe.y;
        f->z = probe.z;
    }
    if (f->y > SWC_MAX_ALTITUDE) {
        f->y = SWC_MAX_ALTITUDE;
        if (f->vy > 0.0)
            f->vy = 0.0;
    }
    f->x = swc_wrap_tile(f->x);
    f->z = swc_wrap_tile(f->z);
    return touched;
}

/* One step of flight. forward is along the view direction (so it climbs when looking up), right along the
 * horizon, up along world y. Velocity approaches the target exponentially and the step is integrated exactly,
 * so the path does not depend on the frame rate. */
bool swc_flight_step(const SwcCity *city, SwcFlight *f, double forward, double right, double up,
                     double speed_mul, double dt) {
    if (!(dt > 0.0))
        return false;
    double cp = cos(f->pitch), sp = sin(f->pitch);
    double sy = sin(f->yaw), cy = cos(f->yaw);
    double ax = -sy * cp * forward + cy * right;
    double ay = sp * forward + up * 0.7;
    double az = -cy * cp * forward - sy * right;
    double mag = hyp3(ax, ay, az);
    if (mag > 1.0) {
        ax /= mag;
        ay /= mag;
        az /= mag;
    }
    int steps = (int)ceil(dt / MAX_PHYSICS_STEP - 1e-9);
    steps = steps < 1 ? 1 : steps;
    double h = dt / steps;
    double e = exp(-RESPONSE * h);
    double k = (1.0 - e) / RESPONSE;
    bool touched = false;
    for (int i = 0; i < steps; i++) {
        /* Top speed depends on altitude; take it at the middle of the step. */
        double top = swc_top_speed(f->y + f->vy * h * 0.5, speed_mul);
        double tx = ax * top, ty = ay * top, tz = az * top;
        double ox = f->vx - tx, oy = f->vy - ty, oz = f->vz - tz;
        f->vx = tx + ox * e;
        f->vy = ty + oy * e;
        f->vz = tz + oz * e;
        if (swc_flight_move(city, f, tx * h + ox * k, ty * h + oy * k, tz * h + oz * k))
            touched = true;
    }
    return touched;
}

bool swc_flight_step_towards(const SwcCity *city, SwcFlight *f, double tx, double ty, double tz, double omega,
                             double reach, double dt) {
    if (!(dt > 0.0))
        return false;
    double ax = f->x - tx, ay = f->y - ty, az = f->z - tz;
    /* The shortest way round the wrapped tile. */
    ax -= SWC_TILE * floor(ax / SWC_TILE + 0.5);
    az -= SWC_TILE * floor(az / SWC_TILE + 0.5);
    double far_away = hyp3(ax, ay, az);
    if (far_away > reach) {
        double k = reach / far_away;
        ax *= k;
        ay *= k;
        az *= k;
    }
    /* x(t) = target + (a + b t) e^(-omega t), with b = v0 + omega a. */
    double e = exp(-omega * dt);
    double bx = f->vx + omega * ax, by = f->vy + omega * ay, bz = f->vz + omega * az;
    double dx = (ax + bx * dt) * e - ax, dy = (ay + by * dt) * e - ay, dz = (az + bz * dt) * e - az;
    f->vx = (bx - omega * (ax + bx * dt)) * e;
    f->vy = (by - omega * (ay + by * dt)) * e;
    f->vz = (bz - omega * (az + bz * dt)) * e;
    return swc_flight_move(city, f, dx, dy, dz);
}

double swc_spring_value(double value, double *rate, double offset, double omega, double dt) {
    double e = exp(-omega * dt);
    double b = *rate + omega * offset;
    double next = (offset + b * dt) * e;
    *rate = (b - omega * (offset + b * dt)) * e;
    return value + (next - offset);
}
