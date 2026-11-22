/* Synthwave City - the air traffic pass: the web version's airtraffic layer. Thousands of small craft (cars,
 * haulers, drones) in the sky lanes and six hero ships (freighters, airships, patrol craft). Where a craft is
 * at any moment is a closed-form function of the loop phase in the vertex shader, so nothing is animated
 * here: the plan is packed and uploaded once and every frame is a handful of instanced draws.
 *
 *   hulls   (opaque)     3 draws, one per craft shape, instanced over that shape's craft, and 1 for the hero
 * ships lights  (additive)   1 draw for every craft's engine glows, lamps, strobe and trail, 1 for the hero
 * lights
 *
 * The 16 floats of a craft are one interleaved instance record: the three shape draws and the light draw read
 * the same buffer, each from its own offset. */
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_air.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

#define HULL_DRAW_DISTANCE 2000.0f /* the web version's HULL_DRAW_DISTANCE: farther hulls are dropped */
#define TRAIL_ROWS 4

typedef struct MeshGpu {
    unsigned vao, ibo;
    unsigned vbo[8];
    int vbo_count;
    int index_count;
    int instances;
} MeshGpu;

static SwcProg g_hull, g_craft_light, g_hero, g_hero_light;
static MeshGpu g_hull_mesh[SWC_CRAFT_SHAPES], g_light_mesh, g_hero_mesh, g_hero_light_mesh;
static unsigned g_craft_vbo, g_hero_vbo; /* the interleaved instance records */

static SwcCraftPack g_craft; /* about 790 KB: static */
static SwcHeroPack g_heroes;
static SwcHull g_hull_cpu;
static SwcHeroLight g_hero_lights_cpu[SWC_HERO_LIGHT_MAX];

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

/* Four vec4 (or two) instance attributes read from `shared`, a buffer of `stride` floats per instance,
 * starting `first` instances in. */
static void add_instances(unsigned prog, unsigned shared, const char *const *names, int n, int stride,
                          int first) {
    rlEnableVertexBuffer(shared);
    for (int k = 0; k < n; k++) {
        int loc = rlGetLocationAttrib(prog, names[k]);
        if (loc < 0)
            continue;
        rlSetVertexAttribute((unsigned)loc, 4, RL_FLOAT, false, stride * (int)sizeof(float),
                             (first * stride + k * 4) * (int)sizeof(float));
        rlEnableVertexAttribute((unsigned)loc);
        rlSetVertexAttributeDivisor((unsigned)loc, 1);
    }
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

static const char *const CRAFT_NAMES[4] = {"aLane", "aMove", "aBob", "aLook"};
static const char *const HERO_NAMES[2] = {"aRoute", "aRun"};

/* A hull: positions at location 0, the part id (and, for the heroes, the type) per vertex. */
static void upload_hull(MeshGpu *g, unsigned prog, const SwcHull *h, bool with_type, unsigned shared,
                        const char *const *names, int name_count, int stride, int first, int instances) {
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, 0, 3, h->position, h->vertex_count * 3 * (int)sizeof(float));
    add_attribute(g, rlGetLocationAttrib(prog, "aPart"), 1, h->part, h->vertex_count * (int)sizeof(float));
    if (with_type)
        add_attribute(g, rlGetLocationAttrib(prog, "aType"), 1, h->type,
                      h->vertex_count * (int)sizeof(float));
    add_instances(prog, shared, names, name_count, stride, first);
    g->ibo = rlLoadVertexBufferElement(h->index, h->index_count * (int)sizeof(uint16_t), false);
    g->index_count = h->index_count;
    g->instances = instances;
    rlDisableVertexArray();
}

/* Per craft: SWC_CRAFT_LIGHTS sprite quads (aVert = corner x, corner y, light index), then one trail ribbon
 * (side, where along it, SWC_CRAFT_LIGHTS), with TRAIL_ROWS cross-sections. */
