/* Synthwave City - the renderer: a CPU-friendly raylib 3D demake of the web version's look.
 *
 * Everything is plain rlgl immediate-mode geometry: no shaders, no render targets, so it also runs on the
 * headless software rasteriser used by the smoke tests. What the demake does instead of the web version's
 * shaders:
 *   - fog: colours are blended towards a haze colour per vertex (the haze matches the sky at the horizon);
 *   - neon, windows, beams and holograms: additive blending (alpha-weighted) on top of the solid pass;
 *   - bloom: soft round sprites ("glow") over every light;
 *   - sky: banded 2D gradients, a striped sun and stars, drawn first.
 * Not ported: wet-street reflections and true HDR bloom.
 * The whole scene is drawn relative to the camera, so float precision does not suffer far from the origin. */
#include <math.h>
#include <string.h>

#include "arcade/gfx.h"
#include "arcade/rng.h"
#include "rlgl.h"
#include "synthwave_city_front.h"

/* ------------------------------------------------------------------------------------------------ */
/* Colours                                                                                          */
/* ------------------------------------------------------------------------------------------------ */

typedef struct Rgb {
    float r, g, b;
} Rgb;

static Rgb rgb(float r, float g, float b) {
    Rgb c = {r, g, b};
    return c;
}
static Rgb hexrgb(unsigned h) {
    return rgb((float)((h >> 16) & 255u) / 255.0f, (float)((h >> 8) & 255u) / 255.0f,
               (float)(h & 255u) / 255.0f);
}
static Rgb mixc(Rgb a, Rgb b, float t) {
    return rgb(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t);
}
static Rgb mulc(Rgb a, float k) {
    return rgb(a.r * k, a.g * k, a.b * k);
}
static Rgb mulcc(Rgb a, Rgb b) {
    return rgb(a.r * b.r, a.g * b.g, a.b * b.b);
}
static Rgb addc(Rgb a, Rgb b) {
    return rgb(a.r + b.r, a.g + b.g, a.b + b.b);
}
static unsigned char to_byte(float v) {
    return (unsigned char)(v <= 0.0f ? 0.0f : (v >= 1.0f ? 255.0f : v * 255.0f + 0.5f));
}
/* The GPU path draws this geometry into a linear HDR target: colours are written as linear light. */
static const SwcLegacyOptions *g_opt; /* NULL when the CPU renderer draws everything */
static bool g_linear_out;
static bool g_no_buildings; /* the GPU draws the buildings and their lights */
static bool g_no_beams;     /* the GPU draws the searchlights, pillar beacons and laser fans */
static bool g_no_ground;    /* the GPU draws the streets, markings, plazas, lamps and the wet mirror */
static bool g_no_air;       /* the sky lanes' craft: the GPU draws them, or the player switched them off */
static bool g_no_street;    /* the cars on the streets: the GPU draws them, or the player switched them off */
static bool g_no_holo;      /* hologram panels, wire solids and ticker rings: same */
static unsigned g_live_layers = SWC_LIVE_ALL; /* the layers the player has not switched off (the L key) */

static float srgb_to_linear(float v) {
    return v <= 0.04045f ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}
