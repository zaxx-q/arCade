/* raylib front-end helpers shared by the hub and every game.
 *
 * RULE: only *_game.c files, the files in core/gfx/ and app/ may include this header (it includes raylib.h).
 * Game LOGIC files (*_logic.c / *_logic.h) must never include it. */
#ifndef ARCADE_GFX_H
#define ARCADE_GFX_H

#include "arcade/common.h"
#include "arcade/fx.h"
#include "arcade/game.h"
#include "arcade/input.h"
#include "arcade/sandbox.h"
#include "raylib.h"

/* ---- Neon palette (use these so every game looks like it belongs to the hub) ---- */
#define ARC_COL_BG                                                                                           \
    CLITERAL(Color) {                                                                                        \
        8, 10, 22, 255                                                                                       \
    }
#define ARC_COL_PANEL                                                                                        \
    CLITERAL(Color) {                                                                                        \
        18, 22, 46, 255                                                                                      \
    }
#define ARC_COL_TEXT                                                                                         \
    CLITERAL(Color) {                                                                                        \
        232, 236, 255, 255                                                                                   \
    }
#define ARC_COL_DIM                                                                                          \
    CLITERAL(Color) {                                                                                        \
        120, 128, 170, 255                                                                                   \
    }
#define ARC_COL_CYAN                                                                                         \
    CLITERAL(Color) {                                                                                        \
        0, 229, 255, 255                                                                                     \
    }
#define ARC_COL_MAGENTA                                                                                      \
    CLITERAL(Color) {                                                                                        \
        255, 64, 200, 255                                                                                    \
    }
#define ARC_COL_YELLOW                                                                                       \
    CLITERAL(Color) {                                                                                        \
        255, 214, 64, 255                                                                                    \
    }
#define ARC_COL_GREEN                                                                                        \
    CLITERAL(Color) {                                                                                        \
        80, 255, 160, 255                                                                                    \
    }
#define ARC_COL_RED                                                                                          \
    CLITERAL(Color) {                                                                                        \
        255, 80, 90, 255                                                                                     \
    }

/* ---- Window lifecycle: called ONCE by the hub (app/), never by a game ---- */
void arcade_gfx_init(bool headless); /* opens the 1280x720 window; headless = no FPS cap */
void arcade_gfx_shutdown(void);
bool arcade_quit_requested(void); /* true when the window close button was pressed */
void arcade_flush_input(void);    /* draws one blank frame so stale key presses are consumed */

/* ---- Game loop: every game uses exactly this pattern ----
 *
 *   arcade_loop_begin(ctx);
 *   while (arcade_loop_running()) {
 *       ArcadeInput in;
 *       arcade_poll(&in);
 *       xxx_step(&state, &in, ARC_DT);   // pure logic
 *       BeginDrawing();
 *       ... draw ...
 *       arcade_frame_end();              // instead of EndDrawing()
 *       if (xxx_is_over(&state)) break;
 *   }
 */
void arcade_loop_begin(const GameContext *ctx);
bool arcade_loop_running(void);    /* false on ESC (interactive), window close, or max_frames (smoke) */
void arcade_poll(ArcadeInput *in); /* real keyboard/mouse, or scripted "monkey" input in smoke runs */
void arcade_frame_end(void);       /* EndDrawing() + smoke screenshots + frame counter */
int arcade_frame_index(void);
bool arcade_screenshot_ok(void); /* smoke: at least one non-blank snapshot was exported */

/* ---- Audio (optional: every game must still work silently) ---- */
bool arcade_audio_ready(void); /* false in smoke runs and on machines without a sound device */
/* Synthesises a short decaying sine beep. Check arcade_audio_ready() first; free with UnloadSound(). */
Sound gfx_make_tone(float freq_hz, float seconds, float volume);

/* ---- Drawing helpers ---- */
void gfx_backdrop(float time_s); /* dark background with a faint animated grid */
void gfx_text_center(const char *text, int cx, int y, int size, Color c);
void gfx_text_right(const char *text, int right_x, int y, int size, Color c);
void gfx_glow_circle(float x, float y, float r, Color c);
void gfx_glow_line(Vec2 a, Vec2 b, float thick, Color c);
void gfx_hud(const char *title, int score, const char *extra); /* top bar: title, score, extra text */
void gfx_draw_particles(const ParticleSystem *ps, Vec2 offset);
Color gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* ---- Sandbox rendering (hub backdrop + games/sandbox) ---- */
typedef struct GfxSandboxView {
    Texture2D tex;
    int w, h;
} GfxSandboxView;

void gfx_sandbox_view_init(GfxSandboxView *v, const Sandbox *sb);
void gfx_sandbox_view_free(GfxSandboxView *v);
void gfx_sandbox_view_draw(GfxSandboxView *v, const Sandbox *sb, Rectangle dst, Color tint);

#endif
