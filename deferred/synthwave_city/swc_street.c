/* Synthwave City - the street life layer's data. See swc_street.h. Ported from the web version
 * (src/layers/streetlife/pack.ts and mesh.ts); the order of every hash and arithmetic step follows it. */
#include "swc_street.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double js_round(double v) {
    return floor(v + 0.5);
}

static double fract_of(double v) {
    return v - floor(v);
}

/* x mod y for whole numbers, as sl_imod in GLSL. */
static double imod(double x, double y) {
    return x - y * floor((x + 0.5) / y);
}

/* ---------------------------------------------------------------------------------------------------- */
/* The lane table                                                                                       */
/* ---------------------------------------------------------------------------------------------------- */

/* Speed range per lane slot, in laps (1 lap = 1.5625 m/s): the inner lane is the fast one. */
static const int AVENUE_LAPS[3][2] = {{18, 21}, {14, 17}, {10, 13}};
static const int STREET_LAPS[2][2] = {{9, 11}, {6, 8}};

double swc_street_traffic_density(double quality_density) {
    return sqrt(swc_clamp(quality_density, 0.05, 1.0));
}

void swc_street_build_lanes(const SwcCity *city, double density, uint32_t seed, SwcLaneTable *out) {
    memset(out, 0, sizeof(*out));
    double d = swc_clamp(density, 0.05, 4.0);
    double min_spacing = INFINITY, max_spacing = 0.0;
    for (int i = 0; i < city->lane_count; i++) {
        const SwcLane *lane = &city->lanes[i];
        if (lane->kind == SWC_LANE_SKY)
            continue;
        if (lane->slot < 0 || lane->slot > 2 || lane->line < 0 || lane->line >= SWC_CELLS)
            continue;
        double offset = swc_wrap_delta(lane->fixed, (double)lane->line * SWC_CELL, SWC_TILE);
        int side = offset > 0.0 ? 1 : 0;
        int row = lane->axis * SWC_STREET_ROWS_PER_AXIS + side * 3 + lane->slot;
        int at = (row * SWC_CELLS + lane->line) * 4;
        if (out->data[at + 2] > 0.0f)
            continue; /* two lanes in one row: the web version throws; the plan never makes them */
        bool avenue = lane->kind == SWC_LANE_AVENUE;
        double u = swc_hash_unit4(seed, (uint32_t)lane->line, (uint32_t)row, 1u);
        double target = (avenue ? 30.0 + 10.0 * u : 33.0 + 13.0 * u) / d;
        int count =
            (int)swc_clamp(js_round(SWC_TILE / target), 1.0, floor(SWC_TILE / SWC_STREET_MIN_SPACING));
        int lo, hi;
        if (avenue) {
            lo = AVENUE_LAPS[lane->slot][0];
            hi = AVENUE_LAPS[lane->slot][1];
        } else if (lane->slot < 2) {
            lo = STREET_LAPS[lane->slot][0];
            hi = STREET_LAPS[lane->slot][1];
        } else {
            lo = 8;
            hi = 10;
        }
        uint32_t hv[4];
        hv[0] = seed;
        hv[1] = (uint32_t)lane->line;
        hv[2] = (uint32_t)row;
        hv[3] = 2u;
        int laps = lo + (int)(swc_hash_ints(hv, 4) % (uint32_t)(hi - lo + 1));
        float phase = (float)swc_hash_unit4(seed, (uint32_t)lane->line, (uint32_t)row, 3u);
        double spacing = SWC_TILE / (double)count;
        out->data[at] = (float)offset;
        out->data[at + 1] = (float)(lane->dir * laps);
        out->data[at + 2] = (float)count;
        out->data[at + 3] = phase;
        SwcLaneRec *r = &out->records[out->record_count++];
        r->axis = lane->axis;
        r->line = (uint8_t)lane->line;
        r->row = (uint8_t)row;
        r->dir = lane->dir;
        r->laps = laps;
        r->count = count;
        r->offset = offset;
        r->phase = phase;
        r->spacing = spacing;
        out->slots += count;
        if (spacing < min_spacing)
            min_spacing = spacing;
        if (spacing > max_spacing)
            max_spacing = spacing;
    }
    if (out->record_count == 0) {
        min_spacing = SWC_TILE;
        max_spacing = SWC_TILE;
    }
    out->min_spacing = min_spacing;
    out->max_spacing = max_spacing;
}

/* ---------------------------------------------------------------------------------------------------- */
/* The GLSL hashes                                                                                      */
/* ---------------------------------------------------------------------------------------------------- */

