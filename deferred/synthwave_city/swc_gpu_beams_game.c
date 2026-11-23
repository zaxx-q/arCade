/* Synthwave City - the beams pass: the web version's beams layer. Searchlights sweeping from roofs and
 * plazas, beacon pillars and laser fans are instances of one ribbon (16 segments) that the vertex shader
 * aims, widens and turns to face the viewer; each emitter also gets a camera-facing flare sprite. Everything
 * is additive, depth-tested and leaves the depth buffer and the alpha of the frame alone. Four draws: ribbons
 * and flares of the sky set (searchlights, lasers) and of the pillar set. The pillar set is kept apart
 * because the wet streets mirror it and nothing else. */
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_beams.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

#define RIBBON_SEGMENTS 16

typedef struct MeshGpu {
    unsigned vao, ibo;
    unsigned vbo[8];
    int vbo_count;
    int index_count;
    int instances;
} MeshGpu;

typedef struct SetGpu {
    MeshGpu ribbon, flare;
} SetGpu;

static SwcProg g_ribbon, g_flare;
static SetGpu g_set[2]; /* 0: sky set, 1: pillar set */
static SwcPackedBeams g_packed;
static bool g_synced;
static uint32_t g_synced_seed;
static int g_synced_sites;
static SwcTier g_synced_tier;

static void add_attribute(MeshGpu *g, int loc, int comps, const float *data, int count, int divisor) {
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

static void free_mesh(MeshGpu *g) {
    if (g->vao != 0)
        rlUnloadVertexArray(g->vao);
    for (int i = 0; i < g->vbo_count; i++)
        rlUnloadVertexBuffer(g->vbo[i]);
    if (g->ibo != 0)
        rlUnloadVertexBuffer(g->ibo);
    memset(g, 0, sizeof(*g));
}

/* The per-instance attributes of a set, the same seven for the ribbon and the flare. */
static void add_instances(MeshGpu *g, unsigned prog, const SwcBeamSet *s) {
    add_attribute(g, rlGetLocationAttrib(prog, "aOrigin"), 3, s->origin, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aShape"), 4, s->shape, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aLook"), 4, s->look, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aTilt"), 4, s->tilt, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aAzim"), 4, s->azim, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aFan"), 4, s->fan, s->count, 1);
    add_attribute(g, rlGetLocationAttrib(prog, "aRoll"), 4, s->roll, s->count, 1);
}

/* A strip from y = 0 to 1, x = -1 to 1. Rows sit closer together near the emitter, where the beam is
 * brightest and narrowest. */
static void upload_ribbon(MeshGpu *g, const SwcBeamSet *s) {
    float position[(RIBBON_SEGMENTS + 1) * 2 * 3];
    uint16_t index[RIBBON_SEGMENTS * 6];
    for (int i = 0; i <= RIBBON_SEGMENTS; i++) {
        float u = (float)i / (float)RIBBON_SEGMENTS;
        float t = u * (0.4f + 0.6f * u);
        float *row = &position[i * 6];
        row[0] = -1.0f;
        row[1] = t;
        row[2] = 0.0f;
        row[3] = 1.0f;
        row[4] = t;
        row[5] = 0.0f;
    }
    for (int i = 0; i < RIBBON_SEGMENTS; i++) {
        uint16_t a = (uint16_t)(i * 2);
        uint16_t *q = &index[i * 6];
        q[0] = a;
        q[1] = (uint16_t)(a + 1);
        q[2] = (uint16_t)(a + 2);
        q[3] = (uint16_t)(a + 2);
        q[4] = (uint16_t)(a + 1);
        q[5] = (uint16_t)(a + 3);
    }
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, 0, 3, position, (RIBBON_SEGMENTS + 1) * 2,
                  0); /* position is location 0 in the prelude */
    add_instances(g, g_ribbon.id, s);
    g->ibo = rlLoadVertexBufferElement(index, (int)sizeof(index), false);
    g->index_count = RIBBON_SEGMENTS * 6;
    g->instances = s->count;
    rlDisableVertexArray();
}

