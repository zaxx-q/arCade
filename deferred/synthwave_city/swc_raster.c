/* Synthwave City - the software 2D rasteriser. See swc_raster.h. */
#include "swc_raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define SUBS 16 /* sample rows per pixel row */
#define MAX_STATES 16
#define FLATNESS 0.02 /* how far, in pixels, a flattened curve may stray from the true one */

typedef struct Pt {
    double x, y;
} Pt;

/* x' = a x + c y + e,  y' = b x + d y + f */
typedef struct Mat {
    double a, b, c, d, e, f;
} Mat;

static Mat mat_mul(Mat m, Mat n) { /* m after n: apply n first */
    Mat r;
    r.a = m.a * n.a + m.c * n.b;
    r.b = m.b * n.a + m.d * n.b;
    r.c = m.a * n.c + m.c * n.d;
    r.d = m.b * n.c + m.d * n.d;
    r.e = m.a * n.e + m.c * n.f + m.e;
    r.f = m.b * n.e + m.d * n.f + m.f;
    return r;
}

static Pt mat_apply(const Mat *m, double x, double y) {
    Pt p = {m->a * x + m->c * y + m->e, m->b * x + m->d * y + m->f};
    return p;
}

static bool mat_invert(const Mat *m, Mat *out) {
    double det = m->a * m->d - m->b * m->c;
    if (fabs(det) < 1e-12)
        return false;
    double id = 1.0 / det;
    out->a = m->d * id;
    out->b = -m->b * id;
    out->c = -m->c * id;
    out->d = m->a * id;
    out->e = (m->c * m->f - m->d * m->e) * id;
    out->f = (m->b * m->e - m->a * m->f) * id;
    return true;
}

typedef struct Paint {
    bool gradient;
    uint32_t rgb;
    SwcGradient g;
} Paint;

typedef struct State {
    Mat ctm;
    Paint fill, stroke;
    double line_width, miter_limit;
    SwcLineCap cap;
    SwcLineJoin join;
    bool stroke_uses_fill, box_on;
    double box_x0, box_y0, box_x1, box_y1, hole;
    int cx0, cy0, cx1, cy1; /* clip box, exclusive max */
    const uint8_t *mask;    /* coverage 0..255 over the clip box, or NULL for the whole box */
} State;

typedef struct Sub {
    int start, count, segments;
    bool closed;
} Sub;

typedef struct Edge {
    double y0, y1, x0, slope;
    int dir;
} Edge;

typedef struct EdgeList {
    Edge *e;
    int n, cap;
    double minx, miny, maxx, maxy;
} EdgeList;

struct SwcCanvas {
    int w, h;
    uint8_t *px;
    State st[MAX_STATES];
    int depth;
    /* the current path, in user space */
    Pt *pts;
    int npts, cap_pts;
    Sub *subs;
    int nsub, cap_subs;
    bool has_pending;
    Pt pending;
    /* scratch */
    EdgeList edges, holes;
    float *cov, *cov2;
    size_t cov_cap;
    float *acc, *delta;
    int row_cap;
    int *active;
    int active_cap;
    double *xs;
    int *dirs;
    uint8_t **masks;
    int nmasks, cap_masks;
    bool failed;
};

/* ---------------------------------------------------------------------------------------------------- */
/* Memory helpers                                                                                       */
/* ---------------------------------------------------------------------------------------------------- */

static void *grow(void *p, int *cap, int need, size_t elem) {
    if (need <= *cap)
        return p;
    int nc = *cap == 0 ? 64 : *cap;
    while (nc < need)
        nc *= 2;
    void *q = realloc(p, (size_t)nc * elem);
    if (q == NULL)
        return NULL;
    *cap = nc;
    return q;
}

static void el_reset(EdgeList *l) {
    l->n = 0;
    l->minx = l->miny = 1e30;
    l->maxx = l->maxy = -1e30;
}

static void el_edge(SwcCanvas *cv, EdgeList *l, double x0, double y0, double x1, double y1) {
    if (y0 == y1)
        return;
    int dir = 1;
    if (y0 > y1) {
        double t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
        dir = -1;
    }
    Edge *e = (Edge *)grow(l->e, &l->cap, l->n + 1, sizeof(Edge));
    if (e == NULL) {
        cv->failed = true;
        return;
    }
    l->e = e;
    Edge *d = &l->e[l->n++];
    d->y0 = y0;
    d->y1 = y1;
    d->x0 = x0;
    d->slope = (x1 - x0) / (y1 - y0);
    d->dir = dir;
}

/* Adds a closed polygon. With `positive`, it is wound the same way whatever its input order, so that a union
 * of such polygons fills as a union under the non-zero rule. */
static void el_poly(SwcCanvas *cv, EdgeList *l, const Pt *p, int n, bool positive) {
    if (n < 3)
        return;
    bool flip = false;
    if (positive) {
        double area = 0;
        for (int i = 0; i < n; i++) {
            const Pt *a = &p[i], *b = &p[(i + 1) % n];
            area += a->x * b->y - b->x * a->y;
        }
        flip = area < 0;
    }
    for (int i = 0; i < n; i++) {
        const Pt *a = &p[flip ? n - 1 - i : i];
        const Pt *b = &p[flip ? (n - 1 - ((i + 1) % n)) : ((i + 1) % n)];
        el_edge(cv, l, a->x, a->y, b->x, b->y);
        if (a->x < l->minx)
            l->minx = a->x;
        if (a->x > l->maxx)
            l->maxx = a->x;
        if (a->y < l->miny)
            l->miny = a->y;
        if (a->y > l->maxy)
            l->maxy = a->y;
    }
}

