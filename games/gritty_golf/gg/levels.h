#ifndef GOLF_LEVELS_H
#define GOLF_LEVELS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* [arCade patch 1] arCade already defines Vec2 with the same layout (two floats). A file that sees both
 * headers would get two different types, so CMake defines GG_VEC2_FROM_ARCADE and we reuse arCade's. */
#ifdef GG_VEC2_FROM_ARCADE
#include "arcade/mathx.h"
#else
typedef struct {
    float x;
    float y;
} Vec2;
#endif

typedef struct {
    const Vec2* points;
    int point_count;
} PolygonData;

typedef struct {
    Vec2 position;          /* World position (pivot) */
    float rotation;         /* Radians */
    float width;            /* Wall bounding width */
    float height;           /* Wall bounding height */
    Vec2 collider_offset;   /* Local collider center offset (typically 0.0, 78.0) */
    Vec2 open_displacement; /* World displacement vector (dx, dy) when opened */
    float tween_duration;   /* 0.3s */
    int tween_type;         /* 0 = TRANS_BACK (ease out), 1 = TRANS_CUBIC (ease in-out) */
    bool play_hit_sfx;      /* true if SFX_BALL_HIT plays when opened */
} WallData;

typedef struct {
    Vec2 position;          /* Button world center (radius 18.0) */
    int target_wall_index;  /* Index into level's walls array, or -1 if none */
} ButtonData;

typedef struct {
    int index;
    float camera_zoom;      /* 1.0, 0.75, 0.66, 0.33 */
    Vec2 player_spawn;
    Vec2 flagpole_pos;
    float flagpole_rotation;
    bool has_banner;        /* True for Level 0 */

    const PolygonData* polylands;
    int polyland_count;

    const PolygonData* death_polylands;
    int death_polyland_count;

    const WallData* walls;
    int wall_count;

    const ButtonData* buttons;
    int button_count;

    const Vec2* strikes;    /* +3 strike pickup positions (radius 16.03) */
    int strike_count;
} LevelData;

#define GOLF_LEVEL_COUNT 25

extern const LevelData g_levels[GOLF_LEVEL_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* GOLF_LEVELS_H */
