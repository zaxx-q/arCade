/* Window, frame loop, input polling and smoke-test screenshots. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"
#include "gfx_touch.h"
#ifdef ARCADE_ANDROID
#include "gfx_android.h"
#endif
#include "arcade/str.h"
#include "arcade/version.h"
#include "arcade/view.h"
#include "rlgl.h"

static struct {
    bool headless;
    int frame;
    int max_frames;
    ArcadeMonkey monkey;
    char shot_path[512];
    bool shot_ok;
    ArcView view; /* how the 1280 x 720 design canvas sits in the window right now */
    bool native;  /* inside arcade_native_begin / arcade_native_end: units are window pixels */
} g;

/* ---- window size: the design canvas is scaled and centred in whatever window we have ---- */

/* Refits the canvas to the current window size and points the mouse at it. Cheap, so it runs whenever the
 * window may have changed (after every EndDrawing, which is where raylib processes the resize events).
 * GetMousePosition() is then (window position - offset) / scale = a design position. */
static void view_sync(void) {
    /* The render size is the size of the framebuffer being drawn to. Unlike GetScreenWidth() it only changes
     * when the window really has been resized, so the frame in which F11 is pressed is not drawn too big. */
    arc_view_fit(&g.view, GetRenderWidth(), GetRenderHeight());
    if (g.native)
        return; /* inside the native section the mouse stays in window pixels */
    SetMouseOffset((int)-g.view.off_x, (int)-g.view.off_y);
    SetMouseScale(1.0f / g.view.scale, 1.0f / g.view.scale);
}

/* The modelview matrix that maps design units to window pixels (translate, then scale). */
static void apply_design_transform(void) {
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    rlTranslatef(g.view.off_x, g.view.off_y, 0.0f);
    rlScalef(g.view.scale, g.view.scale, 1.0f);
}

void arcade_gfx_init_sized(bool headless, const char *assets_dir, int width, int height, bool fullscreen) {
    SetTraceLogLevel(LOG_WARNING);
    unsigned flags = FLAG_VSYNC_HINT;
/* A phone has one fixed window, and raylib's EGL setup does not check that 4x MSAA exists. */
#ifndef ARCADE_ANDROID
    if (!headless)
        flags |= FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
#endif
    SetConfigFlags(flags);
    if (width < 1 || height < 1) {
        width = ARC_SCREEN_W;
        height = ARC_SCREEN_H;
    }
#ifdef ARCADE_ANDROID
    width = height = 0; /* 0 x 0: the whole screen */
#endif
    InitWindow(width, height, "arCade " ARCADE_VERSION_STRING);
#ifdef ARCADE_ANDROID
    gfx_android_init();
#endif
    gfx_text_init(assets_dir);
    SetExitKey(KEY_NULL); /* ESC means "back to the hub", not "close the window" */
    if (!headless) {
#ifndef ARCADE_ANDROID
        SetWindowMinSize(ARC_VIEW_MIN_W, ARC_VIEW_MIN_H);
#endif
        SetTargetFPS(ARC_FPS); /* the games take one fixed step per frame: also on a 90 or 120 Hz screen */
    }
    memset(&g, 0, sizeof(g));
    g.headless = headless;
    if (!headless && fullscreen)
        ToggleBorderlessWindowed(); /* no video mode switch: instant, and it restores the old size */
    view_sync();
    gfx_touch_init(headless, &g.view);
    if (!headless)
        InitAudioDevice(); /* may fail on machines without sound: games must cope */
}

const char *arcade_default_data_dir(void) {
#ifdef ARCADE_ANDROID
    return gfx_android_files_dir(); /* the folder of the executable is the read-only system */
#else
    return GetApplicationDirectory();
#endif
}

void arcade_gfx_init(bool headless, const char *assets_dir) {
    arcade_gfx_init_sized(headless, assets_dir, 0, 0, false);
}

void arcade_gfx_shutdown(void) {
    gfx_text_shutdown();
    if (IsAudioDeviceReady())
        CloseAudioDevice();
    if (IsWindowReady())
        CloseWindow();
}

bool arcade_quit_requested(void) {
    return WindowShouldClose();
}

void arcade_flush_input(void) {
    BeginDrawing();
    ClearBackground(ARC_COL_BG);
    EndDrawing();
    view_sync();
    gfx_touch_flush(
        &g.view); /* back in the hub: the controls are off, and the fingers that are down are old */
}

void arcade_loop_begin(const GameContext *ctx) {
    view_sync();
    gfx_touch_game_begin();
    g.headless = ctx->headless;
    g.frame = 0;
    g.max_frames = ctx->max_frames;
    g.shot_ok = false;
    g.shot_path[0] = '\0';
    if (ctx->screenshot_path)
        arc_strlcpy(g.shot_path, ctx->screenshot_path, sizeof(g.shot_path));
    if (ctx->headless)
        arcade_monkey_init(&g.monkey, ctx->seed);
}

bool arcade_loop_running(void) {
    if (WindowShouldClose())
        return false;
    if (g.headless)
        return g.max_frames <= 0 || g.frame < g.max_frames;
    return !arcade_key_pressed(KEY_ESCAPE); /* Esc, the Android back key or the back button */
}

static float axis(bool neg, bool pos) {
    return (pos ? 1.0f : 0.0f) - (neg ? 1.0f : 0.0f);
}

