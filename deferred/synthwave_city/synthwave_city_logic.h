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
#include "swc_rig.h"
#include "swc_song.h"
#include "swc_traffic.h"

/* The city a real window opens: the web version's default seed. (Smoke runs and tests keep their own.) */
#define SWC_DEFAULT_SEED 1984u
/* Rolled seeds (the N key) stay short enough to read out: 1..SWC_MAX_ROLLED (src/app/seed.ts). */
#define SWC_MAX_ROLLED 999999u

#define SWC_MOOD_COUNT 3 /* sunset, midnight, miami */
/* The soundtrack loop is bars 16..31 of the song: a verse that builds, then the chorus. */
#define SWC_MUSIC_FIRST_BAR 16
#define SWC_MUSIC_BARS 16

typedef struct synthwave_city_State {
    SwcCity city; /* about 2.5 MB: this struct must live in static storage */
    SwcTraffic traffic;
    SwcFlight cam; /* the pose the camera shows (copied from the rig after every step) */
    double roll;   /* bank angle the camera leans into a turn with, radians */
    double fov;    /* vertical field of view being drawn, degrees: widens a little at speed */
    SwcRig rig;    /* the camera rig: tour, free flight, joining the tour, gliding to a viewpoint */
    SwcViewpoint views[SWC_VIEW_COUNT];
    bool touring;       /* the drone flies the camera (the tour, or on its way back to it) */
    int mood;           /* 0 sunset, 1 midnight, 2 miami */
    int view;           /* last viewpoint jumped to (0..5), -1 for none */
    double speed_scale; /* mouse wheel: 0.25 .. 4 */
    double clock;       /* seconds into the traffic loop, 0..SWC_LOOP_SECONDS */
    double beat_time;   /* seconds into the music loop; the front-end syncs it to the audio when it plays */
    double distance;    /* metres flown */
    bool touched;       /* the camera brushed a building this step */
    /* The pointer, for drag looks and double clicks. */
    float prev_aim_x, prev_aim_y;
    bool prev_look;
    bool prev_aim_valid; /* prev_aim_* come from the previous frame (not from before a grab or a release) */
    bool pointer_locked; /* set by the front-end: it hides and captures the cursor, so the mouse looks */
    bool grab_wanted;    /* out: the viewer asked for the mouse (a click in flight, or the double click); the
                          * front-end grabs it and calls swc_set_pointer_lock(), which clears this */
    double time_ms;      /* milliseconds since the start, for the double click */
    bool tap_valid;
    double tap_start, last_tap_end;
    float tap_x, tap_y, last_tap_x, last_tap_y;
    bool tour_look; /* the controls were last mapped for the tour (a drag pans) */
} synthwave_city_State;

void synthwave_city_init(synthwave_city_State *s, unsigned seed);
/* A new seed from 1 to SWC_MAX_ROLLED, never `current`, drawn from `entropy` (the web version's rollSeed). */
unsigned swc_roll_seed(unsigned current, uint32_t entropy);
/* Builds another city and starts the tour over: the opening shot of the new city. Keeps the mood. */
void swc_new_city(synthwave_city_State *s, unsigned seed);

/* The front-end tells the logic that it grabbed (true) or freed (false) the mouse. While grabbed, moving the
 * mouse in free flight looks around without a button, as pointer lock does in the web version. */
void swc_set_pointer_lock(synthwave_city_State *s, bool locked);
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
/* Glides to viewpoint n over one second (what the number keys do), then free flight. */
void swc_glide_to_view(synthwave_city_State *s, int n);
/* Puts the camera at a pose at once, at rest, in free flight (the debug switches use it). */
void swc_place_camera(synthwave_city_State *s, double x, double y, double z, double yaw, double pitch);

#endif
