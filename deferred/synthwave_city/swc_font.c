/* Synthwave City - the stroke font. See swc_font.h. */
#include "swc_font.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define BEARING 0.065 /* side bearing of a letter, em */
#define SLANT 0.2

/* Each glyph is a path of strokes in a unit box: u across (0 left .. 1 right), v up (0 baseline .. 1 cap
 * line). The stroke is inset by half its width, so u = 0 and v = 1 are where the outer edges of the letter
 * lie. M u v        start a stroke L u v        line to A cu cv ru rv a0 a1   an elliptical arc in degrees (v
 * up), joined to the stroke so far when there is one C c1u c1v c2u c2v u v a cubic curve Z            close
 * the stroke D u v        a square dot as wide as the stem Diagonals run a little past the box; the letter is
 * cut flat at the cap line and the baseline. */
typedef struct Glyph {
    char c;
    double advance; /* em, for the proportional face */
    const char *path;
} Glyph;

/* A lower-case letter: a glyph that is shorter than the capitals and has its own side bearing. */
typedef struct LowerGlyph {
    Glyph glyph;
    double height;  /* em, from the baseline to the top of the box */
    double bearing; /* em, left of the box */
} LowerGlyph;

static const Glyph GLYPHS[] = {
    {' ', 0.28, ""},
    {'A', 0.72, "M-0.06 -0.12 L0.5 1 L1.06 -0.12 M0.2 0.32 L0.8 0.32"},
    {'B', 0.68,
     "M0 0 L0 1 L0.55 1 A0.55 0.765 0.43 0.235 90 -90 L0 0.53 M0.55 0.53 A0.55 0.265 0.45 0.265 90 -90 L0 0"},
    {'C', 0.70, "A0.5 0.5 0.5 0.5 42 318"},
    {'D', 0.72, "M0 0 L0 1 L0.45 1 A0.45 0.5 0.55 0.5 90 -90 Z"},
    {'E', 0.60, "M1 1 L0 1 L0 0 L1 0 M0 0.5 L0.85 0.5"},
    {'F', 0.58, "M1 1 L0 1 L0 0 M0 0.52 L0.85 0.52"},
    {'G', 0.74, "A0.5 0.5 0.5 0.5 40 360 L0.52 0.5"},
    {'H', 0.74, "M0 0 L0 1 M1 0 L1 1 M0 0.5 L1 0.5"},
    {'I', 0.30, "M0.5 0 L0.5 1"},
    {'J', 0.52, "M1 1 L1 0.32 A0.5 0.32 0.5 0.32 0 -180"},
    {'K', 0.68, "M0 0 L0 1 M1.06 1.1 L0.05 0.46 M0.34 0.68 L1.06 -0.1"},
    {'L', 0.56, "M0 1 L0 0 L1 0"},
    {'M', 0.90, "M0 0 L0 1 L0.5 0.28 L1 1 L1 0"},
    {'N', 0.76, "M0 0 L0 1 L1 0 L1 1"},
    {'O', 0.78, "A0.5 0.5 0.5 0.5 90 450 Z"},
    {'P', 0.65, "M0 0 L0 1 L0.55 1 A0.55 0.7 0.43 0.3 90 -90 L0 0.4"},
    {'Q', 0.78, "A0.5 0.5 0.5 0.5 90 450 Z M0.58 0.3 L1.04 -0.1"},
    {'R', 0.68, "M0 0 L0 1 L0.55 1 A0.55 0.7 0.43 0.3 90 -90 L0 0.4 M0.5 0.4 L1.04 -0.1"},
    {'S', 0.65, "A0.5 0.75 0.46 0.25 25 270 A0.5 0.25 0.5 0.25 90 -155"},
    {'T', 0.66, "M0 1 L1 1 M0.5 1 L0.5 0"},
    {'U', 0.73, "M0 1 L0 0.36 A0.5 0.36 0.5 0.36 180 360 L1 1"},
    {'V', 0.70, "M-0.06 1.12 L0.5 0 L1.06 1.12"},
    {'W', 1.00, "M-0.04 1.1 L0.25 0 L0.5 0.72 L0.75 0 L1.04 1.1"},
    {'X', 0.68, "M-0.06 1.1 L1.06 -0.1 M1.06 1.1 L-0.06 -0.1"},
    {'Y', 0.65, "M-0.06 1.1 L0.5 0.5 L1.06 1.1 M0.5 0.5 L0.5 0"},
    {'Z', 0.62, "M0 1 L1 1 L0 0 L1 0"},
    {'0', 0.64, "A0.5 0.5 0.5 0.5 90 450 Z"},
    {'1', 0.50, "M0.05 0.76 L0.62 1 L0.62 0"},
    {'2', 0.62, "A0.5 0.75 0.5 0.25 165 -40 L0 0 L1 0"},
    {'3', 0.62, "A0.46 0.76 0.46 0.24 118 -90 L0.4 0.52 M0.46 0.52 A0.5 0.26 0.5 0.26 90 -118"},
    {'4', 0.64, "M0.75 0 L0.75 1 L-0.02 0.3 L1.02 0.3"},
    {'5', 0.62, "M0.92 1 L0.1 1 L0.06 0.56 A0.5 0.31 0.5 0.31 130 -140"},
    {'6', 0.64, "A0.5 0.335 0.5 0.335 90 450 Z M0 0.335 C0 0.78 0.3 1 0.95 0.98"},
    {'7', 0.60, "M0 1 L1 1 L0.34 0"},
    {'8', 0.64, "A0.5 0.755 0.42 0.245 90 450 Z A0.5 0.255 0.5 0.255 90 450 Z"},
    {'9', 0.64, "A0.5 0.665 0.5 0.335 90 450 Z M1 0.665 C1 0.22 0.7 0 0.05 0.02"},
    {'%', 0.88, "A0.22 0.75 0.2 0.18 90 450 Z A0.78 0.25 0.2 0.18 90 450 Z M0.88 1.05 L0.12 -0.05"},
    {'/', 0.40, "M1 1.05 L0 -0.05"},
    {'.', 0.30, "D0.5 0"},
    {':', 0.30, "D0.5 0 D0.5 0.62"},
    {'-', 0.40, "M0 0.38 L1 0.38"},
};
#define GLYPH_COUNT ((int)(sizeof(GLYPHS) / sizeof(GLYPHS[0])))
/* The few lower-case letters the adverts use (the radio station VELVET), with a regular face's proportions:
 * an x-height of 0.53 em, ascenders 0.66 to 0.72. */
