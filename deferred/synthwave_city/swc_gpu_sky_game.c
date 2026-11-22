/* Synthwave City - the sky pass: the web version's sky shader on a cube dome, plus the uniforms every scene
 * program shares. */
#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"
#include "swc_shaders.h"

static SwcProg g_sky;

bool swc_sky_init(void) {
    const char *vs[] = {swc_shader_get("prelude_vs.glsl"), swc_shader_get("sky.vert")};
    const char *fs[] = {swc_shader_get("prelude_fs.glsl"), "#define SKY_DETAIL 1",
                        swc_shader_get("common.glsl"), swc_shader_get("sky.frag")};
    return swc_prog_build(&g_sky, "sky", vs, 2, fs, 4);
}

void swc_sky_free(void) {
    swc_prog_free(&g_sky);
}

void swc_set_shared_uniforms(SwcProg *p, const SwcFrameUniforms *f, const SwcLook *look) {
    float camera_position[3] = {0.0f, f->cam_y, 0.0f};
    swc_u1f(p, "uTime", f->time);
    swc_u1f(p, "uLoop", f->loop);
    swc_u4f(p, "uBeat", f->beat);
    swc_u3f(p, "uCam", f->cam);
    swc_u2f(p, "uFar", f->far_fade[0], f->far_fade[1]);
    swc_u4f(p, "uFog", look->fog);
    swc_u3f(p, "uFogWarm", look->fog_warm);
    swc_u3f(p, "uFogCool", look->fog_cool);
    swc_u3f(p, "uSunDir", look->sun_dir);
    swc_u3f(p, "uSkyTop", look->sky_top);
    swc_u3f(p, "uSkyMid", look->sky_mid);
    swc_u3f(p, "uSkyHorizon", look->sky_horizon);
    swc_u3f(p, "uSunTop", look->sun_top);
    swc_u3f(p, "uSunBottom", look->sun_bottom);
    swc_u3f(p, "uAmbient", look->ambient);
    swc_u3fv(p, "uPalette", &look->palette[0][0], SWC_LOOK_PALETTE);
    swc_u1f(p, "uGlow", look->glow);
    swc_u1f(p, "uStars", look->stars);
    swc_u1f(p, "uWet", f->wet);
    swc_u1f(p, "uMotion", f->motion);
    swc_u1f(p, "uExposure", look->exposure);
    swc_u2f(p, "uResolution", f->resolution[0], f->resolution[1]);
    swc_um4(p, "viewMatrix", f->view);
    swc_um4(p, "projectionMatrix", f->proj);
    swc_u3f(p, "cameraPosition", camera_position);
}

void swc_sky_draw(const SwcFrameUniforms *f, const SwcLook *look) {
    if (g_sky.id == 0)
        return;
    rlDrawRenderBatchActive(); /* what raylib has queued goes first */
    rlDisableColorBlend();
    rlEnableDepthTest();
    rlDisableDepthMask();
    swc_prog_use(&g_sky);
    swc_set_shared_uniforms(&g_sky, f, look);
    swc_draw_cube();
    rlDisableShader();
    rlEnableDepthMask();
    rlEnableColorBlend();
}
