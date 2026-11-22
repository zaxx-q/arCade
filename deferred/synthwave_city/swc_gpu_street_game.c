/* Synthwave City - the street life pass: the web version's streetlife layer. Cars, vans and buses on every
 * street and avenue, and the people on the sidewalks.
 *
 * Nothing is stored per car. The lane table (a 40 x 12 float texture) says how many cars each lane holds, how
 * fast they go and where they started; the vertex shaders work out which car each instance of a fixed grid is
 * from the camera position and the loop phase (see swc_street.h). Past the handover distance the same
 * arithmetic runs per pixel on one quad per road and every car becomes a pair of dashes.
 *
 *   bodies  (opaque, mirrored)      one draw: the grid of near cars, each the 34-vertex vehicle mesh
 *   crowds  (opaque)                one draw: a camera-facing impostor for every person
 *   lamps   (additive, mirrored)    one draw: two headlights, two tail lights and a roof beacon per car
 *   lights  (additive)              light on the road (pools, smears, underglow), then the far traffic
 *
 * The additive quads that lie on the road are coplanar with the ground; the web version pulls them towards
 * the camera with a polygon offset (factor -2, units -8). rlgl has none, so the vertex shaders pull the clip
 * z. */
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"
#include "swc_shaders.h"
#include "swc_street.h"

#define CAR_FIELD_MAX 12000
#define ROAD_QUADS (SWC_CELLS * 2)

typedef struct MeshGpu {
    unsigned vao, ibo;
    unsigned vbo[8];
    int vbo_count;
    int index_count;
    int instances;
} MeshGpu;

static SwcProg g_car, g_lamp, g_pool, g_far, g_walker;
static MeshGpu g_body_mesh, g_lamp_mesh, g_pool_mesh, g_far_mesh, g_crowd_mesh;
static unsigned g_slot_vbo; /* the grid of near cars, shared by the bodies, the lamps and the pools */
static unsigned g_lane_tex; /* the lane table */
static unsigned g_cell_tex; /* the plan's cells, for the block colours */
static float g_handover[2]; /* uHandover */
static float g_crowd[4];    /* uCrowd */

static SwcLaneTable g_table; /* 480 records and 2 KB of texels: static */
static float g_slots[CAR_FIELD_MAX * 4];
static float g_walkers[8200 * 4];

static bool g_synced;
static uint32_t g_synced_seed;
static int g_synced_lanes;

static void add_attribute(MeshGpu *g, int loc, int comps, const void *data, int bytes) {
    unsigned vbo = rlLoadVertexBuffer(data, bytes, false);
    if (g->vbo_count < 8)
        g->vbo[g->vbo_count++] = vbo;
    if (loc < 0) /* the compiler dropped an attribute the shader does not use */
        return;
    rlSetVertexAttribute((unsigned)loc, comps, RL_FLOAT, false, comps * (int)sizeof(float), 0);
    rlEnableVertexAttribute((unsigned)loc);
}

/* A per-instance attribute of `comps` floats read from a buffer that is not owned by the mesh. */
static void add_instances(unsigned prog, unsigned shared, const char *name, int comps) {
    int loc = rlGetLocationAttrib(prog, name);
    if (loc < 0)
        return;
    rlEnableVertexBuffer(shared);
    rlSetVertexAttribute((unsigned)loc, comps, RL_FLOAT, false, comps * (int)sizeof(float), 0);
    rlEnableVertexAttribute((unsigned)loc);
    rlSetVertexAttributeDivisor((unsigned)loc, 1);
}

static void free_mesh(MeshGpu *g) {
    if (g->vao != 0)
        rlUnloadVertexArray(g->vao);
    for (int i = 0; i < g->vbo_count; i++)
        rlUnloadVertexBuffer(g->vbo[i]);
    if (g->ibo != 0)
        rlUnloadVertexBuffer(g->ibo);
    memset(g, 0, sizeof(*g));
}

/* `count` quads in one mesh; each vertex is (x, y, quad number). */
static void upload_quads(MeshGpu *g, int count, float x0, float x1, float y0, float y1) {
    float v[5 * 4 * 3];
    uint16_t idx[5 * 6];
    for (int i = 0; i < count; i++) {
        const float c[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
        for (int k = 0; k < 4; k++) {
            v[(i * 4 + k) * 3] = c[k][0];
            v[(i * 4 + k) * 3 + 1] = c[k][1];
            v[(i * 4 + k) * 3 + 2] = (float)i;
        }
        uint16_t a = (uint16_t)(i * 4);
        const uint16_t t[6] = {
            a, (uint16_t)(a + 1), (uint16_t)(a + 2), (uint16_t)(a + 2), (uint16_t)(a + 1), (uint16_t)(a + 3)};
        memcpy(&idx[i * 6], t, sizeof(t));
    }
    add_attribute(g, 0, 3, v, count * 4 * 3 * (int)sizeof(float));
    g->ibo = rlLoadVertexBufferElement(idx, count * 6 * (int)sizeof(uint16_t), false);
    g->index_count = count * 6;
}

static bool build_program(SwcProg *p, const char *label, const char *vert, const char *frag) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get(vert)};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get(frag)};
    return swc_prog_build(p, label, vs, 4, fs, 3);
}

