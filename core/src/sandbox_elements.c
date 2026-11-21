/* The element table. One row per element; the columns are listed in the header comment below.
 * Behaviour that cannot be described by a number lives in sandbox_react.c. */
#include "arcade/sandbox.h"

/* Columns of each row, in order:
 *   name, r, g, b, density, kind, flags, speed, spread, slip, grip, ignite, shade, spray
 *
 * density : heavier things sink through lighter fluids (powders float on denser liquids too)
 * speed   : powders/liquids = cells fallen per tick in open air; gases and fire = chance (/256) to rise
 * spread  : liquids look this many cells sideways for somewhere to flow to
 * slip    : liquids move with this chance (/256) each tick; low = thick and slow (lava)
 * grip    : powders: 0 slides down any edge, 1 needs a 2 cell drop (steeper piles), ...
 * ignite  : chance (/256) per tick to catch fire next to something hot; 0 = does not burn
 * shade   : brightness noise per cell
 * spray   : brush density (/256) */
static const ElementDef k_elements[EL_COUNT] = {
    [EL_EMPTY] = {"Eraser", 8, 10, 22, 0, MOVE_NONE, 0, 0, 0, 0, 0, 0, 0, 255},

    /* the original four, with the same colours as before */
    [EL_SAND] = {"Sand", 232, 196, 98, 50, MOVE_POWDER, 0, 4, 0, 0, 0, 0, 14, 255},
    [EL_WATER] = {"Water", 64, 140, 255, 20, MOVE_LIQUID, 0, 4, 96, 255, 0, 0, 8, 255},
    [EL_STONE] = {"Stone", 130, 130, 150, 100, MOVE_NONE, 0, 0, 0, 0, 0, 0, 12, 255},

    /* powders */
    [EL_SOIL] = {"Soil", 104, 72, 54, 46, MOVE_POWDER, 0, 4, 0, 0, 1, 0, 14, 255},
    [EL_SNOW] = {"Snow", 238, 244, 255, 5, MOVE_POWDER, 0, 1, 0, 0, 0, 0, 8, 200},
    [EL_GUNPOWDER] = {"Gunpowder", 70, 66, 86, 44, MOVE_POWDER, 0, 4, 0, 0, 0, 255, 12, 255},
    [EL_SEED] = {"Seed", 205, 214, 120, 6, MOVE_POWDER, 0, 3, 0, 0, 0, 60, 10, 90},
    [EL_ASH] = {"Ash", 176, 168, 160, 14, MOVE_POWDER, 0, 2, 0, 0, 0, 0, 12, 255},
    [EL_THERMITE] = {"Thermite", 176, 72, 56, 55, MOVE_POWDER, 0, 4, 0, 0, 0, 60, 14, 255},

    /* liquids */
    [EL_OIL] = {"Oil", 78, 52, 66, 10, MOVE_LIQUID, 0, 4, 32, 255, 0, 90, 6, 255},
    [EL_ACID] = {"Acid", 146, 255, 70, 22, MOVE_LIQUID, EF_ACIDPROOF, 4, 32, 255, 0, 0, 10, 255},
    [EL_LAVA] = {"Lava", 255, 98, 24, 60, MOVE_LIQUID, EF_HOT | EF_ACIDPROOF, 2, 3, 70, 0, 0, 24, 255},
    [EL_MERCURY] = {"Mercury", 178, 190, 204, 136, MOVE_LIQUID, 0, 4, 24, 255, 0, 0, 20, 255},
    [EL_NITROGEN] = {"Nitrogen", 110, 235, 245, 8, MOVE_LIQUID, 0, 4, 96, 255, 0, 0, 8, 255},

    /* gases */
    [EL_STEAM] = {"Steam", 206, 214, 228, 2, MOVE_GAS, 0, 180, 0, 0, 0, 0, 10, 110},
    [EL_METHANE] = {"Methane", 176, 120, 214, 1, MOVE_GAS, 0, 200, 0, 0, 0, 255, 8, 110},
    [EL_SMOKE] = {"Smoke", 84, 84, 96, 3, MOVE_GAS, 0, 170, 0, 0, 0, 0, 8, 110},

    /* solids */
    [EL_WOOD] = {"Wood", 160, 112, 64, 100, MOVE_NONE, 0, 0, 0, 0, 0, 40, 10, 255},
    [EL_ICE] = {"Ice", 156, 214, 244, 100, MOVE_NONE, 0, 0, 0, 0, 0, 0, 10, 255},
    [EL_GLASS] = {"Glass", 130, 220, 214, 100, MOVE_NONE, EF_ACIDPROOF, 0, 0, 0, 0, 0, 6, 255},
    [EL_WALL] = {"Wall", 70, 78, 110, 255, MOVE_NONE, EF_ACIDPROOF, 0, 0, 0, 0, 0, 4, 255},
    [EL_PLANT] = {"Plant", 62, 172, 90, 100, MOVE_NONE, 0, 0, 0, 0, 0, 90, 16, 255},
    [EL_FUSE] = {"Fuse", 206, 186, 150, 100, MOVE_NONE, 0, 0, 0, 0, 0, 255, 6, 255},
    [EL_TNT] = {"TNT", 214, 62, 66, 100, MOVE_NONE, 0, 0, 0, 0, 0, 140, 6, 255},

    /* energetic and alive */
    [EL_FIRE] = {"Fire", 255, 170, 40, 1, MOVE_FIRE, EF_HOT, 160, 0, 0, 0, 0, 0, 100},
    [EL_URANIUM] = {"Uranium", 96, 230, 110, 190, MOVE_NONE, 0, 0, 0, 0, 0, 0, 10, 255},
    [EL_TERMITE] = {"Termite", 240, 226, 184, 52, MOVE_CRAWL, 0, 0, 0, 0, 0, 70, 8, 24},
    [EL_LIFE] = {"Life", 232, 80, 200, 100, MOVE_NONE, 0, 0, 0, 0, 0, 0, 0, 255},

    /* hidden */
    [EL_GAMMA] = {"Gamma", 190, 255, 214, 0, MOVE_RAY, EF_HIDDEN, 0, 0, 0, 0, 0, 0, 0},
    [EL_MOLTEN_URANIUM] = {"Molten Uranium", 190, 255, 90, 150, MOVE_LIQUID, EF_HIDDEN | EF_HOT, 2, 3, 50, 0,
                           0, 20, 0},
};

const ElementDef *sandbox_element(ElementId id) {
    if ((int)id < 0 || id >= EL_COUNT)
        return &k_elements[EL_EMPTY];
    return &k_elements[id];
}

int sandbox_palette_count(void) {
    int n = 0;
    for (int i = 0; i < EL_COUNT; i++)
        if (!(k_elements[i].flags & EF_HIDDEN))
            n++;
    return n;
}

ElementId sandbox_palette_id(int index) {
    int n = 0;
    for (int i = 0; i < EL_COUNT; i++) {
        if (k_elements[i].flags & EF_HIDDEN)
            continue;
        if (n == index)
            return (ElementId)i;
        n++;
    }
    return EL_EMPTY;
}

int sandbox_palette_index(ElementId id) {
    if ((int)id < 0 || id >= EL_COUNT || (k_elements[id].flags & EF_HIDDEN))
        return -1;
    int n = 0;
    for (int i = 0; i < (int)id; i++)
        if (!(k_elements[i].flags & EF_HIDDEN))
            n++;
    return n;
}
