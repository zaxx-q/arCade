/* Synthwave City - the hologram ad atlas. See swc_atlas.h. A port of the web version's atlas.ts: the same
 * layouts, sizes, colours and random draws, so the adverts are the same drawings. */
#include "swc_atlas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "swc_core.h"
#include "swc_font.h"

#define TAU (3.14159265358979323846 * 2.0)
#define PI_ 3.14159265358979323846

/* ---- inks: one colour channel each. Bit 24 marks a colour as set (so black is a colour). ---- */
#define SET 0x1000000u
#define NONE 0x2000000u
#define IA (SET | 0xff0000u)
#define IB (SET | 0x00ff00u)
#define IW (SET | 0x0000ffu)
#define IAW (SET | 0xff00ffu)
/* ink A warmed with a little white: for large hot areas, where full AW would bloom to a white blob */
#define IAH (SET | 0xff0078u)
#define IBW (SET | 0x00ffffu)
/* ink B with a little white, for the tops of large gradient lettering */
#define IBH (SET | 0x00ff80u)
#define IAB (SET | 0xffff00u)
#define IK (SET | 0x000000u)
/* dim versions, for large fills that must stay under the bloom threshold */
#define IA2 (SET | 0xc00000u)
#define IB2 (SET | 0x00c000u)
#define IA3 (SET | 0x800000u)
#define IB3 (SET | 0x008000u)

typedef SwcCanvas G;

typedef struct Txt {
    double size;
    int weight; /* 0: 800 */
    bool italic;
    double track;          /* em */
    int align;             /* 0 left, 1 center, 2 right */
    uint32_t fill;         /* 0: ink W; NONE: outline only */
    uint32_t grad0, grad1; /* a top-to-bottom gradient fill when grad0 is set */
    uint32_t stroke;       /* 0: none */
    double stroke_w;       /* 0: size * 0.09 */
    double max_w;          /* 0: no limit */
    double stretch;        /* 0: 1 */
    bool mono;
} Txt;

#define LEFT 0
#define CENTER 1
#define RIGHT 2

/* Draws one line of text with y at the middle of the capitals; returns the width it took. */
static double type_at(G *g, const char *s, double x, double y, const Txt *st, bool dry) {
    SwcFontStyle fs;
    fs.size = st->size;
    fs.weight = st->weight != 0 ? st->weight : 800;
    fs.italic = st->italic;
    fs.mono = st->mono;
    fs.spacing = st->track * st->size;
    double track = fs.spacing;
    double natural = fmax(1.0, swc_font_measure(&fs, s) - track);
    double sx = st->stretch != 0.0 ? st->stretch : 1.0;
    if (st->max_w > 0.0 && natural * sx > st->max_w)
        sx = st->max_w / natural;
    double drawn = natural * sx;
    if (dry)
        return drawn;
    swc_cv_save(g);
    swc_cv_translate(g, st->align == CENTER ? x - drawn / 2 : (st->align == RIGHT ? x - drawn : x),
                     y + st->size * 0.36);
    swc_cv_scale(g, sx, 1);
    if (st->stroke != 0) {
        double lw = st->stroke_w > 0 ? st->stroke_w : st->size * 0.09;
        swc_cv_fill_rgb(g, st->stroke);
        swc_font_draw(g, &fs, s, 0, 0, lw, st->fill == NONE);
    }
    if (st->grad0 != 0) {
        SwcGradient gr = swc_gradient2(0, -st->size * 0.74, 0, 0, st->grad0, st->grad1);
        swc_cv_fill_gradient(g, &gr);
        swc_font_draw(g, &fs, s, 0, 0, 0, false);
    } else if (st->fill != NONE) {
        swc_cv_fill_rgb(g, st->fill != 0 ? st->fill : IW);
        swc_font_draw(g, &fs, s, 0, 0, 0, false);
    }
    swc_cv_restore(g);
    return drawn;
}

#define TXT(g, s, x, y, ...) type_at((g), (s), (x), (y), &(Txt){__VA_ARGS__}, false)
#define MEASURE(s, ...) type_at(NULL, (s), 0, 0, &(Txt){__VA_ARGS__}, true)

/* Letters stacked downwards, for the tall banners. */
static void stack(G *g, const char *s, double x, double y0, double step, Txt st) {
    st.align = CENTER;
    for (int i = 0; s[i]; i++) {
        char one[2] = {s[i], 0};
        type_at(g, one, x, y0 + i * step, &st, false);
    }
}

static void rr(G *g, double x, double y, double w, double h, double r) {
    double q = fmin(r, fmin(w / 2, h / 2));
    swc_cv_begin_path(g);
    swc_cv_move_to(g, x + q, y);
    swc_cv_arc_to(g, x + w, y, x + w, y + h, q);
    swc_cv_arc_to(g, x + w, y + h, x, y + h, q);
    swc_cv_arc_to(g, x, y + h, x, y, q);
    swc_cv_arc_to(g, x, y, x + w, y, q);
    swc_cv_close_path(g);
}

static void poly(G *g, const double *pts, int n) { /* n values: x, y pairs */
    swc_cv_begin_path(g);
    swc_cv_move_to(g, pts[0], pts[1]);
    for (int i = 2; i < n; i += 2)
        swc_cv_line_to(g, pts[i], pts[i + 1]);
    swc_cv_close_path(g);
}
#define POLY(g, ...)                                                                                         \
    do {                                                                                                     \
        const double pts_[] = {__VA_ARGS__};                                                                 \
        poly((g), pts_, (int)(sizeof(pts_) / sizeof(double)));                                               \
    } while (0)

static void line(G *g, double x0, double y0, double x1, double y1, uint32_t ink, double width) {
    swc_cv_stroke_rgb(g, ink);
    swc_cv_line_width(g, width);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, x0, y0);
    swc_cv_line_to(g, x1, y1);
    swc_cv_stroke(g);
}

static void disc(G *g, double x, double y, double r, uint32_t ink) {
    swc_cv_fill_rgb(g, ink);
    swc_cv_begin_path(g);
    swc_cv_arc(g, x, y, r, 0, TAU, false);
    swc_cv_fill(g, SWC_NONZERO);
}

static void ring(G *g, double x, double y, double r, uint32_t ink, double width) {
    swc_cv_stroke_rgb(g, ink);
    swc_cv_line_width(g, width);
    swc_cv_begin_path(g);
    swc_cv_arc(g, x, y, r, 0, TAU, false);
    swc_cv_stroke(g);
}

static void set_vgrad(G *g, double y0, double y1, uint32_t top, uint32_t bottom) {
    SwcGradient gr = swc_gradient2(0, y0, 0, y1, top, bottom);
    swc_cv_fill_gradient(g, &gr);
}

static void set_hgrad(G *g, double x0, double x1, uint32_t left, uint32_t right) {
    SwcGradient gr = swc_gradient2(x0, 0, x1, 0, left, right);
    swc_cv_fill_gradient(g, &gr);
}

/* The striped sun: a gradient disc with horizontal cuts that widen towards the bottom. */
static void sun(G *g, double cx, double cy, double r, uint32_t top, uint32_t bottom, int cuts) {
    swc_cv_save(g);
    swc_cv_begin_path(g);
    swc_cv_arc(g, cx, cy, r, 0, TAU, false);
    swc_cv_clip(g, SWC_NONZERO);
    set_vgrad(g, cy - r, cy + r, top, bottom);
    swc_cv_fill_rect(g, cx - r, cy - r, r * 2, r * 2);
    swc_cv_fill_rgb(g, IK);
    for (int i = 0; i < cuts; i++) {
        double t = (double)i / (double)(cuts - 1);
        swc_cv_fill_rect(g, cx - r, cy + r * (0.02 + 0.86 * t), r * 2, r * (0.035 + 0.085 * t));
    }
    swc_cv_restore(g);
}

/* A perspective floor grid between the horizon y0 and y1. */
static void floor_grid(G *g, double x0, double x1, double y0, double y1, uint32_t ink, double width) {
    swc_cv_save(g);
    swc_cv_begin_path(g);
    swc_cv_rect(g, x0, y0, x1 - x0, y1 - y0);
    swc_cv_clip(g, SWC_NONZERO);
    double cx = (x0 + x1) / 2;
    for (int i = -14; i <= 14; i++)
        line(g, cx + i * 12, y0, cx + i * 70, y1, ink, width);
    for (int i = 0; i < 7; i++) {
        double t = pow((i + 0.35) / 7, 1.9);
        line(g, x0, y0 + (y1 - y0) * t, x1, y0 + (y1 - y0) * t, ink, width);
    }
    swc_cv_restore(g);
}

