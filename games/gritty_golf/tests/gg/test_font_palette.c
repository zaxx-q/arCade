/*
 * test_font_palette.c - the baked bitmap font and the three-colour palette
 *
 * These tests make sure that
 *   - the font files are well formed and exactly pixel-aligned (no blur possible),
 *   - every piece of UI text fits where it is drawn,
 *   - the palette is readable (WCAG contrast) whatever colours someone picks.
 */
#include <stdio.h>
#include <string.h>
#include "test_framework.h"
#include "bmfont.h"
#include "level.h"
#include "palette.h"
#include "util.h"

/* [arCade patch 3] arCade ships the assets in assets/gritty_golf/ (CMake passes the folder). */
#ifndef GG_ASSETS_DIR
#define GG_ASSETS_DIR "."
#endif

static const char *font_path(const char *name)
{
    static char path[400];
    snprintf(path, sizeof path, "%s/font/%s", GG_ASSETS_DIR, name);
    return path;
}

/* width and height of a PNG file: they sit at fixed byte positions of the header */
static bool png_size(const char *path, int *w, int *h)
{
    unsigned char hdr[24];
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return false;
    size_t n = fread(hdr, 1, sizeof hdr, fp);
    fclose(fp);
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (n != sizeof hdr || memcmp(hdr, sig, 8) != 0)
        return false;
    *w = (hdr[16] << 24) | (hdr[17] << 16) | (hdr[18] << 8) | hdr[19];     /* big-endian */
    *h = (hdr[20] << 24) | (hdr[21] << 16) | (hdr[22] << 8) | hdr[23];
    return true;
}

static void write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "w");
    if (fp) { fputs(text, fp); fclose(fp); }
}

/* ---- font files ------------------------------------------------------------------- */
static void test_fonts_load(void)
{
    BakedFont f8, f16;
    CHECK(Bmfont_Load(&f8, font_path("silkscreen-8.fnt")));
    CHECK(Bmfont_Load(&f16, font_path("silkscreen-16.fnt")));
    CHECK_INT(f8.size, 8);
    CHECK_INT(f16.size, 16);
    CHECK_INT(f8.cap_height, 5);
    CHECK_INT(f16.cap_height, 10);
    CHECK_STR(f8.atlas, "silkscreen-8.png");
    CHECK_STR(f16.atlas, "silkscreen-16.png");
    CHECK_INT(f8.count, 95);                                   /* printable ASCII 32..126 */
    CHECK_INT(f16.count, 95);
    for (int i = 0; i < f8.count; i++) {                        /* sorted, contiguous */
        CHECK_INT(f8.glyphs[i].codepoint, 32 + i);
        CHECK_INT(f16.glyphs[i].codepoint, 32 + i);
    }
}

static void test_glyphs_inside_atlas_and_disjoint(void)
{
    static const char *names[2][2] = { { "silkscreen-8.fnt", "silkscreen-8.png" },
                                       { "silkscreen-16.fnt", "silkscreen-16.png" } };
    for (int k = 0; k < 2; k++) {
        BakedFont f;
        CHECK(Bmfont_Load(&f, font_path(names[k][0])));
        int aw = 0, ah = 0;
        CHECK(png_size(font_path(names[k][1]), &aw, &ah));
        CHECK(aw > 0 && ah > 0);

        for (int i = 0; i < f.count; i++) {
            const BakedGlyph *g = &f.glyphs[i];
            CHECK(g->w >= 0 && g->h >= 0);
            CHECK(g->x >= 0 && g->y >= 0);
            CHECK(g->x + g->w <= aw);                           /* never reads outside the picture */
            CHECK(g->y + g->h <= ah);
            CHECK(g->advance > 0);
            for (int j = i + 1; j < f.count; j++) {             /* rectangles must not overlap */
                const BakedGlyph *o = &f.glyphs[j];
                if (g->w == 0 || o->w == 0) continue;
                bool apart = g->x + g->w <= o->x || o->x + o->w <= g->x ||
                             g->y + g->h <= o->y || o->y + o->h <= g->y;
                CHECK(apart);
            }
        }
    }
}

