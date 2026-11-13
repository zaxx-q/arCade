/*
 * audio.h  -  sound interface
 *
 * The game LOGIC only calls Audio_PlaySfx().  audio.c (raylib) implements it
 * for the real game, tests/audio_stub.c implements it for the unit tests
 * (it just counts calls).  Because the logic only knows this header, it never
 * depends on raylib.
 *
 *   [T01] separate compilation: declaration here, definition elsewhere
 */
#ifndef GG_AUDIO_H
#define GG_AUDIO_H

#include <stdbool.h>
#include "common.h"

/* Play one sound effect.  pitch_shift = true -> random speed 0.8x..1.2x */
void Audio_PlaySfx(SfxId id, bool pitch_shift);

/* ---- implemented by audio.c only (not needed by the logic or tests) ---- */
bool Audio_Init(const char *asset_dir, int volume_percent);  /* false = no sound device */
void Audio_Shutdown(void);
void Audio_SetVolume(int volume_percent);
bool Audio_ToggleMute(void);                                  /* returns new muted state */

#endif /* GG_AUDIO_H */
