/* Beat Dodge - PURE game logic (no raylib). Owner: Member 5
 * Hook: rhythm bullet-dodger. Obstacles are spawned from a beat map and arrive exactly on the beat. */
#ifndef BEAT_DODGE_LOGIC_H
#define BEAT_DODGE_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/rng.h"

#define BD_LANES 3
#define BD_MAX_BEATS 256
#define BD_LEAD_BEATS 3.0f /* an obstacle takes this many beats to fall from the top to the player line */
#define BD_START_LIVES 3
#define BD_TOP_Y 60.0f
#define BD_PLAYER_Y 580.0f
#define BD_LANE_SPACING 220.0f

typedef struct bd_BeatMap {
    int bpm;
    int length;                /* number of beats */
    int8_t lane[BD_MAX_BEATS]; /* -1 = rest, 0..BD_LANES-1 = obstacle in that lane */
} bd_BeatMap;

/* Beat map file format (text files in assets/beatmaps/):
 *   # comment
 *   bpm 120
 *   ..0.1.2.   <- one character per beat: '.' = rest, '0' '1' '2' = obstacle lane (lines are concatenated) */
bool bd_beatmap_load(const char *path, bd_BeatMap *out); /* false if missing or invalid (out is untouched) */
void bd_beatmap_default(bd_BeatMap *out, unsigned seed); /* built-in generated map, always valid */

typedef struct beat_dodge_State {
    bd_BeatMap map;
    float time;      /* seconds since the start */
    float spb;       /* seconds per beat */
    int next_hit;    /* first beat whose obstacle has not yet arrived at the player line */
    int last_beat;   /* beat number seen in the previous step */
    bool beat_event; /* true for the one step in which a new beat begins (front-end plays a tick) */
    int lane;        /* player lane */
    int lives, dodged, combo, best_combo;
    int score;
    float hit_flash; /* seconds of red flash after a hit */
    bool over, won;
} beat_dodge_State;

void beat_dodge_init(beat_dodge_State *s, unsigned seed);
void beat_dodge_use_map(beat_dodge_State *s, const bd_BeatMap *map); /* restart with another map */
void beat_dodge_step(beat_dodge_State *s, const ArcadeInput *in, float dt);
bool beat_dodge_is_over(const beat_dodge_State *s);
int beat_dodge_score(const beat_dodge_State *s);
/* Vertical pixel position of the obstacle of beat i (for drawing); < BD_TOP_Y means "not spawned yet". */
float beat_dodge_obstacle_y(const beat_dodge_State *s, int i);

#endif