static void test_find_and_fallback(void)
{
    BakedFont f;
    CHECK(Bmfont_Load(&f, font_path("silkscreen-8.fnt")));

    const BakedGlyph *a = Bmfont_Find(&f, 'A');
    CHECK_INT(a->codepoint, 'A');
    CHECK(a->w > 0 && a->h > 0);
    CHECK_INT(Bmfont_Find(&f, ' ')->codepoint, ' ');
    CHECK_INT(Bmfont_Find(&f, ' ')->w, 0);                      /* a space has no pixels, only an advance */
    CHECK(Bmfont_Find(&f, ' ')->advance > 0);

    for (int cp = 32; cp <= 126; cp++)                          /* binary search finds EVERY glyph */
        CHECK_INT(Bmfont_Find(&f, cp)->codepoint, cp);

    CHECK_INT(Bmfont_Find(&f, 200)->codepoint, '?');            /* unknown -> '?' */
    CHECK_INT(Bmfont_Find(&f, 0x20AC)->codepoint, '?');
    CHECK_INT(Bmfont_Find(&f, 0)->codepoint, '?');
    CHECK_INT(Bmfont_Find(&f, -5)->codepoint, '?');
    CHECK_INT(Bmfont_Find(&f, 31)->codepoint, '?');             /* just below the table */
    CHECK_INT(Bmfont_Find(&f, 127)->codepoint, '?');            /* just above it */
}

static void test_width(void)
{
    BakedFont f;
    CHECK(Bmfont_Load(&f, font_path("silkscreen-8.fnt")));
    CHECK_INT(Bmfont_Width(&f, ""), 0);
    CHECK_INT(Bmfont_Width(&f, "A"), Bmfont_Find(&f, 'A')->advance);
    CHECK_INT(Bmfont_Width(&f, "AB"), Bmfont_Find(&f, 'A')->advance + Bmfont_Find(&f, 'B')->advance);
    CHECK_INT(Bmfont_Width(&f, "A B"), Bmfont_Width(&f, "A") + Bmfont_Width(&f, " ") + Bmfont_Width(&f, "B"));
    CHECK_INT(Bmfont_Width(&f, "\xC3\xA9"), 2 * Bmfont_Find(&f, '?')->advance);  /* 2 unknown bytes */
    CHECK(Bmfont_Width(&f, "WWW") > Bmfont_Width(&f, "iii"));   /* proportional font */
    CHECK_INT(Bmfont_Width(&f, "AAAA"), 4 * Bmfont_Width(&f, "A"));
}

/* THE reason text is crisp: Silkscreen is built on an 8 px grid, so the 16 px font is
 * EXACTLY the 8 px font doubled, glyph by glyph.  Nothing is ever fractional. */
static void test_16_is_exact_double_of_8(void)
{
    BakedFont f8, f16;
    CHECK(Bmfont_Load(&f8, font_path("silkscreen-8.fnt")));
    CHECK(Bmfont_Load(&f16, font_path("silkscreen-16.fnt")));
    for (int cp = 32; cp <= 126; cp++) {
        const BakedGlyph *a = Bmfont_Find(&f8, cp), *b = Bmfont_Find(&f16, cp);
        CHECK_INT(b->advance, 2 * a->advance);
        CHECK_INT(b->w, 2 * a->w);
        CHECK_INT(b->h, 2 * a->h);
        CHECK_INT(b->xoff, 2 * a->xoff);
        if (a->w > 0)
            CHECK_INT(b->yoff, 2 * a->yoff);
    }
    CHECK_INT(f16.cap_height, 2 * f8.cap_height);
}

static void test_parser_rejects_bad_files(void)
{
    BakedFont f;
    const char *p = TmpPath("font");
    TmpRemove("font");
    CHECK(!Bmfont_Load(&f, p));                                  /* missing file */

    write_text(p, "FONT 8 7 5 2 a.png\n63 0 0 3 5 0 0 4\n65 4 0 3 5 0 0 4\n");
    CHECK(Bmfont_Load(&f, p));                                   /* the smallest valid font: has '?' */
    CHECK_INT(f.count, 2);
    CHECK_INT(Bmfont_Find(&f, 65)->codepoint, 65);
    CHECK_INT(Bmfont_Find(&f, 66)->codepoint, 63);

    write_text(p, "# comment\n\nFONT 8 7 5 2 a.png\r\n63 0 0 3 5 0 0 4\r\n\r\n65 4 0 3 5 0 0 4\r\n");
    CHECK(Bmfont_Load(&f, p));                                   /* comments, blank lines, CRLF */

    write_text(p, "63 0 0 3 5 0 0 4\n");                         /* glyph before the header */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 2 a.png\n65 4 0 3 5 0 0 4\n63 0 0 3 5 0 0 4\n");   /* NOT sorted */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 2 a.png\n63 0 0 3 5 0 0 4\n63 4 0 3 5 0 0 4\n");   /* duplicate code */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 2 a.png\n65 0 0 3 5 0 0 4\n66 4 0 3 5 0 0 4\n");   /* no '?' fallback */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 2 a.png\n63 0 0 3 5 0 0\n");       /* a number is missing */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 2 a.png\n63 0 0 three 5 0 0 4\n");  /* not a number */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7\n63 0 0 3 5 0 0 4\n");               /* broken header */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 0 a.png\n");                       /* declares zero glyphs */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "FONT 8 7 5 999 a.png\n63 0 0 3 5 0 0 4\n");   /* declares too many */
    CHECK(!Bmfont_Load(&f, p));
    write_text(p, "");
    CHECK(!Bmfont_Load(&f, p));
    CHECK_INT(f.count, 0);                                       /* a failed load leaves a clean struct */

    FILE *fp = fopen(p, "w");                                    /* more glyphs than the table holds */
    CHECK(fp != NULL);
    if (fp == NULL)
        return;
    fprintf(fp, "FONT 8 7 5 100 a.png\n");
    for (int i = 0; i < BMFONT_MAX_GLYPHS + 5; i++) fprintf(fp, "%d 0 0 1 1 0 0 2\n", 40 + i);
    fclose(fp);
    CHECK(!Bmfont_Load(&f, p));
    TmpRemove("font");
}

