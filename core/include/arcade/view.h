/* How the 1280 x 720 design canvas fits a window of any size. Pure C99: no raylib in this file.
 *
 * Every screen is laid out on the design canvas (ARC_SCREEN_W x ARC_SCREEN_H). The window can be any size
 * (resized by hand, forced to full screen by a window manager, F11): the canvas is scaled uniformly by the
 * biggest factor that fits and centred, so nothing is stretched. When the window is not 16:9, the strips left
 * over on two sides are still part of the window: `left`, `top`, `right` and `bottom` give its edges in
 * design units, so a backdrop or a full-width bar can reach them (they are 0 / ARC_SCREEN_W / ARC_SCREEN_H
 * when the window is exactly 16:9).
 *
 * The raylib side (core/gfx/gfx.c) applies this as a transform at the start of every frame and maps the
 * mouse back through arc_view_to_design(). */
#ifndef ARCADE_VIEW_H
#define ARCADE_VIEW_H

#include "arcade/common.h"
#include "arcade/mathx.h"

/* The smallest window the front-end allows: half the design canvas. */
#define ARC_VIEW_MIN_W (ARC_SCREEN_W / 2)
#define ARC_VIEW_MIN_H (ARC_SCREEN_H / 2)

typedef struct ArcView {
    int win_w, win_h;    /* window size in pixels */
    float scale;         /* window pixels per design pixel */
    float off_x, off_y;  /* window pixel where design (0, 0) lands (a whole number) */
    float left, top;     /* the window's top-left corner in design units (<= 0) */
    float right, bottom; /* the window's bottom-right corner in design units (>= the canvas size) */
} ArcView;

/* Fits the canvas into a window of win_w x win_h pixels (values below 1 count as 1). */
void arc_view_fit(ArcView *v, int win_w, int win_h);

/* Window pixel <-> design position. */
Vec2 arc_view_to_design(const ArcView *v, float win_x, float win_y);
Vec2 arc_view_to_window(const ArcView *v, float x, float y);

/* The smallest whole-pixel window rectangle that covers the design rectangle (x, y, w, h): what a scissor
 * needs, because the GPU clips in window pixels. */
void arc_view_window_rect(const ArcView *v, float x, float y, float w, float h, int *wx, int *wy, int *ww,
                          int *wh);

/* The scale a font atlas is rasterised for: the view scale rounded to the nearest 1/8 (so that dragging a
 * window edge does not build a new set of atlases for every pixel of size), never below 1/2. The common
 * exact sizes (1x, 1.5x, 2x, 3x for 720p, 1080p, 1440p, 4K) are kept exactly. */
float arc_view_text_scale(float view_scale);

#endif
