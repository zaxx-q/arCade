/* The buildings layer's instance data. The numbers marked "web version" were produced by the original
 * TypeScript packer (src/layers/buildings/pack.ts) for the same seed, so they pin the port to it. */
#include <string.h>

#include "arcade_test.h"
#include "swc_pack.h"

static SwcCity city;
static SwcPackedBoxes boxes;
static SwcPackedLights lights;

typedef struct GoldenShape {
    int count;
    double pos, size, style, seed; /* sum of every float of the attribute */
    double first[13], last[13];    /* pos, size, style, seed of the first and of the last instance */
} GoldenShape;

typedef struct Golden {
    uint32_t seed;
    int boxes;
    GoldenShape square, round;
    struct {
        int count;
        double site, kind;
        double first[6];
    } lights;
} Golden;

static const Golden GOLDEN[] = {
    {1984u,
     17001,
     {16797,
      108060026.73545551,
      1847155.2416629791,
      127730.0,
      1315919.6122632148,
      {34.386165618896484, 0.0, 35.53746795654297, 27.144386291503906, 21.408737182617188, 29.446989059448242,
       3.0, 2.0, 0.0, 3.0, 0.3722836971282959, 0.23380041122436523, 21.408737182617188},
      {6360.998046875, 0.0, 6349.54150390625, 32.16620635986328, 60.043060302734375, 55.078819274902344, 0.0,
       1.0, 3.0, 3.0, 0.6504831314086914, 0.22659170627593994, 60.043060302734375}},
     {204,
      1497328.5283565521,
      33367.54858493805,
      2453.0,
      46124.337906577624,
      {5362.17333984375, 0.0, 121.19993591308594, 44.67926025390625, 174.28530883789062, 44.67926025390625,
       1.0, 1.0, 2.0, 7.0, 0.8451055884361267, 0.592423141002655, 174.28530883789062},
      {4597.53369140625, 374.9093933105469, 6281.23779296875, 39.522274017333984, 6.587863445281982,
       39.522274017333984, 5.0, 3.0, 2.0, 5.0, 0.7715092897415161, 0.468344122171402, 381.49725341796875}},
     {907,
      7014815.031798959,
      5896.846732141916,
      {3924.0, 224.0687713623047, 122.94172668457031, 1.600000023841858, 6.0, 0.6234853863716125}}},
    {1u,
     16772,
     {16493,
      104312934.82897902,
      1812987.2179999352,
      123675.0,
      1282094.590245234,
      {42.36455535888672, 0.0, 43.823158264160156, 39.322998046875, 4.333543300628662, 42.24020767211914, 0.0,
       0.0, 2.0, 2.0, 0.27929458022117615, 0.6902467608451843, 12.097708702087402},
      {6366.69482421875, 5.1501665115356445, 6347.9951171875, 20.775358200073242, 6.857297420501709,
       49.665184020996094, 1.0, 1.0, 5.0, 1.0, 0.04645560681819916, 0.5930243730545044, 12.007463455200195}},
     {279,
      1808994.2847223282,
      45506.64587020874,
      3299.0,
      72951.10607393365,
      {1333.0765380859375, 0.0, 79.6357192993164, 36.150962829589844, 98.9382095336914, 36.150962829589844,
       4.0, 1.0, 0.0, 7.0, 0.36329641938209534, 0.6990662813186646, 98.9382095336914},
      {2687.351318359375, 123.81122589111328, 6316.0, 34.2253303527832, 5.4533209800720215, 34.2253303527832,
       5.0, 2.0, 4.0, 5.0, 0.016233736649155617, 0.5402812957763672, 129.26454162597656}},
     {823,
      5475061.898992419,
      5353.397511542076,
      {1196.0, 147.27772521972656, 39.19306564331055, 1.2000000476837158, 6.0, 0.6199309229850769}}},
    {42u,
     16572,
     {16383,
      104751214.48421097,
      1743055.6341257095,
      122993.0,
      1063139.2428403348,
      {51.26778030395508, 0.0, 32.640220642089844, 57.8220329284668, 56.67350387573242, 20.566909790039062,
       3.0, 1.0, 2.0, 3.0, 0.9197467565536499, 0.24025806784629822, 56.67350387573242},
      {6361.1484375, 17.70947265625, 6362.0439453125, 29.83294677734375, 27.655662536621094,
       26.483028411865234, 0.0, 2.0, 0.0, 1.0, 0.7448307871818542, 0.9875128865242004, 45.365135192871094}},
     {189,
      1237511.8589010239,
      28950.387316703796,
      2253.0,
      39242.05360614788,
      {2446.277587890625, 0.0, 1234.313232421875, 48.08466720581055, 109.45751953125, 48.08466720581055, 3.0,
       1.0, 2.0, 6.0, 0.6457619071006775, 0.41455569863319397, 115.72439575195312},
      {3600.0, 154.58380126953125, 5644.82666015625, 47.32255172729492, 5.586944103240967, 47.32255172729492,
       5.0, 2.0, 1.0, 5.0, 0.6893553137779236, 0.5362210273742676, 160.17074584960938}},
     {614,
      4027412.5025496483,
      3998.768177836202,
      {2752.457763671875, 178.05038452148438, 1088.6365966796875, 1.600000023841858, 6.0,
       0.4647938907146454}}},
};