static float linear_to_srgb(float v) {
    return v <= 0.0031308f ? v * 12.92f : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
}
static unsigned char lin_byte(float v) {
    static unsigned char lut[256];
    static bool ready;
    if (!ready) {
        for (int i = 0; i < 256; i++)
            lut[i] = to_byte(srgb_to_linear((float)i / 255.0f));
        ready = true;
    }
    return lut[to_byte(v)];
}
static Color col(Rgb c, float a) {
    if (g_linear_out) {
        Color o = {lin_byte(c.r), lin_byte(c.g), lin_byte(c.b), to_byte(a)};
        return o;
    }
    Color o = {to_byte(c.r), to_byte(c.g), to_byte(c.b), to_byte(a)};
    return o;
}
static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static float smooth(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/* The neon palette of the web version (src/contracts/palette.ts), as sRGB. */
static const unsigned PALETTE_HEX[SWC_PALETTE_SIZE] = {0xff2bd6, 0x19f0ff, 0x9b4dff, 0xff7a1a,
                                                       0xffe14d, 0x49ff7a, 0xff2740, 0xdff4ff};

typedef struct MoodStyle {
    const char *name;
    Rgb sky_top, sky_mid, horizon_cool, horizon_warm;
    Rgb sun_top, sun_bottom;
    float sun_dir[3]; /* unit vector towards the sun (x, y, z) */
    Rgb ambient;      /* light on faces turned away from the sun */
    Rgb sun_light;    /* extra light on faces turned towards it */
    Rgb body;         /* facade colour before lighting */
    Rgb road;         /* asphalt */
    float fog_range;  /* metres at which fog reaches about 63% (at street level) */
    float stars;      /* 0..2 */
    float glow;       /* multiplier on neon */
    float windows;    /* how brightly windows burn */
    Rgb tint;         /* per-channel multiplier on the palette */
} MoodStyle;

static MoodStyle g_moods[SWC_MOOD_COUNT];

static void unit3(float v[3]) {
    float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= l;
    v[1] /= l;
    v[2] /= l;
}

static void init_moods(void) {
    MoodStyle *m = &g_moods[0]; /* sunset */
    m->name = "Sunset";
    m->sky_top = hexrgb(0x06021c);
    m->sky_mid = hexrgb(0x3a0d6e);
    m->horizon_cool = hexrgb(0x70268f);
    m->horizon_warm = hexrgb(0xff3f8e);
    m->sun_top = hexrgb(0xffe85a);
    m->sun_bottom = hexrgb(0xff2f8a);
    m->sun_dir[0] = 0.0f, m->sun_dir[1] = 0.075f, m->sun_dir[2] = -1.0f;
    m->ambient = rgb(0.16f, 0.09f, 0.30f);
    m->sun_light = rgb(0.80f, 0.28f, 0.46f);
    m->body = rgb(0.55f, 0.50f, 0.70f);
    m->road = rgb(0.050f, 0.032f, 0.090f);
    m->fog_range = 700.0f;
    m->stars = 0.8f;
    m->glow = 1.0f;
    m->windows = 0.85f;
    m->tint = rgb(1.0f, 1.0f, 1.0f);

    m = &g_moods[1]; /* midnight */
    m->name = "Midnight";
    m->sky_top = hexrgb(0x010210);
    m->sky_mid = hexrgb(0x07103a);
    m->horizon_cool = hexrgb(0x14145a);
    m->horizon_warm = hexrgb(0x3946c0);
    m->sun_top = hexrgb(0xe8f4ff);
    m->sun_bottom = hexrgb(0x56c8ff);
    m->sun_dir[0] = 0.35f, m->sun_dir[1] = 0.16f, m->sun_dir[2] = -1.0f;
    m->ambient = rgb(0.05f, 0.07f, 0.20f);
    m->sun_light = rgb(0.30f, 0.50f, 0.85f);
    m->body = rgb(0.50f, 0.55f, 0.75f);
    m->road = rgb(0.020f, 0.026f, 0.075f);
    m->fog_range = 780.0f;
    m->stars = 1.6f;
    m->glow = 1.1f;
    m->windows = 1.0f;
    m->tint = rgb(0.9f, 1.0f, 1.15f);

    m = &g_moods[2]; /* miami */
    m->name = "Miami";
    m->sky_top = hexrgb(0x1b0a52);
    m->sky_mid = hexrgb(0xb03aa8);
    m->horizon_cool = hexrgb(0x8f4fc0);
    m->horizon_warm = hexrgb(0xffb07a);
    m->sun_top = hexrgb(0xfff3a0);
    m->sun_bottom = hexrgb(0xff5a5a);
    m->sun_dir[0] = -0.25f, m->sun_dir[1] = 0.11f, m->sun_dir[2] = -1.0f;
    m->ambient = rgb(0.22f, 0.12f, 0.36f);
    m->sun_light = rgb(0.85f, 0.46f, 0.38f);
    m->body = rgb(0.60f, 0.55f, 0.70f);
    m->road = rgb(0.070f, 0.045f, 0.110f);
    m->fog_range = 620.0f;
    m->stars = 0.35f;
    m->glow = 0.95f;
    m->windows = 0.7f;
    m->tint = rgb(1.05f, 1.0f, 0.95f);
    for (int i = 0; i < SWC_MOOD_COUNT; i++)
        unit3(g_moods[i].sun_dir);
}

const char *swc_mood_name(int mood) {
    return g_moods[mood >= 0 && mood < SWC_MOOD_COUNT ? mood : 0].name;
}

/* ------------------------------------------------------------------------------------------------ */
/* Per-frame state                                                                                  */
/* ------------------------------------------------------------------------------------------------ */

#define DRAW_DIST 1500.0f      /* ordinary buildings are drawn out to here */
#define DRAW_DIST_TALL 2400.0f /* ... and the tallest ones out to here */
#define CELL_RANGE 15          /* cells searched each way: covers DRAW_DIST_TALL */
#define WINDOW_DIST 320.0f
#define TRIM_DIST 700.0f
#define MAX_VIS_BOXES 9000
#define MAX_VEHICLES 1800
#define MAX_SPRITES 7000
#define FAR_PLANE 4000.0

typedef struct Frame {
    const synthwave_city_State *s;
    const MoodStyle *mood;
    double camx, camy, camz;
    float fwd[3], right[3], up[3]; /* the camera's axes, leaning with the roll */
    float right0[3], up0[3];       /* the same without the roll: the sky is drawn upright, then turned */
    float roll;                    /* radians */
    float tan_h, tan_v, sec_h, sec_v, focal;
    float sun_h[2]; /* horizontal unit vector towards the sun */
    double t;
    float pulse;
    int w, h;
    Rgb pal[SWC_PALETTE_SIZE];
    Rgb face[5]; /* light on +x, -x, +z, -z faces and on roofs */
} Frame;

static Frame F;

static void setup_frame(const synthwave_city_State *s, double t) {
    F.s = s;
    F.mood = &g_moods[s->mood >= 0 && s->mood < SWC_MOOD_COUNT ? s->mood : 0];
    F.camx = s->cam.x;
    F.camy = s->cam.y;
    F.camz = s->cam.z;
    F.t = t;
    F.pulse = (float)swc_beat_pulse(s);
    F.w = GetScreenWidth();
    F.h = GetScreenHeight();
    float cy = (float)cos(s->cam.yaw), sy = (float)sin(s->cam.yaw);
    float cp = (float)cos(s->cam.pitch), sp = (float)sin(s->cam.pitch);
    F.fwd[0] = -sy * cp;
    F.fwd[1] = sp;
    F.fwd[2] = -cy * cp;
    F.right[0] = cy;
    F.right[1] = 0.0f;
    F.right[2] = -sy;
    F.up[0] = -F.right[2] * F.fwd[1];
    F.up[1] = F.right[2] * F.fwd[0] - F.right[0] * F.fwd[2];
    F.up[2] = F.right[0] * F.fwd[1];
    F.roll = (float)s->roll;
    for (int i = 0; i < 3; i++) {
        F.right0[i] = F.right[i];
        F.up0[i] = F.up[i];
    }
    if (F.roll != 0.0f) { /* three.js "YXZ": the roll turns the right and up axes about the view axis */
        float cr = cosf(F.roll), sr = sinf(F.roll);
        for (int i = 0; i < 3; i++) {
            F.right[i] = cr * F.right0[i] + sr * F.up0[i];
            F.up[i] = -sr * F.right0[i] + cr * F.up0[i];
        }
    }
    F.tan_v = tanf((float)s->fov * 0.5f * (ARC_PI / 180.0f));
    F.tan_h = F.tan_v * (float)F.w / (float)F.h;
    F.sec_h = sqrtf(1.0f + F.tan_h * F.tan_h);
    F.sec_v = sqrtf(1.0f + F.tan_v * F.tan_v);
    F.focal = (float)F.h * 0.5f / F.tan_v;
    float sh = sqrtf(F.mood->sun_dir[0] * F.mood->sun_dir[0] + F.mood->sun_dir[2] * F.mood->sun_dir[2]);
    F.sun_h[0] = F.mood->sun_dir[0] / sh;
    F.sun_h[1] = F.mood->sun_dir[2] / sh;
    for (int i = 0; i < SWC_PALETTE_SIZE; i++)
        F.pal[i] = mulcc(hexrgb(PALETTE_HEX[i]), F.mood->tint);
    static const float normals[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 4; i++) {
        float d = normals[i][0] * F.sun_h[0] + normals[i][1] * F.sun_h[1];
        F.face[i] = addc(F.mood->ambient, mulc(F.mood->sun_light, d > 0.0f ? d * d : 0.0f));
    }
    F.face[4] = addc(mulc(F.mood->ambient, 1.5f), mulc(F.mood->sun_light, 0.12f));
}

/* Is a sphere (camera-relative centre, radius) inside the view? */
static bool visible(float x, float y, float z, float r) {
    float cz = x * F.fwd[0] + y * F.fwd[1] + z * F.fwd[2];
    if (cz < -r)
        return false;
    float cx = x * F.right[0] + y * F.right[1] + z * F.right[2];
    if (fabsf(cx) > cz * F.tan_h + r * F.sec_h)
        return false;
    float cy = x * F.up[0] + y * F.up[1] + z * F.up[2];
    return fabsf(cy) <= cz * F.tan_v + r * F.sec_v;
}

static float rel_dist(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}

/* ------------------------------------------------------------------------------------------------ */
/* Fog                                                                                              */
/* ------------------------------------------------------------------------------------------------ */

/* The haze colour towards a horizontal direction: warm towards the sun, cool away from it. It is also the
 * colour of the sky at the horizon, so far geometry melts into it. */
static Rgb haze(float dx, float dz) {
    float l = sqrtf(dx * dx + dz * dz);
    if (g_linear_out &&
        g_opt->look != NULL) { /* sc_fogColor of the web version, handed over as display colour */
        const SwcLook *k = g_opt->look;
        float sl = sqrtf(k->sun_dir[0] * k->sun_dir[0] + k->sun_dir[2] * k->sun_dir[2]);
        float t = 0.5f;
        if (l > 1e-4f && sl > 1e-4f)
            t = 0.5f + 0.5f * (dx * k->sun_dir[0] + dz * k->sun_dir[2]) / (l * sl);
        t *= t;
        return rgb(linear_to_srgb(k->fog_cool[0] + (k->fog_warm[0] - k->fog_cool[0]) * t),
                   linear_to_srgb(k->fog_cool[1] + (k->fog_warm[1] - k->fog_cool[1]) * t),
                   linear_to_srgb(k->fog_cool[2] + (k->fog_warm[2] - k->fog_cool[2]) * t));
    }
    float w = 0.0f;
    if (l > 1e-4f) {
        float d = (dx * F.sun_h[0] + dz * F.sun_h[1]) / l;
        w = d > 0.0f ? d * d : 0.0f;
    }
    return mulc(mixc(F.mood->horizon_cool, F.mood->horizon_warm, w), 0.92f);
}

/* Share of fog at a distance and a height above the street: thicker low down, thinner up in the air. */
static float fog_amount(float dist, float world_y) {
    float k = dist / F.mood->fog_range;
    float f = 1.0f - expf(-k * k);
    f *= 0.30f + 0.70f * expf(-(world_y > 0.0f ? world_y : 0.0f) / 240.0f);
    return f > 0.97f ? 0.97f : f;
}

/* A lit colour at a camera-relative point, blended into the haze. */
static Color fogged(Rgb lit, float rx, float ry, float rz) {
    float f = fog_amount(rel_dist(rx, ry, rz), ry + (float)F.camy);
    return col(mixc(lit, haze(rx, rz), f), 1.0f);
}

/* Emission fades out in fog instead of blending: returns the alpha that goes with the colour. */
static float fade(float rx, float ry, float rz) {
    return 1.0f - fog_amount(rel_dist(rx, ry, rz), ry + (float)F.camy);
}

/* ------------------------------------------------------------------------------------------------ */
/* Immediate-mode helpers                                                                           */
/* ------------------------------------------------------------------------------------------------ */

static void vtx(float x, float y, float z, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(x, y, z);
}

static void quad(const float a[3], const float b[3], const float c[3], const float d[3], Color ca, Color cb,
                 Color cc, Color cd) {
    vtx(a[0], a[1], a[2], ca);
    vtx(b[0], b[1], b[2], cb);
    vtx(c[0], c[1], c[2], cc);
    vtx(d[0], d[1], d[2], cd);
}

static void tex_vtx(float x, float y, float z, float u, float v, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlTexCoord2f(u, v);
    rlVertex3f(x, y, z);
}

/* ------------------------------------------------------------------------------------------------ */
/* Textures                                                                                         */
/* ------------------------------------------------------------------------------------------------ */

#define TEX_N 64
#define WINDOW_TEXTURES 4
static Texture2D g_window_tex[WINDOW_TEXTURES];
static Texture2D g_glow_tex;
static bool g_ready;
static int g_users; /* swc_render_init() calls not yet matched by a shutdown: the hub backdrop and a game
                       started from the archive can both be using the renderer at the same time */
/* metres covered by one repeat of each texture: across, up */
static const float TEX_SPAN[WINDOW_TEXTURES][2] = {
    {24.0f, 32.0f}, {24.0f, 32.0f}, {24.0f, 32.0f}, {24.0f, 32.0f}};

static Texture2D make_texture(const Color *pixels) {
    Image img;
    img.data = (void *)pixels;
    img.width = TEX_N;
    img.height = TEX_N;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D t = LoadTextureFromImage(img);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    SetTextureFilter(t, TEXTURE_FILTER_POINT); /* crisp windows, and half the cost on the CPU rasteriser */
    return t;
}

static void put_px(Color *px, int x, int y, float v) {
    unsigned char b = to_byte(v);
    Color c = {b, b, b, 255};
    px[(y & (TEX_N - 1)) * TEX_N + (x & (TEX_N - 1))] = c;
}

static void fill_cells(Color *px, Rng *r, int cells, int wx, int wy, int ox, int oy, float lit, bool floors) {
    int step = TEX_N / cells;
    for (int cy = 0; cy < cells; cy++) {
        bool floor_on = rng_float(r) < 0.35f;
        for (int cx = 0; cx < cells; cx++) {
            float v = rng_float(r) < lit || (floors && floor_on) ? 0.45f + 0.55f * rng_float(r) : 0.0f;
            for (int y = 0; y < wy; y++)
                for (int x = 0; x < wx; x++)
                    put_px(px, cx * step + ox + x, cy * step + oy + y, v);
        }
    }
}

static void init_textures(void) {
    static Color px[TEX_N * TEX_N];
    Rng r;
    rng_seed(&r, 0x5c17u);
    memset(px, 0, sizeof(px));
    for (int i = 0; i < TEX_N * TEX_N; i++)
        px[i].a = 255;
    /* 0: punched windows, 8 x 8 cells of 8 px */
    fill_cells(px, &r, 8, 4, 5, 2, 2, 0.55f, false);
    g_window_tex[0] = make_texture(px);
    /* 1: curtain glass, 16 x 16 small panes */
    memset(px, 0, sizeof(px));
    for (int i = 0; i < TEX_N * TEX_N; i++)
        px[i].a = 255;
    fill_cells(px, &r, 16, 3, 3, 0, 0, 0.6f, true);
    g_window_tex[1] = make_texture(px);
    /* 2: ribbon windows, a lit band in each of 8 floors, broken here and there */
    memset(px, 0, sizeof(px));
    for (int i = 0; i < TEX_N * TEX_N; i++)
        px[i].a = 255;
    for (int row = 0; row < 8; row++)
        for (int seg = 0; seg < 4; seg++) {
            float v = rng_float(&r) < 0.7f ? 0.5f + 0.5f * rng_float(&r) : 0.0f;
            for (int y = 2; y < 6; y++)
                for (int x = 1; x < 15; x++)
                    put_px(px, seg * 16 + x, row * 8 + y, v);
        }
    g_window_tex[2] = make_texture(px);
    /* 3: vertical strips, 8 columns, with dashes */
    memset(px, 0, sizeof(px));
    for (int i = 0; i < TEX_N * TEX_N; i++)
        px[i].a = 255;
    for (int col_i = 0; col_i < 8; col_i++)
        for (int blk = 0; blk < 4; blk++) {
            float v = rng_float(&r) < 0.75f ? 0.5f + 0.5f * rng_float(&r) : 0.0f;
            for (int y = 1; y < 15; y++)
                for (int x = 2; x < 6; x++)
                    put_px(px, col_i * 8 + x, blk * 16 + y, v);
        }
    g_window_tex[3] = make_texture(px);
    /* soft round glow */
    for (int y = 0; y < TEX_N; y++)
        for (int x = 0; x < TEX_N; x++) {
            float dx = ((float)x + 0.5f) / (TEX_N * 0.5f) - 1.0f,
                  dy = ((float)y + 0.5f) / (TEX_N * 0.5f) - 1.0f;
            float d2 = dx * dx + dy * dy;
            float a = d2 >= 1.0f ? 0.0f : (1.0f - d2) * (1.0f - d2) * (0.25f + 0.75f * expf(-d2 * 5.0f));
            Color c = {255, 255, 255, to_byte(a)};
            px[y * TEX_N + x] = c;
        }
    g_glow_tex = make_texture(px);
}

void swc_render_init(void) {
    if (g_users++ > 0)
        return; /* somebody already built the textures: share them */
    init_moods();
    init_textures();
    g_ready = true;
}

void swc_render_shutdown(void) {
    if (g_users == 0 || --g_users > 0)
        return; /* nothing to release, or another user still needs the textures */
    for (int i = 0; i < WINDOW_TEXTURES; i++)
        UnloadTexture(g_window_tex[i]);
    UnloadTexture(g_glow_tex);
    g_ready = false;
}

/* ------------------------------------------------------------------------------------------------ */
/* Glow sprites: the demake's bloom                                                                 */
/* ------------------------------------------------------------------------------------------------ */

typedef struct Sprite {
    float x, y, z, size;
    Color c;
} Sprite;

static Sprite g_sprites[MAX_SPRITES];
static int g_sprite_count;

/* A soft light of radius `size` metres. Fog dims it; a light too far away to be a pixel stays one pixel. */
static void glow(float x, float y, float z, float size, Rgb c, float alpha) {
    if (g_sprite_count >= MAX_SPRITES)
        return;
    float d = rel_dist(x, y, z);
    if (d < 0.5f)
        return;
    float px = size * F.focal / d; /* radius on the screen, pixels */
    if (px < 1.5f) {
        alpha *= px / 1.5f;
        size = 1.5f * d / F.focal;
    }
    alpha *= fade(x, y, z) * F.mood->glow;
    if (alpha < 0.02f)
        return;
    Sprite *s = &g_sprites[g_sprite_count++];
    s->x = x;
    s->y = y;
    s->z = z;
    s->size = size;
    s->c = col(c, alpha);
}

static void flush_sprites(void) {
    if (g_sprite_count == 0)
        return;
    rlSetTexture(g_glow_tex.id);
    rlBegin(RL_QUADS);
    for (int i = 0; i < g_sprite_count; i++) {
        const Sprite *s = &g_sprites[i];
        float rx = F.right[0] * s->size, ry = F.right[1] * s->size, rz = F.right[2] * s->size;
        float ux = F.up[0] * s->size, uy = F.up[1] * s->size, uz = F.up[2] * s->size;
        tex_vtx(s->x - rx - ux, s->y - ry - uy, s->z - rz - uz, 0.0f, 1.0f, s->c);
        tex_vtx(s->x + rx - ux, s->y + ry - uy, s->z + rz - uz, 1.0f, 1.0f, s->c);
        tex_vtx(s->x + rx + ux, s->y + ry + uy, s->z + rz + uz, 1.0f, 0.0f, s->c);
        tex_vtx(s->x - rx + ux, s->y - ry + uy, s->z - rz + uz, 0.0f, 0.0f, s->c);
    }
    rlEnd();
    rlSetTexture(0);
    g_sprite_count = 0;
}

/* ------------------------------------------------------------------------------------------------ */
/* Sky                                                                                              */
/* ------------------------------------------------------------------------------------------------ */

/* Sky colour at an elevation (radians above the horizon) in the direction of azimuth `az`. */
static Rgb sky_colour(float elev, float az) {
    float dx = -sinf(az), dz = -cosf(az);
    Rgb hz = haze(dx, dz);
    Rgb c = mixc(hz, F.mood->sky_mid, smooth(0.0f, 0.42f, elev));
    return mixc(c, F.mood->sky_top, smooth(0.30f, 1.25f, elev));
}

static void draw_sky(void) {
    const synthwave_city_State *s = F.s;
    const int bands = 40;
    /* the far ground ring reaches 3600 m: from the camera's height it covers every angle below this */
    float covered = atanf((float)F.camy / 3400.0f) + 0.04f;
    float yaw = (float)s->cam.yaw, pitch = (float)s->cam.pitch;
    float f = F.focal, cx = (float)F.w * 0.5f, cy = (float)F.h * 0.5f;
    /* The sky is drawn as if the camera did not lean, and the whole picture then turned by the roll about its
     * centre; the bands reach as far as the turned screen needs (the screen itself when there is no roll). */
    float half_w = cx * fabsf(cosf(F.roll)) + cy * fabsf(sinf(F.roll));
    float half_h = cx * fabsf(sinf(F.roll)) + cy * fabsf(cosf(F.roll));
    if (F.roll == 0.0f)
        half_w = cx, half_h = cy;
    float cols_x[4];
    float az[4];
    for (int i = 0; i < 4; i++) {
        cols_x[i] = (cx - half_w) + 2.0f * half_w * (float)i / 3.0f;
        az[i] = yaw - atanf((cols_x[i] - cx) / f / cosf(pitch));
    }
    if (F.roll != 0.0f) {
        rlPushMatrix();
        rlTranslatef(cx, cy, 0.0f);
        rlRotatef(F.roll * (180.0f / ARC_PI), 0.0f, 0.0f, 1.0f);
        rlTranslatef(-cx, -cy, 0.0f);
    }
    for (int b = 0; b < bands; b++) {
        float y0 = (cy - half_h) + 2.0f * half_h * (float)b / (float)bands;
        float y1 = (cy - half_h) + 2.0f * half_h * (float)(b + 1) / (float)bands;
        float e0 = pitch + atanf((cy - y0) / f), e1 = pitch + atanf((cy - y1) / f);
        if (e1 < -covered) /* the ground ring hides everything this far below the horizon */
            continue;
        for (int i = 0; i < 3; i++) {
            Color tl = col(sky_colour(e0, az[i]), 1.0f), bl = col(sky_colour(e1, az[i]), 1.0f);
            Color tr = col(sky_colour(e0, az[i + 1]), 1.0f), br = col(sky_colour(e1, az[i + 1]), 1.0f);
            Rectangle r = {cols_x[i], y0, cols_x[i + 1] - cols_x[i] + 1.0f, y1 - y0 + 1.0f};
            DrawRectangleGradientEx(r, tl, bl, br, tr);
        }
    }

    /* stars: fixed directions on the sky, twinkling a little */
    int stars = (int)(260.0f * F.mood->stars);
    for (int i = 0; i < stars; i++) {
        uint32_t h1 = swc_hash32((uint32_t)i * 2u + 11u), h2 = swc_hash32((uint32_t)i * 2u + 12u);
        float saz = (float)h1 / 4294967296.0f * ARC_TAU;
        float sel = 0.08f + 1.45f * powf((float)h2 / 4294967296.0f, 0.8f);
        float dx = -sinf(saz) * cosf(sel), dy = sinf(sel), dz = -cosf(saz) * cosf(sel);
        float zc = dx * F.fwd[0] + dy * F.fwd[1] + dz * F.fwd[2];
        if (zc < 0.05f)
            continue;
        float xs = cx + (dx * F.right0[0] + dz * F.right0[2]) / zc * f;
        float ys = cy - (dx * F.up0[0] + dy * F.up0[1] + dz * F.up0[2]) / zc * f;
        float tw = 0.65f + 0.35f * sinf((float)F.t * (1.0f + (float)(h1 & 7u) * 0.3f) + (float)i);
        float a = clampf(F.mood->stars * 0.5f, 0.0f, 1.0f) * tw * smooth(0.10f, 0.55f, sel);
        DrawRectangle((int)xs, (int)ys, (h1 & 15u) == 0 ? 2 : 1, (h1 & 15u) == 0 ? 2 : 1,
                      col(rgb(1, 1, 1), a));
    }

    /* the sun: a disc with the classic stripes cut out of its lower half */
    const MoodStyle *m = F.mood;
    float zc = m->sun_dir[0] * F.fwd[0] + m->sun_dir[1] * F.fwd[1] + m->sun_dir[2] * F.fwd[2];
    if (zc > 0.05f) {
        float sx = cx + (m->sun_dir[0] * F.right0[0] + m->sun_dir[2] * F.right0[2]) / zc * f;
        float sy =
            cy - (m->sun_dir[0] * F.up0[0] + m->sun_dir[1] * F.up0[1] + m->sun_dir[2] * F.up0[2]) / zc * f;
        float radius = 0.115f * f;
        BeginBlendMode(BLEND_ADDITIVE);
        for (int k = 0; k < 4; k++)
            DrawCircle((int)sx, (int)sy, radius * (1.2f + (float)k * 0.6f),
                       col(m->sun_bottom, (0.075f - 0.015f * (float)k) * (0.8f + 0.5f * F.pulse)));
        EndBlendMode();
        for (float row = -radius; row < radius; row += 2.0f) {
            float t = (row + radius) / (2.0f * radius);
            if (t > 0.45f) { /* stripes: thicker towards the bottom */
                float u = (t - 0.45f) / 0.55f;
                float ph = u * 7.0f;
                if (ph - floorf(ph) < 0.18f + 0.55f * u)
                    continue;
            }
            float chord = sqrtf(fmaxf(0.0f, radius * radius - row * row));
            DrawRectangle((int)(sx - chord), (int)(sy + row), (int)(chord * 2.0f), 2,
                          col(mixc(m->sun_top, m->sun_bottom, clampf(t * 1.1f, 0.0f, 1.0f)), 1.0f));
        }
    }
    if (F.roll != 0.0f)
        rlPopMatrix();
}

/* ------------------------------------------------------------------------------------------------ */
/* The solid pass: ground and buildings                                                             */
/* ------------------------------------------------------------------------------------------------ */

typedef struct VisBox {
    int index;
    float ox, oz; /* add to the box's tile coordinates to get camera-relative ones */
    float dist;
} VisBox;

static VisBox g_vis[MAX_VIS_BOXES];
static int g_vis_count;

typedef struct VisCell {
    int index;
    float rx, rz; /* camera-relative corner (min x, min z) of the cell */
    float dist;
} VisCell;
static VisCell g_cells[(2 * CELL_RANGE + 1) * (2 * CELL_RANGE + 1)];
static int g_cell_count;

static void collect_cells(void) {
    const SwcCity *c = &F.s->city;
    int ccx = (int)floor(F.camx / SWC_CELL), ccz = (int)floor(F.camz / SWC_CELL);
    g_cell_count = 0;
    g_vis_count = 0;
    for (int dz = -CELL_RANGE; dz <= CELL_RANGE; dz++)
        for (int dx = -CELL_RANGE; dx <= CELL_RANGE; dx++) {
            int ix = ccx + dx, iz = ccz + dz;
            int ci = swc_cell_index(ix, iz);
            float rx = (float)(ix * SWC_CELL - F.camx), rz = (float)(iz * SWC_CELL - F.camz);
            float mx = rx + (float)SWC_CELL * 0.5f, mz = rz + (float)SWC_CELL * 0.5f;
            float cell_h = (float)c->cells[ci].height * 4.0f;
            float hd = sqrtf(mx * mx + mz * mz);
            float max_d = DRAW_DIST + clampf(cell_h * 4.0f, 0.0f, DRAW_DIST_TALL - DRAW_DIST);
            if (hd > max_d)
                continue;
            float mid_y = cell_h * 0.5f - (float)F.camy;
            float rad = sqrtf(2.0f * 80.0f * 80.0f + mid_y * 0.0f + (cell_h * 0.5f) * (cell_h * 0.5f));
            if (!visible(mx, mid_y, mz, rad + 6.0f))
                continue;
            VisCell *v = &g_cells[g_cell_count++];
            v->index = ci;
            v->rx = rx;
            v->rz = rz;
            v->dist = hd;
        }
}

/* Light on a vertical face with horizontal normal (nx, nz). */
static Rgb face_light(float nx, float nz) {
    float d = nx * F.sun_h[0] + nz * F.sun_h[1];
    return addc(F.mood->ambient, mulc(F.mood->sun_light, d > 0.0f ? d * d : 0.0f));
}

static void draw_ground(void) {
    const SwcCity *city = &F.s->city;
    const MoodStyle *m = F.mood;
    float gy = (float)-F.camy;
    /* beyond the cells: one ring of haze, so the ground never ends in a visible edge */
    {
        const int sectors = 48;
        float r0 = DRAW_DIST * 0.8f, r1 = 3600.0f;
        rlBegin(RL_QUADS);
        for (int i = 0; i < sectors; i++) {
            float a0 = ARC_TAU * (float)i / sectors, a1 = ARC_TAU * (float)(i + 1) / sectors;
            float p[4][3] = {{sinf(a0) * r0, gy - 0.3f, cosf(a0) * r0},
                             {sinf(a1) * r0, gy - 0.3f, cosf(a1) * r0},
                             {sinf(a1) * r1, gy - 0.3f, cosf(a1) * r1},
                             {sinf(a0) * r1, gy - 0.3f, cosf(a0) * r1}};
            Color cc[4];
            for (int k = 0; k < 4; k++)
                cc[k] = col(mixc(m->road, haze(p[k][0], p[k][2]), 0.97f), 1.0f);
            quad(p[0], p[1], p[2], p[3], cc[0], cc[1], cc[2], cc[3]);
        }
        rlEnd();
    }
    rlBegin(RL_QUADS);
    for (int i = 0; i < g_cell_count; i++) {
        const VisCell *vc = &g_cells[i];
        int cx = vc->index % SWC_CELLS, cz = vc->index / SWC_CELLS;
        const SwcCell *cell = &city->cells[vc->index];
        SwcRect blk = swc_block_rect(cx, cz);
        float bx0 = vc->rx + (float)(blk.x0 - cx * SWC_CELL), bx1 = vc->rx + (float)(blk.x1 - cx * SWC_CELL);
        float bz0 = vc->rz + (float)(blk.z0 - cz * SWC_CELL), bz1 = vc->rz + (float)(blk.z1 - cz * SWC_CELL);
        float x0 = vc->rx, x1 = vc->rx + (float)SWC_CELL, z0 = vc->rz, z1 = vc->rz + (float)SWC_CELL;
        /* the road surface is split into the four corners of the cell so that fog follows it closely */
        float xs[3] = {x0, 0.5f * (x0 + x1), x1}, zs[3] = {z0, 0.5f * (z0 + z1), z1};
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) {
                float p[4][3] = {{xs[a], gy, zs[b]},
                                 {xs[a + 1], gy, zs[b]},
                                 {xs[a + 1], gy, zs[b + 1]},
                                 {xs[a], gy, zs[b + 1]}};
                Color cc[4];
                for (int k = 0; k < 4; k++)
                    cc[k] = fogged(m->road, p[k][0], gy, p[k][2]);
                quad(p[0], p[1], p[2], p[3], cc[0], cc[1], cc[2], cc[3]);
            }
        /* the block: a slightly lighter pavement, tinted by the neighbourhood's colour */
        Rgb pave = mixc(mulc(m->road, 2.2f), mulc(F.pal[cell->palette & 7], 0.10f),
                        cell->district == SWC_PLAZA ? 0.9f : 0.35f);
        if (cell->district == SWC_PLAZA)
            pave = mulc(pave, 1.6f);
        float yb = gy + 0.04f;
        float p[4][3] = {{bx0, yb, bz0}, {bx1, yb, bz0}, {bx1, yb, bz1}, {bx0, yb, bz1}};
        Color cc[4];
        for (int k = 0; k < 4; k++)
            cc[k] = fogged(pave, p[k][0], yb, p[k][2]);
        quad(p[0], p[1], p[2], p[3], cc[0], cc[1], cc[2], cc[3]);
    }
    rlEnd();
}

