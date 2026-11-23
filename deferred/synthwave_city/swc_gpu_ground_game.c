/* Synthwave City - the ground pass: the web version's ground layer. One disc under the camera whose fragment
 * shader draws the streets, markings, plazas and the wet mirror; the lamp posts (one pattern of lamps
 * instanced on a small lattice that follows the camera in whole pattern steps); and a glow sprite for every
 * lamp in the tile. The layout of the lamps is swc_ground.c's; the plan's cells reach the shader as a 40 x 40
 * texture. Three draws: ground and posts are opaque, the glows additive. */
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_ground.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

/* The disc reaches past the far plane (14000) in the corners of the widest view. */
#define DISC_RADIUS (14000.0 * 1.75)

typedef char swc_cell_is_one_rgba_texel[(sizeof(SwcCell) == 4) ? 1 : -1];

typedef struct MeshGpu {
    unsigned vao, ibo;
    unsigned vbo[8];
    int vbo_count;
    int index_count;
    int instances;
} MeshGpu;

static SwcProg g_ground, g_ground_flat, g_post, g_glow; /* g_ground_flat: the one without a mirror */
static MeshGpu g_disc, g_post_mesh, g_glow_mesh;
static unsigned g_cell_tex, g_black_tex;
static double g_spacing;
static bool g_synced;
static uint32_t g_synced_seed;
static SwcTier g_lamps_tier = SWC_TIER_COUNT; /* the tier the lamps were built for (none yet) */
static bool g_flat_tried;                     /* the variant without a mirror is built when first needed */

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

static void upload_disc(void) {
    static SwcDisc disc;
    swc_build_disc(DISC_RADIUS, &disc);
    free_mesh(&g_disc);
    g_disc.vao = rlLoadVertexArray();
    rlEnableVertexArray(g_disc.vao);
    add_attribute(&g_disc, 0, 3, disc.position, disc.vertex_count, 0);
    g_disc.ibo = rlLoadVertexBufferElement(disc.index, disc.index_count * (int)sizeof(uint16_t), false);
    g_disc.index_count = disc.index_count;
    g_disc.instances = 1;
    rlDisableVertexArray();
}

/* The lamp post (per vertex: position, part, axis) and the lattice of its instances. */
static bool upload_posts(const float *pattern, int per_pattern) {
    static SwcPostMesh post;
    swc_build_post(&post);
    int copies = SWC_POST_LATTICE * 2;
    int count = per_pattern * copies * copies;
    float *a_lamp = malloc((size_t)count * 4 * sizeof(float));
    float *a_info = malloc((size_t)count * 2 * sizeof(float));
    if (a_lamp == NULL || a_info == NULL) {
        free(a_lamp);
        free(a_info);
        return false;
    }
    count = swc_build_post_instances(pattern, per_pattern, SWC_POST_LATTICE, a_lamp, a_info);
    free_mesh(&g_post_mesh);
    g_post_mesh.vao = rlLoadVertexArray();
    rlEnableVertexArray(g_post_mesh.vao);
    add_attribute(&g_post_mesh, 0, 3, post.position, post.vertex_count, 0);
    add_attribute(&g_post_mesh, rlGetLocationAttrib(g_post.id, "aPart"), 1, post.part, post.vertex_count, 0);
    add_attribute(&g_post_mesh, rlGetLocationAttrib(g_post.id, "aAxis"), 3, post.axis, post.vertex_count, 0);
    add_attribute(&g_post_mesh, rlGetLocationAttrib(g_post.id, "aLamp"), 4, a_lamp, count, 1);
    add_attribute(&g_post_mesh, rlGetLocationAttrib(g_post.id, "aInfo"), 2, a_info, count, 1);
    g_post_mesh.ibo = rlLoadVertexBufferElement(post.index, post.index_count * (int)sizeof(uint16_t), false);
    g_post_mesh.index_count = post.index_count;
    g_post_mesh.instances = count;
    rlDisableVertexArray();
    free(a_lamp);
    free(a_info);
    return true;
}

