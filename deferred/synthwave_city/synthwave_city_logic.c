/* Synthwave City - game logic (see synthwave_city_logic.h). PURE C99, no raylib.
 * The flight model is ported from src/camera/flight.ts and the viewpoints from src/camera/viewpoints.ts of
 * the web version; the drone tour is a simpler design (see tour_step) that keeps the same guarantees: it only
 * flies above road centre lines, and the camera can never enter a building. */
#include "synthwave_city_logic.h"

#include <string.h>

#define MAX_PHYSICS_STEP (1.0 / 120.0) /* a longer frame is split, so 30 Hz and 60 Hz fly the same path */
#define SUB_STEP 1.5                   /* longest move between collision tests: thinner than any wall */
#define MAX_SUB_STEPS 96
#define RESPONSE 3.0 /* how fast velocity approaches what the controls ask for, per second */

#define LOOK_SENSITIVITY 0.0032 /* radians per pixel of mouse drag */
#define MAX_DRAG_PIXELS 300.0   /* a bigger jump of the pointer in one frame is ignored */

static double hyp3(double a, double b, double c) {
    return sqrt(a * a + b * b + c * c);
}

/* ================================================================================================== */
/* Flight                                                                                             */
/* ================================================================================================== */

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

/* A critically damped spring pulling the sphere towards a point, with collision. `reach` caps how far away
 * the target is taken to be, which caps the speed at about reach * omega / 2. */
