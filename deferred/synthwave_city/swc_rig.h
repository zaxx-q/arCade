/* Synthwave City - the camera rig: one camera that is either on the drone tour or in the viewer's hands, and
 * moves between the two without a jump. PURE C99, no raylib. Ported from src/camera/rig.ts of the web
 * version (the part that is not DOM: the input mapping lives in synthwave_city_logic.c).
 *
 *   tour   the pose is the tour's
 *   free   flight physics driven by the controls
 *   join   returning to the tour, part one: flight physics pulled to the nearest road, with collision
 *   blend  part two: a new tour has started from that point and the pose cross-fades onto it
 *   ease   a one-second glide to a viewpoint, after which the mode is free
 *
 * The rig also leans the camera into turns (roll), adds the viewer's free look and zoom on the tour, and
 * widens the lens a little at high speed. */
#ifndef SWC_RIG_H
#define SWC_RIG_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"
#include "swc_flight.h"
#include "swc_tour.h"

#define SWC_FOV_DEG 62.0 /* vertical field of view of the web version's camera (CAMERA_FOV), degrees */
#define SWC_VIEW_COUNT 6

typedef struct SwcViewpoint {
    const char *name;
    double x, y, z, yaw, pitch;
} SwcViewpoint;

typedef enum SwcPhase {
    SWC_PHASE_TOUR = 0,
    SWC_PHASE_FREE = 1,
    SWC_PHASE_JOIN = 2,
    SWC_PHASE_BLEND = 3,
    SWC_PHASE_EASE = 4
} SwcPhase;

/* Seconds without input in free flight before the app may offer to resume the tour. */
#define SWC_IDLE_OFFER 30.0
#define SWC_EASE_TIME 1.0     /* seconds a glide to a viewpoint takes */
#define SWC_BLEND_TIME 2.5    /* seconds the cross-fade back onto the tour takes */
#define SWC_TURN_RATE 1.3     /* yaw and pitch rate of key turning at full deflection, radians per second */
#define SWC_LOOK_HOLD 5.0     /* seconds a free look on the tour holds after the last drag */
#define SWC_MIN_TOUR_FOV 25.0 /* narrowest and widest lens a zoom can choose on the tour, degrees */
#define SWC_MAX_TOUR_FOV 85.0

/* What the controls ask for right now (the web version's InputState). Axes are -1..1. */
typedef struct SwcRigInput {
    double forward; /* along the view direction */
    double right;   /* along the horizon to the right */
    double up;      /* along the world's up */
    bool boost;
    /* Look waiting to be applied, radians. Positive yaw turns left, positive pitch looks up. The rig zeroes
     * these after each update, so look that arrived while it could not be used is dropped. */
    double look_yaw, look_pitch;
    /* Steady turning from keys or a stick, -1..1: positive turns left, positive tilt looks up. */
    double turn, tilt;
    /* Free look on the tour, radians, waiting to be applied: a drag while the camera is touring. Kept apart
     * from look_* so it never counts as taking over. */
    double pan_yaw, pan_pitch;
    /* Lens zoom waiting to be applied, as a factor on tan(fov / 2): below 1 zooms in. Reset to 1. */
    double zoom;
    /* Multiplier on the top speed of free flight: 1 is the web version's. (The wheel sets it in this port.)
     */
    double speed_scale;
    /* True while anything asks the camera to move or turn (a flight key, a free-flight look). */
    bool active;
} SwcRigInput;

typedef struct SwcRig {
    const SwcCity *city;
    const SwcViewpoint *views; /* SWC_VIEW_COUNT entries */
    uint32_t seed;
    SwcTour tour;
    SwcTourSample sample;
    SwcFlight free; /* the flight body: the pose in free flight, the join and the blend */

    SwcPhase phase;
    double tour_t;
    uint32_t epoch;
    double idle;
    double free_roll;
    double fov_kick;
    double yaw_rate, pitch_rate;

    /* Free look on the tour: an offset from the tour's view, and a lens. */
    double look_yaw_rate, look_pitch_rate;
    double look_yaw, look_pitch, look_idle;
    double tour_fov, lens;

    /* Returning to the tour. */
    double join_x, join_y, join_z, join_yaw, join_pitch, join_t, blend_t, join_limit;
    bool via_active;
    double via_y;

    /* Gliding to a viewpoint. */
    double ease_t;
    int ease_index;
    double from_x, from_y, from_z, from_yaw, from_pitch, from_roll;

    /* The pose and velocity last written: what the renderer draws. */
    double x, y, z, yaw, pitch, roll;
    double vx, vy, vz;
    bool touched; /* free flight brushed a building in the last update */
    double fov;   /* degrees, as the lens is right now (rounded to 1/20 degree like the web version) */
} SwcRig;

/* A rig over `city` and the viewpoints (both must outlive it), starting on the tour. */
void swc_rig_init(SwcRig *r, const SwcCity *city, const SwcViewpoint *views, uint32_t seed);

/* An input state with nothing asked for. */
void swc_rig_input_clear(SwcRigInput *in);

/* One frame. Consumes (zeroes) the pending look, pan and zoom of `in`. */
void swc_rig_update(SwcRig *r, SwcRigInput *in, double dt);

/* True on the tour, the join and the blend; false in free flight and the glide to a viewpoint. */
bool swc_rig_touring(const SwcRig *r);
/* True once free flight has had no input for SWC_IDLE_OFFER seconds. */
bool swc_rig_resume_suggested(const SwcRig *r);
/* Current speed, metres per second. */
double swc_rig_speed(const SwcRig *r);

/* Space: the tour hands over to free flight, free flight returns to the tour. */
void swc_rig_toggle(SwcRig *r);
/* Hand control to the viewer from wherever the camera is (a double click on the tour). */
void swc_rig_take_over(SwcRig *r);
/* Glide to viewpoints[index] and stay there in free flight. Out-of-range indices are ignored. */
void swc_rig_glide_to_view(SwcRig *r, int index);
/* Jump to viewpoints[index] at once, in free flight. */
void swc_rig_jump_to_view(SwcRig *r, int index);
/* Put the camera at a pose at once, in free flight, at rest. */
void swc_rig_place(SwcRig *r, double x, double y, double z, double yaw, double pitch);

#endif
