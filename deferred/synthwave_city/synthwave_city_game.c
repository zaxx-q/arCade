/* Synthwave City - raylib front-end: window loop, HUD, soundtrack and the scripted flight of smoke runs.
 * All rules are in synthwave_city_logic.c; everything on screen except the HUD is in
 * synthwave_city_render_game.c.
 *
 * The game can run two ways, and both use the same step and draw functions below:
 *   - play(): the normal game, started from the hub's archive or by a smoke run (own loop);
 *   - the guest session (synthwave_city_session.h): the hub runs it behind its menu, in the hub's loop. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/gfx.h"
#include "arcade/synth.h"
#include "synthwave_city_front.h"
#include "synthwave_city_session.h"

/* ------------------------------------------------------------------ soundtrack */

#define SONG_NOTES 2048
#define SONG_TAIL_SECONDS 1.0f /* the last notes ring on past the loop end: folded back onto its start */

#define MUSIC_LEVEL_PLAYING 0.8f /* volume while the player flies */

/* One soundtrack per running game (play() has one, the guest session has another), so they never share a
 * stream. `level` is the volume the track plays at; `muted` overrides it with silence. */
typedef struct Soundtrack {
    bool ready;
    bool muted;
    bool paused;
    float level;
    Music music;
    unsigned char *wav;
} Soundtrack;

/* Renders the loop of the song (bars 16..31 of the first movement) with the core synthesizer and starts it.
 */
