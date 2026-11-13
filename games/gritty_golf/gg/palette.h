/*
 * palette.h  -  readability maths for the game's three colours
 *
 * The colours themselves live in palette_colors.h, which tools/recolor_assets.py
 * GENERATES together with the repainted textures, so code and art can never
 * disagree.  This header only provides the contrast calculation used by the tests.
 */
#ifndef GG_PALETTE_H
#define GG_PALETTE_H

#include "common.h"

float Palette_Luminance(Rgba c);                 /* 0 (black) .. 1 (white), gamma-correct  */
float Palette_Contrast(Rgba a, Rgba b);          /* 1 (identical) .. 21 (black on white)   */

/* Use `preferred` for text on `bg` if it is readable enough (contrast >= min_ratio),
 * otherwise `fallback`.  This keeps small accent text legible in ANY palette. */
Rgba  Palette_PickText(Rgba bg, Rgba preferred, Rgba fallback, float min_ratio);

#endif /* GG_PALETTE_H */
