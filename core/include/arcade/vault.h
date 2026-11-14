/* The hub's "vault": a stone basin built into the falling-sand backdrop. Fill it up (sand, water, or both)
 * and it opens its drain, which is how the hub unlocks the archive of deferred games (see docs/DEFERRED.md).
 * Pure C on top of core/sandbox, so the rules are unit-tested without a window. */
#ifndef ARCADE_VAULT_H
#define ARCADE_VAULT_H

#include "arcade/sandbox.h"

#define VAULT_OPEN_SHARE 0.90f /* share of the interior that must be filled for the vault to open */

typedef struct Vault {
    int x, y, w, h; /* interior rectangle in sandbox cells; walls and floor are built around it */
    bool open;      /* latched: once it has been full enough it stays open */
} Vault;

/* Clears the interior and builds the stone walls + floor around it. The floor has a 2-cell plug in the middle
 * that vault_open_now() removes. The vault starts closed. */
void vault_build(Sandbox *sb, Vault *v, int x, int y, int w, int h);

/* Is the sandbox cell (cx, cy) inside the interior? */
bool vault_contains(const Vault *v, int cx, int cy);

/* 0..1: how close the vault is to opening (1 = full enough). Stays 1 only while it is still full. */
float vault_progress(const Sandbox *sb, const Vault *v);

/* Call once per simulation tick. Opens the vault when it is full enough; returns true only on that tick. */
bool vault_update(Sandbox *sb, Vault *v);

/* Opens the vault right away (removes the floor plug so the contents drain out). Used by `--all`. */
void vault_open_now(Sandbox *sb, Vault *v);

#endif
