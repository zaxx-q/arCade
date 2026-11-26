/* Synthwave City - post-processing: the HDR scene target, the bloom of the web version (pmndrs
 * MipmapBlurPass, 7 levels) and the last pass (chromatic aberration, bloom, grade, sRGB, dither). */
#include <stddef.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

SwcPostParams swc_post_defaults(void) {
    SwcPostParams p;
    memset(&p, 0, sizeof(p));
    p.exposure = 1.0f;
    p.vignette = 0.42f;
    p.grain = 0.035f;
    p.hold = 0.45f;
    p.crt = 0.0f;
    p.ca_offset[0] = 0.0011f;
    p.ca_offset[1] = 0.0011f * 0.6f;
    p.ca_modulation = 0.2f;
    p.bloom_threshold = 0.82f;
    p.bloom_smoothing = 0.3f;
    p.bloom_radius = 0.78f;
    p.bloom_intensity = 1.15f;
    p.bloom = true;
    p.post = true;
    return p;
}

static bool build_programs(SwcPost *p) {
    const char *vs[] = {swc_shader_get("fullscreen.vert")};
    const char *luma[] = {swc_shader_get("bloom_luma.frag")};
    const char *down[] = {swc_shader_get("bloom_down.frag")};
    const char *up[] = {swc_shader_get("bloom_up.frag")};
    const char *post[] = {swc_shader_get("post.frag")};
    return swc_prog_build(&p->p_luma, "bloom_luma", vs, 1, luma, 1) &&
           swc_prog_build(&p->p_down, "bloom_down", vs, 1, down, 1) &&
           swc_prog_build(&p->p_up, "bloom_up", vs, 1, up, 1) &&
           swc_prog_build(&p->p_post, "post", vs, 1, post, 1);
}

/* ---- the multisampled scene target ---- */

#if defined(_WIN32)
#define SWC_GLCALL __stdcall
#else
#define SWC_GLCALL
#endif

typedef void(SWC_GLCALL *GlGenRenderbuffers)(int n, unsigned *ids);
typedef void(SWC_GLCALL *GlDeleteRenderbuffers)(int n, const unsigned *ids);
typedef void(SWC_GLCALL *GlBindRenderbuffer)(unsigned target, unsigned id);
typedef void(SWC_GLCALL *GlRenderbufferStorageMultisample)(unsigned target, int samples, unsigned format,
                                                           int width, int height);
typedef unsigned(SWC_GLCALL *GlGetError)(void);

#define GL_NO_ERROR 0u
#define GL_RENDERBUFFER 0x8D41u
#define GL_READ_FRAMEBUFFER 0x8CA8u
#define GL_DRAW_FRAMEBUFFER 0x8CA9u
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_RGBA16F 0x881Au
#define GL_DEPTH_COMPONENT24 0x81A6u

static struct {
    bool tried, ok;
    GlGenRenderbuffers gen;
    GlDeleteRenderbuffers del;
    GlBindRenderbuffer bind;
    GlRenderbufferStorageMultisample storage;
    GlGetError error;
} gl;

/* A function pointer from the loader. A cast between void * and a function pointer is not standard C, so the
 * bytes are copied. */
static bool load_proc(const char *name, void *fn, size_t size) {
    void *addr = rlGetProcAddress(name);
    if (addr == NULL || size != sizeof(addr))
        return false;
    memcpy(fn, &addr, size);
    return true;
}

static bool load_gl(void) {
    if (!gl.tried) {
        gl.tried = true;
        gl.ok = load_proc("glGenRenderbuffers", &gl.gen, sizeof(gl.gen)) &&
                load_proc("glDeleteRenderbuffers", &gl.del, sizeof(gl.del)) &&
                load_proc("glBindRenderbuffer", &gl.bind, sizeof(gl.bind)) &&
                load_proc("glRenderbufferStorageMultisample", &gl.storage, sizeof(gl.storage)) &&
                load_proc("glGetError", &gl.error, sizeof(gl.error));
        if (!gl.ok)
            TraceLog(LOG_WARNING, "SWC: no multisampled renderbuffers on this GPU; MSAA is off");
    }
    return gl.ok;
}

