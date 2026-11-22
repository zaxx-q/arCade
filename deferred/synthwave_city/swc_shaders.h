/* Synthwave City - the GLSL sources, embedded at build time (see embed_shaders.cmake). Plain C. */
#ifndef SWC_SHADERS_H
#define SWC_SHADERS_H

/* The text of shaders/<name> ("sky.frag", "common.glsl", ...), NUL-terminated; NULL if there is none. */
const char *swc_shader_get(const char *name);

#endif
