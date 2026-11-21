#include "arcade/sandbox.h"

#include <string.h>

#include "sandbox_internal.h"

void sandbox_init(Sandbox *sb, int w, int h, unsigned seed) {
    sb->w = arc_clampi(w, 1, SANDBOX_MAX_W);
    sb->h = arc_clampi(h, 1, SANDBOX_MAX_H);
    sb->tick = 0;
    sb->heat_view = false;
    rng_seed(&sb->rng, seed);
    sandbox_clear(sb);
}

void sandbox_clear(Sandbox *sb) {
    memset(sb->cells, EL_EMPTY, sizeof(sb->cells));
    memset(sb->aux, 0, sizeof(sb->aux));
    memset(sb->moved, 0, sizeof(sb->moved));
    sandbox_heat_clear(sb);
    sb->shock.active = false;
    sb->flash = 0;
    sb->blast_budget = SANDBOX_BLASTS_PER_TICK;
}

ElementId sandbox_get(const Sandbox *sb, int x, int y) {
    return sandbox_at(sb, x, y);
}

void sandbox_set(Sandbox *sb, int x, int y, ElementId id) {
    if (!sandbox_inside(sb, x, y) || (int)id < 0 || id >= EL_COUNT)
        return;
    int i = y * sb->w + x;
    sb->cells[i] = (uint8_t)id;
    sb->aux[i] = 0; /* a freshly placed cell starts with a clean memory */
    sb->moved[i] = 0;
    sb->temp[i] = SANDBOX_AMBIENT; /* ... and with the temperature its element is made at */
    sandbox_heat_on_put(sb, i, id);
}

void sandbox_paint(Sandbox *sb, int cx, int cy, int radius, ElementId id) {
    if (radius < 0)
        radius = 0;
    int spray = sandbox_element(id)->spray;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius + radius)
                continue; /* round-ish brush */
            int x = cx + dx, y = cy + dy;
            if (!sandbox_inside(sb, x, y))
                continue;
            if (id == EL_EMPTY) {
                sandbox_set(sb, x, y, EL_EMPTY);
            } else if (sandbox_get(sb, x, y) == EL_EMPTY && (spray >= 255 || sandbox_chance(sb, spray))) {
                sandbox_set(sb, x, y, id);
            }
        }
    }
}

int sandbox_count(const Sandbox *sb, ElementId id) {
    int n = 0;
    for (int i = 0; i < sb->w * sb->h; i++)
        if (sb->cells[i] == (uint8_t)id)
            n++;
    return n;
}

int sandbox_count_in(const Sandbox *sb, int x, int y, int w, int h, ElementId id) {
    int x0 = arc_maxi(x, 0), y0 = arc_maxi(y, 0);
    int x1 = arc_mini(x + w, sb->w), y1 = arc_mini(y + h, sb->h); /* exclusive ends */
    int n = 0;
    for (int cy = y0; cy < y1; cy++)
        for (int cx = x0; cx < x1; cx++)
            if (sb->cells[cy * sb->w + cx] == (uint8_t)id)
                n++;
    return n;
}

/* ---- helpers shared by the engine files (declared in sandbox_internal.h) ---- */

int sandbox_rand(Sandbox *sb, int n) {
    return n <= 1 ? 0 : (int)(rng_u32(&sb->rng) % (uint32_t)n);
}

bool sandbox_chance(Sandbox *sb, int n) {
    return (int)(rng_u32(&sb->rng) & 255u) < n;
}

void sandbox_swap(Sandbox *sb, int a, int b) {
    uint8_t t = sb->cells[a];
    sb->cells[a] = sb->cells[b];
    sb->cells[b] = t;
    t = sb->aux[a];
    sb->aux[a] = sb->aux[b];
    sb->aux[b] = t;
    t = sb->moved[a]; /* the "moved last tick" bit travels with the cell it describes */
    sb->moved[a] = sb->moved[b];
    sb->moved[b] = t;
    sb->moved[a] |= MOVED_NOW;
    sb->moved[b] |= MOVED_NOW;
    int16_t temp = sb->temp[a]; /* heat moves with the cell */
    sb->temp[a] = sb->temp[b];
    sb->temp[b] = temp;
    if (sb->temp[a] != SANDBOX_AMBIENT || sb->temp[b] != SANDBOX_AMBIENT) {
        sandbox_heat_mark(sb, a / sb->w);
        sandbox_heat_mark(sb, b / sb->w);
    }
}

void sandbox_put(Sandbox *sb, int x, int y, ElementId id, uint8_t aux) {
    if (!sandbox_inside(sb, x, y))
        return;
    int i = y * sb->w + x;
    sb->cells[i] = (uint8_t)id;
    sb->aux[i] = id == EL_EMPTY ? 0 : aux;
    sb->moved[i] = MOVED_NOW; /* a new cell does not act in the tick it was made */
    sandbox_heat_on_put(sb, i, id);
}

void sandbox_step(Sandbox *sb) {
    int total = sb->w * sb->h;
    for (int i = 0; i < total; i++) /* "moved this tick" becomes "moved last tick"; EXCITED survives */
        sb->moved[i] = (uint8_t)(((sb->moved[i] & MOVED_NOW) << 1) | (sb->moved[i] & EXCITED));
    sb->tick++;
    sandbox_blast_step(sb); /* a travelling shock wave pushes things before anything else moves */

    /* Bottom row first, so a falling cell never gets moved twice in one tick. */
    for (int y = sb->h - 1; y >= 0; y--) {
        bool left_to_right = ((sb->tick + (unsigned)y) & 1u) == 0u; /* alternate to avoid drift */
        for (int k = 0; k < sb->w; k++) {
            int x = left_to_right ? k : sb->w - 1 - k;
            int i = y * sb->w + x;
            if ((sb->moved[i] & MOVED_NOW) || sb->cells[i] == EL_EMPTY)
                continue;
            sandbox_react(sb, x, y); /* chemistry first: the cell may burn, melt, grow... */
            if (sb->moved[i] & MOVED_NOW)
                continue; /* it changed into something else: that is its turn for this tick */
            sandbox_move(sb, x, y);
        }
    }
    sandbox_heat_step(sb);
    if (sb->tick % LIFE_PERIOD == 0)
        sandbox_life_step(sb);
}
