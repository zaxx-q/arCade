/* Gritty Golf - raylib sound backend. Ported from the upstream audio.c (see gg/VENDORED.md).
 *
 * The engine only calls Audio_PlaySfx(); this file is what answers it in the real game (the unit tests link
 * tests/gg/audio_stub.c instead).
 *
 * Two differences from upstream, both because the arCade hub owns the audio device:
 *   - no InitAudioDevice / CloseAudioDevice calls: we only load our sounds when arcade_audio_ready() says so
 *   - no SetMasterVolume call: that would also change the hub's own sounds, so volume and mute are applied to
 *     our voices only
 *
 * raylib restarts a Sound that is already playing, so a quick series of identical effects would cut each
 * other off. We keep VOICES copies ("aliases") of every effect and rotate through them. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "arcade/rng.h"
#include "gritty_golf_front.h"

#define VOICES 4

static const char *const SFX_FILES[SFX_COUNT] = {
    /* indexed by SfxId */
    [SFX_BALL_HIT] = "sfxBallHit.wav", [SFX_BALL_LANDED] = "sfxBallLanded.wav",
    [SFX_BALL_OOB] = "sfxBallOob.wav", [SFX_LEVEL_BEAT] = "sfxLevelBeat.wav",
    [SFX_POWER] = "sfxPower.wav",      [SFX_POWER_2] = "sfxPower2.wav"};

static Sound s_voice[SFX_COUNT][VOICES];
static int s_next[SFX_COUNT];
static bool s_ready = false;
static bool s_muted = false;
static int s_volume = 70;
static Rng s_rng; /* only the pitch of a sound effect depends on it, never the game rules */

bool Audio_Init(const char *asset_dir, int volume_percent) {
    if (!arcade_audio_ready())
        return false; /* smoke run or a machine without a sound card: the game stays silent */

    rng_seed(&s_rng, 0x6767u);
    for (int id = 0; id < SFX_COUNT; id++) {
        char path[320];
        snprintf(path, sizeof(path), "%s/audio/%s", asset_dir, SFX_FILES[id]);
        s_voice[id][0] = LoadSound(path);
        for (int v = 1; v < VOICES; v++)
            s_voice[id][v] = LoadSoundAlias(s_voice[id][0]); /* shares the sample data */
        s_next[id] = 0;
    }
    s_ready = true;
    s_muted = false;
    Audio_SetVolume(volume_percent);
    return true;
}

void Audio_Shutdown(void) {
    if (!s_ready)
        return;
    for (int id = 0; id < SFX_COUNT; id++) {
        for (int v = 1; v < VOICES; v++)
            UnloadSoundAlias(s_voice[id][v]); /* aliases first ... */
        UnloadSound(s_voice[id][0]);          /* ... then the original */
    }
    s_ready = false; /* the device itself belongs to the hub */
}

void Audio_SetVolume(int volume_percent) {
    s_volume = volume_percent < 0 ? 0 : (volume_percent > 100 ? 100 : volume_percent);
}

bool Audio_ToggleMute(void) {
    s_muted = !s_muted;
    return s_muted;
}

void Audio_PlaySfx(SfxId id, bool pitch_shift) {
    if (!s_ready || s_muted || id < 0 || id >= SFX_COUNT)
        return;

    float pitch = pitch_shift ? rng_rangef(&s_rng, 0.8f, 1.2f) : 1.0f; /* random speed 0.8x .. 1.2x */

    Sound voice = s_voice[id][s_next[id]];
    s_next[id] = (s_next[id] + 1) % VOICES;
    SetSoundVolume(voice, (float)s_volume / 100.0f);
    SetSoundPitch(voice, pitch);
    PlaySound(voice);
}
