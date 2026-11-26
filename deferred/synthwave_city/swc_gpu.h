/* Synthwave City - what the GPU renderer's files share. Plain types only (no raylib): the files that
 * include this header and call OpenGL are the *_game.c ones. Targets are OpenGL 3.3 core through rlgl. */
#ifndef SWC_GPU_H
#define SWC_GPU_H

#include <stdbool.h>

#include "swc_city.h"
#include "swc_look.h"
#include "swc_quality.h"
#include "synthwave_city_front.h"

/* ---- shader programs ---- */

#define SWC_PROG_UNIFORMS 48
typedef struct SwcProg {
    unsigned id; /* 0 = not loaded */
    int count;
    const char *name[SWC_PROG_UNIFORMS]; /* string literals: found again by comparing the text */
    int loc[SWC_PROG_UNIFORMS];
} SwcProg;

/* Joins the parts (each a NUL-terminated GLSL text, in order) into one vertex and one fragment shader and
 * links them. Logs the compiler's complaints and returns false if anything fails. */
bool swc_prog_build(SwcProg *p, const char *label, const char *const *vs, int vs_n, const char *const *fs,
                    int fs_n);
void swc_prog_free(SwcProg *p);
void swc_prog_use(const SwcProg *p);
/* Uniform setters: the program must be in use. A uniform the shader does not use is skipped quietly. */
void swc_u1i(SwcProg *p, const char *name, int v);
void swc_u1f(SwcProg *p, const char *name, float v);
void swc_u2f(SwcProg *p, const char *name, float x, float y);
void swc_u3f(SwcProg *p, const char *name, const float v[3]);
void swc_u4f(SwcProg *p, const char *name, const float v[4]);
void swc_u2fv(SwcProg *p, const char *name, const float *v, int count);
void swc_u3fv(SwcProg *p, const char *name, const float *v, int count);
void swc_u4fv(SwcProg *p, const char *name, const float *v, int count);
void swc_um4(SwcProg *p, const char *name, const float m[16]);
/* Binds a texture to a unit and points the sampler at it. */
void swc_utex(SwcProg *p, const char *name, int unit, unsigned texture);

/* ---- render targets ---- */

typedef struct SwcTarget {
    unsigned fbo, tex, depth; /* depth is a renderbuffer, 0 when there is none */
    int w, h;
} SwcTarget;

/* A 16-bit float RGBA target, linearly filtered and clamped. */
bool swc_target_create(SwcTarget *t, int w, int h, bool with_depth);
/* The same, read through a mip chain (trilinear): swc_target_make_mips builds the chain once it is drawn. */
bool swc_target_create_mipped(SwcTarget *t, int w, int h, bool with_depth);
void swc_target_make_mips(const SwcTarget *t);
void swc_target_destroy(SwcTarget *t);
void swc_target_bind(const SwcTarget *t); /* draws into it, viewport and all */

/* ---- shared geometry ---- */

bool swc_geometry_init(void); /* the full-screen triangle and the unit cube */
void swc_geometry_free(void);
void swc_draw_fullscreen(void);
void swc_draw_cube(void); /* a 2x2x2 cube centred on the origin, 36 vertices, both faces drawn */

/* ---- the sky pass (swc_gpu_sky_game.c) ---- */

typedef struct SwcFrameUniforms {
    float time;        /* uTime: seconds on the traffic loop */
    float loop;        /* uLoop: time / 4096 */
    float beat[4];     /* uBeat: pulse, bar fraction, beat in the bar, energy */
    float cam[3];      /* uCam: tile position */
    float cam_y;       /* cameraPosition.y */
    float far_fade[2]; /* uFar */
    float view[16], proj[16];
    float resolution[2];
    float motion; /* uMotion */
    float wet;    /* uWet */
} SwcFrameUniforms;

bool swc_sky_init(void);
void swc_sky_free(void);
/* The web version's sky has a plainer star band and cloud streak on its "low" tier (SKY_DETAIL 0): false
 * picks that variant, which is built the first time it is asked for. */
