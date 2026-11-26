/* Synthwave City - the GPU frame. Sky, post-processing and moods follow the web version's shaders; the city
 * itself is still drawn by the CPU renderer's geometry (synthwave_city_render_game.c) into the HDR target
 * until its layers get shaders of their own.
 *
 *   HDR target <- traffic (legacy) <- buildings, lamp posts, craft hulls, cars, people, ground (GPU) <- sky
 *   (open pixels only) <- aviation lights, craft lights, beams, car lamps and road light, lamp glows <- the
 *   legacy glows
 *   bloom (7-level mip blur) -> last pass: aberration, bloom, grade, sRGB, dither -> the window
 */
#include <math.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"
#include "swc_gpu.h"

#define NEAR_PLANE 1.0 /* the web version's camera: near 1, far 14000, vertical field of view 62 */
#define FAR_PLANE 14000.0

#define STARTUP_BUDGET_MS 6.0 /* per frame, for the ad atlas's steps in a real window */

static struct {
    bool ready;
    SwcPost post;
    SwcLook look;
    bool look_ready;
    bool have_time;
    double last_time;
    unsigned frame;
    SwcFrameUniforms fu;
    SwcGpuDebug dbg;
    bool have_buildings; /* false: the buildings shader did not build, the CPU geometry draws them */
    bool have_beams;     /* false: the beams shaders did not build, the CPU beams draw instead */
    bool have_ground;    /* false: the ground shaders did not build, the CPU ground draws instead */
    bool have_air;       /* false: the air traffic shaders did not build, the CPU craft draw instead */
    bool have_street;    /* false: the street life shaders did not build, the CPU cars draw instead */
    bool have_holo;      /* false: the hologram shaders did not build, the CPU panels draw instead */
    SwcTarget refl;      /* the scene from under the ground, for the wet streets */
    bool have_refl;
    SwcFrameUniforms mfu; /* the mirror camera's frame */
    SwcReflection mirror;
    SwcTier tier;           /* the quality tier: what swc_gpu_set_tier sets */
    SwcAdaptiveScale scale; /* the adaptive render scale of that tier */
    bool interactive;       /* a real window: adaptive scale, start-up work spread over frames */
    double real_dt;         /* the last frame's real duration, seconds (swc_gpu_note_frame) */
    int post_msaa;          /* the MSAA samples the post chain was asked for */
    SwcTier applied_tier;   /* the tier the targets, the sky and the scale bounds were last set for */
    int frame_w, frame_h;   /* the scene's size: the window's, by pixel ratio cap and render scale */
} G;

static int g_gpu_users; /* swc_gpu_init() calls not yet matched by a shutdown: the hub backdrop and a game
                           started from the archive can both be using the GPU renderer at the same time */

/* (Re)creates the mirror target for a scene of w x h; a tier without a mirror (or a GPU that cannot make one)
 * leaves the streets showing none. */
static void reflection_target(const SwcQuality *q, int w, int h) {
    swc_target_destroy(&G.refl);
    G.have_refl = false;
    int mw, mh;
    swc_quality_mirror_size(q, w, h, &mw, &mh);
    if (mw == 0)
        return; /* "low": the web version has no reflection pass either */
    G.have_refl = swc_target_create_mipped(&G.refl, mw, mh, true);
    if (!G.have_refl)
        TraceLog(LOG_WARNING, "SWC: no reflection target; the wet streets will not mirror the city");
}

/* The scene's size and the MSAA the tier asks for, given the window and the adaptive scale. */
static void scene_size(const SwcQuality *q, int *w, int *h) {
    swc_quality_render_size(q, GetScreenWidth(), GetScreenHeight(), GetRenderWidth(), GetRenderHeight(),
                            G.scale.scale, w, h);
}

bool swc_gpu_available(void) {
    int v = rlGetVersion();
    return v == RL_OPENGL_33 || v == RL_OPENGL_43;
}

