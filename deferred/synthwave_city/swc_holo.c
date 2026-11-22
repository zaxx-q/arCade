/* Synthwave City - the holograms layer's data. See swc_holo.h. Ported from the web version
 * (src/layers/holograms/layout.ts, pack.ts and wire.ts); the order of every vertex and hash follows it. */
#include "swc_holo.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

static double js_round(double v) {
    return floor(v + 0.5);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Layout                                                                                               */
/* ---------------------------------------------------------------------------------------------------- */

#define WIDE_W 512
#define WIDE_H 256
#define TALL_W 256
#define TALL_H 512
#define STRIP_W 2048
#define STRIP_H 128

int swc_atlas_layout(SwcAtlasCell out[SWC_ATLAS_CELL_COUNT]) {
    int n = 0;
    int per_row = SWC_ATLAS_SIZE / WIDE_W;
    for (int i = 0; i < SWC_ATLAS_WIDE_COUNT; i++) {
        SwcAtlasCell c = {SWC_CELL_WIDE, i, (i % per_row) * WIDE_W, (i / per_row) * WIDE_H, WIDE_W, WIDE_H};
        out[n++] = c;
    }
    int tall_y = (SWC_ATLAS_WIDE_COUNT / per_row) * WIDE_H;
    for (int i = 0; i < SWC_ATLAS_TALL_COUNT; i++) {
        SwcAtlasCell c = {SWC_CELL_TALL, i, i * TALL_W, tall_y, TALL_W, TALL_H};
        out[n++] = c;
    }
    int strip_y = tall_y + TALL_H;
    for (int i = 0; i < SWC_ATLAS_STRIP_COUNT; i++) {
        SwcAtlasCell c = {SWC_CELL_STRIP, i, 0, strip_y + i * STRIP_H, STRIP_W, STRIP_H};
        out[n++] = c;
    }
    return n;
}

int swc_atlas_cells_of_kind(const SwcAtlasCell *cells, int kind, const SwcAtlasCell **out, int cap) {
    int n = 0;
    for (int i = 0; i < SWC_ATLAS_CELL_COUNT; i++)
        if (cells[i].kind == kind && n < cap)
            out[n++] = &cells[i];
    return n;
}

void swc_atlas_cell_rect(const SwcAtlasCell *cell, int inset, float out[4]) {
    out[0] = (float)((double)(cell->x + inset) / SWC_ATLAS_SIZE);
    out[1] = (float)(1.0 - (double)(cell->y + cell->h - inset) / SWC_ATLAS_SIZE);
    out[2] = (float)((double)(cell->w - 2 * inset) / SWC_ATLAS_SIZE);
    out[3] = (float)((double)(cell->h - 2 * inset) / SWC_ATLAS_SIZE);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Palette partners, fit, density                                                                       */
/* ---------------------------------------------------------------------------------------------------- */

/* magenta: cyan, yellow, violet / cyan: magenta, violet, orange / violet: cyan, orange, magenta /
 * orange: magenta, yellow, cyan / yellow: magenta, orange, cyan / green: violet, cyan, yellow /
 * red: yellow, orange, cyan / white: cyan, magenta, violet */
static const int PARTNERS[8][3] = {{1, 4, 2}, {0, 2, 3}, {1, 3, 0}, {0, 4, 1},
                                   {0, 3, 1}, {2, 1, 4}, {4, 3, 1}, {1, 0, 2}};

int swc_holo_partner_color(int color, int pick) {
    int c = color < 0 ? 0 : (color > 7 ? 7 : color);
    return PARTNERS[c][((pick % 3) + 3) % 3];
}

#define MAX_STRETCH 1.12
#define CONTENT_FILL 0.93

void swc_holo_content_fit(double panel_aspect, double cell_aspect, double out[2]) {
    double fx = 1.0, fy = 1.0;
    if (panel_aspect > cell_aspect) {
        fx = (cell_aspect / panel_aspect) * MAX_STRETCH;
        if (fx > 1.0)
            fx = 1.0;
    } else {
        fy = (panel_aspect / cell_aspect) * MAX_STRETCH;
        if (fy > 1.0)
            fy = 1.0;
    }
    out[0] = 1.0 / (fx * CONTENT_FILL);
    out[1] = 1.0 / (fy * CONTENT_FILL);
}

bool swc_holo_keep_site(int index, const SwcSite *site, double density) {
    if (density >= 1.0)
        return true;
    if (site->kind == SWC_SITE_HOLO_PANEL && site->w >= SWC_HOLO_ALWAYS_KEEP_WIDTH)
        return true;
    return swc_hash_unit2((uint32_t)index, 0x51a7u) < density;
}

static uint32_t site_hash(int i, const SwcSite *s) {
    uint32_t v[2];
    v[0] = (uint32_t)i;
    v[1] = (uint32_t)(int32_t)floor(s->seed * 65536.0);
    return swc_hash_ints(v, 2);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Panels                                                                                               */
/* ---------------------------------------------------------------------------------------------------- */

void swc_holo_free_panels(SwcPanelPack *p) {
    free(p->data);
    free(p->cell_a);
    free(p->cell_b);
    memset(p, 0, sizeof(*p));
}

bool swc_holo_pack_panels(const SwcCity *city, const SwcAtlasCell *cells, double density, SwcPanelPack *out) {
    memset(out, 0, sizeof(*out));
    const SwcAtlasCell *wide[SWC_ATLAS_CELL_COUNT], *tall[SWC_ATLAS_CELL_COUNT];
    int wn = swc_atlas_cells_of_kind(cells, SWC_CELL_WIDE, wide, SWC_ATLAS_CELL_COUNT);
    int tn = swc_atlas_cells_of_kind(cells, SWC_CELL_TALL, tall, SWC_ATLAS_CELL_COUNT);
    if (wn < 2 || tn < 2)
        return false;
    int n = 0;
    for (int i = 0; i < city->site_count; i++)
        if (city->sites[i].kind == SWC_SITE_HOLO_PANEL && swc_holo_keep_site(i, &city->sites[i], density))
            n++;
    out->count = n;
    out->data = (float *)calloc((size_t)(n > 0 ? n : 1) * SWC_PANEL_STRIDE, sizeof(float));
    out->cell_a = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    out->cell_b = (int *)calloc((size_t)(n > 0 ? n : 1), sizeof(int));
    if (out->data == NULL || out->cell_a == NULL || out->cell_b == NULL) {
        swc_holo_free_panels(out);
        return false;
    }
    int wide_turn = 0, tall_turn = 0, k = 0;
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *s = &city->sites[i];
        if (s->kind != SWC_SITE_HOLO_PANEL || !swc_holo_keep_site(i, s, density))
            continue;
        bool is_tall = s->w < s->h;
        const SwcAtlasCell **group = is_tall ? tall : wide;
        int glen = is_tall ? tn : wn;
        /* Round robin, so every ad is on show somewhere; the second ad is any other of the same shape. */
        int a = (is_tall ? tall_turn++ : wide_turn++) % glen;
        uint32_t h = site_hash(k, s);
        int b = (a + 1 + (int)(h % (uint32_t)(glen - 1))) % glen;
        const SwcAtlasCell *cell_a = group[a], *cell_b = group[b];
        out->cell_a[k] = a;
        out->cell_b[k] = b;
        float *f = &out->data[k * SWC_PANEL_STRIDE];
        f[SWC_PANEL_POS] = (float)s->x;
        f[SWC_PANEL_POS + 1] = (float)s->y;
        f[SWC_PANEL_POS + 2] = (float)s->z;
        f[SWC_PANEL_SIZE] = (float)s->w;
        f[SWC_PANEL_SIZE + 1] = (float)s->h;
        f[SWC_PANEL_SIZE + 2] = (float)s->yaw;
        f[SWC_PANEL_SIZE + 3] = (float)s->seed;
        /* 55% of panels switch ads; a full A-B-A cycle takes 20 to 40 s and divides the loop exactly. */
        bool switches = ((h >> 8) % 100u) < 55u;
        double cycle = 20.0 + (double)((h >> 16) % 2000u) / 100.0;
        double laps = switches ? js_round(SWC_LOOP_SECONDS / cycle) : 0.0;
        f[SWC_PANEL_LOOK] = (float)s->color;
        f[SWC_PANEL_LOOK + 1] = (float)swc_holo_partner_color(s->color, (int)((h >> 4) % 3u));
        f[SWC_PANEL_LOOK + 2] = (float)laps;
        f[SWC_PANEL_LOOK + 3] = (float)((h >> 12) % 4u);
        double fit[2];
        swc_holo_content_fit(s->w / s->h, (double)cell_a->w / (double)cell_a->h, fit);
        f[SWC_PANEL_FIT] = (float)fit[0];
        f[SWC_PANEL_FIT + 1] = (float)fit[1];
        swc_atlas_cell_rect(cell_a, 1, &f[SWC_PANEL_RECT_A]);
        swc_atlas_cell_rect(cell_b, 1, &f[SWC_PANEL_RECT_B]);
        k++;
    }
    return true;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Tickers                                                                                              */
/* ---------------------------------------------------------------------------------------------------- */

/* Scroll speed of ticker text, metres per second, before it is rounded to whole laps per loop. */
#define TICKER_SPEED 11.0

void swc_holo_free_tickers(SwcTickerPack *p) {
    free(p->data);
    memset(p, 0, sizeof(*p));
}

bool swc_holo_pack_tickers(const SwcCity *city, const SwcAtlasCell *cells, double density,
                           SwcTickerPack *out) {
    memset(out, 0, sizeof(*out));
    const SwcAtlasCell *strips[SWC_ATLAS_CELL_COUNT];
    int sn = swc_atlas_cells_of_kind(cells, SWC_CELL_STRIP, strips, SWC_ATLAS_CELL_COUNT);
    if (sn < 1)
        return false;
    double keep =
        density > 0.6 ? density : 0.6; /* tickers mark the megatowers; only the lowest tier thins them */
    int n = 0;
    for (int i = 0; i < city->site_count; i++)
        if (city->sites[i].kind == SWC_SITE_TICKER && swc_holo_keep_site(i, &city->sites[i], keep))
            n++;
    out->count = n;
    out->data = (float *)calloc((size_t)(n > 0 ? n : 1) * SWC_TICKER_STRIDE, sizeof(float));
    if (out->data == NULL)
        return false;
    int k = 0;
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *s = &city->sites[i];
        if (s->kind != SWC_SITE_TICKER || !swc_holo_keep_site(i, s, keep))
            continue;
        const SwcAtlasCell *strip = strips[k % sn];
        double perimeter = 4.0 * (s->w + s->d);
        double strip_length = s->h * ((double)strip->w / (double)strip->h);
        double repeats = js_round(perimeter / strip_length);
        if (repeats < 1.0)
            repeats = 1.0;
        double laps = js_round((TICKER_SPEED * SWC_LOOP_SECONDS) / perimeter);
        if (laps < 1.0)
            laps = 1.0;
        uint32_t h = site_hash(k, s);
        float rect[4];
        swc_atlas_cell_rect(strip, 2, rect);
        float *f = &out->data[k * SWC_TICKER_STRIDE];
        f[SWC_TICKER_POS] = (float)s->x;
        f[SWC_TICKER_POS + 1] = (float)s->y;
        f[SWC_TICKER_POS + 2] = (float)s->z;
        f[SWC_TICKER_EXT] = (float)s->w;
        f[SWC_TICKER_EXT + 1] = (float)s->d;
        f[SWC_TICKER_EXT + 2] = (float)s->h;
        f[SWC_TICKER_EXT + 3] = (float)s->seed;
        f[SWC_TICKER_LOOK] = (float)s->color;
        f[SWC_TICKER_LOOK + 1] = (float)swc_holo_partner_color(s->color, (int)(h % 3u));
        f[SWC_TICKER_LOOK + 2] = (float)repeats;
        f[SWC_TICKER_LOOK + 3] = (float)laps;
        f[SWC_TICKER_RECT] = 0.0f;
        f[SWC_TICKER_RECT + 1] = rect[1];
        f[SWC_TICKER_RECT + 2] = 1.0f;
        f[SWC_TICKER_RECT + 3] = rect[3];
        k++;
    }
    return true;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Wire solids                                                                                          */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct V3 {
    double v[3];
} V3;

static V3 v3(double x, double y, double z) {
    V3 r = {{x, y, z}};
    return r;
}

static V3 scale(V3 a, double s) {
    return v3(a.v[0] * s, a.v[1] * s, a.v[2] * s);
}

static V3 unit(V3 a) {
    return scale(a, 1.0 / sqrt(a.v[0] * a.v[0] + a.v[1] * a.v[1] + a.v[2] * a.v[2]));
}

static V3 midpoint(V3 a, V3 b) {
    return unit(v3(a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2]));
}

typedef struct Part {
    double spin[4];
    double color, gain, kind, line, fill, bob;
} Part;

typedef struct Builder {
    float *data;
    int vertices, cap;
    bool failed;
    V3 site;
    double seed, radius;
    Part part;
} Builder;

static void vertex(Builder *b, V3 p, V3 bary) {
    if (b->failed)
        return;
    if (b->vertices == b->cap) {
        int cap = b->cap == 0 ? 4096 : b->cap * 2;
        float *d = (float *)realloc(b->data, (size_t)cap * SWC_WIRE_STRIDE * sizeof(float));
        if (d == NULL) {
            b->failed = true;
            return;
        }
        b->data = d;
        b->cap = cap;
    }
    float *f = &b->data[(size_t)b->vertices * SWC_WIRE_STRIDE];
    const Part *t = &b->part;
    for (int k = 0; k < 3; k++) {
        f[SWC_WIRE_POSITION + k] = (float)p.v[k];
        f[SWC_WIRE_BARY + k] = (float)bary.v[k];
        f[SWC_WIRE_ANCHOR + k] = (float)b->site.v[k];
    }
    for (int k = 0; k < 4; k++)
        f[SWC_WIRE_SPIN + k] = (float)t->spin[k];
    f[SWC_WIRE_LOOK] = (float)t->color;
    f[SWC_WIRE_LOOK + 1] = (float)t->gain;
    f[SWC_WIRE_LOOK + 2] = (float)t->kind;
    f[SWC_WIRE_LOOK + 3] = (float)b->seed;
    f[SWC_WIRE_STYLE] = (float)t->line;
    f[SWC_WIRE_STYLE + 1] = (float)t->fill;
    f[SWC_WIRE_STYLE + 2] = (float)b->radius;
    f[SWC_WIRE_STYLE + 3] = (float)t->bob;
    b->vertices++;
}

static void use(Builder *b, double kind, double color, double gain, double fill, double line, double bob,
                double s0, double s1, double s2, double s3) {
    Part p = {{s0, s1, s2, s3}, color, gain, kind, line, fill, bob};
    b->part = p;
}

/* A triangle; bc, ca and ab say whether the edge opposite a, b and c is drawn. */
static void tri(Builder *b, V3 a, V3 bb, V3 c, bool bc, bool ca, bool ab) {
    double x = bc ? 0.0 : 1.0, y = ca ? 0.0 : 1.0, z = ab ? 0.0 : 1.0;
    vertex(b, a, v3(1, y, z));
    vertex(b, bb, v3(x, 1, z));
    vertex(b, c, v3(x, y, 1));
}

/* A quad a-b-c-d, split along a-c; each flag says whether that side is drawn. */
static void quad(Builder *b, V3 a, V3 bb, V3 c, V3 d, bool ab, bool bc, bool cd, bool da) {
    tri(b, a, bb, c, bc, false, ab);
    tri(b, a, c, d, cd, da, false);
}

/* A triangle that carries raw coordinates in place of edge data (the cone and the glow). */
static void raw(Builder *b, V3 a, V3 bb, V3 c, V3 ta, V3 tb, V3 tc) {
    vertex(b, a, ta);
    vertex(b, bb, tb);
    vertex(b, c, tc);
}

static void add_icosahedron(Builder *b, double r, bool subdivide) {
    double t = (1.0 + sqrt(5.0)) / 2.0;
    V3 verts[12] = {v3(-1, t, 0),  v3(1, t, 0),  v3(-1, -t, 0), v3(1, -t, 0), v3(0, -1, t),  v3(0, 1, t),
                    v3(0, -1, -t), v3(0, 1, -t), v3(t, 0, -1),  v3(t, 0, 1),  v3(-t, 0, -1), v3(-t, 0, 1)};
    for (int i = 0; i < 12; i++)
        verts[i] = unit(verts[i]);
    static const int faces[20][3] = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                     {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                     {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                     {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}};
    for (int f = 0; f < 20; f++) {
        V3 p = verts[faces[f][0]], q = verts[faces[f][1]], s = verts[faces[f][2]];
        if (!subdivide) {
            tri(b, scale(p, r), scale(q, r), scale(s, r), true, true, true);
            continue;
        }
        V3 pq = midpoint(p, q), qs = midpoint(q, s), sp = midpoint(s, p);
        tri(b, scale(p, r), scale(pq, r), scale(sp, r), true, true, true);
        tri(b, scale(q, r), scale(qs, r), scale(pq, r), true, true, true);
        tri(b, scale(s, r), scale(sp, r), scale(qs, r), true, true, true);
        tri(b, scale(pq, r), scale(qs, r), scale(sp, r), true, true, true);
    }
}

static void add_octahedron(Builder *b, double r, double height, V3 centre) {
#define AT(x, y, z) v3(centre.v[0] + (x), centre.v[1] + (y), centre.v[2] + (z))
    V3 ring[4] = {AT(r, 0, 0), AT(0, 0, r), AT(-r, 0, 0), AT(0, 0, -r)};
    for (int i = 0; i < 4; i++) {
        V3 p = ring[i], q = ring[(i + 1) % 4];
        tri(b, AT(0, height, 0), p, q, true, true, true);
        tri(b, AT(0, -height, 0), q, p, true, true, true);
    }
#undef AT
}

static V3 torus_at(double major, double minor, int nu, int nv, int i, int j) {
    double u = ((double)i / nu) * PI * 2.0, v = ((double)j / nv) * PI * 2.0;
    double ring = major + minor * cos(v);
    return v3(ring * cos(u), minor * sin(v), ring * sin(u));
}

static void add_torus(Builder *b, double major, double minor, int nu, int nv) {
    for (int i = 0; i < nu; i++)
        for (int j = 0; j < nv; j++)
            quad(b, torus_at(major, minor, nu, nv, i, j), torus_at(major, minor, nu, nv, i + 1, j),
                 torus_at(major, minor, nu, nv, i + 1, j + 1), torus_at(major, minor, nu, nv, i, j + 1), true,
                 true, true, true);
}

static V3 sphere_at(double r, int lon, int lat, int i, int j) {
    double u = ((double)i / lon) * PI * 2.0, v = ((double)j / lat) * PI;
    return v3(r * sin(v) * cos(u), r * cos(v), r * sin(v) * sin(u));
}

static void add_sphere(Builder *b, double r, int lon, int lat) {
    for (int i = 0; i < lon; i++)
        for (int j = 0; j < lat; j++) {
            if (j == 0)
                tri(b, sphere_at(r, lon, lat, i, 0), sphere_at(r, lon, lat, i + 1, 1),
                    sphere_at(r, lon, lat, i, 1), true, true, true);
            else if (j == lat - 1)
                tri(b, sphere_at(r, lon, lat, i, j), sphere_at(r, lon, lat, i + 1, j),
                    sphere_at(r, lon, lat, i, lat), true, true, true);
            else
                quad(b, sphere_at(r, lon, lat, i, j), sphere_at(r, lon, lat, i + 1, j),
                     sphere_at(r, lon, lat, i + 1, j + 1), sphere_at(r, lon, lat, i, j + 1), true, true, true,
                     true);
        }
}

static V3 annulus_at(double y, int segments, int i, double r) {
    double a = ((double)i / segments) * PI * 2.0;
    return v3(r * cos(a), y, r * sin(a));
}

/* A flat ring in the xz plane at height y; every `tick`-th spoke is drawn. */
static void add_annulus(Builder *b, double inner, double outer, double y, int segments, int tick) {
    for (int i = 0; i < segments; i++)
        quad(b, annulus_at(y, segments, i, inner), annulus_at(y, segments, i + 1, inner),
             annulus_at(y, segments, i + 1, outer), annulus_at(y, segments, i, outer), true,
             (i + 1) % tick == 0, true, i % tick == 0);
}

/* A hoop: a short cylinder wall round the y axis; every `tick`-th upright is drawn. */
static void add_hoop(Builder *b, double r, double half_height, int segments, int tick) {
    for (int i = 0; i < segments; i++) {
        double a0 = ((double)i / segments) * PI * 2.0, a1 = ((double)(i + 1) / segments) * PI * 2.0;
        quad(b, v3(r * cos(a0), -half_height, r * sin(a0)), v3(r * cos(a1), -half_height, r * sin(a1)),
             v3(r * cos(a1), half_height, r * sin(a1)), v3(r * cos(a0), half_height, r * sin(a0)), true,
             (i + 1) % tick == 0, true, i % tick == 0);
    }
}

static V3 pyramid_corner(double half, double y0, double y1, int k, double t, double lift) {
    double sx = (k == 0 || k == 3) ? -1.0 : 1.0, sz = k < 2 ? -1.0 : 1.0;
    double h = half * (1.0 - t);
    return v3(sx * h, y0 + (y1 - y0) * t + lift, sz * h);
}

/* A square pyramid cut into bands; the top band floats free as a capstone. */
static void add_pyramid(Builder *b, double half, double y0, double y1, int bands, double cap_lift,
                        bool cap_only) {
    for (int k = 0; k < 4; k++) {
        int n = (k + 1) % 4;
        for (int j = 0; j < bands; j++) {
            bool cap = j == bands - 1;
            if (cap != cap_only)
                continue;
            double t0 = (double)j / bands, t1 = (double)(j + 1) / bands, lift = cap ? cap_lift : 0.0;
            if (cap)
                tri(b, pyramid_corner(half, y0, y1, k, t0, lift), pyramid_corner(half, y0, y1, n, t0, lift),
                    pyramid_corner(half, y0, y1, k, 1.0, lift), true, true, true);
            else
                quad(b, pyramid_corner(half, y0, y1, k, t0, lift), pyramid_corner(half, y0, y1, n, t0, lift),
                     pyramid_corner(half, y0, y1, n, t1, lift), pyramid_corner(half, y0, y1, k, t1, lift),
                     true, true, true, true);
        }
    }
    if (!cap_only)
        quad(b, pyramid_corner(half, y0, y1, 0, 0, 0), pyramid_corner(half, y0, y1, 1, 0, 0),
             pyramid_corner(half, y0, y1, 2, 0, 0), pyramid_corner(half, y0, y1, 3, 0, 0), true, true, true,
             true);
}

/* Whole laps per loop for a turn that takes about `seconds`; never 0. */
static double laps_of(double seconds, double sign) {
    double n = js_round(4096.0 / seconds);
    return sign * (n > 1.0 ? n : 1.0);
}

static const V3 ORIGIN = {{0, 0, 0}};

static void add_shape(Builder *b, int shape, double r, double color_a, double color_b, double phase,
                      uint32_t h) {
    double dir = (h & 1u) ? 1.0 : -1.0;
    double line = fmax(0.12, r * 0.006);
    /* `solid` of the web version: kind edges, bob 1, gain 1 unless a part says otherwise. */
#define SOLID(color, fill, gain, s0, s1, s2, s3)                                                             \
    use(b, SWC_WIRE_KIND_EDGES, (color), (gain), (fill), line, 1.0, (s0), (s1), (s2), (s3))
    switch (shape) {
    case 0: /* icosphere */
        SOLID(color_a, 0.05, 1.0, laps_of(46, dir), phase, 0.35, laps_of(120, -dir));
        add_icosahedron(b, r * 0.96, true);
        SOLID(color_b, 0.16, 1.15, laps_of(17, -dir), phase, 0.9, laps_of(40, dir));
        add_icosahedron(b, r * 0.4, false);
        break;
    case 1: /* torus */
        SOLID(color_a, 0.045, 1.0, laps_of(30, dir), phase, 1.05, laps_of(75, dir));
        add_torus(b, r * 0.7, r * 0.27, 30, 12);
        SOLID(color_b, 0.2, 1.15, laps_of(12, -dir), phase, 0.6, laps_of(31, -dir));
        add_octahedron(b, r * 0.2, r * 0.3, ORIGIN);
        break;
    case 2: /* globe */
        SOLID(color_a, 0.04, 1.0, laps_of(50, dir), phase, 0.41, 0);
        add_sphere(b, r * 0.86, 24, 12);
        SOLID(color_b, 0.12, 1.2, laps_of(21, -dir), phase, 0.5, laps_of(160, dir));
        add_annulus(b, r * 1.08, r * 1.2, 0, 60, 5);
        add_octahedron(b, r * 0.07, r * 0.07, v3(r * 1.14, 0, 0));
        break;
    case 3: /* pyramid */
        SOLID(color_a, 0.06, 1.0, laps_of(38, dir), phase, 0, 0);
        add_pyramid(b, r * 0.82, -r * 0.5, r * 0.82, 5, r * 0.09, false);
        SOLID(7, 0.3, 1.1, laps_of(38, dir), phase, 0, 0);
        add_pyramid(b, r * 0.82, -r * 0.5, r * 0.82, 5, r * 0.09, true);
        SOLID(color_b, 0.08, 1.0, laps_of(26, -dir), phase, 0, 0);
        add_pyramid(b, r * 0.5, -r * 0.6, -r * 1.0, 3, 0, false);
        add_pyramid(b, r * 0.5, -r * 0.6, -r * 1.0, 3, 0, true);
        break;
    case 4: { /* rings */
        const int count = 7;
        for (int k = 0; k < count; k++) {
            double y = (-0.84 + (1.68 * k) / (count - 1)) * r;
            double rk = sqrt(fmax(r * r - y * y, 0.0));
            double turn = k % 2 == 0 ? dir : -dir;
            SOLID(k % 2 == 0 ? color_a : color_b, 0.22, 1.0, laps_of(24 + 5 * k, turn), phase + k * 0.13, 0,
                  0);
            add_annulus(b, rk * 0.8, rk, y, 40, 4);
        }
        SOLID(7, 0.3, 1.0, laps_of(9, dir), phase, 0, 0);
        add_octahedron(b, r * 0.12, r * 0.9, ORIGIN);
        break;
    }
    case 5: { /* gyro */
        static const double hoops[3][4] = {{1, 1.5708, 36, 90}, {0.8, 1.0, 22, 47}, {0.6, 0.55, 14, 29}};
        for (int k = 0; k < 3; k++) {
            double turn = k % 2 == 0 ? dir : -dir;
            SOLID(k == 1 ? color_b : color_a, 0.4, 1.3, laps_of(hoops[k][2], turn), phase + k * 0.21,
                  hoops[k][1], laps_of(hoops[k][3], -turn));
            add_hoop(b, r * hoops[k][0], r * 0.085, 48, 4);
        }
        SOLID(color_b, 0.2, 1.2, laps_of(10, dir), phase, 0.7, laps_of(23, dir));
        add_icosahedron(b, r * 0.26, false);
        break;
    }
    default:
        break;
    }
#undef SOLID
}

/* The glow sprite, the projector cone and the ring on the plaza floor that every object shares. */
static void add_stage(Builder *b, const SwcSite *site, double color_a, double phase) {
    double r = site->w;
    /* Soft inner glow: one camera-facing quad. */
    use(b, SWC_WIRE_KIND_GLOW, color_a, 1.0, 0.0, 0.0, 1.0, 0, 0, 0, 0);
    double s = r * 1.9;
    raw(b, v3(-1, -1, s), v3(1, -1, s), v3(1, 1, s), v3(-1, -1, 0), v3(1, -1, 0), v3(1, 1, 0));
    raw(b, v3(-1, -1, s), v3(1, 1, s), v3(-1, 1, s), v3(-1, -1, 0), v3(1, 1, 0), v3(-1, 1, 0));
    /* The cone of light from the plaza floor up to the object. */
    const int segments = 24;
    double y_base = 0.6 - site->y, y_top = -r * 0.3;
    use(b, SWC_WIRE_KIND_CONE, color_a, 1.0, 0.0, 0.0, 0.0, 0, 0, 0, 0);
    for (int i = 0; i < segments; i++) {
        double u0 = (double)i / segments, u1 = (double)(i + 1) / segments;
        V3 a = annulus_at(y_base, segments, i, r * 0.1), c = annulus_at(y_base, segments, i + 1, r * 0.1),
           d = annulus_at(y_top, segments, i + 1, r * 0.78), e = annulus_at(y_top, segments, i, r * 0.78);
        raw(b, a, c, d, v3(0, u0, 0), v3(0, u1, 0), v3(1, u1, 0));
        raw(b, a, d, e, v3(0, u0, 0), v3(1, u1, 0), v3(1, u0, 0));
    }
    /* The projector ring on the floor. */
    use(b, SWC_WIRE_KIND_EDGES, color_a, 1.3, 0.5, 0.12, 0.0, laps_of(40, 1), phase, 0, 0);
    add_annulus(b, r * 0.42, r * 0.5, y_base, 48, 4);
    add_annulus(b, r * 0.1, r * 0.14, y_base, 24, 24);
}

int swc_holo_pick_objects(const SwcCity *city, double density, const SwcSite **out, int cap) {
    int n = 0;
    double keep = density > 0.5 ? density : 0.5;
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *s = &city->sites[i];
        if (s->kind == SWC_SITE_HOLO_OBJECT && swc_holo_keep_site(i, s, keep) && n < cap)
            out[n++] = s;
    }
    return n;
}

void swc_holo_free_wire(SwcWireMesh *m) {
    free(m->data);
    free(m->shapes);
    memset(m, 0, sizeof(*m));
}

bool swc_holo_build_wire(const SwcSite *const *objects, int count, SwcWireMesh *out) {
    memset(out, 0, sizeof(*out));
    Builder b;
    memset(&b, 0, sizeof(b));
    out->shapes = (int *)calloc((size_t)(count > 0 ? count : 1), sizeof(int));
    if (out->shapes == NULL)
        return false;
    for (int i = 0; i < count; i++) {
        const SwcSite *s = objects[i];
        uint32_t h = site_hash(i, s);
        int shape = i % SWC_WIRE_SHAPE_COUNT;
        out->shapes[i] = shape;
        double color_b = (double)swc_holo_partner_color(s->color, (int)((h >> 4) % 3u));
        /* The seed is quantised here so the shader can hash it exactly. */
        b.site = v3(s->x, s->y, s->z);
        b.seed = floor(s->seed * 4096.0);
        b.radius = s->w;
        add_shape(&b, shape, s->w, (double)s->color, color_b, s->seed, h);
        add_stage(&b, s, (double)s->color, s->seed);
    }
    if (b.failed) {
        free(b.data);
        free(out->shapes);
        memset(out, 0, sizeof(*out));
        return false;
    }
    out->object_count = count;
    out->vertex_count = b.vertices;
    out->data = b.data;
    return true;
}
