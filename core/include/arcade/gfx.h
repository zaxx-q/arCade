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
#include "arcade/touch.h"
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
void arcade_gfx_init(bool headless,
                     const char *assets_dir); /* opens the 1280x720 window and loads the font */
/* The same, with a window of width x height (0 = the design size) that may start in borderless full screen.
 * The window is resizable in both cases: see "Window size and full screen" below. */
void arcade_gfx_init_sized(bool headless, const char *assets_dir, int width, int height, bool fullscreen);
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
 *       arcade_frame_begin();            // instead of BeginDrawing()
 *       ... draw ...
 *       arcade_frame_end();              // instead of EndDrawing()
 *       if (xxx_is_over(&state)) break;
 *   }
 */
void arcade_loop_begin(const GameContext *ctx);
bool arcade_loop_running(void);    /* false on ESC (interactive), window close, or max_frames (smoke) */
void arcade_poll(ArcadeInput *in); /* real keyboard/mouse, or scripted "monkey" input in smoke runs */
void arcade_frame_begin(void);     /* BeginDrawing() + the window-to-canvas transform (see below) */
void arcade_frame_end(void);       /* EndDrawing() + smoke screenshots + frame counter */
int arcade_frame_index(void);
bool arcade_screenshot_ok(void); /* smoke: at least one non-blank snapshot was exported */

/* ---- Window size and full screen ----
 *
 * The window can be any size: dragged to a new size, forced to full screen by a window manager, or put into
 * borderless full screen with F11 (arcade_frame_begin handles the key in every loop). Everything is still
 * laid out on the fixed design canvas ARC_SCREEN_W x ARC_SCREEN_H; arcade_frame_begin() scales that canvas
 * to the biggest size that fits the window and centres it, so a game draws exactly as before:
 *  - coordinates are design pixels, never window pixels (do not use GetScreenWidth()/GetScreenHeight());
 *  - the mouse (ArcadeInput.aim_x/aim_y, GetMousePosition()) is already in design pixels;
 *  - what the canvas does not cover (a window that is not 16:9) is only a margin: backdrops and full-width
 *    bars should reach the window's edges, so use arcade_view_bounds() instead of 0..ARC_SCREEN_W;
 *  - BeginScissorMode() works in window pixels: use arcade_scissor_begin() instead;
 *  - a renderer that fills the real window itself (a 3D scene) draws between arcade_native_begin() and
 *    arcade_native_end(), where one unit is one window pixel.
 * The pure math (and its tests) is in arcade/view.h. */
Rectangle
arcade_view_bounds(void);       /* the whole window in design units: x and y are <= 0 when it is not 16:9 */
float arcade_view_scale(void);  /* window pixels per design pixel (1 inside arcade_native_begin) */
void arcade_native_begin(void); /* leave the design transform: units are window pixels (3D scenes) */
void arcade_native_end(void);   /* back to the design canvas */
void arcade_scissor_begin(int x, int y, int w, int h); /* BeginScissorMode() with a design-pixel rectangle */
Vector2
arcade_window_mouse(void); /* the mouse in window pixels (mouse look: same speed at any window size) */
void arcade_toggle_fullscreen(void); /* borderless full screen on/off (F11); no video mode change */
bool arcade_is_fullscreen(void);
/* Called by arcade_frame_begin / gfx_text: */
Vector2 arcade_view_snap(Vector2 p); /* a design position moved to the nearest window pixel (crisp text) */
void gfx_text_frame_begin(void);     /* drops font atlases made for another scale */

/* Where the program keeps its files (scores, progress): the folder of the executable on a computer, the app's
 * private folder on Android. Works before arcade_gfx_init(). */
const char *arcade_default_data_dir(void);

/* ---- Touch screens (Android) ----
 *
 * A phone has no keyboard and no mouse. Inside a game loop nothing changes for the game: arcade_poll() turns
 * the virtual stick (bottom left), the action button (bottom right) and every other finger into the same
 * ArcadeInput as before (move_x/move_y, action, aim_x/aim_y, fire ...), "back" leaves the game, and the
 * keyboard button shows an on-screen keyboard whose letters arrive as ArcadeInput.typed. Rules and tests:
 * arcade/touch.h and arcade/osk.h. A game can ignore all of this.
 *
 * The hub reads keys through arcade_key_pressed() / arcade_text_char() instead of IsKeyPressed() /
 * GetCharPressed(), so the same screens work with a finger. On a computer the touch UI is off, unless the
 * environment variable ARCADE_TOUCH_UI=1 is set (the left mouse button is then one finger: a way to try it).
 */