bool swc_streetlife_init(void) {
    g_synced = false;
    g_handover[1] = (float)SWC_STREET_HANDOVER_END; /* quality "high": min(880, fadeStart * 0.8) */
    g_handover[0] = (float)(SWC_STREET_HANDOVER_END * (SWC_STREET_HANDOVER_START / SWC_STREET_HANDOVER_END));
    g_crowd[0] = (float)SWC_CROWD_START;
    g_crowd[1] = (float)SWC_CROWD_END;
    g_crowd[2] = (float)SWC_CROWD_PER_FACE; /* high: round(64 * density 1) */
    g_crowd[3] = 0.0f;
    return build_program(&g_car, "street_cars", "sl_car.vert", "sl_car.frag") &&
           build_program(&g_lamp, "street_lamps", "sl_lamp.vert", "sl_lamp.frag") &&
           build_program(&g_pool, "street_pools", "sl_pool.vert", "sl_pool.frag") &&
           build_program(&g_far, "street_far", "sl_far.vert", "sl_far.frag") &&
           build_program(&g_walker, "street_walkers", "sl_walker.vert", "sl_walker.frag");
}

static void free_scene(void) {
    free_mesh(&g_body_mesh);
    free_mesh(&g_lamp_mesh);
    free_mesh(&g_pool_mesh);
    free_mesh(&g_far_mesh);
    free_mesh(&g_crowd_mesh);
    if (g_slot_vbo != 0)
        rlUnloadVertexBuffer(g_slot_vbo);
    if (g_lane_tex != 0)
        rlUnloadTexture(g_lane_tex);
    if (g_cell_tex != 0)
        rlUnloadTexture(g_cell_tex);
    g_slot_vbo = g_lane_tex = g_cell_tex = 0;
}

void swc_streetlife_free(void) {
    free_scene();
    swc_prog_free(&g_car);
    swc_prog_free(&g_lamp);
    swc_prog_free(&g_pool);
    swc_prog_free(&g_far);
    swc_prog_free(&g_walker);
    g_synced = false;
}

static unsigned make_texture(const void *data, int w, int h, int format) {
    unsigned id = rlLoadTexture(data, w, h, format, 1);
    if (id != 0) {
        rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
        rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
        rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    }
    return id;
}

static void upload_bodies(void) {
    static SwcCarMesh car;
    swc_street_build_car_mesh(&car);
    free_mesh(&g_body_mesh);
    g_body_mesh.vao = rlLoadVertexArray();
    rlEnableVertexArray(g_body_mesh.vao);
    add_attribute(&g_body_mesh, 0, 3, car.position, car.vertex_count * 3 * (int)sizeof(float));
    add_attribute(&g_body_mesh, rlGetLocationAttrib(g_car.id, "aAlt"), 3, car.alt,
                  car.vertex_count * 3 * (int)sizeof(float));
    add_attribute(&g_body_mesh, rlGetLocationAttrib(g_car.id, "aPart"), 1, car.part,
                  car.vertex_count * (int)sizeof(float));
    add_instances(g_car.id, g_slot_vbo, "aSlot", 4);
    g_body_mesh.ibo = rlLoadVertexBufferElement(car.index, car.index_count * (int)sizeof(uint16_t), false);
    g_body_mesh.index_count = car.index_count;
}

static void upload_quad_mesh(MeshGpu *g, unsigned prog, int quads) {
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    upload_quads(g, quads, -1.0f, 1.0f, -1.0f, 1.0f);
    add_instances(prog, g_slot_vbo, "aSlot", 4);
    rlDisableVertexArray();
}

