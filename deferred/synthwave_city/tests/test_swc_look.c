/* The look of the GPU renderer: mood table, easing between moods, and the camera matrices. */
#include "arcade_test.h"
#include "swc_look.h"

static void test_hex_converts_srgb_to_linear(void) {
    float c[3];
    swc_hex_to_linear(0xffffff, 1.0f, c);
    CHECK_NEAR(c[0], 1.0, 1e-6);
    CHECK_NEAR(c[2], 1.0, 1e-6);
    swc_hex_to_linear(0x808080, 1.0f, c);
    CHECK_NEAR(c[1], 0.2158605, 1e-5); /* 128/255 in sRGB */
    swc_hex_to_linear(0x0a0a0a, 2.0f, c);
    CHECK_NEAR(c[0], 2.0 * 10.0 / 255.0 / 12.92, 1e-6); /* the linear toe, times the gain */
}

static void test_moods_are_sound(void) {
    static const char *names[SWC_LOOK_MOODS] = {"Sunset", "Midnight", "Miami"};
    for (int i = 0; i < SWC_LOOK_MOODS; i++) {
        SwcMood m;
        swc_mood_get(i, &m);
        CHECK(m.name != NULL && m.name[0] == names[i][0]);
        double l = m.sun_dir[0] * m.sun_dir[0] + m.sun_dir[1] * m.sun_dir[1] + m.sun_dir[2] * m.sun_dir[2];
        CHECK_NEAR(l, 1.0, 1e-6);
        CHECK(m.sun_dir[1] > 0.0f &&
              m.sun_dir[2] < 0.0f); /* above the horizon, ahead of a camera facing -z */
        CHECK(m.fog[3] > 0.9f && m.fog[3] <= 1.0f);
        CHECK(m.exposure > 0.5f && m.exposure < 1.5f);
        CHECK(m.sun_top[0] + m.sun_top[1] + m.sun_top[2] > 1.0f); /* the sun is HDR */
    }
    SwcMood bad, sunset;
    swc_mood_get(99, &bad);
    swc_mood_get(0, &sunset);
    CHECK(bad.name == sunset.name || bad.name[0] == 'S');
}

static void test_a_new_look_sits_on_its_mood(void) {
    for (int i = 0; i < SWC_LOOK_MOODS; i++) {
        SwcLook look;
        SwcMood m;
        swc_look_init(&look, i);
        swc_mood_get(i, &m);
        CHECK_EQ_INT(look.mood, i);
        CHECK_NEAR(look.sky_top[2], m.sky_top[2], 1e-7);
        CHECK_NEAR(look.sun_top[1], m.sun_top[1], 1e-6);
        CHECK_NEAR(look.fog[1], m.fog[1], 1e-9);
        CHECK_NEAR(look.exposure, m.exposure, 1e-6);
        float c[3];
        swc_hex_to_linear(0x19f0ff, 1.0f, c); /* palette 1: cyan, tinted by the mood */
        CHECK_NEAR(look.palette[1][2], c[2] * m.tint[2], 1e-6);
    }
}

static void test_easing_moves_a_share_of_the_way_per_step(void) {
    SwcLook look;
    SwcMood sunset, midnight;
    swc_mood_get(0, &sunset);
    swc_mood_get(1, &midnight);
    swc_look_init(&look, 0);
    swc_look_set_mood(&look, 1, false);
    CHECK_NEAR(look.exposure, sunset.exposure, 1e-9); /* nothing moves until time passes */
    swc_look_update(&look, 0.5);
    double k = 1.0 - exp(-1.2 * 0.5);
    CHECK_NEAR(look.exposure, sunset.exposure + (midnight.exposure - sunset.exposure) * k, 1e-5);
    CHECK_NEAR(look.stars, sunset.stars + (midnight.stars - sunset.stars) * k, 1e-5);
}

static void test_easing_converges_and_keeps_the_sun_a_unit_vector(void) {
    SwcLook look;
    SwcMood m;
    swc_look_init(&look, 0);
    swc_look_set_mood(&look, 2, false);
    for (int i = 0; i < 2000; i++) {
        swc_look_update(&look, 1.0 / 60.0);
        double l = look.sun_dir[0] * look.sun_dir[0] + look.sun_dir[1] * look.sun_dir[1] +
                   look.sun_dir[2] * look.sun_dir[2];
        CHECK_NEAR(l, 1.0, 1e-5);
    }
    swc_mood_get(2, &m);
    CHECK_NEAR(look.sky_mid[0], m.sky_mid[0], 1e-4);
    CHECK_NEAR(look.sun_dir[0], m.sun_dir[0], 1e-4);
    CHECK_NEAR(look.fog[0], m.fog[0], 1e-7);
}

