#include <string.h>

#include "laser_mirrors_logic.h"

/* ---- the levels ----------------------------------------------------------------------------------------
 * Legend of the rows: '.' empty   '#' wall   '/' '\' fixed mirrors   'r' 'R' rotatable mirrors (they start as
 * '/' and '\')   'S' splitter   'T' target   '>' 'v' '<' '^' the laser emitter and the way it shoots.
 * Every row is LM_W characters and there are LM_H rows. 'par' is the fewest clicks that solve the level;
 * tests/test_laser_mirrors.c checks it, and that each level has exactly one solution. */
typedef struct LmLevel {
    const char *name;
    const char *hint;
    int par;
    const char *rows[LM_H];
} LmLevel;

static const LmLevel k_levels[LM_LEVEL_COUNT] = {
    {"FIRST LIGHT",
     "Click a yellow mirror to flip it.",
     1,
     {"..............", "..............", "....#.........", "..............", ">...r.........",
      "..............", "....T.........", ".............."}},
    {"ZIG-ZAG",
     "Follow the beam. Not every mirror needs a click.",
     2,
     {">...r.........", "..............", "..............", ".........T....", "..............",
      "....R....R....", "..............", ".............."}},
    {"FORK",
     "A splitter lets light through AND sends a copy sideways.",
     2,
     {"..............", ".T..r.........", "..............", "..............", ">...S....r....",
      "..............", ".........T....", ".............."}},
    {"UPSTREAM",
     "A splitter's copy leaves like off a / mirror: feed it from another side.",
     3,
     {".....vT.......", "......S.....T.", "..............", "R....R........", "..............",
      "R.....R.......", "..............", ".............."}},
    {"BANK SHOT",
     "Cyan mirrors are fixed. They will not turn, so use them.",
     3,
     {".....T...R..r.", ".........T....", ".........r..R.", ".......\\......", ">....S.../....",
      "..............", "..............", ".............."}},
    {"REVERSE",
     "The laser can shoot from any wall.",
     5,
     {"..............", "........R...r.", "..............", "........S....r", "............TT",
      "..........R..<", "..............", "........r.r..."}},
    {"CROSSFIRE",
     "Beams pass through each other. Only mirrors turn them.",
     4,
     {"..T.v.........", "..r...r.......", "T...S.........", "..............", "r.....S..r....",
      "..............", "..............", "r...r....T...."}},
    {"DAISY CHAIN",
     "One splitter can feed another.",
     5,
     {".......r....r.", "..............", "..........r.R.", "T.S....S...T.r", "..............",
      "..............", ".......r.....R", "..T.......^..."}},
    {"HALL OF MIRRORS",
     "Think backwards: trace the light from a target to the laser.",
     5,
     {"..R..R.....T..", "....../....S.T", "..............", "...T....R.....", "...........R.<",
      "..R...S.R.....", ".....r.....R..", ".............."}},
    {"TRIPLE SPLIT",
     "Splitters turn one beam into many. Mind where each copy goes.",
     7,
     {".T...R..S..r..", ".....T........", ".S...r........", "..............", ".....r.....R..",
      "....R...R.....", "........T.....", ".^..R...S....T"}},
    {"THE COMB",
     "Take it one splitter at a time.",
     8,
     {".T.T.T.r..R...", "..............", ".S.S...S.r....", "..........T...", ".....r.....r..",
      "....r..r......", ".........r.R..", ".r..R..^......"}},
    {"FINAL EXAM",
     "Everything you have learned. Good luck.",
     8,
     {"....T..R..Rv..", "..............", "....r.rR...S..", "...R.r........", "........rT....",
      ".....r....r...", "..............", "...r..S.S.Tr.T"}},
};

static const int k_dx[4] = {1, 0, -1, 0};
static const int k_dy[4] = {0, 1, 0, -1};

void laser_mirrors_load_rows(laser_mirrors_State *s, const char *const rows[LM_H]) {
    memset(s->cell, 0, sizeof(s->cell));
    memset(s->rotatable, 0, sizeof(s->rotatable));
    memset(s->beam, 0, sizeof(s->beam));
    memset(s->lit, 0, sizeof(s->lit));
    s->emitter_x = s->emitter_y = s->emitter_dir = 0;
    s->targets = s->targets_lit = 0;
    s->solved = false;
    for (int y = 0; y < LM_H; y++) {
        if (rows[y] == NULL) /* a missing row is all empty */
            continue;
        for (int x = 0; x < LM_W && rows[y][x] != '\0'; x++) { /* a short row is padded with empty tiles */
            switch (rows[y][x]) {
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
            case '>':
            case 'v':
            case '<':
            case '^': /* the emitter's tile stays empty; it only remembers where and which way to shoot */
                s->emitter_x = x;
                s->emitter_y = y;
                s->emitter_dir =
                    rows[y][x] == '>' ? 0 : (rows[y][x] == 'v' ? 1 : (rows[y][x] == '<' ? 2 : 3));
                break;
            default:
                break;
            }
        }
    }
    laser_mirrors_recompute(s);
}