/* Chevrons pointing right, starting at x; size is their height. */
static void chevrons(G *g, double x, double y, double size, int count, uint32_t ink, double gap) {
    swc_cv_fill_rgb(g, ink);
    double t = size * 0.34;
    for (int i = 0; i < count; i++) {
        double px = x + i * size * gap;
        POLY(g, px, y - size / 2, px + t, y - size / 2, px + t + size * 0.42, y, px + t, y + size / 2, px,
             y + size / 2, px + size * 0.42, y);
        swc_cv_fill(g, SWC_NONZERO);
    }
}

static void palm(G *g, double x, double y, double h, double lean, uint32_t ink) {
    double tx = x + lean * h * 0.3, ty = y - h;
    swc_cv_stroke_rgb(g, ink);
    swc_cv_fill_rgb(g, ink);
    swc_cv_line_cap(g, SWC_CAP_ROUND);
    swc_cv_line_width(g, h * 0.05);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, x, y);
    swc_cv_quad_to(g, x + lean * h * 0.02, y - h * 0.55, tx, ty);
    swc_cv_stroke(g);
    for (int i = 0; i < 7; i++) {
        double a = -PI_ * (0.03 + (0.94 * i) / 6);
        double len = h * (0.5 + 0.08 * cos(i * 2.4));
        double ex = tx + cos(a) * len, ey = ty + sin(a) * len * 0.5 + len * 0.34;
        double mx = tx + cos(a) * len * 0.5, my = ty + sin(a) * len * 0.62;
        swc_cv_begin_path(g);
        swc_cv_move_to(g, tx, ty);
        swc_cv_quad_to(g, mx, my - len * 0.16, ex, ey);
        swc_cv_quad_to(g, mx, my + len * 0.1, tx, ty);
        swc_cv_fill(g, SWC_NONZERO);
    }
    swc_cv_line_cap(g, SWC_CAP_BUTT);
}

/* A block skyline standing on y, with windows punched out. */
static void skyline(G *g, double x0, double x1, double y, double max_h, SwcRng *rng, uint32_t ink) {
    double x = x0;
    while (x < x1) {
        double w = swc_rng_range(rng, 14, 34);
        double h = max_h * swc_rng_range(rng, 0.25, 1);
        swc_cv_fill_rgb(g, ink);
        swc_cv_fill_rect(g, x, y - h, fmin(w, x1 - x), h);
        if (swc_rng_chance(rng, 0.4))
            swc_cv_fill_rect(g, x + w / 2 - 1, y - h - swc_rng_range(rng, 6, 18), 2, 18);
        swc_cv_fill_rgb(g, IK);
        for (double wy = y - h + 5; wy < y - 4; wy += 7)
            for (double wx = x + 4; wx < x + w - 5; wx += 6)
                if (swc_rng_chance(rng, 0.45))
                    swc_cv_fill_rect(g, wx, wy, 3, 3);
        x += w + swc_rng_range(rng, 1, 4);
    }
}

static void burst(G *g, double cx, double cy, double r, int spikes, uint32_t ink) {
    swc_cv_fill_rgb(g, ink);
    swc_cv_begin_path(g);
    for (int i = 0; i < spikes * 2; i++) {
        double a = ((double)i / (spikes * 2)) * TAU - PI_ / 2;
        double rad = i % 2 == 0 ? r : r * 0.74;
        swc_cv_line_to(g, cx + cos(a) * rad, cy + sin(a) * rad);
    }
    swc_cv_close_path(g);
    swc_cv_fill(g, SWC_NONZERO);
}

/* A rounded badge with text knocked out of it (filled) or drawn inside it (outlined). */
static void badge(G *g, double x, double y, double w, double h, const char *text, uint32_t ink, bool filled) {
    rr(g, x, y, w, h, h * 0.28);
    if (filled) {
        swc_cv_fill_rgb(g, ink);
        swc_cv_fill(g, SWC_NONZERO);
    } else {
        swc_cv_stroke_rgb(g, ink);
        swc_cv_line_width(g, fmax(2, h * 0.09));
        swc_cv_stroke(g);
    }
    TXT(g, text, x + w / 2, y + h / 2, .size = h * 0.56, .weight = 900, .track = 0.12, .align = CENTER,
        .fill = filled ? IK : ink, .max_w = w * 0.84);
}

static void bars(G *g, double x, double y, double w, double h, int count, SwcRng *rng, uint32_t top,
                 uint32_t bottom) {
    double step = w / count;
    set_vgrad(g, y - h, y, top, bottom);
    for (int i = 0; i < count; i++) {
        double bh = h * (0.2 + 0.8 * fabs(sin(i * 0.9 + swc_rng_range(rng, 0, 1.2))));
        for (double s = 0; s < bh; s += 7)
            swc_cv_fill_rect(g, x + i * step, y - s - 5, step * 0.68, 5);
    }
}

/* Invented signage glyphs: strokes on a 3 x 3 lattice. They are shapes, so they need no font. */
static const int GLYPH_SEGMENTS[16][2] = {{0, 2}, {3, 5}, {6, 8}, {0, 1}, {4, 5}, {7, 8}, {0, 6}, {1, 7},
                                          {2, 8}, {1, 4}, {5, 8}, {3, 6}, {2, 4}, {4, 6}, {0, 4}, {4, 8}};
#define GLYPH_SEGMENT_COUNT 16

static void glyph(G *g, double x, double y, double s, SwcRng *rng, uint32_t ink) {
    swc_cv_stroke_rgb(g, ink);
    swc_cv_line_width(g, s * 0.15);
    swc_cv_line_cap(g, SWC_CAP_SQUARE);
    int n = swc_rng_int(rng, 3, 4);
    int start = swc_rng_int(rng, 0, GLYPH_SEGMENT_COUNT - 1);
    swc_cv_begin_path(g);
    for (int k = 0; k < n; k++) {
        int step = swc_rng_int(rng, 2, 5);
        const int *seg = GLYPH_SEGMENTS[(start + k * step) % GLYPH_SEGMENT_COUNT];
        int p = seg[0], q = seg[1];
        swc_cv_move_to(g, x + (p % 3) * s * 0.5, y + (p / 3) * s * 0.5);
        swc_cv_line_to(g, x + (q % 3) * s * 0.5, y + (q / 3) * s * 0.5);
    }
    swc_cv_stroke(g);
    swc_cv_line_cap(g, SWC_CAP_BUTT);
}

static void glyph_run(G *g, double x, double y, double s, int count, SwcRng *rng, uint32_t ink,
                      bool vertical) {
    for (int i = 0; i < count; i++)
        glyph(g, vertical ? x : x + i * s * 1.5, vertical ? y + i * s * 1.5 : y, s, rng, ink);
}

/* A field of dots that shrink from one side to the other. */
static void halftone(G *g, double x, double y, double w, double h, uint32_t ink, double pitch) {
    swc_cv_fill_rgb(g, ink);
    for (int j = 0; j * pitch < h; j++) {
        for (int i = 0; i * pitch < w; i++) {
            double r = pitch * 0.46 * (1 - (i * pitch) / w);
            if (r < 0.5)
                continue;
            swc_cv_begin_path(g);
            swc_cv_arc(g, x + i * pitch + (j % 2) * pitch * 0.5, y + j * pitch, r, 0, TAU, false);
            swc_cv_fill(g, SWC_NONZERO);
        }
    }
}

/* Marquee bulbs round a rectangle. */
static void bulbs(G *g, double x, double y, double w, double h, double pitch, uint32_t on, uint32_t off) {
    int nx = (int)floor(w / pitch + 0.5), ny = (int)floor(h / pitch + 0.5);
    int k = 0;
#define BULB(px, py)                                                                                         \
    do {                                                                                                     \
        disc(g, (px), (py), pitch * 0.24, k % 2 == 0 ? on : off);                                            \
        k++;                                                                                                 \
    } while (0)
    for (int i = 0; i < nx; i++)
        BULB(x + (i * w) / nx, y);
    for (int i = 0; i < ny; i++)
        BULB(x + w, y + (i * h) / ny);
    for (int i = 0; i < nx; i++)
        BULB(x + w - (i * w) / nx, y + h);
    for (int i = 0; i < ny; i++)
        BULB(x, y + h - (i * h) / ny);
#undef BULB
}

static const char *const HEART[] = {".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X..."};
static const char *const COIN[] = {".XXXX.", "XX..XX", "X.XX.X", "X.XX.X", "XX..XX", ".XXXX."};