static void test_easing_does_not_depend_on_the_frame_rate(void) {
    SwcLook a, b;
    swc_look_init(&a, 0);
    swc_look_init(&b, 0);
    swc_look_set_mood(&a, 1, false);
    swc_look_set_mood(&b, 1, false);
    for (int i = 0; i < 60; i++)
        swc_look_update(&a, 1.0 / 60.0);
    for (int i = 0; i < 30; i++)
        swc_look_update(&b, 1.0 / 30.0);
    CHECK_NEAR(a.exposure, b.exposure, 1e-5);
    CHECK_NEAR(a.palette[3][0], b.palette[3][0], 1e-5);
}

static void mul_point(const float m[16], const double p[3], double out[4]) {
    for (int r = 0; r < 4; r++)
        out[r] = m[0 * 4 + r] * p[0] + m[1 * 4 + r] * p[1] + m[2 * 4 + r] * p[2] + m[3 * 4 + r];
}

static void test_view_matrix_maps_the_view_direction_to_minus_z(void) {
    static const double poses[4][2] = {{0.0, 0.0}, {0.7, 0.0}, {-1.2, 0.4}, {2.5, -0.9}};
    for (int i = 0; i < 4; i++) {
        double yaw = poses[i][0], pitch = poses[i][1];
        float v[16];
        swc_view_matrix(yaw, pitch, 100.0, v);
        double fwd[3] = {-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch)};
        double eye[3] = {0.0, 100.0, 0.0};
        double p[3] = {eye[0] + fwd[0] * 50.0, eye[1] + fwd[1] * 50.0, eye[2] + fwd[2] * 50.0};
        double o[4];
        mul_point(v, p, o);
        CHECK_NEAR(o[0], 0.0, 1e-3);
        CHECK_NEAR(o[1], 0.0, 1e-3);
        CHECK_NEAR(o[2], -50.0, 1e-3);
        mul_point(v, eye, o); /* the eye is the view-space origin */
        CHECK_NEAR(o[0], 0.0, 1e-3);
        CHECK_NEAR(o[1], 0.0, 1e-3);
        CHECK_NEAR(o[2], 0.0, 1e-3);
    }
}

static void test_yaw_turns_towards_minus_x(void) {
    float v[16];
    swc_view_matrix(0.5, 0.0, 0.0, v);
    double left[3] = {-10.0, 0.0, -10.0 * 1.0 / tan(0.5)}; /* straight ahead at yaw 0.5 is (-sin, 0, -cos) */
    double o[4];
    mul_point(v, left, o);
    CHECK_NEAR(o[0], 0.0, 1e-3); /* on the view axis: yaw 0.5 faces towards -x */
}

static void test_projection_maps_the_frustum_edge_to_the_screen_edge(void) {
    float p[16];
    double aspect = 16.0 / 9.0, fov = 62.0, t = tan(fov * 0.5 * 3.14159265358979323846 / 180.0);
    swc_proj_matrix(fov, aspect, 1.0, 14000.0, p);
    double top[3] = {0.0, 20.0 * t, -20.0}, side[3] = {20.0 * t * aspect, 0.0, -20.0};
    double o[4];
    mul_point(p, top, o);
    CHECK_NEAR(o[1] / o[3], 1.0, 1e-5);
    mul_point(p, side, o);
    CHECK_NEAR(o[0] / o[3], 1.0, 1e-5);
    double near_pt[3] = {0.0, 0.0, -1.0}, far_pt[3] = {0.0, 0.0, -14000.0};
    mul_point(p, near_pt, o);
    CHECK_NEAR(o[2] / o[3], -1.0, 1e-5);
    mul_point(p, far_pt, o);
    CHECK_NEAR(o[2] / o[3], 1.0, 1e-4);
    CHECK_NEAR(o[3], 14000.0, 1e-2); /* w = -z: what the sky shader relies on */
}

static void test_matrix_product_composes_in_column_major_order(void) {
    float t[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1}; /* translate by (5, 6, 7) */
    float s[16] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 0, 0, 0, 1}; /* scale by (2, 3, 4) */
    float ts[16], st[16];
    swc_mat4_mul(t, s, ts); /* scale first, then translate */
    swc_mat4_mul(s, t, st); /* translate first, then scale */
    double p[3] = {1.0, 1.0, 1.0}, o[4];
    mul_point(ts, p, o);
    CHECK_NEAR(o[0], 7.0, 1e-6);
    CHECK_NEAR(o[1], 9.0, 1e-6);
    CHECK_NEAR(o[2], 11.0, 1e-6);
    mul_point(st, p, o);
    CHECK_NEAR(o[0], 12.0, 1e-6);
    CHECK_NEAR(o[1], 21.0, 1e-6);
    CHECK_NEAR(o[2], 32.0, 1e-6);
    swc_mat4_mul(ts, s, ts); /* the output may be an input */
    mul_point(ts, p, o);
    CHECK_NEAR(o[0], 9.0, 1e-6); /* scale by 2 twice: 4, then +5 */
}