/* True on a touch screen: screens may show touch-friendly hints. */
bool arcade_touch_ui(void);
/* IsKeyPressed(), or a tap on a zone registered for that key, or the on-screen keyboard's Enter/Backspace.
 * KEY_ESCAPE is also pressed by the Android back key. */
bool arcade_key_pressed(int key);
/* The next typed character (GetCharPressed(), or the on-screen keyboard); 0 = none. */
int arcade_text_char(void);
/* Call while drawing: a tap inside r (design units) counts as pressing `key` in the next frame. The zones
 * are forgotten at every arcade_frame_begin(). */
void arcade_touch_key_zone(Rectangle r, int key);
/* Shows or hides the on-screen keyboard (arcade_flush_input closes it). */
void arcade_osk_open(bool open);
bool arcade_osk_is_open(void);
/* The keyboard's top edge in design units: keep what must stay visible above it. */
float arcade_osk_top(void);

/* Smoke tests only: the touch UI is on, the fingers are the ones given here (ids as a phone would number
 * them) and arcade_poll() reads them instead of the monkey, like on a real device. */
void arcade_touch_test_begin(void);
void arcade_touch_test_fingers(const TouchPoint *pts, int n);
void arcade_touch_test_end(void);

/* ---- Audio (optional: every game must still work silently) ---- */
bool arcade_audio_ready(void); /* false in smoke runs and on machines without a sound device */
/* Synthesises a short decaying sine beep. Check arcade_audio_ready() first; free with UnloadSound(). */
Sound gfx_make_tone(float freq_hz, float seconds, float volume);

/* ---- Text: ALWAYS use these instead of DrawText()/MeasureText() so every screen uses the project font ----
 */
#define GFX_TEXT_SCALE 1.25f     /* multiplies every requested size (the font has a small x-height) */
#define GFX_TEXT_TRACKING 0.04f  /* extra letter spacing, fraction of the pixel size */
#define GFX_TEXT_YSHIFT 0.0f     /* vertical nudge, fraction of the pixel size */
#define GFX_TEXT_BOLD_BELOW 34   /* faux-bold (strike three times, 1 px apart) below this pixel size */
#define GFX_TEXT_SHADOW_BELOW 44 /* soft shadow for pixel sizes below this */
void gfx_text_init(const char *assets_dir); /* called by arcade_gfx_init */
void gfx_text_shutdown(void);
void gfx_text(const char *text, int x, int y, int size, Color c); /* x,y = top-left of the text box */
int gfx_text_width(const char *text, int size);

/* ---- Drawing helpers ---- */
void gfx_backdrop(float time_s); /* dark background with a faint animated grid */
void gfx_text_center(const char *text, int cx, int y, int size, Color c);
void gfx_text_right(const char *text, int right_x, int y, int size, Color c);
void gfx_glow_circle(float x, float y, float r, Color c);
void gfx_glow_line(Vec2 a, Vec2 b, float thick, Color c);
void gfx_hud(const char *title, int score, const char *extra); /* top bar: title, score, extra text */
/* Full-width fills: they run to the window's left and right edges, not just the canvas's (see "Window size
 * and full screen" above). gfx_band covers the design rows y .. y + h; gfx_fill_below covers everything from
 * row y to the bottom of the window (a screen flash or tint under the HUD bar). */
void gfx_band(int y, int h, Color c);
void gfx_fill_below(int y, Color c);
void gfx_draw_particles(const ParticleSystem *ps, Vec2 offset);
Color gfx_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/* ---- Sandbox rendering (hub backdrop + deferred/sandbox) ---- */
typedef struct GfxSandboxView {
    Texture2D tex;
    int w, h;
} GfxSandboxView;

void gfx_sandbox_view_init(GfxSandboxView *v, const Sandbox *sb);
void gfx_sandbox_view_free(GfxSandboxView *v);
void gfx_sandbox_view_draw(GfxSandboxView *v, const Sandbox *sb, Rectangle dst, Color tint);

#endif
