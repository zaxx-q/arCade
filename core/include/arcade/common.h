/* Shared constants and tiny inline helpers. Pure C99: no raylib in this file. */
#ifndef ARCADE_COMMON_H
#define ARCADE_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* M_PI is not standard C99 (and is missing with -std=c99 / MSVC): always use these. */
#define ARC_PI 3.14159265358979323846f
#define ARC_TAU 6.28318530717958647692f

#define ARC_SCREEN_W 1280
#define ARC_SCREEN_H 720
#define ARC_FPS 60
#define ARC_DT (1.0f / 60.0f) /* fixed logic step: every game advances in steps of this size */

#define ARC_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

static inline int arc_mini(int a, int b) {
    return a < b ? a : b;
}
static inline int arc_maxi(int a, int b) {
    return a > b ? a : b;
}
static inline float arc_minf(float a, float b) {
    return a < b ? a : b;
}
static inline float arc_maxf(float a, float b) {
    return a > b ? a : b;
}
static inline int arc_clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float arc_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

#endif
