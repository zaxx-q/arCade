/* Synthwave City - the buildings pass: the web version's buildings layer. Every box of the city plan is one
 * instance of a unit box or a unit cylinder (swc_shapes.c) with its seeds and style packed by swc_pack.c;
 * the whole facade (windows, sign panels, shop fronts, neon trim) is drawn by the fragment shader. A second,
 * additive pass draws the aviation lights as sprites. Two draw calls for the buildings, one for the lights.
 */
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"
#include "swc_pack.h"
#include "swc_shaders.h"
#include "swc_shapes.h"

typedef struct ShapeGpu {
    unsigned vao, ibo;
    unsigned vbo[8];
    int vbo_count;
    int index_count;
    int instances;
} ShapeGpu;

static SwcProg g_boxes, g_lights;
static ShapeGpu g_shape[2];
static ShapeGpu g_lights_gpu;
static SwcPackedBoxes g_packed;
static SwcPackedLights g_packed_lights;
static bool g_synced;
static uint32_t g_synced_seed;
static int g_synced_boxes;

/* An attribute: its own buffer, read from `data`, one value per vertex or (divisor 1) one per instance. */
static void add_attribute(ShapeGpu *g, int loc, int comps, const float *data, int count, int divisor) {
    unsigned vbo = rlLoadVertexBuffer(data, count * comps * (int)sizeof(float), false);
    if (g->vbo_count < 8)
        g->vbo[g->vbo_count++] = vbo;
    if (loc < 0) /* the compiler dropped an attribute the shader does not use */
        return;
    rlSetVertexAttribute((unsigned)loc, comps, RL_FLOAT, false, comps * (int)sizeof(float), 0);
    rlEnableVertexAttribute((unsigned)loc);
    if (divisor > 0)
        rlSetVertexAttributeDivisor((unsigned)loc, divisor);
}

static void free_shape(ShapeGpu *g) {
    if (g->vao != 0)
        rlUnloadVertexArray(g->vao);
    for (int i = 0; i < g->vbo_count; i++)
        rlUnloadVertexBuffer(g->vbo[i]);
    if (g->ibo != 0)
        rlUnloadVertexBuffer(g->ibo);
    memset(g, 0, sizeof(*g));
}

static void upload_shape(ShapeGpu *g, const SwcShape *sh, int shape) {
    free_shape(g);
    unsigned prog = g_boxes.id;
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, 0, 3, sh->position, sh->vertex_count, 0); /* position is location 0 in the prelude */
    add_attribute(g, rlGetLocationAttrib(prog, "normal"), 3, sh->normal, sh->vertex_count, 0);
    add_attribute(g, rlGetLocationAttrib(prog, "aFace"), 3, sh->face, sh->vertex_count, 0);
    int n = g_packed.count[shape], f = g_packed.first[shape];
    add_attribute(g, rlGetLocationAttrib(prog, "aPos"), SWC_POS_SIZE, &g_packed.pos[f * SWC_POS_SIZE], n, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aSize"), SWC_SIZE_SIZE, &g_packed.size[f * SWC_SIZE_SIZE], n,
                  1);
    add_attribute(g, rlGetLocationAttrib(prog, "aStyle"), SWC_STYLE_SIZE, &g_packed.style[f * SWC_STYLE_SIZE],
                  n, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aSeed"), SWC_SEED_SIZE, &g_packed.seed[f * SWC_SEED_SIZE], n,
                  1);
    g->ibo = rlLoadVertexBufferElement(sh->index, sh->index_count * (int)sizeof(uint16_t), false);
    g->index_count = sh->index_count;
    g->instances = n;
    rlDisableVertexArray();
}

static void upload_lights(void) {
    static const float quad[12] = {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
                                   1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};
    static const uint16_t index[6] = {0, 1, 2, 0, 2, 3};
    ShapeGpu *g = &g_lights_gpu;
    free_shape(g);
    unsigned prog = g_lights.id;
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, 0, 3, quad, 4, 0);
    int n = g_packed_lights.count;
    add_attribute(g, rlGetLocationAttrib(prog, "aSite"), SWC_LIGHT_SITE_SIZE, g_packed_lights.site, n, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aKind"), SWC_LIGHT_KIND_SIZE, g_packed_lights.kind, n, 1);
    g->ibo = rlLoadVertexBufferElement(index, (int)sizeof(index), false);
    g->index_count = 6;
    g->instances = n;
    rlDisableVertexArray();
}

bool swc_buildings_init(void) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), "layout(location = 1) in vec3 normal;",
                        swc_shader_get("common.glsl"), swc_shader_get("common_vertex.glsl"),
                        swc_shader_get("buildings.vert")};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("buildings.frag")};
    const char *lvs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("common_vertex.glsl"), swc_shader_get("lights.vert")};
    const char *lfs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("lights.frag")};
    g_synced = false;
    return swc_prog_build(&g_boxes, "buildings", vs, 5, fs, 3) &&
           swc_prog_build(&g_lights, "building_lights", lvs, 4, lfs, 3);
}

void swc_buildings_free(void) {
    free_shape(&g_shape[SWC_SHAPE_SQUARE]);
    free_shape(&g_shape[SWC_SHAPE_ROUND]);
    free_shape(&g_lights_gpu);
    swc_prog_free(&g_boxes);
    swc_prog_free(&g_lights);
    g_synced = false;
}

void swc_buildings_sync(const SwcCity *city) {
    if (g_boxes.id == 0 || (g_synced && g_synced_seed == city->seed && g_synced_boxes == city->box_count))
        return;
    swc_pack_boxes(city, &g_packed);
    swc_pack_roof_lights(city, &g_packed_lights);
    static SwcShape box, cylinder;
    swc_shape_unit_box(&box);
    swc_shape_unit_cylinder(&cylinder, SWC_CYLINDER_SEGMENTS);
    upload_shape(&g_shape[SWC_SHAPE_SQUARE], &box, SWC_SHAPE_SQUARE);
    upload_shape(&g_shape[SWC_SHAPE_ROUND], &cylinder, SWC_SHAPE_ROUND);
    upload_lights();
    g_synced = true;
    g_synced_seed = city->seed;
    g_synced_boxes = city->box_count;
}

void swc_buildings_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_boxes.id == 0 || !g_synced)
        return;
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling(); /* the unit shapes wind counter-clockwise seen from outside */
    swc_prog_use(&g_boxes);
    swc_set_shared_uniforms(&g_boxes, f, look);
    for (int s = 0; s < 2; s++) {
        const ShapeGpu *g = &g_shape[s];
        if (g->instances <= 0)
            continue;
        rlEnableVertexArray(g->vao);
        rlDrawVertexArrayElementsInstanced(0, g->index_count, NULL, g->instances);
        rlDisableVertexArray();
    }
    rlDisableShader();
    rlDisableBackfaceCulling(); /* the legacy pass that follows draws both sides */
    rlEnableColorBlend();
}

void swc_building_lights_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_lights.id == 0 || !g_synced || g_lights_gpu.instances <= 0)
        return;
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    swc_prog_use(&g_lights);
    swc_set_shared_uniforms(&g_lights, f, look);
    rlEnableVertexArray(g_lights_gpu.vao);
    rlDrawVertexArrayElementsInstanced(0, g_lights_gpu.index_count, NULL, g_lights_gpu.instances);
    rlDisableVertexArray();
    rlDisableShader();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
