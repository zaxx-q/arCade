/* Synthwave City - GPU helpers: shader programs, render targets and the two meshes every pass draws with.
 * Everything goes through rlgl, so the code runs on whatever OpenGL 3.3 context raylib created. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"

/* ------------------------------------------------------------------ programs */

static char *join_parts(const char *const *parts, int n) {
    size_t len = 1;
    for (int i = 0; i < n; i++) {
        if (parts[i] == NULL)
            return NULL;
        len += strlen(parts[i]) + 1;
    }
    char *s = (char *)malloc(len);
    if (s == NULL)
        return NULL;
    char *w = s;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(parts[i]);
        memcpy(w, parts[i], l);
        w += l;
        *w++ = '\n';
    }
    *w = '\0';
    return s;
}

bool swc_prog_build(SwcProg *p, const char *label, const char *const *vs, int vs_n, const char *const *fs,
                    int fs_n) {
    memset(p, 0, sizeof(*p));
    char *v = join_parts(vs, vs_n);
    char *f = join_parts(fs, fs_n);
    if (v != NULL && f != NULL)
        p->id = rlLoadShaderProgram(v, f);
    free(v);
    free(f);
    if (p->id == 0)
        TraceLog(LOG_WARNING, "SWC: shader '%s' did not build", label);
    return p->id != 0;
}

void swc_prog_free(SwcProg *p) {
    if (p->id != 0)
        rlUnloadShaderProgram(p->id);
    memset(p, 0, sizeof(*p));
}

void swc_prog_use(const SwcProg *p) {
    rlEnableShader(p->id);
}

static int uloc(SwcProg *p, const char *name) {
    for (int i = 0; i < p->count; i++)
        if (p->name[i] == name || strcmp(p->name[i], name) == 0)
            return p->loc[i];
    int loc = rlGetLocationUniform(p->id, name);
    if (p->count < SWC_PROG_UNIFORMS) {
        p->name[p->count] = name;
        p->loc[p->count] = loc;
        p->count++;
    }
    return loc;
}