bool swc_gpu_init(void) {
    if (g_gpu_users++ > 0)
        return G.ready; /* somebody already built the shaders and FBOs: share them */
    if (!swc_gpu_available()) {
        g_gpu_users--;
        return false;
    }
    memset(&G, 0, sizeof(G));
    G.dbg.layers = 255;
    G.dbg.reflect = true;
    G.dbg.post = true;
    G.dbg.bloom = true;
    G.dbg.msaa = true;
    G.tier = SWC_TIER_HIGH;
    G.applied_tier = G.tier;
    const SwcQuality *q = swc_quality(G.tier);
    swc_adaptive_init(&G.scale, q->min_render_scale, q->max_render_scale);
    scene_size(q, &G.frame_w, &G.frame_h);
    G.post_msaa = q->msaa;
    if (!swc_geometry_init() || !swc_sky_init() ||
        !swc_post_init(&G.post, G.frame_w, G.frame_h, G.post_msaa)) {
        swc_post_free(&G.post);
        swc_sky_free();
        swc_geometry_free();
        g_gpu_users--;
        TraceLog(LOG_WARNING, "SWC: the GPU renderer could not start; using the CPU renderer");
        return false;
    }
    G.have_buildings = swc_buildings_init();
    if (!G.have_buildings) {
        swc_buildings_free();
        TraceLog(LOG_WARNING,
                 "SWC: the buildings shader did not build; the CPU geometry draws the buildings");
    }
    G.have_beams = swc_beams_init();
    if (!G.have_beams) {
        swc_beams_free();
        TraceLog(LOG_WARNING, "SWC: the beams shaders did not build; the CPU beams are drawn instead");
    }
    G.have_ground = swc_ground_init();
    if (!G.have_ground) {
        swc_ground_free();
        TraceLog(LOG_WARNING, "SWC: the ground shaders did not build; the CPU ground is drawn instead");
    }
    G.have_air = swc_airtraffic_init();
    if (!G.have_air) {
        swc_airtraffic_free();
        TraceLog(LOG_WARNING, "SWC: the air traffic shaders did not build; the CPU craft are drawn instead");
    }
    G.have_street = swc_streetlife_init();
    if (!G.have_street) {
        swc_streetlife_free();
        TraceLog(LOG_WARNING, "SWC: the street life shaders did not build; the CPU cars are drawn instead");
    }
    G.have_holo = swc_holograms_init();
    if (!G.have_holo) {
        swc_holograms_free();
        TraceLog(LOG_WARNING, "SWC: the hologram shaders did not build; the CPU panels are drawn instead");
    }
    reflection_target(q, G.frame_w, G.frame_h);
    G.ready = true;
    return true;
}

void swc_gpu_set_tier(SwcTier tier) {
    if ((int)tier < 0 || (int)tier >= SWC_TIER_COUNT)
        tier = SWC_TIER_HIGH;
    G.tier = tier; /* the next frame notices (swc_render_scene_gpu) */
}

SwcTier swc_gpu_tier(void) {
    return G.tier;
}

void swc_gpu_set_interactive(bool on) {
    G.interactive = on;
    swc_holograms_set_budget(on ? STARTUP_BUDGET_MS : 0.0);
}

void swc_gpu_note_frame(double seconds) {
    G.real_dt = seconds;
}

double swc_gpu_render_scale(void) {
    return G.scale.scale;
}

int swc_gpu_msaa_samples(void) {
    return G.ready ? G.post.msaa.samples : 0;
}

void swc_gpu_shutdown(void) {
    if (g_gpu_users == 0 || --g_gpu_users > 0)
        return; /* nothing to release, or another user still needs the GPU resources */
    swc_post_free(&G.post);
    swc_target_destroy(&G.refl);
    swc_holograms_free();
    swc_streetlife_free();
    swc_airtraffic_free();
    swc_ground_free();
    swc_beams_free();
    swc_buildings_free();
    swc_sky_free();
    swc_geometry_free();
    G.ready = false;
}

void swc_gpu_set_debug(const SwcGpuDebug *d) {
    G.dbg = *d;
}

static bool gpu_buildings(void) {
    return G.have_buildings && (G.dbg.layers & 4) != 0;
}

/* "owns" is who draws a layer (the legacy renderer then leaves it out); "show" is whether it is on screen:
 * the player can switch the beams and the air traffic off with the L key, and then nobody draws them. */
static bool owns_beams(void) {
    return G.have_beams && (G.dbg.layers & 8) != 0;
}

static bool show_beams(void) {
    return owns_beams() && (swc_live_layers() & SWC_LIVE_BEAMS) != 0;
}

static bool owns_air(void) {
    return G.have_air && (G.dbg.layers & 32) != 0;
}

static bool show_air(void) {
    return owns_air() && (swc_live_layers() & SWC_LIVE_AIR) != 0;
}

static bool owns_street(void) {
    return G.have_street && (G.dbg.layers & 64) != 0;
}

static bool show_street(void) {
    return owns_street() && (swc_live_layers() & SWC_LIVE_STREET) != 0;
}

static bool owns_holo(void) {
    return G.have_holo && (G.dbg.layers & 128) != 0;
}

static bool show_holo(void) {
    return owns_holo() && (swc_live_layers() & SWC_LIVE_HOLO) != 0;
}