static Rgb box_body(const SwcBox *b) {
    const MoodStyle *m = F.mood;
    Rgb body = m->body;
    switch (b->style) {
    case SWC_STYLE_GLASS:
        body = mixc(body, rgb(0.20f, 0.45f, 0.65f), 0.55f);
        break;
    case SWC_STYLE_MEGA:
        body = mulc(body, 0.65f);
        break;
    case SWC_STYLE_PODIUM:
        body = mulc(body, 0.95f);
        break;
    case SWC_STYLE_SLAB:
        body = mixc(body, rgb(0.55f, 0.55f, 0.60f), 0.5f);
        break;
    default:
        break;
    }
    return mixc(body, F.pal[b->color_a & 7], 0.10f + 0.10f * b->seed);
}

static bool box_emissive(const SwcBox *b) {
    return b->style == SWC_STYLE_CROWN || b->style == SWC_STYLE_SPIRE || b->style == SWC_STYLE_KIOSK;
}

/* One vertical face: p0 and p1 are the bottom corners, h the height. */
static void side_face(float x0, float z0, float x1, float z1, float y0, float y1, Rgb lit,
                      float bottom_dark) {
    float a[3] = {x0, y0, z0}, b[3] = {x1, y0, z1}, c[3] = {x1, y1, z1}, d[3] = {x0, y1, z0};
    Color ca = fogged(mulc(lit, bottom_dark), x0, y0, z0), cb = fogged(mulc(lit, bottom_dark), x1, y0, z1);
    Color cc = fogged(lit, x1, y1, z1), cd = fogged(lit, x0, y1, z0);
    quad(a, b, c, d, ca, cb, cc, cd);
}