static void pixels(G *g, const char *const *rows, int nrows, double x, double y, double s, uint32_t ink) {
    swc_cv_fill_rgb(g, ink);
    for (int j = 0; j < nrows; j++) {
        const char *row = rows[j];
        for (int i = 0; row[i]; i++)
            if (row[i] == 'X')
                swc_cv_fill_rect(g, x + i * s, y + j * s, s - 1, s - 1);
    }
}

static void can(G *g, double cx, double cy, double w, double h, double tilt) {
    swc_cv_save(g);
    swc_cv_translate(g, cx, cy);
    swc_cv_rotate(g, tilt);
    SwcGradient body;
    memset(&body, 0, sizeof(body));
    body.x0 = -w / 2;
    body.x1 = w / 2;
    body.stop_count = 4;
    body.stop_at[0] = 0;
    body.stop_at[1] = 0.3;
    body.stop_at[2] = 0.55;
    body.stop_at[3] = 1;
    body.stop_rgb[0] = IA2;
    body.stop_rgb[1] = IAH;
    body.stop_rgb[2] = IA;
    body.stop_rgb[3] = IA3;
    rr(g, -w / 2, -h / 2, w, h, w * 0.16);
    swc_cv_fill_gradient(g, &body);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IW);
    swc_cv_fill_rect(g, -w * 0.42, -h / 2, w * 0.84, h * 0.03);
    swc_cv_fill_rect(g, -w * 0.42, h / 2 - h * 0.03, w * 0.84, h * 0.03);
    swc_cv_fill_rgb(g, IK);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, -w / 2, h * 0.02);
    swc_cv_bezier_to(g, -w * 0.15, -h * 0.2, w * 0.15, h * 0.22, w / 2, -h * 0.06);
    swc_cv_line_to(g, w / 2, h * 0.14);
    swc_cv_bezier_to(g, w * 0.15, h * 0.42, -w * 0.15, 0, -w / 2, h * 0.22);
    swc_cv_close_path(g);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_stroke_rgb(g, IB);
    swc_cv_line_width(g, w * 0.07);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, -w / 2, h * 0.12);
    swc_cv_bezier_to(g, -w * 0.15, -h * 0.1, w * 0.15, h * 0.32, w / 2, h * 0.04);
    swc_cv_stroke(g);
    swc_cv_restore(g);
}

static void bowl(G *g, double cx, double cy, double r) {
    /* steam */
    swc_cv_stroke_rgb(g, IW);
    swc_cv_line_width(g, r * 0.07);
    swc_cv_line_cap(g, SWC_CAP_ROUND);
    for (int i = -1; i <= 1; i++) {
        double x = cx + i * r * 0.42;
        swc_cv_begin_path(g);
        swc_cv_move_to(g, x, cy - r * 0.3);
        swc_cv_bezier_to(g, x - r * 0.25, cy - r * 0.62, x + r * 0.25, cy - r * 0.86, x, cy - r * 1.2);
        swc_cv_stroke(g);
    }
    /* noodles over the rim, then chopsticks */
    swc_cv_stroke_rgb(g, IAB);
    swc_cv_line_width(g, r * 0.06);
    for (int i = 0; i < 5; i++) {
        swc_cv_begin_path(g);
        swc_cv_arc(g, cx - r * 0.6 + i * r * 0.3, cy - r * 0.02, r * 0.2, PI_, TAU, false);
        swc_cv_stroke(g);
    }
    line(g, cx + r * 0.1, cy - r * 0.05, cx + r * 1.15, cy - r * 0.85, IW, r * 0.06);
    line(g, cx + r * 0.3, cy - r * 0.02, cx + r * 1.25, cy - r * 0.68, IW, r * 0.06);
    swc_cv_line_cap(g, SWC_CAP_BUTT);
    /* the bowl */
    set_vgrad(g, cy, cy + r * 0.8, IA, IA2);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, cx - r, cy);
    swc_cv_line_to(g, cx + r, cy);
    swc_cv_bezier_to(g, cx + r * 0.95, cy + r * 0.62, cx + r * 0.5, cy + r * 0.78, cx + r * 0.36,
                     cy + r * 0.78);
    swc_cv_line_to(g, cx - r * 0.36, cy + r * 0.78);
    swc_cv_bezier_to(g, cx - r * 0.5, cy + r * 0.78, cx - r * 0.95, cy + r * 0.62, cx - r, cy);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IB);
    swc_cv_fill_rect(g, cx - r * 0.86, cy + r * 0.22, r * 1.72, r * 0.1);
    swc_cv_fill_rgb(g, IA);
    swc_cv_fill_rect(g, cx - r * 0.42, cy + r * 0.8, r * 0.84, r * 0.12);
}

static void car(G *g, double x, double y, double w) {
    double h = w * 0.3;
#define PX(u) (x + (u) * w)
#define PY(v) (y - h + (v) * h)
    set_hgrad(g, x, x + w, IA, IAH);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, PX(0), PY(0.78));
    swc_cv_line_to(g, PX(0.03), PY(0.5));
    swc_cv_line_to(g, PX(0.27), PY(0.4));
    swc_cv_line_to(g, PX(0.43), PY(0.06));
    swc_cv_line_to(g, PX(0.66), PY(0.04));
    swc_cv_line_to(g, PX(0.8), PY(0.36));
    swc_cv_line_to(g, PX(0.97), PY(0.46));
    swc_cv_line_to(g, PX(1), PY(0.78));
    swc_cv_close_path(g);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IK);
    POLY(g, PX(0.45), PY(0.13), PX(0.64), PY(0.11), PX(0.74), PY(0.36), PX(0.33), PY(0.4));
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rect(g, PX(0.555), PY(0.1), w * 0.012, h * 0.3);
    line(g, PX(0.05), PY(0.6), PX(0.96), PY(0.6), IK, h * 0.035);
    static const double wheels[2] = {0.2, 0.78};
    for (int i = 0; i < 2; i++) {
        double u = wheels[i];
        disc(g, PX(u), PY(0.8), h * 0.25, IK);
        ring(g, PX(u), PY(0.8), h * 0.18, IW, h * 0.05);
        disc(g, PX(u), PY(0.8), h * 0.05, IW);
    }
    /* tail light and the headlight's throw */
    swc_cv_fill_rgb(g, IW);
    swc_cv_fill_rect(g, PX(0.005), PY(0.5), w * 0.03, h * 0.07);
    set_hgrad(g, PX(0.97), PX(1.22), IBW, IK);
    POLY(g, PX(0.98), PY(0.48), PX(1.22), PY(0.34), PX(1.22), PY(0.78), PX(0.98), PY(0.56));
    swc_cv_fill(g, SWC_NONZERO);
#undef PX
#undef PY
}

static void cassette(G *g, double x, double y, double w) {
    double h = w * 0.62;
    rr(g, x, y, w, h, w * 0.04);
    swc_cv_stroke_rgb(g, IW);
    swc_cv_line_width(g, w * 0.022);
    swc_cv_stroke(g);
    rr(g, x + w * 0.08, y + h * 0.12, w * 0.84, h * 0.54, w * 0.03);
    swc_cv_fill_rgb(g, IA2);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IA);
    swc_cv_fill_rect(g, x + w * 0.08, y + h * 0.12, w * 0.84, h * 0.1);
    swc_cv_fill_rgb(g, IAB);
    swc_cv_fill_rect(g, x + w * 0.08, y + h * 0.22, w * 0.84, h * 0.035);
    rr(g, x + w * 0.2, y + h * 0.32, w * 0.6, h * 0.26, h * 0.13);
    swc_cv_fill_rgb(g, IK);
    swc_cv_fill(g, SWC_NONZERO);
    static const double reels[2] = {0.32, 0.68};
    for (int r = 0; r < 2; r++) {
        double u = reels[r];
        ring(g, x + w * u, y + h * 0.45, h * 0.085, IB, w * 0.02);
        for (int i = 0; i < 3; i++) {
            double a = ((double)i / 3) * TAU + u * 5;
            line(g, x + w * u, y + h * 0.45, x + w * u + cos(a) * h * 0.075,
                 y + h * 0.45 + sin(a) * h * 0.075, IB, w * 0.012);
        }
    }
    swc_cv_fill_rgb(g, IB2);
    swc_cv_fill_rect(g, x + w * 0.41, y + h * 0.4, w * 0.18, h * 0.1);
    swc_cv_stroke_rgb(g, IW);
    swc_cv_line_width(g, w * 0.016);
    POLY(g, x + w * 0.2, y + h, x + w * 0.26, y + h * 0.78, x + w * 0.74, y + h * 0.78, x + w * 0.8, y + h);
    swc_cv_stroke(g);
    static const double holes[4] = {0.34, 0.44, 0.56, 0.66};
    for (int i = 0; i < 4; i++)
        disc(g, x + w * holes[i], y + h * 0.89, w * 0.014, IW);
}

