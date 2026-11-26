/* The ground layer's data. The numbers marked "web version" were produced by the original TypeScript layout
 * (src/layers/ground/layout.ts), so they pin the port to it: where every lamp stands, the repeated post
 * instances, the ground disc and the lamp post mesh vertex by vertex. */
#include <math.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_ground.h"

typedef struct GoldenLamps {
    double spacing, extent;
    int count;
    double fields[6]; /* sum of each of the six floats over every lamp */
    double first[6], last[6];
    double head_sums[3], head_first[3], head_last[3];
} GoldenLamps;

typedef struct GoldenPosts {
    int per_pattern, total;
    double a_lamp_sums[4], a_info_sums[2];
    double first[4], last[4];
} GoldenPosts;

typedef struct GoldenDisc {
    int vertices, indices;
    double sum_x, sum_z, index_sum;
    double v1[3], v7[3], last[3];
    int first_index[12], last_index[6];
} GoldenDisc;

static float lamps[25600 * SWC_LAMP_STRIDE];
static float pattern[256 * SWC_LAMP_STRIDE];
static float a_lamp[4096 * 4];
static float a_info[4096 * 2];
static SwcDisc disc;
static SwcPostMesh post;

static const GoldenLamps GOLDEN_LAMPS[] = {
    {40.0,
     640.0,
     256,
     {71680.0, 71680.0, 0.0, 0.0, 1920.0, 512.0},
     {20.0, -14.899999618530273, 0.0, 1.0, 9.0, 3.0},
     {488.8999938964844, 620.0, -1.0, 0.0, 7.0, 1.0},
     {71679.99999999991, 1907.2000000000082, 71680.0},
     {20.0, 8.95, -12.299999618530274},
     {486.29999389648435, 6.95, 620.0}},
    {80.0,
     640.0,
     128,
     {35840.0, 35840.0, 0.0, 0.0, 960.0, 256.0},
     {40.0, -14.899999618530273, 0.0, 1.0, 9.0, 3.0},
     {488.8999938964844, 600.0, -1.0, 0.0, 7.0, 1.0},
     {35840.0, 953.6000000000022, 35840.0},
     {40.0, 8.95, -12.299999618530274},
     {486.29999389648435, 6.95, 600.0}},
    {40.0,
     6400.0,
     25600,
     {80896000.0, 80896000.0, 0.0, 0.0, 192000.0, 51200.0},
     {20.0, -14.899999618530273, 0.0, 1.0, 9.0, 3.0},
     {6248.89990234375, 6380.0, -1.0, 0.0, 7.0, 1.0},
     {80896000.0000011, 190720.00000005722, 80895999.99999999},
     {20.0, 8.95, -12.299999618530274},
     {6246.29990234375, 6.95, 6380.0}},
    {80.0,
     6400.0,
     12800,
     {40448000.0, 40448000.0, 0.0, 0.0, 96000.0, 25600.0},
     {40.0, -14.899999618530273, 0.0, 1.0, 9.0, 3.0},
     {6248.89990234375, 6360.0, -1.0, 0.0, 7.0, 1.0},
     {40448000.0, 95359.9999999778, 40447999.99999993},
     {40.0, 8.95, -12.299999618530274},
     {6246.29990234375, 6.95, 6360.0}},
};

static const GoldenPosts GOLDEN_POSTS = {256,
                                         4096,
                                         {-163840.0, -163840.0, 0.0, 0.0},
                                         {30720.0, 8192.0},
                                         {-1260.0, -1294.9000244140625, 0.0, 1.0},
                                         {1128.9000244140625, 1260.0, -1.0, 0.0}};

static const GoldenDisc GOLDEN_DISC = {721,
                                       4176,
                                       0.0,
                                       0.0,
                                       1455888.0,
                                       {2.0, 0.0, 0.0},
                                       {1.4142135381698608, 0.0, 1.4142135381698608},
                                       {24290.3984375, 0.0, -3197.8916015625},
                                       {0, 2, 1, 1, 2, 50, 1, 50, 49, 49, 50, 98},
                                       {672, 625, 673, 672, 673, 720}};