static double sum(const float *a, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++)
        s += (double)a[i];
    return s;
}

static void gather(const SwcPackedBoxes *p, int shape, int k, double out[13]) {
    int i = p->first[shape] + k;
    for (int j = 0; j < 3; j++)
        out[j] = (double)p->pos[i * SWC_POS_SIZE + j];
    for (int j = 0; j < 3; j++)
        out[3 + j] = (double)p->size[i * SWC_SIZE_SIZE + j];
    for (int j = 0; j < 4; j++)
        out[6 + j] = (double)p->style[i * SWC_STYLE_SIZE + j];
    for (int j = 0; j < 3; j++)
        out[10 + j] = (double)p->seed[i * SWC_SEED_SIZE + j];
}

static void check_shape(const SwcPackedBoxes *p, int shape, const GoldenShape *g) {
    CHECK_EQ_INT(p->count[shape], g->count);
    int f = p->first[shape], n = p->count[shape];
    CHECK_NEAR(sum(&p->pos[f * SWC_POS_SIZE], n * SWC_POS_SIZE), g->pos, 1e-6 * g->pos);
    CHECK_NEAR(sum(&p->size[f * SWC_SIZE_SIZE], n * SWC_SIZE_SIZE), g->size, 1e-6 * g->size);
    CHECK_NEAR(sum(&p->style[f * SWC_STYLE_SIZE], n * SWC_STYLE_SIZE), g->style, 1e-6 * g->style);
    CHECK_NEAR(sum(&p->seed[f * SWC_SEED_SIZE], n * SWC_SEED_SIZE), g->seed, 1e-6 * g->seed);
    double v[13];
    gather(p, shape, 0, v);
    for (int j = 0; j < 13; j++)
        CHECK_NEAR(v[j], g->first[j], 1e-9);
    gather(p, shape, n - 1, v);
    for (int j = 0; j < 13; j++)
        CHECK_NEAR(v[j], g->last[j], 1e-9);
}

static void test_matches_the_web_version(void) {
    for (size_t g = 0; g < sizeof GOLDEN / sizeof GOLDEN[0]; g++) {
        const Golden *e = &GOLDEN[g];
        swc_city_build(&city, e->seed);
        swc_pack_boxes(&city, &boxes);
        swc_pack_roof_lights(&city, &lights);
        CHECK_EQ_INT(city.box_count, e->boxes);
        check_shape(&boxes, SWC_SHAPE_SQUARE, &e->square);
        check_shape(&boxes, SWC_SHAPE_ROUND, &e->round);
        CHECK_EQ_INT(lights.count, e->lights.count);
        CHECK_NEAR(sum(lights.site, lights.count * SWC_LIGHT_SITE_SIZE), e->lights.site,
                   1e-6 * e->lights.site);
        CHECK_NEAR(sum(lights.kind, lights.count * SWC_LIGHT_KIND_SIZE), e->lights.kind,
                   1e-6 * e->lights.kind);
        for (int j = 0; j < 4; j++)
            CHECK_NEAR((double)lights.site[j], e->lights.first[j], 1e-9);
        for (int j = 0; j < 2; j++)
            CHECK_NEAR((double)lights.kind[j], e->lights.first[4 + j], 1e-9);
    }
}

