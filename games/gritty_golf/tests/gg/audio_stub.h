#ifndef GG_AUDIO_STUB_H
#define GG_AUDIO_STUB_H
#include "common.h"
extern int g_sfx_count[SFX_COUNT];     /* how many times each sound was requested */
extern int g_sfx_pitched[SFX_COUNT];   /* ... and how many of those were pitch-shifted */
void AudioStub_Reset(void);
#endif
