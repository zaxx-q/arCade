#include <string.h>

#include "ghost_loop_logic.h"

/* ---- the levels ---------------------------------------------------------------------------------------
 * Every level is a room (see GL_ROOM_*) with solid walls, pads and doors. A "barrier" is a wall that spans
 * the whole height of the room (30 px thick) with a doorway: two wall pieces plus a door that fills the gap,
 * so a closed door can never be walked around. tests/test_ghost_loop.c proves that for every level, and that
 * each level can be beaten with its par number of ghosts (a scripted solution is replayed through the real
 * simulation). */
static const gl_Level k_levels[GL_LEVEL_COUNT] = {
    {"HOLD THE DOOR",
     "Stand on the pad to open its door. SPACE ends your round: the run so far becomes a ghost.",
     {200, 540},
     {1000, 380},
     2,
     1,
     2,
     {{640, 80, 670, 280}, {640, 480, 670, 680}},
     1,
     {{320, 180}},
     1,
     {{{640, 280, 670, 480}, 0}}},
    {"CHAIN REACTION",
     "Some pads sit behind doors. A ghost can open the way for another ghost.",
     {180, 380},
     {1040, 380},
     2,
     2,
     4,
     {{420, 80, 450, 300}, {420, 460, 450, 680}, {800, 80, 830, 300}, {800, 460, 830, 680}},
     2,
     {{300, 560}, {620, 180}},
     2,
     {{{420, 300, 450, 460}, 0}, {{800, 300, 830, 460}, 1}}},
    {"VENN",
     "One ghost, two doors. Look closely at the pads.",
     {180, 560},
     {1040, 380},
     1,
     1,
     4,
     {{520, 80, 550, 280}, {520, 480, 550, 680}, {780, 80, 810, 280}, {780, 480, 810, 680}},
     2,
     {{330, 200}, {374, 200}},
     2,
     {{{520, 280, 550, 480}, 0}, {{780, 280, 810, 480}, 1}}},
    {"RELAY",
     "A ghost replays your whole round, not just where it ended up.",
     {180, 380},
     {1060, 380},
     1,
     1,
     4,
     {{540, 80, 570, 300}, {540, 460, 570, 680}, {790, 80, 820, 300}, {790, 460, 820, 680}},
     2,
     {{300, 150}, {300, 610}},
     2,
     {{{540, 300, 570, 460}, 0}, {{790, 300, 820, 460}, 1}}},
    {"THE LONG WAY",
     "Some trips take most of a round. Plan your route.",
     {200, 620},
     {220, 260},
     2,
     1,
     4,
     {{330, 80, 360, 560}, {120, 380, 170, 410}, {270, 380, 330, 410}, {540, 240, 570, 680}},
     1,
     {{900, 420}},
     1,
     {{{170, 380, 270, 410}, 0}}},
    {"GRAND CENTRAL",
     "Three doors, two ghosts. Work out who needs whom.",
     {180, 380},
     {1100, 380},
     2,
     2,
     6,
     {{480, 80, 510, 300},
      {480, 460, 510, 680},
      {760, 80, 790, 300},
      {760, 460, 790, 680},
      {1010, 80, 1040, 300},
      {1010, 460, 1040, 680}},
     3,
     {{300, 150}, {300, 610}, {640, 150}},
     3,
     {{{480, 300, 510, 460}, 0}, {{760, 300, 790, 460}, 1}, {{1010, 300, 1040, 460}, 2}}},
};

const gl_Level *ghost_loop_level(int level) {
    return &k_levels[arc_clampi(level, 0, GL_LEVEL_COUNT - 1)];
}

/* ---- collision ----------------------------------------------------------------------------------------- */

/* Does a circle of the given radius centred on p overlap the rectangle? (touching does not count) */
static bool circle_hits_rect(Vec2 p, float radius, gl_Rect r) {
    float cx = arc_clampf(p.x, r.x0, r.x1), cy = arc_clampf(p.y, r.y0, r.y1);
    float dx = p.x - cx, dy = p.y - cy;
    return dx * dx + dy * dy < radius * radius;
}

/* Would an actor that moves from 'from' to 'to' end up inside something solid? Walls always stop it. A closed
 * door stops it from getting INTO the door, but an actor the door closed on may still walk out of it. */
static bool blocked(const gl_Level *lv, const bool door_open[], Vec2 from, Vec2 to) {
    for (int i = 0; i < lv->wall_count; i++)
        if (circle_hits_rect(to, GL_R, lv->walls[i]))
            return true;
    for (int i = 0; i < lv->door_count; i++)
        if (!door_open[i] && circle_hits_rect(to, GL_R, lv->doors[i].box) &&
            !circle_hits_rect(from, GL_R, lv->doors[i].box))
            return true;
    return false;
}

static int8_t sign8(float v) {
    return v > 0.1f ? 1 : (v < -0.1f ? -1 : 0);
}

