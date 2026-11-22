/* Synthwave City - the holograms layer's data: where the ads sit in the atlas, which ad each floating panel
 * shows, the ticker rings and the wire solids with their projector cones. PURE C99, no raylib.
 * Ported from the web version (src/layers/holograms/layout.ts, pack.ts and wire.ts).
 *
 * Everything is packed once per plan into flat float arrays that the GPU reads as instance or vertex
 * attributes; the shaders do the animation (the ad switching, the ticker scroll, the spin). */
#ifndef SWC_HOLO_H
#define SWC_HOLO_H

#include <stdbool.h>
#include <stdint.h>

#include "swc_city.h"

/* ---- the ad atlas layout: a 2048 x 2048 canvas ----
 *   y    0..1024   16 wide ads, 512 x 256 (2:1): roof billboards, avenue panels, megatower panels
 *   y 1024..1536    8 tall ads, 256 x 512 (1:2): banners across streets
 *   y 1536..2048    4 ticker strips, 2048 x 128 (16:1): the full width, so they wrap round a ring */
#define SWC_ATLAS_SIZE 2048
#define SWC_ATLAS_WIDE_COUNT 16
#define SWC_ATLAS_TALL_COUNT 8
#define SWC_ATLAS_STRIP_COUNT 4
#define SWC_ATLAS_CELL_COUNT (SWC_ATLAS_WIDE_COUNT + SWC_ATLAS_TALL_COUNT + SWC_ATLAS_STRIP_COUNT)
/* Pixels of every cell's edge that the art leaves black, so mip levels do not bleed between ads. */
#define SWC_ATLAS_CELL_PAD 10

typedef enum SwcCellKind { SWC_CELL_WIDE = 0, SWC_CELL_TALL, SWC_CELL_STRIP } SwcCellKind;

typedef struct SwcAtlasCell {
    uint8_t kind;   /* SwcCellKind */
    int index;      /* position among the cells of its kind */
    int x, y, w, h; /* top-left corner and size in canvas pixels, y down */
} SwcAtlasCell;

/* Fills SWC_ATLAS_CELL_COUNT cells (wide, then tall, then strips) and returns how many. */
int swc_atlas_layout(SwcAtlasCell out[SWC_ATLAS_CELL_COUNT]);
/* The cells of one kind, in order; returns how many. */
int swc_atlas_cells_of_kind(const SwcAtlasCell *cells, int kind, const SwcAtlasCell **out, int cap);
/* A cell as a texture rectangle (u0, v0, du, dv) for a canvas uploaded with the first row at the top
 * (v = 0 at the bottom of the canvas), pulled in by `inset` pixels on every side. */
void swc_atlas_cell_rect(const SwcAtlasCell *cell, int inset, float out[4]);

/* ---- palette ---- */
/* A second ink for each palette index (pick 0..2), chosen so the pair reads as a designed duotone. */
int swc_holo_partner_color(int color, int pick);

/* ---- panels ---- */
/* Floats per panel in SwcPanelPack.data and where each field starts. */
#define SWC_PANEL_STRIDE 21
#define SWC_PANEL_POS 0  /* 3: centre x, y, z */
#define SWC_PANEL_SIZE 3 /* 4: width, height, yaw, seed */
#define SWC_PANEL_LOOK                                                                                       \
    7 /* 4: ink A, ink B palette index, wipe laps per loop (0: never switches), wipe style 0..3 */
#define SWC_PANEL_FIT 11    /* 2: scale from panel uv to ad uv about the centre */
#define SWC_PANEL_RECT_A 13 /* 4: atlas rectangle of the first ad */
#define SWC_PANEL_RECT_B 17 /* 4: atlas rectangle of the second ad */

/* Panels this wide are landmarks and survive every density. */
#define SWC_HOLO_ALWAYS_KEEP_WIDTH 80.0

typedef struct SwcPanelPack {
    int count;
    float *data; /* count * SWC_PANEL_STRIDE */
    int *cell_a; /* index into the cells of the panel's class (wide or tall): first and second ad */
    int *cell_b;
} SwcPanelPack;

/* Scale from panel uv to ad uv: the ad keeps its shape (within a 12% stretch) and is centred. */
void swc_holo_content_fit(double panel_aspect, double cell_aspect, double out[2]);
bool swc_holo_keep_site(int index, const SwcSite *site, double density);

/* Returns false when out of memory. Free with swc_holo_free_panels. */
bool swc_holo_pack_panels(const SwcCity *city, const SwcAtlasCell *cells, double density, SwcPanelPack *out);
void swc_holo_free_panels(SwcPanelPack *p);

/* ---- ticker rings ---- */
#define SWC_TICKER_STRIDE 15
#define SWC_TICKER_POS 0   /* 3: centre x, y, z */
#define SWC_TICKER_EXT 3   /* 4: half-extent x, half-extent z, band height, seed */
#define SWC_TICKER_LOOK 7  /* 4: ink A, ink B, strip repeats round the ring, laps per loop */
#define SWC_TICKER_RECT 11 /* 4: atlas rectangle of the strip (full width; v pulled in) */

typedef struct SwcTickerPack {
    int count;
    float *data; /* count * SWC_TICKER_STRIDE */
} SwcTickerPack;

bool swc_holo_pack_tickers(const SwcCity *city, const SwcAtlasCell *cells, double density,
                           SwcTickerPack *out);
void swc_holo_free_tickers(SwcTickerPack *p);

/* ---- wire solids ---- */
#define SWC_WIRE_KIND_EDGES 0
#define SWC_WIRE_KIND_GLOW 1
#define SWC_WIRE_KIND_CONE 2
#define SWC_WIRE_SHAPE_COUNT 6 /* icosphere, torus, globe, pyramid, rings, gyro */

#define SWC_WIRE_STRIDE 21
#define SWC_WIRE_POSITION 0 /* 3: metres from the anchor before rotation; glow: corner x, y and half-size */
#define SWC_WIRE_BARY 3     /* 3: edge coordinates; cone: x is 0 at the ground and 1 at the top */
#define SWC_WIRE_ANCHOR 6   /* 3: the site position */
#define SWC_WIRE_SPIN 9     /* 4: spin laps per loop, phase 0..1, tilt in radians, precession laps per loop */
#define SWC_WIRE_LOOK 13    /* 4: palette index, brightness, kind, seed */
#define SWC_WIRE_STYLE 17   /* 4: line half-width in metres, face fill, object radius, bob weight */

typedef struct SwcWireMesh {
    int vertex_count;
    float *data; /* vertex_count * SWC_WIRE_STRIDE; triangles are unshared */
    int object_count;
    int *shapes; /* the shape chosen for each object, in order */
} SwcWireMesh;

/* The holoObject sites that survive the density, in plan order; returns how many (writes at most `cap`). */
int swc_holo_pick_objects(const SwcCity *city, double density, const SwcSite **out, int cap);
bool swc_holo_build_wire(const SwcSite *const *objects, int count, SwcWireMesh *out);
void swc_holo_free_wire(SwcWireMesh *m);

#endif
