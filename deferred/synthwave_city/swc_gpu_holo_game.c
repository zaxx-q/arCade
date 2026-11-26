/* Synthwave City - the holograms pass: the web version's holograms layer. Floating ad panels with their
 * projector fans and lamps, wire solids with their light cones over the plazas, and ticker rings round the
 * megatowers. Three draws, all additive, depth-tested, no depth write, both faces drawn; the wet streets
 * mirror them.
 *
 * The ads are one 2048 x 2048 atlas drawn by code at start-up (swc_atlas.h). It holds three inks, not
 * colours, and the web version keeps it as an sRGB texture; here the texels are decoded to linear light on
 * the CPU and held as half floats, which filters the same way (rlgl has no sRGB format). The panel and ticker
 * shaders read it with explicit gradients, mip-mapped and anisotropic, repeating along S so that the ticker
 * strips wrap round a ring.
 *
 *   panels   one draw, instanced: three quads each (the image, the fan of light under it, the lamp)
 *   wire     one draw, not instanced: every solid's triangles in one mesh (edges, glow sprite, cone)
 *   tickers  one draw, instanced: four quads each (one per side of the ring) */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_atlas.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

#define FLOAT_BYTES ((int)sizeof(float))

typedef struct MeshGpu {
    unsigned vao, ibo;
    unsigned vbo[2];
    int vbo_count;
    int index_count;
    int vertex_count; /* for the draws that are not indexed */
    int instances;
} MeshGpu;

static SwcProg g_panel, g_wire, g_ticker;
static MeshGpu g_panel_mesh, g_wire_mesh, g_ticker_mesh;
/* The atlas is made in steps (a real window spreads them over frames, a harness run does them all at once).
 */
typedef enum {
    ATLAS_NONE,
    ATLAS_DRAWING,
    ATLAS_CONVERTING,
    ATLAS_UPLOADING,
    ATLAS_MIPPING,
    ATLAS_READY,
    ATLAS_FAILED
} AtlasStage;
static unsigned g_atlas;
static uint32_t g_atlas_seed; /* the seed the atlas is made for: a tier change keeps it */
static AtlasStage g_stage = ATLAS_NONE;
static SwcAtlasJob g_job;
static SwcCanvas *g_canvas; /* the finished drawing, while it is converted */
static uint16_t *g_texels;  /* the half-float texture, while it is filled */
static int g_rows_done;
static double g_budget_ms;   /* time per frame for the atlas's steps; 0: no limit */
static double g_atlas_began; /* GetTime() when the first step ran, for the log line */
static bool g_synced;
static uint32_t g_synced_seed;
static int g_synced_sites;
static SwcTier g_synced_tier;

static void free_mesh(MeshGpu *g) {
    if (g->vao != 0)
        rlUnloadVertexArray(g->vao);
    for (int i = 0; i < g->vbo_count; i++)
        rlUnloadVertexBuffer(g->vbo[i]);
    if (g->ibo != 0)
        rlUnloadVertexBuffer(g->ibo);
    memset(g, 0, sizeof(*g));
}

/* A buffer held by the mesh; the vertex array in use. */
static unsigned add_buffer(MeshGpu *g, const void *data, int bytes) {
    unsigned vbo = rlLoadVertexBuffer(data, bytes, false);
    if (g->vbo_count < 2)
        g->vbo[g->vbo_count++] = vbo;
    return vbo;
}

/* One attribute of an interleaved buffer: `comps` floats at float offset `at` of `stride` floats per record.
 * A name the compiler dropped is skipped. */
static void attribute(unsigned vbo, int loc, int comps, int stride, int at, bool per_instance) {
    if (loc < 0)
        return;
    rlEnableVertexBuffer(vbo);
    rlSetVertexAttribute((unsigned)loc, comps, RL_FLOAT, false, stride * FLOAT_BYTES, at * FLOAT_BYTES);
    rlEnableVertexAttribute((unsigned)loc);
    if (per_instance)
        rlSetVertexAttributeDivisor((unsigned)loc, 1);
}

static bool build_program(SwcProg *p, const char *label, const char *vert, const char *frag) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get(vert)};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get(frag)};
    return swc_prog_build(p, label, vs, 4, fs, 3);
}

bool swc_holograms_init(void) {
    g_synced = false;
    return build_program(&g_panel, "holo_panels", "holo_panel.vert", "holo_panel.frag") &&
           build_program(&g_wire, "holo_wire", "holo_wire.vert", "holo_wire.frag") &&
           build_program(&g_ticker, "holo_tickers", "holo_ticker.vert", "holo_ticker.frag");
}