/* One fixed step of movement for the player OR a ghost: both use exactly this function. Each axis is moved on
 * its own so an actor that runs into a wall slides along it instead of sticking. */
static Vec2 move_actor(const gl_Level *lv, const bool door_open[], Vec2 p, gl_Move m) {
    Vec2 dir = vec2_norm(vec2((float)m.mx, (float)m.my));
    Vec2 d = vec2_scale(dir, GL_SPEED * ARC_DT);
    Vec2 q = p;
    q.x = arc_clampf(p.x + d.x, GL_ROOM_X0 + GL_R, GL_ROOM_X1 - GL_R);
    if (blocked(lv, door_open, p, q))
        q.x = p.x;
    Vec2 r = q;
    r.y = arc_clampf(q.y + d.y, GL_ROOM_Y0 + GL_R, GL_ROOM_Y1 - GL_R);
    if (blocked(lv, door_open, q, r))
        r.y = q.y;
    return r;
}

/* ---- rounds and levels -----------------------------------------------------------------------------------
 */

/* Everybody back to the start, the recording of the round in progress is thrown away. Ghosts are kept. */
static void begin_round(ghost_loop_State *s) {
    const gl_Level *lv = ghost_loop_level(s->level);
    s->step = 0;
    s->player = lv->start;
    for (int g = 0; g < GL_MAX_GHOSTS; g++)
        s->ghost_pos[g] = lv->start;
    memset(s->pad_down, 0, sizeof(s->pad_down));
    memset(s->door_open, 0, sizeof(s->door_open));
    memset(s->recording, 0, sizeof(s->recording));
}

void ghost_loop_load_level(ghost_loop_State *s, int level) {
    int total = s->total;
    memset(s, 0, sizeof(*s));
    s->total = total;
    s->level = arc_clampi(level, 0, GL_LEVEL_COUNT - 1);
    begin_round(s);
}

void ghost_loop_init(ghost_loop_State *s, unsigned seed) {
    (void)seed; /* the campaign always starts on level 0 */
    memset(s, 0, sizeof(*s));
    ghost_loop_load_level(s, 0);
}

static void notify(ghost_loop_State *s, GlNotice n) {
    s->notice = n;
    s->notice_steps = GL_NOTICE_STEPS;
}

/* The round is over (SPACE or the timer): the run just played becomes the next ghost. If every ghost slot is
 * already used there is nothing to record, so the same round simply starts again. */
static void end_round(ghost_loop_State *s, bool timer_ran_out) {
    const gl_Level *lv = ghost_loop_level(s->level);
    if (s->ghost_count >= lv->max_ghosts) {
        begin_round(s);
        notify(s, timer_ran_out ? GL_NOTICE_TIME_UP : GL_NOTICE_NO_GHOSTS);
        return;
    }
    memcpy(s->ghost_moves[s->ghost_count], s->recording, sizeof(s->recording));
    s->ghost_count++;
    s->round++;
    begin_round(s);
}

/* The U key: forget the newest ghost and play its round again. */
static void undo_round(ghost_loop_State *s) {
    if (s->ghost_count > 0) {
        s->ghost_count--;
        s->round--;
    }
    begin_round(s);
}

/* The C key: back to a clean level with no ghosts at all. */
static void restart_level(ghost_loop_State *s) {
    s->ghost_count = 0;
    s->round = 0;
    begin_round(s);
}

int ghost_loop_level_points(const ghost_loop_State *s) {
    const gl_Level *lv = ghost_loop_level(s->level);
    return arc_maxi(100, 1500 - 300 * arc_maxi(0, s->round - lv->par_ghosts) - s->step);
}

static void next_level(ghost_loop_State *s) {
    if (s->level + 1 >= GL_LEVEL_COUNT) {
        s->won = true;
        return;
    }
    ghost_loop_load_level(s, s->level + 1);
}

/* Is any actor (the player or a ghost) standing on this pad? */
static bool pad_pressed(const ghost_loop_State *s, Vec2 pad) {
    if (vec2_dist(s->player, pad) < GL_PAD_R)
        return true;
    for (int g = 0; g < s->ghost_count; g++)
        if (vec2_dist(s->ghost_pos[g], pad) < GL_PAD_R)
            return true;
    return false;
}