static void upload_craft_lights(MeshGpu *g, unsigned prog, int count) {
    static const float rows[TRAIL_ROWS] = {0.0f, 0.14f, 0.48f, 1.0f};
    float verts[(SWC_CRAFT_LIGHTS * 4 + TRAIL_ROWS * 2) * 3];
    uint16_t index[SWC_CRAFT_LIGHTS * 6 + (TRAIL_ROWS - 1) * 6];
    int nv = 0, ni = 0;
    for (int k = 0; k < SWC_CRAFT_LIGHTS; k++) {
        int base = nv / 3;
        static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (int c = 0; c < 4; c++) {
            verts[nv++] = corner[c][0];
            verts[nv++] = corner[c][1];
            verts[nv++] = (float)k;
        }
        static const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int i = 0; i < 6; i++)
            index[ni++] = (uint16_t)(base + order[i]);
    }
    int first = nv / 3;
    for (int r = 0; r < TRAIL_ROWS; r++) {
        verts[nv++] = -1.0f;
        verts[nv++] = rows[r];
        verts[nv++] = (float)SWC_CRAFT_LIGHTS;
        verts[nv++] = 1.0f;
        verts[nv++] = rows[r];
        verts[nv++] = (float)SWC_CRAFT_LIGHTS;
        if (r == 0)
            continue;
        int a = first + (r - 1) * 2;
        index[ni++] = (uint16_t)a;
        index[ni++] = (uint16_t)(a + 1);
        index[ni++] = (uint16_t)(a + 3);
        index[ni++] = (uint16_t)a;
        index[ni++] = (uint16_t)(a + 3);
        index[ni++] = (uint16_t)(a + 2);
    }
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    /* the quad corners are the shader's `aVert`; the prelude only fixes `position` at location 0 */
    add_attribute(g, rlGetLocationAttrib(prog, "aVert"), 3, verts, nv * (int)sizeof(float));
    add_instances(prog, g_craft_vbo, CRAFT_NAMES, 4, SWC_CRAFT_STRIDE, 0);
    g->ibo = rlLoadVertexBufferElement(index, ni * (int)sizeof(uint16_t), false);
    g->index_count = ni;
    g->instances = count;
    rlDisableVertexArray();
}

/* One quad per light of every hero type; the shader keeps a ship's own. Sprites and the pool span -1..1 both
 * ways, ribbons (the searchlight cone and the exhaust plumes) -1..1 across and 0..1 along. */
static void upload_hero_lights(MeshGpu *g, unsigned prog, int count) {
    int n = swc_air_hero_lights(g_hero_lights_cpu);
    static float local[SWC_HERO_LIGHT_MAX * 4 * 3], spec[SWC_HERO_LIGHT_MAX * 4 * 4],
        quad[SWC_HERO_LIGHT_MAX * 4 * 3];
    static uint16_t index[SWC_HERO_LIGHT_MAX * 6];
    for (int i = 0; i < n; i++) {
        const SwcHeroLight *l = &g_hero_lights_cpu[i];
        bool ribbon = l->mode == SWC_GLOW_CONE || l->mode == SWC_GLOW_PLUME;
        float y0 = ribbon ? 0.0f : -1.0f;
        float corners[8] = {-1.0f, y0, 1.0f, y0, 1.0f, 1.0f, -1.0f, 1.0f};
        for (int v = 0; v < 4; v++) {
            int o = i * 4 + v;
            local[o * 3] = l->x;
            local[o * 3 + 1] = l->y;
            local[o * 3 + 2] = l->z;
            spec[o * 4] = l->size;
            spec[o * 4 + 1] = l->color;
            spec[o * 4 + 2] = (float)l->mode;
            spec[o * 4 + 3] = l->phase;
            quad[o * 3] = corners[v * 2];
            quad[o * 3 + 1] = corners[v * 2 + 1];
            quad[o * 3 + 2] = (float)l->type;
        }
        static const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; k++)
            index[i * 6 + k] = (uint16_t)(i * 4 + order[k]);
    }
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, rlGetLocationAttrib(prog, "aLocal"), 3, local, n * 4 * 3 * (int)sizeof(float));
    add_attribute(g, rlGetLocationAttrib(prog, "aSpec"), 4, spec, n * 4 * 4 * (int)sizeof(float));
    add_attribute(g, rlGetLocationAttrib(prog, "aQuad"), 3, quad, n * 4 * 3 * (int)sizeof(float));
    add_instances(prog, g_hero_vbo, HERO_NAMES, 2, SWC_HERO_STRIDE, 0);
    g->ibo = rlLoadVertexBufferElement(index, n * 6 * (int)sizeof(uint16_t), false);
    g->index_count = n * 6;
    g->instances = count;
    rlDisableVertexArray();
}

static bool build_program(SwcProg *p, const char *label, const char *vert, const char *frag) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get(vert)};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get(frag)};
    return swc_prog_build(p, label, vs, 4, fs, 3);
}

bool swc_airtraffic_init(void) {
    g_synced = false;
    return build_program(&g_hull, "air_hull", "hull.vert", "hull.frag") &&
           build_program(&g_craft_light, "air_craft_lights", "craft_light.vert", "air_light.frag") &&
           build_program(&g_hero, "air_hero", "hero.vert", "hero.frag") &&
           build_program(&g_hero_light, "air_hero_lights", "hero_light.vert", "air_light.frag");
}

static void free_scene(void) {
    for (int i = 0; i < SWC_CRAFT_SHAPES; i++)
        free_mesh(&g_hull_mesh[i]);
    free_mesh(&g_light_mesh);
    free_mesh(&g_hero_mesh);
    free_mesh(&g_hero_light_mesh);
    if (g_craft_vbo != 0)
        rlUnloadVertexBuffer(g_craft_vbo);
    if (g_hero_vbo != 0)
        rlUnloadVertexBuffer(g_hero_vbo);
    g_craft_vbo = g_hero_vbo = 0;
}