static bool music_start(Soundtrack *m, unsigned seed) {
    memset(m, 0, sizeof(*m));
    m->level = MUSIC_LEVEL_PLAYING;
    if (!arcade_audio_ready())
        return false;
    static SwcSong song;
    static SynthNote notes[SONG_NOTES];
    swc_song_init(&song, seed);
    int n = swc_song_synth_notes(&song, SWC_MUSIC_FIRST_BAR, SWC_MUSIC_BARS, SWC_BPM, notes, SONG_NOTES);
    float loop_s = (float)swc_music_seconds();
    size_t loop_frames = synth_frames(loop_s, SYNTH_RATE);
    size_t tail_frames = synth_frames(SONG_TAIL_SECONDS, SYNTH_RATE);
    int16_t *pcm = (int16_t *)MemAlloc((unsigned int)((loop_frames + tail_frames) * sizeof(int16_t)));
    size_t bytes = synth_wav_bytes(loop_frames);
    m->wav = (unsigned char *)MemAlloc((unsigned int)bytes);
    if (pcm == NULL || m->wav == NULL) {
        if (pcm != NULL)
            MemFree(pcm);
        if (m->wav != NULL)
            MemFree(m->wav);
        m->wav = NULL;
        return false;
    }
    SynthOptions o;
    o.rate = SYNTH_RATE;
    o.bpm = (float)SWC_BPM;
    o.drums = true;
    o.drums_from = 0.0f;
    o.drums_to = loop_s;
    synth_render(notes, (size_t)n, &o, pcm, loop_frames + tail_frames);
    int16_t *out = (int16_t *)(m->wav + 44);
    for (size_t i = 0; i < loop_frames; i++) { /* fold the tail onto the start: a seamless loop */
        int v = pcm[i] + (i < tail_frames ? pcm[loop_frames + i] : 0);
        out[i] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
    MemFree(pcm);
    synth_wav_header(loop_frames, SYNTH_RATE, m->wav);
    m->music = LoadMusicStreamFromMemory(".wav", m->wav, (int)bytes);
    if (!IsMusicValid(m->music)) {
        MemFree(m->wav);
        m->wav = NULL;
        return false;
    }
    m->music.looping = true;
    SetMusicVolume(m->music, m->level);
    PlayMusicStream(m->music);
    m->ready = true;
    return true;
}

/* Keeps the audio streaming and re-aligns the beat clock when it drifts from what is playing. */
static void music_update(Soundtrack *m, synthwave_city_State *s) {
    if (!m->ready || m->paused)
        return;
    UpdateMusicStream(m->music);
    double played = (double)GetMusicTimePlayed(m->music);
    if (played < 0.3)
        return;
    played = fmod(played, swc_music_seconds());
    if (fabs(swc_wrap_delta(played, s->beat_time, swc_music_seconds())) > 0.06)
        swc_sync_beat(s, played);
}

static void music_apply_volume(const Soundtrack *m) {
    if (m->ready)
        SetMusicVolume(m->music, m->muted ? 0.0f : m->level);
}

static void music_set_muted(Soundtrack *m, bool muted) {
    m->muted = muted;
    music_apply_volume(m);
}

static void music_set_level(Soundtrack *m, float level) {
    m->level = level;
    music_apply_volume(m);
}

static void music_set_paused(Soundtrack *m, bool paused) {
    if (m->ready && m->paused != paused) {
        if (paused)
            PauseMusicStream(m->music);
        else
            ResumeMusicStream(m->music);
    }
    m->paused = paused;
}

static void music_stop(Soundtrack *m) {
    if (m->ready) {
        StopMusicStream(m->music);
        UnloadMusicStream(m->music);
    }
    if (m->wav != NULL)
        MemFree(m->wav);
    memset(m, 0, sizeof(*m));
}

/* ------------------------------------------------------------------ the scripted flight of smoke runs */

/* Smoke runs replace the random "monkey" with a short film that touches every control: the tour, the three
 * moods, free flight with a boost, a jump to a viewpoint, a look-around drag and the four states of the layer
 * switch. It repeats every 900 frames.
 * The frame at 299 (the default smoke length) shows a canyon flight in the Miami mood. */
static void scripted_input(int frame, ArcadeInput *in) {
    int f = frame % 900;
    arcade_input_clear(in);
    switch (f) {
    case 100:
        in->typed = 't'; /* midnight */
        break;
    case 160:
        in->typed = '2'; /* Downtown Canyon */
        break;
    case 250:
        in->typed = 't'; /* miami */
        break;
    case 330:
        in->typed = '5'; /* Megatower */
        break;
    case 420:
        in->typed = 't'; /* back to sunset */
        break;
    case 470:
        in->typed = '3'; /* Rooftop */
        break;
    case 560:
        in->typed = '4'; /* Panorama */
        break;
    case 650:
        in->typed = '6'; /* Plaza */
        break;
    case 760:
        in->action = in->action_pressed = true; /* back to the tour */
        break;
    case 790:
    case 800:
    case 810:
        in->typed = 'q'; /* medium, low, high again (the GPU renderer only; the CPU one has no tiers) */
        break;
    case 840:
    case 850:
    case 860:
    case 870:
    case 880:
    case 890:
        in->typed = 'l'; /* beams, air, street, holograms, nothing, everything again */
        break;
    default:
        break;
    }
    if (f >= 165 && f < 330) { /* flying: forward, a boost for a while, gentle weave, a metre or two */
        in->move_y = -1.0f;
        in->move_x = 0.06f * sinf((float)f * 0.05f);
        in->alt = f >= 200 && f < 260;
    }
    if (f >= 335 && f < 420) {
        in->move_y = -1.0f;
        in->move_x = 0.4f;
    }
    if (f >= 560 && f < 650) { /* a slow look-around: the mouse button is held and the pointer drifts */
        in->fire = true;
        in->aim_x = 600.0f + (float)(f - 560) * 4.0f;
        in->aim_y = 360.0f + 30.0f * sinf((float)f * 0.1f);
    }
    if (f >= 480 && f < 560) {
        in->move_y = -0.5f;
    }
}

/* ------------------------------------------------------------------ renderer choice and debug switches */

/* The GPU renderer runs on a real OpenGL 3.3 context; the software rasteriser of the headless smoke run (and
 * a GPU that cannot make the render targets) gets the CPU renderer, which looks the same everywhere. */
enum { RENDERER_CPU_ONLY, RENDERER_CPU, RENDERER_GPU };

/* Headless runs only: environment variables steer the verification harness (not available to players).
 *   SWC_RENDERER=cpu|gpu   SWC_VIEW=1..6 (fly nowhere: stay at that viewpoint)   SWC_MOOD=0..2
 *   SWC_FREEZE=seconds (stops the clock)   SWC_LAYERS=bits (1 sky, 2 legacy city, 4 buildings, 8 beams, 16
 * ground, 32 air traffic, 64 street life, 128 holograms)   SWC_POST=0 SWC_BLOOM=0 SWC_REFLECT=0
 *   SWC_LIVE=0..15 (the layer switch of the L key: 1 beams, 2 air traffic, 4 street life, 8 holograms; either
 *   renderer)
 *   SWC_POSE=x,y,z,yaw,pitch (stay at this pose instead of a viewpoint) */
typedef struct DebugSwitches {
    bool on;
    int view;      /* -1: none */
    int mood;      /* -1: none */
    double freeze; /* < 0: none */
    bool force_cpu;
    int live; /* the layer switch's mask */
    bool has_pose;
    double pose[5];
    SwcTier tier;
    bool msaa;
    bool interactive;
} DebugSwitches;

static double env_number(const char *name, double fallback) {
    const char *v = getenv(name);
    return v != NULL && v[0] != '\0' ? atof(v) : fallback;
}

static DebugSwitches debug_read(bool headless) {
    DebugSwitches d = {false,         -1,   -1,   -1.0, false, (int)SWC_LIVE_ALL, false, {0.0},
                       SWC_TIER_HIGH, true, false};
    if (!headless)
        return d;
    d.on = true;
    d.view = (int)env_number("SWC_VIEW", 0.0) - 1;
    d.mood = (int)env_number("SWC_MOOD", -1.0);
    d.freeze = env_number("SWC_FREEZE", -1.0);
    const char *r = getenv("SWC_RENDERER");
    d.force_cpu = r != NULL && strcmp(r, "cpu") == 0;
    d.live = (int)env_number("SWC_LIVE", (double)SWC_LIVE_ALL);
    const char *pose = getenv("SWC_POSE");
    if (pose != NULL &&
        sscanf(pose, "%lf,%lf,%lf,%lf,%lf", &d.pose[0], &d.pose[1], &d.pose[2], &d.pose[3], &d.pose[4]) == 5)
        d.has_pose = true;
    swc_tier_from_name(getenv("SWC_TIER"), &d.tier);
    d.msaa = env_number("SWC_MSAA", 1.0) != 0.0;
    d.interactive = env_number("SWC_INTERACTIVE", 0.0) != 0.0;
    return d;
}

/* ------------------------------------------------------------------ HUD */

/* The HUD is laid out for the hub's 1280 x 720 window. A window of another size (a maximised or full-screen
 * one) gets the same layout scaled by its height, anchored to the bottom, so the text stays where the eye
 * expects it and does not shrink into a corner. At 720 pixels every number below is what it always was. */
#define SHADE_TOP 540
#define SHADE_STRIPS 18
#define SHADE_STRIP_H 10 /* the shade covers y = 540..720, darkest at the bottom */

typedef struct HudInfo {
    int renderer;
    unsigned live;    /* the layer switch (the L key) */
    SwcTier tier;     /* the quality tier (the Q key) */
    double scale;     /* the scene's share of the window's pixels per side */
    int msaa;         /* samples in use */
    char toast[48];   /* the last L or Q press's new state, for a moment */
    float toast_left; /* seconds the toast stays */
} HudInfo;

static int hud_px(float k, int v) {
    return (int)floorf((float)v * k + 0.5f);
}

static void draw_overlay(const synthwave_city_State *s, float t, const HudInfo *h, const char *esc_does) {
    double speed = sqrt(s->cam.vx * s->cam.vx + s->cam.vy * s->cam.vy + s->cam.vz * s->cam.vz);
    float k = (float)GetScreenHeight() / (float)ARC_SCREEN_H;
    int width = GetScreenWidth();
    /* a soft shade behind the text, so it stays readable over bright sky */
    for (int i = 0; i < SHADE_STRIPS;
         i++) { /* plain strips: a gradient quad is not blended right by the CPU rasteriser */
        float f = (float)(i + 1) / (float)SHADE_STRIPS;
        int y0 = hud_px(k, SHADE_TOP + i * SHADE_STRIP_H),
            y1 = hud_px(k, SHADE_TOP + (i + 1) * SHADE_STRIP_H);
        DrawRectangle(0, y0, width, y1 - y0, Fade(BLACK, 0.62f * f * sqrtf(f)));
    }
    const char *mode = s->touring ? "TOUR" : "FLY";
    char gpu[64] = "";
    if (h->renderer == RENDERER_GPU) { /* the tier, and what it costs the picture: MSAA, a reduced scale */
        int n = snprintf(gpu, sizeof(gpu), "   GPU %s", swc_tier_label(h->tier));
        if (h->msaa > 0 && n > 0 && n < (int)sizeof(gpu))
            n += snprintf(gpu + n, sizeof(gpu) - (size_t)n, "   MSAA %dx", h->msaa);
        if (h->scale < 0.995 && n > 0 && n < (int)sizeof(gpu))
            snprintf(gpu + n, sizeof(gpu) - (size_t)n, "   RES %.0f%%", h->scale * 100.0);
    }
    gfx_text(TextFormat("%s   ALT %.0f m   SPEED %.0f km/h   %.1f km%s%s%s", mode, s->cam.y, speed * 3.6,
                        s->distance / 1000.0, gpu, h->live != SWC_LIVE_ALL ? "   " : "",
                        h->live != SWC_LIVE_ALL ? swc_layers_label(h->live) : ""),
             hud_px(k, 24), hud_px(k, 650), hud_px(k, 18), ARC_COL_CYAN);
    if (h->toast_left > 0.0f) /* the new state, for a moment, also when it is back to the start */
        gfx_text(h->toast, hud_px(k, 24), hud_px(k, 600), hud_px(k, 20),
                 Fade(ARC_COL_YELLOW, h->toast_left > 0.5f ? 0.95f : h->toast_left * 1.9f));
    if (t < 14.0f) {
        Color hint = Fade(ARC_COL_TEXT, 0.8f * (1.0f - t / 14.0f) + 0.15f);
        gfx_text("WASD / arrows: fly   drag with the mouse: look   RIGHT MOUSE: boost   wheel: speed",
                 hud_px(k, 24), hud_px(k, 676), hud_px(k, 16), hint);
        gfx_text(
            h->renderer == RENDERER_CPU_ONLY
                ? TextFormat(
                      "SPACE: tour on / off   1-6: viewpoints   T: mood   L: layers   M: mute   ESC: %s",
                      esc_does)
                : TextFormat("SPACE: tour on / off   1-6: viewpoints   T: mood   L: layers   G: renderer   "
                             "Q: quality   M: mute   ESC: %s",
                             esc_does),
            hud_px(k, 24), hud_px(k, 696), hud_px(k, 16), hint);
    }
    if (s->view >= 0 && !s->touring)
        gfx_text(s->views[s->view].name, hud_px(k, 24), hud_px(k, 626), hud_px(k, 20),
                 Fade(ARC_COL_MAGENTA, 0.9f));
}

/* ------------------------------------------------------------------ one frame of the game */

/* One step as play() and the guest session both run it: the key the front-end owns (M), the rules, and the
 * audio (which also re-aligns the beat clock with what is playing). */
static void game_step(synthwave_city_State *s, Soundtrack *m, const ArcadeInput *in, float dt) {
    if (in->typed == 'm' || in->typed == 'M')
        music_set_muted(m, !m->muted);
    synthwave_city_step(s, in, dt);
    music_update(m, s);
}

/* One picture: the city, and on top of it the HUD when `hud` is set. `scene_time` drives things that move on
 * their own (search lights), `hud_time` fades the controls help. */
static void game_draw(const synthwave_city_State *s, float scene_time, bool hud, float hud_time,
                      const HudInfo *h, const char *esc_does) {
    ClearBackground(BLACK);
    if (h->renderer == RENDERER_GPU) {
        swc_gpu_note_frame((double)GetFrameTime()); /* for the adaptive render scale */
        swc_render_scene_gpu(s, (double)scene_time);
    } else {
        swc_render_scene(s, (double)scene_time);
    }
    if (hud) {
        gfx_hud("Synthwave City", synthwave_city_score(s), swc_mood_name(s->mood));
        draw_overlay(s, hud_time, h, esc_does);
    }
}

/* ------------------------------------------------------------------ the game */

static GameResult play(const GameContext *ctx) {
    static synthwave_city_State s; /* about 2.5 MB: static, like every big state in arCade */
    static Soundtrack music;
    unsigned city_seed = ctx->headless ? ctx->seed : SWC_DEFAULT_SEED;
    synthwave_city_init(&s, city_seed);
    swc_render_init();
    DebugSwitches dbg = debug_read(ctx->headless);
    int renderer = RENDERER_CPU_ONLY;
    bool gpu_ready = false;
    if (swc_gpu_available()) {
        gpu_ready = swc_gpu_init();
        renderer = gpu_ready && !dbg.force_cpu ? RENDERER_GPU : RENDERER_CPU;
        if (!gpu_ready)
            renderer = RENDERER_CPU_ONLY;
    }
    if (dbg.on && gpu_ready) {
        SwcGpuDebug g = {(int)env_number("SWC_LAYERS", 255.0), env_number("SWC_POST", 1.0) != 0.0,
                         env_number("SWC_BLOOM", 1.0) != 0.0, env_number("SWC_REFLECT", 1.0) != 0.0,
                         dbg.msaa};
        swc_gpu_set_debug(&g);
    }
    SwcTier tier = dbg.tier; /* "high" unless a harness says otherwise */
    if (gpu_ready) {
        swc_gpu_set_tier(tier);
        swc_gpu_set_interactive(!ctx->headless || dbg.interactive); /* a harness run is not a real window */
    }
    unsigned live =
        (unsigned)dbg.live & SWC_LIVE_ALL; /* the layer switch: everything on unless a harness says so */
    swc_live_layers_set(live);
    HudInfo hud;
    memset(&hud, 0, sizeof(hud));
    if (dbg.view >= 0 && dbg.view < SWC_VIEW_COUNT)
        swc_goto_view(&s, dbg.view);
    if (dbg.has_pose) {
        s.cam.x = dbg.pose[0], s.cam.y = dbg.pose[1], s.cam.z = dbg.pose[2];
        s.cam.vx = s.cam.vy = s.cam.vz = 0.0;
        s.cam.yaw = dbg.pose[3], s.cam.pitch = dbg.pose[4];
        s.touring = false;
        s.view = -1;
    }
    if (dbg.mood >= 0)
        s.mood = dbg.mood;
    music_start(&music, city_seed);
    arcade_loop_begin(ctx); /* REQUIRED: resets the frame counter and smoke-test state */
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        if (ctx->headless && (dbg.view >= 0 || dbg.has_pose))
            arcade_input_clear(&in); /* hold still at the chosen viewpoint */
        else if (ctx->headless)
            scripted_input(arcade_frame_index(), &in);
        else
            arcade_poll(&in);
        if ((in.typed == 'g' || in.typed == 'G') && gpu_ready)
            renderer = renderer == RENDERER_GPU ? RENDERER_CPU : RENDERER_GPU;
        if (in.typed == 'l' || in.typed == 'L') {
            live = swc_layers_next(live);
            swc_live_layers_set(live);
            snprintf(hud.toast, sizeof(hud.toast), "LAYERS   %s", swc_layers_label(live));
            hud.toast_left = 2.5f;
        }
        if ((in.typed == 'q' || in.typed == 'Q') && gpu_ready && renderer == RENDERER_GPU) {
            tier = swc_tier_next(tier);
            swc_gpu_set_tier(tier);
            snprintf(hud.toast, sizeof(hud.toast), "QUALITY   %s", swc_tier_label(tier));
            hud.toast_left = 2.5f;
        }
        game_step(&s, &music, &in, ARC_DT);
        if (dbg.mood >= 0)
            s.mood = dbg.mood;
        if (dbg.freeze >= 0.0)
            s.clock = dbg.freeze;
        if (dbg.freeze >= 0.0)
            s.beat_time = dbg.freeze; /* the web version's beat on a frozen clock: time * BPM / 60 */
        t += ARC_DT;
        hud.toast_left = hud.toast_left > ARC_DT ? hud.toast_left - ARC_DT : 0.0f;
        hud.renderer = renderer;
        hud.live = live;
        hud.tier = tier;
        hud.scale = renderer == RENDERER_GPU ? swc_gpu_render_scale() : 1.0;
        hud.msaa = renderer == RENDERER_GPU ? swc_gpu_msaa_samples() : 0;
        BeginDrawing();
        game_draw(&s, t, true, t, &hud, "leave");
        arcade_frame_end(); /* REQUIRED instead of EndDrawing() */
    }
    music_stop(&music);
    swc_live_layers_set(SWC_LIVE_ALL); /* the switch is the player's, not the next run's */
    swc_gpu_shutdown();
    swc_render_shutdown();
    GameResult r;
    r.score = synthwave_city_score(&s);
    r.finished = false; /* an endless flight: leaving with ESC is the only way out, and it is never ranked */
    return r;
}