static bool flight_step_towards(const SwcCity *city, SwcFlight *f, double tx, double ty, double tz,
                                double omega, double reach, double dt) {
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

/* ================================================================================================== */
/* Viewpoints (the six named views of the web version, derived from the landmarks)                    */
/* ================================================================================================== */

static double yaw_towards(double x, double z, double tx, double tz) {
    return atan2(-swc_wrap_delta(tx, x, SWC_TILE), -swc_wrap_delta(tz, z, SWC_TILE));
}

/* A place to stand on the tallest flat roof of a cell, three metres back from its edge on the sun's side.
 * Returns false if the cell has no roof wide enough. */
static bool rooftop(const SwcCity *city, int cell_x, int cell_z, double *ox, double *oy, double *oz) {
    double x0 = cell_x * SWC_CELL, z0 = cell_z * SWC_CELL;
    double best = 0.0, bx = 0.0, bz = 0.0;
    for (int gz = 6; gz < (int)SWC_CELL; gz += 6) {
        for (int gx = 6; gx < (int)SWC_CELL; gx += 6) {
            double x = x0 + gx, z = z0 + gz;
            double h = swc_city_height_at(city, x, z);
            if (h <= best)
                continue;
            /* Flat for five metres each way: a roof, not a spire or a ledge. */
            if (swc_city_height_at(city, x - 5.0, z) != h || swc_city_height_at(city, x + 5.0, z) != h)
                continue;
            if (swc_city_height_at(city, x, z - 5.0) != h || swc_city_height_at(city, x, z + 5.0) != h)
                continue;
            best = h;
            bx = x;
            bz = z;
        }
    }
    if (best <= 0.0)
        return false;
    /* Walk towards the sun (-z) to the edge of that roof. */
    double edge = bz;
    while (edge > z0 && swc_city_height_at(city, bx, edge - 1.0) == best)
        edge -= 1.0;
    *ox = bx;
    *oy = best + 2.4;
    *oz = fmin(bz, edge + 3.0);
    return true;
}

static void set_view(SwcViewpoint *v, const char *name, double x, double y, double z, double yaw,
                     double pitch) {
    v->name = name;
    v->x = swc_wrap_tile(x);
    v->y = y;
    v->z = swc_wrap_tile(z);
    v->yaw = yaw;
    v->pitch = pitch;
}

static void compute_viewpoints(synthwave_city_State *s) {
    const SwcCity *city = &s->city;
    /* The landmark list always ends with the downtown entry. */
    const SwcLandmark *downtown = &city->landmarks[city->landmark_count - 1];
    const SwcLandmark *mega = downtown;
    for (int i = 0; i < city->landmark_count; i++)
        if (city->landmarks[i].kind == SWC_LM_MEGATOWER) {
            mega = &city->landmarks[i]; /* the tallest: they are sorted */
            break;
        }
    /* The plaza nearest downtown, so the view has towers behind it. */
    const SwcLandmark *plaza = downtown;
    double nearest = 1e30;
    for (int i = 0; i < city->landmark_count; i++) {
        const SwcLandmark *l = &city->landmarks[i];
        if (l->kind != SWC_LM_PLAZA)
            continue;
        double d =
            hypot(swc_wrap_delta(l->x, downtown->x, SWC_TILE), swc_wrap_delta(l->z, downtown->z, SWC_TILE));
        if (d < nearest) {
            nearest = d;
            plaza = l;
        }
    }

    /* The avenue that runs towards the sun beside the downtown cell. */
    double avenue_x = floor((double)downtown->cell_x / SWC_AVENUE_EVERY + 0.5) * SWC_AVENUE_EVERY * SWC_CELL;
    double roof_x, roof_y, roof_z;
    bool has_roof = rooftop(city, downtown->cell_x, downtown->cell_z, &roof_x, &roof_y, &roof_z);
    double mega_road_x = (mega->cell_x + 1) * SWC_CELL;
    double mega_z = mega->z + 1.5 * SWC_CELL;
    double plaza_x = (plaza->cell_x + 1) * SWC_CELL, plaza_z = (plaza->cell_z + 1) * SWC_CELL;

    SwcViewpoint *v = s->views;
    set_view(&v[0], "Sunset Avenue", avenue_x, 2.4, (downtown->cell_z + 1.5) * SWC_CELL, 0.0, 0.07);
    set_view(&v[1], "Downtown Canyon", avenue_x, 120.0, (downtown->cell_z + 3.5) * SWC_CELL, 0.0, 0.02);
    if (has_roof)
        set_view(&v[2], "Rooftop", roof_x, roof_y, roof_z, 0.25, -0.1);
    else
        set_view(&v[2], "Rooftop", avenue_x, fmax(60.0, downtown->height + 4.0),
                 (downtown->cell_z + 0.5) * SWC_CELL, 0.5, -0.14);
    set_view(&v[3], "Panorama", avenue_x, 900.0, downtown->z + 1500.0, 0.0, -0.3);
    set_view(&v[4], "Megatower", mega_road_x, fmax(120.0, mega->height * 0.5), mega_z,
             0.35 * yaw_towards(mega_road_x, mega_z, mega->x, mega->z), 0.12);
    set_view(&v[5], "Plaza", plaza_x, 70.0, plaza_z, yaw_towards(plaza_x, plaza_z, plaza->x, plaza->z), -0.5);

    /* Belt and braces: if a plan ever puts something there, stand just outside it. */
    for (int i = 0; i < SWC_VIEW_COUNT; i++) {
        SwcVec3 p;
        p.x = v[i].x;
        p.y = v[i].y;
        p.z = v[i].z;
        for (int k = 0; k < 4 && swc_city_collide(city, &p, SWC_CAMERA_RADIUS + 0.2); k++) {
        }
        v[i].x = swc_wrap_tile(p.x);
        v[i].y = p.y;
        v[i].z = swc_wrap_tile(p.z);
    }
}

/* ================================================================================================== */
/* The drone tour                                                                                     */
/* ================================================================================================== */

static double angle_delta(double a, double b) {
    return swc_wrap_delta(a, b, SWC_TAU);
}

/* Low over the street, mid-height between the towers, or high above the skyline. */
static double pick_altitude(SwcRng *rng) {
    double u = swc_rng_next(rng);
    if (u < 0.40)
        return swc_rng_range(rng, 8.0, 26.0);
    if (u < 0.75)
        return swc_rng_range(rng, 50.0, 140.0);
    return swc_rng_range(rng, 180.0, 420.0);
}

/* Turns the carrot by a quarter turn: +1 and -1 are the two directions. */
static void tour_turn(SwcTour *t, int turn) {
    int axis = t->axis, dir = t->dir;
    if (turn > 0) {
        t->axis = 1 - axis;
        t->dir = axis == 0 ? dir : -dir;
    } else if (turn < 0) {
        t->axis = 1 - axis;
        t->dir = axis == 0 ? -dir : dir;
    }
}

/* Advances the carrot by dt seconds. It runs along road centre lines (air above a road is free at every
 * height, so nothing is ever in its way), slows down before a planned turn, and at each intersection picks
 * the next turn and sometimes a new altitude. */
static void tour_step(SwcTour *t, double dt) {
    double along = t->axis == 0 ? t->px : t->pz;
    double next = t->dir > 0 ? (floor(along / SWC_CELL + 1e-9) + 1.0) * SWC_CELL
                             : (ceil(along / SWC_CELL - 1e-9) - 1.0) * SWC_CELL;
    double to_corner = fabs(next - along);
    double speed = 36.0 + 0.1 * t->alt_target;
    if (t->turn_next != 0)
        speed *= 0.45 + 0.55 * swc_smoothstep(10.0, 70.0, to_corner);
    double remaining = speed * dt;
    for (int guard = 0; remaining > 0.0 && guard < 4; guard++) {
        along = t->axis == 0 ? t->px : t->pz;
        next = t->dir > 0 ? (floor(along / SWC_CELL + 1e-9) + 1.0) * SWC_CELL
                          : (ceil(along / SWC_CELL - 1e-9) - 1.0) * SWC_CELL;
        to_corner = fabs(next - along);
        bool at_corner = remaining >= to_corner;
        double moved = at_corner ? to_corner : remaining;
        along = at_corner ? next : along + (double)t->dir * moved;
        remaining -= moved;
        if (t->axis == 0)
            t->px = swc_wrap_tile(along);
        else
            t->pz = swc_wrap_tile(along);
        if (at_corner) {
            tour_turn(t, t->turn_next);
            double u = swc_rng_next(&t->rng);
            t->turn_next = u < 0.55 ? 0 : (u < 0.775 ? 1 : -1);
            if (swc_rng_chance(&t->rng, 0.45))
                t->alt_target = pick_altitude(&t->rng);
        }
    }
    /* Climb or dive towards the target altitude, at most 16 m/s. */
    double dy = swc_clamp(t->alt_target - t->py, -16.0 * dt, 16.0 * dt);
    t->py += dy;
}

/* Starts the tour from where the camera is: the carrot goes onto the nearest road ahead. */
static void tour_attach(synthwave_city_State *s) {
    SwcTour *t = &s->tour;
    const SwcFlight *f = &s->cam;
    double fx = -sin(f->yaw), fz = -cos(f->yaw);
    if (fabs(fx) > fabs(fz)) {
        t->axis = 0;
        t->dir = fx > 0.0 ? 1 : -1;
        t->pz = swc_wrap_tile(floor(f->z / SWC_CELL + 0.5) * SWC_CELL);
        t->px = swc_wrap_tile(floor(f->x / SWC_CELL + 0.5) * SWC_CELL + (double)t->dir * SWC_CELL);
    } else {
        t->axis = 1;
        t->dir = fz > 0.0 ? 1 : -1;
        t->px = swc_wrap_tile(floor(f->x / SWC_CELL + 0.5) * SWC_CELL);
        t->pz = swc_wrap_tile(floor(f->z / SWC_CELL + 0.5) * SWC_CELL + (double)t->dir * SWC_CELL);
    }
    t->alt_target = fmax(14.0, f->y);
    t->py = t->alt_target;
    t->turn_next = 0;
}

/* The camera follows the carrot and looks where it is going. */
static bool tour_fly(synthwave_city_State *s, double dt) {
    SwcTour *t = &s->tour;
    SwcFlight *f = &s->cam;
    tour_step(t, dt);
    bool touched = flight_step_towards(&s->city, f, t->px, t->py, t->pz, 2.4, 80.0, dt);
    double hs = hypot(f->vx, f->vz);
    double k = 1.0 - exp(-dt * 2.2);
    if (hs > 4.0)
        f->yaw += angle_delta(atan2(-f->vx, -f->vz), f->yaw) * k;
    double want = swc_clamp(atan2(f->vy, hs > 1.0 ? hs : 1.0) * 0.7, -0.3, 0.3) -
                  0.14 * swc_smoothstep(60.0, 350.0, f->y);
    f->pitch += (want - f->pitch) * (1.0 - exp(-dt * 2.0));
    return touched;
}

/* ================================================================================================== */
/* Public functions                                                                                   */
/* ================================================================================================== */

double swc_music_seconds(void) {
    return (double)(SWC_MUSIC_BARS * SWC_BEATS_PER_BAR) * 60.0 / SWC_BPM;
}

void swc_sync_beat(synthwave_city_State *s, double music_seconds) {
    s->beat_time = swc_wrap(music_seconds, swc_music_seconds());
}

double swc_beat_phase(const synthwave_city_State *s) {
    double beats = s->beat_time * SWC_BPM / 60.0;
    return beats - floor(beats);
}

double swc_beat_pulse(const synthwave_city_State *s) {
    return exp(-swc_beat_phase(s) * 5.0);
}

double swc_loop_fraction(const synthwave_city_State *s) {
    return s->clock / SWC_LOOP_SECONDS;
}

void swc_goto_view(synthwave_city_State *s, int n) {
    if (n < 0 || n >= SWC_VIEW_COUNT)
        return;
    const SwcViewpoint *v = &s->views[n];
    s->cam.x = v->x;
    s->cam.y = v->y;
    s->cam.z = v->z;
    s->cam.vx = s->cam.vy = s->cam.vz = 0.0;
    s->cam.yaw = v->yaw;
    s->cam.pitch = v->pitch;
    s->touring = false;
    s->view = n;
}

void synthwave_city_init(synthwave_city_State *s, unsigned seed) {
    memset(s, 0, sizeof(*s));
    swc_city_build(&s->city, (uint32_t)seed);
    swc_traffic_init(&s->traffic, &s->city, 0.77); /* the web version's "medium" quality traffic */
    compute_viewpoints(s);
    s->mood = 0;
    s->view = -1;
    s->speed_scale = 1.0;

    /* Start low on Sunset Avenue, flying towards the sun and the downtown skyline. */
    s->cam.x = s->views[0].x;
    s->cam.y = 18.0;
    s->cam.z = s->views[0].z;
    s->cam.yaw = 0.0;
    s->cam.pitch = 0.05;
    SwcRng base;
    swc_rng_seed(&base, (uint32_t)seed);
    s->tour.rng = swc_rng_fork(&base, 4u);
    s->tour.axis = 1;
    s->tour.dir = -1;
    s->tour.px = s->cam.x;
    s->tour.pz = swc_wrap_tile(s->cam.z - 60.0);
    s->tour.py = 18.0;
    s->tour.alt_target = 18.0;
    s->tour.turn_next = 0;
    s->touring = true;
}

void synthwave_city_step(synthwave_city_State *s, const ArcadeInput *in, float dt_f) {
    double dt = (double)dt_f;
    if (!(dt > 0.0))
        return;
    if (dt > 0.25)
        dt = 0.25;
    s->touched = false;
    s->clock = swc_wrap(s->clock + dt, SWC_LOOP_SECONDS);
    s->beat_time = swc_wrap(s->beat_time + dt, swc_music_seconds());

    /* keys: T cycles the mood, 1..6 jump to a named viewpoint */
    int key = in->typed;
    if (key == 't' || key == 'T')
        s->mood = (s->mood + 1) % SWC_MOOD_COUNT;
    if (key >= '1' && key < '1' + SWC_VIEW_COUNT)
        swc_goto_view(s, key - '1');

    /* the wheel scales the cruise speed by 1.25 per step */
    if (in->wheel != 0)
        s->speed_scale = swc_clamp(s->speed_scale * (in->wheel > 0 ? 1.25 : 0.8), 0.25, 4.0);

    /* Space toggles the tour. Any flight control takes the camera back. */
    bool dragging = in->fire && s->prev_look;
    bool stick = fabs((double)in->move_x) > 0.01 || fabs((double)in->move_y) > 0.01;
    if (in->action_pressed) {
        s->touring = !s->touring;
        if (s->touring)
            tour_attach(s);
    } else if (s->touring && (stick || dragging || in->alt || in->wheel != 0)) {
        s->touring = false;
    }

    /* look: drag with the left mouse button */
    if (dragging && !s->touring) {
        double dx = swc_clamp((double)(in->aim_x - s->prev_aim_x), -MAX_DRAG_PIXELS, MAX_DRAG_PIXELS);
        double dy = swc_clamp((double)(in->aim_y - s->prev_aim_y), -MAX_DRAG_PIXELS, MAX_DRAG_PIXELS);
        s->cam.yaw = swc_wrap(s->cam.yaw - dx * LOOK_SENSITIVITY + SWC_PI, SWC_TAU) - SWC_PI;
        s->cam.pitch = swc_clamp(s->cam.pitch - dy * LOOK_SENSITIVITY, -SWC_MAX_PITCH, SWC_MAX_PITCH);
    }
    s->prev_look = in->fire;
    s->prev_aim_x = in->aim_x;
    s->prev_aim_y = in->aim_y;

    double px = s->cam.x, py = s->cam.y, pz = s->cam.z;
    if (s->touring) {
        s->touched = tour_fly(s, dt);
    } else {
        double mul = (in->alt ? SWC_BOOST : 1.0) * s->speed_scale;
        s->touched =
            swc_flight_step(&s->city, &s->cam, -(double)in->move_y, (double)in->move_x, 0.0, mul, dt);
    }
    double mx = swc_wrap_delta(s->cam.x, px, SWC_TILE), mz = swc_wrap_delta(s->cam.z, pz, SWC_TILE);
    s->distance += hyp3(mx, s->cam.y - py, mz);
}

bool synthwave_city_is_over(const synthwave_city_State *s) {
    (void)s;
    return false; /* an endless flight: the player leaves with ESC */
}

int synthwave_city_score(const synthwave_city_State *s) {
    return (int)(s->distance / 1000.0);
}
