/* Synthwave City - the air traffic layer's data: which craft fly in which sky lane, the routes of the hero
 * ships, the hull meshes and the light tables. PURE C99, no raylib.
 * Ported from the web version (src/layers/airtraffic/pack.ts and shapes.ts). Every craft is one instance with
 * the same sixteen floats whatever its shape; where it is at any moment is a closed-form function of the loop
 * phase (the vertex shaders' at_craft() and at_hero(), mirrored by swc_air_craft_state and swc_air_hero_state
 * below), so nothing is animated on the CPU.
 *
 * Hull frame: x to the craft's right, y up, z BACKWARDS (the nose is at -z). */
#ifndef SWC_AIR_H
#define SWC_AIR_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"

/* ---- small craft ---- */
#define SWC_CRAFT_CAR 0
#define SWC_CRAFT_HAULER 1
#define SWC_CRAFT_DRONE 2
#define SWC_CRAFT_SHAPES 3

#define SWC_SKY_LANE_MAX_WEAVE 6.0 /* metres: the largest sideways sway of a craft */

/* Floats per craft, four vec4 instance attributes: aLane, aMove, aBob, aLook. */
#define SWC_CRAFT_STRIDE 16
enum {
    SWC_CRAFT_FIXED = 0, /* the lane's fixed coordinate plus this craft's constant sideways offset */
    SWC_CRAFT_Y,         /* lane altitude plus this craft's constant vertical offset */
    SWC_CRAFT_AXIS,      /* 0: travels along x, 1: along z */
    SWC_CRAFT_DIR,       /* +1 or -1 */
    SWC_CRAFT_LAPS,      /* whole laps of the tile per loop */
    SWC_CRAFT_PHASE,
    SWC_CRAFT_WEAVE_AMP,
    SWC_CRAFT_WEAVE_CYCLES, /* whole weave cycles per loop */
    SWC_CRAFT_BOB_AMP,
    SWC_CRAFT_BOB_CYCLES,
    SWC_CRAFT_TRAIL, /* trail length, metres */
    SWC_CRAFT_SCALE,
    SWC_CRAFT_SEED, /* 0..1, a multiple of 1/4096 so it is exact in float32 */
    SWC_CRAFT_SHAPE,
    SWC_CRAFT_ENGINE_COLOR, /* palette index of the engines and the trail */
    SWC_CRAFT_ACCENT_COLOR  /* palette index of the hull accents */
};

/* The most craft a plan can hold at full density: 160 sky lanes with at most 74 craft each, with margin. */
#define SWC_CRAFT_MAX 12288

typedef struct SwcCraftPack {
    int count;
    int shape_start[SWC_CRAFT_SHAPES + 1]; /* first craft of each shape; [SHAPES] == count */
    float
        data[SWC_CRAFT_MAX * SWC_CRAFT_STRIDE]; /* sorted by shape. About 790 KB: keep it in static storage */
    uint16_t lane_of[SWC_CRAFT_MAX];            /* index into the plan's sky lanes, for each craft */
} SwcCraftPack;

/* The seed the layer's packers take: the city seed mixed with the layer's name, as the web engine does. */
uint32_t swc_air_layer_seed(uint32_t city_seed);

/* Speed of a lane in whole laps per loop, and craft in a lane at a density (1 = full). */
int swc_air_laps_for_lane(int slot, double h);
int swc_air_craft_in_lane(int slot, double h, double density);

/* Fills every sky lane with craft. Deterministic in (city lanes, density, layer seed). */
void swc_air_pack_craft(const SwcCity *city, double density, uint32_t seed, SwcCraftPack *out);

typedef struct SwcCraftState {
    double x, y, z; /* tile coordinates of the craft's centre */
    double along;   /* position along the lane, [0, SWC_TILE) */
    double lateral; /* offset from the lane's fixed coordinate (the weave only) */
} SwcCraftState;

/* The motion the vertex shader computes, in double precision. */
void swc_air_craft_state(const float *data, int index, double loop, SwcCraftState *out);

/* ---- hero ships ---- */
#define SWC_HERO_FREIGHTER 0
#define SWC_HERO_AIRSHIP 1
#define SWC_HERO_PATROL 2
#define SWC_HERO_TYPES 3