/* One glow sprite per lamp in the tile, so the streets carry points of light out to the far fade. */
static bool upload_glows(void) {
    static const float quad[12] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
                                   -1.0f, 1.0f,  0.0f, 1.0f, 1.0f,  0.0f};
    static const uint16_t index[6] = {0, 1, 2, 2, 1, 3};
    int total = swc_lamp_count(g_spacing, SWC_TILE);
    float *lamps = malloc((size_t)total * SWC_LAMP_STRIDE * sizeof(float));
    float *head = malloc((size_t)total * 3 * sizeof(float));
    float *colour = malloc((size_t)total * sizeof(float));
    if (lamps == NULL || head == NULL || colour == NULL) {
        free(lamps);
        free(head);
        free(colour);
        return false;
    }
    total = swc_build_lamps(g_spacing, SWC_TILE, lamps);
    for (int i = 0; i < total; i++) {
        swc_lamp_head(lamps, i, &head[i * 3]);
        colour[i] = lamps[i * SWC_LAMP_STRIDE + SWC_LAMP_COLOR];
    }
    free_mesh(&g_glow_mesh);
    g_glow_mesh.vao = rlLoadVertexArray();
    rlEnableVertexArray(g_glow_mesh.vao);
    add_attribute(&g_glow_mesh, 0, 3, quad, 4, 0);
    add_attribute(&g_glow_mesh, rlGetLocationAttrib(g_glow.id, "aHead"), 3, head, total, 1);
    add_attribute(&g_glow_mesh, rlGetLocationAttrib(g_glow.id, "aCol"), 1, colour, total, 1);
    g_glow_mesh.ibo = rlLoadVertexBufferElement(index, (int)sizeof(index), false);
    g_glow_mesh.index_count = 6;
    g_glow_mesh.instances = total;
    rlDisableVertexArray();
    free(lamps);
    free(head);
    free(colour);
    return true;
}

static unsigned make_texture(const void *rgba, int w, int h) {
    unsigned id = rlLoadTexture(rgba, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    if (id != 0) {
        rlTextureParameters(id, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_NEAREST);
        rlTextureParameters(id, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_NEAREST);
        rlTextureParameters(id, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
        rlTextureParameters(id, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    }
    return id;
}

bool swc_ground_init(void) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get("ground.vert")};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("ground_defines.glsl"), "#define G_REFLECT 1\n",
                        swc_shader_get("ground.frag")};
    const char *pvs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("common_vertex.glsl"), swc_shader_get("ground_defines.glsl"),
                         swc_shader_get("lamp_post.vert")};
    const char *pfs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("lamp_post.frag")};
    const char *gvs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("common_vertex.glsl"), swc_shader_get("ground_defines.glsl"),
                         swc_shader_get("lamp_glow.vert")};
    const char *gfs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                         swc_shader_get("lamp_glow.frag")};
    g_synced = false;
    if (!swc_prog_build(&g_ground, "ground", vs, 4, fs, 5) ||
        !swc_prog_build(&g_post, "lamp_posts", pvs, 5, pfs, 3) ||
        !swc_prog_build(&g_glow, "lamp_glows", gvs, 5, gfs, 3))
        return false;

    static const unsigned char black[4] = {0, 0, 0, 255};
    g_black_tex = make_texture(black, 1, 1);
    g_lamps_tier = SWC_TIER_COUNT;
    g_flat_tried = false;
    upload_disc();
    return g_disc.vao != 0 && g_black_tex != 0; /* the lamps follow with the first sync, for the tier */
}

/* The lamps at the spacing of a tier's density: the posts (one pattern, repeated over a lattice) and one glow
 * per lamp in the tile. */
static bool build_lamps(double density) {
    g_spacing = swc_lamp_spacing(density);
    int per_pattern = swc_lamp_count(g_spacing, SWC_LAMP_PATTERN);
    float *pattern = malloc((size_t)per_pattern * SWC_LAMP_STRIDE * sizeof(float));
    if (pattern == NULL)
        return false;
    per_pattern = swc_build_lamps(g_spacing, SWC_LAMP_PATTERN, pattern);
    bool ok = upload_posts(pattern, per_pattern);
    free(pattern);
    return ok && upload_glows();
}

/* The streets shader without the mirror: what the web version compiles when a tier has no reflection pass.
 * Built when it is first needed, so a machine that never uses the "low" tier never pays for it. */
static void build_flat_variant(void) {
    g_flat_tried = true;
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("common_vertex.glsl"), swc_shader_get("ground.vert")};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), swc_shader_get("common.glsl"),
                        swc_shader_get("ground_defines.glsl"), swc_shader_get("ground.frag")};
    if (!swc_prog_build(&g_ground_flat, "ground_flat", vs, 4, fs, 4))
        TraceLog(LOG_WARNING, "SWC: the mirror-free streets shader did not build; the main one is used");
}

