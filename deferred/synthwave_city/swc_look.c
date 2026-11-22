/* Synthwave City - moods, easing and camera matrices. See swc_look.h. */
#include "swc_look.h"

#include <math.h>
#include <string.h>

#define EASE_RATE 1.2 /* src/engine/uniforms.ts: damp(1.2, dt) */

static const unsigned PALETTE_HEX[SWC_LOOK_PALETTE] = {0xff2bd6, 0x19f0ff, 0x9b4dff, 0xff7a1a,
                                                       0xffe14d, 0x49ff7a, 0xff2740, 0xdff4ff};

void swc_hex_to_linear(unsigned hex, float gain, float out[3]) {
    for (int i = 0; i < 3; i++) {
        float c = (float)((hex >> (16 - 8 * i)) & 255u) / 255.0f;
        out[i] = (c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f)) * gain;
    }
}

static void set3(float v[3], float a, float b, float c) {
    v[0] = a;
    v[1] = b;
    v[2] = c;
}

static void unit_dir(float v[3], float x, float y, float z) {
    float l = sqrtf(x * x + y * y + z * z);
    set3(v, x / l, y / l, z / l);
}

void swc_mood_get(int id, SwcMood *m) {
    memset(m, 0, sizeof(*m));
    switch (id) {
    case 1:
        m->name = "Midnight";
        swc_hex_to_linear(0x010210, 1.0f, m->sky_top);
        swc_hex_to_linear(0x07103a, 0.9f, m->sky_mid);
        swc_hex_to_linear(0x2a2a9c, 0.9f, m->sky_horizon);
        swc_hex_to_linear(0xe8f4ff, 3.0f, m->sun_top);
        swc_hex_to_linear(0x56c8ff, 2.4f, m->sun_bottom);
        unit_dir(m->sun_dir, 0.35f, 0.16f, -1.0f);
        swc_hex_to_linear(0x2c3aa8, 0.4f, m->fog_warm);
        swc_hex_to_linear(0x100e40, 0.5f, m->fog_cool);
        m->fog[0] = 0.00009f, m->fog[1] = 0.00045f, m->fog[2] = 1.0f / 300.0f, m->fog[3] = 0.95f;
        swc_hex_to_linear(0x0a0c38, 0.5f, m->ambient);
        m->stars = 1.6f, m->exposure = 1.1f, m->glow = 1.1f;
        set3(m->tint, 0.9f, 1.0f, 1.15f);
        break;
    case 2:
        m->name = "Miami";
        swc_hex_to_linear(0x1b0a52, 1.0f, m->sky_top);
        swc_hex_to_linear(0xb03aa8, 0.9f, m->sky_mid);
        swc_hex_to_linear(0xffb07a, 1.2f, m->sky_horizon);
        swc_hex_to_linear(0xfff3a0, 6.0f, m->sun_top);
        swc_hex_to_linear(0xff5a5a, 4.5f, m->sun_bottom);
        unit_dir(m->sun_dir, -0.25f, 0.11f, -1.0f);
        swc_hex_to_linear(0xd0607a, 0.5f, m->fog_warm);
        swc_hex_to_linear(0x46309a, 0.5f, m->fog_cool);
        m->fog[0] = 0.00013f, m->fog[1] = 0.0006f, m->fog[2] = 1.0f / 240.0f, m->fog[3] = 0.96f;
        swc_hex_to_linear(0x3a1a66, 0.6f, m->ambient);
        m->stars = 0.35f, m->exposure = 0.92f, m->glow = 0.95f;
        set3(m->tint, 1.05f, 1.0f, 0.95f);
        break;
    default:
        m->name = "Sunset";
        swc_hex_to_linear(0x06021c, 1.0f, m->sky_top);
        swc_hex_to_linear(0x3a0d6e, 0.9f, m->sky_mid);
        swc_hex_to_linear(0xff3f8e, 1.1f, m->sky_horizon);
        swc_hex_to_linear(0xffe85a, 6.0f, m->sun_top);
        swc_hex_to_linear(0xff2f8a, 4.0f, m->sun_bottom);
        unit_dir(m->sun_dir, 0.0f, 0.075f, -1.0f);
        swc_hex_to_linear(0xa8247c, 0.3f, m->fog_warm);
        swc_hex_to_linear(0x2c1670, 0.45f, m->fog_cool);
        m->fog[0] = 0.00011f, m->fog[1] = 0.00055f, m->fog[2] = 1.0f / 260.0f, m->fog[3] = 0.96f;
        swc_hex_to_linear(0x20104a, 0.5f, m->ambient);
        m->stars = 0.8f, m->exposure = 1.0f, m->glow = 1.0f;
        set3(m->tint, 1.0f, 1.0f, 1.0f);
        break;
    }
}