/* ---- does the text FIT where it is drawn?  (widths in pixels; the canvas is 256 wide) ---- */
static void test_all_level_names_fit(void)
{
    BakedFont f8, f16;
    CHECK(Bmfont_Load(&f8, font_path("silkscreen-8.fnt")));
    CHECK(Bmfont_Load(&f16, font_path("silkscreen-16.fnt")));
    int big = 0;
    for (int i = 0; i < LEVEL_COUNT; i++) {
        char buf[64];
        snprintf(buf, sizeof buf, "%02d  %s", i, Level_Name(i));
        int w16 = Bmfont_Width(&f16, buf), w8 = Bmfont_Width(&f8, buf);
        int used = (w16 <= 244) ? w16 : w8;                      /* ui.c picks the big font when it fits */
        if (w16 <= 244) big++;
        CHECK(used <= 244);                                      /* leaves >= 6 px each side (+ outline) */
    }
    CHECK(big >= 20);                                            /* nearly every title can use the big font */
}

typedef struct { const char *text; int size; int max_width; const char *where; } Fit;

static void test_ui_strings_fit(void)
{
    static const Fit fits[] = {
        { "1. PRESS Z, X, C, SPACE OR CLICK",   8, 248, "tutorial line 1" },
        { "2. PRESS AGAIN TO LOCK THE ANGLE",   8, 248, "tutorial line 2" },
        { "3. PRESS AGAIN: POWER, THEN SHOOT",  8, 248, "tutorial line 3" },
        { "R RESTART  P AIM ASSIST  M MUTE",    8, 166, "controls hint (must stop before the flag at x=175)" },
        { "SHOTS 99   99:59.9",                 8, 100, "HUD counter (between the bats and the buttons)" },
        { "OUT OF STRIKES - PRESS R",           8, 248, "retry hint" },
        { "NEW BEST: 99 STROKES",               8, 248, "toast" },
        { "ASSIST",                             8,  34, "assist label (above the bats, x 4..38)" },
        { "v1.0 C-PORT",                        8,  80, "version label" },
        { "STAGE SELECT",                      16, 170, "dialog title (must stop before the X button)" },
        { "ESC TO CLOSE",                       8, 150, "dialog subtitle" },
        { "CLEARED 25 / 25",                    8, 150, "dialog footer" },
        { "00",                                16,  26, "stage button label" },
        { "24",                                16,  26, "stage button label" },
        { "88",                                16,  26, "stage button label (widest digits)" },
        { "THANKS FOR PLAYING!",               16, 246, "ending title" },
        { "PLAYER   WWWWWWWWWWWWWWWWWWWWWWW",   8, 250, "ending: longest possible name (23 x W)" },
        { "PRESS ACTION TO PLAY AGAIN",         8, 248, "ending prompt" },
        { "ORIGINAL GAME BY ESCADA GAMES",      8, 248, "credit 1" },
        { "C PORT - EDUCATIONAL PROJECT",       8, 248, "credit 2" },
    };
    BakedFont f8, f16;
    CHECK(Bmfont_Load(&f8, font_path("silkscreen-8.fnt")));
    CHECK(Bmfont_Load(&f16, font_path("silkscreen-16.fnt")));
    for (size_t i = 0; i < sizeof fits / sizeof fits[0]; i++) {
        int w = Bmfont_Width(fits[i].size == 8 ? &f8 : &f16, fits[i].text);
        if (w > fits[i].max_width)
            printf("    '%s' (%s) is %d px wide, limit %d\n", fits[i].text, fits[i].where, w, fits[i].max_width);
        CHECK(w <= fits[i].max_width);
    }
}