/* ------------------------------------------------------------------ the guest session (hosted by the hub) */

/* Volume while the hub's menu is in front: a quiet bed under it, not a wall of sound. */
#define MUSIC_LEVEL_BEHIND_MENU 0.3f

typedef struct Guest {
    bool active;
    bool player_flies; /* the player holds the camera */
    bool hand_back;    /* the camera was just given back: the next step lets the drone take it again */
    float time;        /* seconds since the session began: drives things that move on their own */
    float hud_time;    /* seconds since the player took the camera: fades the controls help */
    int renderer;
    bool gpu_ready;
    SwcTier tier;
    HudInfo hud;
    Soundtrack music;
} Guest;

static Guest g_guest;
static synthwave_city_State g_guest_state; /* about 2.5 MB: static, and separate from play()'s */

void swc_session_begin(unsigned seed) {
    if (g_guest.active)
        swc_session_end();
    memset(&g_guest, 0, sizeof(g_guest));
    synthwave_city_init(&g_guest_state, seed);
    swc_render_init();
    g_guest.renderer = RENDERER_CPU_ONLY;
    g_guest.tier = SWC_TIER_HIGH;
    if (swc_gpu_available()) {
        g_guest.gpu_ready = swc_gpu_init();
        g_guest.renderer = g_guest.gpu_ready ? RENDERER_GPU : RENDERER_CPU_ONLY;
        if (g_guest.gpu_ready) {
            swc_gpu_set_tier(g_guest.tier);
            swc_gpu_set_interactive(true);
        }
    }
    music_start(&g_guest.music, seed); /* silent if there is no audio device: the game still runs */
    music_set_level(&g_guest.music, MUSIC_LEVEL_BEHIND_MENU);
    g_guest.active = true;
}