static bool gpu_ground(void) {
    return G.have_ground && (G.dbg.layers & 16) != 0;
}

/* The GPU layers that sit between the opaque legacy geometry and the legacy glows. */
static void gpu_layers(void *user, int w, int h) {
    (void)user;
    (void)w;
    (void)h;
    if (gpu_buildings())
        swc_buildings_draw(&G.fu, &G.look);
    if (show_air())
        swc_airtraffic_hulls_draw(&G.fu, &G.look);
    if (show_street()) { /* opaque too, and before the ground, which they hide */
        swc_streetlife_bodies_draw(&G.fu, &G.look);
        swc_streetlife_crowds_draw(&G.fu, &G.look);
    }
    if (gpu_ground()) { /* the opaque layers first: what they hide never reaches the heavy ground shader */
        swc_lamp_posts_draw(&G.fu, &G.look);
        swc_ground_draw(&G.fu, &G.look, &G.mirror);
    }
    if (G.dbg.layers & 1)
        swc_sky_draw(&G.fu, &G.look);
    if (gpu_buildings()) /* after the sky: the lights are additive and must not be painted over by it */
        swc_building_lights_draw(&G.fu, &G.look);
    if (show_air()) /* also additive: after the sky */
        swc_airtraffic_lights_draw(&G.fu, &G.look);
    if (show_beams())
        swc_beams_draw(&G.fu, &G.look);
    if (show_street()) { /* additive: the lamps, the light on the road, the far traffic */
        swc_streetlife_lamps_draw(&G.fu, &G.look);
        swc_streetlife_lights_draw(&G.fu, &G.look);
    }
    if (show_holo()) /* additive: the panels, the wire solids and the ticker rings */
        swc_holograms_draw(&G.fu, &G.look);
    if (gpu_ground())
        swc_lamp_glows_draw(&G.fu, &G.look);
}

/* The scene as a camera under the ground sees it, into the mirror target: everything the web version leaves
 * out of "no reflect" (not the ground itself, not the searchlights and lasers; the air traffic is mirrored).
 * Then the mip chain the ground reads it by. */
static void reflection_pass(const synthwave_city_State *s, int w, int h) {
    G.mirror.tex = 0;
    /* The web version's no-post fallback has no mirror either: the pass belongs to its post pipeline. */
    if (!G.have_refl || !G.dbg.reflect || !G.dbg.post || !gpu_ground())
        return;
    SwcFrameUniforms *m = &G.mfu;
    *m = G.fu; /* the resolution stays that of the real frame: pixel-size limits are per screen pixel */
    m->cam_y = -G.fu.cam_y;
    swc_mirror_view_matrix(s->cam.yaw, s->cam.pitch, s->roll, s->cam.y, m->view);
    swc_mirror_sample_matrix(G.fu.proj, m->view, G.mirror.matrix);

    swc_target_bind(&G.refl);
    rlEnableDepthMask();
    rlClearColor(0, 0, 0, 255);
    rlClearScreenBuffers();
    if (gpu_buildings())
        swc_buildings_draw(m, &G.look);
    swc_lamp_posts_draw(m, &G.look);
    if (show_air())
        swc_airtraffic_hulls_draw(m, &G.look);
    if (show_street())
        swc_streetlife_bodies_draw(m, &G.look);
    if (G.dbg.layers & 1)
        swc_sky_draw(m, &G.look);
    if (gpu_buildings())
        swc_building_lights_draw(m, &G.look);
    if (show_air())
        swc_airtraffic_lights_draw(m, &G.look);
    if (show_street())
        swc_streetlife_lamps_draw(m, &G.look);
    if (show_beams())
        swc_beams_draw_pillars(m, &G.look);
    if (show_holo())
        swc_holograms_draw(m, &G.look);
    swc_lamp_glows_draw(m, &G.look);
    rlDrawRenderBatchActive();
    rlDisableFramebuffer();
    rlViewport(0, 0, w, h);
    swc_target_make_mips(&G.refl);
    G.mirror.tex = G.refl.tex;
}