#define ROUND_SIDES 12

static void draw_box_solid(const SwcBox *b, float ox, float oz) {
    float cx = b->cx + ox, cz = b->cz + oz;
    float hx = b->sx * 0.5f, hz = b->sz * 0.5f;
    float y0 = b->y0 - (float)F.camy, y1 = y0 + b->sy;
    bool emissive = box_emissive(b);
    Rgb body = box_body(b);
    Rgb glow_c = mulc(F.pal[b->color_a & 7], F.mood->glow * (0.75f + 0.25f * F.pulse));
    if (b->flags & SWC_FLAG_ROUND) {
        float r = fminf(hx, hz);
        for (int i = 0; i < ROUND_SIDES; i++) {
            float a0 = ARC_TAU * (float)i / ROUND_SIDES, a1 = ARC_TAU * (float)(i + 1) / ROUND_SIDES;
            float am = 0.5f * (a0 + a1);
            float nx = cosf(am), nz = sinf(am);
            if ((cx + nx * r) * nx + (cz + nz * r) * nz > 0.0f) /* turned away from the camera */
                continue;
            Rgb lit = emissive ? glow_c : mulcc(body, face_light(nx, nz));
            side_face(cx + cosf(a0) * r, cz + sinf(a0) * r, cx + cosf(a1) * r, cz + sinf(a1) * r, y0, y1, lit,
                      0.7f);
        }
        if (y1 < 0.0f) {
            Rgb lit = emissive ? glow_c : mulcc(body, F.face[4]);
            for (int i = 0; i < ROUND_SIDES; i += 2) {
                float a0 = ARC_TAU * (float)i / ROUND_SIDES, a1 = ARC_TAU * (float)(i + 1) / ROUND_SIDES,
                      a2 = ARC_TAU * (float)(i + 2) / ROUND_SIDES;
                float p0[3] = {cx, y1, cz}, p1[3] = {cx + cosf(a0) * r, y1, cz + sinf(a0) * r},
                      p2[3] = {cx + cosf(a1) * r, y1, cz + sinf(a1) * r},
                      p3[3] = {cx + cosf(a2) * r, y1, cz + sinf(a2) * r};
                Color c0 = fogged(lit, cx, y1, cz), c1 = fogged(lit, p1[0], y1, p1[2]),
                      c2 = fogged(lit, p2[0], y1, p2[2]), c3 = fogged(lit, p3[0], y1, p3[2]);
                quad(p0, p1, p2, p3, c0, c1, c2, c3);
            }
        }
        return;
    }
    float xa = cx - hx, xb = cx + hx, za = cz - hz, zb = cz + hz;
    if (xb < 0.0f) /* the +x face looks towards the camera */
        side_face(xb, zb, xb, za, y0, y1, emissive ? glow_c : mulcc(body, F.face[0]), 0.7f);
    if (xa > 0.0f)
        side_face(xa, za, xa, zb, y0, y1, emissive ? glow_c : mulcc(body, F.face[1]), 0.7f);
    if (zb < 0.0f)
        side_face(xa, zb, xb, zb, y0, y1, emissive ? glow_c : mulcc(body, F.face[2]), 0.7f);
    if (za > 0.0f)
        side_face(xb, za, xa, za, y0, y1, emissive ? glow_c : mulcc(body, F.face[3]), 0.7f);
    if (y1 < 0.0f) {
        Rgb lit = emissive ? glow_c : mulcc(body, F.face[4]);
        float a[3] = {xa, y1, za}, bb[3] = {xb, y1, za}, c[3] = {xb, y1, zb}, d[3] = {xa, y1, zb};
        quad(a, bb, c, d, fogged(lit, xa, y1, za), fogged(lit, xb, y1, za), fogged(lit, xb, y1, zb),
             fogged(lit, xa, y1, zb));
    }
}

