/* The colour of one cell. Kept out of the renderers so every front-end (the game, the hub backdrop, tests)
 * shows the same picture, and so adding an element never means touching drawing code.
 * Everything here is a pure function of the grid and the tick: no random state is consumed. */
#include "sandbox_internal.h"

#define BURN_BIT 0x80u /* same meaning as in sandbox_react.c */

static uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B1u ^ b * 0x85EBCA6Bu ^ c * 0xC2B2AE35u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

typedef struct Rgb {
    int r, g, b;
} Rgb;

static Rgb rgb(int r, int g, int b) {
    Rgb c = {r, g, b};
    return c;
}

/* Blend a towards b; t is 0..256. */
static Rgb mix(Rgb a, Rgb b, int t) {
    t = arc_clampi(t, 0, 256);
    return rgb(a.r + (b.r - a.r) * t / 256, a.g + (b.g - a.g) * t / 256, a.b + (b.b - a.b) * t / 256);
}

static Rgb add(Rgb a, int v) {
    return rgb(a.r + v, a.g + v, a.b + v);
}

static uint32_t pack(Rgb c) {
    return ((uint32_t)arc_clampi(c.r, 0, 255) << 16) | ((uint32_t)arc_clampi(c.g, 0, 255) << 8) |
           (uint32_t)arc_clampi(c.b, 0, 255);
}

/* Per-cell brightness noise in -amount..+amount, fixed for a given position. */
static int grain(uint32_t h, int amount) {
    return ((int)(h & 255u) - 128) * amount / 128;
}

uint32_t sandbox_pixel(const Sandbox *sb, int x, int y) {
    int i = y * sb->w + x;
    if (!sandbox_inside(sb, x, y))
        return 0;
    ElementId id = (ElementId)sb->cells[i];
    const ElementDef *d = sandbox_element(id);
    Rgb bg = rgb(sandbox_element(EL_EMPTY)->r, sandbox_element(EL_EMPTY)->g, sandbox_element(EL_EMPTY)->b);
    if (id == EL_EMPTY)
        return pack(bg);

    Rgb base = rgb(d->r, d->g, d->b);
    uint32_t cell = hash3((uint32_t)x, (uint32_t)y, 0u);                 /* fixed per position */
    uint32_t t1 = hash3((uint32_t)x, (uint32_t)y, sb->tick);             /* new every tick */
    uint32_t t4 = hash3((uint32_t)x, (uint32_t)y, 1u + (sb->tick >> 2)); /* changes every 4 ticks */
    int aux = sb->aux[i];
    bool open_above = sandbox_at(sb, x, y - 1) == EL_EMPTY;
    Rgb c = add(base, grain(cell, d->shade));

    switch (id) {
    case EL_WATER:
        if (open_above) /* the surface catches the light */
            c = add(c, 30 + ((t4 & 31u) == 0u ? 60 : 0));
        break;
    case EL_MERCURY:
        if (open_above)
            c = add(c, 34);
        break;
    case EL_ACID:
        if ((t4 & 15u) == 0u)
            c = add(c, 60); /* bubbles */
        break;
    case EL_NITROGEN:
        c = add(c, (t4 & 7u) == 0u ? 40 : 0);
        break;
    case EL_ICE:
    case EL_SNOW:
        if ((hash3((uint32_t)x, (uint32_t)y, 7u + (sb->tick >> 4)) & 63u) == 0u)
            c = add(c, 70); /* a glint */
        break;
    case EL_STEAM:
    case EL_METHANE:
    case EL_SMOKE:
        c = mix(bg, c, 90 + (int)(t4 & 63u)); /* thin gases let the background through */
        break;
    case EL_FIRE: {
        int heat = 255 - arc_mini(255, aux * 30); /* young flames are yellow-white, old ones red */
        c = mix(rgb(230, 52, 22), rgb(255, 238, 130), heat);
        c = add(c, grain(t1, 40));
        break;
    }
    case EL_LAVA: {
        int glow = (int)(t4 & 255u);
        c = mix(base, rgb(255, 214, 70), glow / 3 + (aux > 0 ? 90 : 0));
        break;
    }
    case EL_WOOD:
    case EL_PLANT:
    case EL_FUSE:
    case EL_OIL:
        if (aux & BURN_BIT) { /* embers: flickering orange on the way to charcoal */
            int timer = aux & 0x7F;
            Rgb ember = (t1 & 3u) == 0u ? rgb(255, 190, 60) : rgb(240, 90, 24);
            c = mix(mix(c, rgb(46, 36, 34), arc_mini(200, timer * 4)), ember, 120 + grain(t1, 60));
        }
        break;
    case EL_TNT:
        if (aux > 0)
            c = ((sb->tick >> 1) & 1u) ? rgb(255, 245, 230) : rgb(255, 60, 40); /* a lit charge blinks */
        break;
    case EL_URANIUM:
        c = mix(c, rgb(214, 255, 130), aux);
        if (sb->moved[i] & EXCITED)
            c = rgb(255, 255, 240);
        break;
    case EL_MOLTEN_URANIUM:
        c = mix(c, rgb(255, 255, 190), (int)(t4 & 127u));
        break;
    case EL_GAMMA:
        c = (t1 & 3u) == 0u ? rgb(255, 255, 255) : base;
        break;
    case EL_LIFE:
        c = add(c, (int)((sb->tick / LIFE_PERIOD + (uint32_t)(x + y)) & 1u) * 14);
        break;
    default:
        break;
    }
    return pack(c);
}