static void msaa_destroy(SwcMsaa *m) {
    if (m->fbo != 0)
        rlUnloadFramebuffer(m->fbo); /* also deletes the depth renderbuffer it holds */
    else if (m->depth != 0)
        gl.del(1, &m->depth);
    if (m->color != 0)
        gl.del(1, &m->color);
    memset(m, 0, sizeof(*m));
}

static bool msaa_try(SwcMsaa *m, int w, int h, int samples) {
    memset(m, 0, sizeof(*m));
    while (gl.error() != GL_NO_ERROR) { /* an error left by someone else is not ours */
    }
    gl.gen(1, &m->color);
    gl.bind(GL_RENDERBUFFER, m->color);
    gl.storage(GL_RENDERBUFFER, samples, GL_RGBA16F, w, h);
    gl.gen(1, &m->depth);
    gl.bind(GL_RENDERBUFFER, m->depth);
    gl.storage(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w, h);
    gl.bind(GL_RENDERBUFFER, 0);
    if (gl.error() != GL_NO_ERROR) { /* more samples than the GPU has, or no room */
        msaa_destroy(m);
        return false;
    }
    m->fbo = rlLoadFramebuffer();
    if (m->fbo != 0) {
        rlFramebufferAttach(m->fbo, m->color, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_RENDERBUFFER, 0);
        rlFramebufferAttach(m->fbo, m->depth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    if (m->fbo == 0 || !rlFramebufferComplete(m->fbo)) {
        msaa_destroy(m);
        return false;
    }
    m->samples = samples;
    return true;
}

/* The most samples up to `want` (halving: 4, 2) that this GPU gives; false if none. */
static bool msaa_create(SwcMsaa *m, int w, int h, int want) {
    memset(m, 0, sizeof(*m));
    if (want < 2 || !load_gl())
        return false;
    for (int samples = want; samples >= 2; samples /= 2)
        if (msaa_try(m, w, h, samples))
            return true;
    TraceLog(LOG_WARNING, "SWC: %dx MSAA is not available at %dx%d; drawing without it", want, w, h);
    return false;
}

bool swc_post_init(SwcPost *p, int w, int h, int msaa) {
    memset(p, 0, sizeof(*p));
    p->w = w;
    p->h = h;
    /* The MSAA target holds the depth; the resolved HDR target needs none then. */
    bool multisampled = msaa_create(&p->msaa, w, h, msaa);
    bool ok = build_programs(p) && swc_target_create(&p->hdr, w, h, !multisampled) &&
              swc_target_create(&p->luma, w, h, false);
    int lw = w, lh = h;
    for (int i = 0; ok && i < SWC_BLOOM_LEVELS; i++) {
        lw = (lw + 1) / 2; /* Math.round(w * 0.5) of the web version: halves round up */
        lh = (lh + 1) / 2;
        ok = swc_target_create(&p->down[i], lw, lh, false);
        if (ok && i < SWC_BLOOM_LEVELS - 1)
            ok = swc_target_create(&p->up[i], lw, lh, false);
    }
    p->ready = ok;
    if (!ok)
        swc_post_free(p);
    return ok;
}

void swc_post_free(SwcPost *p) {
    if (p->msaa.fbo != 0 || p->msaa.color != 0)
        msaa_destroy(&p->msaa);
    swc_target_destroy(&p->hdr);
    swc_target_destroy(&p->luma);
    for (int i = 0; i < SWC_BLOOM_LEVELS; i++)
        swc_target_destroy(&p->down[i]);
    for (int i = 0; i < SWC_BLOOM_LEVELS - 1; i++)
        swc_target_destroy(&p->up[i]);
    swc_prog_free(&p->p_luma);
    swc_prog_free(&p->p_down);
    swc_prog_free(&p->p_up);
    swc_prog_free(&p->p_post);
    p->ready = false;
}

/* The target the scene is drawn into, as raylib wants to be told about it. */
static RenderTexture2D scene_as_render_texture(const SwcPost *p) {
    RenderTexture2D rt;
    memset(&rt, 0, sizeof(rt));
    bool multisampled = p->msaa.samples > 0;
    rt.id = multisampled ? p->msaa.fbo : p->hdr.fbo;
    rt.texture.id = p->hdr.tex;
    rt.texture.width = p->hdr.w;
    rt.texture.height = p->hdr.h;
    rt.texture.mipmaps = 1;
    rt.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rt.depth.id = multisampled ? p->msaa.depth : p->hdr.depth;
    rt.depth.width = p->hdr.w;
    rt.depth.height = p->hdr.h;
    return rt;
}

void swc_post_scene_begin(SwcPost *p) {
    BeginTextureMode(scene_as_render_texture(p));
    ClearBackground(BLACK); /* colour and depth */
}

void swc_post_scene_end(SwcPost *p) {
    EndTextureMode();
    if (p->msaa.samples > 0) { /* resolve the samples into the single-sample HDR target the bloom reads */
        rlBindFramebuffer(GL_READ_FRAMEBUFFER, p->msaa.fbo);
        rlBindFramebuffer(GL_DRAW_FRAMEBUFFER, p->hdr.fbo);
        rlBlitFramebuffer(0, 0, p->w, p->h, 0, 0, p->w, p->h, GL_COLOR_BUFFER_BIT);
        rlDisableFramebuffer(); /* both bindings back to the window */
    }
}

static void bloom(SwcPost *p, const SwcPostParams *a) {
    swc_prog_use(&p->p_luma);
    swc_target_bind(&p->luma);
    swc_utex(&p->p_luma, "inputBuffer", 0, p->hdr.tex);
    swc_u1f(&p->p_luma, "threshold", a->bloom_threshold);
    swc_u1f(&p->p_luma, "smoothing", a->bloom_smoothing);
    swc_draw_fullscreen();

    swc_prog_use(&p->p_down);
    const SwcTarget *src = &p->luma;
    for (int i = 0; i < SWC_BLOOM_LEVELS; i++) {
        swc_target_bind(&p->down[i]);
        swc_utex(&p->p_down, "inputBuffer", 0, src->tex);
        swc_u2f(&p->p_down, "texelSize", 1.0f / (float)src->w, 1.0f / (float)src->h);
        swc_draw_fullscreen();
        src = &p->down[i];
    }

    swc_prog_use(&p->p_up);
    for (int i = SWC_BLOOM_LEVELS - 2; i >= 0; i--) {
        swc_target_bind(&p->up[i]);
        swc_utex(&p->p_up, "inputBuffer", 0, src->tex);
        swc_utex(&p->p_up, "supportBuffer", 1, p->down[i].tex);
        swc_u2f(&p->p_up, "texelSize", 1.0f / (float)src->w, 1.0f / (float)src->h);
        swc_u1f(&p->p_up, "radius", a->bloom_radius);
        swc_draw_fullscreen();
        src = &p->up[i];
    }
}

void swc_post_present(SwcPost *p, const SwcPostParams *a) {
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    bool with_bloom = a->post && a->bloom;
    if (with_bloom)
        bloom(p, a);

    rlDisableFramebuffer(); /* the window */
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    swc_prog_use(&p->p_post);
    swc_utex(&p->p_post, "inputBuffer", 0, p->hdr.tex);
    swc_utex(&p->p_post, "bloomBuffer", 1, with_bloom ? p->up[0].tex : p->hdr.tex);
    swc_u2f(&p->p_post, "resolution", (float)p->hdr.w, (float)p->hdr.h);
    swc_u2f(&p->p_post, "caOffset", a->post ? a->ca_offset[0] : 0.0f, a->post ? a->ca_offset[1] : 0.0f);
    swc_u1f(&p->p_post, "caModulation", a->ca_modulation);
    swc_u1f(&p->p_post, "bloomIntensity", with_bloom ? a->bloom_intensity : 0.0f);
    swc_u1f(&p->p_post, "uExposure", a->exposure);
    swc_u1f(&p->p_post, "uHold", a->hold);
    swc_u1f(&p->p_post, "uVignette", a->vignette);
    swc_u1f(&p->p_post, "uGrain", a->grain);
    swc_u1f(&p->p_post, "uSeed", a->seed);
    swc_u1f(&p->p_post, "uCrt", a->crt);
    swc_u1f(&p->p_post, "uRaw", a->post ? 0.0f : 1.0f);
    swc_draw_fullscreen();
    rlDisableShader();

    rlActiveTextureSlot(1);
    rlDisableTexture();
    rlActiveTextureSlot(0);
    rlDisableTexture();
    rlEnableBackfaceCulling(); /* raylib's own state again: it draws the HUD next */
    rlEnableColorBlend();
}