static const LowerGlyph LOWER[] = {
    {{'e', 0.556, "M0 0.5 L1 0.5 A0.5 0.5 0.5 0.5 0 322"}, 0.53, 0.04},
    {{'l', 0.262, "M0.5 0 L0.5 1"}, 0.72, 0.067},
    {{'t', 0.278, "M0.36 1 L0.36 0.3 A0.86 0.3 0.5 0.3 180 270 M0 0.8 L0.95 0.8"}, 0.66, 0.012},
    {{'v', 0.500, "M-0.06 1.12 L0.5 0 L1.06 1.12"}, 0.53, 0.008},
};
#define LOWER_COUNT ((int)(sizeof(LOWER) / sizeof(LOWER[0])))
/* A monospaced capital I has its serifs. */
static const char MONO_I[] = "M0.1 1 L0.9 1 M0.5 1 L0.5 0 M0.1 0 L0.9 0";

/* The glyph of a character; the box height and the left bearing (em) are 0 for the capital's own. */
static const Glyph *find_glyph(char c, double *height, double *bearing) {
    *height = 0;
    *bearing = 0;
    for (int i = 0; i < LOWER_COUNT; i++)
        if (LOWER[i].glyph.c == c) {
            *height = LOWER[i].height;
            *bearing = LOWER[i].bearing;
            return &LOWER[i].glyph;
        }
    int u = toupper((unsigned char)c);
    for (int i = 0; i < GLYPH_COUNT; i++)
        if (GLYPHS[i].c == (char)u)
            return &GLYPHS[i];
    return NULL;
}

double swc_font_advance(const SwcFontStyle *st, char c) {
    if (st->mono)
        return 0.6 * st->size;
    double height, bearing;
    const Glyph *g = find_glyph(c, &height, &bearing);
    return (g != NULL ? g->advance : 0.6) * st->size;
}

double swc_font_measure(const SwcFontStyle *st, const char *text) {
    double w = 0;
    for (const char *p = text; *p; p++)
        w += swc_font_advance(st, *p) + st->spacing;
    return w;
}

double swc_font_stem(const SwcFontStyle *st) {
    double e = st->weight >= 900 ? 0.195 : (st->weight >= 800 ? 0.165 : (st->weight > 500 ? 0.14 : 0.092));
    if (st->mono)
        e *= 0.88;
    return e * st->size;
}

typedef struct Frame {
    double bear, bw, cap, sw; /* pixels: bearing, body width, cap height, stem */
} Frame;

static double map_x(const Frame *f, double u) {
    double range = f->bw - f->sw;
    return f->bear + f->sw / 2.0 + u * (range > 0 ? range : 0.0);
}
static double map_y(const Frame *f, double v) {
    return -(f->sw / 2.0 + v * (f->cap - f->sw));
}

static void number(const char **p, double *out) {
    char *end;
    *out = strtod(*p, &end);
    *p = end;
}

/* Builds the glyph's path in the canvas (the pen at the origin, baseline y = 0). Returns true when it has
 * dots. */