void laser_mirrors_load_level(laser_mirrors_State *s, int level) {
    level = arc_clampi(level, 0, LM_LEVEL_COUNT - 1);
    laser_mirrors_load_rows(s, k_levels[level].rows);
    s->level = level;
    s->par = k_levels[level].par;
    s->moves = 0;
    s->hold = 0;
}

const char *laser_mirrors_level_name(int level) {
    return k_levels[arc_clampi(level, 0, LM_LEVEL_COUNT - 1)].name;
}

const char *laser_mirrors_level_hint(int level) {
    return k_levels[arc_clampi(level, 0, LM_LEVEL_COUNT - 1)].hint;
}

int laser_mirrors_level_par(int level) {
    return k_levels[arc_clampi(level, 0, LM_LEVEL_COUNT - 1)].par;
}

void laser_mirrors_init(laser_mirrors_State *s, unsigned seed) {
    (void)seed; /* the campaign always starts on level 0 */
    memset(s, 0, sizeof(*s));
    laser_mirrors_load_level(s, 0);
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

int laser_mirrors_level_points(const laser_mirrors_State *s) {
    if (!s->solved)
        return 0;
    return arc_maxi(100, 1000 - 60 * arc_maxi(0, s->moves - s->par));
}

/* The R key: put every rotatable mirror back to how the level started. The clicks already spent stay spent,
 * so resetting is a shortcut for undoing, not a way to wipe the move counter. */
static void restart_level(laser_mirrors_State *s) {
    int moves = s->moves;
    laser_mirrors_load_level(s, s->level);
    s->moves = moves;
}

/* The player is done with the cleared level: load the next one, or end the run after the last. */
static void next_level(laser_mirrors_State *s) {
    if (s->level + 1 >= LM_LEVEL_COUNT) {
        s->finished = true;
        return;
    }
    laser_mirrors_load_level(s, s->level + 1);
}

void laser_mirrors_step(laser_mirrors_State *s, const ArcadeInput *in, float dt) {
    (void)dt; /* nothing moves on its own: the board only changes when the player clicks */
    if (s->finished)
        return;
    if (s->solved) { /* cleared: bank is already done, wait for a click or SPACE */
        s->hold++;
        if (s->hold >= LM_CLEAR_HOLD && (in->fire_pressed || in->action_pressed))
            next_level(s);
        return;
    }
    if (in->typed == 'r' || in->typed == 'R') {
        restart_level(s);
        return;
    }
    if (!in->fire_pressed)
        return;
    if (in->aim_x < LM_OX || in->aim_y < LM_OY)
        return;
    int tx = (int)((in->aim_x - LM_OX) / LM_TILE), ty = (int)((in->aim_y - LM_OY) / LM_TILE);
    if (!laser_mirrors_toggle(s, tx, ty))
        return;
    s->moves++;
    if (s->solved) { /* bank the points the moment the last target lights up */
        s->level_points = laser_mirrors_level_points(s);
        s->total += s->level_points;
        s->hold = 0;
    }
}

bool laser_mirrors_is_over(const laser_mirrors_State *s) {
    return s->finished;
}

int laser_mirrors_score(const laser_mirrors_State *s) {
    return s->total;
}

/* Tries every combination of the rotatable mirrors. Bit i of 'mask' says: mirror i is flipped compared with
 * the board we were given. The combination with the fewest set bits is the cheapest solution. */
LmSolve laser_mirrors_solve(const laser_mirrors_State *s) {
    LmSolve r;
    int px[LM_MAX_ROT], py[LM_MAX_ROT], k = 0;
    r.rotatable = 0;
    r.solutions = 0;
    r.min_moves = -1;
    r.flips = 0;
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++)
            if (s->rotatable[y][x]) {
                if (k < LM_MAX_ROT) {
                    px[k] = x;
                    py[k] = y;
                }
                k++;
            }
    r.rotatable = k;
    if (k > LM_MAX_ROT) {
        r.solutions = -1;
        return r;
    }
    laser_mirrors_State t = *s;
    for (uint32_t mask = 0; mask < (1u << k); mask++) {
        int flipped = 0;
        for (int i = 0; i < k; i++) {
            uint8_t start = s->cell[py[i]][px[i]];
            uint8_t other = (start == LM_SLASH) ? LM_BACK : LM_SLASH;
            bool f = ((mask >> i) & 1u) != 0;
            t.cell[py[i]][px[i]] = f ? other : start;
            flipped += f ? 1 : 0;
        }
        laser_mirrors_recompute(&t);
        if (!t.solved)
            continue;
        r.solutions++;
        if (r.min_moves < 0 || flipped < r.min_moves) {
            r.min_moves = flipped;
            r.flips = mask;
        }
    }
    return r;
}

bool laser_mirrors_can_solve(const laser_mirrors_State *s) {
    return laser_mirrors_solve(s).solutions > 0;
}