static void test_every_box_is_one_instance_in_plan_order(void) {
    swc_city_build(&city, 1984u);
    swc_pack_boxes(&city, &boxes);
    CHECK_EQ_INT(boxes.count[0] + boxes.count[1], city.box_count);
    CHECK_EQ_INT(boxes.first[0], 0);
    CHECK_EQ_INT(boxes.first[1], boxes.count[0]);
    int next[2] = {0, 0};
    for (int i = 0; i < city.box_count; i++) {
        const SwcBox *b = &city.boxes[i];
        int shape = (b->flags & SWC_FLAG_ROUND) ? SWC_SHAPE_ROUND : SWC_SHAPE_SQUARE;
        int k = boxes.first[shape] + next[shape]++;
        CHECK_NEAR(boxes.pos[k * 3], b->cx, 0.0);
        CHECK_NEAR(boxes.pos[k * 3 + 1], b->y0, 0.0);
        CHECK_NEAR(boxes.pos[k * 3 + 2], b->cz, 0.0);
        CHECK_NEAR(boxes.size[k * 3 + 1], b->sy, 0.0);
        CHECK_NEAR(boxes.style[k * 4], b->style, 0.0);
        CHECK_NEAR(boxes.style[k * 4 + 3], b->flags, 0.0);
        CHECK_NEAR(boxes.seed[k * 3], b->seed, 0.0);
    }
    CHECK_EQ_INT(next[0], boxes.count[0]);
    CHECK_EQ_INT(next[1], boxes.count[1]);
    CHECK(boxes.count[1] > 0 && boxes.count[0] > boxes.count[1]);
}

static void test_a_building_shares_its_seed_and_roof_height(void) {
    swc_city_build(&city, 1984u);
    swc_pack_boxes(&city, &boxes);
    static float roof[SWC_MAX_BUILDINGS], seen[SWC_MAX_BUILDINGS];
    memset(roof, 0, sizeof roof);
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < city.box_count; i++) {
        const SwcBox *b = &city.boxes[i];
        if (b->style != SWC_STYLE_SPIRE && b->y0 + b->sy > roof[b->building])
            roof[b->building] = b->y0 + b->sy;
    }
    int next[2] = {0, 0};
    for (int i = 0; i < city.box_count; i++) {
        const SwcBox *b = &city.boxes[i];
        int shape = (b->flags & SWC_FLAG_ROUND) ? SWC_SHAPE_ROUND : SWC_SHAPE_SQUARE;
        int k = boxes.first[shape] + next[shape]++;
        float want = roof[b->building] > 1.0f ? roof[b->building] : 1.0f;
        CHECK_NEAR(boxes.seed[k * 3 + 2], want, 1e-3);
        float bs = boxes.seed[k * 3 + 1];
        CHECK(bs >= 0.0f && bs < 1.0f);
        if (seen[b->building] == 0.0f)
            seen[b->building] = bs + 1.0f; /* +1: a seed of exactly 0 still counts as seen */
        else
            CHECK_NEAR(bs + 1.0f, seen[b->building], 0.0); /* the same for every box of the building */
    }
}

static void test_a_spire_does_not_raise_the_roof_height(void) {
    swc_city_build(&city, 1984u);
    swc_pack_boxes(&city, &boxes);
    int checked = 0;
    for (int i = 0; i < city.box_count && checked < 50; i++) {
        const SwcBox *b = &city.boxes[i];
        if (b->style != SWC_STYLE_SPIRE)
            continue;
        CHECK(boxes.roof[b->building] > 0.0f);
        CHECK(boxes.roof[b->building] <=
              b->y0 + 0.01f); /* the mast stands on the roof: it is higher than it */
        checked++;
    }
    CHECK(checked > 0);
}

static void test_roof_lights_are_the_roof_light_sites(void) {
    swc_city_build(&city, 42u);
    swc_pack_roof_lights(&city, &lights);
    int want = 0;
    for (int i = 0; i < city.site_count; i++)
        want += city.sites[i].kind == SWC_SITE_ROOF_LIGHT;
    CHECK_EQ_INT(lights.count, want);
    CHECK(lights.count > 100);
    for (int k = 0; k < lights.count; k++) {
        CHECK(lights.site[k * 4 + 3] > 0.0f);
        CHECK(lights.kind[k * 2] >= 0.0f && lights.kind[k * 2] < 8.0f);
        CHECK(lights.kind[k * 2 + 1] >= 0.0f && lights.kind[k * 2 + 1] < 1.0f);
    }
}

int main(void) {
    RUN(test_matches_the_web_version);
    RUN(test_every_box_is_one_instance_in_plan_order);
    RUN(test_a_building_shares_its_seed_and_roof_height);
    RUN(test_a_spire_does_not_raise_the_roof_height);
    RUN(test_roof_lights_are_the_roof_light_sites);
    return TEST_SUMMARY();
}