void swc_u1i(SwcProg *p, const char *name, int v) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, &v, RL_SHADER_UNIFORM_INT, 1);
}
void swc_u1f(SwcProg *p, const char *name, float v) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, &v, RL_SHADER_UNIFORM_FLOAT, 1);
}
void swc_u2f(SwcProg *p, const char *name, float x, float y) {
    int loc = uloc(p, name);
    float v[2] = {x, y};
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC2, 1);
}
void swc_u3f(SwcProg *p, const char *name, const float v[3]) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC3, 1);
}
void swc_u4f(SwcProg *p, const char *name, const float v[4]) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC4, 1);
}
void swc_u2fv(SwcProg *p, const char *name, const float *v, int count) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC2, count);
}
void swc_u3fv(SwcProg *p, const char *name, const float *v, int count) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC3, count);
}
void swc_u4fv(SwcProg *p, const char *name, const float *v, int count) {
    int loc = uloc(p, name);
    if (loc >= 0)
        rlSetUniform(loc, v, RL_SHADER_UNIFORM_VEC4, count);
}
void swc_um4(SwcProg *p, const char *name, const float m[16]) {
    int loc = uloc(p, name);
    if (loc < 0)
        return;
    /* raylib's Matrix fields m0..m15 are the column-major array in order */
    Matrix mat = {m[0], m[4], m[8],  m[12], m[1], m[5], m[9],  m[13],
                  m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
    rlSetUniformMatrix(loc, mat);
}
void swc_utex(SwcProg *p, const char *name, int unit, unsigned texture) {
    int loc = uloc(p, name);
    rlActiveTextureSlot(unit);
    rlEnableTexture(texture);
    if (loc >= 0)
        rlSetUniform(loc, &unit, RL_SHADER_UNIFORM_INT, 1);
    rlActiveTextureSlot(0);
}

/* ------------------------------------------------------------------ targets */

bool swc_target_create(SwcTarget *t, int w, int h, bool with_depth) {
    memset(t, 0, sizeof(*t));
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    t->w = w;
    t->h = h;
    t->tex = rlLoadTexture(NULL, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    if (t->tex == 0)
        return false;
    rlTextureParameters(t->tex, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(t->tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(t->tex, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(t->tex, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    t->fbo = rlLoadFramebuffer();
    if (t->fbo == 0) {
        swc_target_destroy(t);
        return false;
    }
    rlFramebufferAttach(t->fbo, t->tex, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (with_depth) {
        t->depth = rlLoadTextureDepth(w, h, true);
        rlFramebufferAttach(t->fbo, t->depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    if (!rlFramebufferComplete(t->fbo)) {
        TraceLog(LOG_WARNING, "SWC: a %dx%d float render target is not complete on this GPU", w, h);
        swc_target_destroy(t);
        return false;
    }
    return true;
}

bool swc_target_create_mipped(SwcTarget *t, int w, int h, bool with_depth) {
    if (!swc_target_create(t, w, h, with_depth))
        return false;
    rlTextureParameters(t->tex, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_MIP_LINEAR);
    return true;
}

void swc_target_make_mips(const SwcTarget *t) {
    int levels = 1;
    rlGenTextureMipmaps(t->tex, t->w, t->h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, &levels);
}

void swc_target_destroy(SwcTarget *t) {
    if (t->fbo != 0)
        rlUnloadFramebuffer(t->fbo); /* also deletes the attachments raylib knows of */
    if (t->tex != 0)
        rlUnloadTexture(t->tex);
    memset(t, 0, sizeof(*t));
}

void swc_target_bind(const SwcTarget *t) {
    rlEnableFramebuffer(t->fbo);
    rlViewport(0, 0, t->w, t->h);
}

/* ------------------------------------------------------------------ meshes */

static unsigned g_tri_vao, g_tri_vbo, g_cube_vao, g_cube_vbo;

static unsigned make_mesh(const float *xyz, int bytes, unsigned *vbo) {
    unsigned vao = rlLoadVertexArray();
    rlEnableVertexArray(vao);
    *vbo = rlLoadVertexBuffer(xyz, bytes, false);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 3 * (int)sizeof(float), 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
    return vao;
}

bool swc_geometry_init(void) {
    static const float tri[9] = {-1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f};
    g_tri_vao = make_mesh(tri, (int)sizeof(tri), &g_tri_vbo);
    /* the unit cube as 12 triangles: corner index bits are x, y, z */
    static const int quads[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4},
                                    {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
    float cube[36 * 3];
    int n = 0;
    for (int q = 0; q < 6; q++) {
        static const int order[6] = {0, 1, 2, 0, 2, 3};
        for (int k = 0; k < 6; k++) {
            int c = quads[q][order[k]];
            cube[n++] = (c & 1) ? 1.0f : -1.0f;
            cube[n++] = (c & 2) ? 1.0f : -1.0f;
            cube[n++] = (c & 4) ? 1.0f : -1.0f;
        }
    }
    g_cube_vao = make_mesh(cube, (int)sizeof(cube), &g_cube_vbo);
    return g_tri_vao != 0 && g_cube_vao != 0;
}

void swc_geometry_free(void) {
    if (g_tri_vao != 0) {
        rlUnloadVertexArray(g_tri_vao);
        rlUnloadVertexBuffer(g_tri_vbo);
    }
    if (g_cube_vao != 0) {
        rlUnloadVertexArray(g_cube_vao);
        rlUnloadVertexBuffer(g_cube_vbo);
    }
    g_tri_vao = g_tri_vbo = g_cube_vao = g_cube_vbo = 0;
}

void swc_draw_fullscreen(void) {
    rlEnableVertexArray(g_tri_vao);
    rlDrawVertexArray(0, 3);
    rlDisableVertexArray();
}

void swc_draw_cube(void) {
    rlEnableVertexArray(g_cube_vao);
    rlDrawVertexArray(0, 36);
    rlDisableVertexArray();
}