static int cmp_edge(const void *a, const void *b) {
    double d = ((const Edge *)a)->y0 - ((const Edge *)b)->y0;
    return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Coverage                                                                                             */
/* ---------------------------------------------------------------------------------------------------- */

/* Coverage of the edges over the box [bx0,bx1) x [by0,by1) into out (row-major, width bx1 - bx0). */
static void coverage(SwcCanvas *cv, EdgeList *l, SwcFillRule rule, int bx0, int by0, int bx1, int by1,
                     float *out) {
    int w = bx1 - bx0, h = by1 - by0;
    memset(out, 0, (size_t)w * (size_t)h * sizeof(float));
    if (l->n == 0 || w <= 0 || h <= 0)
        return;
    qsort(l->e, (size_t)l->n, sizeof(Edge), cmp_edge);
    if (cv->active_cap < l->n) {
        int nc = l->n > 256 ? l->n : 256;
        int *na = (int *)realloc(cv->active, (size_t)nc * sizeof(int));
        double *nx = (double *)realloc(cv->xs, (size_t)nc * sizeof(double));
        int *nd = (int *)realloc(cv->dirs, (size_t)nc * sizeof(int));
        if (na != NULL)
            cv->active = na;
        if (nx != NULL)
            cv->xs = nx;
        if (nd != NULL)
            cv->dirs = nd;
        if (na == NULL || nx == NULL || nd == NULL) {
            cv->failed = true;
            return;
        }
        cv->active_cap = nc;
    }
    if (cv->row_cap < w + 2) {
        int nc = w + 2 > 512 ? w + 2 : 512;
        float *na = (float *)realloc(cv->acc, (size_t)nc * sizeof(float));
        float *nd = (float *)realloc(cv->delta, (size_t)nc * sizeof(float));
        if (na != NULL)
            cv->acc = na;
        if (nd != NULL)
            cv->delta = nd;
        if (na == NULL || nd == NULL) {
            cv->failed = true;
            return;
        }
        cv->row_cap = nc;
    }
    int *act = cv->active;
    float *acc = cv->acc, *delta = cv->delta;
    int nact = 0, next = 0;
    const float unit = 1.0f / (float)SUBS;
    for (int row = 0; row < h; row++) {
        double y = (double)(by0 + row);
        bool touched = false;
        memset(acc, 0, (size_t)(w + 2) * sizeof(float));
        memset(delta, 0, (size_t)(w + 2) * sizeof(float));
        for (int s = 0; s < SUBS; s++) {
            double sy = y + ((double)s + 0.5) / (double)SUBS;
            while (next < l->n && l->e[next].y0 <= sy)
                act[nact++] = next++;
            int k = 0, nx = 0;
            for (int i = 0; i < nact; i++) {
                const Edge *e = &l->e[act[i]];
                if (e->y1 <= sy)
                    continue;
                act[k++] = act[i];
                /* insertion sort by x */
                double x = e->x0 + (sy - e->y0) * e->slope;
                int j = nx++;
                while (j > 0 && cv->xs[j - 1] > x) {
                    cv->xs[j] = cv->xs[j - 1];
                    cv->dirs[j] = cv->dirs[j - 1];
                    j--;
                }
                cv->xs[j] = x;
                cv->dirs[j] = e->dir;
            }
            nact = k;
            int wind = 0;
            double xa = 0;
            for (int i = 0; i < nx; i++) {
                int prev = wind;
                if (rule == SWC_NONZERO)
                    wind += cv->dirs[i];
                else
                    wind ^= 1;
                double x0 = 0, x1 = 0;
                bool span = false;
                if (prev == 0 && wind != 0)
                    xa = cv->xs[i];
                else if (prev != 0 && wind == 0) {
                    x0 = xa;
                    x1 = cv->xs[i];
                    span = true;
                }
                if (!span)
                    continue;
                x0 -= (double)bx0;
                x1 -= (double)bx0;
                if (x0 < 0)
                    x0 = 0;
                if (x1 > (double)w)
                    x1 = (double)w;
                if (x1 <= x0)
                    continue;
                touched = true;
                int ia = (int)x0, ib = (int)x1;
                if (ia == ib) {
                    acc[ia] += (float)(x1 - x0) * unit;
                } else {
                    acc[ia] += (float)((double)(ia + 1) - x0) * unit;
                    delta[ia + 1] += unit;
                    delta[ib] -= unit;
                    acc[ib] += (float)(x1 - (double)ib) * unit;
                }
            }
        }
        if (!touched)
            continue;
        float run = 0;
        float *o = out + (size_t)row * (size_t)w;
        for (int x = 0; x < w; x++) {
            run += delta[x];
            float c = run + acc[x];
            o[x] = c > 1.0f ? 1.0f : (c < 0.0f ? 0.0f : c);
        }
    }
}

/* ---------------------------------------------------------------------------------------------------- */
/* Painting                                                                                             */
/* ---------------------------------------------------------------------------------------------------- */

static void paint_at(const Paint *p, const Mat *inv, double px, double py, float rgb[3]) {
    if (!p->gradient) {
        rgb[0] = (float)((p->rgb >> 16) & 255u);
        rgb[1] = (float)((p->rgb >> 8) & 255u);
        rgb[2] = (float)(p->rgb & 255u);
        return;
    }
    const SwcGradient *g = &p->g;
    Pt u = mat_apply(inv, px, py);
    double dx = g->x1 - g->x0, dy = g->y1 - g->y0, len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((u.x - g->x0) * dx + (u.y - g->y0) * dy) / len2 : 0.0;
    int n = g->stop_count;
    if (n <= 0) {
        rgb[0] = rgb[1] = rgb[2] = 0;
        return;
    }
    uint32_t c0, c1;
    double f = 0;
    if (t <= g->stop_at[0]) {
        c0 = c1 = g->stop_rgb[0];
    } else if (t >= g->stop_at[n - 1]) {
        c0 = c1 = g->stop_rgb[n - 1];
    } else {
        int i = 1;
        while (i < n - 1 && t > g->stop_at[i])
            i++;
        c0 = g->stop_rgb[i - 1];
        c1 = g->stop_rgb[i];
        double span = g->stop_at[i] - g->stop_at[i - 1];
        f = span > 0 ? (t - g->stop_at[i - 1]) / span : 1.0;
    }
    for (int k = 0; k < 3; k++) {
        int sh = 16 - 8 * k;
        double a = (double)((c0 >> sh) & 255u), b = (double)((c1 >> sh) & 255u);
        rgb[k] = (float)(a + (b - a) * f);
    }
}

/* Composites coverage (box bx0.., size w x h) with the paint, source-over, through the clip. */
static void blend(SwcCanvas *cv, const State *st, const Paint *paint, const float *cov, int bx0, int by0,
                  int w, int h) {
    Mat inv = {1, 0, 0, 1, 0, 0};
    if (paint->gradient && !mat_invert(&st->ctm, &inv))
        return;
    int cw = st->cx1 - st->cx0;
    for (int row = 0; row < h; row++) {
        int y = by0 + row;
        const float *c = cov + (size_t)row * (size_t)w;
        uint8_t *dst = cv->px + ((size_t)y * (size_t)cv->w + (size_t)bx0) * 4u;
        for (int x = 0; x < w; x++, dst += 4) {
            float a = c[x];
            if (a <= 0.001f)
                continue;
            if (st->mask != NULL)
                a *= (float)st->mask[(size_t)(y - st->cy0) * (size_t)cw + (size_t)(bx0 + x - st->cx0)] *
                     (1.0f / 255.0f);
            if (a <= 0.001f)
                continue;
            float rgb[3];
            paint_at(paint, &inv, (double)(bx0 + x) + 0.5, (double)y + 0.5, rgb);
            for (int k = 0; k < 3; k++) {
                float v = (float)dst[k] + (rgb[k] - (float)dst[k]) * a;
                dst[k] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v + 0.5f));
            }
        }
    }
}