void swc_session_end(void) {
    if (!g_guest.active)
        return;
    music_stop(&g_guest.music);
    swc_gpu_shutdown();
    swc_render_shutdown();
    memset(&g_guest, 0, sizeof(g_guest));
}

bool swc_session_active(void) {
    return g_guest.active;
}

void swc_session_set_control(bool player) {
    Guest *g = &g_guest;
    if (!g->active || g->player_flies == player)
        return;
    g->player_flies = player;
    if (player) {
        g->hud_time = 0.0f; /* the controls help shows again each time the player takes the camera */
        g->hand_back = false;
    } else {
        g->hand_back = true;
    }
    music_set_level(&g->music, player ? MUSIC_LEVEL_PLAYING : MUSIC_LEVEL_BEHIND_MENU);
}

void swc_session_update(const ArcadeInput *in, float dt) {
    Guest *g = &g_guest;
    if (!g->active)
        return;
    ArcadeInput idle;
    if (!g->player_flies || in == NULL) { /* nobody flies: empty input, so only the drone moves the camera */
        arcade_input_clear(&idle);
        if (g->hand_back && !g_guest_state.touring)
            idle.action_pressed = true; /* the same press as Space: the drone takes the camera from here */
        g->hand_back = false;
        in = &idle;
    }
    if ((in->typed == 'g' || in->typed == 'G') && g->gpu_ready)
        g->renderer = g->renderer == RENDERER_GPU ? RENDERER_CPU : RENDERER_GPU;
    if (in->typed == 'l' || in->typed == 'L') {
        unsigned live = swc_layers_next(swc_live_layers());
        swc_live_layers_set(live);
        snprintf(g->hud.toast, sizeof(g->hud.toast), "LAYERS   %s", swc_layers_label(live));
        g->hud.toast_left = 2.5f;
    }
    if ((in->typed == 'q' || in->typed == 'Q') && g->gpu_ready && g->renderer == RENDERER_GPU) {
        g->tier = swc_tier_next(g->tier);
        swc_gpu_set_tier(g->tier);
        snprintf(g->hud.toast, sizeof(g->hud.toast), "QUALITY   %s", swc_tier_label(g->tier));
        g->hud.toast_left = 2.5f;
    }
    game_step(&g_guest_state, &g->music, in, dt);
    g->time += dt;
    g->hud_time += dt;
    g->hud.toast_left = g->hud.toast_left > dt ? g->hud.toast_left - dt : 0.0f;
}