static void ring_half(G *g, double cx, double cy, double r, double tilt, double start, double end) {
    swc_cv_stroke_rgb(g, IB);
    swc_cv_line_width(g, r * 0.13);
    swc_cv_begin_path(g);
    swc_cv_ellipse(g, cx, cy, r * 1.75, r * 0.42, tilt, start, end, false);
    swc_cv_stroke(g);
    swc_cv_stroke_rgb(g, IW);
    swc_cv_line_width(g, r * 0.03);
    swc_cv_begin_path(g);
    swc_cv_ellipse(g, cx, cy, r * 1.52, r * 0.34, tilt, start, end, false);
    swc_cv_stroke(g);
}

static void planet(G *g, double cx, double cy, double r) {
    double tilt = -0.32;
    ring_half(g, cx, cy, r, tilt, PI_, TAU);
    swc_cv_save(g);
    swc_cv_begin_path(g);
    swc_cv_arc(g, cx, cy, r, 0, TAU, false);
    swc_cv_clip(g, SWC_NONZERO);
    set_vgrad(g, cy - r, cy + r, IAH, IA2);
    swc_cv_fill_rect(g, cx - r, cy - r, r * 2, r * 2);
    swc_cv_fill_rgb(g, IK);
    for (int i = 0; i < 4; i++)
        swc_cv_fill_rect(g, cx - r, cy - r * 0.5 + i * r * 0.42, r * 2, r * (0.04 + i * 0.025));
    swc_cv_restore(g);
    ring_half(g, cx, cy, r, tilt, 0, PI_);
}

static void bolt(G *g, double cx, double cy, double h) {
#define BP(u, v) cx + (u) * h, cy + (v) * h
    POLY(g, BP(0.1, -0.5), BP(-0.26, 0.06), BP(-0.04, 0.06), BP(-0.14, 0.5), BP(0.28, -0.1), BP(0.05, -0.1),
         BP(0.2, -0.5));
#undef BP
    set_vgrad(g, cy - h / 2, cy + h / 2, IAH, IA);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_stroke_rgb(g, IB);
    swc_cv_line_width(g, h * 0.03);
    swc_cv_line_join(g, SWC_JOIN_MITER);
    swc_cv_stroke(g);
}

static void eye(G *g, double cx, double cy, double w) {
    double h = w * 0.3;
    swc_cv_begin_path(g);
    swc_cv_move_to(g, cx - w / 2, cy);
    swc_cv_quad_to(g, cx, cy - h * 1.9, cx + w / 2, cy);
    swc_cv_quad_to(g, cx, cy + h * 1.9, cx - w / 2, cy);
    swc_cv_close_path(g);
    swc_cv_stroke_rgb(g, IW);
    swc_cv_line_width(g, w * 0.03);
    swc_cv_stroke(g);
    swc_cv_save(g);
    swc_cv_clip(g, SWC_NONZERO);
    disc(g, cx, cy, h * 0.92, IA2);
    for (int i = 0; i < 3; i++)
        ring(g, cx, cy, h * (0.92 - i * 0.24), i == 1 ? IAW : IA, w * 0.022);
    disc(g, cx, cy, h * 0.3, IK);
    disc(g, cx - h * 0.28, cy - h * 0.3, h * 0.13, IW);
    swc_cv_restore(g);
    for (int i = 0; i < 4; i++) {
        double a = ((double)i / 4) * TAU;
        line(g, cx + cos(a) * w * 0.56, cy + sin(a) * h * 1.4, cx + cos(a) * w * 0.63, cy + sin(a) * h * 1.75,
             IB, w * 0.02);
    }
}

static void moon(G *g, double cx, double cy, double r, uint32_t ink) {
    swc_cv_save(g);
    swc_cv_begin_path(g);
    swc_cv_arc(g, cx, cy, r, 0, TAU, false);
    swc_cv_arc(g, cx + r * 0.45, cy - r * 0.2, r * 0.86, 0, TAU, true);
    swc_cv_clip(g, SWC_EVENODD);
    disc(g, cx, cy, r, ink);
    swc_cv_restore(g);
}

static void speed_lines(G *g, double x0, double x1, double y0, double y1, SwcRng *rng, uint32_t ink) {
    for (double y = y0; y < y1; y += 9) {
        double a = swc_rng_range(rng, x0, x0 + (x1 - x0) * 0.6);
        double len = swc_rng_range(rng, 30, (x1 - x0) * 0.5);
        double lw = swc_rng_range(rng, 1.5, 4);
        line(g, a, y, fmin(x1, a + len), y, ink, lw);
    }
}

/* ---------------------------------------------------------------------------------------------------- */
/* Wide adverts, 512 x 256                                                                              */
/* ---------------------------------------------------------------------------------------------------- */

typedef void (*AdFn)(G *g, SwcRng *rng);

/* NOVA COLA */
static void wide_nova_cola(G *g, SwcRng *rng) {
    (void)rng;
    can(g, 92, 130, 78, 176, -0.2);
    static const double dots[6][3] = {{168, 52, 9},   {186, 96, 5},  {160, 150, 7},
                                      {182, 196, 11}, {204, 152, 4}, {148, 104, 4}};
    for (int i = 0; i < 6; i++)
        ring(g, dots[i][0], dots[i][1], dots[i][2], IB, 2.5);
    TXT(g, "NOVA", 214, 74, .size = 96, .weight = 900, .italic = true, .fill = IA, .stroke = IW,
        .stroke_w = 5, .max_w = 280);
    TXT(g, "COLA", 236, 152, .size = 96, .weight = 900, .italic = true, .grad0 = IBH, .grad1 = IB,
        .max_w = 258);
    set_hgrad(g, 214, 498, IA, IB);
    swc_cv_fill_rect(g, 214, 201, 284, 4);
    TXT(g, "ICE COLD / ZERO GRAVITY", 216, 224, .size = 17, .weight = 700, .track = 0.2, .fill = IW,
        .max_w = 280);
}

/* HYPERLINE */
static void wide_hyperline(G *g, SwcRng *rng) {
    speed_lines(g, 14, 330, 20, 76, rng, IA2);
    chevrons(g, 340, 48, 54, 4, IB, 0.7);
    TXT(g, "HYPERLINE", 256, 112, .size = 74, .weight = 900, .italic = true, .track = 0.04, .align = CENTER,
        .fill = IW, .stroke = IA, .stroke_w = 9, .max_w = 470);
    set_hgrad(g, 20, 492, IA, IB);
    swc_cv_fill_rect(g, 20, 152, 472, 5);
    /* the train: a long pill with a tapered nose and a window strip */
    set_hgrad(g, 20, 400, IA3, IAH);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, 14, 178);
    swc_cv_line_to(g, 330, 178);
    swc_cv_quad_to(g, 388, 180, 412, 206);
    swc_cv_line_to(g, 14, 206);
    swc_cv_close_path(g);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IK);
    for (double x = 26; x < 320; x += 22)
        swc_cv_fill_rect(g, x, 184, 15, 7);
    POLY(g, 334, 184, 362, 186, 384, 198, 334, 198);
    swc_cv_fill(g, SWC_NONZERO);
    line(g, 14, 212, 500, 212, IB, 2);
    TXT(g, "MAGLEV", 498, 182, .size = 15, .weight = 800, .track = 0.2, .align = RIGHT, .fill = IB,
        .max_w = 76);
    TXT(g, "EXPRESS", 498, 200, .size = 15, .weight = 800, .track = 0.2, .align = RIGHT, .fill = IB,
        .max_w = 76);
    TXT(g, "DOWNTOWN IN 4 MIN", 256, 232, .size = 17, .weight = 700, .track = 0.3, .align = CENTER,
        .fill = IW, .mono = true, .max_w = 470);
}

/* KIRO NOODLE BAR */
static void wide_kiro(G *g, SwcRng *rng) {
    bowl(g, 104, 150, 76);
    TXT(g, "KIRO", 206, 82, .size = 118, .weight = 900, .fill = IA, .stroke = IW, .stroke_w = 5,
        .stretch = 1.08, .max_w = 250);
    TXT(g, "NOODLE BAR", 208, 156, .size = 34, .weight = 800, .track = 0.16, .fill = IB, .max_w = 248);
    badge(g, 208, 190, 170, 40, "OPEN 25 HRS", IW, false);
    glyph_run(g, 474, 34, 20, 6, rng, IAB, true);
    line(g, 462, 26, 462, 232, IA2, 2);
}