void swc_airtraffic_free(void) {
    free_scene();
    swc_prog_free(&g_hull);
    swc_prog_free(&g_craft_light);
    swc_prog_free(&g_hero);
    swc_prog_free(&g_hero_light);
    g_synced = false;
}

void swc_airtraffic_sync(const SwcCity *city) {
    if (g_hull.id == 0 || g_craft_light.id == 0 || g_hero.id == 0 || g_hero_light.id == 0 ||
        (g_synced && g_synced_seed == city->seed && g_synced_lanes == city->lane_count))
        return;
    free_scene();
    /* Full density: quality tiers come later, and the web version's `high` tier is 1. */
    uint32_t layer_seed = swc_air_layer_seed(city->seed);
    swc_air_pack_craft(city, 1.0, layer_seed, &g_craft);
    swc_air_plan_heroes(city, layer_seed, &g_heroes);

    if (g_craft.count > 0) {
        g_craft_vbo =
            rlLoadVertexBuffer(g_craft.data, g_craft.count * SWC_CRAFT_STRIDE * (int)sizeof(float), false);
        for (int s = 0; s < SWC_CRAFT_SHAPES; s++) {
            int from = g_craft.shape_start[s], to = g_craft.shape_start[s + 1];
            if (to == from)
                continue;
            swc_air_build_craft_hull(s, &g_hull_cpu);
            upload_hull(&g_hull_mesh[s], g_hull.id, &g_hull_cpu, false, g_craft_vbo, CRAFT_NAMES, 4,
                        SWC_CRAFT_STRIDE, from, to - from);
        }
        upload_craft_lights(&g_light_mesh, g_craft_light.id, g_craft.count);
    }
    if (g_heroes.count > 0) {
        g_hero_vbo =
            rlLoadVertexBuffer(g_heroes.data, g_heroes.count * SWC_HERO_STRIDE * (int)sizeof(float), false);
        swc_air_build_hero_hull(&g_hull_cpu);
        upload_hull(&g_hero_mesh, g_hero.id, &g_hull_cpu, true, g_hero_vbo, HERO_NAMES, 2, SWC_HERO_STRIDE, 0,
                    g_heroes.count);
        upload_hero_lights(&g_hero_light_mesh, g_hero_light.id, g_heroes.count);
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

static void hero_bob_uniform(SwcProg *p) {
    float bob[SWC_HERO_TYPES * 2];
    for (int t = 0; t < SWC_HERO_TYPES; t++) {
        const SwcHeroLimits *lim = swc_air_hero_limits(t);
        bob[t * 2] = (float)lim->bob_amp;
        bob[t * 2 + 1] = (float)lim->bob_cycles;
    }
    swc_u2fv(p, "uHeroBob", bob, SWC_HERO_TYPES);
}

void swc_airtraffic_hulls_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced)
        return;
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling(); /* the hulls are wound counter-clockwise seen from outside */
    swc_prog_use(&g_hull);
    swc_set_shared_uniforms(&g_hull, f, look);
    swc_u1f(&g_hull, "uHullFar", HULL_DRAW_DISTANCE);
    for (int s = 0; s < SWC_CRAFT_SHAPES; s++)
        draw_mesh(&g_hull_mesh[s]);
    swc_prog_use(&g_hero);
    swc_set_shared_uniforms(&g_hero, f, look);
    hero_bob_uniform(&g_hero);
    draw_mesh(&g_hero_mesh);
    rlDisableShader();
    rlDisableBackfaceCulling(); /* the legacy pass that follows draws both sides */
    rlEnableColorBlend();
}

void swc_airtraffic_lights_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (!g_synced)
        return;
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    /* Add colour, leave the alpha of the frame alone: One/One for rgb, Zero/One for alpha. */
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling(); /* ribbons are seen from both sides */
    swc_prog_use(&g_craft_light);
    swc_set_shared_uniforms(&g_craft_light, f, look);
    swc_u4fv(&g_craft_light, "uLights", swc_air_craft_light_table(), SWC_CRAFT_SHAPES * SWC_CRAFT_LIGHTS);
    swc_u4fv(&g_craft_light, "uTrail", swc_air_craft_trail_table(), SWC_CRAFT_SHAPES);
    draw_mesh(&g_light_mesh);
    swc_prog_use(&g_hero_light);
    swc_set_shared_uniforms(&g_hero_light, f, look);
    hero_bob_uniform(&g_hero_light);
    draw_mesh(&g_hero_light_mesh);
    rlDisableShader();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
