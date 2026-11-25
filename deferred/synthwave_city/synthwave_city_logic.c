/* Synthwave City - game logic (see synthwave_city_logic.h). PURE C99, no raylib.
 * The flight model (swc_flight.c), the drone tour (swc_tour.c) and the camera rig (swc_rig.c) are ported from
 * src/camera/ of the web version, and the viewpoints from src/camera/viewpoints.ts. This file maps the
 * controls onto the rig, and keeps the clocks. */
#include "synthwave_city_logic.h"

#include <math.h>
#include <string.h>

/* The mouse and the double click (src/camera/input.ts of the web version). */
#define MOUSE_SENSITIVITY 0.0022 /* radians per pixel of mouse drag */
#define WHEEL_ZOOM 0.0015        /* lens zoom per pixel of wheel travel: a notch is 100 pixels */
#define WHEEL_NOTCH_PIXELS 100.0
#define TAP_TIME 300.0        /* a tap is a press no longer than this, milliseconds... */
#define TAP_SLOP 12.0         /* ...that moves no further than this, pixels */
#define DOUBLE_TAP_GAP 350.0  /* the second tap goes down within this long after the first lifted... */
#define DOUBLE_TAP_REACH 40.0 /* ...and this close to where the first one went down */
#define MAX_DRAG_PIXELS 300.0 /* a bigger jump of the pointer in one frame is ignored */

