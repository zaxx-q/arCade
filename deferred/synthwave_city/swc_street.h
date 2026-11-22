/* Synthwave City - the street life layer's data: which cars run in which lane, the grid of instance slots the
 * GPU draws, the crowds and the vehicle mesh. PURE C99, no raylib.
 * Ported from the web version (src/layers/streetlife/pack.ts and mesh.ts).
 *
 * Nothing is stored per car. A lane's cars are a pure function of (lane, index, loop phase), so the GPU draws
 * a fixed grid of "the k-th car from the camera in this lane of the j-th road from the camera" and the vertex
 * shader works out which car that is this frame. swc_street_slot_car() and swc_street_walker_at() below
 * mirror that arithmetic in double precision for the tests. */
#ifndef SWC_STREET_H
#define SWC_STREET_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"

/* Lane rows per road line in the lane table: axis * 6 + side * 3 + slot. */
#define SWC_STREET_ROWS_PER_AXIS 6
#define SWC_STREET_LANE_ROWS (SWC_STREET_ROWS_PER_AXIS * 2)
#define SWC_STREET_RECORD_MAX (SWC_CELLS * SWC_STREET_LANE_ROWS)

/* Smallest centre-to-centre distance between two cars in one lane, and the smallest even spacing, metres. */
#define SWC_STREET_MIN_GAP 16.0
#define SWC_STREET_MIN_SPACING 28.0
#define SWC_STREET_JITTER_MAX 0.5
#define SWC_STREET_SKIP_CHANCE 0.12
#define SWC_STREET_BUS_CHANCE 0.06
#define SWC_STREET_VAN_CHANCE 0.15
#define SWC_STREET_EMERGENCY_CHANCE 0.006
#define SWC_STREET_TAXI_CHANCE 0.06

/* Real cars are drawn inside HANDOVER_END (3D distance, metres); between the two they cross-fade into the far
 * dashes. */
#define SWC_STREET_HANDOVER_START 620.0
#define SWC_STREET_HANDOVER_END 880.0

/* People are drawn inside CROWD_END metres and shrink away from CROWD_START. */
#define SWC_CROWD_START 240.0
#define SWC_CROWD_END 330.0
#define SWC_CROWD_LINES 2
#define SWC_CROWD_BLOCKS_BEFORE 3
#define SWC_CROWD_BLOCKS_AFTER 2
#define SWC_CROWD_PER_FACE 64
#define SWC_CROWD_MARGIN 0.8
#define SWC_CROWD_END_MARGIN 1.0
#define SWC_CROWD_STANDING_CHANCE 0.16

typedef struct SwcLaneRec {
    uint8_t axis;  /* 0: runs along x, 1: along z */
    uint8_t line;  /* grid line, 0..39 */
    uint8_t row;   /* row in the lane table, 0..11 */
    int8_t dir;    /* +1 or -1 */
    int laps;      /* whole laps of the tile per loop: the speed */
    int count;     /* car slots in the lane (some are left empty) */
    double offset; /* signed distance from the road's centre line, metres */
    float phase;   /* phase of slot 0 at loop 0, as stored */
    double spacing;
} SwcLaneRec;

typedef struct SwcLaneTable {
    int record_count;
    SwcLaneRec records[SWC_STREET_RECORD_MAX];
    /* SWC_CELLS x SWC_STREET_LANE_ROWS texels, RGBA float: offset, dir * laps, count, phase. A count of 0
     * means no lane. Row-major: texel (line, row) is at ((row * SWC_CELLS + line) * 4). */
    float data[SWC_CELLS * SWC_STREET_LANE_ROWS * 4];
    int slots; /* total car slots across all lanes */
    double min_spacing, max_spacing;
} SwcLaneTable;

/* How full the lanes are for a quality tier's density multiplier (the streets thin out more gently than the
 * multiplier does): sqrt of it, clamped. */
double swc_street_traffic_density(double quality_density);