static void free_meshes(void) {
    free_mesh(&g_panel_mesh);
    free_mesh(&g_wire_mesh);
    free_mesh(&g_ticker_mesh);
}

static void free_atlas(void) {
    if (g_atlas != 0)
        rlUnloadTexture(g_atlas);
    g_atlas = 0;
    swc_atlas_job_free(&g_job);
    if (g_canvas != NULL)
        swc_cv_destroy(g_canvas);
    g_canvas = NULL;
    free(g_texels);
    g_texels = NULL;
    g_rows_done = 0;
    g_stage = ATLAS_NONE;
}

void swc_holograms_set_budget(double ms) {
    g_budget_ms = ms > 0.0 ? ms : 0.0;
}

static void free_scene(void) {
    free_meshes();
    free_atlas();
}

void swc_holograms_free(void) {
    free_scene();
    swc_prog_free(&g_panel);
    swc_prog_free(&g_wire);
    swc_prog_free(&g_ticker);
    g_synced = false;
}

/* ---- the atlas texture ---- */

#define CONVERT_ROWS 128 /* rows of the atlas turned into half floats per step */

/* The texture's first image: the atlas as linear half floats, row 0 at the top of the picture (the shaders'
 * atlas rectangles count v from the bottom, like a canvas uploaded with flipY). */
static unsigned upload_texels(const uint16_t *texels, int w, int h) {
    return rlLoadTexture(texels, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
}

static void finish_texture(unsigned id, int w, int h) {
    int levels = 1;
    rlGenTextureMipmaps(id, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, &levels);
    rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
    rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_REPEAT);
    rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(id, RL_TEXTURE_FILTER_ANISOTROPIC, 16);
}

/* One step of making the atlas. */
static void atlas_step(uint32_t layer_seed) {
    switch (g_stage) {
    case ATLAS_NONE:
        g_atlas_began = GetTime();
        g_stage = swc_atlas_job_start(&g_job, layer_seed) ? ATLAS_DRAWING : ATLAS_FAILED;
        break;
    case ATLAS_DRAWING:
        if (!swc_atlas_job_next(&g_job)) { /* the last cell is drawn: on to the pixels */
            g_canvas = swc_atlas_job_take(&g_job);
            size_t n = (size_t)swc_cv_width(g_canvas) * (size_t)swc_cv_height(g_canvas) * 4u;
            g_texels = malloc(n * sizeof(uint16_t));
            g_rows_done = 0;
            g_stage = g_texels != NULL ? ATLAS_CONVERTING : ATLAS_FAILED;
        }
        break;
    case ATLAS_CONVERTING: {
        uint16_t lut[256];
        swc_atlas_srgb_lut(lut);
        swc_atlas_half_rows(g_canvas, lut, g_rows_done, CONVERT_ROWS, g_texels);
        g_rows_done += CONVERT_ROWS;
        if (g_rows_done >= swc_cv_height(g_canvas))
            g_stage = ATLAS_UPLOADING;
        break;
    }
    case ATLAS_UPLOADING:
        g_atlas = upload_texels(g_texels, swc_cv_width(g_canvas), swc_cv_height(g_canvas));
        free(g_texels);
        g_texels = NULL;
        g_stage = g_atlas != 0 ? ATLAS_MIPPING : ATLAS_FAILED;
        break;
    case ATLAS_MIPPING:
        finish_texture(g_atlas, swc_cv_width(g_canvas), swc_cv_height(g_canvas));
        swc_cv_destroy(g_canvas);
        g_canvas = NULL;
        g_stage = ATLAS_READY;
        TraceLog(LOG_INFO, "SWC: hologram atlas ready after %.0f ms", (GetTime() - g_atlas_began) * 1000.0);
        break;
    case ATLAS_READY:
    case ATLAS_FAILED:
        break;
    }
}

/* Works on the atlas until it is ready or this frame's budget is spent (at least one step per call). */
static void atlas_advance(uint32_t seed) {
    uint32_t layer_seed = swc_layer_seed(seed, "holograms");
    if (g_stage != ATLAS_NONE && g_atlas_seed != seed)
        free_atlas(); /* a new city has other ads */
    g_atlas_seed = seed;
    double t0 = GetTime();
    while (g_stage != ATLAS_READY && g_stage != ATLAS_FAILED) {
        atlas_step(layer_seed);
        if (g_budget_ms > 0.0 && (GetTime() - t0) * 1000.0 >= g_budget_ms)
            break;
    }
}