/* The texture coordinates a world point lands on under a sampling matrix. */
static void sample_uv(const float m[16], const double p[3], double uv[2]) {
    double o[4];
    mul_point(m, p, o);
    uv[0] = o[0] / o[3];
    uv[1] = o[1] / o[3];
}

static void test_mirror_view_sees_a_ground_point_where_its_reflected_ray_goes(void) {
    static const double poses[5][3] = {
        {0.0, 0.0, 2.0}, {0.7, -0.2, 40.0}, {-1.2, 0.3, 120.0}, {2.5, -0.5, 8.0}, {4.0, 0.9, 300.0}};
    float proj[16];
    swc_proj_matrix(62.0, 16.0 / 9.0, 1.0, 14000.0, proj);
    for (int i = 0; i < 5; i++) {
        double yaw = poses[i][0], pitch = poses[i][1], h = poses[i][2];
        float mv[16], m[16];
        swc_mirror_view_matrix(yaw, pitch, h, mv);
        swc_mirror_sample_matrix(proj, mv, m);
        /* a ground point P, and the scene point Q on the ray that leaves P after the camera's ray C -> P
         * turned over */
        for (int k = 0; k < 6; k++) {
            double px = -30.0 + 17.0 * k, pz = -40.0 - 23.0 * k;
            double d[3] = {px, h, pz}; /* C = (0, h, 0) -> P is (px, -h, pz); mirrored it goes up */
            double t = 0.5 + 0.9 * k;
            double q[3] = {px + t * d[0], t * d[1], pz + t * d[2]};
            double p[3] = {px, 0.0, pz}, a[2], b[2];
            sample_uv(m, p, a);
            sample_uv(m, q, b);
            CHECK_NEAR(a[0], b[0], 1e-3); /* C', P and Q are on one line, so they share a pixel */
            CHECK_NEAR(a[1], b[1], 1e-3);
        }
    }
}

static void test_mirror_image_is_in_frame_where_the_ground_is(void) {
    float proj[16];
    swc_proj_matrix(62.0, 16.0 / 9.0, 1.0, 14000.0, proj);
    static const double poses[4][3] = {
        {0.0, -0.3, 30.0}, {1.1, 0.0, 10.0}, {-2.0, -0.6, 200.0}, {3.0, 0.4, 3.0}};
    int in_view = 0;
    for (int i = 0; i < 4; i++) {
        double yaw = poses[i][0], pitch = poses[i][1], h = poses[i][2];
        float v[16], mv[16], m[16], real[16];
        swc_view_matrix(yaw, pitch, h, v);
        swc_mirror_view_matrix(yaw, pitch, h, mv);
        swc_mirror_sample_matrix(proj, mv, m);
        swc_mat4_mul(proj, v, real);
        for (int gz = -20; gz <= 20; gz++)
            for (int gx = -20; gx <= 20; gx++) {
                double p[3] = {gx * 37.0, 0.0, gz * 41.0}, o[4];
                mul_point(real, p, o);
                if (o[3] <= 0.0 || fabs(o[0]) > 0.97 * o[3] || fabs(o[1]) > 0.97 * o[3])
                    continue; /* not on the real screen */
                in_view++;
                double uv[2];
                sample_uv(m, p, uv);
                CHECK(uv[0] > -0.02 && uv[0] < 1.02); /* the mirror camera has it in frame too */
                CHECK(uv[1] > -0.02 && uv[1] < 1.02);
            }
    }
    CHECK(in_view > 100);
}

int main(void) {
    RUN(test_hex_converts_srgb_to_linear);
    RUN(test_moods_are_sound);
    RUN(test_a_new_look_sits_on_its_mood);
    RUN(test_easing_moves_a_share_of_the_way_per_step);
    RUN(test_easing_converges_and_keeps_the_sun_a_unit_vector);
    RUN(test_easing_does_not_depend_on_the_frame_rate);
    RUN(test_view_matrix_maps_the_view_direction_to_minus_z);
    RUN(test_yaw_turns_towards_minus_x);
    RUN(test_projection_maps_the_frustum_edge_to_the_screen_edge);
    RUN(test_matrix_product_composes_in_column_major_order);
    RUN(test_mirror_view_sees_a_ground_point_where_its_reflected_ray_goes);
    RUN(test_mirror_image_is_in_frame_where_the_ground_is);
    return TEST_SUMMARY();
}