/* ---- palette --------------------------------------------------------------------- */
static void test_contrast_maths(void)
{
    Rgba black = { 0, 0, 0, 255 }, white = { 255, 255, 255, 255 }, grey = { 119, 119, 119, 255 };
    CHECK_NEAR(Palette_Luminance(black), 0.0, 1e-6);
    CHECK_NEAR(Palette_Luminance(white), 1.0, 1e-4);
    CHECK_NEAR(Palette_Contrast(black, white), 21.0, 0.01);     /* the largest possible ratio */
    CHECK_NEAR(Palette_Contrast(white, black), 21.0, 0.01);     /* symmetric */
    CHECK_NEAR(Palette_Contrast(grey, grey), 1.0, 1e-6);        /* identical colours */
    CHECK_NEAR(Palette_Contrast(white, grey), 4.48, 0.05);      /* #777 on white is the classic ~4.5 */
    CHECK(Palette_Luminance(grey) > 0.15f && Palette_Luminance(grey) < 0.21f);   /* gamma: mid-grey is NOT 0.5 */
}

static void test_palette_is_readable(void)
{
    Rgba bg = COL_BG, land = COL_LAND, ink = COL_INK;

    CHECK(PAL_BG != PAL_LAND && PAL_LAND != PAL_INK && PAL_BG != PAL_INK);       /* three different colours */
    CHECK(Palette_Luminance(bg) < Palette_Luminance(land));     /* dark sky, mid terrain, light ink */
    CHECK(Palette_Luminance(land) < Palette_Luminance(ink));

    float ink_on_bg = Palette_Contrast(ink, bg);
    float land_on_bg = Palette_Contrast(land, bg);
    printf("    palette '%s': ink/bg %.2f:1, land/bg %.2f:1, ink/land %.2f:1\n",
           PAL_NAME, ink_on_bg, land_on_bg, Palette_Contrast(ink, land));
    CHECK(ink_on_bg >= 4.5f);       /* WCAG AA for normal text: all the text is ink on bg (outlined) */
    CHECK(land_on_bg >= 3.0f);      /* WCAG for graphics: the terrain must stand out from the sky    */
    CHECK(Palette_Contrast(ink, bg) > Palette_Contrast(land, bg) * 0.5f);   /* the text pair is the strongest one */
}

/* Small accent text is only allowed where it reaches the 4.5:1 text contrast. */
static void test_pick_text(void)
{
    Rgba bg = { 0, 0, 0, 255 };
    Rgba bright = { 255, 255, 0, 255 };            /* very readable on black  */
    Rgba dim = { 40, 40, 40, 255 };                /* nearly invisible on black */
    Rgba fallback = { 255, 255, 255, 255 };

    Rgba got = Palette_PickText(bg, bright, fallback, 4.5f);
    CHECK(got.r == bright.r && got.g == bright.g);               /* readable: the preferred colour is kept */
    got = Palette_PickText(bg, dim, fallback, 4.5f);
    CHECK(got.r == fallback.r && got.b == fallback.b);            /* too faint: the fallback is used */
    got = Palette_PickText(bg, dim, fallback, 1.0f);              /* with a lax limit even dim text passes */
    CHECK(got.r == dim.r);
    got = Palette_PickText(bg, bg, fallback, 4.5f);               /* identical to the background: never */
    CHECK(got.r == fallback.r);

    /* with the REAL palette: whatever the UI picks must really be readable */
    Rgba land = COL_LAND, ink = COL_INK, pal_bg = COL_BG;
    Rgba small = Palette_PickText(pal_bg, land, ink, 4.5f);
    CHECK(Palette_Contrast(small, pal_bg) >= 4.5f);
    Rgba large = Palette_PickText(pal_bg, land, ink, 3.0f);
    CHECK(Palette_Contrast(large, pal_bg) >= 3.0f);
}

static void test_color_macros(void)
{
    Rgba bg = COL_BG;
    CHECK_INT(bg.r, (PAL_BG >> 16) & 255);
    CHECK_INT(bg.g, (PAL_BG >> 8) & 255);
    CHECK_INT(bg.b, PAL_BG & 255);
    CHECK_INT(bg.a, 255);
    CHECK_INT(PAL_R(0x112233u), 0x11);
    CHECK_INT(PAL_G(0x112233u), 0x22);
    CHECK_INT(PAL_B(0x112233u), 0x33);
    Rgba o = COL_OUTLINE, i = COL_INK;                           /* hazard outline uses the ink colour */
    CHECK(o.r == i.r && o.g == i.g && o.b == i.b);
}

void run_font_palette_tests(void)
{
    RUN(test_fonts_load);
    RUN(test_glyphs_inside_atlas_and_disjoint);
    RUN(test_find_and_fallback);
    RUN(test_width);
    RUN(test_16_is_exact_double_of_8);
    RUN(test_parser_rejects_bad_files);
    RUN(test_all_level_names_fit);
    RUN(test_ui_strings_fit);
    RUN(test_contrast_maths);
    RUN(test_palette_is_readable);
    RUN(test_pick_text);
    RUN(test_color_macros);
}