void swc_streetlife_sync(const SwcCity *city) {
    if (g_car.id == 0 || g_lamp.id == 0 || g_pool.id == 0 || g_far.id == 0 || g_walker.id == 0 ||
        (g_synced && g_synced_seed == city->seed && g_synced_lanes == city->lane_count))
        return;
    free_scene();
    /* Full density: quality tiers come later, and the web version's `high` tier is 1. */
    swc_street_build_lanes(city, swc_street_traffic_density(1.0), swc_layer_seed(city->seed, "streetlife"),
                           &g_table);
    g_lane_tex =
        make_texture(g_table.data, SWC_CELLS, SWC_STREET_LANE_ROWS, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32);
    g_cell_tex = make_texture(city->cells, SWC_CELLS, SWC_CELLS, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int cars = swc_street_build_car_field(&g_table, g_handover[1], g_slots, CAR_FIELD_MAX);
    if (g_lane_tex != 0 && g_cell_tex != 0 && cars > 0 && g_table.record_count > 0) {
        g_slot_vbo = rlLoadVertexBuffer(g_slots, cars * 4 * (int)sizeof(float), false);
        upload_bodies();
        g_body_mesh.instances = cars;
        rlDisableVertexArray();
        upload_quad_mesh(&g_lamp_mesh, g_lamp.id, 5); /* two headlights, two tail lights, one beacon */
        g_lamp_mesh.instances = cars;
        upload_quad_mesh(&g_pool_mesh, g_pool.id, 3); /* headlight pool, tail smear, underglow */
        g_pool_mesh.instances = cars;

        /* Far traffic: one quad per road, (axis, grid line). */
        float roads[ROAD_QUADS * 2];
        for (int i = 0; i < ROAD_QUADS; i++) {
            roads[i * 2] = i < SWC_CELLS ? 0.0f : 1.0f;
            roads[i * 2 + 1] = (float)(i % SWC_CELLS);
        }
        free_mesh(&g_far_mesh);
        g_far_mesh.vao = rlLoadVertexArray();
        rlEnableVertexArray(g_far_mesh.vao);
        upload_quads(&g_far_mesh, 1, -1.0f, 1.0f, -1.0f, 1.0f);
        add_attribute(&g_far_mesh, rlGetLocationAttrib(g_far.id, "aRoad"), 2, roads, (int)sizeof(roads));
        {
            int loc = rlGetLocationAttrib(g_far.id, "aRoad");
            if (loc >= 0)
                rlSetVertexAttributeDivisor((unsigned)loc, 1);
        }
        g_far_mesh.instances = ROAD_QUADS;
        rlDisableVertexArray();
    }
    /* Crowds: an upright impostor per person. */
    int walkers = swc_street_build_crowd_field(SWC_CROWD_PER_FACE, g_walkers, 8200);
    if (walkers > 0) {
        free_mesh(&g_crowd_mesh);
        g_crowd_mesh.vao = rlLoadVertexArray();
        rlEnableVertexArray(g_crowd_mesh.vao);
        upload_quads(&g_crowd_mesh, 1, -1.0f, 1.0f, 0.0f, 1.0f);
        add_attribute(&g_crowd_mesh, rlGetLocationAttrib(g_walker.id, "aWalker"), 4, g_walkers,
                      walkers * 4 * (int)sizeof(float));
        {
            int loc = rlGetLocationAttrib(g_walker.id, "aWalker");
            if (loc >= 0)
                rlSetVertexAttributeDivisor((unsigned)loc, 1);
        }
        g_crowd_mesh.instances = walkers;
        rlDisableVertexArray();
    }
    g_synced = true;
    g_synced_seed = city->seed;
    g_synced_lanes = city->lane_count;
}

static void draw_mesh(const MeshGpu *g) {
    if (g->instances <= 0 || g->vao == 0)
        return;
    rlEnableVertexArray(g->vao);
    rlDrawVertexArrayElementsInstanced(0, g->index_count, NULL, g->instances);
    rlDisableVertexArray();
}

/* Everything the street shaders read besides the shared uniforms. */
static void use_program(SwcProg *p, const SwcFrameUniforms *f, const SwcLook *look) {
    swc_prog_use(p);
    swc_set_shared_uniforms(p, f, look);
    swc_utex(p, "uCellTex", 0, g_cell_tex);
    swc_utex(p, "uLaneTex", 1, g_lane_tex);
    swc_u2f(p, "uHandover", g_handover[0], g_handover[1]);
    swc_u2f(p, "uLamp", 2.4f, 0.45f);
    {
        const float dash[4] = {0.9f, 0.29f, 0.14f, 0.6f};
        swc_u4f(p, "uDash", dash);
    }
    swc_u4f(p, "uCrowd", g_crowd);
}

static void opaque_begin(void) {
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

static void additive_begin(bool cull) {
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    /* Add colour, leave the alpha of the frame alone: One/One for rgb, Zero/One for alpha. */
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    if (cull)
        rlEnableBackfaceCulling();
    else
        rlDisableBackfaceCulling();
}

static void state_end(void) {
    rlDisableShader();
    rlEnableDepthMask();
    rlDisableBackfaceCulling(); /* the passes that follow draw both sides */
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void swc_streetlife_bodies_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced || g_body_mesh.instances <= 0)
        return;
    opaque_begin();
    use_program(&g_car, f, look);
    draw_mesh(&g_body_mesh);
    state_end();
    rlEnableColorBlend();
}

void swc_streetlife_crowds_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced || g_crowd_mesh.instances <= 0)
        return;
    opaque_begin();
    use_program(&g_walker, f, look);
    draw_mesh(&g_crowd_mesh);
    state_end();
    rlEnableColorBlend();
}

void swc_streetlife_lamps_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced || g_lamp_mesh.instances <= 0)
        return;
    additive_begin(true);
    use_program(&g_lamp, f, look);
    draw_mesh(&g_lamp_mesh);
    state_end();
}

void swc_streetlife_lights_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced)
        return;
    additive_begin(false); /* the quads on the road are wound for one travel direction only */
    if (g_pool_mesh.instances > 0) {
        use_program(&g_pool, f, look);
        draw_mesh(&g_pool_mesh);
    }
    if (g_far_mesh.instances > 0) {
        use_program(&g_far, f, look);
        draw_mesh(&g_far_mesh);
    }
    state_end();
}