static uint32_t float_bits(double v) {
    float f = (float)(v == 0.0 ? 0.0 : v);
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

static double unit_of(uint32_t h) {
    return (double)(h >> 8) / 16777216.0;
}

void swc_street_hash33(double x, double y, double z, double out[3]) {
    uint32_t h = swc_hash32(float_bits(x) ^ swc_hash32(float_bits(y) ^ swc_hash32(float_bits(z))));
    uint32_t h2 = swc_hash32(h);
    out[0] = unit_of(h);
    out[1] = unit_of(h2);
    out[2] = unit_of(swc_hash32(h2));
}

/* ---------------------------------------------------------------------------------------------------- */
/* The car model                                                                                        */
/* ---------------------------------------------------------------------------------------------------- */

double swc_street_jitter_range(double spacing) {
    return swc_clamp(1.0 - SWC_STREET_MIN_GAP / spacing, 0.0, SWC_STREET_JITTER_MAX);
}

double swc_street_lane_id(const SwcLaneRec *rec) {
    return (double)rec->line + (double)rec->row * 64.0;
}

void swc_street_car_random(const SwcLaneRec *rec, int index, double out[3]) {
    swc_street_hash33(swc_street_lane_id(rec), (double)index, 1.0, out);
}

bool swc_street_car_present(const SwcLaneRec *rec, int index) {
    double r[3];
    swc_street_car_random(rec, index, r);
    return r[1] >= SWC_STREET_SKIP_CHANCE;
}

int swc_street_car_class(const SwcLaneRec *rec, int index) {
    double r2[3];
    swc_street_hash33(swc_street_lane_id(rec), (double)index, 2.0, r2);
    bool inner = rec->row % 3 == 0;
    int klass;
    if (r2[0] < SWC_STREET_BUS_CHANCE)
        klass = inner ? SWC_STREET_CLASS_VAN : SWC_STREET_CLASS_BUS;
    else if (r2[0] < SWC_STREET_BUS_CHANCE + SWC_STREET_VAN_CHANCE)
        klass = SWC_STREET_CLASS_VAN;
    else if (r2[0] < SWC_STREET_BUS_CHANCE + SWC_STREET_VAN_CHANCE + SWC_STREET_BIKE_CHANCE)
        klass = SWC_STREET_CLASS_BIKE;
    else
        klass = SWC_STREET_CLASS_CAR;
    /* Taxis and emergency vehicles are always cars. */
    if (r2[1] < SWC_STREET_EMERGENCY_CHANCE + SWC_STREET_TAXI_CHANCE)
        klass = SWC_STREET_CLASS_CAR;
    return klass;
}

double swc_street_car_coordinate(const SwcLaneRec *rec, int index, double loop) {
    double r[3];
    swc_street_car_random(rec, index, r);
    double base = fract_of((double)rec->dir * (double)rec->laps * loop + (double)rec->phase);
    return swc_wrap(
        (base + ((double)index + (r[0] - 0.5) * swc_street_jitter_range(rec->spacing)) / (double)rec->count) *
            SWC_TILE,
        SWC_TILE);
}

/* ---------------------------------------------------------------------------------------------------- */
/* The instance grid for near cars                                                                      */
/* ---------------------------------------------------------------------------------------------------- */

int swc_street_car_field_lines(double radius) {
    return (int)ceil(radius / SWC_CELL + 0.5);
}

int swc_street_car_field_reach(const SwcLaneTable *t, double radius) {
    return (int)ceil(radius / t->min_spacing) + 1;
}

int swc_street_car_field_count(const SwcLaneTable *t, double radius) {
    int lines = swc_street_car_field_lines(radius), reach = swc_street_car_field_reach(t, radius);
    return 2 * (2 * lines + 1) * SWC_STREET_ROWS_PER_AXIS * (2 * reach + 1);
}

typedef struct FieldCell {
    int j, k;
    double d;
} FieldCell;

static int field_cell_order(const void *pa, const void *pb) {
    const FieldCell *a = (const FieldCell *)pa, *b = (const FieldCell *)pb;
    if (a->d != b->d)
        return a->d < b->d ? -1 : 1;
    if (a->j != b->j)
        return a->j < b->j ? -1 : 1;
    return a->k < b->k ? -1 : (a->k > b->k ? 1 : 0);
}

int swc_street_build_car_field(const SwcLaneTable *t, double radius, float *out, int cap) {
    int lines = swc_street_car_field_lines(radius), reach = swc_street_car_field_reach(t, radius);
    int count = swc_street_car_field_count(t, radius);
    if (count > cap)
        return 0;
    int ncell = (2 * lines + 1) * (2 * reach + 1);
    FieldCell *cells = (FieldCell *)malloc((size_t)ncell * sizeof(FieldCell));
    if (cells == NULL)
        return 0;
    int n = 0;
    for (int j = -lines; j <= lines; j++)
        for (int k = -reach; k <= reach; k++) {
            cells[n].j = j;
            cells[n].k = k;
            cells[n].d = hypot((double)j * SWC_CELL, (double)k * t->min_spacing);
            n++;
        }
    /* Nearest first, so the depth buffer rejects most of what is hidden. */
    qsort(cells, (size_t)n, sizeof(FieldCell), field_cell_order);
    int o = 0;
    for (int c = 0; c < n; c++)
        for (int axis = 0; axis < 2; axis++)
            for (int sub = 0; sub < SWC_STREET_ROWS_PER_AXIS; sub++) {
                out[o++] = (float)axis;
                out[o++] = (float)cells[c].j;
                out[o++] = (float)sub;
                out[o++] = (float)cells[c].k;
            }
    free(cells);
    return count;
}

SwcPlacedCar swc_street_slot_car(const SwcLaneTable *t, double cam_x, double cam_z, double loop, int axis,
                                 int j, int sub, int k) {
    SwcPlacedCar none = {NULL, 0, 0.0, 0.0};
    double cam_along = axis == 0 ? cam_x : cam_z;
    double cam_across = axis == 0 ? cam_z : cam_x;
    double line_f = floor(cam_across / SWC_CELL + 0.5) + (double)j;
    int line = (int)imod(line_f, (double)SWC_CELLS);
    int row = axis * SWC_STREET_ROWS_PER_AXIS + sub;
    double count = (double)t->data[(row * SWC_CELLS + line) * 4 + 2];
    if (count < 0.5)
        return none;
    const SwcLaneRec *rec = NULL;
    for (int i = 0; i < t->record_count; i++)
        if (t->records[i].line == line && t->records[i].row == row) {
            rec = &t->records[i];
            break;
        }
    if (rec == NULL)
        return none;
    double base = fract_of((double)rec->dir * (double)rec->laps * loop + (double)rec->phase);
    double q = (cam_along / SWC_TILE - base) * count;
    double idx_f = floor(q + 0.5) + (double)k;
    int index = (int)imod(idx_f, count);
    double r[3];
    swc_street_car_random(rec, index, r);
    SwcPlacedCar p;
    p.rec = rec;
    p.index = index;
    p.along = (idx_f - q + (r[0] - 0.5) * swc_street_jitter_range(rec->spacing)) * rec->spacing;
    p.across = line_f * SWC_CELL + rec->offset - cam_across;
    return p;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Crowds                                                                                               */
/* ---------------------------------------------------------------------------------------------------- */

int swc_street_crowd_count(int per_face) {
    int n = per_face < 0 ? 0 : per_face;
    return 2 * (2 * SWC_CROWD_LINES + 1) * 2 * (SWC_CROWD_BLOCKS_BEFORE + SWC_CROWD_BLOCKS_AFTER + 1) * n;
}

int swc_street_build_crowd_field(int per_face, float *out, int cap) {
    int n = per_face < 0 ? 0 : per_face;
    int count = swc_street_crowd_count(n);
    if (count > cap)
        return 0;
    int o = 0;
    for (int axis = 0; axis < 2; axis++)
        for (int j = -SWC_CROWD_LINES; j <= SWC_CROWD_LINES; j++)
            for (int side = 0; side < 2; side++)
                for (int b = -SWC_CROWD_BLOCKS_BEFORE; b <= SWC_CROWD_BLOCKS_AFTER; b++)
                    for (int p = 0; p < n; p++) {
                        out[o++] = (float)(axis * 2 + side);
                        out[o++] = (float)j;
                        out[o++] = (float)b;
                        out[o++] = (float)p;
                    }
    return count;
}

SwcWalker swc_street_walker_at(double cam_x, double cam_z, double loop, int axis_side, int j, int b, int p) {
    int axis = axis_side >= 2 ? 1 : 0;
    int side = axis_side - 2 * axis;
    double cam_along = axis == 0 ? cam_x : cam_z;
    double cam_across = axis == 0 ? cam_z : cam_x;
    double line_f = floor(cam_across / SWC_CELL + 0.5) + (double)j;
    double block_f = floor(cam_along / SWC_CELL + 0.5) + (double)b;
    int line = (int)imod(line_f, (double)SWC_CELLS);
    int block = (int)imod(block_f, (double)SWC_CELLS);
    int next = (int)imod(block_f + 1.0, (double)SWC_CELLS);
    const SwcRoad *road = swc_road_spec(line);
    double lo = block_f * SWC_CELL + swc_road_spec(block)->road_half + SWC_CROWD_END_MARGIN;
    double hi = (block_f + 1.0) * SWC_CELL - swc_road_spec(next)->road_half - SWC_CROWD_END_MARGIN;
    double id = (double)line + (double)axis_side * 64.0;
    double r1[3], r2[3];
    swc_street_hash33(id, (double)block, (double)p, r1);
    swc_street_hash33(id, (double)block, (double)p + 256.0, r2);
    bool standing = r2[0] < SWC_CROWD_STANDING_CHANCE;
    int laps = standing ? 0 : (int)floor(12.0 + 11.99 * r1[1]);
    double u = fract_of((double)laps * loop + r1[2]);
    double tri = fabs(2.0 * u - 1.0);
    double along = lo + (hi - lo) * tri;
    double inner = road->road_half + SWC_CROWD_MARGIN;
    double outer = road->corridor_half - SWC_CROWD_MARGIN;
    double lateral = inner + (outer - inner) * r1[0];
    double across = line_f * SWC_CELL + (double)(side * 2 - 1) * lateral;
    SwcWalker w;
    w.x = axis == 0 ? along : across;
    w.z = axis == 0 ? across : along;
    w.along = along - cam_along;
    w.across = across - cam_across;
    w.standing = standing;
    w.laps = laps;
    return w;
}

/* ---------------------------------------------------------------------------------------------------- */
/* People between the buildings: alleys and plazas                                                      */
/* ---------------------------------------------------------------------------------------------------- */

const double SWC_CROWD_BY_DISTRICT[5] = {1.0, 0.7, 0.55, 1.0, 0.4};

SwcRect swc_street_cell_rect(int cx, int cz) {
    SwcRect r;
    r.x0 = swc_road_spec((int)imod((double)cx, (double)SWC_CELLS))->corridor_half;
    r.x1 = SWC_CELL - swc_road_spec((int)imod((double)(cx + 1), (double)SWC_CELLS))->corridor_half;
    r.z0 = swc_road_spec((int)imod((double)cz, (double)SWC_CELLS))->corridor_half;
    r.z1 = SWC_CELL - swc_road_spec((int)imod((double)(cz + 1), (double)SWC_CELLS))->corridor_half;
    return r;
}

/* One building's extent across an alley axis, in the cell's own coordinates. */
typedef struct Span {
    double a, b;
} Span;

/* Most street-level boxes one cell holds (the plan puts about a dozen to a hundred in a cell). A cell over
 * the cap would only lose alleys; tests/test_swc_street.c checks that no seed comes near it. */
#define SPAN_CAP 256

static void sort_spans_by_start(Span *s, int n) {
    for (int i = 1; i < n; i++) {
        Span v = s[i];
        int j = i - 1;
        while (j >= 0 && s[j].a > v.a) {
            s[j + 1] = s[j];
            j--;
        }
        s[j + 1] = v;
    }
}

/* Alley candidates: centre and half width. */
typedef struct Gap {
    double centre, half;
} Gap;

static void sort_gaps_widest_first(Gap *g, int n) {
    for (int i = 1; i < n; i++) {
        Gap v = g[i];
        int j = i - 1;
        while (j >= 0 && g[j].half < v.half) {
            g[j + 1] = g[j];
            j--;
        }
        g[j + 1] = v;
    }
}

static void sort_gaps_by_centre(Gap *g, int n) {
    for (int i = 1; i < n; i++) {
        Gap v = g[i];
        int j = i - 1;
        while (j >= 0 && g[j].centre > v.centre) {
            g[j + 1] = g[j];
            j--;
        }
        g[j + 1] = v;
    }
}

void swc_street_build_alleys(const SwcCity *city, SwcAlleyTable *out) {
    static Span spans[2][SPAN_CAP];
    static Gap gaps[SPAN_CAP];
    memset(out, 0, sizeof(*out));
    const int width = SWC_CELLS * SWC_ALLEY_TEXELS;
    out->width = width;
    out->height = SWC_CELLS;
    for (int cz = 0; cz < SWC_CELLS; cz++) {
        for (int cx = 0; cx < SWC_CELLS; cx++) {
            int cell = cz * SWC_CELLS + cx;
            int district = city->cells[cell].district;
            if (district == SWC_PLAZA || district == SWC_MEGA)
                continue;
            /* The extents of the street-level boxes across each axis' alleys, in plan order. */
            int n = 0;
            for (int i = city->box_start[cell]; i < city->box_start[cell + 1] && n < SPAN_CAP; i++) {
                const SwcBox *box = &city->boxes[city->box_list[i]];
                if ((double)box->y0 >= SWC_ALLEY_GROUND)
                    continue;
                double x = (double)box->cx, z = (double)box->cz;
                double hx = (double)box->sx / 2.0, hz = (double)box->sz / 2.0;
                double lx = x - floor(x / SWC_CELL) * SWC_CELL, lz = z - floor(z / SWC_CELL) * SWC_CELL;
                spans[0][n].a = lz - hz;
                spans[0][n].b = lz + hz;
                spans[1][n].a = lx - hx;
                spans[1][n].b = lx + hx;
                n++;
            }
            for (int axis = 0; axis < 2; axis++) {
                Span *list = spans[axis];
                sort_spans_by_start(list, n);
                /* Merge the overlapping extents in place. */
                int merged = 0;
                for (int i = 0; i < n; i++) {
                    if (merged > 0 && list[i].a <= list[merged - 1].b) {
                        if (list[i].b > list[merged - 1].b)
                            list[merged - 1].b = list[i].b;
                    } else
                        list[merged++] = list[i];
                }
                int ng = 0;
                for (int i = 1; i < merged; i++) {
                    double lo = list[i - 1].b, hi = list[i].a;
                    if (hi - lo >= SWC_ALLEY_MIN_WIDTH) {
                        gaps[ng].centre = (lo + hi) / 2.0;
                        gaps[ng].half = (hi - lo) / 2.0;
                        ng++;
                    }
                }
                /* The widest three, in order across the block. */
                sort_gaps_widest_first(gaps, ng);
                int kept = ng < SWC_ALLEYS_PER_AXIS ? ng : SWC_ALLEYS_PER_AXIS;
                sort_gaps_by_centre(gaps, kept);
                int at = (cz * width + cx * SWC_ALLEY_TEXELS + axis * 2) * 4;
                for (int k = 0; k < kept; k++) {
                    out->data[at + k] = (float)gaps[k].centre;
                    out->data[at + 4 + k] = (float)gaps[k].half;
                }
                out->data[at + 3] = (float)kept;
                out->count += kept;
            }
        }
    }
}

int swc_street_block_count(int per_cell) {
    int n = per_cell < 0 ? 0 : per_cell;
    int side = 2 * SWC_BLOCK_REACH + 1;
    return side * side * n;
}

int swc_street_build_block_field(int per_cell, float *out, int cap) {
    int n = per_cell < 0 ? 0 : per_cell;
    int count = swc_street_block_count(n);
    if (count > cap)
        return 0;
    int o = 0;
    for (int dz = -SWC_BLOCK_REACH; dz <= SWC_BLOCK_REACH; dz++)
        for (int dx = -SWC_BLOCK_REACH; dx <= SWC_BLOCK_REACH; dx++)
            for (int p = 0; p < n; p++) {
                out[o++] = (float)dx;
                out[o++] = (float)dz;
                out[o++] = (float)p;
                out[o++] = 0.0f;
            }
    return count;
}

SwcBlockWalker swc_street_block_walker_at(const SwcAlleyTable *alleys, const SwcCity *city, double cam_x,
                                          double cam_z, double loop, int dx, int dz, int p, int per_cell) {
    double cell_xf = floor(cam_x / SWC_CELL) + (double)dx;
    double cell_zf = floor(cam_z / SWC_CELL) + (double)dz;
    int cx = (int)imod(cell_xf, (double)SWC_CELLS), cz = (int)imod(cell_zf, (double)SWC_CELLS);
    int district = city->cells[cz * SWC_CELLS + cx].district;
    double share = district < 5 ? SWC_CROWD_BY_DISTRICT[district] : 1.0;
    SwcRect rect = swc_street_cell_rect(cx, cz);
    double x0 = cell_xf * SWC_CELL + rect.x0, x1 = cell_xf * SWC_CELL + rect.x1;
    double z0 = cell_zf * SWC_CELL + rect.z0, z1 = cell_zf * SWC_CELL + rect.z1;
    double id = (double)(cz * SWC_CELLS + cx + SWC_BLOCK_HASH_BASE);
    double r1[3], r2[3], r4[3];
    swc_street_hash33(id, (double)p, 11.0, r1);
    swc_street_hash33(id, (double)p, 12.0, r2);
    swc_street_hash33(id, (double)p, 14.0, r4);
    SwcBlockWalker none;
    memset(&none, 0, sizeof(none));
    none.mode = SWC_BLOCK_ALLEY;
    none.cx = cx;
    none.cz = cz;

    if (district == SWC_PLAZA) {
        if (!((double)p + 0.5 < (double)per_cell * share))
            return none;
        double mx = (x0 + x1) / 2.0, mz = (z0 + z1) / 2.0;
        double hx = (x1 - x0) / 2.0, hz = (z1 - z0) / 2.0;
        SwcBlockWalker w;
        memset(&w, 0, sizeof(w));
        w.show = true;
        w.cx = cx;
        w.cz = cz;
        if (r4[0] < SWC_PLAZA_RING_SHARE + SWC_PLAZA_STAND_SHARE) {
            bool standing = r4[0] >= SWC_PLAZA_RING_SHARE;
            double radius = SWC_PLAZA_RING_MIN + (SWC_PLAZA_RING_MAX - SWC_PLAZA_RING_MIN) * r4[1];
            double speed = 1.0 + 0.5 * r1[1];
            double laps =
                standing ? 0.0 : fmax(1.0, floor((speed * SWC_LOOP_SECONDS) / (2.0 * SWC_PI * radius)));
            double dir = r4[2] < 0.5 ? -1.0 : 1.0;
            double a = 2.0 * SWC_PI * fract_of(dir * laps * loop + r1[2]);
            w.mode = standing ? SWC_BLOCK_STAND : SWC_BLOCK_RING;
            w.x = mx + radius * cos(a);
            w.z = mz + radius * sin(a);
            w.laps = (int)laps;
            w.lap_length = 2.0 * SWC_PI * radius;
            return w;
        }
        bool along0 = r4[2] < 0.5;
        double ha = along0 ? hx : hz, hp = along0 ? hz : hx;
        double sign = fract_of(r4[2] * 7.0) < 0.5 ? -1.0 : 1.0;
        double offset =
            sign * (SWC_PLAZA_CROSS_CENTRE + (hp - SWC_PLAZA_CROSS_EDGE - SWC_PLAZA_CROSS_CENTRE) * r4[1]);
        double laps = floor(12.0 + 11.99 * r1[1]);
        double tri = fabs(2.0 * fract_of(laps * loop + r1[2]) - 1.0);
        double along = -ha + 1.0 + (2.0 * ha - 2.0) * tri;
        w.mode = SWC_BLOCK_CROSS;
        w.x = mx + (along0 ? along : offset);
        w.z = mz + (along0 ? offset : along);
        w.laps = (int)laps;
        w.lap_length = 2.0 * (2.0 * ha - 2.0);
        return w;
    }

    /* An alley: slot p belongs to alley place p mod 6. */
    int a = (int)imod((double)p, 6.0);
    int axis = a >= 3 ? 1 : 0;
    int k = a - 3 * axis;
    int q = (int)floor((double)p / 6.0);
    int at = (cz * alleys->width + cx * SWC_ALLEY_TEXELS + axis * 2) * 4;
    double n = (double)alleys->data[at + 3];
    if (!((double)k + 0.5 < n) || !((double)q + 0.5 < ((double)per_cell / 6.0) * share))
        return none;
    double centre = (double)alleys->data[at + k];
    double half = (double)alleys->data[at + 4 + k];
    bool standing = r2[0] < SWC_ALLEY_STANDING;
    double laps = standing ? 0.0 : floor(12.0 + 11.99 * r1[1]);
    double tri = fabs(2.0 * fract_of(laps * loop + r1[2]) - 1.0);
    double lo = (axis == 0 ? x0 : z0) + 0.5, hi = (axis == 0 ? x1 : z1) - 0.5;
    double along = lo + (hi - lo) * tri;
    double lateral = standing ? (r1[0] < 0.5 ? -1.0 : 1.0) * fmax(half - SWC_ALLEY_STAND_MARGIN, 0.0)
                              : (2.0 * r1[0] - 1.0) * fmax(half - SWC_ALLEY_WALK_MARGIN, 0.0);
    double across = (axis == 0 ? cell_zf : cell_xf) * SWC_CELL + centre + lateral;
    SwcBlockWalker w;
    memset(&w, 0, sizeof(w));
    w.show = true;
    w.mode = SWC_BLOCK_ALLEY;
    w.x = axis == 0 ? along : across;
    w.z = axis == 0 ? across : along;
    w.cx = cx;
    w.cz = cz;
    w.laps = (int)laps;
    w.lap_length = 2.0 * (hi - lo);
    return w;
}

/* ---------------------------------------------------------------------------------------------------- */
/* The vehicle mesh                                                                                     */
/* ---------------------------------------------------------------------------------------------------- */

const double SWC_CAR_DIMS[3] = {2.25, 1.26, 0.95};
const double SWC_VAN_DIMS[3] = {2.7, 2.1, 1.02};
const double SWC_BIKE_DIMS[3] = {1.05, 1.62, 0.4};
const double SWC_BUS_SCALE[3] = {2.13, 1.48, 1.24};
const double SWC_CAR_FLOOR = 0.2;
const double SWC_VAN_FLOOR = 0.3;
const double SWC_BIKE_FLOOR = 0.45;
const double SWC_BIKE_BODY_HALF = 0.17;
const double SWC_BIKE_TYRE_HALF = 0.07;
const double SWC_CAR_AXLES[3] = {1.42, -1.38, 0.34};
const double SWC_VAN_AXLES[3] = {1.75, -1.75, 0.36};
const double SWC_BIKE_AXLES[3] = {0.72, -0.7, 0.33};

typedef struct Part {
    int id;
    int n; /* vertices */
    double car[10][3];
    double alt[10][3];
    double bike[10][3];
    int face_count;
    int face_len[8];
    int faces[8][5];
} Part;

static void add_face(Part *p, int len, const int *ids) {
    p->face_len[p->face_count] = len;
    for (int i = 0; i < len; i++)
        p->faces[p->face_count][i] = ids[i];
    p->face_count++;
}

/* A prism: a side profile in (x, y), extruded to z = -halfWidth and +halfWidth. */
static Part prism(int id, const double car[5][2], double car_half, const double alt[5][2], double alt_half,
                  const double bike[5][2], double bike_half) {
    Part p;
    memset(&p, 0, sizeof(p));
    p.id = id;
    const int n = 5;
    p.n = 2 * n;
    for (int s = 0; s < 2; s++) {
        double sign = s == 0 ? -1.0 : 1.0;
        for (int i = 0; i < n; i++) {
            int v = s * n + i;
            p.car[v][0] = car[i][0];
            p.car[v][1] = car[i][1];
            p.car[v][2] = sign * car_half;
            p.alt[v][0] = alt[i][0];
            p.alt[v][1] = alt[i][1];
            p.alt[v][2] = sign * alt_half;
            p.bike[v][0] = bike[i][0];
            p.bike[v][1] = bike[i][1];
            p.bike[v][2] = sign * bike_half;
        }
    }
    int ring[5], ring2[5];
    for (int i = 0; i < n; i++) {
        ring[i] = i;
        ring2[i] = i + n;
    }
    add_face(&p, n, ring);
    add_face(&p, n, ring2);
    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        int quad[4] = {i, next, n + next, n + i};
        add_face(&p, 4, quad);
    }
    return p;
}

typedef struct Frustum {
    double x0, x1, y0, w0, x2, x3, y1, w1;
} Frustum;

static void frustum_verts(const Frustum *s, double v[8][3]) {
    const double t[8][3] = {{s->x0, s->y0, -s->w0}, {s->x1, s->y0, -s->w0}, {s->x1, s->y0, s->w0},
                            {s->x0, s->y0, s->w0},  {s->x2, s->y1, -s->w1}, {s->x3, s->y1, -s->w1},
                            {s->x3, s->y1, s->w1},  {s->x2, s->y1, s->w1}};
    memcpy(v, t, sizeof(t));
}

/* A frustum: a base rectangle and a smaller roof rectangle. No floor. */
static Part frustum(int id, const Frustum *car, const Frustum *alt, const Frustum *bike) {
    Part p;
    memset(&p, 0, sizeof(p));
    p.id = id;
    p.n = 8;
    frustum_verts(car, p.car);
    frustum_verts(alt, p.alt);
    frustum_verts(bike, p.bike);
    static const int f[5][4] = {{4, 5, 6, 7}, {1, 2, 6, 5}, {0, 3, 7, 4}, {0, 1, 5, 4}, {3, 2, 6, 7}};
    for (int i = 0; i < 5; i++)
        add_face(&p, 4, f[i]);
    return p;
}

static void axle_verts(double x, double r, double w, double v[8][3]) {
    const double t[8][3] = {{x - r, 0, -w},     {x + r, 0, -w},     {x + r, 0, w},     {x - r, 0, w},
                            {x - r, 2 * r, -w}, {x + r, 2 * r, -w}, {x + r, 2 * r, w}, {x - r, 2 * r, w}};
    memcpy(v, t, sizeof(t));
}

/* An axle: a box as wide as the vehicle standing on the road. Only its four walls are drawn. */
static Part axle(int id, double car_x, double car_r, double car_half, double alt_x, double alt_r,
                 double alt_half, double bike_x, double bike_r, double bike_half) {
    Part p;
    memset(&p, 0, sizeof(p));
    p.id = id;
    p.n = 8;
    axle_verts(car_x, car_r, car_half, p.car);
    axle_verts(alt_x, alt_r, alt_half, p.alt);
    axle_verts(bike_x, bike_r, bike_half, p.bike);
    static const int f[4][4] = {{1, 2, 6, 5}, {0, 3, 7, 4}, {0, 1, 5, 4}, {3, 2, 6, 7}};
    for (int i = 0; i < 4; i++)
        add_face(&p, 4, f[i]);
    return p;
}

void swc_street_build_car_mesh(SwcCarMesh *out) {
    memset(out, 0, sizeof(*out));
    const double car_l = SWC_CAR_DIMS[0], car_h = SWC_CAR_DIMS[1], car_w = SWC_CAR_DIMS[2];
    const double van_l = SWC_VAN_DIMS[0], van_h = SWC_VAN_DIMS[1], van_w = SWC_VAN_DIMS[2];
    const double bike_l = SWC_BIKE_DIMS[0], bike_h = SWC_BIKE_DIMS[1], bike_w = SWC_BIKE_DIMS[2];
    Part parts[4];
    /* Body: tail, deck, cowl, nose, and back along the floor. */
    const double body_car[5][2] = {
        {-car_l, SWC_CAR_FLOOR}, {-car_l, 0.86}, {0.9, 0.74}, {car_l, 0.5}, {car_l, SWC_CAR_FLOOR}};
    const double body_alt[5][2] = {
        {-van_l, SWC_VAN_FLOOR}, {-van_l, 1.2}, {1.5, 1.2}, {van_l, 1.05}, {van_l, SWC_VAN_FLOOR}};
    /* Motorcycle: a raised tail, the seat and tank, a fairing up to the screen, its nose. */
    const double body_bike[5][2] = {{-bike_l, SWC_BIKE_FLOOR + 0.1},
                                    {-bike_l, 0.92},
                                    {0.25, 1.0},
                                    {bike_l, 1.02},
                                    {0.98, SWC_BIKE_FLOOR}};
    parts[0] = prism(0, body_car, car_w, body_alt, van_w, body_bike, SWC_BIKE_BODY_HALF);
    /* Cabin: raked screens on the car, a tall box with a sloped windscreen on the van, and on the motorcycle
     * the rider, from the seat to the helmet, leaning into the wind. */
    const Frustum cab_car = {-1.75, 1.0, 0.7, 0.86, -1.0, 0.1, car_h, 0.66};
    const Frustum cab_alt = {-2.66, 2.35, 1.18, 1.0, -2.6, 1.55, van_h, 0.93};
    const Frustum cab_bike = {-0.62, 0.08, 0.9, bike_w * 0.5, -0.22, 0.12, bike_h, 0.15};
    parts[1] = frustum(1, &cab_car, &cab_alt, &cab_bike);
    parts[2] = axle(2, SWC_CAR_AXLES[0], SWC_CAR_AXLES[2], car_w + 0.03, SWC_VAN_AXLES[0], SWC_VAN_AXLES[2],
                    van_w + 0.03, SWC_BIKE_AXLES[0], SWC_BIKE_AXLES[2], SWC_BIKE_TYRE_HALF);
    parts[3] = axle(2, SWC_CAR_AXLES[1], SWC_CAR_AXLES[2], car_w + 0.03, SWC_VAN_AXLES[1], SWC_VAN_AXLES[2],
                    van_w + 0.03, SWC_BIKE_AXLES[1], SWC_BIKE_AXLES[2], SWC_BIKE_TYRE_HALF);

    int nv = 0, ni = 0;
    for (int pi = 0; pi < 4; pi++) {
        const Part *part = &parts[pi];
        int base = nv;
        double c[3] = {0, 0, 0};
        for (int i = 0; i < part->n; i++)
            for (int k = 0; k < 3; k++)
                c[k] += part->car[i][k];
        for (int k = 0; k < 3; k++)
            c[k] /= (double)part->n;
        for (int i = 0; i < part->n; i++) {
            for (int k = 0; k < 3; k++) {
                out->position[nv * 3 + k] = (float)part->car[i][k];
                out->alt[nv * 3 + k] = (float)part->alt[i][k];
                out->bike[nv * 3 + k] = (float)part->bike[i][k];
            }
            out->part[nv] = (float)part->id;
            nv++;
        }
        for (int f = 0; f < part->face_count; f++) {
            const int *face = part->faces[f];
            for (int i = 1; i + 1 < part->face_len[f]; i++) {
                const double *a = part->car[face[0]], *b = part->car[face[i]], *cc = part->car[face[i + 1]];
                double ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                double ac[3] = {cc[0] - a[0], cc[1] - a[1], cc[2] - a[2]};
                double n[3] = {ab[1] * ac[2] - ab[2] * ac[1], ab[2] * ac[0] - ab[0] * ac[2],
                               ab[0] * ac[1] - ab[1] * ac[0]};
                double outward = n[0] * (a[0] - c[0]) + n[1] * (a[1] - c[1]) + n[2] * (a[2] - c[2]);
                bool out_ward = outward > 0.0;
                out->index[ni++] = (uint16_t)(base + face[0]);
                out->index[ni++] = (uint16_t)(base + face[out_ward ? i : i + 1]);
                out->index[ni++] = (uint16_t)(base + face[out_ward ? i + 1 : i]);
            }
        }
    }
    out->vertex_count = nv;
    out->index_count = ni;
}