static bool build_path(SwcCanvas *cv, const char *path, const Frame *f) {
    bool have = false, dots = false;
    double lu = 0, lv = 0; /* the stroke's end so far, in the unit box */
    const char *p = path;
    while (*p) {
        while (*p == ' ')
            p++;
        char op = *p++;
        if (op == 0)
            break;
        double a[6] = {0, 0, 0, 0, 0, 0};
        int n = op == 'M' || op == 'L' || op == 'D' ? 2 : (op == 'A' || op == 'C' ? 6 : 0);
        for (int i = 0; i < n; i++)
            number(&p, &a[i]);
        switch (op) {
        case 'M':
            swc_cv_move_to(cv, map_x(f, a[0]), map_y(f, a[1]));
            have = true;
            lu = a[0];
            lv = a[1];
            break;
        case 'L':
            if (have)
                swc_cv_line_to(cv, map_x(f, a[0]), map_y(f, a[1]));
            else
                swc_cv_move_to(cv, map_x(f, a[0]), map_y(f, a[1]));
            have = true;
            lu = a[0];
            lv = a[1];
            break;
        case 'A': {
            double sweep = a[5] - a[4];
            int steps = (int)ceil(fabs(sweep) / 7.5);
            steps = steps < 4 ? 4 : (steps > 64 ? 64 : steps);
            for (int i = 0; i <= steps; i++) {
                double th = (a[4] + sweep * (double)i / (double)steps) * PI / 180.0;
                double x = map_x(f, a[0] + a[2] * cos(th)), y = map_y(f, a[1] + a[3] * sin(th));
                if (i == 0 && !have)
                    swc_cv_move_to(cv, x, y);
                else
                    swc_cv_line_to(cv, x, y);
            }
            have = true;
            lu = a[0] + a[2] * cos(a[5] * PI / 180.0);
            lv = a[1] + a[3] * sin(a[5] * PI / 180.0);
            break;
        }
        case 'C': {
            for (int i = 1; i <= 24; i++) {
                double t = (double)i / 24.0, s = 1.0 - t;
                double b0 = s * s * s, b1 = 3 * s * s * t, b2 = 3 * s * t * t, b3 = t * t * t;
                swc_cv_line_to(cv, map_x(f, b0 * lu + b1 * a[0] + b2 * a[2] + b3 * a[4]),
                               map_y(f, b0 * lv + b1 * a[1] + b2 * a[3] + b3 * a[5]));
            }
            lu = a[4];
            lv = a[5];
            break;
        }
        case 'Z':
            swc_cv_close_path(cv);
            have = false;
            break;
        case 'D': {
            double x = map_x(f, a[0]), y = map_y(f, a[1]);
            swc_cv_move_to(cv, x, y);
            swc_cv_line_to(cv, x, y);
            have = false;
            dots = true;
            break;
        }
        default:
            break;
        }
    }
    return dots;
}

void swc_font_draw(SwcCanvas *cv, const SwcFontStyle *st, const char *text, double x, double y, double widen,
                   bool ring) {
    double sw = swc_font_stem(st), cap = SWC_FONT_CAP_HEIGHT * st->size;
    swc_cv_save(cv);
    swc_cv_translate(cv, x, y);
    if (st->italic)
        swc_cv_skew(cv, SLANT);
    swc_cv_stroke_with_fill_style(cv, true);
    swc_cv_line_width(cv, sw + widen);
    swc_cv_miter_limit(cv, 4.0);
    swc_cv_line_join(cv, widen > 0 ? SWC_JOIN_ROUND : SWC_JOIN_MITER);
    swc_cv_line_cap(cv,
                    SWC_CAP_SQUARE); /* a free end then reaches the cap line, baseline or side of the box */
    if (ring && widen > 0 && sw > widen)
        swc_cv_stroke_hole(cv, sw - widen);
    double pen = 0;
    for (const char *c = text; *c; c++) {
        double height, bearing;
        const Glyph *g = find_glyph(*c, &height, &bearing);
        double adv = swc_font_advance(st, *c);
        if (g != NULL && g->path[0] != 0) {
            Frame f;
            f.bear = st->mono ? 0.05 * st->size : (bearing > 0 ? bearing : BEARING) * st->size;
            f.bw = adv - 2.0 * f.bear;
            f.cap = height > 0 ? height * st->size : cap;
            f.sw = sw;
            const char *path = (st->mono && toupper((unsigned char)*c) == 'I') ? MONO_I : g->path;
            swc_cv_save(cv);
            swc_cv_translate(cv, pen, 0);
            double hx = widen / 2.0, slack = 0.5;
            double bw = f.bw > sw ? f.bw : sw;
            swc_cv_stroke_box(cv, true, f.bear - slack - hx, -f.cap - hx, f.bear + bw + slack + hx, hx);
            swc_cv_begin_path(cv);
            bool dots = build_path(cv, path, &f);
            if (dots) /* a dot is a zero-length stroke: a round cap makes it round, as a bold face's is */
                swc_cv_line_cap(cv, SWC_CAP_ROUND);
            swc_cv_stroke(cv);
            swc_cv_restore(cv);
        }
        pen += adv + st->spacing;
    }
    swc_cv_restore(cv);
}
