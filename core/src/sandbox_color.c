/* The colour of one cell. Kept out of the renderers so every front-end (the game, the hub backdrop, tests)
 * shows the same picture, and so adding an element never means touching drawing code.
 * Everything here is a pure function of the grid and the tick: no random state is consumed. */
#include "sandbox_internal.h"

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

/* Lava is coloured by its temperature, not by a fixed base colour: pale yellow when it is very hot, then
 * amber, orange and red, and a dull crimson just before it sets into rock. */
static Rgb lava_color(int temp) {
    static const struct {
        int t;
        int r, g, b;
    } stops[] = {{500, 92, 28, 34},    {600, 150, 30, 30},   {750, 212, 52, 26},    {900, 246, 98, 24},
                 {1050, 255, 150, 40}, {1200, 255, 205, 96}, {1500, 255, 240, 190}, {2400, 255, 255, 240}};
    const int n = (int)(sizeof stops / sizeof stops[0]);
    if (temp <= stops[0].t)
        return rgb(stops[0].r, stops[0].g, stops[0].b);
    for (int k = 1; k < n; k++) {
        if (temp <= stops[k].t) {
            int f = (temp - stops[k - 1].t) * 256 / (stops[k].t - stops[k - 1].t);
            return mix(rgb(stops[k - 1].r, stops[k - 1].g, stops[k - 1].b),
                       rgb(stops[k].r, stops[k].g, stops[k].b), f);
        }
    }
    return rgb(stops[n - 1].r, stops[n - 1].g, stops[n - 1].b);
}

static uint32_t cell_color(const Sandbox *sb, int x, int y) {
    int i = y * sb->w + x;
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
    case EL_LAVA:
        c = add(lava_color(sb->temp[i]), grain(t4, 16));
        if (aux > 0) /* thermite melt: still burning bright */
            c = mix(c, rgb(255, 235, 170), 90);
        break;
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
    case EL_URANIUM:
        c = mix(c, rgb(214, 255, 130), arc_clampi((sb->temp[i] - SANDBOX_AMBIENT) * 255 / 1200, 0, 255));
        break;
    case EL_MOLTEN_URANIUM: { /* glows orange when it forms and goes white as it nears critical */
        int heat = sb->temp[i];
        Rgb glow = mix(rgb(255, 170, 60), rgb(255, 250, 215), arc_clampi((heat - 1000) * 256 / 260, 0, 256));
        c = mix(c, rgb(255, 255, 190), (int)(t4 & 127u));
        c = mix(c, glow, arc_clampi((heat - 900) * 2, 60, 210));
        break;
    }
    case EL_GAMMA:
        c = (t1 & 3u) == 0u ? rgb(255, 255, 255) : base;
        break;
    case EL_SPARK: /* white-hot, cooling to orange as it burns out */
        c = (t1 & 3u) == 0u ? rgb(255, 255, 235) : mix(rgb(255, 110, 36), base, arc_mini(256, aux * 12));
        break;
    case EL_LIFE:
        c = add(c, (int)((sb->tick / LIFE_PERIOD + (uint32_t)(x + y)) & 1u) * 14);
        break;
    case EL_STEEL:
        if (y % 3 == 0) /* brushed metal */
            c = add(c, 18);
        if (open_above)
            c = add(c, 22);
        break;
    case EL_BRICK: { /* courses of bricks with mortar between them */
        int row = y / 4;
        int off = (row & 1) ? 4 : 0;
        if (y % 4 == 0 || (x + off) % 8 == 0)
            c = mix(c, rgb(196, 188, 176), 200);
        break;
    }
    case EL_HEATER: {
        int pulse = (int)((sb->tick / 3 + (uint32_t)(x * 3 + y * 5)) % 12u);
        c = mix(c, rgb(255, 150, 60), pulse < 3 ? 140 - pulse * 40 : 24);
        break;
    }
    case EL_COOLER: {
        int pulse = (int)((sb->tick / 3 + (uint32_t)(x * 5 + y * 3)) % 12u);
        c = mix(c, rgb(170, 235, 255), pulse < 3 ? 130 - pulse * 40 : 20);
        break;
    }
    default:
        break;
    }
    /* incandescence: whatever is hot enough glows red, then orange, then white-yellow */
    int temp = sb->temp[i];
    if (temp >= 450 && d->kind != MOVE_GAS && d->kind != MOVE_FIRE && d->kind != MOVE_RAY && id != EL_LAVA &&
        id != EL_MOLTEN_URANIUM && id != EL_HEATER && !(aux & BURN_BIT && d->ignite > 0)) {
        Rgb glow = temp < 800    ? mix(rgb(190, 36, 20), rgb(255, 120, 30), (temp - 450) * 256 / 350)
                   : temp < 1200 ? mix(rgb(255, 120, 30), rgb(255, 210, 120), (temp - 800) * 256 / 400)
                                 : mix(rgb(255, 210, 120), rgb(255, 245, 220), (temp - 1200) * 256 / 600);
        c = mix(c, glow, (temp - 450) * 256 / 350 + 40);
    }
    return pack(c);
}

/* Thermal map: blue is cold, a dim teal is room temperature, then yellow, orange, red and white as it heats
 * up. */
static uint32_t heat_color(const Sandbox *sb, int i) {
    static const struct {
        int t;
        int r, g, b;
    } stops[] = {{-200, 90, 110, 255},  {-60, 40, 150, 255},  {5, 36, 200, 215},   {20, 28, 52, 66},
                 {60, 120, 170, 60},    {150, 235, 215, 60},  {400, 255, 140, 30}, {800, 235, 50, 30},
                 {1300, 255, 200, 140}, {2200, 255, 255, 255}};
    const int n = (int)(sizeof stops / sizeof stops[0]);
    if (sb->cells[i] == EL_EMPTY)
        return pack(rgb(10, 14, 24));
    int t = sb->temp[i];
    if (t <= stops[0].t)
        return pack(rgb(stops[0].r, stops[0].g, stops[0].b));
    for (int k = 1; k < n; k++) {
        if (t <= stops[k].t) {
            int span = stops[k].t - stops[k - 1].t;
            int f = (t - stops[k - 1].t) * 256 / span;
            return pack(mix(rgb(stops[k - 1].r, stops[k - 1].g, stops[k - 1].b),
                            rgb(stops[k].r, stops[k].g, stops[k].b), f));
        }
    }
    return pack(rgb(255, 255, 255));
}

uint32_t sandbox_pixel(const Sandbox *sb, int x, int y) {
    if (!sandbox_inside(sb, x, y))
        return 0;
    uint32_t px = sb->heat_view ? heat_color(sb, y * sb->w + x) : cell_color(sb, x, y);
    if (sb->flash > 0) { /* the white-out after a huge blast */
        Rgb c = rgb((int)((px >> 16) & 255u), (int)((px >> 8) & 255u), (int)(px & 255u));
        px = pack(mix(c, rgb(255, 250, 235), sb->flash));
    }
    return px;
}
