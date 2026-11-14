/* Time Shooter - PURE game logic (no raylib). Owner: Member 1
 * Hook: time only moves when YOU move (or just fired). Stand still and the whole world freezes. */
#ifndef TIME_SHOOTER_LOGIC_H
#define TIME_SHOOTER_LOGIC_H

#include "arcade/common.h"
#include "arcade/input.h"
#include "arcade/mathx.h"
#include "arcade/rng.h"

#define TS_MAX_ENEMIES 40
#define TS_MAX_BULLETS 24
#define TS_IDLE_SCALE 0.04f /* world speed while the player stands still */

typedef struct ts_Enemy {
    Vec2 pos;
    float speed;
    bool alive;
} ts_Enemy;

typedef struct ts_Bullet {
    Vec2 pos, vel;
    bool alive;
} ts_Bullet;

typedef struct time_shooter_State {
    Vec2 player;
    float time_scale; /* 1.0 = normal, TS_IDLE_SCALE = frozen */
    ts_Enemy enemies[TS_MAX_ENEMIES];
    ts_Bullet bullets[TS_MAX_BULLETS];
    float spawn_timer, cooldown, burst;
    float world_time; /* seconds of WORLD time survived (frozen time does not count) */
    int kills;
    Vec2 last_kill; /* where the latest kill happened (for particles in the front-end) */
    bool over;
    Rng rng;
} time_shooter_State;

void time_shooter_init(time_shooter_State *s, unsigned seed);
void time_shooter_step(time_shooter_State *s, const ArcadeInput *in, float dt);
bool time_shooter_is_over(const time_shooter_State *s);
int time_shooter_score(const time_shooter_State *s);

#endif
