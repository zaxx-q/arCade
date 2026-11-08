#include "arcade/sandbox.h"

#include <string.h>

static const ElementDef k_elements[EL_COUNT] = {
    [EL_EMPTY] = {"Eraser", 8, 10, 22, 0, MOVE_NONE},
    [EL_SAND] = {"Sand", 232, 196, 98, 3, MOVE_POWDER},
    [EL_WATER] = {"Water", 64, 140, 255, 2, MOVE_LIQUID},
    [EL_STONE] = {"Stone", 130, 130, 150, 9, MOVE_NONE},
};

const ElementDef *sandbox_element(ElementId id) {
    if ((int)id < 0 || id >= EL_COUNT)
        return &k_elements[EL_EMPTY];
    return &k_elements[id];
}

void sandbox_init(Sandbox *sb, int w, int h, unsigned seed) {
    sb->w = arc_clampi(w, 1, SANDBOX_MAX_W);
    sb->h = arc_clampi(h, 1, SANDBOX_MAX_H);
    sb->tick = 0;
    rng_seed(&sb->rng, seed);
    sandbox_clear(sb);
}

void sandbox_clear(Sandbox *sb) {
    memset(sb->cells, EL_EMPTY, sizeof(sb->cells));
    memset(sb->moved, 0, sizeof(sb->moved));
}

static bool in_bounds(const Sandbox *sb, int x, int y) {
    return x >= 0 && y >= 0 && x < sb->w && y < sb->h;
}

ElementId sandbox_get(const Sandbox *sb, int x, int y) {
    if (!in_bounds(sb, x, y))
        return EL_STONE;
    return (ElementId)sb->cells[y * sb->w + x];
}

void sandbox_set(Sandbox *sb, int x, int y, ElementId id) {
    if (in_bounds(sb, x, y) && (int)id >= 0 && id < EL_COUNT)
        sb->cells[y * sb->w + x] = (uint8_t)id;
}

void sandbox_paint(Sandbox *sb, int cx, int cy, int radius, ElementId id) {
    if (radius < 0)
        radius = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius + radius)
                continue; /* round-ish brush */
            int x = cx + dx, y = cy + dy;
            if (!in_bounds(sb, x, y))
                continue;
            if (id == EL_EMPTY || sandbox_get(sb, x, y) == EL_EMPTY)
                sandbox_set(sb, x, y, id);
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

/* Can `mover` move into (x, y)?  Empty cells always; lighter liquids can be pushed out of the way. */
static bool can_enter(const Sandbox *sb, int x, int y, ElementId mover) {
    if (!in_bounds(sb, x, y))
        return false;
    ElementId t = sandbox_get(sb, x, y);
    if (t == EL_EMPTY)
        return true;
    const ElementDef *td = sandbox_element(t), *md = sandbox_element(mover);
    return td->kind == MOVE_LIQUID && td->density < md->density;
}

static void move_cell(Sandbox *sb, int x, int y, int nx, int ny) {
    int a = y * sb->w + x, b = ny * sb->w + nx;
    uint8_t t = sb->cells[b];
    sb->cells[b] = sb->cells[a];
    sb->cells[a] = t;
    sb->moved[a] = 1;
    sb->moved[b] = 1;
}

void sandbox_step(Sandbox *sb) {
    memset(sb->moved, 0, (size_t)(sb->w * sb->h));
    sb->tick++;
    for (int y = sb->h - 1; y >= 0; y--) {
        bool left_to_right = ((sb->tick + (unsigned)y) & 1u) == 0u; /* alternate to avoid drift */
        for (int i = 0; i < sb->w; i++) {
            int x = left_to_right ? i : sb->w - 1 - i;
            if (sb->moved[y * sb->w + x])
                continue;
            ElementId e = (ElementId)sb->cells[y * sb->w + x];
            MoveKind kind = sandbox_element(e)->kind;
            if (kind == MOVE_NONE)
                continue;

            int dir = (rng_u32(&sb->rng) & 1u) ? 1 : -1;
            if (can_enter(sb, x, y + 1, e)) {
                move_cell(sb, x, y, x, y + 1);
            } else if (can_enter(sb, x + dir, y + 1, e) && can_enter(sb, x + dir, y, e)) {
                move_cell(sb, x, y, x + dir, y + 1);
            } else if (can_enter(sb, x - dir, y + 1, e) && can_enter(sb, x - dir, y, e)) {
                move_cell(sb, x, y, x - dir, y + 1);
            } else if (kind == MOVE_LIQUID) {
                if (in_bounds(sb, x + dir, y) && sandbox_get(sb, x + dir, y) == EL_EMPTY)
                    move_cell(sb, x, y, x + dir, y);
                else if (in_bounds(sb, x - dir, y) && sandbox_get(sb, x - dir, y) == EL_EMPTY)
                    move_cell(sb, x, y, x - dir, y);
            }
        }
    }
}