/* SUNSET FM 88.8 */
static void wide_sunset_fm(G *g, SwcRng *rng) {
    sun(g, 118, 118, 92, IAH, IA, 6);
    line(g, 14, 214, 226, 214, IB, 2);
    TXT(g, "SUNSET FM", 240, 46, .size = 46, .weight = 900, .italic = true, .fill = IW, .max_w = 258);
    TXT(g, "88.8", 236, 122, .size = 112, .weight = 900, .italic = true, .grad0 = IBH, .grad1 = IB,
        .stroke = IK, .max_w = 262);
    badge(g, 242, 188, 104, 40, "ON AIR", IA, true);
    bars(g, 362, 230, 136, 46, 12, rng, IAB, IA);
    TXT(g, "ALL NIGHT SYNTH", 120, 234, .size = 13, .weight = 700, .track = 0.28, .align = CENTER, .fill = IB,
        .max_w = 210);
}

/* ZEPHYRA MOTORS */
static void wide_zephyra(G *g, SwcRng *rng) {
    (void)rng;
    floor_grid(g, 12, 500, 168, 244, IB2, 1.6);
    line(g, 12, 168, 500, 168, IB, 2.5);
    car(g, 70, 190, 300);
    TXT(g, "ZEPHYRA", 256, 42, .size = 52, .weight = 900, .track = 0.3, .align = CENTER, .fill = IW,
        .max_w = 440);
    TXT(g, "MOTORS", 256, 82, .size = 17, .weight = 700, .track = 0.9, .align = CENTER, .fill = IB,
        .max_w = 300);
    line(g, 60, 82, 150, 82, IA, 2);
    line(g, 362, 82, 452, 82, IA, 2);
    TXT(g, "VECTOR GT", 498, 208, .size = 24, .weight = 900, .italic = true, .align = RIGHT, .fill = IA,
        .stroke = IK, .stroke_w = 6, .max_w = 120);
    TXT(g, "TWIN TURBO", 498, 232, .size = 13, .weight = 700, .track = 0.2, .align = RIGHT, .fill = IW,
        .stroke = IK, .stroke_w = 5, .mono = true, .max_w = 120);
}

/* ARCADE PALACE */
static void wide_arcade_palace(G *g, SwcRng *rng) {
    (void)rng;
    bulbs(g, 22, 22, 468, 212, 18, IW, IA2);
    TXT(g, "ARCADE", 256, 86, .size = 96, .weight = 900, .align = CENTER, .fill = IA, .stroke = IW,
        .stroke_w = 5, .max_w = 400);
    TXT(g, "PALACE", 256, 150, .size = 40, .weight = 800, .track = 0.5, .align = CENTER, .fill = IB,
        .max_w = 380);
    for (int i = 0; i < 3; i++)
        pixels(g, HEART, 6, 62 + i * 40, 184, 5, IAW);
    pixels(g, COIN, 6, 418, 182, 6, IAB);
    TXT(g, "INSERT COIN", 300, 200, .size = 22, .weight = 700, .track = 0.12, .align = CENTER, .fill = IW,
        .mono = true, .max_w = 200);
}

/* LUX CAPSULE HOTEL */
static void wide_lux(G *g, SwcRng *rng) {
    (void)rng;
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            bool lit = i == 1 && j == 1;
            rr(g, 26 + i * 56, 40 + j * 62, 46, 50, 16);
            if (lit) {
                set_vgrad(g, 40 + j * 62, 90 + j * 62, IAH, IA);
                swc_cv_fill(g, SWC_NONZERO);
            }
            swc_cv_stroke_rgb(g, lit ? IW : IB2);
            swc_cv_line_width(g, 3);
            swc_cv_stroke(g);
        }
    }
    moon(g, 470, 46, 22, IW);
    TXT(g, "LUX", 212, 84, .size = 112, .weight = 900, .fill = IAH, .stroke = IW, .stroke_w = 3,
        .stretch = 1.25, .max_w = 220);
    TXT(g, "CAPSULE HOTEL", 214, 148, .size = 28, .weight = 800, .track = 0.2, .fill = IB, .max_w = 282);
    badge(g, 214, 184, 150, 42, "VACANCY", IA, true);
    TXT(g, "FROM 39 A NIGHT", 380, 205, .size = 13, .weight = 700, .track = 0.12, .fill = IW, .max_w = 118);
}

/* ORBIT AIRLINES */
static void wide_orbit(G *g, SwcRng *rng) {
    (void)rng;
    planet(g, 112, 138, 56);
    SwcGradient gr = swc_gradient2(150, 0, 480, 0, IA2, IAW);
    swc_cv_stroke_gradient(g, &gr);
    swc_cv_line_width(g, 5);
    swc_cv_begin_path(g);
    swc_cv_move_to(g, 206, 212);
    swc_cv_quad_to(g, 452, 214, 480, 48);
    swc_cv_stroke(g);
    POLY(g, 488, 26, 486, 66, 470, 50, 448, 46);
    swc_cv_fill_rgb(g, IW);
    swc_cv_fill(g, SWC_NONZERO);
    TXT(g, "ORBIT", 216, 84, .size = 96, .weight = 900, .italic = true, .fill = IW, .stroke = IA,
        .stroke_w = 8, .max_w = 226);
    TXT(g, "AIRLINES", 220, 146, .size = 26, .weight = 800, .track = 0.5, .fill = IB, .max_w = 200);
    TXT(g, "FLY THE NEON SKY", 222, 178, .size = 14, .weight = 700, .track = 0.24, .fill = IW, .max_w = 170);
    TXT(g, "GATE 88 / NOW BOARDING", 498, 234, .size = 13, .weight = 700, .track = 0.12, .align = RIGHT,
        .fill = IAB, .mono = true, .max_w = 230);
}

/* CLUB COSTA NEON */
static void wide_costa(G *g, SwcRng *rng) {
    (void)rng;
    sun(g, 132, 132, 70, IAB, IA, 5);
    for (int i = 0; i < 6; i++) {
        double y = 208 + i * 6, half = 108 - i * 15;
        line(g, 132 - half, y, 132 + half, y, i % 2 == 0 ? IB : IB2, 2.5);
    }
    palm(g, 62, 206, 150, 0.5, IK);
    palm(g, 62, 206, 150, 0.5, IB2);
    palm(g, 204, 206, 112, -0.45, IB2);
    TXT(g, "COSTA", 250, 76, .size = 92, .weight = 900, .italic = true, .fill = IA, .stroke = IW,
        .stroke_w = 5, .max_w = 250);
    TXT(g, "NEON", 262, 150, .size = 86, .weight = 900, .italic = true, .fill = NONE, .stroke = IBW,
        .stroke_w = 5, .track = 0.08, .max_w = 236);
    TXT(g, "BEACH CLUB / SUBLEVEL 3", 252, 216, .size = 15, .weight = 700, .track = 0.2, .fill = IW,
        .max_w = 246);
}

/* STRATA TOWERS */
static void wide_strata(G *g, SwcRng *rng) {
    skyline(g, 14, 498, 244, 92, rng, IA2);
    set_vgrad(g, 70, 244, IBW, IB2);
    POLY(g, 392, 244, 392, 96, 410, 70, 428, 96, 428, 244);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_fill_rgb(g, IK);
    for (double y = 104; y < 240; y += 9)
        swc_cv_fill_rect(g, 397, y, 26, 3);
    line(g, 410, 70, 410, 40, IW, 2);
    disc(g, 410, 38, 4, IW);
    TXT(g, "LIVE ABOVE", 20, 50, .size = 50, .weight = 900, .italic = true, .fill = IW, .max_w = 330);
    TXT(g, "IT ALL", 20, 100, .size = 50, .weight = 900, .italic = true, .fill = IA, .stroke = IW,
        .stroke_w = 4, .max_w = 250);
    TXT(g, "STRATA TOWERS", 22, 142, .size = 20, .weight = 800, .track = 0.34, .fill = IB, .stroke = IK,
        .stroke_w = 6, .max_w = 300);
    burst(g, 462, 54, 40, 14, IAB);
    TXT(g, "LEVEL", 462, 44, .size = 13, .weight = 900, .align = CENTER, .fill = IK, .max_w = 52);
    TXT(g, "400", 462, 62, .size = 22, .weight = 900, .align = CENTER, .fill = IK, .max_w = 52);
}

