#include "time_shooter_logic.h"

#define PLAYER_SPEED 260.0f
#define PLAYER_R 14.0f
#define ENEMY_R 12.0f
#define BULLET_R 6.0f
#define BULLET_SPEED 720.0f
#define FIRE_COOLDOWN 0.25f
#define FIRE_BURST 0.20f /* firing lets time run for a moment so shots can land */
#define TOP_MARGIN 44.0f

void time_shooter_init(time_shooter_State *s, unsigned seed) {
    rng_seed(&s->rng, seed);
    for (int i = 0; i < TS_MAX_ENEMIES; i++)
        s->enemies[i].alive = false;
    for (int i = 0; i < TS_MAX_BULLETS; i++)
        s->bullets[i].alive = false;
    s->player = vec2(ARC_SCREEN_W / 2.0f, ARC_SCREEN_H / 2.0f);
    s->time_scale = TS_IDLE_SCALE;
    s->spawn_timer = 0.8f;
    s->cooldown = 0.0f;
    s->burst = 0.0f;
    s->world_time = 0.0f;
    s->kills = 0;
    s->last_kill = s->player;
    s->over = false;
}

/* TODO(Member 1): enemy kinds (fast, shooter), a wave table and pickups: see GAME.md "Implementation steps"
 * 1-4. */
static void spawn_enemy(time_shooter_State *s) {
    for (int i = 0; i < TS_MAX_ENEMIES; i++) {
        if (s->enemies[i].alive)
            continue;
        Vec2 p = vec2(0, 0);
        for (int attempt = 0; attempt < 6; attempt++) { /* try to spawn away from the player */
            int side = rng_range(&s->rng, 0, 3);
            float along_x = rng_rangef(&s->rng, 0.0f, (float)ARC_SCREEN_W);
            float along_y = rng_rangef(&s->rng, TOP_MARGIN, (float)ARC_SCREEN_H);
            p = side == 0   ? vec2(along_x, TOP_MARGIN)
                : side == 1 ? vec2(along_x, (float)ARC_SCREEN_H)
                : side == 2 ? vec2(0.0f, along_y)
                            : vec2((float)ARC_SCREEN_W, along_y);
            if (vec2_dist(p, s->player) > 280.0f)
                break;
        }
        s->enemies[i].pos = p;
        s->enemies[i].speed = rng_rangef(&s->rng, 70.0f, 110.0f) + (float)s->kills * 1.5f;
        s->enemies[i].alive = true;
        return;
    }
}

static void fire_bullet(time_shooter_State *s, Vec2 target) {
    Vec2 dir = vec2_norm(vec2_sub(target, s->player));
    if (dir.x == 0.0f && dir.y == 0.0f)
        dir = vec2(1.0f, 0.0f);
    for (int i = 0; i < TS_MAX_BULLETS; i++) {
        if (s->bullets[i].alive)
            continue;
        s->bullets[i].pos = s->player;
        s->bullets[i].vel = vec2_scale(dir, BULLET_SPEED);
        s->bullets[i].alive = true;
        return;
    }
}

void time_shooter_step(time_shooter_State *s, const ArcadeInput *in, float dt) {
    if (s->over)
        return;

    /* 1. the player always lives in real time */
    Vec2 move = vec2(in->move_x, in->move_y);
    bool moving = vec2_len(move) > 0.1f;
    if (moving) {
        s->player = vec2_add(s->player, vec2_scale(vec2_norm(move), PLAYER_SPEED * dt));
        s->player.x = arc_clampf(s->player.x, PLAYER_R, ARC_SCREEN_W - PLAYER_R);
        s->player.y = arc_clampf(s->player.y, TOP_MARGIN + PLAYER_R, ARC_SCREEN_H - PLAYER_R);
    }
    s->cooldown = arc_maxf(0.0f, s->cooldown - dt);
    s->burst = arc_maxf(0.0f, s->burst - dt);
    if ((in->fire_pressed || in->fire) && s->cooldown <= 0.0f) {
        fire_bullet(s, vec2(in->aim_x, in->aim_y));
        s->cooldown = FIRE_COOLDOWN;
        s->burst = FIRE_BURST;
    }

    /* 2. the world only advances as fast as the player acts */
    s->time_scale = (moving || s->burst > 0.0f) ? 1.0f : TS_IDLE_SCALE;
    float wdt = dt * s->time_scale;
    s->world_time += wdt;

    s->spawn_timer -= wdt;
    if (s->spawn_timer <= 0.0f) {
        spawn_enemy(s);
        s->spawn_timer = arc_maxf(0.5f, 1.4f - (float)s->kills * 0.03f);
    }

    for (int i = 0; i < TS_MAX_ENEMIES; i++) {
        ts_Enemy *e = &s->enemies[i];
        if (!e->alive)
            continue;
        Vec2 dir = vec2_norm(vec2_sub(s->player, e->pos));
        e->pos = vec2_add(e->pos, vec2_scale(dir, e->speed * wdt));
        if (arc_circles_overlap(e->pos, ENEMY_R, s->player, PLAYER_R))
            s->over = true;
    }

    for (int b = 0; b < TS_MAX_BULLETS; b++) {
        ts_Bullet *bl = &s->bullets[b];
        if (!bl->alive)
            continue;
        bl->pos = vec2_add(bl->pos, vec2_scale(bl->vel, wdt));
        if (bl->pos.x < -20 || bl->pos.x > ARC_SCREEN_W + 20 || bl->pos.y < TOP_MARGIN - 20 ||
            bl->pos.y > ARC_SCREEN_H + 20) {
            bl->alive = false;
            continue;
        }
        for (int i = 0; i < TS_MAX_ENEMIES; i++) {
            ts_Enemy *e = &s->enemies[i];
            if (e->alive && arc_circles_overlap(bl->pos, BULLET_R, e->pos, ENEMY_R)) {
                e->alive = false;
                bl->alive = false;
                s->kills++;
                s->last_kill = e->pos;
                break;
            }
        }
    }
}

bool time_shooter_is_over(const time_shooter_State *s) {
    return s->over;
}

int time_shooter_score(const time_shooter_State *s) {
    return s->kills * 100 + (int)s->world_time;
}