#define SWC_HERO_STRIDE 8 /* two vec4 instance attributes: aRoute, aRun */
enum {
    SWC_HERO_FIXED = 0,
    SWC_HERO_Y,
    SWC_HERO_AXIS,
    SWC_HERO_DIR,
    SWC_HERO_LAPS,
    SWC_HERO_PHASE,
    SWC_HERO_SEED,
    SWC_HERO_TYPE
};
#define SWC_HERO_MAX 8

typedef struct SwcHeroLimits {
    double half_width, half_length, below, above;
    int laps;       /* whole laps per loop */
    double bob_amp; /* vertical drift amplitude, metres */
    int bob_cycles; /* whole cycles per loop */
} SwcHeroLimits;

const SwcHeroLimits *swc_air_hero_limits(int type);

typedef struct SwcHeroPack {
    int count;
    float data[SWC_HERO_MAX * SWC_HERO_STRIDE];
} SwcHeroPack;

/* Routes for the hero ships (four wide ones above the avenues, two patrol craft in the canyons); `seed` is
 * the layer seed. */
void swc_air_plan_heroes(const SwcCity *city, uint32_t seed, SwcHeroPack *out);

typedef struct SwcHeroState {
    double x, y, z, along;
} SwcHeroState;
void swc_air_hero_state(const float *data, int index, double loop, SwcHeroState *out);

/* ---- hull meshes ---- */
#define SWC_HULL_MAX_VERTICES 512
#define SWC_HULL_MAX_INDICES 2304
#define SWC_HULL_MAX_PIECES 48

/* Part ids the fragment shaders read. */
enum {
    SWC_PART_BODY = 0,
    SWC_PART_GLASS,
    SWC_PART_ENGINE,
    SWC_PART_CARGO,
    SWC_PART_FIN,
    SWC_PART_LIGHTBAR,
    SWC_PART_LAMP
};

typedef struct SwcHull {
    int vertex_count, index_count, piece_count;
    float position[SWC_HULL_MAX_VERTICES * 3];
    float part[SWC_HULL_MAX_VERTICES]; /* a SWC_PART_ value per vertex */
    float type[SWC_HULL_MAX_VERTICES]; /* the hero type (0 for the small craft, one mesh per shape) */
    uint16_t index[SWC_HULL_MAX_INDICES];
    int piece_start[SWC_HULL_MAX_PIECES], piece_length[SWC_HULL_MAX_PIECES]; /* index ranges of the pieces */
    bool piece_closed[SWC_HULL_MAX_PIECES];
} SwcHull;

void swc_air_build_craft_hull(int shape, SwcHull *out);
void swc_air_build_hero_hull(SwcHull *out);

/* ---- lights ---- */
#define SWC_CRAFT_LIGHTS 5 /* two engines, one headlamp, a strobe and a beacon */

/* Per shape, SWC_CRAFT_LIGHTS entries of (x, y, z, half-size in metres) at scale 1; a half-size of 0 means
 * the shape has no such light. SWC_CRAFT_LIGHT_FLOATS floats in all. */
#define SWC_CRAFT_LIGHT_FLOATS (SWC_CRAFT_SHAPES * SWC_CRAFT_LIGHTS * 4)
const float *swc_air_craft_light_table(void);
/* Per shape: where the trail starts (y, z), half the distance between the two exhaust lines, and the
 * half-width of one line. SWC_CRAFT_TRAIL_FLOATS floats in all. */
#define SWC_CRAFT_TRAIL_FLOATS (SWC_CRAFT_SHAPES * 4)
const float *swc_air_craft_trail_table(void);

/* How a hero light behaves; read by the hero light shader. */
enum {
    SWC_GLOW_STEADY = 0,
    SWC_GLOW_STROBE = 1,
    SWC_GLOW_PULSE = 2,
    SWC_GLOW_POLICE_A = 3,
    SWC_GLOW_POLICE_B = 4,
    SWC_GLOW_ENGINE = 5,
    SWC_GLOW_SEARCHLAMP = 6,
    SWC_GLOW_HEAD = 7,
    SWC_GLOW_CONE = 10,
    SWC_GLOW_POOL = 11,
    SWC_GLOW_PLUME = 12
};

typedef struct SwcHeroLight {
    int type;
    float x, y, z;
    float size;  /* sprite half-size, metres; for a plume its length */
    float color; /* palette index; a fraction blends two neighbours */
    int mode;
    float phase;
} SwcHeroLight;

#define SWC_HERO_LIGHT_MAX 96
/* Fills `out` (SWC_HERO_LIGHT_MAX entries) and returns the count. */
int swc_air_hero_lights(SwcHeroLight *out);

#endif