/* The lane table of a plan's street and avenue lanes (sky lanes are the air layer's). `density` is what
 * swc_street_traffic_density returns; `seed` is the layer seed. */
void swc_street_build_lanes(const SwcCity *city, double density, uint32_t seed, SwcLaneTable *out);

/* ---- the GLSL hashes, bit for bit (sc_bits, sc_hashu, sc_unit, sc_hash33) ---- */
void swc_street_hash33(double x, double y, double z, double out[3]);

/* ---- the car model ---- */
double swc_street_jitter_range(double spacing);
double swc_street_lane_id(const SwcLaneRec *rec);
/* The first hash triple of a car: x jitter, y presence, z tint. */
void swc_street_car_random(const SwcLaneRec *rec, int index, double out[3]);
bool swc_street_car_present(const SwcLaneRec *rec, int index);
/* Tile coordinate of car `index` along its lane at loop phase `loop` (0..1). */
double swc_street_car_coordinate(const SwcLaneRec *rec, int index, double loop);

/* ---- the instance grid of near cars: 4 floats per instance (axis, road offset j, lane row 0..5, car offset
 * k), nearest first. ---- */
int swc_street_car_field_lines(double radius);
int swc_street_car_field_reach(const SwcLaneTable *t, double radius);
int swc_street_car_field_count(const SwcLaneTable *t, double radius);
/* Writes count * 4 floats; returns the instance count (0 if `cap` instances do not fit). */
int swc_street_build_car_field(const SwcLaneTable *t, double radius, float *out, int cap);

typedef struct SwcPlacedCar {
    const SwcLaneRec *rec; /* NULL: no car in this slot */
    int index;             /* car index within its lane */
    double along, across;  /* offsets from the camera, metres, along the lane and across it */
} SwcPlacedCar;

/* Which car an instance slot draws for a camera at (cam_x, cam_z), as the vertex shader computes it. */
SwcPlacedCar swc_street_slot_car(const SwcLaneTable *t, double cam_x, double cam_z, double loop, int axis,
                                 int j, int sub, int k);

/* ---- crowds: 4 floats per instance (axis * 2 + side, road offset, block offset, index on the block face)
 * ---- */
int swc_street_crowd_count(int per_face);
int swc_street_build_crowd_field(int per_face, float *out, int cap);

typedef struct SwcWalker {
    double x, z;          /* tile coordinates (not wrapped into the tile) */
    double along, across; /* offsets from the camera along and across the road */
    bool standing;
    int laps; /* trips along the block face and back per loop; 0 when standing */
} SwcWalker;

SwcWalker swc_street_walker_at(double cam_x, double cam_z, double loop, int axis_side, int j, int b, int p);

/* ---- the vehicle mesh: the wedge-shaped car, with the van it can become stored per vertex ---- */
#define SWC_CAR_MESH_VERTEX_MAX 64
#define SWC_CAR_MESH_INDEX_MAX 256

/* Half length, height and half width of the two stored shapes, metres. */
extern const double SWC_CAR_DIMS[3];
extern const double SWC_VAN_DIMS[3];
extern const double SWC_BUS_SCALE[3]; /* a bus is the van stretched by this much along x, y and z */
extern const double SWC_CAR_FLOOR, SWC_VAN_FLOOR;
extern const double SWC_CAR_AXLES[3]; /* wheel centres along x (front, rear) and wheel radius */
extern const double SWC_VAN_AXLES[3];

typedef struct SwcCarMesh {
    int vertex_count, index_count;
    float position[SWC_CAR_MESH_VERTEX_MAX * 3];
    float alt[SWC_CAR_MESH_VERTEX_MAX * 3];
    float part[SWC_CAR_MESH_VERTEX_MAX]; /* 0 body, 1 cabin, 2 wheels */
    uint16_t index[SWC_CAR_MESH_INDEX_MAX];
} SwcCarMesh;

void swc_street_build_car_mesh(SwcCarMesh *out);

#endif