static int clamp_mood(int mood) {
    return mood >= 0 && mood < SWC_LOOK_MOODS ? mood : 0;
}

void swc_look_init(SwcLook *look, int mood) {
    memset(look, 0, sizeof(*look));
    swc_look_set_mood(look, mood, true);
}

static void ease3(float v[3], const float target[3], float k) {
    for (int i = 0; i < 3; i++)
        v[i] += (target[i] - v[i]) * k;
}

void swc_look_set_mood(SwcLook *look, int mood, bool immediate) {
    look->mood = clamp_mood(mood);
    if (immediate)
        swc_look_update(look, 1e6);
}

void swc_look_update(SwcLook *look, double dt) {
    SwcMood m;
    swc_mood_get(look->mood, &m);
    float k = dt >= 1e5 ? 1.0f : (float)(1.0 - exp(-EASE_RATE * dt));
    if (k <= 0.0f)
        return;
    ease3(look->fog_warm, m.fog_warm, k);
    ease3(look->fog_cool, m.fog_cool, k);
    ease3(look->sky_top, m.sky_top, k);
    ease3(look->sky_mid, m.sky_mid, k);
    ease3(look->sky_horizon, m.sky_horizon, k);
    ease3(look->sun_top, m.sun_top, k);
    ease3(look->sun_bottom, m.sun_bottom, k);
    ease3(look->ambient, m.ambient, k);
    ease3(look->sun_dir, m.sun_dir, k);
    float l = sqrtf(look->sun_dir[0] * look->sun_dir[0] + look->sun_dir[1] * look->sun_dir[1] +
                    look->sun_dir[2] * look->sun_dir[2]);
    if (l > 1e-6f)
        for (int i = 0; i < 3; i++)
            look->sun_dir[i] /= l;
    for (int i = 0; i < 4; i++)
        look->fog[i] += (m.fog[i] - look->fog[i]) * k;
    look->glow += (m.glow - look->glow) * k;
    look->stars += (m.stars - look->stars) * k;
    look->exposure += (m.exposure - look->exposure) * k;
    for (int p = 0; p < SWC_LOOK_PALETTE; p++) {
        float base[3], target[3];
        swc_hex_to_linear(PALETTE_HEX[p], 1.0f, base);
        for (int c = 0; c < 3; c++)
            target[c] = base[c] * m.tint[c];
        ease3(look->palette[p], target, k);
    }
}

void swc_view_matrix(double yaw, double pitch, double eye_y, float out[16]) {
    double cy = cos(yaw), sy = sin(yaw), cp = cos(pitch), sp = sin(pitch);
    double right[3] = {cy, 0.0, -sy};
    double up[3] = {sy * sp, cp, cy * sp};
    double back[3] = {sy * cp, -sp, cy * cp};
    const double *row[3] = {right, up, back};
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++)
            out[c * 4 + r] = (float)row[r][c];
        out[3 * 4 + r] = (float)(-row[r][1] * eye_y); /* the eye is at (0, eye_y, 0) */
    }
    out[3] = out[7] = out[11] = 0.0f;
    out[15] = 1.0f;
}

void swc_proj_matrix(double fov_deg, double aspect, double near_plane, double far_plane, float out[16]) {
    double t = tan(fov_deg * 0.5 * 3.14159265358979323846 / 180.0);
    memset(out, 0, 16 * sizeof(float));
    out[0] = (float)(1.0 / (aspect * t));
    out[5] = (float)(1.0 / t);
    out[10] = (float)(-(far_plane + near_plane) / (far_plane - near_plane));
    out[11] = -1.0f;
    out[14] = (float)(-2.0 * far_plane * near_plane / (far_plane - near_plane));
}

void swc_mat4_mul(const float a[16], const float b[16], float out[16]) {
    float r[16];
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            double sum = 0.0;
            for (int k = 0; k < 4; k++)
                sum += (double)a[k * 4 + row] * (double)b[c * 4 + k];
            r[c * 4 + row] = (float)sum;
        }
    memcpy(out, r, sizeof(r));
}

void swc_mirror_view_matrix(double yaw, double pitch, double eye_y, float out[16]) {
    swc_view_matrix(yaw, -pitch, -eye_y, out);
}

void swc_mirror_sample_matrix(const float proj[16], const float mirror_view[16], float out[16]) {
    static const float bias[16] = {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 0.5f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f};
    float pv[16];
    swc_mat4_mul(proj, mirror_view, pv);
    swc_mat4_mul(bias, pv, out);
}
