/* The unit box and cylinder of the buildings layer: sizes, and that every triangle faces the way its
 * vertices say (the GPU culls back faces, so a flipped triangle is a hole in the city). */
#include <math.h>

#include "arcade_test.h"
#include "swc_shapes.h"

static SwcShape shape;

/* Dot of a triangle's geometric normal (counter-clockwise = outward) with the average vertex normal. */
static double facing(const SwcShape *s, int t) {
    const uint16_t *ix = &s->index[t * 3];
    double p[3][3], n[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++)
        for (int j = 0; j < 3; j++) {
            p[k][j] = s->position[ix[k] * 3 + j];
            n[j] += s->normal[ix[k] * 3 + j];
        }
    double a[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
    double b[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
    double g[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    return g[0] * n[0] + g[1] * n[1] + g[2] * n[2];
}

static void check_sound(const SwcShape *s) {
    CHECK(s->vertex_count > 0 && s->vertex_count <= SWC_SHAPE_MAX_VERTICES);
    CHECK(s->index_count > 0 && s->index_count % 3 == 0 && s->index_count <= SWC_SHAPE_MAX_INDICES);
    for (int i = 0; i < s->index_count; i++)
        CHECK(s->index[i] < s->vertex_count);
    for (int t = 0; t < s->index_count / 3; t++)
        CHECK(facing(s, t) > 1e-9); /* counter-clockwise seen from outside, none degenerate */
    for (int v = 0; v < s->vertex_count; v++) {
        double l = 0.0;
        for (int j = 0; j < 3; j++)
            l += (double)s->normal[v * 3 + j] * (double)s->normal[v * 3 + j];
        CHECK_NEAR(l, 1.0, 1e-6);
        CHECK(s->position[v * 3 + 1] >= 0.0f && s->position[v * 3 + 1] <= 1.0f); /* one unit tall, on y = 0 */
        CHECK(fabsf(s->position[v * 3]) <= 0.5f + 1e-6f && fabsf(s->position[v * 3 + 2]) <= 0.5f + 1e-6f);
        CHECK(s->face[v * 3] >= 0.0f && s->face[v * 3] <= (float)SWC_FACE_TOP);
        CHECK(s->face[v * 3 + 1] >= -1e-6f && s->face[v * 3 + 1] <= 1.0f + 1e-6f);
        CHECK(s->face[v * 3 + 2] >= -1e-6f && s->face[v * 3 + 2] <= 1.0f + 1e-6f);
    }
}

static void test_the_box_has_four_walls_and_a_top(void) {
    swc_shape_unit_box(&shape);
    CHECK_EQ_INT(shape.vertex_count, 20);
    CHECK_EQ_INT(shape.index_count, 30);
    check_sound(&shape);
    /* no bottom: nothing faces down */
    for (int v = 0; v < shape.vertex_count; v++)
        CHECK(shape.normal[v * 3 + 1] > -0.5f);
}

static void test_u_runs_left_to_right_seen_from_outside(void) {
    swc_shape_unit_box(&shape);
    /* Face +x (id 0): the viewer stands at +x looking towards -x, so their right is -z... which makes u grow
     * with z decreasing. The web version's rule is tangent = (nz, 0, -nx) = (0, 0, -1) for +x. */
    double z0 = 0, z1 = 0;
    for (int v = 0; v < shape.vertex_count; v++)
        if (shape.face[v * 3] == 0.0f) {
            if (shape.face[v * 3 + 1] == 0.0f)
                z0 = shape.position[v * 3 + 2];
            if (shape.face[v * 3 + 1] == 1.0f)
                z1 = shape.position[v * 3 + 2];
        }
    CHECK(z1 < z0);
}

static void test_the_cylinder_is_a_wall_and_a_cap(void) {
    swc_shape_unit_cylinder(&shape, SWC_CYLINDER_SEGMENTS);
    CHECK_EQ_INT(shape.vertex_count, 3 * SWC_CYLINDER_SEGMENTS + 4);
    CHECK_EQ_INT(shape.index_count, 9 * SWC_CYLINDER_SEGMENTS);
    check_sound(&shape);
    /* the wall's u runs 1 .. 0 round the circle and the seam column is duplicated */
    CHECK_NEAR(shape.face[1], 1.0, 1e-6);
    CHECK_NEAR(shape.face[(2 * SWC_CYLINDER_SEGMENTS) * 3 + 1], 0.0, 1e-6);
    CHECK_NEAR(shape.position[0], shape.position[(2 * SWC_CYLINDER_SEGMENTS) * 3], 1e-5);
}

static void test_fewer_segments_still_make_a_sound_cylinder(void) {
    for (int n = 3; n <= 12; n++) {
        swc_shape_unit_cylinder(&shape, n);
        check_sound(&shape);
    }
}

int main(void) {
    RUN(test_the_box_has_four_walls_and_a_top);
    RUN(test_u_runs_left_to_right_seen_from_outside);
    RUN(test_the_cylinder_is_a_wall_and_a_cap);
    RUN(test_fewer_segments_still_make_a_sound_cylinder);
    return TEST_SUMMARY();
}