static void frame_uniforms(const synthwave_city_State *s, const SwcQuality *q, int w, int h) {
    SwcFrameUniforms *f = &G.fu;
    memset(f, 0, sizeof(*f));
    f->time = (float)s->clock;
    f->loop = (float)swc_loop_fraction(s);
    double beat = s->beat_time * SWC_BPM / 60.0;
    double in_beat = beat - floor(beat);
    double fall = 1.0 - in_beat;
    f->beat[0] = (float)(fall * fall * fall);
    f->beat[1] = (float)(beat / 4.0 - floor(beat / 4.0));
    f->beat[2] = (float)fmod(floor(beat), 16.0);
    f->beat[3] = 1.0f;
    f->cam[0] = (float)s->cam.x;
    f->cam[1] = (float)s->cam.y;
    f->cam[2] = (float)s->cam.z;
    f->cam_y = (float)s->cam.y;
    f->far_fade[0] = (float)q->fade_start;
    f->far_fade[1] = (float)q->fade_end;
    swc_view_matrix(s->cam.yaw, s->cam.pitch, s->roll, s->cam.y, f->view);
    swc_proj_matrix(s->fov, (double)w / (double)h, NEAR_PLANE, FAR_PLANE, f->proj);
    f->resolution[0] = (float)w;
    f->resolution[1] = (float)h;
    f->motion = 1.0f;
    f->wet = 1.0f;
}

void swc_render_scene_gpu(const synthwave_city_State *s, double time_s) {
    if (!G.ready)
        return;
    double dt = 0.0;
    if (G.have_time) {
        dt = time_s - G.last_time;
        dt = dt < 0.0 ? 0.0 : (dt > 0.1 ? 0.1 : dt);
    }
    G.have_time = true;
    G.last_time = time_s;

    /* The tier, and the render scale that adapts to how long frames really take. */
    const SwcQuality *q = swc_quality(G.tier);
    bool tier_changed = q->tier != G.applied_tier;
    if (tier_changed) {
        G.applied_tier = q->tier;
        swc_adaptive_set_range(&G.scale, q->min_render_scale, q->max_render_scale);
        swc_sky_set_detail(q->tier != SWC_TIER_LOW);
    }
    if (G.interactive && G.real_dt > 0.0) {
        double real = G.real_dt > 0.1 ? 0.1 : G.real_dt;
        swc_adaptive_update(&G.scale, real * 1000.0, real);
    }
    int w, h;
    scene_size(q, &w, &h);
    int msaa = G.dbg.msaa ? q->msaa : 0;
    if (w != G.post.w || h != G.post.h || msaa != G.post_msaa) { /* the window, the scale or MSAA changed */
        swc_post_free(&G.post);
        G.post_msaa = msaa;
        if (!swc_post_init(&G.post, w, h, msaa)) {
            G.ready = false;
            return;
        }
        tier_changed = true; /* the mirror follows the frame's size */
    }
    if (tier_changed)
        reflection_target(q, w, h);

    if (!G.look_ready) {
        swc_look_init(&G.look, s->mood);
        G.look_ready = true;
    } else {
        if (G.look.mood != s->mood)
            swc_look_set_mood(&G.look, s->mood, false);
        swc_look_update(&G.look, dt);
    }
    frame_uniforms(s, q, w, h);

    SwcPostParams pp = swc_post_defaults();
    pp.exposure = G.look.exposure;
    pp.seed = (float)(G.frame % 251u) / 251.0f;
    pp.post = G.dbg.post;
    pp.bloom = G.dbg.bloom;

    if (G.have_buildings)
        swc_buildings_sync(&s->city);
    if (G.have_beams)
        swc_beams_sync(&s->city, q);
    if (G.have_ground)
        swc_ground_sync(&s->city, q);
    if (G.have_air)
        swc_airtraffic_sync(&s->city, q);
    if (G.have_street)
        swc_streetlife_sync(&s->city, q);
    if (G.have_holo)
        swc_holograms_sync(&s->city, q);

    reflection_pass(s, w, h);
    swc_post_scene_begin(&G.post);
    if (G.dbg.layers & 2) {
        SwcLegacyOptions opt;
        memset(&opt, 0, sizeof(opt));
        opt.flags = SWC_LEGACY_NO_SKY | SWC_LEGACY_LINEAR | (gpu_buildings() ? SWC_LEGACY_NO_BUILDINGS : 0u) |
                    (owns_beams() ? SWC_LEGACY_NO_BEAMS : 0u) | (gpu_ground() ? SWC_LEGACY_NO_GROUND : 0u) |
                    (owns_air() ? SWC_LEGACY_NO_AIR : 0u) | (owns_street() ? SWC_LEGACY_NO_STREET : 0u) |
                    (owns_holo() ? SWC_LEGACY_NO_HOLO : 0u);
        opt.look = &G.look;
        opt.after_solid = gpu_layers;
        swc_render_scene_legacy(s, time_s, &opt);
    } else {
        rlDisableBackfaceCulling(); /* the dome is seen from inside; the city pass has this off already */
        gpu_layers(NULL, w, h);
        rlEnableBackfaceCulling();
    }
    swc_post_scene_end(&G.post);
    swc_post_present(&G.post, &pp);
    G.frame++;
}
