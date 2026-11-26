/* Synthwave City - the look of the GPU renderer, as plain data: the three moods in linear light, the easing
 * between them, the neon palette and the camera matrices the shaders take as uniforms. PURE C (no raylib).
 * The numbers are those of the web version (src/contracts/moods.ts, palette.ts, src/engine/uniforms.ts);
 * all colours are linear RGB with the mood's gain already applied, ready for a uniform. */
#ifndef SWC_LOOK_H
#define SWC_LOOK_H

#include <stdbool.h>

#define SWC_LOOK_PALETTE 8
#define SWC_LOOK_MOODS 3

/* One mood: the target the live values ease towards. */
typedef struct SwcMood {
    const char *name;
    float sky_top[3], sky_mid[3], sky_horizon[3];
    float sun_top[3], sun_bottom[3];
    float sun_dir[3]; /* unit vector towards the sun */
    float fog_warm[3], fog_cool[3];
    float fog[4]; /* x: density per metre, y: extra at ground level, z: height falloff per metre, w: cap */
    float ambient[3];
    float stars, exposure, glow;
    float tint[3]; /* per-channel multiplier on the palette */
} SwcMood;

/* The live values (the web version's shared uniforms): they ease towards the current mood. */
typedef struct SwcLook {
    int mood;
    float fog_warm[3], fog_cool[3];
    float sky_top[3], sky_mid[3], sky_horizon[3];
    float sun_top[3], sun_bottom[3];
    float sun_dir[3];
    float ambient[3];
    float fog[4];
    float glow, stars, exposure;
    float palette[SWC_LOOK_PALETTE][3];
} SwcLook;

/* '#rrggbb' as sRGB to linear RGB, scaled by `gain`. */
void swc_hex_to_linear(unsigned hex, float gain, float out[3]);

/* The mood table (id 0 sunset, 1 midnight, 2 miami; anything else is sunset). */
void swc_mood_get(int id, SwcMood *out);

/* Starts on a mood with every value already at its target. */
void swc_look_init(SwcLook *look, int mood);

/* Picks the mood to ease towards; `immediate` jumps there at once. */
void swc_look_set_mood(SwcLook *look, int mood, bool immediate);

/* Eases every value towards the mood by dt seconds: 1 - exp(-1.2 dt) of the way, as the web version does. */
void swc_look_update(SwcLook *look, double dt);

/* View matrix (column-major, as GLSL reads it) of a camera at (0, eye_y, 0): the world is drawn relative to
 * the camera in x and z. yaw 0 faces -z and positive yaw turns towards -x; pitch up is positive; positive
 * roll leans the top of the picture to the right (three.js "YXZ" order). */
void swc_view_matrix(double yaw, double pitch, double roll, double eye_y, float out[16]);

/* Perspective matrix (column-major, OpenGL clip space), vertical field of view in degrees. */
void swc_proj_matrix(double fov_deg, double aspect, double near_plane, double far_plane, float out[16]);

/* out = a * b for column-major 4x4 matrices (out may be a or b). */
void swc_mat4_mul(const float a[16], const float b[16], float out[16]);

/* The camera that sees what a flat mirror at height 0 shows to a camera at (0, eye_y, 0) with this yaw, pitch
 * and roll: it stands at (0, -eye_y, 0) and looks along the view direction turned over in y, with the
 * camera's up turned over too (the web version's reflection pass builds it that way). */
void swc_mirror_view_matrix(double yaw, double pitch, double roll, double eye_y, float out[16]);

/* Takes a world point (x and z relative to the camera, y absolute) to texture coordinates (x, y in 0..1, z
 * depth in 0..1) of the image a mirror camera drew: bias * projection * view. For a point on the mirror plane
 * the result is where the mirror camera saw what the real camera sees reflected there. */
void swc_mirror_sample_matrix(const float proj[16], const float mirror_view[16], float out[16]);

#endif
