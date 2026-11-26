/* Synthwave City - the layer switch's states: see swc_layers.h. */
#include "swc_layers.h"

#include <string.h>

unsigned swc_layers_next(unsigned mask) {
    switch (mask & SWC_LIVE_ALL) {
    case SWC_LIVE_ALL:
        return SWC_LIVE_BEAMS;
    case SWC_LIVE_BEAMS:
        return SWC_LIVE_AIR;
    case SWC_LIVE_AIR:
        return SWC_LIVE_STREET;
    case SWC_LIVE_STREET:
        return SWC_LIVE_HOLO;
    case SWC_LIVE_HOLO:
        return 0u;
    case 0u:
        return SWC_LIVE_ALL;
    default:
        return SWC_LIVE_BEAMS;
    }
}

const char *swc_layers_label(unsigned mask) {
    static char text[64];
    static const struct {
        unsigned bit;
        const char *name;
    } layers[] = {{SWC_LIVE_BEAMS, "BEAMS"},
                  {SWC_LIVE_AIR, "AIR"},
                  {SWC_LIVE_STREET, "STREET"},
                  {SWC_LIVE_HOLO, "HOLOGRAMS"}};
    mask &= SWC_LIVE_ALL;
    if (mask == SWC_LIVE_ALL)
        return "ALL LAYERS ON";
    if (mask == 0u)
        return "ALL LAYERS OFF";
    strcpy(text, "ONLY ");
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if ((mask & layers[i].bit) == 0u)
            continue;
        if (n++ > 0)
            strcat(text, " + ");
        strcat(text, layers[i].name);
    }
    return text;
}
