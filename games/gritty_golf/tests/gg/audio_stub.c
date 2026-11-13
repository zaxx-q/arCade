/*
 * audio_stub.c - replaces src/audio.c in the unit tests.
 * It plays nothing; it only COUNTS calls so tests can assert on them.
 */
#include "audio.h"
#include "audio_stub.h"

int g_sfx_count[SFX_COUNT];
int g_sfx_pitched[SFX_COUNT];

void Audio_PlaySfx(SfxId id, bool pitch_shift)
{
    if (id < 0 || id >= SFX_COUNT)
        return;
    g_sfx_count[id]++;
    if (pitch_shift)
        g_sfx_pitched[id]++;
}

void AudioStub_Reset(void)
{
    for (int i = 0; i < SFX_COUNT; i++) {
        g_sfx_count[i] = 0;
        g_sfx_pitched[i] = 0;
    }
}