static void draw_buildings_solid(void) {
    const SwcCity *city = &F.s->city;
    for (int i = 0; i < g_cell_count; i++) {
        const VisCell *vc = &g_cells[i];
        int cx = vc->index % SWC_CELLS, cz = vc->index / SWC_CELLS;
        float ox = vc->rx - (float)(cx * SWC_CELL), oz = vc->rz - (float)(cz * SWC_CELL);
        for (int k = city->box_start[vc->index]; k < city->box_start[vc->index + 1]; k++) {
            const SwcBox *b = &city->boxes[city->box_list[k]];
            float rx = b->cx + ox, rz = b->cz + oz;
            float ry = b->y0 + b->sy * 0.5f - (float)F.camy;
            float dist = rel_dist(rx, ry, rz);
            float size = fmaxf(b->sx, fmaxf(b->sy, b->sz));
            if (size * F.focal / fmaxf(dist, 1.0f) < 1.2f) /* smaller than a pixel */
                continue;
            float rad = 0.5f * sqrtf(b->sx * b->sx + b->sy * b->sy + b->sz * b->sz);
            if (!visible(rx, ry, rz, rad))
                continue;
            if (g_vis_count < MAX_VIS_BOXES) {
                VisBox *v = &g_vis[g_vis_count++];
                v->index = city->box_list[k];
                v->ox = ox;
                v->oz = oz;
                v->dist = dist;
            }
        }
    }
    rlBegin(RL_QUADS);
    for (int i = 0; i < g_vis_count; i++)
        draw_box_solid(&city->boxes[g_vis[i].index], g_vis[i].ox, g_vis[i].oz);
    rlEnd();
}

/* ------------------------------------------------------------------------------------------------ */
/* Traffic                                                                                          */
/* ------------------------------------------------------------------------------------------------ */

#define TRAFFIC_DIST 420.0f
#define SKY_TRAFFIC_DIST 1100.0f

typedef struct Vehicle {
    float x, y, z; /* camera-relative centre */
    float len, wid, hgt;
    int axis; /* 0: long side along x */
    int dir;  /* +1 or -1 along the axis */
    int kind; /* SwcVehicleKind, or -1 for an aircraft */
    float seed;
} Vehicle;

static Vehicle g_veh[MAX_VEHICLES];
static int g_veh_count;

static void vehicle_size(int kind, float *len, float *wid, float *hgt) {
    switch (kind) {
    case SWC_VEHICLE_BUS:
        *len = 11.0f, *wid = 2.5f, *hgt = 3.1f;
        break;
    case SWC_VEHICLE_VAN:
        *len = 5.4f, *wid = 2.0f, *hgt = 2.0f;
        break;
    case -1:
        *len = 7.5f, *wid = 3.4f, *hgt = 1.3f;
        break;
    default:
        *len = 4.5f, *wid = 1.9f, *hgt = 1.4f;
        break;
    }
}

static void collect_traffic(void) {
    const synthwave_city_State *s = F.s;
    const SwcCity *city = &s->city;
    const SwcTraffic *tr = &s->traffic;
    double u = swc_loop_fraction(s);
    g_veh_count = 0;
    for (int lane = 0; lane < city->lane_count; lane++) {
        const SwcLane *l = &city->lanes[lane];
        bool sky = l->kind == SWC_LANE_SKY;
        if (sky ? g_no_air : g_no_street)
            continue;
        float reach = sky ? SKY_TRAFFIC_DIST : TRAFFIC_DIST;
        double cam_across = l->axis == 0 ? F.camz : F.camx, cam_along = l->axis == 0 ? F.camx : F.camz;
        float across = (float)swc_wrap_delta(l->fixed, cam_across, SWC_TILE);
        float lane_y = (float)(l->y - F.camy);
        if (fabsf(across) > reach)
            continue;
        if (!sky && (float)F.camy > 260.0f) /* from high up the streets are too small to see */
            continue;
        const SwcLaneTraffic *lt = &tr->lane[lane];
        float half = sqrtf(fmaxf(0.0f, reach * reach - across * across));
        int span = (int)ceilf(half / (float)lt->spacing) + 1;
        int i0 = swc_vehicle_slot_near(tr, city, lane, u, cam_along);
        for (int k = -span; k <= span; k++) {
            int i = ((i0 + k) % lt->count + lt->count) % lt->count;
            if (span * 2 + 1 > lt->count && k > span - 0) /* a short lane: do not visit a slot twice */
                break;
            if (!swc_vehicle_present(tr, lane, i))
                continue;
            float along = (float)swc_wrap_delta(swc_vehicle_coord(tr, city, lane, i, u), cam_along, SWC_TILE);
            if (fabsf(along) > half + 6.0f)
                continue;
            if (g_veh_count >= MAX_VEHICLES)
                return;
            Vehicle *v = &g_veh[g_veh_count];
            v->axis = l->axis;
            v->dir = l->dir;
            v->seed = (float)swc_vehicle_seed(tr, lane, i);
            v->kind = sky ? -1 : swc_vehicle_kind(tr, lane, i);
            vehicle_size(v->kind, &v->len, &v->wid, &v->hgt);
            float bob = sky ? 1.5f * sinf((float)F.t * 0.8f + v->seed * 40.0f) : 0.0f;
            if (l->axis == 0) {
                v->x = along;
                v->z = across;
            } else {
                v->x = across;
                v->z = along;
            }
            v->y = lane_y + (sky ? bob : v->hgt * 0.5f + 0.1f);
            float r = 0.5f * v->len + 2.0f;
            if (!visible(v->x, v->y, v->z, r))
                continue;
            g_veh_count++;
        }
    }
}

static Rgb vehicle_colour(const Vehicle *v) {
    if (v->kind == SWC_VEHICLE_TAXI)
        return rgb(0.95f, 0.72f, 0.12f);
    if (v->kind == SWC_VEHICLE_EMERGENCY)
        return rgb(0.9f, 0.9f, 0.95f);
    if (v->kind == SWC_VEHICLE_BUS)
        return mixc(rgb(0.3f, 0.35f, 0.5f), F.pal[(int)(v->seed * 8.0f) & 7], 0.25f);
    if (v->kind == -1)
        return mixc(rgb(0.55f, 0.56f, 0.66f), F.pal[(int)(v->seed * 8.0f) & 7], 0.4f);
    return mixc(rgb(0.25f, 0.25f, 0.32f), F.pal[(int)(v->seed * 8.0f) & 7], 0.55f);
}

static void box_faces(float cx, float cy, float cz, float sx, float sy, float sz, Rgb body) {
    float xa = cx - sx * 0.5f, xb = cx + sx * 0.5f, za = cz - sz * 0.5f, zb = cz + sz * 0.5f;
    float y0 = cy - sy * 0.5f, y1 = cy + sy * 0.5f;
    if (xb < 0.0f)
        side_face(xb, zb, xb, za, y0, y1, mulcc(body, F.face[0]), 0.8f);
    if (xa > 0.0f)
        side_face(xa, za, xa, zb, y0, y1, mulcc(body, F.face[1]), 0.8f);
    if (zb < 0.0f)
        side_face(xa, zb, xb, zb, y0, y1, mulcc(body, F.face[2]), 0.8f);
    if (za > 0.0f)
        side_face(xb, za, xa, za, y0, y1, mulcc(body, F.face[3]), 0.8f);
    if (y1 < 0.0f) {
        Rgb lit = mulcc(body, F.face[4]);
        float a[3] = {xa, y1, za}, b[3] = {xb, y1, za}, c[3] = {xb, y1, zb}, d[3] = {xa, y1, zb};
        quad(a, b, c, d, fogged(lit, xa, y1, za), fogged(lit, xb, y1, za), fogged(lit, xb, y1, zb),
             fogged(lit, xa, y1, zb));
    }
}