static bool box_of(const SwcCanvas *cv, const State *st, const EdgeList *l, int *x0, int *y0, int *x1,
                   int *y1) {
    (void)cv;
    if (l->n == 0)
        return false;
    *x0 = (int)floor(l->minx);
    *y0 = (int)floor(l->miny);
    *x1 = (int)ceil(l->maxx);
    *y1 = (int)ceil(l->maxy);
    if (*x0 < st->cx0)
        *x0 = st->cx0;
    if (*y0 < st->cy0)
        *y0 = st->cy0;
    if (*x1 > st->cx1)
        *x1 = st->cx1;
    if (*y1 > st->cy1)
        *y1 = st->cy1;
    return *x1 > *x0 && *y1 > *y0;
}

static bool ensure_cov(SwcCanvas *cv, size_t n) {
    if (cv->cov_cap >= n && cv->cov != NULL && cv->cov2 != NULL)
        return true;
    size_t cap = n > 4096 ? n : 4096;
    float *a = (float *)realloc(cv->cov, cap * sizeof(float));
    if (a != NULL)
        cv->cov = a;
    float *b = (float *)realloc(cv->cov2, cap * sizeof(float));
    if (b != NULL)
        cv->cov2 = b;
    if (a == NULL || b == NULL) {
        cv->failed = true;
        return false;
    }
    cv->cov_cap = cap;
    return true;
}

/* Fills the edges with the paint; with `holes` not NULL, only what the first set covers and the second does
 * not. */
