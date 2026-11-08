#include "@ID@_logic.h"

#define PLAYER_SPEED 260.0f
#define PLAYER_R 14.0f
#define PELLET_R 10.0f
#define ROUND_SECONDS 20.0f

static void respawn_pellet(@ID@_State *s) {
    s->pellet = vec2(rng_rangef(&s->rng, 60.0f, ARC_SCREEN_W - 60.0f), rng_rangef(&s->rng, 90.0f, ARC_SCREEN_H - 60.0f));
}

void @ID@_init(@ID@_State *s, unsigned seed) {
    rng_seed(&s->rng, seed);
    s->pos = vec2(ARC_SCREEN_W / 2.0f, ARC_SCREEN_H / 2.0f);
    s->score = 0;
    s->time_left = ROUND_SECONDS;
    respawn_pellet(s);
}

void @ID@_step(@ID@_State *s, const ArcadeInput *in, float dt) {
    if (@ID@_is_over(s)) return;
    s->time_left -= dt;
    Vec2 dir = vec2_norm(vec2(in->move_x, in->move_y));
    s->pos = vec2_add(s->pos, vec2_scale(dir, PLAYER_SPEED * dt));
    s->pos.x = arc_clampf(s->pos.x, PLAYER_R, ARC_SCREEN_W - PLAYER_R);
    s->pos.y = arc_clampf(s->pos.y, 40.0f + PLAYER_R, ARC_SCREEN_H - PLAYER_R);
    if (arc_circles_overlap(s->pos, PLAYER_R, s->pellet, PELLET_R)) {
        s->score += 10;
        respawn_pellet(s);
    }
}

bool @ID@_is_over(const @ID@_State *s) { return s->time_left <= 0.0f; }

int @ID@_score(const @ID@_State *s) { return s->score; }
