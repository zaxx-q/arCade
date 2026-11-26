/* Synthwave City - the layers a player can switch off while flying (the L key). PURE C99, no raylib.
 * A set bit means the layer is drawn. The renderers (CPU and GPU) read the mask; this file only knows how it
 * cycles and what to call it, so that the HUD and the tests agree. */
#ifndef SWC_LAYERS_H
#define SWC_LAYERS_H

#define SWC_LIVE_BEAMS 1u  /* searchlights, pillar beacons and laser fans */
#define SWC_LIVE_AIR 2u    /* the small craft and the hero ships of the sky lanes */
#define SWC_LIVE_STREET 4u /* the cars on the streets and the people on the sidewalks */
#define SWC_LIVE_HOLO 8u   /* the floating ads, the wire solids and the ticker rings */
#define SWC_LIVE_ALL (SWC_LIVE_BEAMS | SWC_LIVE_AIR | SWC_LIVE_STREET | SWC_LIVE_HOLO)

/* The state after one press of the key: everything, then each layer on its own (beams, air, street life,
 * holograms), then none, then everything again. A mask that is none of those (a harness can set any) goes to
 * the first single-layer state if it has a known bit set, and to everything if it has none. */
unsigned swc_layers_next(unsigned mask);
/* "ALL LAYERS ON", "ONLY AIR", "ONLY BEAMS + STREET", "ALL LAYERS OFF", for the HUD. The text lives in a
 * static buffer that the next call overwrites. */
const char *swc_layers_label(unsigned mask);

#endif
