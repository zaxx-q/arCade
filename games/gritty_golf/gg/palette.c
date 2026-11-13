/*
 * palette.c  -  how readable is a colour pair?  (WCAG contrast ratio)
 *
 * The game uses exactly three colours (see palette.h).  Because there are so few,
 * the choice of palette decides whether small text is readable, so the unit tests
 * measure it with the formula web accessibility standards use:
 *
 *     L  = 0.2126 R + 0.7152 G + 0.0722 B           (R,G,B converted to LINEAR light)
 *     contrast = (L_lighter + 0.05) / (L_darker + 0.05)        range 1 (same) .. 21 (black/white)
 *
 *   [T12 Math]  powf() for the sRGB gamma curve
 */
#include <math.h>
#include "palette.h"

/* sRGB value 0..255 -> linear light 0..1 */
static float to_linear(uint8_t v)
{
    float c = (float)v / 255.0f;
    return (c <= 0.04045f) ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

float Palette_Luminance(Rgba c)
{
    return 0.2126f * to_linear(c.r) + 0.7152f * to_linear(c.g) + 0.0722f * to_linear(c.b);
}

float Palette_Contrast(Rgba a, Rgba b)
{
    float la = Palette_Luminance(a), lb = Palette_Luminance(b);
    float hi = la > lb ? la : lb;
    float lo = la > lb ? lb : la;
    return (hi + 0.05f) / (lo + 0.05f);
}

Rgba Palette_PickText(Rgba bg, Rgba preferred, Rgba fallback, float min_ratio)
{
    return (Palette_Contrast(preferred, bg) >= min_ratio) ? preferred : fallback;
}
