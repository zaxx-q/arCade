#include <string.h>

#include "laser_mirrors_logic.h"

/* TODO(Member 3): turn this single level into an array of levels, validate each with
 * laser_mirrors_can_solve() (GAME.md step 1). */
/* Built-in level. '.' empty  '#' wall  '/' '\' fixed mirrors  'r' 'R' rotatable mirrors (start '/' and '\')
 *                 'S' splitter  'T' target  'E' laser emitter (shoots right) */
static const char *const k_level[LM_H] = {
    "..............", ".........T....", "..............", "....R......T..",
    "E...S....R....", "..............", "..............", "..............",
};

static const int k_dx[4] = {1, 0, -1, 0};
static const int k_dy[4] = {0, 1, 0, -1};

void laser_mirrors_init(laser_mirrors_State *s, unsigned seed) {
    (void)seed; /* TODO(Member 3): pick / generate a level from the seed */
    memset(s, 0, sizeof(*s));
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++) {
            switch (k_level[y][x]) {
            case '#':
                s->cell[y][x] = LM_WALL;
                break;
            case '/':
                s->cell[y][x] = LM_SLASH;
                break;
            case '\\':
                s->cell[y][x] = LM_BACK;
                break;
            case 'r':
                s->cell[y][x] = LM_SLASH;
                s->rotatable[y][x] = true;
                break;
            case 'R':
                s->cell[y][x] = LM_BACK;
                s->rotatable[y][x] = true;
                break;
            case 'S':
                s->cell[y][x] = LM_SPLIT;
                break;
            case 'T':
                s->cell[y][x] = LM_TARGET;
                s->targets++;
                break;
            case 'E':
                s->emitter_x = x;
                s->emitter_y = y;
                s->emitter_dir = 0;
                break;
            default:
                break;
            }
        }
    laser_mirrors_recompute(s);
}

/* Follow the beam from (x, y) in direction d. Each (tile, direction) pair is visited at most once,
 * so loops between mirrors cannot hang and the recursion depth is bounded. */
static void trace(laser_mirrors_State *s, int x, int y, int d) {
    while (x >= 0 && x < LM_W && y >= 0 && y < LM_H) {
        if (s->cell[y][x] == LM_WALL)
            return;
        if (s->beam[y][x] & (1u << d))
            return;
        s->beam[y][x] = (uint8_t)(s->beam[y][x] | (1u << d));
        switch (s->cell[y][x]) {
        case LM_TARGET:
            s->lit[y][x] = true;
            return;
        case LM_SLASH:
            d = 3 - d;
            break; /* '/'  : right<->up, left<->down */
        case LM_BACK:
            d = d ^ 1;
            break;       /* '\\' : right<->down, left<->up */
        case LM_SPLIT: { /* the beam continues AND a second beam leaves like off a '/' */
            int b = 3 - d;
            trace(s, x + k_dx[b], y + k_dy[b], b); /* recursion */
            break;
        }
        default:
            break;
        }
        x += k_dx[d];
        y += k_dy[d];
    }
}

void laser_mirrors_recompute(laser_mirrors_State *s) {
    memset(s->beam, 0, sizeof(s->beam));
    memset(s->lit, 0, sizeof(s->lit));
    trace(s, s->emitter_x, s->emitter_y, s->emitter_dir);
    s->targets_lit = 0;
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++)
            if (s->lit[y][x])
                s->targets_lit++;
    s->solved = s->targets > 0 && s->targets_lit == s->targets;
}

bool laser_mirrors_toggle(laser_mirrors_State *s, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= LM_W || ty >= LM_H || !s->rotatable[ty][tx])
        return false;
    s->cell[ty][tx] = (s->cell[ty][tx] == LM_SLASH) ? LM_BACK : LM_SLASH;
    laser_mirrors_recompute(s);
    return true;
}

void laser_mirrors_step(laser_mirrors_State *s, const ArcadeInput *in, float dt) {
    (void)dt;
    if (s->solved)
        return;
    if (!in->fire_pressed)
        return;
    int tx = (int)((in->aim_x - LM_OX) / LM_TILE), ty = (int)((in->aim_y - LM_OY) / LM_TILE);
    if (in->aim_x < LM_OX || in->aim_y < LM_OY)
        return;
    if (laser_mirrors_toggle(s, tx, ty))
        s->moves++;
}

bool laser_mirrors_is_over(const laser_mirrors_State *s) {
    return s->solved;
}

int laser_mirrors_score(const laser_mirrors_State *s) {
    if (!s->solved)
        return 0;
    return arc_maxi(100, 1000 - 60 * arc_maxi(0, s->moves - LM_PAR_MOVES));
}

bool laser_mirrors_can_solve(const laser_mirrors_State *s) {
    int pos_x[16], pos_y[16], k = 0;
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++)
            if (s->rotatable[y][x] && k < 16) {
                pos_x[k] = x;
                pos_y[k++] = y;
            }
    laser_mirrors_State t = *s;
    for (unsigned mask = 0; mask < (1u << k); mask++) { /* every combination of the k mirrors */
        for (int i = 0; i < k; i++)
            t.cell[pos_y[i]][pos_x[i]] = ((mask >> i) & 1u) ? LM_BACK : LM_SLASH;
        laser_mirrors_recompute(&t);
        if (t.solved)
            return true;
    }
    return false;
}
