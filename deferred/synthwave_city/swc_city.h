/* Synthwave City - the city plan. PURE C99, no raylib.
 *
 * A seed goes in, plain data comes out: a 40 x 40 grid of cells (districts), about 17,000 axis-aligned
 * building boxes, 520 traffic lanes and about 2,000 "sites" for holograms, searchlights and beacons.
 * Ported from the web version (the files in src/city/); the same seed gives the same city.
 *
 * Coordinates are tile coordinates: x and z in 0..SWC_TILE, y up, road surface at y = 0. The tile repeats,
 * so every query wraps around its edges. */
#ifndef SWC_CITY_H
#define SWC_CITY_H

#include "swc_core.h"

/* ---- capacities (the web version builds 16.5k-17.2k boxes and 1.9k-2.3k sites for every seed tried) ---- */
#define SWC_MAX_BOXES 24000
#define SWC_MAX_BUILDINGS 16000
#define SWC_MAX_SITES 3600
#define SWC_MAX_LANES 600
#define SWC_MAX_LANDMARKS 160
#define SWC_MAX_MEGA 16    /* megatowers (the plan builds 12) */
#define SWC_MAX_SEGMENTS 8 /* stacked segments of one megatower (5 to 7) */
#define SWC_MAX_PLAZAS 80
#define SWC_PALETTE_SIZE 8 /* neon palette indices 0..7, see synthwave_city_render_game.c */

typedef enum SwcDistrict {
    SWC_LOWRISE = 0,
    SWC_MIDTOWN = 1,
    SWC_DOWNTOWN = 2,
    SWC_PLAZA = 3,
    SWC_MEGA = 4
} SwcDistrict;

typedef enum SwcBoxStyle {
    SWC_STYLE_PODIUM = 0, /* wide base with shop fronts */
    SWC_STYLE_SHAFT = 1,  /* punched-window tower shaft */
    SWC_STYLE_GLASS = 2,  /* curtain-glass tower */
    SWC_STYLE_SLAB = 3,   /* horizontal ribbon windows */
    SWC_STYLE_STRIP = 4,  /* vertical-strip tower */
    SWC_STYLE_CROWN = 5,  /* lit cap on top of a tower */
    SWC_STYLE_SPIRE = 6,  /* thin mast */
    SWC_STYLE_MEGA = 7,   /* megatower segment */
    SWC_STYLE_KIOSK = 8   /* low plaza kiosk */
} SwcBoxStyle;

#define SWC_FLAG_ROOF 1     /* top of its stack: the roof is exposed */
#define SWC_FLAG_STREET 2   /* bottom of its stack: it meets the street */
#define SWC_FLAG_ROUND 4    /* a cylinder inscribed in the box */
#define SWC_FLAG_LANDMARK 8 /* part of a megatower */

/* One cell of the 40 x 40 grid. */
typedef struct SwcCell {
    uint8_t district; /* SwcDistrict */
    uint8_t palette;  /* main neon colour of the neighbourhood, 0..7 */
    uint8_t height;   /* tallest box in the cell, in units of 4 m (capped at 255) */
    uint8_t seed;     /* a random byte, fixed per cell */
} SwcCell;

/* An axis-aligned box. Stored as float, like the web version's Float32Array. */
typedef struct SwcBox {
    float cx, y0, cz;         /* centre x, base y, centre z */
    float sx, sy, sz;         /* full size along x, y, z */
    float seed;               /* random 0..1, fixed per box */
    int building;             /* index of the building this box belongs to */
    uint8_t style;            /* SwcBoxStyle */
    uint8_t color_a, color_b; /* neon palette indices: main trim and accent */
    uint8_t flags;            /* SWC_FLAG_* */
} SwcBox;

/* One building = a stack of boxes. Kept in double because the sites are placed from these numbers. */
typedef struct SwcBuilding {
    double x, z;           /* centre */
    double base_x, base_z; /* footprint of the base */
    double top_x, top_z;   /* footprint of the top segment */
    double top;            /* roof height, not counting a spire */
    double spire_top;      /* tip of the spire, or 0 when there is none */
    int id, cell;
    int mega_slot; /* index into SwcCity.segments for megatowers, else -1 */
    int segment_count;
    uint8_t district;
    uint8_t has_crown, round;
    uint8_t color_a, color_b;
} SwcBuilding;

/* One stacked segment of a megatower. */
typedef struct SwcSegment {
    double y0, h, sx, sz;
} SwcSegment;

typedef enum SwcLaneKind { SWC_LANE_STREET = 0, SWC_LANE_AVENUE = 1, SWC_LANE_SKY = 2 } SwcLaneKind;