/* MIRA-9 */
static void wide_mira(G *g, SwcRng *rng) {
    (void)rng;
    for (int i = 0; i < 5; i++)
        ring(g, 112, 128, 22 + i * 17, i % 2 == 0 ? IA : IB2, i == 0 ? 6 : 2.5);
    disc(g, 112, 128, 11, IW);
    swc_cv_stroke_rgb(g, IBW);
    swc_cv_line_width(g, 3);
    swc_cv_begin_path(g);
    for (double x = 14; x <= 210; x += 4) {
        double k = exp(-pow((x - 112) / 52, 2));
        swc_cv_line_to(g, x, 128 + sin(x * 0.42) * 28 * k);
    }
    swc_cv_stroke(g);
    TXT(g, "MIRA", 228, 78, .size = 104, .weight = 900, .fill = IW, .stroke = IA, .stroke_w = 8,
        .stretch = 1.1, .max_w = 216);
    TXT(g, "9", 456, 62, .size = 60, .weight = 900, .italic = true, .fill = IAB, .max_w = 44);
    TXT(g, "PERSONAL A.I. COMPANION", 230, 148, .size = 16, .weight = 800, .track = 0.2, .fill = IB,
        .max_w = 268);
    line(g, 230, 168, 498, 168, IA2, 2);
    TXT(g, "SHE REMEMBERS EVERYTHING", 230, 192, .size = 17, .weight = 700, .italic = true, .fill = IW,
        .max_w = 268);
    TXT(g, "NOW IN EVERY HOME", 230, 222, .size = 12, .weight = 700, .track = 0.3, .fill = IA, .mono = true,
        .max_w = 268);
}

/* MEGAMALL */
static void wide_megamall(G *g, SwcRng *rng) {
    (void)rng;
    chevrons(g, 16, 70, 96, 3, IA2, 0.5);
    chevrons(g, 148, 70, 96, 1, IA, 0.5);
    TXT(g, "MEGAMALL", 244, 62, .size = 60, .weight = 900, .italic = true, .fill = IW, .stroke = IA,
        .stroke_w = 7, .max_w = 254);
    TXT(g, "LEVEL 88", 246, 110, .size = 24, .weight = 800, .track = 0.4, .fill = IB, .max_w = 250);
    burst(g, 88, 184, 56, 16, IAB);
    TXT(g, "SALE", 88, 170, .size = 22, .weight = 900, .align = CENTER, .fill = IK, .max_w = 80);
    TXT(g, "-50%", 88, 196, .size = 30, .weight = 900, .align = CENTER, .fill = IK, .max_w = 84);
    set_hgrad(g, 170, 498, IB, IA);
    swc_cv_fill_rect(g, 170, 150, 328, 5);
    TXT(g, "EVERYTHING.", 172, 186, .size = 30, .weight = 900, .fill = IA, .max_w = 200);
    TXT(g, "ALWAYS.", 380, 186, .size = 30, .weight = 900, .fill = IB, .max_w = 118);
    TXT(g, "4000 SHOPS / 9 FOOD COURTS / 1 SKY", 172, 224, .size = 13, .weight = 700, .track = 0.1,
        .fill = IW, .mono = true, .max_w = 326);
}

/* KILOVOLT ENERGY */
static void wide_kilovolt(G *g, SwcRng *rng) {
    (void)rng;
    for (int i = 0; i < 18; i++) {
        double a = ((double)i / 18) * TAU;
        double reach = i % 2 == 0 ? 106 : 84;
        line(g, 104 + cos(a) * 62, 128 + sin(a) * 62, 104 + cos(a) * reach, 128 + sin(a) * reach, IB2, 3);
    }
    bolt(g, 104, 128, 196);
    halftone(g, 384, 24, 118, 60, IA2, 10);
    TXT(g, "KILOVOLT", 206, 96, .size = 88, .weight = 900, .italic = true, .fill = IAB, .stroke = IK,
        .stroke_w = 4, .max_w = 290);
    TXT(g, "ENERGY DRINK", 210, 152, .size = 26, .weight = 800, .track = 0.3, .fill = IW, .max_w = 284);
    set_hgrad(g, 210, 498, IA, IB);
    swc_cv_fill_rect(g, 210, 176, 288, 5);
    TXT(g, "STAY UP FOREVER", 210, 208, .size = 22, .weight = 900, .italic = true, .fill = IB, .max_w = 284);
}

/* MAGNETA TAPES */
static void wide_magneta(G *g, SwcRng *rng) {
    (void)rng;
    /* the three-band corner stripe */
    swc_cv_save(g);
    swc_cv_begin_path(g);
    swc_cv_rect(g, 12, 12, 488, 232);
    swc_cv_clip(g, SWC_NONZERO);
    static const uint32_t inks[3] = {IA, IAB, IB};
    for (int i = 0; i < 3; i++) {
        swc_cv_fill_rgb(g, inks[i]);
        POLY(g, 300 + i * 26, 12, 322 + i * 26, 12, 142 + i * 26, 244, 120 + i * 26, 244);
        swc_cv_fill(g, SWC_NONZERO);
    }
    swc_cv_restore(g);
    swc_cv_fill_rgb(g, IK);
    swc_cv_fill_rect(g, 24, 50, 220, 152);
    cassette(g, 30, 60, 208);
    TXT(g, "MAGNETA", 498, 66, .size = 54, .weight = 900, .align = RIGHT, .fill = IW, .stroke = IK,
        .stroke_w = 12, .max_w = 226);
    TXT(g, "CHROME C-90", 498, 112, .size = 22, .weight = 800, .track = 0.2, .align = RIGHT, .fill = IB,
        .stroke = IK, .stroke_w = 8, .max_w = 200);
    TXT(g, "RECORD", 498, 176, .size = 34, .weight = 900, .italic = true, .align = RIGHT, .fill = IA,
        .stroke = IK, .stroke_w = 8, .max_w = 170);
    TXT(g, "THE NIGHT", 498, 214, .size = 34, .weight = 900, .italic = true, .align = RIGHT, .fill = IW,
        .stroke = IK, .stroke_w = 8, .max_w = 170);
}

/* OKU OPTICS */
static void wide_oku(G *g, SwcRng *rng) {
    eye(g, 120, 118, 196);
    TXT(g, "OKU", 250, 86, .size = 124, .weight = 900, .fill = IA, .stroke = IW, .stroke_w = 5,
        .stretch = 1.1, .max_w = 244);
    TXT(g, "OPTICS", 254, 160, .size = 30, .weight = 800, .track = 0.62, .fill = IB, .max_w = 240);
    TXT(g, "SEE IN NEON", 120, 220, .size = 18, .weight = 800, .track = 0.3, .align = CENTER, .fill = IW,
        .max_w = 196);
    glyph_run(g, 258, 196, 18, 9, rng, IAB, false);
    static const double marks[4][2] = {{20, 20}, {492, 20}, {20, 236}, {492, 236}};
    for (int i = 0; i < 4; i++) {
        double x = marks[i][0], y = marks[i][1];
        line(g, x - 6, y, x + 6, y, IW, 2);
        line(g, x, y - 6, x, y + 6, IW, 2);
    }
}

/* GRAVBALL FINALS */
static void wide_gravball(G *g, SwcRng *rng) {
    (void)rng;
    halftone(g, 14, 62, 150, 120, IA3, 11);
    swc_cv_save(g);
    swc_cv_translate(g, 498, 0);
    swc_cv_scale(g, -1, 1);
    halftone(g, 0, 62, 150, 120, IB3, 11);
    swc_cv_restore(g);
    TXT(g, "GRAVBALL FINALS", 256, 34, .size = 26, .weight = 900, .track = 0.34, .align = CENTER, .fill = IW,
        .max_w = 460);
    line(g, 24, 56, 488, 56, IAB, 2);
    TXT(g, "CRIMSON", 20, 122, .size = 60, .weight = 900, .italic = true, .fill = IA, .stroke = IW,
        .stroke_w = 4, .max_w = 188);
    TXT(g, "COBALT", 492, 122, .size = 60, .weight = 900, .italic = true, .align = RIGHT, .fill = IB,
        .stroke = IW, .stroke_w = 4, .max_w = 188);
    POLY(g, 256, 76, 302, 122, 256, 168, 210, 122);
    swc_cv_fill_rgb(g, IK);
    swc_cv_fill(g, SWC_NONZERO);
    swc_cv_stroke_rgb(g, IAB);
    swc_cv_line_width(g, 4);
    swc_cv_stroke(g);
    TXT(g, "VS", 256, 122, .size = 44, .weight = 900, .italic = true, .align = CENTER, .fill = IW,
        .max_w = 62);
    badge(g, 108, 192, 84, 38, "LIVE", IA, true);
    TXT(g, "TONIGHT / CHANNEL 404", 204, 211, .size = 22, .weight = 800, .track = 0.14, .fill = IW,
        .max_w = 290);
}