void swc_ground_free(void) {
    free_mesh(&g_disc);
    free_mesh(&g_post_mesh);
    free_mesh(&g_glow_mesh);
    if (g_cell_tex != 0)
        rlUnloadTexture(g_cell_tex);
    if (g_black_tex != 0)
        rlUnloadTexture(g_black_tex);
    g_cell_tex = g_black_tex = 0;
    swc_prog_free(&g_ground);
    swc_prog_free(&g_ground_flat);
    swc_prog_free(&g_post);
    swc_prog_free(&g_glow);
    g_synced = false;
}

void swc_ground_sync(const SwcCity *city, const SwcQuality *q) {
    if (g_ground.id == 0)
        return;
    if (g_lamps_tier != q->tier) { /* the first sync, or the tier changed: lamps at the new spacing */
        g_lamps_tier = q->tier;
        if (!build_lamps(q->density))
            TraceLog(LOG_WARNING, "SWC: the street lamps could not be uploaded");
    }
    if (g_synced && g_synced_seed == city->seed)
        return;
    if (g_cell_tex != 0)
        rlUnloadTexture(g_cell_tex);
    g_cell_tex = make_texture(city->cells, SWC_CELLS, SWC_CELLS);
    g_synced = g_cell_tex != 0;
    g_synced_seed = city->seed;
}

/* Opaque state for the ground and the posts: both sides drawn, depth tested and written. */
static void opaque_state(void) {
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlDisableBackfaceCulling();
}

static void unbind_textures(void) {
    rlActiveTextureSlot(1);
    rlDisableTexture();
    rlActiveTextureSlot(0);
    rlDisableTexture();
}

void swc_ground_draw(const SwcFrameUniforms *f, const SwcLook *look, const SwcReflection *refl) {
    if (g_ground.id == 0 || !g_synced)
        return;
    bool mirrored = refl != NULL && refl->tex != 0;
    if (!mirrored && g_ground_flat.id == 0 && !g_flat_tried)
        build_flat_variant();
    /* No mirror: the variant that never samples one, or (if it did not build) the main shader with its mirror
     * switched off. */
    SwcProg *prog = mirrored || g_ground_flat.id == 0 ? &g_ground : &g_ground_flat;
    opaque_state();
    swc_prog_use(prog);
    swc_set_shared_uniforms(prog, f, look);
    swc_u1f(prog, "uLampSp", (float)g_spacing);
    swc_utex(prog, "uCellTex", 0, g_cell_tex);
    if (prog == &g_ground) {
        swc_utex(prog, "uReflTex", 1, mirrored ? refl->tex : g_black_tex);
        if (mirrored)
            swc_um4(prog, "uReflMatrix", refl->matrix);
        swc_u1f(prog, "uReflAmount", mirrored ? 1.0f : 0.0f);
    }
    rlEnableVertexArray(g_disc.vao);
    rlDrawVertexArrayElements(0, g_disc.index_count, NULL);
    rlDisableVertexArray();
    rlDisableShader();
    unbind_textures();
    rlEnableColorBlend();
}

void swc_lamp_posts_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_post.id == 0 || g_post_mesh.instances <= 0)
        return;
    opaque_state();
    swc_prog_use(&g_post);
    swc_set_shared_uniforms(&g_post, f, look);
    swc_u2f(&g_post, "uPostFade", (float)SWC_POST_FADE_START, (float)SWC_POST_FADE_END);
    rlEnableVertexArray(g_post_mesh.vao);
    rlDrawVertexArrayElementsInstanced(0, g_post_mesh.index_count, NULL, g_post_mesh.instances);
    rlDisableVertexArray();
    rlDisableShader();
    rlEnableColorBlend();
}

void swc_lamp_glows_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_glow.id == 0 || g_glow_mesh.instances <= 0)
        return;
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    swc_prog_use(&g_glow);
    swc_set_shared_uniforms(&g_glow, f, look);
    rlEnableVertexArray(g_glow_mesh.vao);
    rlDrawVertexArrayElementsInstanced(0, g_glow_mesh.index_count, NULL, g_glow_mesh.instances);
    rlDisableVertexArray();
    rlDisableShader();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}