static bool atlas_ready(void) {
    return g_stage == ATLAS_READY && g_atlas != 0;
}

/* ---- the meshes ---- */

/* Three quads, each vertex (x, y, part): the image, the fan under it, the lamp. */
static void upload_panels(const SwcPanelPack *p) {
    float quad[3 * 4 * 3];
    uint16_t idx[3 * 6];
    for (int part = 0; part < 3; part++) {
        const float c[4][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        for (int k = 0; k < 4; k++) {
            quad[(part * 4 + k) * 3] = c[k][0];
            quad[(part * 4 + k) * 3 + 1] = c[k][1];
            quad[(part * 4 + k) * 3 + 2] = (float)part;
        }
        uint16_t o = (uint16_t)(part * 4);
        const uint16_t t[6] = {o, (uint16_t)(o + 1), (uint16_t)(o + 2),
                               o, (uint16_t)(o + 2), (uint16_t)(o + 3)};
        memcpy(&idx[part * 6], t, sizeof(t));
    }
    free_mesh(&g_panel_mesh);
    MeshGpu *g = &g_panel_mesh;
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    unsigned corners = add_buffer(g, quad, (int)sizeof(quad));
    attribute(corners, 0, 3, 3, 0, false);
    unsigned inst = add_buffer(g, p->data, p->count * SWC_PANEL_STRIDE * FLOAT_BYTES);
    unsigned prog = g_panel.id;
    attribute(inst, rlGetLocationAttrib(prog, "aPos"), 3, SWC_PANEL_STRIDE, SWC_PANEL_POS, true);
    attribute(inst, rlGetLocationAttrib(prog, "aSize"), 4, SWC_PANEL_STRIDE, SWC_PANEL_SIZE, true);
    attribute(inst, rlGetLocationAttrib(prog, "aLook"), 4, SWC_PANEL_STRIDE, SWC_PANEL_LOOK, true);
    attribute(inst, rlGetLocationAttrib(prog, "aFit"), 2, SWC_PANEL_STRIDE, SWC_PANEL_FIT, true);
    attribute(inst, rlGetLocationAttrib(prog, "aRectA"), 4, SWC_PANEL_STRIDE, SWC_PANEL_RECT_A, true);
    attribute(inst, rlGetLocationAttrib(prog, "aRectB"), 4, SWC_PANEL_STRIDE, SWC_PANEL_RECT_B, true);
    g->ibo = rlLoadVertexBufferElement(idx, (int)sizeof(idx), false);
    g->index_count = 3 * 6;
    g->instances = p->count;
    rlDisableVertexArray();
}

/* Four quads, each vertex (side, along the side 0..1, up the band 0..1). */
static void upload_tickers(const SwcTickerPack *p) {
    float quad[4 * 4 * 3];
    uint16_t idx[4 * 6];
    for (int side = 0; side < 4; side++) {
        const float c[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (int k = 0; k < 4; k++) {
            quad[(side * 4 + k) * 3] = (float)side;
            quad[(side * 4 + k) * 3 + 1] = c[k][0];
            quad[(side * 4 + k) * 3 + 2] = c[k][1];
        }
        uint16_t o = (uint16_t)(side * 4);
        const uint16_t t[6] = {o, (uint16_t)(o + 1), (uint16_t)(o + 2),
                               o, (uint16_t)(o + 2), (uint16_t)(o + 3)};
        memcpy(&idx[side * 6], t, sizeof(t));
    }
    free_mesh(&g_ticker_mesh);
    MeshGpu *g = &g_ticker_mesh;
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    unsigned corners = add_buffer(g, quad, (int)sizeof(quad));
    attribute(corners, 0, 3, 3, 0, false);
    unsigned inst = add_buffer(g, p->data, p->count * SWC_TICKER_STRIDE * FLOAT_BYTES);
    unsigned prog = g_ticker.id;
    attribute(inst, rlGetLocationAttrib(prog, "aPos"), 3, SWC_TICKER_STRIDE, SWC_TICKER_POS, true);
    attribute(inst, rlGetLocationAttrib(prog, "aExt"), 4, SWC_TICKER_STRIDE, SWC_TICKER_EXT, true);
    attribute(inst, rlGetLocationAttrib(prog, "aLook"), 4, SWC_TICKER_STRIDE, SWC_TICKER_LOOK, true);
    attribute(inst, rlGetLocationAttrib(prog, "aRect"), 4, SWC_TICKER_STRIDE, SWC_TICKER_RECT, true);
    g->ibo = rlLoadVertexBufferElement(idx, (int)sizeof(idx), false);
    g->index_count = 4 * 6;
    g->instances = p->count;
    rlDisableVertexArray();
}

/* Every solid's triangles, unshared, one interleaved buffer. */
static void upload_wire(const SwcWireMesh *m) {
    free_mesh(&g_wire_mesh);
    MeshGpu *g = &g_wire_mesh;
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    unsigned vbo = add_buffer(g, m->data, m->vertex_count * SWC_WIRE_STRIDE * FLOAT_BYTES);
    unsigned prog = g_wire.id;
    attribute(vbo, 0, 3, SWC_WIRE_STRIDE, SWC_WIRE_POSITION, false);
    attribute(vbo, rlGetLocationAttrib(prog, "aBary"), 3, SWC_WIRE_STRIDE, SWC_WIRE_BARY, false);
    attribute(vbo, rlGetLocationAttrib(prog, "aAnchor"), 3, SWC_WIRE_STRIDE, SWC_WIRE_ANCHOR, false);
    attribute(vbo, rlGetLocationAttrib(prog, "aSpin"), 4, SWC_WIRE_STRIDE, SWC_WIRE_SPIN, false);
    attribute(vbo, rlGetLocationAttrib(prog, "aLook"), 4, SWC_WIRE_STRIDE, SWC_WIRE_LOOK, false);
    attribute(vbo, rlGetLocationAttrib(prog, "aStyle"), 4, SWC_WIRE_STRIDE, SWC_WIRE_STYLE, false);
    g->vertex_count = m->vertex_count;
    rlDisableVertexArray();
}

void swc_holograms_sync(const SwcCity *city, const SwcQuality *q) {
    if (g_panel.id == 0 || g_wire.id == 0 || g_ticker.id == 0)
        return;
    /* The ads do not depend on the tier: made once per seed and kept. The panels, rings and solids below do
     * not need the picture to be packed, so they go up at once and the ads fade in when the atlas is ready.
     */
    atlas_advance(city->seed);
    if (g_synced && g_synced_seed == city->seed && g_synced_sites == city->site_count &&
        g_synced_tier == q->tier)
        return;
    free_meshes();
    g_synced = true; /* even if something below fails: do not retry every frame */
    g_synced_seed = city->seed;
    g_synced_sites = city->site_count;
    g_synced_tier = q->tier;

    const double density = q->density;
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    swc_atlas_layout(cells);

    SwcPanelPack panels = {0};
    if (swc_holo_pack_panels(city, cells, density, &panels) && panels.count > 0)
        upload_panels(&panels);
    swc_holo_free_panels(&panels);

    SwcTickerPack tickers = {0};
    if (swc_holo_pack_tickers(city, cells, density, &tickers) && tickers.count > 0)
        upload_tickers(&tickers);
    swc_holo_free_tickers(&tickers);

    const SwcSite *picked[SWC_MAX_SITES];
    int n = swc_holo_pick_objects(city, density, picked, SWC_MAX_SITES);
    SwcWireMesh wire = {0};
    if (n > 0 && swc_holo_build_wire(picked, n, &wire) && wire.vertex_count > 0)
        upload_wire(&wire);
    swc_holo_free_wire(&wire);
}

/* ---- drawing ---- */

static void draw_instanced(SwcProg *prog, const MeshGpu *g, const SwcFrameUniforms *f, const SwcLook *look) {
    if (g->vao == 0)
        return;
    swc_prog_use(prog);
    swc_set_shared_uniforms(prog, f, look);
    swc_utex(prog, "uAtlas", 2, g_atlas);
    rlEnableVertexArray(g->vao);
    if (g->instances > 0)
        rlDrawVertexArrayElementsInstanced(0, g->index_count, NULL, g->instances);
    else
        rlDrawVertexArray(0, g->vertex_count);
    rlDisableVertexArray();
}

void swc_holograms_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_panel.id == 0 || !atlas_ready())
        return;
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    /* Add colour, leave the alpha of the frame alone: One/One for rgb, Zero/One for alpha. */
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling(); /* a panel and a solid are seen from both sides */
    draw_instanced(&g_panel, &g_panel_mesh, f, look);
    draw_instanced(&g_wire, &g_wire_mesh, f, look);
    draw_instanced(&g_ticker, &g_ticker_mesh, f, look);
    rlDisableShader();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