/* A quad, x and y from -1 to 1. */
static void upload_flare(MeshGpu *g, const SwcBeamSet *s) {
    static const float quad[12] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
                                   -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  0.0f};
    static const uint16_t index[6] = {0, 1, 2, 2, 1, 3};
    free_mesh(g);
    g->vao = rlLoadVertexArray();
    rlEnableVertexArray(g->vao);
    add_attribute(g, 0, 3, quad, 4, 0);
    add_instances(g, g_flare.id, s);
    g->ibo = rlLoadVertexBufferElement(index, (int)sizeof(index), false);
    g->index_count = 6;
    g->instances = s->count;
    rlDisableVertexArray();
}

bool swc_beams_init(void) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get("beams.vert")};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("beams.frag")};
    const char *fvs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("common_vertex.glsl"), swc_shader_get("flare.vert")};
    const char *ffs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("flare.frag")};
    g_synced = false;
    return swc_prog_build(&g_ribbon, "beams", vs, 4, fs, 3) &&
           swc_prog_build(&g_flare, "beam_flares", fvs, 4, ffs, 3);
}

void swc_beams_free(void) {
    for (int i = 0; i < 2; i++) {
        free_mesh(&g_set[i].ribbon);
        free_mesh(&g_set[i].flare);
    }
    swc_prog_free(&g_ribbon);
    swc_prog_free(&g_flare);
    g_synced = false;
}

void swc_beams_sync(const SwcCity *city, const SwcQuality *q) {
    if (g_ribbon.id == 0 || g_flare.id == 0 ||
        (g_synced && g_synced_seed == city->seed && g_synced_sites == city->site_count &&
         g_synced_tier == q->tier))
        return;
    swc_pack_beams(city, q->density, swc_beam_layer_seed(city->seed), &g_packed);
    const SwcBeamSet *sets[2] = {&g_packed.sky, &g_packed.pillars};
    for (int i = 0; i < 2; i++) {
        if (sets[i]->count == 0) {
            free_mesh(&g_set[i].ribbon);
            free_mesh(&g_set[i].flare);
            continue;
        }
        upload_ribbon(&g_set[i].ribbon, sets[i]);
        upload_flare(&g_set[i].flare, sets[i]);
    }
    g_synced = true;
    g_synced_seed = city->seed;
    g_synced_sites = city->site_count;
    g_synced_tier = q->tier;
}

static void draw_all(SwcProg *prog, const SwcFrameUniforms *f, const SwcLook *look, bool ribbons,
                     int first_set, int set_count) {
    swc_prog_use(prog);
    swc_set_shared_uniforms(prog, f, look);
    for (int i = first_set; i < first_set + set_count; i++) {
        const MeshGpu *g = ribbons ? &g_set[i].ribbon : &g_set[i].flare;
        if (g->instances <= 0)
            continue;
        rlEnableVertexArray(g->vao);
        rlDrawVertexArrayElementsInstanced(0, g->index_count, NULL, g->instances);
        rlDisableVertexArray();
    }
}

static void draw_sets(const SwcFrameUniforms *f, const SwcLook *look, int first_set, int set_count) {
    if (g_ribbon.id == 0 || g_flare.id == 0 || !g_synced)
        return;
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    /* Add colour, leave the alpha of the frame alone: One/One for rgb, Zero/One for alpha. */
    rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM_SEPARATE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling(); /* a ribbon is seen from both sides */
    draw_all(&g_ribbon, f, look, true, first_set, set_count);
    draw_all(&g_flare, f, look, false, first_set, set_count);
    rlDisableShader();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void swc_beams_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    draw_sets(f, look, 0, 2);
}

void swc_beams_draw_pillars(const SwcFrameUniforms *f, const SwcLook *look) {
    draw_sets(f, look, 1, 1);
}
