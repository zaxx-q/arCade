/* Synthwave City - the automatic drone tour. PURE C99, no raylib.
 * Ported 1:1 from the web version (src/camera/tour.ts).
 *
 * The route is a polyline of straight segments along road centre lines, each flown at a constant speed
 * with a linear change of altitude. The camera position is that polyline convolved, in time, with a
 * triangle kernel of half-width SWC_TOUR_SMOOTH seconds: that one operation rounds every corner, eases
 * every change of speed and climb, and gives a path with continuous acceleration. Speeds through corners
 * are limited so the rounded corner stays inside the street (cornerLimit), which is why the tour cannot
 * hit a building: air above every road is free at every height.
 *
 * The pose is a pure function of (city, start, seed, time): segments are generated on demand into a fixed
 * buffer, and asking for an earlier time regenerates from the start. Nothing is allocated. Same seed =>
 * same tour, on every machine (the numbers are checked against the TypeScript original). */
#ifndef SWC_TOUR_H
#define SWC_TOUR_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"
#include "swc_core.h"

#define SWC_TOUR_SMOOTH 2.6      /* half-width of the position kernel, seconds. A 90 degree turn takes 2x */
#define SWC_TOUR_LOOK_AHEAD 1.4  /* the camera looks at where it will be this many seconds from now */
#define SWC_TOUR_LOOK_SMOOTH 3.5 /* half-width of the kernel that fades looks towards landmarks */
#define SWC_TOUR_MIN_Y 6.0       /* lowest altitude the generator aims for, metres */
#define SWC_TOUR_MAX_ROLL 0.24   /* largest bank angle, radians */

#define SWC_TOUR_CAP 256     /* segments held at once */
#define SWC_TOUR_STRIDE 13   /* doubles per segment record */
#define SWC_TOUR_MAX_MEGA 16 /* megatowers remembered (= SWC_MAX_MEGA) */

/* Where a tour begins: a point on a road centre line, snapped onto the line the heading runs along. */
typedef struct SwcTourStart {
    double x, y, z; /* tile metres */
    int heading;    /* 0: -z (towards the sun), 1: -x, 2: +z, 3: +x */
} SwcTourStart;

typedef struct SwcTourSample {
    double x, y, z; /* position, x and z wrapped into 0..SWC_TILE */
    double yaw, pitch, roll;
    double vx, vy, vz; /* velocity, metres per second of tour time */
    double speed;
} SwcTourSample;

typedef struct SwcTour {
    const SwcCity *city;
    double seg[SWC_TOUR_CAP * SWC_TOUR_STRIDE];
    int count;
    int cursor;
    bool dropped;

    int mega_count;
    double mega_x[SWC_TOUR_MAX_MEGA], mega_z[SWC_TOUR_MAX_MEGA], mega_h[SWC_TOUR_MAX_MEGA];
    int mega_cell_x[SWC_TOUR_MAX_MEGA], mega_cell_z[SWC_TOUR_MAX_MEGA];

    double corner_limit[3]; /* street to street, mixed, avenue to avenue; metres per second */

    /* Start, kept for rewinds. */
    SwcTourStart start;
    uint32_t start_seed;
    bool canonical;

    /* Generator state: where the pending leg starts, and the pending leg itself. */
    SwcRng rng;
    double gx, gy, gz, gt;
    int p_h;
    double p_len, p_y1, p_v, p_corner;
    int p_look;
    double p_look_x, p_look_z, p_look_h, p_look_s;
    /* The leg after it, filled by decide(). */
    int n_turn;
    double n_len, n_y1, n_v;
    int n_look;
    double n_look_x, n_look_z, n_look_h, n_look_s;
    /* Scene program. */
    int scene;
    int legs_left, legs_max;
    double scene_y;
    int mega_index;
    int mega_phase;
    bool mega_done;
    int orbit_left;
    double orbit_top;
    int ring; /* blocks between the megatower and the road the orbit uses: 0 hugs its own block */
    int target_ix, target_iz;
    int last_mega, older_mega;
    double near_d;

    /* Scratch for the evaluator. */
    double ox, oy, oz, ovx, ovy, ovz;
    double bx, bz; /* origin the evaluator measures x and z from: the start of the segment being flown */
    SwcTourSample tap;
} SwcTour;

/* Builds a tour over `city` (which must outlive it) and starts it at the opening shot. */
void swc_tour_init(SwcTour *t, const SwcCity *city, uint32_t seed);

/* Begins a new tour. With NULL it starts from the opening shot: an avenue beside the densest downtown
 * cell, flying towards the sun. Otherwise it starts at the given point on a road. Time restarts at 0. */
void swc_tour_restart(SwcTour *t, const SwcTourStart *start, uint32_t seed);

/* The tour pose at time `time` (seconds since this tour began, >= 0). Asking for an earlier time than
 * before is fine (it regenerates from the start). */
void swc_tour_sample(SwcTour *t, double time, SwcTourSample *out);

/* Fastest constant speed through a corner from a road with free half-width `free_in` onto one with
 * `free_out`, metres per second. */
double swc_tour_corner_limit(double free_in, double free_out);

#endif
