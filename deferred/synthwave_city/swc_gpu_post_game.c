/* Synthwave City - post-processing: the HDR scene target, the bloom of the web version (pmndrs
 * MipmapBlurPass, 7 levels) and the last pass (chromatic aberration, bloom, grade, sRGB, dither). */
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

bool swc_post_init(SwcPost *p, int w, int h) {
    memset(p, 0, sizeof(*p));
    p->w = w;
    p->h = h;
    bool ok = build_programs(p) && swc_target_create(&p->hdr, w, h, true) &&
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

static RenderTexture2D hdr_as_render_texture(const SwcPost *p) {
    RenderTexture2D rt;
    memset(&rt, 0, sizeof(rt));
    rt.id = p->hdr.fbo;
    rt.texture.id = p->hdr.tex;
    rt.texture.width = p->hdr.w;
    rt.texture.height = p->hdr.h;
    rt.texture.mipmaps = 1;
    rt.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rt.depth.id = p->hdr.depth;
    rt.depth.width = p->hdr.w;
    rt.depth.height = p->hdr.h;
    return rt;
}

void swc_post_scene_begin(SwcPost *p) {
    BeginTextureMode(hdr_as_render_texture(p));
    ClearBackground(BLACK); /* colour and depth */
}

void swc_post_scene_end(SwcPost *p) {
    (void)p;
    EndTextureMode();
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