void ghost_loop_step(ghost_loop_State *s, const ArcadeInput *in, float dt) {
    (void)dt;
    if (s->won)
        return;
    const gl_Level *lv = ghost_loop_level(s->level);
    if (s->notice_steps > 0 && --s->notice_steps == 0)
        s->notice = GL_NOTICE_NONE;
    if (s->cleared) { /* the level is done: wait a moment, then SPACE moves on */
        s->clear_hold++;
        if (s->clear_hold >= GL_CLEAR_HOLD && in->action_pressed)
            next_level(s);
        return;
    }
    if (in->typed == 'c' || in->typed == 'C') {
        restart_level(s);
        return;
    }
    if (in->typed == 'r' || in->typed == 'R') {
        begin_round(s);
        return;
    }
    if (in->typed == 'u' || in->typed == 'U') {
        undo_round(s);
        return;
    }
    if (in->action_pressed) {
        end_round(s, false);
        return;
    }

    gl_Move m;
    m.mx = sign8(in->move_x);
    m.my = sign8(in->move_y);
    s->recording[s->step] = m;

    /* pads and doors are decided from the positions at the start of the step, so everybody agrees */
    for (int p = 0; p < lv->pad_count; p++)
        s->pad_down[p] = pad_pressed(s, lv->pads[p]);
    for (int d = 0; d < lv->door_count; d++)
        s->door_open[d] = s->pad_down[lv->doors[d].pad];
    s->player = move_actor(lv, s->door_open, s->player, m);
    for (int g = 0; g < s->ghost_count; g++)
        s->ghost_pos[g] = move_actor(lv, s->door_open, s->ghost_pos[g], s->ghost_moves[g][s->step]);

    if (vec2_dist(s->player, lv->exit) < GL_EXIT_R) {
        s->level_points = ghost_loop_level_points(s);
        s->total += s->level_points;
        s->cleared = true;
        s->clear_hold = 0;
        return;
    }
    s->step++;
    if (s->step >= GL_ROUND_STEPS)
        end_round(s, true);
}

bool ghost_loop_is_over(const ghost_loop_State *s) {
    return s->won;
}

int ghost_loop_score(const ghost_loop_State *s) {
    return s->total;
}

/* ---- level validation ------------------------------------------------------------------------------------
 */

#define GL_CELL 8.0f  /* size of a flood-fill cell in pixels */
#define GL_GRID_W 130 /* (GL_ROOM_X1 - GL_ROOM_X0) / GL_CELL */
#define GL_GRID_H 75  /* (GL_ROOM_Y1 - GL_ROOM_Y0) / GL_CELL */

/* Can a circle of radius r stand with its centre at p? Closed doors count as walls here. */
static bool spot_is_free(const gl_Level *lv, unsigned open_doors, Vec2 p, float r) {
    if (p.x < GL_ROOM_X0 + r || p.x > GL_ROOM_X1 - r || p.y < GL_ROOM_Y0 + r || p.y > GL_ROOM_Y1 - r)
        return false;
    for (int i = 0; i < lv->wall_count; i++)
        if (circle_hits_rect(p, r, lv->walls[i]))
            return false;
    for (int i = 0; i < lv->door_count; i++)
        if (!((open_doors >> i) & 1u) && circle_hits_rect(p, r, lv->doors[i].box))
            return false;
    return true;
}

bool ghost_loop_can_reach(int level, unsigned open_doors, Vec2 from, Vec2 target, float radius) {
    const gl_Level *lv = ghost_loop_level(level);
    /* Slightly smaller than the real actor, and 8-way steps: this can only say "reachable" when the real
     * actor might not manage, never the other way round. That is the safe side for a "no loophole" check. */
    float r = GL_R - 2.0f;
    static const int dx[8] = {1, -1, 0, 0, 1, 1, -1, -1}, dy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    unsigned char seen[GL_GRID_H][GL_GRID_W];
    int queue[GL_GRID_H * GL_GRID_W];
    int head = 0, tail = 0;
    memset(seen, 0, sizeof(seen));
    int sx = (int)((from.x - GL_ROOM_X0) / GL_CELL), sy = (int)((from.y - GL_ROOM_Y0) / GL_CELL);
    if (sx < 0 || sy < 0 || sx >= GL_GRID_W || sy >= GL_GRID_H)
        return false;
    seen[sy][sx] = 1;
    queue[tail++] = sy * GL_GRID_W + sx;
    while (head < tail) {
        int cell = queue[head++];
        int cx = cell % GL_GRID_W, cy = cell / GL_GRID_W;
        Vec2 c = vec2(GL_ROOM_X0 + ((float)cx + 0.5f) * GL_CELL, GL_ROOM_Y0 + ((float)cy + 0.5f) * GL_CELL);
        if (vec2_dist(c, target) < radius)
            return true;
        for (int k = 0; k < 8; k++) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (nx < 0 || ny < 0 || nx >= GL_GRID_W || ny >= GL_GRID_H || seen[ny][nx])
                continue;
            Vec2 n =
                vec2(GL_ROOM_X0 + ((float)nx + 0.5f) * GL_CELL, GL_ROOM_Y0 + ((float)ny + 0.5f) * GL_CELL);
            if (!spot_is_free(lv, open_doors, n, r))
                continue;
            seen[ny][nx] = 1;
            queue[tail++] = ny * GL_GRID_W + nx;
        }
    }
    return false;
}