static void draw_vehicles_solid(void) {
    rlBegin(RL_QUADS);
    for (int i = 0; i < g_veh_count; i++) {
        const Vehicle *v = &g_veh[i];
        Rgb body = vehicle_colour(v);
        float sx = v->axis == 0 ? v->len : v->wid, sz = v->axis == 0 ? v->wid : v->len;
        if (v->kind == -1) { /* aircraft are lit brighter than cars so they read against the sky */
            box_faces(v->x, v->y, v->z, sx, v->hgt, sz, mulc(body, 1.9f));
            continue;
        }
        float low = v->hgt * 0.62f;
        box_faces(v->x, v->y - (v->hgt - low) * 0.5f, v->z, sx, low, sz, body);
        if (v->kind != SWC_VEHICLE_BUS && rel_dist(v->x, v->y, v->z) < 160.0f) { /* a cabin on top */
            float cl = v->len * 0.5f, cw = v->wid * 0.86f, ch = v->hgt - low;
            float csx = v->axis == 0 ? cl : cw, csz = v->axis == 0 ? cw : cl;
            box_faces(v->x, v->y + low * 0.5f, v->z, csx, ch, csz, mulc(body, 0.55f));
        }
    }
    rlEnd();
}

static void add_vehicle_lights(void) {
    for (int i = 0; i < g_veh_count; i++) {
        const Vehicle *v = &g_veh[i];
        float d = rel_dist(v->x, v->y, v->z);
        float fx = v->axis == 0 ? (float)v->dir : 0.0f, fz = v->axis == 1 ? (float)v->dir : 0.0f;
        float half = v->len * 0.5f;
        if (v->kind == -1) { /* aircraft: red and green wing lights, a white strobe on the beat */
            float wx = v->axis == 1 ? 1.0f : 0.0f, wz = v->axis == 0 ? 1.0f : 0.0f;
            glow(v->x + wx * v->wid * 0.5f, v->y, v->z + wz * v->wid * 0.5f, 1.4f, rgb(1.0f, 0.15f, 0.2f),
                 0.9f);
            glow(v->x - wx * v->wid * 0.5f, v->y, v->z - wz * v->wid * 0.5f, 1.4f, rgb(0.2f, 1.0f, 0.35f),
                 0.9f);
            glow(v->x - fx * half, v->y, v->z - fz * half, 2.2f, rgb(1.0f, 0.75f, 0.5f),
                 0.5f + 0.5f * F.pulse);
            continue;
        }
        float lat = v->wid * 0.32f;
        float wx = v->axis == 1 ? 1.0f : 0.0f, wz = v->axis == 0 ? 1.0f : 0.0f;
        int n = d < 90.0f ? 2 : 1;
        for (int k = 0; k < n; k++) {
            float side = n == 1 ? 0.0f : (k == 0 ? lat : -lat);
            float hy = v->y - v->hgt * 0.1f;
            glow(v->x + fx * half + wx * side, hy, v->z + fz * half + wz * side, 0.9f,
                 rgb(1.0f, 0.93f, 0.75f), 0.8f);
            glow(v->x - fx * half + wx * side, hy, v->z - fz * half + wz * side, 0.75f,
                 rgb(1.0f, 0.12f, 0.1f), 0.8f);
        }
        if (v->kind == SWC_VEHICLE_EMERGENCY) {
            bool red = fmodf((float)F.t * 4.0f + v->seed * 7.0f, 1.0f) < 0.5f;
            glow(v->x, v->y + v->hgt * 0.55f, v->z, 2.6f,
                 red ? rgb(1.0f, 0.1f, 0.15f) : rgb(0.2f, 0.4f, 1.0f), 1.0f);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* The additive pass: neon, windows, signs, beams                                                   */
/* ------------------------------------------------------------------------------------------------ */

static void line3(const float a[3], const float b[3], Rgb c, float alpha_a, float alpha_b) {
    vtx(a[0], a[1], a[2], col(c, alpha_a));
    vtx(b[0], b[1], b[2], col(c, alpha_b));
}

/* Block outlines, avenue centre lines (the ground's) and the roof trims of nearby buildings. */
static void draw_neon_lines(void) {
    const SwcCity *city = &F.s->city;
    const MoodStyle *m = F.mood;
    float gy = (float)-F.camy + 0.08f;
    rlSetLineWidth(1.5f);
    rlBegin(RL_LINES);
    int line_cells = g_no_ground ? 0 : g_cell_count;
    for (int i = 0; i < line_cells; i++) {
        const VisCell *vc = &g_cells[i];
        int cx = vc->index % SWC_CELLS, cz = vc->index / SWC_CELLS;
        const SwcCell *cell = &city->cells[vc->index];
        SwcRect blk = swc_block_rect(cx, cz);
        float x0 = vc->rx + (float)(blk.x0 - cx * SWC_CELL), x1 = vc->rx + (float)(blk.x1 - cx * SWC_CELL);
        float z0 = vc->rz + (float)(blk.z0 - cz * SWC_CELL), z1 = vc->rz + (float)(blk.z1 - cz * SWC_CELL);
        Rgb c = mulc(F.pal[cell->palette & 7], 0.55f * m->glow * (0.8f + 0.2f * F.pulse));
        float p[4][3] = {{x0, gy, z0}, {x1, gy, z0}, {x1, gy, z1}, {x0, gy, z1}};
        for (int k = 0; k < 4; k++) {
            const float *a = p[k], *b = p[(k + 1) % 4];
            line3(a, b, c, 0.9f * fade(a[0], gy, a[2]), 0.9f * fade(b[0], gy, b[2]));
        }
        /* the road centre line through this cell's corner: avenue = warm yellow, street = violet */
        int gx = cx, gz = cz;
        bool avx = swc_is_avenue(gx), avz = swc_is_avenue(gz);
        Rgb cl_x = avz ? mulc(F.pal[4], 0.5f) : mulc(F.pal[2], 0.35f);
        Rgb cl_z = avx ? mulc(F.pal[4], 0.5f) : mulc(F.pal[2], 0.35f);
        float zc = vc->rz, xc = vc->rx;
        float a1[3] = {vc->rx, gy, zc}, b1[3] = {vc->rx + (float)SWC_CELL, gy, zc};
        float a2[3] = {xc, gy, vc->rz}, b2[3] = {xc, gy, vc->rz + (float)SWC_CELL};
        line3(a1, b1, cl_x, 0.8f * fade(a1[0], gy, a1[2]), 0.8f * fade(b1[0], gy, b1[2]));
        line3(a2, b2, cl_z, 0.8f * fade(a2[0], gy, a2[2]), 0.8f * fade(b2[0], gy, b2[2]));
    }
    /* roof trims */
    for (int i = 0; i < g_vis_count; i++) {
        const VisBox *vb = &g_vis[i];
        if (vb->dist > TRIM_DIST)
            continue;
        const SwcBox *b = &city->boxes[vb->index];
        if (!(b->flags & SWC_FLAG_ROOF) || (b->flags & SWC_FLAG_ROUND) || b->style == SWC_STYLE_SPIRE)
            continue;
        float cx = b->cx + vb->ox, cz = b->cz + vb->oz, y = b->y0 + b->sy - (float)F.camy;
        float hx = b->sx * 0.5f, hz = b->sz * 0.5f;
        Rgb c = mulc(F.pal[(b->seed > 0.5f ? b->color_a : b->color_b) & 7],
                     0.85f * m->glow * (0.7f + 0.3f * F.pulse));
        float p[4][3] = {
            {cx - hx, y, cz - hz}, {cx + hx, y, cz - hz}, {cx + hx, y, cz + hz}, {cx - hx, y, cz + hz}};
        for (int k = 0; k < 4; k++) {
            const float *a = p[k], *bb = p[(k + 1) % 4];
            line3(a, bb, c, fade(a[0], y, a[2]), fade(bb[0], y, bb[2]));
        }
    }
    rlEnd();
    rlSetLineWidth(1.0f);
}

static int window_texture(const SwcBox *b) {
    switch (b->style) {
    case SWC_STYLE_PODIUM:
    case SWC_STYLE_SHAFT:
        return 0;
    case SWC_STYLE_GLASS:
        return 1;
    case SWC_STYLE_MEGA: /* big windows: the megatowers are seen from far away */
        return 0;
    case SWC_STYLE_SLAB:
        return 2;
    case SWC_STYLE_STRIP:
        return 3;
    default:
        return -1;
    }
}

static Rgb window_colour(const SwcBox *b) {
    float v = b->seed;
    Rgb c;
    if (v < 0.45f)
        c = rgb(1.0f, 0.80f, 0.50f);
    else if (v < 0.70f)
        c = mixc(F.pal[b->color_a & 7], rgb(1, 1, 1), 0.25f);
    else if (v < 0.88f)
        c = mixc(F.pal[b->color_b & 7], rgb(1, 1, 1), 0.2f);
    else
        c = rgb(0.75f, 0.92f, 1.0f);
    return mulc(c, F.mood->windows);
}

/* A textured, additive overlay on a vertical face: the lit windows. */
static void window_face(float x0, float z0, float x1, float z1, float y0, float y1, float u0, float u1,
                        float v0, float v1, float nx, float nz, Rgb c, float push) {
    float ax = x0 + nx * push, az = z0 + nz * push, bx = x1 + nx * push, bz = z1 + nz * push;
    float fa = fade(ax, y0, az), fb = fade(bx, y0, bz);
    float dist = rel_dist(0.5f * (ax + bx), 0.5f * (y0 + y1), 0.5f * (az + bz));
    float near_fade = 1.0f - smooth(WINDOW_DIST * 0.55f, WINDOW_DIST, dist);
    Color ca = col(c, fa * near_fade), cb = col(c, fb * near_fade);
    tex_vtx(ax, y0, az, u0, v1, ca);
    tex_vtx(bx, y0, bz, u1, v1, cb);
    tex_vtx(bx, y1, bz, u1, v0, cb);
    tex_vtx(ax, y1, az, u0, v0, ca);
}

static void draw_windows(void) {
    const SwcCity *city = &F.s->city;
    for (int tex = 0; tex < WINDOW_TEXTURES; tex++) {
        float su = 1.0f / TEX_SPAN[tex][0], sv = 1.0f / TEX_SPAN[tex][1];
        rlSetTexture(g_window_tex[tex].id);
        rlBegin(RL_QUADS);
        for (int i = 0; i < g_vis_count; i++) {
            const VisBox *vb = &g_vis[i];
            if (vb->dist > WINDOW_DIST)
                continue;
            const SwcBox *b = &city->boxes[vb->index];
            if (window_texture(b) != tex || (b->flags & SWC_FLAG_ROUND))
                continue;
            float cx = b->cx + vb->ox, cz = b->cz + vb->oz;
            float hx = b->sx * 0.5f, hz = b->sz * 0.5f;
            float y0 = b->y0 - (float)F.camy, y1 = y0 + b->sy;
            float push = 0.06f + vb->dist * 0.0006f;
            float v0 = b->y0 * sv, v1 = (b->y0 + b->sy) * sv;
            Rgb c = window_colour(b);
            float xa = cx - hx, xb = cx + hx, za = cz - hz, zb = cz + hz;
            float tx0 = b->cx - hx, tx1 = b->cx + hx, tz0 = b->cz - hz,
                  tz1 = b->cz + hz; /* tile coordinates: the texture is anchored to the world */
            if (xb < 0.0f)
                window_face(xb, zb, xb, za, y0, y1, tz1 * su, tz0 * su, v0, v1, 1.0f, 0.0f, c, push);
            if (xa > 0.0f)
                window_face(xa, za, xa, zb, y0, y1, tz0 * su, tz1 * su, v0, v1, -1.0f, 0.0f, c, push);
            if (zb < 0.0f)
                window_face(xa, zb, xb, zb, y0, y1, tx0 * su, tx1 * su, v0, v1, 0.0f, 1.0f, c, push);
            if (za > 0.0f)
                window_face(xb, za, xa, za, y0, y1, tx1 * su, tx0 * su, v0, v1, 0.0f, -1.0f, c, push);
        }
        rlEnd();
        rlSetTexture(0);
    }
}

/* Lit crowns and spires get a halo; lit megatowers a faint one on their tops. */
static void add_building_glows(void) {
    const SwcCity *city = &F.s->city;
    for (int i = 0; i < g_vis_count; i++) {
        const VisBox *vb = &g_vis[i];
        const SwcBox *b = &city->boxes[vb->index];
        if (b->style != SWC_STYLE_CROWN && b->style != SWC_STYLE_SPIRE)
            continue;
        float cx = b->cx + vb->ox, cz = b->cz + vb->oz, cy = b->y0 + b->sy * 0.5f - (float)F.camy;
        float size = b->style == SWC_STYLE_SPIRE ? 4.0f : fmaxf(b->sx, b->sz) * 0.9f;
        glow(cx, cy, cz, size, F.pal[b->color_a & 7], 0.55f * (0.7f + 0.3f * F.pulse));
        if (b->style == SWC_STYLE_SPIRE) /* the aircraft beacon at the tip */
            glow(cx, b->y0 + b->sy - (float)F.camy, cz, 6.0f, F.pal[6], F.pulse > 0.5f ? 1.0f : 0.25f);
    }
}

typedef struct SiteView {
    float x, y, z; /* camera-relative */
    float dist;
} SiteView;

static void beam(const float from[3], const float dir[3], float length, float width0, float width1, Rgb c,
                 float alpha) {
    /* two crossed ribbons so the beam has body from any angle */
    for (int k = 0; k < 2; k++) {
        float side[3];
        if (k == 0) {
            side[0] = F.right[0], side[1] = F.right[1], side[2] = F.right[2];
        } else {
            side[0] = F.up[0], side[1] = F.up[1], side[2] = F.up[2];
        }
        float a0[3], a1[3], b0[3], b1[3];
        for (int i = 0; i < 3; i++) {
            a0[i] = from[i] - side[i] * width0;
            a1[i] = from[i] + side[i] * width0;
            b0[i] = from[i] + dir[i] * length - side[i] * width1;
            b1[i] = from[i] + dir[i] * length + side[i] * width1;
        }
        float fa = fade(from[0], from[1], from[2]), fb = fade(b0[0], b0[1], b0[2]);
        Color ca = col(c, alpha * fa), cb = col(c, 0.0f * fb);
        quad(a0, a1, b1, b0, ca, ca, cb, cb);
    }
}

static void draw_sites(void) {
    const synthwave_city_State *s = F.s;
    const SwcCity *city = &s->city;
    float t = (float)F.t;
    /* the first pass collects what is in view; beams and panels are drawn as additive quads, wire things as
     * lines, lights as glow sprites */
    rlBegin(RL_QUADS);
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *st = &city->sites[i];
        float rx = (float)swc_wrap_delta(st->x, F.camx, SWC_TILE),
              rz = (float)swc_wrap_delta(st->z, F.camz, SWC_TILE);
        float ry = (float)(st->y - F.camy);
        float d = rel_dist(rx, ry, rz);
        Rgb c = F.pal[st->color & 7];
        switch (st->kind) {
        case SWC_SITE_HOLO_PANEL: {
            if (g_no_holo || d > 650.0f || !visible(rx, ry, rz, (float)st->w))
                break;
            float yaw = (float)st->yaw;
            float ex = cosf(yaw) * (float)st->w * 0.5f, ez = -sinf(yaw) * (float)st->w * 0.5f,
                  eh = (float)st->h * 0.5f;
            float flicker = 0.75f + 0.25f * sinf(t * 7.0f + (float)st->seed * 90.0f);
            float a[3] = {rx - ex, ry - eh, rz - ez}, b[3] = {rx + ex, ry - eh, rz + ez},
                  cc[3] = {rx + ex, ry + eh, rz + ez}, dd[3] = {rx - ex, ry + eh, rz - ez};
            float al = 0.30f * flicker * fade(rx, ry, rz) * F.mood->glow;
            Color lo = col(c, al * 1.4f), hi = col(mulc(c, 0.7f), al * 0.6f);
            quad(a, b, cc, dd, lo, lo, hi, hi);
            break;
        }
        case SWC_SITE_SEARCHLIGHT: {
            if (g_no_beams || d > 1500.0f || !visible(rx, ry, rz, (float)st->w))
                break;
            int beams = (int)st->h < 1 ? 1 : ((int)st->h > 3 ? 3 : (int)st->h);
            for (int k = 0; k < beams; k++) {
                float az = (float)st->seed * ARC_TAU +
                           t * (0.28f + 0.1f * (float)k) * ((i & 1) ? 1.0f : -1.0f) +
                           (float)k * ARC_TAU / (float)beams;
                float el = 1.0f + 0.35f * sinf(t * 0.5f + (float)st->seed * 20.0f + (float)k);
                float dir[3] = {cosf(el) * sinf(az), sinf(el), cosf(el) * cosf(az)};
                float from[3] = {rx, ry, rz};
                beam(from, dir, (float)st->w, 0.8f, 7.0f, c, 0.55f);
            }
            break;
        }
        case SWC_SITE_PILLAR: {
            if (g_no_beams || d > 900.0f ||
                !visible(rx, ry + (float)st->h * 0.5f, rz, (float)st->h * 0.5f + 4.0f))
                break;
            float r = (float)st->w, top = (float)st->h;
            float al = 0.85f * fade(rx, ry, rz);
            Color lo = col(mulc(c, 0.35f), al), hi = col(mulc(c, 1.0f), al);
            float p[4][2] = {{-r, -r}, {r, -r}, {r, r}, {-r, r}};
            for (int k = 0; k < 4; k++) {
                const float *p0 = p[k], *p1 = p[(k + 1) % 4];
                float a[3] = {rx + p0[0], ry, rz + p0[1]}, b[3] = {rx + p1[0], ry, rz + p1[1]},
                      cc[3] = {rx + p1[0], ry + top, rz + p1[1]}, dd[3] = {rx + p0[0], ry + top, rz + p0[1]};
                quad(a, b, cc, dd, lo, lo, hi, hi);
            }
            break;
        }
        case SWC_SITE_TICKER: {
            if (g_no_holo || d > 700.0f || !visible(rx, ry, rz, (float)fmax(st->w, st->d) + 6.0f))
                break;
            float hw = (float)st->w, hd = (float)st->d, hh = (float)st->h * 0.5f;
            float pts[5][2] = {{-hw, -hd}, {hw, -hd}, {hw, hd}, {-hw, hd}, {-hw, -hd}};
            float al = 0.45f * fade(rx, ry, rz) * F.mood->glow;
            Color cc = col(c, al);
            for (int k = 0; k < 4; k++) {
                float a[3] = {rx + pts[k][0], ry - hh, rz + pts[k][1]},
                      b[3] = {rx + pts[k + 1][0], ry - hh, rz + pts[k + 1][1]},
                      e[3] = {rx + pts[k + 1][0], ry + hh, rz + pts[k + 1][1]},
                      f[3] = {rx + pts[k][0], ry + hh, rz + pts[k][1]};
                quad(a, b, e, f, cc, cc, cc, cc);
            }
            break;
        }
        default:
            break;
        }
    }
    rlEnd();

    /* wire things: lasers and rotating holo objects */
    rlSetLineWidth(2.0f);
    rlBegin(RL_LINES);
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *st = &city->sites[i];
        if (st->kind != SWC_SITE_LASER && st->kind != SWC_SITE_HOLO_OBJECT && st->kind != SWC_SITE_HOLO_PANEL)
            continue;
        if (g_no_beams && st->kind == SWC_SITE_LASER)
            continue;
        if (g_no_holo && st->kind != SWC_SITE_LASER)
            continue;
        float rx = (float)swc_wrap_delta(st->x, F.camx, SWC_TILE),
              rz = (float)swc_wrap_delta(st->z, F.camz, SWC_TILE);
        float ry = (float)(st->y - F.camy);
        float d = rel_dist(rx, ry, rz);
        Rgb c = F.pal[st->color & 7];
        if (st->kind == SWC_SITE_LASER) {
            if (d > 1200.0f || !visible(rx, ry, rz, (float)st->w))
                continue;
            int beams = (int)st->h < 2 ? 2 : ((int)st->h > 9 ? 9 : (int)st->h);
            for (int k = 0; k < beams; k++) {
                float spread = ((float)k / (float)(beams - 1) - 0.5f) * 1.4f;
                float az = (float)st->seed * ARC_TAU + 0.6f * sinf(t * 0.7f + (float)st->seed * 30.0f) +
                           spread * 0.5f;
                float el = 0.9f + spread * 0.35f + 0.15f * sinf(t * 1.3f + (float)k);
                float dir[3] = {cosf(el) * sinf(az), sinf(el), cosf(el) * cosf(az)};
                float a[3] = {rx, ry, rz}, b[3] = {rx + dir[0] * (float)st->w, ry + dir[1] * (float)st->w,
                                                   rz + dir[2] * (float)st->w};
                line3(a, b, c, 0.9f * fade(rx, ry, rz), 0.0f);
            }
        } else if (st->kind == SWC_SITE_HOLO_OBJECT) {
            float r = (float)st->w;
            if (d > 650.0f || !visible(rx, ry, rz, r * 1.5f))
                continue;
            float rot = t * 0.9f + (float)st->seed * 6.0f, cs = cosf(rot), sn = sinf(rot);
            float v[6][3] = {{r, 0, 0},         {-r, 0, 0}, {0, r * 1.3f, 0},
                             {0, -r * 1.3f, 0}, {0, 0, r},  {0, 0, -r}};
            for (int k = 0; k < 6; k++) {
                float x = v[k][0] * cs - v[k][2] * sn, z = v[k][0] * sn + v[k][2] * cs;
                v[k][0] = rx + x;
                v[k][1] = ry + v[k][1];
                v[k][2] = rz + z;
            }
            static const int edges[12][2] = {{0, 2}, {2, 1}, {1, 3}, {3, 0}, {0, 4}, {4, 1},
                                             {1, 5}, {5, 0}, {2, 4}, {4, 3}, {3, 5}, {5, 2}};
            for (int k = 0; k < 12; k++)
                line3(v[edges[k][0]], v[edges[k][1]], c, 0.9f * fade(rx, ry, rz), 0.9f * fade(rx, ry, rz));
        } else if (d < 450.0f && visible(rx, ry, rz, (float)st->w)) { /* panel frame and fake text lines */
            float yaw = (float)st->yaw;
            float ex = cosf(yaw) * (float)st->w * 0.5f, ez = -sinf(yaw) * (float)st->w * 0.5f,
                  eh = (float)st->h * 0.5f;
            float al = 0.9f * fade(rx, ry, rz);
            float corners[4][3] = {{rx - ex, ry - eh, rz - ez},
                                   {rx + ex, ry - eh, rz + ez},
                                   {rx + ex, ry + eh, rz + ez},
                                   {rx - ex, ry + eh, rz - ez}};
            for (int k = 0; k < 4; k++)
                line3(corners[k], corners[(k + 1) % 4], c, al, al);
            for (int k = 1; k < 5; k++) {
                float yy = ry - eh + (float)st->h * (float)k / 5.0f;
                float frac = 0.35f + 0.5f * (float)swc_hash_unit3((uint32_t)i, (uint32_t)k, 3u);
                float a[3] = {rx - ex * 0.8f, yy, rz - ez * 0.8f},
                      b[3] = {rx - ex * 0.8f + ex * 1.6f * frac, yy, rz - ez * 0.8f + ez * 1.6f * frac};
                line3(a, b, c, al * 0.7f, al * 0.7f);
            }
        }
    }
    rlEnd();
    rlSetLineWidth(1.0f);

    /* lights: roof lights blink, pillars and tickers shine */
    for (int i = 0; i < city->site_count; i++) {
        const SwcSite *st = &city->sites[i];
        float rx = (float)swc_wrap_delta(st->x, F.camx, SWC_TILE),
              rz = (float)swc_wrap_delta(st->z, F.camz, SWC_TILE);
        float ry = (float)(st->y - F.camy);
        float d = rel_dist(rx, ry, rz);
        Rgb c = F.pal[st->color & 7];
        switch (st->kind) {
        case SWC_SITE_ROOF_LIGHT: {
            if (g_no_buildings || d > 1400.0f || !visible(rx, ry, rz, 4.0f))
                break;
            float ph = fmodf(t * 0.5f + (float)st->seed * 3.0f, 1.0f);
            float on = ph < 0.5f ? 1.0f : 0.2f;
            glow(rx, ry, rz, fmaxf(1.6f, (float)st->w * 1.5f), c, on * (0.55f + 0.45f * F.pulse));
            break;
        }
        case SWC_SITE_PILLAR:
            if (!g_no_beams && d < 900.0f && visible(rx, ry + (float)st->h, rz, 6.0f))
                glow(rx, ry + (float)st->h, rz, 5.0f, c, 0.7f);
            break;
        case SWC_SITE_SEARCHLIGHT:
        case SWC_SITE_LASER:
            if (!g_no_beams && d < 1200.0f && visible(rx, ry, rz, 4.0f))
                glow(rx, ry, rz, 3.0f, c, 0.8f);
            break;
        case SWC_SITE_HOLO_OBJECT:
            if (!g_no_holo && d < 650.0f && visible(rx, ry, rz, (float)st->w * 2.0f))
                glow(rx, ry, rz, (float)st->w * 1.8f, c, 0.35f + 0.2f * F.pulse);
            break;
        case SWC_SITE_TICKER: {
            if (g_no_holo || d > 700.0f || !visible(rx, ry, rz, (float)fmax(st->w, st->d) + 6.0f))
                break;
            float hw = (float)st->w, hd = (float)st->d;
            float per = 4.0f * (hw + hd);
            float pos = fmodf(t * 22.0f + (float)st->seed * per, per);
            for (int k = 0; k < 4; k++) { /* a bright head with a short tail running round the ring */
                float p = fmodf(pos - (float)k * 4.0f + per, per);
                float x, z;
                if (p < 2.0f * hw) {
                    x = -hw + p, z = -hd;
                } else if (p < 2.0f * hw + 2.0f * hd) {
                    x = hw, z = -hd + (p - 2.0f * hw);
                } else if (p < 4.0f * hw + 2.0f * hd) {
                    x = hw - (p - 2.0f * hw - 2.0f * hd), z = hd;
                } else {
                    x = -hw, z = hd - (p - 4.0f * hw - 2.0f * hd);
                }
                glow(rx + x, ry, rz + z, (float)st->h * (0.7f - 0.12f * (float)k), c, 0.9f - 0.2f * (float)k);
            }
            break;
        }
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* The frame                                                                                        */
/* ------------------------------------------------------------------------------------------------ */

void swc_render_scene(const synthwave_city_State *s, double time_s) {
    swc_render_scene_legacy(s, time_s, NULL);
}

void swc_live_layers_set(unsigned mask) {
    g_live_layers = mask & SWC_LIVE_ALL;
}

unsigned swc_live_layers(void) {
    return g_live_layers;
}

void swc_render_scene_legacy(const synthwave_city_State *s, double time_s, const SwcLegacyOptions *opt) {
    if (!g_ready)
        return;
    g_opt = opt;
    g_linear_out = opt != NULL && (opt->flags & SWC_LEGACY_LINEAR) != 0 && opt->look != NULL;
    g_no_buildings = opt != NULL && (opt->flags & SWC_LEGACY_NO_BUILDINGS) != 0;
    g_no_beams =
        (opt != NULL && (opt->flags & SWC_LEGACY_NO_BEAMS) != 0) || (g_live_layers & SWC_LIVE_BEAMS) == 0;
    g_no_ground = opt != NULL && (opt->flags & SWC_LEGACY_NO_GROUND) != 0;
    g_no_air = (opt != NULL && (opt->flags & SWC_LEGACY_NO_AIR) != 0) || (g_live_layers & SWC_LIVE_AIR) == 0;
    g_no_street =
        (opt != NULL && (opt->flags & SWC_LEGACY_NO_STREET) != 0) || (g_live_layers & SWC_LIVE_STREET) == 0;
    g_no_holo =
        (opt != NULL && (opt->flags & SWC_LEGACY_NO_HOLO) != 0) || (g_live_layers & SWC_LIVE_HOLO) == 0;
    setup_frame(s, time_s);
    if (opt == NULL || (opt->flags & SWC_LEGACY_NO_SKY) == 0)
        draw_sky();

    double near_plane = rlGetCullDistanceNear(), far_plane = rlGetCullDistanceFar();
    rlSetClipPlanes(0.4, FAR_PLANE);
    Camera3D cam;
    cam.position = (Vector3){0.0f, 0.0f, 0.0f};
    cam.target = (Vector3){F.fwd[0], F.fwd[1], F.fwd[2]};
    /* Without a roll, raylib's own lookAt gets the world up, exactly as before the camera port (the picture
     * is then bit for bit the old one); a roll hands it the leaning up instead. */
    cam.up = F.roll != 0.0f ? (Vector3){F.up[0], F.up[1], F.up[2]} : (Vector3){0.0f, 1.0f, 0.0f};
    cam.fovy = (float)s->fov;
    cam.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(cam);
    rlDisableBackfaceCulling(); /* faces are culled by hand, and the overlays need both sides */

    collect_cells();
    collect_traffic();
    if (!g_no_ground)
        draw_ground();
    if (!g_no_buildings) /* no visible boxes means no window overlays, roof trims or building glows either */
        draw_buildings_solid();
    draw_vehicles_solid();
    if (opt != NULL && opt->after_solid != NULL) {
        rlDrawRenderBatchActive();
        opt->after_solid(opt->user, F.w, F.h);
    }

    /* everything that glows: depth-tested against the solids, never written to the depth buffer */
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    draw_neon_lines();
    draw_windows();
    add_building_glows();
    add_vehicle_lights();
    draw_sites();
    flush_sprites();
    EndBlendMode();
    rlEnableDepthMask();

    rlEnableBackfaceCulling();
    EndMode3D();
    rlSetClipPlanes(near_plane, far_plane);
    g_opt = NULL;
    g_linear_out = false;
    g_no_buildings = false;
    g_no_beams = false;
    g_no_ground = false;
    g_no_air = false;
    g_no_street = false;
    g_no_holo = false;
}