/* A traffic lane: a straight line along x (axis 0) or z (axis 1). */
typedef struct SwcLane {
    double fixed; /* the constant coordinate, tile metres */
    double y;     /* 0 for ground lanes, the altitude for sky lanes */
    int line;     /* grid line this lane belongs to, 0..39 */
    int slot;     /* position among its direction's lanes, 0 nearest the centre line */
    int8_t dir;   /* +1 towards increasing coordinate, -1 towards decreasing */
    uint8_t axis; /* 0: runs along x (z is fixed). 1: runs along z (x is fixed) */
    uint8_t kind; /* SwcLaneKind */
} SwcLane;

typedef enum SwcSiteKind {
    SWC_SITE_HOLO_PANEL = 0, /* x,y,z centre; w by h panel; yaw is the facing direction */
    SWC_SITE_HOLO_OBJECT,    /* x,y,z centre; w radius */
    SWC_SITE_SEARCHLIGHT,    /* x,y,z emitter; w beam length; h number of beams (1..3) */
    SWC_SITE_PILLAR,         /* x,y,z base; w radius; h height */
    SWC_SITE_LASER,          /* x,y,z emitter; w beam length; h beams in the fan */
    SWC_SITE_ROOF_LIGHT,     /* x,y,z light position; w size */
    SWC_SITE_TICKER,         /* x,y,z centre of a rectangular ring; w and d half-extents; h band height */
    SWC_SITE_KIND_COUNT
} SwcSiteKind;

/* A typed anchor point. What w, h and d mean depends on the kind (see SwcSiteKind). */
typedef struct SwcSite {
    double x, y, z;
    double w, h, d;
    double yaw;    /* radians about +y. 0 faces -z; positive turns towards -x */
    double seed;   /* random 0..1, fixed per site */
    uint8_t kind;  /* SwcSiteKind */
    uint8_t color; /* neon palette index */
} SwcSite;

typedef enum SwcLandmarkKind { SWC_LM_MEGATOWER = 0, SWC_LM_PLAZA = 1, SWC_LM_DOWNTOWN = 2 } SwcLandmarkKind;

typedef struct SwcLandmark {
    double x, z;   /* centre, tile metres */
    double height; /* tallest structure there (0 for a plaza) */
    int cell_x, cell_z;
    uint8_t kind; /* SwcLandmarkKind */
} SwcLandmark;

typedef struct SwcCity {
    uint32_t seed;

    SwcCell cells[SWC_CELL_COUNT];
    float field[SWC_CELL_COUNT]; /* skyline field per cell, 0..1 (float, as in the web version) */
    int mega_cells[SWC_MAX_MEGA];
    int mega_count;
    int plaza_cells[SWC_MAX_PLAZAS];
    int plaza_count;
    int peak_cell; /* cell index of the highest point of the skyline field */

    SwcBox boxes[SWC_MAX_BOXES];
    int box_count;
    SwcBuilding buildings[SWC_MAX_BUILDINGS];
    int building_count;
    SwcSegment segments[SWC_MAX_MEGA][SWC_MAX_SEGMENTS];
    int mega_slots_used;

    SwcLane lanes[SWC_MAX_LANES];
    int lane_count;
    SwcSite sites[SWC_MAX_SITES];
    int site_count;
    SwcLandmark landmarks[SWC_MAX_LANDMARKS]; /* megatowers (tallest first), then plazas, then downtown */
    int landmark_count;

    /* Which boxes sit in which cell: the boxes of cell i are list[start[i] .. start[i + 1]). */
    int32_t box_start[SWC_CELL_COUNT + 1];
    int32_t box_list[SWC_MAX_BOXES];
} SwcCity;

/* Builds the whole plan from a seed. `c` is large (about 2.5 MB): keep it in static storage. */
void swc_city_build(SwcCity *c, uint32_t seed);

/* The buildable rectangle of a cell: the cell minus the road corridors on its four sides. */
typedef struct SwcRect {
    double x0, x1, z0, z1;
} SwcRect;
SwcRect swc_block_rect(int cx, int cz);

/* Top of the tallest box covering (x, z), or 0. Wrap-aware. */
double swc_city_height_at(const SwcCity *c, double x, double z);

/* True if the axis-aligned volume (centre x,y,z; half-extents hx,hy,hz) touches any box. */
bool swc_city_volume_blocked(const SwcCity *c, double x, double y, double z, double hx, double hy, double hz);

/* Pushes a sphere out of every box of its cell and keeps it above the road. Mutates pos; returns true if it
 * moved. radius is capped at 11 m (a sphere in one cell cannot reach a box in the next). */
bool swc_city_collide(const SwcCity *c, SwcVec3 *pos, double radius);

/* Lane helpers. */
double swc_lane_coordinate(int axis, int line, int dir, double offset);

#endif