void swc_sky_set_detail(bool full);
/* Draws the sky dome on the far plane of the target that is bound. Depth-tested, so only open sky is painted.
 */
void swc_sky_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* Sets the uniforms every scene program shares (the names are those of the web version's shared uniforms). */
void swc_set_shared_uniforms(SwcProg *p, const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the buildings pass (swc_gpu_buildings_game.c) ---- */

bool swc_buildings_init(void);
void swc_buildings_free(void);
/* Packs the plan's boxes and roof lights and uploads them, once per plan (a new seed packs again). */
void swc_buildings_sync(const SwcCity *city);
/* Every box of the city as one instance of a unit box or cylinder; the whole facade is the fragment shader's.
 * Opaque, depth-tested and depth-writing: draw it before the sky. */
void swc_buildings_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* The aviation lights: additive sprites, depth-tested, no depth write: draw them after the sky. */
void swc_building_lights_draw(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the beams pass (swc_gpu_beams_game.c) ---- */

bool swc_beams_init(void);
void swc_beams_free(void);
/* Packs the plan's searchlights, pillar beacons and laser fans and uploads them, once per plan and tier. */
void swc_beams_sync(const SwcCity *city, const SwcQuality *q);
/* Beam ribbons and emitter flares: additive, depth-tested, no depth write: draw them after the sky. */
void swc_beams_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* Only the beacon pillars, the one set the wet streets mirror. */
void swc_beams_draw_pillars(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the ground pass (swc_gpu_ground_game.c) ---- */

/* The mirror the wet streets show: the scene drawn from under the ground, and the matrix that takes a world
 * point (camera-relative x and z, height) to its place in that image. tex = 0 means no mirror. */
typedef struct SwcReflection {
    unsigned tex;
    float matrix[16];
} SwcReflection;

bool swc_ground_init(void);
void swc_ground_free(void);
/* Uploads the plan's 40 x 40 cells for the streets shader, once per plan, and puts the lamps at the spacing
 * of the tier's density (again when the tier changes). */
void swc_ground_sync(const SwcCity *city, const SwcQuality *q);
/* The streets, markings, plazas and wet mirror: opaque and depth-writing; draw it after the other opaque
 * layers so that what a building hides is rejected by the depth test before this (heavy) shader runs. Without
 * a mirror (refl->tex == 0, the "low" tier) a lighter variant of the shader runs, as the web version compiles
 * one without G_REFLECT. */
void swc_ground_draw(const SwcFrameUniforms *f, const SwcLook *look, const SwcReflection *refl);
/* Lamp posts: opaque. */
void swc_lamp_posts_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* One glow sprite per lamp: additive, depth-tested, no depth write: draw it after the sky. */
void swc_lamp_glows_draw(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the air traffic pass (swc_gpu_air_game.c) ---- */

bool swc_airtraffic_init(void);
void swc_airtraffic_free(void);
/* Packs the plan's craft and hero ships and uploads them, once per plan and tier (a new seed packs again). */
void swc_airtraffic_sync(const SwcCity *city, const SwcQuality *q);
/* The dark hulls of the small craft and of the hero ships: opaque, depth-tested and depth-writing. */
void swc_airtraffic_hulls_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* Engine glows, lamps, strobes, trails, hero lights, searchlight cones and pools: additive, depth-tested, no
 * depth write: draw them after the sky. */
void swc_airtraffic_lights_draw(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the street life pass (swc_gpu_street_game.c) ---- */

bool swc_streetlife_init(void);
void swc_streetlife_free(void);
/* Packs the plan's lane table and uploads it with the grids of near cars and people, once per plan and tier
 * (the tier sets the traffic, the crowd and where the near cars hand over to the far traffic). */
void swc_streetlife_sync(const SwcCity *city, const SwcQuality *q);
/* The cars: opaque, depth-tested and depth-writing. The wet streets mirror them. */
void swc_streetlife_bodies_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* The people on the sidewalks: opaque. Not mirrored. */
void swc_streetlife_crowds_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* Headlights, tail lights and roof beacons: additive, depth-tested, no depth write: draw them after the sky.
 * The wet streets mirror them. */
void swc_streetlife_lamps_draw(const SwcFrameUniforms *f, const SwcLook *look);
/* The light cars throw on the road, and the far traffic (every car past the handover as a pair of dashes):
 * additive, depth-tested, no depth write: draw them after the sky. Not mirrored. */
void swc_streetlife_lights_draw(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- the holograms pass (swc_gpu_holo_game.c) ---- */

bool swc_holograms_init(void);
void swc_holograms_free(void);
/* Draws the ad atlas by code (about 0.15 s), packs the plan's ad panels, ticker rings and wire solids, and
 * uploads them, once per plan and tier (a new seed or tier does it again; the atlas is drawn only once). */
void swc_holograms_sync(const SwcCity *city, const SwcQuality *q);
/* Milliseconds per frame the atlas's steps may take (drawing the ads, making the half floats, uploading,
 * mip maps): a real window spreads them over frames and the ads appear when they are done; 0, the default,
 * does everything in the first sync. */
void swc_holograms_set_budget(double ms);
/* Ad panels, wire solids and ticker rings: additive, depth-tested, no depth write: draw them after the sky.
 * The wet streets mirror them. */
void swc_holograms_draw(const SwcFrameUniforms *f, const SwcLook *look);

/* ---- post-processing (swc_gpu_post_game.c) ---- */

#define SWC_BLOOM_LEVELS 7

/* A multisampled colour + depth target the scene is drawn into when the tier asks for MSAA; it is resolved
 * into SwcPost.hdr (the web version's composer does the same with `multisampling`). Plain OpenGL 3.3, loaded
 * through rlGetProcAddress because rlgl has no multisampled renderbuffers. */
typedef struct SwcMsaa {
    unsigned fbo, color, depth; /* 0: none */
    int samples;
} SwcMsaa;

typedef struct SwcPost {
    bool ready;
    int w, h;
    SwcTarget hdr;                      /* the scene (with depth, unless the MSAA target holds it) */
    SwcMsaa msaa;                       /* samples == 0: the scene is drawn straight into hdr */
    SwcTarget luma;                     /* the bright part of it, full size */
    SwcTarget down[SWC_BLOOM_LEVELS];   /* each half the size of the one before */
    SwcTarget up[SWC_BLOOM_LEVELS - 1]; /* the way back up; up[0] is the bloom */
    SwcProg p_luma, p_down, p_up, p_post;
} SwcPost;

typedef struct SwcPostParams {
    float exposure;
    float vignette;
    float grain;
    float hold;
    float crt;
    float ca_offset[2];
    float ca_modulation;
    float bloom_threshold, bloom_smoothing, bloom_radius, bloom_intensity;
    float seed;
    bool bloom;
    bool post; /* false: the web version's no-post fallback (ACES, gamma 2.2): for comparing with it */
} SwcPostParams;

/* `msaa` is the samples the tier asks for (0 = none). If the GPU or driver cannot do that many, fewer are
 * used, and with none of them the scene is drawn without MSAA: see p->msaa.samples for what was made. */
bool swc_post_init(SwcPost *p, int w, int h, int msaa);
void swc_post_free(SwcPost *p);
/* Draws the scene into the HDR target from here to swc_post_scene_end: clears colour and depth. */
void swc_post_scene_begin(SwcPost *p);
void swc_post_scene_end(SwcPost *p);
/* Bloom and the last pass, into the framebuffer that was active before swc_post_scene_begin. */
void swc_post_present(SwcPost *p, const SwcPostParams *params);
/* The web version's post settings (src/post/index.ts). */
SwcPostParams swc_post_defaults(void);

#endif