static const float GOLDEN_POST_POSITION[] = {0.17000000178813934,
                                             0.0,
                                             0.0,
                                             0.0,
                                             0.0,
                                             0.17000000178813934,
                                             -0.17000000178813934,
                                             0.0,
                                             0.0,
                                             0.0,
                                             0.0,
                                             -0.17000000178813934,
                                             0.10000000149011612,
                                             6.099999904632568,
                                             0.0,
                                             0.0,
                                             6.099999904632568,
                                             0.10000000149011612,
                                             -0.10000000149011612,
                                             6.099999904632568,
                                             0.0,
                                             0.0,
                                             6.099999904632568,
                                             -0.10000000149011612,
                                             0.44343146681785583,
                                             6.936568737030029,
                                             0.0,
                                             0.5,
                                             6.880000114440918,
                                             0.07999999821186066,
                                             0.5565685629844666,
                                             6.823431491851807,
                                             0.0,
                                             0.5,
                                             6.880000114440918,
                                             -0.07999999821186066,
                                             2.0999999046325684,
                                             7.070000171661377,
                                             0.0,
                                             2.0999999046325684,
                                             7.0,
                                             0.07000000029802322,
                                             2.0999999046325684,
                                             6.929999828338623,
                                             0.0,
                                             2.0999999046325684,
                                             7.0,
                                             -0.07000000029802322,
                                             1.9800000190734863,
                                             6.889999866485596,
                                             -0.20000000298023224,
                                             3.2200000286102295,
                                             6.889999866485596,
                                             -0.20000000298023224,
                                             1.9800000190734863,
                                             7.070000171661377,
                                             -0.20000000298023224,
                                             3.2200000286102295,
                                             7.070000171661377,
                                             -0.20000000298023224,
                                             1.9800000190734863,
                                             6.889999866485596,
                                             0.20000000298023224,
                                             3.2200000286102295,
                                             6.889999866485596,
                                             0.20000000298023224,
                                             1.9800000190734863,
                                             7.070000171661377,
                                             0.20000000298023224,
                                             3.2200000286102295,
                                             7.070000171661377,
                                             0.20000000298023224};
