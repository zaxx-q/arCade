/*
 * bmfont.c  -  parse a baked font description and measure text
 */
#include <stdio.h>
#include <string.h>
#include "bmfont.h"
#include "util.h"

/*
 * File format (written by tools/bake_font.py):
 *     # comment
 *     FONT <size> <line_height> <cap_height> <count> <atlas file>
 *     <codepoint> <x> <y> <w> <h> <xoff> <yoff> <advance>      (one line per glyph, ascending)
 *
 *   PSEUDOCODE
 *     open the file ; IF it cannot be opened THEN fail
 *     WHILE a line can be read
 *         skip comments and blank lines
 *         IF the line starts with FONT THEN read the header
 *         ELSE read 8 numbers into the next glyph slot
 *     check: header seen, at least one glyph, codepoints strictly ascending, '?' present
 */

/* [arCade patch 11] The body of the old read loop, shared by Bmfont_Load (a file) and Bmfont_Parse (a text in
 * memory). Handles ONE already trimmed line; false = the file is malformed. */
static bool parse_line(BakedFont *f, const char *line, bool *have_header)
{
    if (line[0] == '\0' || line[0] == '#')
        return true;

    if (strncmp(line, "FONT ", 5) == 0) {
        int declared = 0;
        bool ok = sscanf(line, "FONT %d %d %d %d %63s",
                         &f->size, &f->line_height, &f->cap_height, &declared, f->atlas) == 5
                  && declared > 0 && declared <= BMFONT_MAX_GLYPHS;
        *have_header = ok;
        return ok;
    }

    if (f->count >= BMFONT_MAX_GLYPHS)
        return false;
    BakedGlyph *g = &f->glyphs[f->count];
    bool ok = *have_header &&
              sscanf(line, "%d %d %d %d %d %d %d %d", &g->codepoint, &g->x, &g->y, &g->w,
                     &g->h, &g->xoff, &g->yoff, &g->advance) == 8;
    if (ok) {
        /* the table must be sorted: binary search depends on it */
        if (f->count > 0 && g->codepoint <= f->glyphs[f->count - 1].codepoint)
            ok = false;
        f->count++;
    }
    return ok;
}

/* The checks after the last line; wipes the font when anything is wrong. */
static bool finish(BakedFont *f, bool ok, bool have_header)
{
    if (!ok || !have_header || f->count == 0 || Bmfont_Find(f, '?')->codepoint != '?') {
        memset(f, 0, sizeof *f);
        return false;
    }
    return true;
}

bool Bmfont_Load(BakedFont *f, const char *path)
{
    memset(f, 0, sizeof *f);
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;

    bool have_header = false, ok = true;
    char line[160];
    while (ok && fgets(line, sizeof line, fp) != NULL) {
        Str_Trim(line);
        ok = parse_line(f, line, &have_header);
    }
    fclose(fp);
    return finish(f, ok, have_header);
}

/* [arCade patch 11] The same, for the text of a .fnt file that is already in memory (arCade compiles the
 * assets into the executable). `text` is NUL-terminated; a line longer than the buffer is cut. */
bool Bmfont_Parse(BakedFont *f, const char *text)
{
    memset(f, 0, sizeof *f);
    bool have_header = false, ok = true;
    while (ok && *text != '\0') {
        char line[160];
        size_t n = strcspn(text, "\n");
        size_t keep = n < sizeof line - 1 ? n : sizeof line - 1;
        memcpy(line, text, keep);
        line[keep] = '\0';
        text += n;
        if (*text == '\n')
            text++;
        Str_Trim(line);
        ok = parse_line(f, line, &have_header);
    }
    return finish(f, ok, have_header);
}

/*
 * Bmfont_Find : BINARY SEARCH in the sorted glyph table.
 * A character that is not in the font is shown as '?', so the result is never NULL.
 */
const BakedGlyph *Bmfont_Find(const BakedFont *f, int codepoint)
{
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int c = f->glyphs[mid].codepoint;
        if (c == codepoint)
            return &f->glyphs[mid];
        if (c < codepoint)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (codepoint != '?')
        return Bmfont_Find(f, '?');                  /* recursion: look for the fallback */
    return &f->glyphs[0];                            /* font without '?': first glyph */
}

/* Width in pixels = the sum of the advances (this font has no kerning). */
int Bmfont_Width(const BakedFont *f, const char *text)
{
    int width = 0;
    for (const char *p = text; *p != '\0'; p++)
        width += Bmfont_Find(f, (unsigned char)*p)->advance;
    return width;
}
