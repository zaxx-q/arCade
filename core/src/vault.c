#include "arcade/vault.h"

void vault_build(Sandbox *sb, Vault *v, int x, int y, int w, int h) {
    v->x = x;
    v->y = y;
    v->w = w;
    v->h = h;
    v->open = false;
    for (int cy = y; cy < y + h; cy++) /* clear the interior */
        for (int cx = x; cx < x + w; cx++)
            sandbox_set(sb, cx, cy, EL_EMPTY);
    for (int cy = y; cy <= y + h; cy++) { /* side walls, down to the floor row */
        sandbox_set(sb, x - 1, cy, EL_STONE);
        sandbox_set(sb, x + w, cy, EL_STONE);
    }
    for (int cx = x - 1; cx <= x + w; cx++) /* floor */
        sandbox_set(sb, cx, y + h, EL_STONE);
}

bool vault_contains(const Vault *v, int cx, int cy) {
    return cx >= v->x && cx < v->x + v->w && cy >= v->y && cy < v->y + v->h;
}

float vault_progress(const Sandbox *sb, const Vault *v) {
    int capacity = v->w * v->h;
    if (capacity <= 0)
        return 0.0f;
    int filled = sandbox_count_in(sb, v->x, v->y, v->w, v->h, EL_SAND) +
                 sandbox_count_in(sb, v->x, v->y, v->w, v->h, EL_WATER);
    return arc_clampf((float)filled / ((float)capacity * VAULT_OPEN_SHARE), 0.0f, 1.0f);
}

void vault_open_now(Sandbox *sb, Vault *v) {
    v->open = true;
    int mid = v->x + v->w / 2;
    sandbox_set(sb, mid - 1, v->y + v->h, EL_EMPTY); /* the plug: two floor cells in the middle */
    sandbox_set(sb, mid, v->y + v->h, EL_EMPTY);
}

bool vault_update(Sandbox *sb, Vault *v) {
    if (v->open || vault_progress(sb, v) < 1.0f)
        return false;
    vault_open_now(sb, v);
    return true;
}