void swc_session_draw(void) {
    if (!g_guest.active)
        return;
    Guest *g = &g_guest;
    g->hud.renderer = g->renderer;
    g->hud.live = swc_live_layers();
    g->hud.tier = g->tier;
    g->hud.scale = g->renderer == RENDERER_GPU ? swc_gpu_render_scale() : 1.0;
    g->hud.msaa = g->renderer == RENDERER_GPU ? swc_gpu_msaa_samples() : 0;
    game_draw(&g_guest_state, g->time, g->player_flies, g->hud_time, &g->hud, "menu");
}

void swc_session_pause_audio(bool paused) {
    if (g_guest.active)
        music_set_paused(&g_guest.music, paused);
}

void swc_session_set_muted(bool muted) {
    if (g_guest.active)
        music_set_muted(&g_guest.music, muted);
}

bool swc_session_muted(void) {
    return g_guest.active && g_guest.music.muted;
}

bool swc_session_touring(void) {
    return g_guest.active && g_guest_state.touring;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_synthwave_city_def = {"synthwave_city",
                                         "Synthwave City",
                                         "Fly through an endless neon megacity at sunset",
                                         "WASD fly - drag mouse look - Space tour - T mood",
                                         90,
                                         false, /* a sightseeing flight: nothing to rank */
                                         play};