static const AdFn WIDE_ADS[] = {
    wide_nova_cola, wide_hyperline, wide_kiro,  wide_sunset_fm, wide_zephyra, wide_arcade_palace,
    wide_lux,       wide_orbit,     wide_costa, wide_strata,    wide_mira,    wide_megamall,
    wide_kilovolt,  wide_magneta,   wide_oku,   wide_gravball,
};
#define WIDE_AD_COUNT ((int)(sizeof(WIDE_ADS) / sizeof(WIDE_ADS[0])))

static void arrow_down(G *g, double cx, double y, double size, uint32_t ink) {
    swc_cv_fill_rgb(g, ink);
    POLY(g, cx - size * 0.2, y, cx + size * 0.2, y, cx + size * 0.2, y + size * 0.5, cx + size * 0.5,
         y + size * 0.5, cx, y + size, cx - size * 0.5, y + size * 0.5, cx - size * 0.2, y + size * 0.5);
    swc_cv_fill(g, SWC_NONZERO);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Tall adverts, 256 x 512                                                                              */
/* ---------------------------------------------------------------------------------------------------- */

/* KIRO */
static void tall_kiro(G *g, SwcRng *rng) {
    stack(g, "KIRO", 112, 62, 78,
          (Txt){.size = 92, .weight = 900, .fill = IA, .stroke = IW, .stroke_w = 5, .max_w = 130});
    glyph_run(g, 206, 34, 20, 9, rng, IAB, true);
    line(g, 190, 26, 190, 304, IA2, 2);
    bowl(g, 124, 424, 58);
    TXT(g, "NOODLE BAR", 128, 488, .size = 20, .weight = 800, .track = 0.2, .align = CENTER, .fill = IB,
        .max_w = 220);
}

/* HOTEL */
static void tall_hotel(G *g, SwcRng *rng) {
    (void)rng;
    moon(g, 128, 50, 24, IW);
    stack(g, "HOTEL", 128, 128, 60,
          (Txt){.size = 70, .weight = 900, .grad0 = IBH, .grad1 = IB, .max_w = 110});
    line(g, 44, 96, 44, 396, IA, 3);
    line(g, 212, 96, 212, 396, IA, 3);
    badge(g, 38, 404, 180, 40, "VACANCY", IA, true);
    arrow_down(g, 128, 452, 46, IAB);
}

/* ARCADE */
static void tall_arcade(G *g, SwcRng *rng) {
    (void)rng;
    bulbs(g, 24, 24, 208, 464, 18, IW, IA2);
    stack(g, "ARCADE", 128, 72, 54,
          (Txt){.size = 62, .weight = 900, .fill = IA, .stroke = IW, .stroke_w = 4, .max_w = 110});
    pixels(g, HEART, 6, 93, 392, 10, IAW);
    TXT(g, "OPEN ALL NIGHT", 128, 474, .size = 13, .weight = 700, .track = 0.16, .align = CENTER, .fill = IB,
        .mono = true, .max_w = 180);
}

/* NOVA COLA */
static void tall_nova_cola(G *g, SwcRng *rng) {
    (void)rng;
    TXT(g, "NOVA", 128, 56, .size = 76, .weight = 900, .italic = true, .align = CENTER, .fill = IA,
        .stroke = IW, .stroke_w = 4, .max_w = 216);
    can(g, 128, 262, 116, 268, 0.12);
    static const double dots[6][3] = {{40, 150, 8},   {222, 190, 6}, {34, 300, 5},
                                      {226, 330, 10}, {48, 392, 7},  {212, 120, 4}};
    for (int i = 0; i < 6; i++)
        ring(g, dots[i][0], dots[i][1], dots[i][2], IB, 2.5);
    TXT(g, "COLA", 128, 444, .size = 76, .weight = 900, .italic = true, .align = CENTER, .grad0 = IBH,
        .grad1 = IB, .max_w = 216);
    TXT(g, "ICE COLD", 128, 492, .size = 14, .weight = 700, .track = 0.4, .align = CENTER, .fill = IW,
        .max_w = 200);
}

/* OPEN 25 HRS */
static void tall_open(G *g, SwcRng *rng) {
    (void)rng;
    TXT(g, "OPEN", 128, 50, .size = 50, .weight = 900, .track = 0.2, .align = CENTER, .fill = IW,
        .max_w = 210);
    ring(g, 128, 176, 84, IA, 7);
    ring(g, 128, 176, 68, IB2, 2.5);
    for (int i = 0; i < 12; i++) {
        double a = ((double)i / 12) * TAU;
        line(g, 128 + cos(a) * 56, 176 + sin(a) * 56, 128 + cos(a) * 64, 176 + sin(a) * 64, IW,
             i % 3 == 0 ? 4 : 2);
    }
    line(g, 128, 176, 128, 128, IW, 5);
    line(g, 128, 176, 162, 196, IAB, 5);
    disc(g, 128, 176, 6, IW);
    TXT(g, "25", 128, 338, .size = 150, .weight = 900, .italic = true, .align = CENTER, .fill = IA,
        .stroke = IW, .stroke_w = 5, .max_w = 216);
    TXT(g, "HRS", 128, 436, .size = 56, .weight = 900, .track = 0.3, .align = CENTER, .grad0 = IBH,
        .grad1 = IB, .max_w = 200);
    TXT(g, "WE NEVER CLOSE", 128, 488, .size = 13, .weight = 700, .track = 0.2, .align = CENTER, .fill = IW,
        .mono = true, .max_w = 210);
}

/* SUNSET FM */
static void tall_sunset(G *g, SwcRng *rng) {
    sun(g, 128, 122, 92, IAH, IA, 6);
    palm(g, 62, 256, 150, 0.4, IK);
    palm(g, 62, 256, 150, 0.4, IB2);
    line(g, 20, 256, 236, 256, IB, 2.5);
    TXT(g, "88.8", 128, 310, .size = 96, .weight = 900, .italic = true, .align = CENTER, .grad0 = IBH,
        .grad1 = IB, .max_w = 212);
    TXT(g, "SUNSET FM", 128, 374, .size = 30, .weight = 900, .italic = true, .align = CENTER, .fill = IW,
        .max_w = 212);
    bars(g, 30, 452, 196, 44, 14, rng, IAB, IA);
    badge(g, 68, 462, 120, 34, "ON AIR", IA, true);
}

/* a column of invented signage */
static void tall_signage(G *g, SwcRng *rng) {
    rr(g, 26, 26, 204, 460, 14);
    swc_cv_stroke_rgb(g, IA2);
    swc_cv_line_width(g, 3);
    swc_cv_stroke(g);
    glyph_run(g, 52, 52, 56, 5, rng, IA, true);
    glyph_run(g, 150, 52, 56, 5, rng, IBW, true);
    line(g, 128, 44, 128, 440, IB2, 2);
    TXT(g, "LEVEL 88", 128, 466, .size = 18, .weight = 800, .track = 0.3, .align = CENTER, .fill = IW,
        .max_w = 180);
}

/* CLUB PARALLAX */
static void tall_parallax(G *g, SwcRng *rng) {
    TXT(g, "CLUB", 128, 44, .size = 34, .weight = 800, .track = 0.6, .align = CENTER, .fill = IW,
        .max_w = 200);
    for (int i = 0; i < 5; i++) {
        double s = 96 - i * 19;
        POLY(g, 128, 170 - s, 128 + s * 0.92, 170 + s * 0.6, 128 - s * 0.92, 170 + s * 0.6);
        swc_cv_stroke_rgb(g, i % 2 == 0 ? IA : IB);
        swc_cv_line_width(g, i == 0 ? 6 : 3);
        swc_cv_stroke(g);
    }
    swc_cv_save(g);
    swc_cv_translate(g, 128, 318);
    TXT(g, "PARALLAX", 0, 0, .size = 50, .weight = 900, .italic = true, .align = CENTER, .fill = IAW,
        .stroke = IW, .stroke_w = 3, .max_w = 216);
    swc_cv_restore(g);
    bars(g, 26, 440, 204, 76, 13, rng, IB, IA);
    TXT(g, "DANCE TIL DAWN", 128, 476, .size = 17, .weight = 800, .track = 0.22, .align = CENTER, .fill = IW,
        .max_w = 210);
}

static const AdFn TALL_ADS[] = {tall_kiro, tall_hotel,  tall_arcade,  tall_nova_cola,
                                tall_open, tall_sunset, tall_signage, tall_parallax};
#define TALL_AD_COUNT ((int)(sizeof(TALL_ADS) / sizeof(TALL_ADS[0])))

/* ---------------------------------------------------------------------------------------------------- */
/* Ticker strips, 2048 x 128: a run of items laid end to end, then scaled so the run fills the strip exactly
 */
/* and the text wraps without a seam.                                                                    */
/* ---------------------------------------------------------------------------------------------------- */

#define STRIP_Y 64.0

typedef enum ItemKind { ITEM_WORD, ITEM_SPACE, ITEM_ARROW, ITEM_CHEVRON, ITEM_DIAMOND, ITEM_GLYPHS } ItemKind;

typedef struct Item {
    ItemKind kind;
    const char *text;
    uint32_t ink;
    bool italic, mono;
    int weight;
    double track, width;
    int count;
    bool up;
} Item;

#define MAX_ITEMS 24

typedef struct Items {
    Item v[MAX_ITEMS];
    int n;
} Items;

static void add_word(Items *it, const char *s, uint32_t ink, bool italic, bool mono, int weight,
                     double track) {
    Item w = {ITEM_WORD, s, ink, italic, mono, weight, track, 0, 0, false};
    it->v[it->n++] = w;
}
static void add_space(Items *it, double w) {
    Item s = {ITEM_SPACE, NULL, 0, false, false, 0, 0, w, 0, false};
    it->v[it->n++] = s;
}
static void add_arrow(Items *it, bool up, uint32_t ink) {
    Item a = {ITEM_ARROW, NULL, ink, false, false, 0, 0, 0, 0, up};
    it->v[it->n++] = a;
}
static void add_chevrons(Items *it, int count, uint32_t ink) {
    Item c = {ITEM_CHEVRON, NULL, ink, false, false, 0, 0, 0, count, false};
    it->v[it->n++] = c;
}
static void add_diamond(Items *it, uint32_t ink) {
    Item d = {ITEM_DIAMOND, NULL, ink, false, false, 0, 0, 0, 0, false};
    it->v[it->n++] = d;
}
static void add_glyphs(Items *it, int count, uint32_t ink) {
    Item d = {ITEM_GLYPHS, NULL, ink, false, false, 0, 0, 0, count, false};
    it->v[it->n++] = d;
}
static void add_quote(Items *it, const char *name, bool up, const char *value) {
    add_word(it, name, IW, false, true, 700, 0);
    add_arrow(it, up, up ? IB : IA);
    add_word(it, value, up ? IB : IA, false, true, 700, 0);
    add_space(it, 50);
}

static void strip_items(int index, Items *it) {
    it->n = 0;
    switch (index) {
    case 0:
        add_quote(it, "NVC", true, "88.08");
        add_quote(it, "HYPR", false, "12.40");
        add_quote(it, "KIRO", true, "3.33");
        add_quote(it, "ORBT", false, "4.10");
        add_quote(it, "KVLT", true, "125.0");
        break;
    case 1:
        add_word(it, "GRAVBALL FINALS TONIGHT", IW, false, false, 0, 0);
        add_diamond(it, IA);
        add_word(it, "SKYLANE 4 DELAYS", IAB, false, false, 0, 0);
        add_diamond(it, IA);
        add_word(it, "RAIN OF LIGHT AT MIDNIGHT", IB, false, false, 0, 0);
        add_diamond(it, IA);
        break;
    case 2:
        add_chevrons(it, 3, IA);
        add_word(it, "NOVA COLA", IA, true, false, 900, 0);
        add_chevrons(it, 3, IB);
        add_word(it, "HYPERLINE", IW, true, false, 900, 0);
        add_chevrons(it, 3, IA);
        add_word(it, "KIRO NOODLE BAR", IB, false, false, 900, 0);
        add_chevrons(it, 3, IB);
        add_word(it, "KILOVOLT", IAB, true, false, 900, 0);
        break;
    default:
        add_glyphs(it, 4, IA);
        add_word(it, "WELCOME TO THE CITY THAT NEVER DIMS", IW, false, false, 0, 0.06);
        add_glyphs(it, 4, IB);
        add_chevrons(it, 4, IAB);
        add_word(it, "LEVEL 400 SKY DECK", IB, false, false, 0, 0);
        add_chevrons(it, 4, IAB);
        break;
    }
}

/* Draws one item with its left edge at x and returns its width; with `dry` it only measures. */
static double item_run(G *g, const Item *m, double x, bool dry, SwcRng *rng) {
    switch (m->kind) {
    case ITEM_WORD: {
        Txt t = {.size = 78,
                 .weight = m->weight != 0 ? m->weight : 800,
                 .italic = m->italic,
                 .track = m->track,
                 .fill = m->ink,
                 .mono = m->mono};
        return type_at(g, m->text, x, STRIP_Y, &t, dry) + 30;
    }
    case ITEM_SPACE:
        return m->width;
    case ITEM_ARROW:
        if (!dry) {
            swc_cv_fill_rgb(g, m->ink);
            if (m->up)
                POLY(g, x, STRIP_Y + 24, x + 26, STRIP_Y - 26, x + 52, STRIP_Y + 24);
            else
                POLY(g, x, STRIP_Y - 24, x + 26, STRIP_Y + 26, x + 52, STRIP_Y - 24);
            swc_cv_fill(g, SWC_NONZERO);
        }
        return 70;
    case ITEM_CHEVRON:
        if (!dry)
            chevrons(g, x, STRIP_Y, 62, m->count, m->ink, 0.62);
        return m->count * 62 * 0.62 + 50;
    case ITEM_DIAMOND:
        if (!dry) {
            swc_cv_fill_rgb(g, m->ink);
            POLY(g, x + 22, STRIP_Y - 22, x + 44, STRIP_Y, x + 22, STRIP_Y + 22, x, STRIP_Y);
            swc_cv_fill(g, SWC_NONZERO);
        }
        return 76;
    case ITEM_GLYPHS:
        if (!dry)
            glyph_run(g, x, STRIP_Y - 26, 52, m->count, rng, m->ink, false);
        return m->count * 78 + 20;
    }
    return 0;
}

static void draw_strip(G *g, int index, uint32_t seed, double width) {
    Items items;
    SwcRng rng;
    strip_items(index, &items);
    swc_rng_seed(&rng, seed);
    double total = 0;
    for (int i = 0; i < items.n; i++)
        total += item_run(g, &items.v[i], 0, true, &rng);
    swc_cv_scale(g, width / total, 1);
    swc_rng_seed(&rng, seed); /* a fresh generator for the drawing, as the web version makes */
    double x = 0;
    for (int i = 0; i < items.n; i++)
        x += item_run(g, &items.v[i], x, false, &rng);
}

/* ---------------------------------------------------------------------------------------------------- */

SwcCanvas *swc_atlas_create(void) {
    return swc_cv_create(SWC_ATLAS_SIZE, SWC_ATLAS_SIZE);
}

void swc_atlas_draw_cell(SwcCanvas *g, const SwcAtlasCell *cell, uint32_t layer_seed) {
    SwcRng rng;
    swc_rng_seed(&rng, layer_seed + (uint32_t)(cell->x * 31) + (uint32_t)(cell->y * 17));
    swc_cv_save(g);
    swc_cv_translate(g, cell->x, cell->y);
    swc_cv_begin_path(g);
    if (cell->kind == SWC_CELL_STRIP)
        swc_cv_rect(g, 0, SWC_ATLAS_CELL_PAD, cell->w, cell->h - 2 * SWC_ATLAS_CELL_PAD);
    else
        swc_cv_rect(g, SWC_ATLAS_CELL_PAD, SWC_ATLAS_CELL_PAD, cell->w - 2 * SWC_ATLAS_CELL_PAD,
                    cell->h - 2 * SWC_ATLAS_CELL_PAD);
    swc_cv_clip(g, SWC_NONZERO);
    if (cell->kind == SWC_CELL_WIDE)
        WIDE_ADS[cell->index % WIDE_AD_COUNT](g, &rng);
    else if (cell->kind == SWC_CELL_TALL)
        TALL_ADS[cell->index % TALL_AD_COUNT](g, &rng);
    else
        draw_strip(g, cell->index, layer_seed + (uint32_t)cell->index, (double)cell->w);
    swc_cv_restore(g);
    swc_cv_release_clips(g);
}

SwcCanvas *swc_atlas_draw_all(uint32_t layer_seed) {
    SwcCanvas *cv = swc_atlas_create();
    if (cv == NULL)
        return NULL;
    SwcAtlasCell cells[SWC_ATLAS_CELL_COUNT];
    int n = swc_atlas_layout(cells);
    for (int i = 0; i < n; i++)
        swc_atlas_draw_cell(cv, &cells[i], layer_seed);
    return cv;
}