void arcade_poll(ArcadeInput *in) {
    if (g.headless && !gfx_touch_testing()) {
        arcade_monkey_next(&g.monkey, in);
        return;
    }
    arcade_input_clear(in);
    in->move_x = axis(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT), IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
    in->move_y = axis(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP), IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    in->move_z = axis(IsKeyDown(KEY_Q), IsKeyDown(KEY_E));
    in->shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    Vector2 m = GetMousePosition(); /* already in design pixels: see view_sync() */
    in->aim_x = m.x;
    in->aim_y = m.y;
    in->fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in->fire_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in->alt = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in->alt_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in->action = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER);
    in->action_pressed = IsKeyPressed(KEY_SPACE) || arcade_key_pressed(KEY_ENTER);
    in->left_pressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
    in->right_pressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    in->up_pressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    in->down_pressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    for (int c = arcade_text_char(); c != 0; c = arcade_text_char()) /* the keyboard, or the on-screen one */
        if (c > 32 && c < 127)
            in->typed = c;
    float w = GetMouseWheelMove();
    in->wheel = w > 0.0f ? 1 : (w < 0.0f ? -1 : 0);
    gfx_touch_merge(in); /* fingers: stick, action button, pointer */
}

/* Reads the frame being drawn, exports it as PNG and checks that it is not a single flat colour. */
static void snapshot(void) {
    rlDrawRenderBatchActive(); /* flush pending draw calls so the read-back sees them */
    Image img = LoadImageFromScreen();
    if (img.data == NULL)
        return;
#ifdef ARCADE_HEADLESS_BUILD
    /* raylib's software ("Memory") backend reads the framebuffer back as BGRA, bottom-up.
     * Normalise to RGBA, top-down so the exported PNG looks like the real window. */
    if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        uint8_t *d = (uint8_t *)img.data;
        for (int i = 0; i < img.width * img.height; i++) {
            uint8_t t = d[i * 4];
            d[i * 4] = d[i * 4 + 2];
            d[i * 4 + 2] = t;
        }
        ImageFlipVertical(&img);
    }
#endif
    Color *px = LoadImageColors(img);
    if (px != NULL) {
        int total = img.width * img.height, diff = 0;
        for (int i = 1; i < total; i++)
            if (px[i].r != px[0].r || px[i].g != px[0].g || px[i].b != px[0].b)
                diff++;
        if (diff > total / 300)
            g.shot_ok = true; /* > ~0.3% of pixels differ from the background */
        UnloadImageColors(px);
    }
    ExportImage(img, g.shot_path);
    UnloadImage(img);
}

void arcade_frame_begin(void) {
    if (!g.headless && IsKeyPressed(KEY_F11))
        arcade_toggle_fullscreen(); /* before BeginDrawing: the window is not resized in the middle of a frame
                                     */
    BeginDrawing();
    g.native = false;
    view_sync(); /* the window may have been resized since the last frame ended */
    apply_design_transform();
    gfx_text_frame_begin();
    gfx_touch_frame_begin();
}

void arcade_frame_end(void) {
    arcade_native_end(); /* a game that forgot to leave the native section still gets its controls drawn */
    apply_design_transform();
    gfx_touch_draw(); /* the virtual controls and the keyboard go over everything the game drew */
    bool last = g.max_frames > 0 && g.frame + 1 >= g.max_frames;
    if (g.shot_path[0] != '\0' && (g.frame % 60 == 59 || last))
        snapshot();
    EndDrawing();
    g.native = false;
    view_sync(); /* EndDrawing polled the events: a resize or a mouse move is applied from here on */
    gfx_touch_frame_end(&g.view); /* ... and so are the fingers */
    g.frame++;
}

Rectangle arcade_view_bounds(void) {
    Rectangle r;
    if (g.native) {
        r.x = 0.0f;
        r.y = 0.0f;
        r.width = (float)g.view.win_w;
        r.height = (float)g.view.win_h;
    } else {
        r.x = g.view.left;
        r.y = g.view.top;
        r.width = g.view.right - g.view.left;
        r.height = g.view.bottom - g.view.top;
    }
    return r;
}

float arcade_view_scale(void) {
    return g.native ? 1.0f : g.view.scale;
}

void arcade_native_begin(void) {
    if (g.native)
        return;
    rlDrawRenderBatchActive(); /* what was queued still belongs to the design transform */
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    g.native = true;
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);
}

void arcade_native_end(void) {
    if (!g.native)
        return;
    rlDrawRenderBatchActive();
    g.native = false;
    view_sync();
    apply_design_transform(); /* EndMode3D and EndTextureMode reset the modelview matrix */
}

void arcade_scissor_begin(int x, int y, int w, int h) {
    if (g.native) {
        BeginScissorMode(x, y, w, h);
        return;
    }
    int wx, wy, ww, wh;
    arc_view_window_rect(&g.view, (float)x, (float)y, (float)w, (float)h, &wx, &wy, &ww, &wh);
    BeginScissorMode(wx, wy, ww, wh);
}

Vector2 arcade_window_mouse(void) {
    Vector2 m = GetMousePosition();
    if (g.native)
        return m;
    Vec2 w = arc_view_to_window(&g.view, m.x, m.y);
    Vector2 r = {w.x, w.y};
    return r;
}

Vector2 arcade_view_snap(Vector2 p) {
    if (g.native) {
        Vector2 r = {roundf(p.x), roundf(p.y)};
        return r;
    }
    Vector2 r = {(roundf(g.view.off_x + p.x * g.view.scale) - g.view.off_x) / g.view.scale,
                 (roundf(g.view.off_y + p.y * g.view.scale) - g.view.off_y) / g.view.scale};
    return r;
}

void arcade_toggle_fullscreen(void) {
    if (g.headless)
        return;
    ToggleBorderlessWindowed(); /* the new size arrives with the next window event: see view_sync() */
}

bool arcade_is_fullscreen(void) {
    return !g.headless && IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
}

int arcade_frame_index(void) {
    return g.frame;
}

bool arcade_screenshot_ok(void) {
    return g.shot_ok;
}

bool arcade_audio_ready(void) {
    return !g.headless && IsAudioDeviceReady();
}