static const float GOLDEN_POST_PART[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
static const float GOLDEN_POST_AXIS[] = {0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         0.0,
                                         6.099999904632568,
                                         0.0,
                                         0.0,
                                         6.099999904632568,
                                         0.0,
                                         0.0,
                                         6.099999904632568,
                                         0.0,
                                         0.0,
                                         6.099999904632568,
                                         0.0,
                                         0.5,
                                         6.880000114440918,
                                         0.0,
                                         0.5,
                                         6.880000114440918,
                                         0.0,
                                         0.5,
                                         6.880000114440918,
                                         0.0,
                                         0.5,
                                         6.880000114440918,
                                         0.0,
                                         2.0999999046325684,
                                         7.0,
                                         0.0,
                                         2.0999999046325684,
                                         7.0,
                                         0.0,
                                         2.0999999046325684,
                                         7.0,
                                         0.0,
                                         2.0999999046325684,
                                         7.0,
                                         0.0,
                                         1.9800000190734863,
                                         6.889999866485596,
                                         -0.20000000298023224,
                                         3.2200000286102295,
                                         6.889999866485596,
                                         -0.20000000298023224,
                                         1.9800000190734863,
                                         7.070000171661377,
                                         -0.20000000298023224,
                                         3.2200000286102295,
                                         7.070000171661377,
                                         -0.20000000298023224,
                                         1.9800000190734863,
                                         6.889999866485596,
                                         0.20000000298023224,
                                         3.2200000286102295,
                                         6.889999866485596,
                                         0.20000000298023224,
                                         1.9800000190734863,
                                         7.070000171661377,
                                         0.20000000298023224,
                                         3.2200000286102295,
                                         7.070000171661377,
                                         0.20000000298023224};
static const int GOLDEN_POST_INDEX[] = {
    0,  1,  5,  0,  5,  4,  1,  2,  6,  1,  6,  5,  2,  3,  7,  2,  7,  6,  3,  0,  4,  3,
    4,  7,  4,  5,  9,  4,  9,  8,  5,  6,  10, 5,  10, 9,  6,  7,  11, 6,  11, 10, 7,  4,
    8,  7,  8,  11, 8,  9,  13, 8,  13, 12, 9,  10, 14, 9,  14, 13, 10, 11, 15, 10, 15, 14,
    11, 8,  12, 11, 12, 15, 16, 17, 19, 16, 19, 18, 20, 22, 23, 20, 23, 21, 16, 20, 21, 16,
    21, 17, 18, 19, 23, 18, 23, 22, 16, 18, 22, 16, 22, 20, 17, 21, 23, 17, 23, 19};

static const double GOLDEN_SPACING[4] = {40.0, 40.0, 80.0, 80.0}; /* density 1, 0.8, 0.79, 0.35 */

#define N_LAMPS ((int)(sizeof GOLDEN_LAMPS / sizeof GOLDEN_LAMPS[0]))

static double tol(double v) {
    return 1e-9 * (1.0 + fabs(v));
}

/* The web version keeps lamp heads as doubles and rounds to float only when it uploads them; the C side holds
 * floats, so a head is compared to within float precision. */
static double tol_float(double v) {
    return 1e-6 * (1.0 + fabs(v));
}

static void test_the_lamp_spacing_follows_the_density(void) {
    static const double DENSITY[4] = {1.0, 0.8, 0.79, 0.35};
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(swc_lamp_spacing(DENSITY[i]), GOLDEN_SPACING[i], 0.0);
}

static void test_lamps_match_the_web_version(void) {
    for (int g = 0; g < N_LAMPS; g++) {
        const GoldenLamps *e = &GOLDEN_LAMPS[g];
        CHECK_EQ_INT(swc_lamp_count(e->spacing, e->extent), e->count);
        int n = swc_build_lamps(e->spacing, e->extent, lamps);
        CHECK_EQ_INT(n, e->count);
        for (int k = 0; k < 6; k++) {
            double s = 0.0;
            for (int i = 0; i < n; i++)
                s += (double)lamps[i * SWC_LAMP_STRIDE + k];
            CHECK_NEAR(s, e->fields[k], tol(e->fields[k]));
            CHECK_NEAR((double)lamps[k], e->first[k], tol(e->first[k]));
            CHECK_NEAR((double)lamps[(n - 1) * SWC_LAMP_STRIDE + k], e->last[k], tol(e->last[k]));
        }
        double hs[3] = {0, 0, 0};
        float h[3];
        for (int i = 0; i < n; i++) {
            swc_lamp_head(lamps, i, h);
            for (int k = 0; k < 3; k++)
                hs[k] += (double)h[k];
        }
        for (int k = 0; k < 3; k++)
            CHECK_NEAR(hs[k], e->head_sums[k], tol_float(e->head_sums[k]));
        swc_lamp_head(lamps, 0, h);
        for (int k = 0; k < 3; k++)
            CHECK_NEAR((double)h[k], e->head_first[k], tol_float(e->head_first[k]));
        swc_lamp_head(lamps, n - 1, h);
        for (int k = 0; k < 3; k++)
            CHECK_NEAR((double)h[k], e->head_last[k], tol_float(e->head_last[k]));
    }
}

static void test_post_instances_match_the_web_version(void) {
    int per = swc_build_lamps(SWC_LAMP_SPACING, SWC_LAMP_PATTERN, pattern);
    CHECK_EQ_INT(per, GOLDEN_POSTS.per_pattern);
    int total = swc_build_post_instances(pattern, per, SWC_POST_LATTICE, a_lamp, a_info);
    CHECK_EQ_INT(total, GOLDEN_POSTS.total);
    for (int k = 0; k < 4; k++) {
        double s = 0.0;
        for (int i = 0; i < total; i++)
            s += (double)a_lamp[i * 4 + k];
        CHECK_NEAR(s, GOLDEN_POSTS.a_lamp_sums[k], tol(GOLDEN_POSTS.a_lamp_sums[k]));
        CHECK_NEAR((double)a_lamp[k], GOLDEN_POSTS.first[k], tol(GOLDEN_POSTS.first[k]));
        CHECK_NEAR((double)a_lamp[(total - 1) * 4 + k], GOLDEN_POSTS.last[k], tol(GOLDEN_POSTS.last[k]));
    }
    /* The copies run along x first, then z: the second copy starts one pattern to the right of the first. */
    CHECK_NEAR((double)a_lamp[per * 4] - (double)a_lamp[0], SWC_LAMP_PATTERN, 1e-4);
    CHECK_NEAR((double)a_lamp[per * 4 + 1], (double)a_lamp[1], 1e-4);
    for (int k = 0; k < 2; k++) {
        double s = 0.0;
        for (int i = 0; i < total; i++)
            s += (double)a_info[i * 2 + k];
        CHECK_NEAR(s, GOLDEN_POSTS.a_info_sums[k], tol(GOLDEN_POSTS.a_info_sums[k]));
    }
}

static void test_the_disc_matches_the_web_version(void) {
    swc_build_disc(14000.0 * 1.75, &disc);
    const GoldenDisc *e = &GOLDEN_DISC;
    CHECK_EQ_INT(disc.vertex_count, e->vertices);
    CHECK_EQ_INT(disc.index_count, e->indices);
    double sx = 0.0, sz = 0.0, si = 0.0;
    for (int i = 0; i < disc.vertex_count; i++) {
        sx += (double)disc.position[i * 3];
        sz += (double)disc.position[i * 3 + 2];
        CHECK_NEAR((double)disc.position[i * 3 + 1], 0.0, 0.0);
    }
    for (int i = 0; i < disc.index_count; i++)
        si += (double)disc.index[i];
    CHECK_NEAR(sx, e->sum_x, 1e-6 * (1.0 + fabs(e->sum_x)));
    CHECK_NEAR(sz, e->sum_z, 1e-6 * (1.0 + fabs(e->sum_z)));
    CHECK_NEAR(si, e->index_sum, 0.0);
    for (int k = 0; k < 3; k++) {
        CHECK_NEAR((double)disc.position[3 + k], e->v1[k], tol(e->v1[k]));
        CHECK_NEAR((double)disc.position[21 + k], e->v7[k], tol(e->v7[k]));
        CHECK_NEAR((double)disc.position[(disc.vertex_count - 1) * 3 + k], e->last[k], tol(e->last[k]));
    }
    for (int k = 0; k < 12; k++)
        CHECK_EQ_INT((int)disc.index[k], e->first_index[k]);
    for (int k = 0; k < 6; k++)
        CHECK_EQ_INT((int)disc.index[disc.index_count - 6 + k], e->last_index[k]);
}

static void test_the_post_mesh_matches_the_web_version(void) {
    swc_build_post(&post);
    CHECK_EQ_INT(post.vertex_count, SWC_POST_VERTS);
    CHECK_EQ_INT(post.index_count, SWC_POST_INDICES);
    for (int i = 0; i < SWC_POST_VERTS * 3; i++) {
        CHECK_NEAR((double)post.position[i], (double)GOLDEN_POST_POSITION[i], 1e-7);
        CHECK_NEAR((double)post.axis[i], (double)GOLDEN_POST_AXIS[i], 1e-7);
    }
    for (int i = 0; i < SWC_POST_VERTS; i++)
        CHECK_NEAR((double)post.part[i], (double)GOLDEN_POST_PART[i], 0.0);
    for (int i = 0; i < SWC_POST_INDICES; i++)
        CHECK_EQ_INT((int)post.index[i], GOLDEN_POST_INDEX[i]);
}

/* Lamps face each other across every road and none stands in a junction. */
static void test_lamps_line_the_roads_and_miss_the_junctions(void) {
    int n = swc_build_lamps(SWC_LAMP_SPACING, SWC_TILE, lamps);
    CHECK_EQ_INT(n, 25600);
    for (int i = 0; i < n; i++) {
        const float *l = &lamps[i * SWC_LAMP_STRIDE];
        double x = (double)l[SWC_LAMP_X], z = (double)l[SWC_LAMP_Z];
        double dx = (double)l[SWC_LAMP_DIR_X], dz = (double)l[SWC_LAMP_DIR_Z];
        CHECK(fabs(dx) + fabs(dz) == 1.0); /* a unit direction along one axis */
        /* The road runs along the axis the direction is not on. Distance to the centre line, across the road.
         */
        double across = dx != 0.0 ? x : z, along = dx != 0.0 ? z : x;
        int line = (int)floor(across / SWC_CELL + 0.5);
        const SwcRoad *spec = swc_road_spec(line);
        double off = across - (double)line * SWC_CELL;
        CHECK_NEAR(fabs(off), spec->road_half + SWC_LAMP_SETBACK, 1e-4);
        /* The direction points at the centre line. */
        CHECK((dx != 0.0 ? dx : dz) * off < 0.0);
        /* Not at a crossing: half a spacing from the nearest grid line along the road. */
        double to_line = fabs(along - floor(along / SWC_CELL + 0.5) * SWC_CELL);
        CHECK(to_line >= 0.5 * SWC_LAMP_SPACING - 1e-4 || to_line > 0.5 * SWC_LAMP_SPACING - 1e-4);
        CHECK((int)l[SWC_LAMP_COLOR] == swc_lamp_color(line));
        CHECK((double)l[SWC_LAMP_HEIGHT] ==
              (spec->median > 0.0 ? SWC_LAMP_HEIGHT_AVENUE : SWC_LAMP_HEIGHT_STREET));
    }
}

static void test_lamp_colours_alternate_by_grid_line(void) {
    CHECK_EQ_INT(swc_lamp_color(0), SWC_LAMP_COLOR_EVEN);
    CHECK_EQ_INT(swc_lamp_color(1), SWC_LAMP_COLOR_ODD);
    CHECK_EQ_INT(swc_lamp_color(-1), SWC_LAMP_COLOR_ODD);
    CHECK_EQ_INT(swc_lamp_color(-2), SWC_LAMP_COLOR_EVEN);
    CHECK_EQ_INT(swc_lamp_color(39), SWC_LAMP_COLOR_ODD);
}

static void test_the_disc_is_sound_and_faces_up(void) {
    swc_build_disc(14000.0 * 1.75, &disc);
    CHECK(disc.index_count % 3 == 0);
    for (int i = 0; i < disc.index_count; i += 3) {
        int a = disc.index[i], b = disc.index[i + 1], c = disc.index[i + 2];
        CHECK(a < disc.vertex_count && b < disc.vertex_count && c < disc.vertex_count);
        double ux = (double)disc.position[b * 3] - (double)disc.position[a * 3];
        double uz = (double)disc.position[b * 3 + 2] - (double)disc.position[a * 3 + 2];
        double vx = (double)disc.position[c * 3] - (double)disc.position[a * 3];
        double vz = (double)disc.position[c * 3 + 2] - (double)disc.position[a * 3 + 2];
        CHECK(uz * vx - ux * vz > 0.0); /* the +y component of (b - a) x (c - a) */
    }
    /* The outer ring reaches the radius, and the rings double, so the triangles near the viewer stay small.
     */
    double outer = hypot((double)disc.position[(disc.vertex_count - 1) * 3],
                         (double)disc.position[(disc.vertex_count - 1) * 3 + 2]);
    CHECK_NEAR(outer, 24500.0, 1e-3);
    CHECK_NEAR(hypot((double)disc.position[3], (double)disc.position[5]), SWC_DISC_FIRST_RING, 1e-6);
}

static void test_the_post_mesh_is_a_pole_an_arm_and_a_head(void) {
    swc_build_post(&post);
    int head = 0;
    for (int i = 0; i < post.vertex_count; i++)
        head += post.part[i] > 0.5f;
    CHECK_EQ_INT(head, 8);
    for (int i = 0; i < post.index_count; i++)
        CHECK((int)post.index[i] < post.vertex_count);
    /* The pole starts on the ground and the head sits at the modelled height, the arm length out. */
    double lo = 1e9, hi = -1e9, reach = -1e9;
    for (int i = 0; i < post.vertex_count; i++) {
        lo = fmin(lo, (double)post.position[i * 3 + 1]);
        hi = fmax(hi, (double)post.position[i * 3 + 1]);
        reach = fmax(reach, (double)post.position[i * 3]);
    }
    CHECK_NEAR(lo, 0.0, 1e-6);
    CHECK(hi > SWC_LAMP_MODEL_HEIGHT && hi < SWC_LAMP_MODEL_HEIGHT + 0.2);
    CHECK(reach > SWC_LAMP_ARM && reach < SWC_LAMP_ARM + 0.7);
}

static void test_every_period_divides_the_tile(void) {
    CHECK(swc_ground_periods_divide_tile());
}

int main(void) {
    RUN(test_the_lamp_spacing_follows_the_density);
    RUN(test_lamps_match_the_web_version);
    RUN(test_post_instances_match_the_web_version);
    RUN(test_the_disc_matches_the_web_version);
    RUN(test_the_post_mesh_matches_the_web_version);
    RUN(test_lamps_line_the_roads_and_miss_the_junctions);
    RUN(test_lamp_colours_alternate_by_grid_line);
    RUN(test_the_disc_is_sound_and_faces_up);
    RUN(test_the_post_mesh_is_a_pole_an_arm_and_a_head);
    RUN(test_every_period_divides_the_tile);
    return TEST_SUMMARY();
}
