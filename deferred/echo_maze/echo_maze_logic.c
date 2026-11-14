#include "echo_maze_logic.h"

#define PLAYER_R 10.0f
#define PLAYER_SPEED 170.0f

static const int k_dx[4] = {1, 0, -1, 0};
static const int k_dy[4] = {0, 1, 0, -1};

/* Recursive backtracking: carve a random path, then back up and branch. Produces a "perfect" maze
 * (exactly one route between any two floor tiles). Recursion depth <= number of lattice cells (120). */
static void carve(echo_maze_State *s, int cx, int cy) {
    s->wall[cy][cx] = 0;
    int dirs[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) { /* shuffle the 4 directions */
        int j = rng_range(&s->rng, 0, i), t = dirs[i];
        dirs[i] = dirs[j];
        dirs[j] = t;
    }
    for (int k = 0; k < 4; k++) {
        int nx = cx + k_dx[dirs[k]] * 2, ny = cy + k_dy[dirs[k]] * 2;
        if (nx < 1 || ny < 1 || nx > EM_W - 2 || ny > EM_H - 2)
            continue;
        if (s->wall[ny][nx] == 0)
            continue;                                        /* already visited */
        s->wall[cy + k_dy[dirs[k]]][cx + k_dx[dirs[k]]] = 0; /* knock down the wall between */
        carve(s, nx, ny);
    }
}

Vec2 echo_maze_tile_center(int tx, int ty) {
    return vec2(EM_OX + ((float)tx + 0.5f) * EM_TILE, EM_OY + ((float)ty + 0.5f) * EM_TILE);
}

static bool is_wall_at(const echo_maze_State *s, float px, float py) {
    int tx = (int)((px - EM_OX) / EM_TILE), ty = (int)((py - EM_OY) / EM_TILE);
    if (px < EM_OX || py < EM_OY || tx < 0 || ty < 0 || tx >= EM_W || ty >= EM_H)
        return true;
    return s->wall[ty][tx] != 0;
}

static bool blocked(const echo_maze_State *s, float x, float y) {
    return is_wall_at(s, x - PLAYER_R, y - PLAYER_R) || is_wall_at(s, x + PLAYER_R, y - PLAYER_R) ||
           is_wall_at(s, x - PLAYER_R, y + PLAYER_R) || is_wall_at(s, x + PLAYER_R, y + PLAYER_R);
}

void echo_maze_init(echo_maze_State *s, unsigned seed) {
    for (int y = 0; y < EM_H; y++)
        for (int x = 0; x < EM_W; x++) {
            s->wall[y][x] = 1;
            s->reveal[y][x] = 0.0f;
        }
    rng_seed(&s->rng, seed);
    carve(s, 1, 1);
    s->player = echo_maze_tile_center(1, 1);
    s->ping_origin = s->player;
    s->ping_r = 0.0f;
    s->pinging = false;
    s->ping_cooldown = 0.0f;
    s->exit_glow = 0.0f;
    s->pings = 0;
    s->time = 0.0f;
    s->won = false;
}

bool echo_maze_los_clear(const echo_maze_State *s, Vec2 from, int tx, int ty) {
    Vec2 to = echo_maze_tile_center(tx, ty);
    float dist = vec2_dist(from, to);
    Vec2 dir = vec2_norm(vec2_sub(to, from));
    for (float d = 4.0f; d < dist; d += 6.0f) {
        Vec2 p = vec2_add(from, vec2_scale(dir, d));
        int px = (int)((p.x - EM_OX) / EM_TILE), py = (int)((p.y - EM_OY) / EM_TILE);
        if (px < 0 || py < 0 || px >= EM_W || py >= EM_H)
            return false;
        if (s->wall[py][px] && !(px == tx && py == ty))
            return false;
    }
    return true;
}

/* TODO(Member 2): enemies that hunt toward the last ping, bigger mazes, a ping range upgrade (GAME.md steps
 * 1-2). */
static void advance_ping(echo_maze_State *s, float dt) {
    float old_r = s->ping_r;
    s->ping_r += EM_PING_SPEED * dt;
    for (int y = 0; y < EM_H; y++)
        for (int x = 0; x < EM_W; x++) {
            bool is_exit = (x == EM_W - 2 && y == EM_H - 2);
            if (!s->wall[y][x] && !is_exit)
                continue;
            float d = vec2_dist(s->ping_origin, echo_maze_tile_center(x, y));
            if (d <= old_r || d > s->ping_r)
                continue; /* the ring has not just crossed this tile */
            if (!echo_maze_los_clear(s, s->ping_origin, x, y))
                continue;
            if (is_exit)
                s->exit_glow = EM_REVEAL_TIME;
            else
                s->reveal[y][x] = EM_REVEAL_TIME;
        }
    if (s->ping_r > EM_PING_RANGE)
        s->pinging = false;
}

void echo_maze_step(echo_maze_State *s, const ArcadeInput *in, float dt) {
    if (s->won)
        return;
    s->time += dt;
    s->ping_cooldown = arc_maxf(0.0f, s->ping_cooldown - dt);
    s->exit_glow = arc_maxf(0.0f, s->exit_glow - dt);
    for (int y = 0; y < EM_H; y++)
        for (int x = 0; x < EM_W; x++)
            s->reveal[y][x] = arc_maxf(0.0f, s->reveal[y][x] - dt);

    /* move one axis at a time so the player slides along walls */
    Vec2 dir = vec2_norm(vec2(in->move_x, in->move_y));
    float nx = s->player.x + dir.x * PLAYER_SPEED * dt;
    if (!blocked(s, nx, s->player.y))
        s->player.x = nx;
    float ny = s->player.y + dir.y * PLAYER_SPEED * dt;
    if (!blocked(s, s->player.x, ny))
        s->player.y = ny;

    if (in->action_pressed && s->ping_cooldown <= 0.0f) {
        s->pinging = true;
        s->ping_origin = s->player;
        s->ping_r = 0.0f;
        s->ping_cooldown = EM_PING_COOLDOWN;
        s->pings++;
    }
    if (s->pinging)
        advance_ping(s, dt);

    int tx = (int)((s->player.x - EM_OX) / EM_TILE), ty = (int)((s->player.y - EM_OY) / EM_TILE);
    if (tx == EM_W - 2 && ty == EM_H - 2)
        s->won = true;
}

bool echo_maze_is_over(const echo_maze_State *s) {
    return s->won;
}

/* Faster and fewer pings = more points. */
int echo_maze_score(const echo_maze_State *s) {
    if (!s->won)
        return 0;
    return arc_maxi(100, 3000 - (int)(s->time * 8.0f) - s->pings * 60);
}
