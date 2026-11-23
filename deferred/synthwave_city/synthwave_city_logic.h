/* Synthwave City - PURE game logic (no raylib, no drawing, no keyboard). Owner: arCade team
 * An endless synthwave megacity you fly through. The city comes from swc_city.h, the cars and aircraft from
 * swc_traffic.h; this file adds the camera: free flight with collision, an automatic drone tour, six named
 * viewpoints, and the clocks (traffic loop, beat, mood) the renderer reads.
 * Everything here is deterministic: same seed + same inputs => same result. */
#ifndef SYNTHWAVE_CITY_LOGIC_H
#define SYNTHWAVE_CITY_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "swc_city.h"
#include "swc_song.h"
#include "swc_traffic.h"

/* ---- camera tuning (src/camera/flight.ts of the web version) ---- */
#define SWC_CAMERA_RADIUS 1.8 /* collision sphere, metres; also the lowest eye height */
#define SWC_MAX_ALTITUDE 1500.0
#define SWC_BASE_SPEED 45.0      /* cruise speed at street level, m/s */
#define SWC_ALTITUDE_SPEED 300.0 /* speed grows by 1x for every this many metres of altitude */
#define SWC_BOOST 3.5
#define SWC_MAX_PITCH 1.5
#define SWC_FOV_DEG 62.0 /* vertical field of view */

/* The city a real window opens: the web version's default seed. (Smoke runs and tests keep their own.) */
#define SWC_DEFAULT_SEED 1984u

#define SWC_VIEW_COUNT 6
#define SWC_MOOD_COUNT 3 /* sunset, midnight, miami */
/* The soundtrack loop is bars 16..31 of the song: a verse that builds, then the chorus. */
#define SWC_MUSIC_FIRST_BAR 16
#define SWC_MUSIC_BARS 16

/* Free flight: a sphere with velocity. Position x and z are kept wrapped into 0..SWC_TILE. */
typedef struct SwcFlight {
    double x, y, z;
    double vx, vy, vz;
    double yaw, pitch; /* yaw 0 faces -z; positive turns towards -x. Pitch up is positive. */
} SwcFlight;

typedef struct SwcViewpoint {
    const char *name;
    double x, y, z, yaw, pitch;
} SwcViewpoint;

/* The automatic tour: a "carrot" runs along the road centre lines, turning at random intersections, climbing
 * and descending. The camera is a spring pulled towards it, so corners are rounded and the motion is smooth.
 */
typedef struct SwcTour {
    SwcRng rng;
    double px, py, pz; /* the carrot */
    double alt_target;
    int axis;      /* 0: runs along x on a z road line, 1: along z on an x road line */
    int dir;       /* +1 or -1 */
    int turn_next; /* planned at the next intersection: -1, 0 (straight) or +1 */
} SwcTour;

typedef struct synthwave_city_State {
    SwcCity city; /* about 2.5 MB: this struct must live in static storage */
    SwcTraffic traffic;
    SwcFlight cam;
    SwcTour tour;
    SwcViewpoint views[SWC_VIEW_COUNT];
    bool touring;       /* the drone flies the camera */
    int mood;           /* 0 sunset, 1 midnight, 2 miami */
    int view;           /* last viewpoint jumped to (0..5), -1 for none */
    double speed_scale; /* mouse wheel: 0.25 .. 4 */
    double clock;       /* seconds into the traffic loop, 0..SWC_LOOP_SECONDS */
    double beat_time;   /* seconds into the music loop; the front-end syncs it to the audio when it plays */
    double distance;    /* metres flown */
    bool touched;       /* the camera brushed a building this step */
    float prev_aim_x, prev_aim_y;
    bool prev_look;
} synthwave_city_State;

void synthwave_city_init(synthwave_city_State *s, unsigned seed);
void synthwave_city_step(synthwave_city_State *s, const ArcadeInput *in, float dt);
bool synthwave_city_is_over(const synthwave_city_State *s);
int synthwave_city_score(const synthwave_city_State *s); /* whole kilometres flown */

/* ---- helpers the front-end and the tests use ---- */

/* Seconds in one loop of the soundtrack at SWC_BPM. */
double swc_music_seconds(void);
/* Re-aligns the beat clock with the audio (seconds into the music loop). */
void swc_sync_beat(synthwave_city_State *s, double music_seconds);
/* Position inside the current beat, 0..1; 0 on the beat. */
double swc_beat_phase(const synthwave_city_State *s);
/* A short pulse: 1 on the beat, falling to ~0 by the next one. */
double swc_beat_pulse(const synthwave_city_State *s);
/* Loop fraction 0..1 the traffic uses. */
double swc_loop_fraction(const synthwave_city_State *s);

/* Teleports to viewpoint n (0..5) and switches to free flight. */
void swc_goto_view(synthwave_city_State *s, int n);

/* Top speed at an altitude, m/s. `speed_mul` is 1 for cruising, SWC_BOOST when boosting. */
double swc_top_speed(double y, double speed_mul);

/* One step of flight (axes -1..1 in the camera's frame). Returns true if it touched a building. */
bool swc_flight_step(const SwcCity *city, SwcFlight *f, double forward, double right, double up,
                     double speed_mul, double dt);
/* Slides by (dx, dy, dz), never entering a building. Returns true if it touched one. */
bool swc_flight_move(const SwcCity *city, SwcFlight *f, double dx, double dy, double dz);

#endif
