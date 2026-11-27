#include "arcade/view.h"

#include <math.h>

void arc_view_fit(ArcView *v, int win_w, int win_h) {
    v->win_w = win_w < 1 ? 1 : win_w;
    v->win_h = win_h < 1 ? 1 : win_h;
    /* the biggest uniform scale at which the whole canvas is still visible */
    v->scale = arc_minf((float)v->win_w / (float)ARC_SCREEN_W, (float)v->win_h / (float)ARC_SCREEN_H);
    /* centre it; snapping the corner to a whole pixel keeps the edges of the canvas sharp */
    v->off_x = floorf(((float)v->win_w - (float)ARC_SCREEN_W * v->scale) * 0.5f + 0.5f);
    v->off_y = floorf(((float)v->win_h - (float)ARC_SCREEN_H * v->scale) * 0.5f + 0.5f);
    v->left = -v->off_x / v->scale;
    v->top = -v->off_y / v->scale;
    v->right = ((float)v->win_w - v->off_x) / v->scale;
    v->bottom = ((float)v->win_h - v->off_y) / v->scale;
}

Vec2 arc_view_to_design(const ArcView *v, float win_x, float win_y) {
    return vec2((win_x - v->off_x) / v->scale, (win_y - v->off_y) / v->scale);
}

Vec2 arc_view_to_window(const ArcView *v, float x, float y) {
    return vec2(v->off_x + x * v->scale, v->off_y + y * v->scale);
}

void arc_view_window_rect(const ArcView *v, float x, float y, float w, float h, int *wx, int *wy, int *ww,
                          int *wh) {
    Vec2 a = arc_view_to_window(v, x, y);
    Vec2 b = arc_view_to_window(v, x + w, y + h);
    int x0 = (int)floorf(a.x), y0 = (int)floorf(a.y);
    int x1 = (int)ceilf(b.x), y1 = (int)ceilf(b.y);
    *wx = x0;
    *wy = y0;
    *ww = x1 - x0;
    *wh = y1 - y0;
}

float arc_view_text_scale(float view_scale) {
    float s = floorf(view_scale * 8.0f + 0.5f) / 8.0f;
    return s < 0.5f ? 0.5f : s;
}
