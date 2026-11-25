/* Synthwave City - the camera rig: see swc_rig.h. A line-by-line port of src/camera/rig.ts (without the
 * DOM, the reduced-motion mode and the scrub). */
#include "swc_rig.h"

#include <math.h>
#include <string.h>

#define LOOK_RETURN_OMEGA 1.6 /* stiffness of the spring that returns a free look to the tour's own view */
#define LENS_FOLLOW 14.0      /* how fast the lens follows a zoom, and how fast it returns off the tour */
#define LENS_RETURN 3.0
#define DEG (SWC_PI / 180.0)

#define JOIN_OMEGA 1.6
#define JOIN_REACH 120.0
#define JOIN_TURN_OMEGA 1.1
#define JOIN_MIN_TIME 0.8
#define JOIN_TIMEOUT 9.0
#define FREE_ROLL 0.07

static double smoother(double u) {
    double t = swc_clamp(u, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

/* Frame-rate independent smoothing factor: how far to move towards a target in dt seconds. */
static double damp(double rate, double dt) {
    return 1.0 - exp(-rate * dt);
}

static double angle_delta(double a, double b) {
    return swc_wrap_delta(b, a, SWC_TAU);
}

static double hyp3(double a, double b, double c) {
    return sqrt(a * a + b * b + c * c);
}

void swc_rig_input_clear(SwcRigInput *in) {
    in->forward = in->right = in->up = 0.0;
    in->boost = false;
    in->look_yaw = in->look_pitch = 0.0;
    in->turn = in->tilt = 0.0;
    in->pan_yaw = in->pan_pitch = 0.0;
    in->zoom = 1.0;
    in->speed_scale = 1.0;
    in->active = false;
}

/* ---- the pose last written ---- */

static void write_tour(SwcRig *r) {
    SwcTourSample *s = &r->sample;
    swc_tour_sample(&r->tour, r->tour_t, s);
    r->x = s->x;
    r->y = s->y;
    r->z = s->z;
    r->yaw = s->yaw;
    r->pitch = s->pitch;
    r->roll = s->roll;
    r->vx = s->vx;
    r->vy = s->vy;
    r->vz = s->vz;
}

static void write_free(SwcRig *r) {
    const SwcFlight *f = &r->free;
    r->x = f->x;
    r->y = f->y;
    r->z = f->z;
    r->yaw = f->yaw;
    r->pitch = f->pitch;
    r->roll = r->free_roll;
    r->vx = f->vx;
    r->vy = f->vy;
    r->vz = f->vz;
}

bool swc_rig_touring(const SwcRig *r) {
    return !(r->phase == SWC_PHASE_FREE || r->phase == SWC_PHASE_EASE);
}

bool swc_rig_resume_suggested(const SwcRig *r) {
    return r->phase == SWC_PHASE_FREE && r->idle >= SWC_IDLE_OFFER;
}

double swc_rig_speed(const SwcRig *r) {
    return hyp3(r->vx, r->vy, r->vz);
}

void swc_rig_init(SwcRig *r, const SwcCity *city, const SwcViewpoint *views, uint32_t seed) {
    memset(r, 0, sizeof *r);
    r->city = city;
    r->views = views;
    r->seed = seed;
    r->phase = SWC_PHASE_TOUR;
    r->free.x = SWC_TILE / 2.0;
    r->free.y = 200.0;
    r->free.z = SWC_TILE / 2.0;
    r->tour_fov = SWC_FOV_DEG;
    r->lens = SWC_FOV_DEG;
    swc_tour_init(&r->tour, city, seed);
    write_tour(r);
    r->fov = swc_round_half_up(r->lens * 20.0) / 20.0;
}

/* ---- changes of phase ---- */

/* Off the tour: no look offset, and the lens heads back to the default. */
static void drop_look(SwcRig *r) {
    r->look_yaw = r->look_pitch = 0.0;
    r->look_yaw_rate = r->look_pitch_rate = 0.0;
    r->look_idle = 0.0;
    r->tour_fov = SWC_FOV_DEG;
}

void swc_rig_take_over(SwcRig *r) {
    SwcFlight *f = &r->free;
    if (!swc_rig_touring(r))
        return;
    if (r->phase != SWC_PHASE_JOIN) {
        f->x = r->x;
        f->y = r->y;
        f->z = r->z;
        f->vx = r->vx;
        f->vy = r->vy;
        f->vz = r->vz;
        /* The pose last written already carries any free look. */
        f->yaw = r->yaw;
        f->pitch = swc_clamp(r->pitch, -SWC_MAX_PITCH, SWC_MAX_PITCH);
        r->free_roll = r->roll;
    } else {
        /* The join flies `free` itself; the free look was only added on output. */
        f->yaw = swc_wrap_delta(f->yaw + r->look_yaw, 0.0, SWC_TAU);
        f->pitch = swc_clamp(f->pitch + r->look_pitch, -SWC_MAX_PITCH, SWC_MAX_PITCH);
    }
    r->phase = SWC_PHASE_FREE;
    r->idle = 0.0;
    drop_look(r);
}

/* True if a sphere can travel straight from the camera to a point at height y without touching a box. */
static bool clear_to(const SwcRig *r, double x, double y, double z) {
    const SwcFlight *f = &r->free;
    double dx = swc_wrap_delta(x, f->x, SWC_TILE), dy = y - f->y, dz = swc_wrap_delta(z, f->z, SWC_TILE);
    double n = ceil(hyp3(dx, dy, dz) / 1.5);
    int steps = n < 1.0 ? 1 : (int)n;
    for (int i = 1; i <= steps; i++) {
        double u = (double)i / (double)steps;
        SwcVec3 p = {f->x + dx * u, f->y + dy * u, f->z + dz * u};
        if (swc_city_collide(r->city, &p, 1.0))
            return false;
    }
    return true;
}

/* Start returning to the tour: pick the nearest road that can be reached, and begin a tour there. */
static void start_join(SwcRig *r) {
    SwcFlight *f = &r->free;
    if (r->phase == SWC_PHASE_EASE) {
        f->x = r->x;
        f->y = r->y;
        f->z = r->z;
        f->vx = f->vy = f->vz = 0.0;
        f->yaw = r->yaw;
        f->pitch = swc_clamp(r->pitch, -SWC_MAX_PITCH, SWC_MAX_PITCH);
        r->free_roll = r->roll;
    }
    double px = swc_wrap_tile(f->x), pz = swc_wrap_tile(f->z);
    int cx = (int)floor(px / SWC_CELL), cz = (int)floor(pz / SWC_CELL);
    double lx = px - cx * SWC_CELL, lz = pz - cz * SWC_CELL;
    double y = fmax(f->y, 3.0);

    /* The four road lines round this cell: 0 and 1 run along z, 2 and 3 along x. */
    int best = -1, near = 0;
    double best_d = INFINITY, near_d = INFINITY;
    for (int c = 0; c < 4; c++) {
        double d = c == 0 ? lx : (c == 1 ? SWC_CELL - lx : (c == 2 ? lz : SWC_CELL - lz));
        if (d < near_d) {
            near_d = d;
            near = c;
        }
        if (d >= best_d)
            continue;
        double tx = c == 0 ? cx * SWC_CELL : (c == 1 ? (cx + 1) * SWC_CELL : px);
        double tz = c == 2 ? cz * SWC_CELL : (c == 3 ? (cz + 1) * SWC_CELL : pz);
        if (clear_to(r, tx, y, tz)) {
            best = c;
            best_d = d;
        }
    }
    r->via_active = false;
    r->join_limit = JOIN_TIMEOUT;
    if (best < 0) {
        /* Boxed in: rise above this cell's roofs first, then cross to the nearest road. */
        best = near;
        double top = (double)r->city->cells[swc_cell_index(cx, cz)].height * 4.0; /* CELL_HEIGHT_UNIT */
        y = fmin(SWC_MAX_ALTITUDE - 20.0, fmax(y, top + 25.0));
        r->via_active = true;
        r->via_y = y;
        r->join_limit = JOIN_TIMEOUT + (y - f->y) / 50.0;
    }
    bool along_z = best < 2;
    double jx = best == 0 ? cx * SWC_CELL : (best == 1 ? (cx + 1) * SWC_CELL : px);
    double jz = best == 2 ? cz * SWC_CELL : (best == 3 ? (cz + 1) * SWC_CELL : pz);

    /* Fly on in the direction the camera is already going, or failing that, looking. */
    bool moving = hypot(f->vx, f->vz) > 3.0;
    double want_x = moving ? f->vx : -sin(f->yaw);
    double want_z = moving ? f->vz : -cos(f->yaw);
    SwcTourStart start;
    start.x = jx;
    start.y = y;
    start.z = jz;
    start.heading = along_z ? (want_z <= 0.0 ? 0 : 2) : (want_x <= 0.0 ? 1 : 3);
    r->epoch++;
    uint32_t v[2] = {r->seed, r->epoch};
    swc_tour_restart(&r->tour, &start, swc_hash_ints(v, 2));
    swc_tour_sample(&r->tour, 0.0, &r->sample);
    r->join_x = r->sample.x;
    r->join_y = r->sample.y;
    r->join_z = r->sample.z;
    r->join_yaw = r->sample.yaw;
    r->join_pitch = r->sample.pitch;
    r->join_t = 0.0;
    r->tour_t = 0.0;
    r->yaw_rate = 0.0;
    r->pitch_rate = 0.0;
    r->phase = SWC_PHASE_JOIN;
}

void swc_rig_toggle(SwcRig *r) {
    if (swc_rig_touring(r))
        swc_rig_take_over(r);
    else
        start_join(r);
}

/* Land on the viewpoint and hand over to free flight. */
static void arrive(SwcRig *r) {
    const SwcViewpoint *target = &r->views[r->ease_index];
    SwcFlight *f = &r->free;
    f->x = target->x;
    f->y = target->y;
    f->z = target->z;
    f->vx = f->vy = f->vz = 0.0;
    f->yaw = target->yaw;
    f->pitch = target->pitch;
    r->free_roll = 0.0;
    r->phase = SWC_PHASE_FREE;
    write_free(r);
}

void swc_rig_glide_to_view(SwcRig *r, int index) {
    if (index < 0 || index >= SWC_VIEW_COUNT)
        return;
    r->from_x = r->x;
    r->from_y = r->y;
    r->from_z = r->z;
    r->from_yaw = r->yaw;
    r->from_pitch = r->pitch;
    r->from_roll = r->roll;
    r->ease_index = index;
    r->ease_t = 0.0;
    r->idle = 0.0;
    r->phase = SWC_PHASE_EASE;
    drop_look(r);
}

void swc_rig_jump_to_view(SwcRig *r, int index) {
    if (index < 0 || index >= SWC_VIEW_COUNT)
        return;
    r->ease_index = index;
    r->idle = 0.0;
    drop_look(r);
    arrive(r);
}

void swc_rig_place(SwcRig *r, double x, double y, double z, double yaw, double pitch) {
    SwcFlight *f = &r->free;
    f->x = swc_wrap_tile(x);
    f->y = y;
    f->z = swc_wrap_tile(z);
    f->vx = f->vy = f->vz = 0.0;
    f->yaw = yaw;
    f->pitch = swc_clamp(pitch, -SWC_MAX_PITCH, SWC_MAX_PITCH);
    r->free_roll = 0.0;
    r->phase = SWC_PHASE_FREE;
    r->idle = 0.0;
    drop_look(r);
    write_free(r);
}

/* ---- per frame ---- */

/* On the tour: add the viewer's free look to the pose just written, and take any zoom. */
static void apply_look(SwcRig *r, const SwcRigInput *in, double dt) {
    if (in->pan_yaw != 0.0 || in->pan_pitch != 0.0) {
        r->look_yaw = swc_wrap_delta(r->look_yaw + in->pan_yaw, 0.0, SWC_TAU);
        r->look_pitch += in->pan_pitch;
        r->look_idle = 0.0;
        r->look_yaw_rate = r->look_pitch_rate = 0.0;
    } else {
        r->look_idle += dt;
        if (r->look_idle > SWC_LOOK_HOLD) {
            /* Ease back, starting from rest, the short way round. */
            r->look_yaw =
                swc_spring_value(r->look_yaw, &r->look_yaw_rate, r->look_yaw, LOOK_RETURN_OMEGA, dt);
            r->look_pitch =
                swc_spring_value(r->look_pitch, &r->look_pitch_rate, r->look_pitch, LOOK_RETURN_OMEGA, dt);
        }
    }
    /* Keep the sum inside the free-flight pitch limit, so dragging back responds at once. */
    r->look_pitch = swc_clamp(r->look_pitch, -SWC_MAX_PITCH - r->pitch, SWC_MAX_PITCH - r->pitch);
    r->yaw = swc_wrap_delta(r->yaw + r->look_yaw, 0.0, SWC_TAU);
    r->pitch += r->look_pitch;
    if (in->zoom != 1.0 && in->zoom > 0.0) {
        double half = atan(tan(r->tour_fov * DEG * 0.5) * in->zoom);
        r->tour_fov = swc_clamp(2.0 * half / DEG, SWC_MIN_TOUR_FOV, SWC_MAX_TOUR_FOV);
    }
}

static void update_free(SwcRig *r, const SwcRigInput *in, double dt) {
    SwcFlight *f = &r->free;
    f->yaw = swc_wrap_delta(f->yaw + in->look_yaw + in->turn * SWC_TURN_RATE * dt, 0.0, SWC_TAU);
    f->pitch =
        swc_clamp(f->pitch + in->look_pitch + in->tilt * SWC_TURN_RATE * dt, -SWC_MAX_PITCH, SWC_MAX_PITCH);
    r->touched = swc_flight_step(r->city, f, in->forward, in->right, in->up,
                                 (in->boost ? SWC_BOOST : 1.0) * in->speed_scale, dt);
    /* Lean a little into a sideways slide. */
    double sideways = f->vx * cos(f->yaw) - f->vz * sin(f->yaw);
    double lean = -FREE_ROLL * swc_clamp(sideways / 70.0, -1.0, 1.0);
    r->free_roll += (lean - r->free_roll) * damp(4.0, dt);
    r->idle = in->active ? 0.0 : r->idle + dt;
    write_free(r);
}

/* Turn the free camera towards a direction with a critically damped spring. */
static void turn_towards(SwcRig *r, double yaw, double pitch, double dt) {
    SwcFlight *f = &r->free;
    double omega = JOIN_TURN_OMEGA;
    f->yaw = swc_wrap_delta(swc_spring_value(f->yaw, &r->yaw_rate, angle_delta(yaw, f->yaw), omega, dt), 0.0,
                            SWC_TAU);
    f->pitch = swc_spring_value(f->pitch, &r->pitch_rate, f->pitch - pitch, omega, dt);
    r->free_roll -= r->free_roll * damp(4.0, dt);
}

static void update_join(SwcRig *r, double dt) {
    SwcFlight *f = &r->free;
    r->join_t += dt;
    if (r->via_active) {
        /* Straight up, then across at that height. */
        swc_flight_step_towards(r->city, f, f->x, r->via_y, f->z, JOIN_OMEGA, JOIN_REACH, dt);
        if (fabs(f->y - r->via_y) < 6.0)
            r->via_active = false;
    } else {
        swc_flight_step_towards(r->city, f, r->join_x, r->join_y, r->join_z, JOIN_OMEGA, JOIN_REACH, dt);
    }
    turn_towards(r, r->join_yaw, r->join_pitch, dt);
    write_free(r);
    double off = hyp3(swc_wrap_delta(r->join_x, f->x, SWC_TILE), r->join_y - f->y,
                      swc_wrap_delta(r->join_z, f->z, SWC_TILE));
    bool settled = !r->via_active && off < 2.5 && hyp3(f->vx, f->vy, f->vz) < 6.0 &&
                   fabs(angle_delta(r->join_yaw, f->yaw)) < 0.6 && r->join_t >= JOIN_MIN_TIME;
    if (settled || r->join_t > r->join_limit) {
        r->phase = SWC_PHASE_BLEND;
        r->blend_t = 0.0;
        r->tour_t = 0.0;
    }
}

static void update_blend(SwcRig *r, double dt) {
    SwcFlight *f = &r->free;
    SwcTourSample *s = &r->sample;
    r->blend_t += dt;
    r->tour_t += dt;
    swc_tour_sample(&r->tour, r->tour_t, s);
    swc_flight_step_towards(r->city, f, r->join_x, r->join_y, r->join_z, JOIN_OMEGA, JOIN_REACH, dt);
    turn_towards(r, s->yaw, s->pitch, dt);
    double b = smoother(r->blend_t / SWC_BLEND_TIME);
    SwcVec3 p;
    p.x = f->x + swc_wrap_delta(s->x, f->x, SWC_TILE) * b;
    p.y = f->y + (s->y - f->y) * b;
    p.z = f->z + swc_wrap_delta(s->z, f->z, SWC_TILE) * b;
    /* Both ends are on the same straight road, so this does nothing unless the join timed out far away. */
    swc_city_collide(r->city, &p, SWC_CAMERA_RADIUS);
    r->x = swc_wrap_tile(p.x);
    r->y = p.y;
    r->z = swc_wrap_tile(p.z);
    r->yaw = swc_wrap_delta(f->yaw + angle_delta(f->yaw, s->yaw) * b, 0.0, SWC_TAU);
    r->pitch = f->pitch + (s->pitch - f->pitch) * b;
    r->roll = r->free_roll * (1.0 - b) + s->roll * b;
    r->vx = f->vx + (s->vx - f->vx) * b;
    r->vy = f->vy + (s->vy - f->vy) * b;
    r->vz = f->vz + (s->vz - f->vz) * b;
    if (r->blend_t >= SWC_BLEND_TIME)
        r->phase = SWC_PHASE_TOUR;
}

static void update_ease(SwcRig *r, double dt) {
    r->ease_t = dt > 0.0 ? r->ease_t + dt : SWC_EASE_TIME;
    if (r->ease_t >= SWC_EASE_TIME) {
        arrive(r);
        return;
    }
    const SwcViewpoint *target = &r->views[r->ease_index];
    double e = smoother(r->ease_t / SWC_EASE_TIME);
    double dx = swc_wrap_delta(target->x, r->from_x, SWC_TILE),
           dz = swc_wrap_delta(target->z, r->from_z, SWC_TILE);
    double previous_x = r->x, previous_y = r->y, previous_z = r->z;
    r->x = swc_wrap_tile(r->from_x + dx * e);
    r->y = r->from_y + (target->y - r->from_y) * e;
    r->z = swc_wrap_tile(r->from_z + dz * e);
    r->yaw = swc_wrap_delta(r->from_yaw + angle_delta(r->from_yaw, target->yaw) * e, 0.0, SWC_TAU);
    r->pitch = r->from_pitch + (target->pitch - r->from_pitch) * e;
    r->roll = r->from_roll * (1.0 - e);
    r->vx = swc_wrap_delta(r->x, previous_x, SWC_TILE) / dt;
    r->vy = (r->y - previous_y) / dt;
    r->vz = swc_wrap_delta(r->z, previous_z, SWC_TILE) / dt;
}

void swc_rig_update(SwcRig *r, SwcRigInput *in, double dt) {
    if (!(dt > 0.0))
        dt = 0.0;
    r->touched = false;

    if (r->phase != SWC_PHASE_FREE && r->phase != SWC_PHASE_EASE && in->active)
        swc_rig_take_over(r);

    switch (r->phase) {
    case SWC_PHASE_TOUR:
        r->tour_t += dt;
        write_tour(r);
        break;
    case SWC_PHASE_FREE:
        update_free(r, in, dt);
        break;
    case SWC_PHASE_JOIN:
        update_join(r, dt);
        break;
    case SWC_PHASE_BLEND:
        update_blend(r, dt);
        break;
    default:
        update_ease(r, dt);
        break;
    }
    bool touring = swc_rig_touring(r);
    if (touring)
        apply_look(r, in, dt);
    /* Look that arrived while it could not be used is dropped, not saved up. */
    in->look_yaw = in->look_pitch = 0.0;
    in->pan_yaw = in->pan_pitch = 0.0;
    in->zoom = 1.0;

    double kick = r->phase == SWC_PHASE_FREE ? 7.0 * swc_smoothstep(90.0, 420.0, swc_rig_speed(r)) : 0.0;
    r->fov_kick += (kick - r->fov_kick) * damp(3.0, dt);
    double lens_target = touring ? r->tour_fov : SWC_FOV_DEG;
    r->lens += (lens_target - r->lens) * damp(touring ? LENS_FOLLOW : LENS_RETURN, dt);
    if (fabs(lens_target - r->lens) < 0.01)
        r->lens = lens_target;
    r->fov = swc_round_half_up((r->lens + r->fov_kick) * 20.0) / 20.0;
}
