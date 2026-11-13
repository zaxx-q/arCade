/*
 * bmfont.h  -  metrics of a baked BITMAP font (no graphics code, fully testable)
 *
 * assets/font/silkscreen-8.fnt describes where every character sits in the atlas
 * image.  This module only deals with those numbers: it parses the file, finds a
 * character, and measures a text.  render.c does the actual drawing.
 *
 *   [T13 File]    text file parsing with fgets + sscanf
 *   [T13 Struct] BakedGlyph / BakedFont
 *   [T14 Search]  glyphs are stored sorted, so Bmfont_Find uses BINARY SEARCH
 *   [T09 String]  Bmfont_Width walks a C string character by character
 */
#ifndef GG_BMFONT_H
#define GG_BMFONT_H

#include <stdbool.h>

#define BMFONT_MAX_GLYPHS 128

typedef struct {
    int codepoint;                 /* character code, e.g. 65 = 'A'                 */
    int x, y, w, h;                /* rectangle inside the atlas image              */
    int xoff, yoff;                /* where to draw it relative to the pen position */
    int advance;                   /* how far the pen moves afterwards              */
} BakedGlyph;

typedef struct {
    int  size;                     /* design size in pixels (8 or 16)               */
    int  line_height;              /* distance covered by one text line             */
    int  cap_height;               /* height of the capital letters                 */
    int  count;                    /* glyphs in use                                 */
    char atlas[64];                /* file name of the atlas image                  */
    BakedGlyph glyphs[BMFONT_MAX_GLYPHS];   /* sorted by codepoint                  */
} BakedFont;

bool              Bmfont_Load(BakedFont *f, const char *path);
const BakedGlyph *Bmfont_Find(const BakedFont *f, int codepoint);   /* never NULL once loaded */
int               Bmfont_Width(const BakedFont *f, const char *text);

#endif /* GG_BMFONT_H */
