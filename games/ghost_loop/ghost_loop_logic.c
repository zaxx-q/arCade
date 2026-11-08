#include <string.h>

#include "ghost_loop_logic.h"

static int8_t sign8(float v) {
    return v > 0.1f ? 1 : (v < -0.1f ? -1 : 0);
}

static Vec2 start_pos(void) {
    return vec2(GL_START_X, GL_START_Y);
}

static bool overlaps_door(Vec2 p) {
    return p.x + GL_R > GL_DOOR_X && p.x - GL_R < GL_DOOR_X + GL_DOOR_W && p.y + GL_R > GL_DOOR_Y0 &&
           p.y - GL_R < GL_DOOR_Y1;
}

/* One fixed step of movement for the player OR a ghost: both use exactly this function. */
static Vec2 move_actor(Vec2 p, gl_Move m, bool door_open) {
    Vec2 dir = vec2_norm(vec2((float)m.mx, (float)m.my));
    Vec2 np = vec2_add(p, vec2_scale(dir, GL_SPEED * ARC_DT));
    np.x = arc_clampf(np.x, GL_ROOM_X0 + GL_R, GL_ROOM_X1 - GL_R);
    np.y = arc_clampf(np.y, GL_ROOM_Y0 + GL_R, GL_ROOM_Y1 - GL_R);
    if (!door_open && overlaps_door(np) && !overlaps_door(p))
        return p; /* a closed door blocks entry */
    return np;
}

static void begin_round(ghost_loop_State *s) {
    s->step = 0;
    s->door_open = false;
    s->player = start_pos();
    for (int g = 0; g < GL_MAX_GHOSTS; g++)
        s->ghost_pos[g] = start_pos();
    memset(s->recording, 0, sizeof(s->recording));
}

void ghost_loop_init(ghost_loop_State *s, unsigned seed) {
    (void)seed; /* TODO(Member 4): vary the room layout per seed */
    memset(s, 0, sizeof(*s));
    begin_round(s);
}

/* TODO(Member 4): several pads/doors and timed buttons: generalise pad_pressed() into a loop over a pad array
 * (GAME.md step 1). */
static bool pad_pressed(const ghost_loop_State *s) {
    Vec2 pad = vec2(GL_PAD_X, GL_PAD_Y);
    if (vec2_dist(s->player, pad) < GL_PAD_R)
        return true;
    for (int g = 0; g < s->ghost_count; g++)
        if (vec2_dist(s->ghost_pos[g], pad) < GL_PAD_R)
            return true;
    return false;
}

static void end_round(ghost_loop_State *s) {
    if (s->ghost_count >= GL_MAX_GHOSTS) { /* no more ghosts to record: out of tries */
        s->lost = true;
        return;
    }
    memcpy(s->ghost_moves[s->ghost_count], s->recording, sizeof(s->recording));
    s->ghost_count++;
    s->round++;
    begin_round(s);
}

void ghost_loop_step(ghost_loop_State *s, const ArcadeInput *in, float dt) {
    (void)dt;
    if (s->won || s->lost)
        return;
    gl_Move m;
    m.mx = sign8(in->move_x);
    m.my = sign8(in->move_y);
    s->recording[s->step] = m;

    s->door_open = pad_pressed(s); /* decided from the positions at the start of the step */
    s->player = move_actor(s->player, m, s->door_open);
    for (int g = 0; g < s->ghost_count; g++)
        s->ghost_pos[g] = move_actor(s->ghost_pos[g], s->ghost_moves[g][s->step], s->door_open);

    if (vec2_dist(s->player, vec2(GL_EXIT_X, GL_EXIT_Y)) < GL_EXIT_R) {
        s->won = true;
        s->score = arc_maxi(100, 1500 - 300 * s->round - s->step);
        return;
    }
    s->step++;
    if (s->step >= GL_ROUND_STEPS)
        end_round(s);
}

bool ghost_loop_is_over(const ghost_loop_State *s) {
    return s->won || s->lost;
}

int ghost_loop_score(const ghost_loop_State *s) {
    return s->score;
}