static double hyp3(double a, double b, double c) {
    return sqrt(a * a + b * b + c * c);
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

/* The rig's pose is what the camera shows. */
static void show_rig(synthwave_city_State *s) {
    const SwcRig *r = &s->rig;
    s->cam.x = r->x;
    s->cam.y = r->y;
    s->cam.z = r->z;
    s->cam.vx = r->vx;
    s->cam.vy = r->vy;
    s->cam.vz = r->vz;
    s->cam.yaw = r->yaw;
    s->cam.pitch = r->pitch;
    s->roll = r->roll;
    s->fov = r->fov;
    s->touring = swc_rig_touring(r);
}

void swc_goto_view(synthwave_city_State *s, int n) {
    if (n < 0 || n >= SWC_VIEW_COUNT)
        return;
    swc_rig_jump_to_view(&s->rig, n);
    s->view = n;
    show_rig(s);
}

void swc_glide_to_view(synthwave_city_State *s, int n) {
    if (n < 0 || n >= SWC_VIEW_COUNT)
        return;
    swc_rig_glide_to_view(&s->rig, n);
    s->view = n;
    show_rig(s);
}

void swc_place_camera(synthwave_city_State *s, double x, double y, double z, double yaw, double pitch) {
    swc_rig_place(&s->rig, x, y, z, yaw, pitch);
    s->view = -1;
    show_rig(s);
}

void synthwave_city_init(synthwave_city_State *s, unsigned seed) {
    memset(s, 0, sizeof(*s));
    swc_city_build(&s->city, (uint32_t)seed);
    swc_traffic_init(&s->traffic, &s->city, 0.77); /* the web version's "medium" quality traffic */
    compute_viewpoints(s);
    s->mood = 0;
    s->view = -1;
    s->speed_scale = 1.0;
    s->last_tap_end = -INFINITY;

    /* The opening shot of the tour: an avenue beside downtown, flying towards the sun. */
    swc_rig_init(&s->rig, &s->city, s->views, (uint32_t)seed);
    s->tour_look = true;
    show_rig(s);
}

unsigned swc_roll_seed(unsigned current, uint32_t entropy) {
    SwcRng rng;
    swc_rng_seed(&rng, entropy);
    for (int i = 0; i < 8; i++) {
        double pick = floor(swc_rng_next(&rng) * (double)SWC_MAX_ROLLED);
        unsigned seed = 1u + (unsigned)fmin((double)(SWC_MAX_ROLLED - 1u), pick);
        if (seed != current)
            return seed;
    }
    return current == 1u ? 2u : 1u;
}

void swc_new_city(synthwave_city_State *s, unsigned seed) {
    int mood = s->mood;
    synthwave_city_init(s, seed);
    s->mood = mood;
}

/* The controls for this frame, in the rig's terms. The mouse maps as in src/camera/input.ts: on the tour a
 * drag pans the view and the wheel zooms the lens, and two quick clicks take over; in flight a drag looks. */
static void map_controls(synthwave_city_State *s, const ArcadeInput *in, double dt, SwcRigInput *ri,
                         bool *takeover) {
    swc_rig_input_clear(ri);
    *takeover = false;
    s->time_ms += dt * 1000.0;
    bool touring = swc_rig_touring(&s->rig);
    if (touring != s->tour_look) {
        /* The controls changed meaning: forget a half-made click. */
        s->tour_look = touring;
        s->tap_valid = false;
        s->last_tap_end = -INFINITY;
    }

    bool down = in->fire && !s->prev_look; /* the left button went down */
    bool held = in->fire && s->prev_look;
    bool up = !in->fire && s->prev_look;
    double dx = 0.0, dy = 0.0;
    if (held) {
        dx = swc_clamp((double)(in->aim_x - s->prev_aim_x), -MAX_DRAG_PIXELS, MAX_DRAG_PIXELS);
        dy = swc_clamp((double)(in->aim_y - s->prev_aim_y), -MAX_DRAG_PIXELS, MAX_DRAG_PIXELS);
    }
    if (touring) {
        if (down) {
            /* The press may become a tap; a second one too late or too far away starts afresh. */
            s->tap_valid = true;
            s->tap_start = s->time_ms;
            s->tap_x = in->aim_x;
            s->tap_y = in->aim_y;
            if (s->time_ms - s->last_tap_end > DOUBLE_TAP_GAP ||
                hypot((double)(s->tap_x - s->last_tap_x), (double)(s->tap_y - s->last_tap_y)) >
                    DOUBLE_TAP_REACH)
                s->last_tap_end = -INFINITY;
        }
        if (held) {
            if (s->tap_valid &&
                hypot((double)(in->aim_x - s->tap_x), (double)(in->aim_y - s->tap_y)) > TAP_SLOP)
                s->tap_valid = false;
            ri->pan_yaw -= dx * MOUSE_SENSITIVITY;
            ri->pan_pitch -= dy * MOUSE_SENSITIVITY;
        }
        if (up) {
            bool tap = s->tap_valid && s->time_ms - s->tap_start <= TAP_TIME &&
                       hypot((double)(in->aim_x - s->tap_x), (double)(in->aim_y - s->tap_y)) <= TAP_SLOP;
            s->tap_valid = false;
            if (!tap) {
                s->last_tap_end = -INFINITY;
            } else if (s->last_tap_end > -INFINITY) {
                s->last_tap_end = -INFINITY;
                *takeover = true;
            } else {
                s->last_tap_end = s->time_ms;
                s->last_tap_x = s->tap_x;
                s->last_tap_y = s->tap_y;
            }
        }
        /* Scrolling down widens the view, as on a map (the wheel reports up as positive). */
        if (in->wheel != 0)
            ri->zoom *= exp(swc_clamp(-(double)in->wheel * WHEEL_NOTCH_PIXELS * WHEEL_ZOOM, -1.0, 1.0));
    } else {
        ri->look_yaw -= dx * MOUSE_SENSITIVITY;
        ri->look_pitch -= dy * MOUSE_SENSITIVITY;
        /* The wheel scales the cruise speed by 1.25 per step. */
        if (in->wheel != 0)
            s->speed_scale = swc_clamp(s->speed_scale * (in->wheel > 0 ? 1.25 : 0.8), 0.25, 4.0);
    }
    s->prev_look = in->fire;
    s->prev_aim_x = in->aim_x;
    s->prev_aim_y = in->aim_y;

    ri->forward = swc_clamp(-(double)in->move_y, -1.0, 1.0);
    ri->right = swc_clamp((double)in->move_x, -1.0, 1.0);
    ri->boost = in->alt;
    ri->speed_scale = s->speed_scale;
    ri->active =
        fabs(ri->forward) > 0.01 || fabs(ri->right) > 0.01 || ri->look_yaw != 0.0 || ri->look_pitch != 0.0;
}

void synthwave_city_step(synthwave_city_State *s, const ArcadeInput *in, float dt_f) {
    double dt = (double)dt_f;
    if (!(dt > 0.0))
        return;
    if (dt > 0.25)
        dt = 0.25;
    s->clock = swc_wrap(s->clock + dt, SWC_LOOP_SECONDS);
    s->beat_time = swc_wrap(s->beat_time + dt, swc_music_seconds());

    /* keys: T cycles the mood, 1..6 glide to a named viewpoint, Space toggles the tour */
    int key = in->typed;
    if (key == 't' || key == 'T')
        s->mood = (s->mood + 1) % SWC_MOOD_COUNT;
    if (key >= '1' && key < '1' + SWC_VIEW_COUNT)
        swc_glide_to_view(s, key - '1');
    if (in->action_pressed)
        swc_rig_toggle(&s->rig);

    SwcRigInput ri;
    bool takeover;
    map_controls(s, in, dt, &ri, &takeover);
    if (takeover)
        swc_rig_take_over(&s->rig);

    double px = s->cam.x, py = s->cam.y, pz = s->cam.z;
    swc_rig_update(&s->rig, &ri, dt);
    show_rig(s);
    s->touched = s->rig.touched;
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