static void paint_edges(SwcCanvas *cv, EdgeList *l, EdgeList *holes, SwcFillRule rule, const Paint *paint) {
    const State *st = &cv->st[cv->depth];
    int x0, y0, x1, y1;
    if (!box_of(cv, st, l, &x0, &y0, &x1, &y1))
        return;
    size_t n = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    if (!ensure_cov(cv, n))
        return;
    coverage(cv, l, rule, x0, y0, x1, y1, cv->cov);
    if (holes != NULL && holes->n > 0) {
        coverage(cv, holes, SWC_NONZERO, x0, y0, x1, y1, cv->cov2);
        for (size_t i = 0; i < n; i++) {
            float c = cv->cov[i] - cv->cov2[i];
            cv->cov[i] = c < 0 ? 0 : c;
        }
    }
    if (!cv->failed)
        blend(cv, st, paint, cv->cov, x0, y0, x1 - x0, y1 - y0);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Canvas lifetime and state                                                                            */
/* ---------------------------------------------------------------------------------------------------- */

static void state_init(State *s, int w, int h) {
    memset(s, 0, sizeof(*s));
    s->ctm.a = s->ctm.d = 1.0;
    s->fill.rgb = 0x000000;
    s->stroke.rgb = 0x000000;
    s->line_width = 1.0;
    s->miter_limit = 10.0;
    s->cx1 = w;
    s->cy1 = h;
}

SwcCanvas *swc_cv_create(int width, int height) {
    if (width <= 0 || height <= 0)
        return NULL;
    SwcCanvas *cv = (SwcCanvas *)calloc(1, sizeof(*cv));
    if (cv == NULL)
        return NULL;
    cv->w = width;
    cv->h = height;
    cv->px = (uint8_t *)malloc((size_t)width * (size_t)height * 4u);
    if (cv->px == NULL) {
        free(cv);
        return NULL;
    }
    for (size_t i = 0; i < (size_t)width * (size_t)height; i++) {
        cv->px[i * 4] = cv->px[i * 4 + 1] = cv->px[i * 4 + 2] = 0;
        cv->px[i * 4 + 3] = 255;
    }
    state_init(&cv->st[0], width, height);
    el_reset(&cv->edges);
    el_reset(&cv->holes);
    return cv;
}

void swc_cv_release_clips(SwcCanvas *cv) {
    for (int i = 0; i < cv->nmasks; i++)
        free(cv->masks[i]);
    cv->nmasks = 0;
}

void swc_cv_destroy(SwcCanvas *cv) {
    if (cv == NULL)
        return;
    swc_cv_release_clips(cv);
    free(cv->masks);
    free(cv->px);
    free(cv->pts);
    free(cv->subs);
    free(cv->edges.e);
    free(cv->holes.e);
    free(cv->cov);
    free(cv->cov2);
    free(cv->acc);
    free(cv->delta);
    free(cv->active);
    free(cv->xs);
    free(cv->dirs);
    free(cv);
}

int swc_cv_width(const SwcCanvas *cv) {
    return cv->w;
}
int swc_cv_height(const SwcCanvas *cv) {
    return cv->h;
}
const uint8_t *swc_cv_pixels(const SwcCanvas *cv) {
    return cv->px;
}

static State *cur(SwcCanvas *cv) {
    return &cv->st[cv->depth];
}

void swc_cv_save(SwcCanvas *cv) {
    if (cv->depth + 1 >= MAX_STATES)
        return;
    cv->st[cv->depth + 1] = cv->st[cv->depth];
    cv->depth++;
}

void swc_cv_restore(SwcCanvas *cv) {
    if (cv->depth > 0)
        cv->depth--;
}

void swc_cv_translate(SwcCanvas *cv, double x, double y) {
    Mat t = {1, 0, 0, 1, x, y};
    cur(cv)->ctm = mat_mul(cur(cv)->ctm, t);
}

void swc_cv_scale(SwcCanvas *cv, double sx, double sy) {
    Mat t = {sx, 0, 0, sy, 0, 0};
    cur(cv)->ctm = mat_mul(cur(cv)->ctm, t);
}

void swc_cv_rotate(SwcCanvas *cv, double r) {
    Mat t = {cos(r), sin(r), -sin(r), cos(r), 0, 0};
    cur(cv)->ctm = mat_mul(cur(cv)->ctm, t);
}

void swc_cv_skew(SwcCanvas *cv, double k) {
    Mat t = {1, 0, -k, 1, 0, 0};
    cur(cv)->ctm = mat_mul(cur(cv)->ctm, t);
}

void swc_cv_fill_rgb(SwcCanvas *cv, uint32_t rgb) {
    cur(cv)->fill.gradient = false;
    cur(cv)->fill.rgb = rgb;
}
void swc_cv_stroke_rgb(SwcCanvas *cv, uint32_t rgb) {
    cur(cv)->stroke.gradient = false;
    cur(cv)->stroke.rgb = rgb;
}
void swc_cv_fill_gradient(SwcCanvas *cv, const SwcGradient *g) {
    cur(cv)->fill.gradient = true;
    cur(cv)->fill.g = *g;
}
void swc_cv_stroke_gradient(SwcCanvas *cv, const SwcGradient *g) {
    cur(cv)->stroke.gradient = true;
    cur(cv)->stroke.g = *g;
}
void swc_cv_line_width(SwcCanvas *cv, double w) {
    if (w > 0)
        cur(cv)->line_width = w;
}
void swc_cv_line_cap(SwcCanvas *cv, SwcLineCap cap) {
    cur(cv)->cap = cap;
}
void swc_cv_line_join(SwcCanvas *cv, SwcLineJoin join) {
    cur(cv)->join = join;
}
void swc_cv_miter_limit(SwcCanvas *cv, double limit) {
    if (limit > 0)
        cur(cv)->miter_limit = limit;
}
void swc_cv_stroke_with_fill_style(SwcCanvas *cv, bool on) {
    cur(cv)->stroke_uses_fill = on;
}
void swc_cv_stroke_box(SwcCanvas *cv, bool on, double x0, double y0, double x1, double y1) {
    State *s = cur(cv);
    s->box_on = on;
    s->box_x0 = x0;
    s->box_y0 = y0;
    s->box_x1 = x1;
    s->box_y1 = y1;
}
void swc_cv_stroke_hole(SwcCanvas *cv, double inner) {
    cur(cv)->hole = inner;
}

SwcGradient swc_gradient2(double x0, double y0, double x1, double y1, uint32_t from, uint32_t to) {
    SwcGradient g;
    memset(&g, 0, sizeof(g));
    g.x0 = x0;
    g.y0 = y0;
    g.x1 = x1;
    g.y1 = y1;
    g.stop_count = 2;
    g.stop_at[0] = 0.0;
    g.stop_at[1] = 1.0;
    g.stop_rgb[0] = from;
    g.stop_rgb[1] = to;
    return g;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Paths                                                                                                */
/* ---------------------------------------------------------------------------------------------------- */

void swc_cv_begin_path(SwcCanvas *cv) {
    cv->npts = 0;
    cv->nsub = 0;
    cv->has_pending = false;
}

static Sub *open_sub(SwcCanvas *cv) {
    if (cv->nsub == 0)
        return NULL;
    Sub *s = &cv->subs[cv->nsub - 1];
    return s->closed ? NULL : s;
}

static void push_point(SwcCanvas *cv, double x, double y) {
    Pt *p = (Pt *)grow(cv->pts, &cv->cap_pts, cv->npts + 1, sizeof(Pt));
    if (p == NULL) {
        cv->failed = true;
        return;
    }
    cv->pts = p;
    cv->pts[cv->npts].x = x;
    cv->pts[cv->npts].y = y;
    cv->npts++;
    cv->subs[cv->nsub - 1].count++;
}

static void start_sub(SwcCanvas *cv, double x, double y) {
    Sub *s = (Sub *)grow(cv->subs, &cv->cap_subs, cv->nsub + 1, sizeof(Sub));
    if (s == NULL) {
        cv->failed = true;
        return;
    }
    cv->subs = s;
    Sub *n = &cv->subs[cv->nsub++];
    n->start = cv->npts;
    n->count = 0;
    n->segments = 0;
    n->closed = false;
    cv->has_pending = false;
    push_point(cv, x, y);
}

void swc_cv_move_to(SwcCanvas *cv, double x, double y) {
    Sub *s = open_sub(cv);
    if (s != NULL && s->count == 1 && s->segments == 0) { /* a lone moveTo is replaced */
        cv->pts[s->start].x = x;
        cv->pts[s->start].y = y;
        return;
    }
    start_sub(cv, x, y);
}

void swc_cv_line_to(SwcCanvas *cv, double x, double y) {
    Sub *s = open_sub(cv);
    if (s == NULL) {
        if (cv->has_pending)
            start_sub(cv, cv->pending.x, cv->pending.y);
        else {
            start_sub(cv, x, y);
            return;
        }
        s = open_sub(cv);
    }
    push_point(cv, x, y);
    s->segments++;
}

void swc_cv_close_path(SwcCanvas *cv) {
    Sub *s = open_sub(cv);
    if (s == NULL)
        return;
    s->closed = true;
    cv->pending = cv->pts[s->start];
    cv->has_pending = true;
}

void swc_cv_rect(SwcCanvas *cv, double x, double y, double w, double h) {
    swc_cv_move_to(cv, x, y);
    swc_cv_line_to(cv, x + w, y);
    swc_cv_line_to(cv, x + w, y + h);
    swc_cv_line_to(cv, x, y + h);
    swc_cv_close_path(cv);
}

/* The largest scale the transform applies, for choosing how finely to cut curves. */
static double ctm_scale(const SwcCanvas *cv) {
    const Mat *m = &cv->st[cv->depth].ctm;
    double a = hypot(m->a, m->b), b = hypot(m->c, m->d);
    double s = a > b ? a : b;
    return s > 1e-6 ? s : 1.0;
}

void swc_cv_ellipse(SwcCanvas *cv, double x, double y, double rx, double ry, double rotation, double a0,
                    double a1, bool ccw) {
    if (rx < 0 || ry < 0)
        return;
    /* the same rules a browser uses to bring the end angle to the right turn */
    double sweep;
    if (!ccw) {
        if (a1 - a0 >= 2 * PI)
            sweep = 2 * PI;
        else if (a0 > a1)
            sweep = 2 * PI - fmod(a0 - a1, 2 * PI);
        else
            sweep = a1 - a0;
    } else {
        if (a0 - a1 >= 2 * PI)
            sweep = -2 * PI;
        else if (a0 < a1)
            sweep = -(2 * PI - fmod(a1 - a0, 2 * PI));
        else
            sweep = a1 - a0;
    }
    double r = rx > ry ? rx : ry, scale = ctm_scale(cv);
    double step = 0.5;
    if (r * scale > FLATNESS * 2)
        step = 2.0 * acos(1.0 - FLATNESS / (r * scale));
    int n = (int)ceil(fabs(sweep) / step);
    if (n < 4 && fabs(sweep) > 0.5)
        n = 4;
    if (n < 1)
        n = 1;
    if (n > 720)
        n = 720;
    double cr = cos(rotation), sr = sin(rotation);
    for (int i = 0; i <= n; i++) {
        double a = a0 + sweep * (double)i / (double)n;
        double px = rx * cos(a), py = ry * sin(a);
        double wx = x + px * cr - py * sr, wy = y + px * sr + py * cr;
        if (i == 0) {
            if (open_sub(cv) == NULL && !cv->has_pending)
                swc_cv_move_to(cv, wx, wy);
            else
                swc_cv_line_to(cv, wx, wy);
        } else {
            swc_cv_line_to(cv, wx, wy);
        }
    }
}

void swc_cv_arc(SwcCanvas *cv, double x, double y, double r, double a0, double a1, bool ccw) {
    swc_cv_ellipse(cv, x, y, r, r, 0, a0, a1, ccw);
}

void swc_cv_arc_to(SwcCanvas *cv, double x1, double y1, double x2, double y2, double r) {
    Sub *s = open_sub(cv);
    if (s == NULL) {
        swc_cv_move_to(cv, x1, y1);
        return;
    }
    Pt p0 = cv->pts[s->start + s->count - 1];
    double ax = p0.x - x1, ay = p0.y - y1, bx = x2 - x1, by = y2 - y1;
    double la = hypot(ax, ay), lb = hypot(bx, by);
    if (la < 1e-12 || lb < 1e-12 || r <= 0) {
        swc_cv_line_to(cv, x1, y1);
        return;
    }
    ax /= la;
    ay /= la;
    bx /= lb;
    by /= lb;
    double cross = ax * by - ay * bx, dot = ax * bx + ay * by;
    if (fabs(cross) < 1e-9) { /* collinear */
        swc_cv_line_to(cv, x1, y1);
        return;
    }
    double half = acos(dot < -1 ? -1 : (dot > 1 ? 1 : dot)) / 2.0;
    double tan_d = r / tan(half);
    double t1x = x1 + ax * tan_d, t1y = y1 + ay * tan_d;
    double t2x = x1 + bx * tan_d, t2y = y1 + by * tan_d;
    double mx = ax + bx, my = ay + by, ml = hypot(mx, my);
    double cd = r / sin(half);
    double cx = x1 + mx / ml * cd, cy = y1 + my / ml * cd;
    double a_start = atan2(t1y - cy, t1x - cx), a_end = atan2(t2y - cy, t2x - cx);
    swc_cv_line_to(cv, t1x, t1y);
    swc_cv_arc(cv, cx, cy, r, a_start, a_end, cross > 0);
}

static int curve_steps(const SwcCanvas *cv, double length) {
    int n = (int)ceil(length * ctm_scale(cv) / 5.0);
    return n < 8 ? 8 : (n > 64 ? 64 : n);
}

void swc_cv_quad_to(SwcCanvas *cv, double cx, double cy, double x, double y) {
    Sub *s = open_sub(cv);
    if (s == NULL) {
        swc_cv_move_to(cv, cx, cy);
        s = open_sub(cv);
        if (s == NULL)
            return;
    }
    Pt p0 = cv->pts[s->start + s->count - 1];
    int n = curve_steps(cv, hypot(cx - p0.x, cy - p0.y) + hypot(x - cx, y - cy));
    for (int i = 1; i <= n; i++) {
        double t = (double)i / (double)n, u = 1 - t;
        swc_cv_line_to(cv, u * u * p0.x + 2 * u * t * cx + t * t * x,
                       u * u * p0.y + 2 * u * t * cy + t * t * y);
    }
}

void swc_cv_bezier_to(SwcCanvas *cv, double c1x, double c1y, double c2x, double c2y, double x, double y) {
    Sub *s = open_sub(cv);
    if (s == NULL) {
        swc_cv_move_to(cv, c1x, c1y);
        s = open_sub(cv);
        if (s == NULL)
            return;
    }
    Pt p0 = cv->pts[s->start + s->count - 1];
    int n = curve_steps(cv, hypot(c1x - p0.x, c1y - p0.y) + hypot(c2x - c1x, c2y - c1y) +
                                hypot(x - c2x, y - c2y));
    for (int i = 1; i <= n; i++) {
        double t = (double)i / (double)n, u = 1 - t;
        double b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
        swc_cv_line_to(cv, b0 * p0.x + b1 * c1x + b2 * c2x + b3 * x,
                       b0 * p0.y + b1 * c1y + b2 * c2y + b3 * y);
    }
}

/* ---------------------------------------------------------------------------------------------------- */
/* Fill, clip                                                                                           */
/* ---------------------------------------------------------------------------------------------------- */

static void path_edges(SwcCanvas *cv, EdgeList *l) {
    const Mat *m = &cur(cv)->ctm;
    el_reset(l);
    for (int i = 0; i < cv->nsub; i++) {
        const Sub *s = &cv->subs[i];
        if (s->count < 3)
            continue;
        /* transform in place into a temporary run, then add */
        Pt *tmp = (Pt *)malloc((size_t)s->count * sizeof(Pt));
        if (tmp == NULL) {
            cv->failed = true;
            return;
        }
        for (int k = 0; k < s->count; k++)
            tmp[k] = mat_apply(m, cv->pts[s->start + k].x, cv->pts[s->start + k].y);
        el_poly(cv, l, tmp, s->count, false);
        free(tmp);
    }
}

void swc_cv_fill(SwcCanvas *cv, SwcFillRule rule) {
    path_edges(cv, &cv->edges);
    paint_edges(cv, &cv->edges, NULL, rule, &cur(cv)->fill);
}

void swc_cv_fill_rect(SwcCanvas *cv, double x, double y, double w, double h) {
    /* a rectangle path of its own: the current path is left alone, as in a canvas */
    SwcCanvas *c = cv;
    Pt p[4];
    const Mat *m = &cur(c)->ctm;
    p[0] = mat_apply(m, x, y);
    p[1] = mat_apply(m, x + w, y);
    p[2] = mat_apply(m, x + w, y + h);
    p[3] = mat_apply(m, x, y + h);
    el_reset(&c->edges);
    el_poly(c, &c->edges, p, 4, false);
    paint_edges(c, &c->edges, NULL, SWC_NONZERO, &cur(c)->fill);
}

void swc_cv_clip(SwcCanvas *cv, SwcFillRule rule) {
    State *st = cur(cv);
    path_edges(cv, &cv->edges);
    EdgeList *l = &cv->edges;
    if (l->n == 0) { /* an empty path clips everything away */
        st->cx1 = st->cx0;
        st->cy1 = st->cy0;
        return;
    }
    /* A single axis-aligned rectangle on whole pixels only narrows the box. */
    bool whole = false;
    if (cv->nsub == 1 && cv->subs[0].count == 4) {
        const Pt *q = &cv->pts[cv->subs[0].start];
        Pt d[4];
        for (int i = 0; i < 4; i++)
            d[i] = mat_apply(&st->ctm, q[i].x, q[i].y);
        whole = true;
        for (int i = 0; i < 4; i++) {
            const Pt *a = &d[i], *b = &d[(i + 1) % 4];
            bool aligned = fabs(a->x - b->x) < 1e-9 || fabs(a->y - b->y) < 1e-9;
            bool integral = fabs(a->x - floor(a->x + 0.5)) < 1e-6 && fabs(a->y - floor(a->y + 0.5)) < 1e-6;
            if (!aligned || !integral)
                whole = false;
        }
    }
    int x0, y0, x1, y1;
    if (!box_of(cv, st, l, &x0, &y0, &x1, &y1)) {
        st->cx1 = st->cx0;
        st->cy1 = st->cy0;
        return;
    }
    if (whole) {
        /* the old mask (if any) is indexed from the old box, so keep the mask and only narrow the box when
         * the box is unchanged in origin; otherwise rebuild it below */
        if (st->mask == NULL) {
            st->cx0 = x0;
            st->cy0 = y0;
            st->cx1 = x1;
            st->cy1 = y1;
            return;
        }
    }
    size_t n = (size_t)(x1 - x0) * (size_t)(y1 - y0);
    if (!ensure_cov(cv, n))
        return;
    coverage(cv, l, rule, x0, y0, x1, y1, cv->cov);
    uint8_t *mask = (uint8_t *)malloc(n);
    uint8_t **pool = (uint8_t **)grow(cv->masks, &cv->cap_masks, cv->nmasks + 1, sizeof(uint8_t *));
    if (mask == NULL || pool == NULL) {
        free(mask);
        cv->failed = true;
        return;
    }
    cv->masks = pool;
    cv->masks[cv->nmasks++] = mask;
    int w = x1 - x0, ow = st->cx1 - st->cx0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float c = cv->cov[(size_t)(y - y0) * (size_t)w + (size_t)(x - x0)];
            if (st->mask != NULL)
                c *= (float)st->mask[(size_t)(y - st->cy0) * (size_t)ow + (size_t)(x - st->cx0)] *
                     (1.0f / 255.0f);
            mask[(size_t)(y - y0) * (size_t)w + (size_t)(x - x0)] = (uint8_t)(c * 255.0f + 0.5f);
        }
    st->mask = mask;
    st->cx0 = x0;
    st->cy0 = y0;
    st->cx1 = x1;
    st->cy1 = y1;
}

/* ---------------------------------------------------------------------------------------------------- */
/* Stroke                                                                                               */
/* ---------------------------------------------------------------------------------------------------- */

/* A convex polygon in user space, clipped to the band when one is set, moved to device space and added. */
static void add_user_poly(SwcCanvas *cv, EdgeList *l, const Pt *in, int n) {
    const State *st = cur(cv);
    Pt a[72], b[72];
    int na = n > 64 ? 64 : n;
    memcpy(a, in, (size_t)na * sizeof(Pt));
    if (st->box_on) {
        for (int pass = 0; pass < 4; pass++) {
            double lim = pass == 0   ? st->box_y0
                         : pass == 1 ? st->box_y1
                         : pass == 2 ? st->box_x0
                                     : st->box_x1;
            int nb = 0;
            for (int i = 0; i < na; i++) {
                Pt p = a[i], q = a[(i + 1) % na];
                double pv = pass < 2 ? p.y : p.x, qv = pass < 2 ? q.y : q.x;
                bool low = pass == 0 || pass == 2; /* keep values at or above lim, else at or below */
                bool pin = low ? pv >= lim : pv <= lim;
                bool qin = low ? qv >= lim : qv <= lim;
                if (pin && nb < 70)
                    b[nb++] = p;
                if (pin != qin && nb < 70) {
                    double t = (lim - pv) / (qv - pv);
                    b[nb].x = p.x + (q.x - p.x) * t;
                    b[nb].y = p.y + (q.y - p.y) * t;
                    if (pass < 2)
                        b[nb].y = lim;
                    else
                        b[nb].x = lim;
                    nb++;
                }
            }
            memcpy(a, b, (size_t)nb * sizeof(Pt));
            na = nb;
            if (na < 3)
                return;
        }
    }
    for (int i = 0; i < na; i++)
        a[i] = mat_apply(&st->ctm, a[i].x, a[i].y);
    el_poly(cv, l, a, na, true);
}

static void add_disc(SwcCanvas *cv, EdgeList *l, Pt c, double hw) {
    double scale = ctm_scale(cv), step = 0.6;
    if (hw * scale > 0.2)
        step = 2.0 * acos(1.0 - 0.03 / (hw * scale));
    int n = (int)ceil(2 * PI / step);
    n = n < 8 ? 8 : (n > 64 ? 64 : n);
    Pt p[64];
    for (int i = 0; i < n; i++) {
        double a = 2 * PI * (double)i / (double)n;
        p[i].x = c.x + hw * cos(a);
        p[i].y = c.y + hw * sin(a);
    }
    add_user_poly(cv, l, p, n);
}

static void stroke_edges(SwcCanvas *cv, EdgeList *l, double width) {
    const State *st = cur(cv);
    double hw = width / 2.0;
    el_reset(l);
    Pt *q = NULL;
    int qcap = 0;
    for (int si = 0; si < cv->nsub; si++) {
        const Sub *s = &cv->subs[si];
        if (s->segments == 0 && !s->closed)
            continue; /* a lone moveTo draws nothing */
        q = (Pt *)grow(q, &qcap, s->count + 1, sizeof(Pt));
        if (q == NULL) {
            cv->failed = true;
            return;
        }
        int n = 0;
        for (int k = 0; k < s->count; k++) {
            Pt p = cv->pts[s->start + k];
            if (n > 0 && fabs(p.x - q[n - 1].x) < 1e-9 && fabs(p.y - q[n - 1].y) < 1e-9)
                continue;
            q[n++] = p;
        }
        bool closed = s->closed;
        if (closed && n > 1 && fabs(q[0].x - q[n - 1].x) < 1e-9 && fabs(q[0].y - q[n - 1].y) < 1e-9)
            n--;
        if (n == 1) { /* a dot */
            if (st->cap == SWC_CAP_ROUND)
                add_disc(cv, l, q[0], hw);
            else if (st->cap == SWC_CAP_SQUARE) {
                Pt r[4] = {{q[0].x - hw, q[0].y - hw},
                           {q[0].x + hw, q[0].y - hw},
                           {q[0].x + hw, q[0].y + hw},
                           {q[0].x - hw, q[0].y + hw}};
                add_user_poly(cv, l, r, 4);
            }
            continue;
        }
        if (n < 2)
            continue;
        int segs = closed ? n : n - 1;
        for (int i = 0; i < segs; i++) {
            Pt a = q[i], b = q[(i + 1) % n];
            double dx = b.x - a.x, dy = b.y - a.y, len = hypot(dx, dy);
            double ux = dx / len, uy = dy / len, nx = -uy * hw, ny = ux * hw;
            double e0 = (!closed && i == 0 && st->cap == SWC_CAP_SQUARE) ? hw : 0.0;
            double e1 = (!closed && i == segs - 1 && st->cap == SWC_CAP_SQUARE) ? hw : 0.0;
            Pt r[4] = {{a.x - ux * e0 + nx, a.y - uy * e0 + ny},
                       {b.x + ux * e1 + nx, b.y + uy * e1 + ny},
                       {b.x + ux * e1 - nx, b.y + uy * e1 - ny},
                       {a.x - ux * e0 - nx, a.y - uy * e0 - ny}};
            add_user_poly(cv, l, r, 4);
        }
        if (!closed && st->cap == SWC_CAP_ROUND) {
            add_disc(cv, l, q[0], hw);
            add_disc(cv, l, q[n - 1], hw);
        }
        /* joins */
        int first = closed ? 0 : 1, last = closed ? n - 1 : n - 2;
        for (int i = first; i <= last; i++) {
            Pt prev = q[(i + n - 1) % n], v = q[i], next = q[(i + 1) % n];
            double d0x = v.x - prev.x, d0y = v.y - prev.y, d1x = next.x - v.x, d1y = next.y - v.y;
            double l0 = hypot(d0x, d0y), l1 = hypot(d1x, d1y);
            d0x /= l0;
            d0y /= l0;
            d1x /= l1;
            d1y /= l1;
            double cross = d0x * d1y - d0y * d1x, dot = d0x * d1x + d0y * d1y;
            if (fabs(cross) < 1e-9 && dot > 0)
                continue;
            double turn = acos(dot < -1 ? -1 : (dot > 1 ? 1 : dot));
            SwcLineJoin join = st->join;
            if (join == SWC_JOIN_ROUND && turn > 0.35) {
                add_disc(cv, l, v, hw);
                continue;
            }
            double side = cross > 0 ? -1.0 : 1.0;
            Pt a = {v.x + side * hw * (-d0y), v.y + side * hw * d0x};
            Pt b = {v.x + side * hw * (-d1y), v.y + side * hw * d1x};
            bool miter = join == SWC_JOIN_MITER || join == SWC_JOIN_ROUND;
            double ratio = 1.0 / cos(turn / 2.0); /* miter length over line width */
            if (miter && turn < PI - 1e-3 && ratio <= st->miter_limit) {
                double k = side * hw / (1.0 + dot);
                Pt tip = {v.x + k * ((-d0y) + (-d1y)), v.y + k * (d0x + d1x)};
                Pt w4[4] = {v, a, tip, b};
                add_user_poly(cv, l, w4, 4);
            } else {
                Pt w3[3] = {v, a, b};
                add_user_poly(cv, l, w3, 3);
            }
        }
    }
    free(q);
}

void swc_cv_stroke(SwcCanvas *cv) {
    const State *st = cur(cv);
    const Paint *paint = st->stroke_uses_fill ? &st->fill : &st->stroke;
    stroke_edges(cv, &cv->edges, st->line_width);
    if (st->hole > 0 && st->hole < st->line_width) {
        stroke_edges(cv, &cv->holes, st->hole);
        paint_edges(cv, &cv->edges, &cv->holes, SWC_NONZERO, paint);
    } else {
        paint_edges(cv, &cv->edges, NULL, SWC_NONZERO, paint);
    }
}
