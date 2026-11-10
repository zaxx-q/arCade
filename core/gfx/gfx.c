/* Window, frame loop, input polling and smoke-test screenshots. */
#include <stdio.h>
#include <string.h>

#include "arcade/gfx.h"
#include "arcade/str.h"
#include "arcade/version.h"
#include "rlgl.h"

static struct {
    bool headless;
    int frame;
    int max_frames;
    ArcadeMonkey monkey;
    char shot_path[512];
    bool shot_ok;
} g;

void arcade_gfx_init(bool headless) {
    SetTraceLogLevel(LOG_WARNING);
    unsigned flags = FLAG_VSYNC_HINT;
    if (!headless)
        flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    InitWindow(ARC_SCREEN_W, ARC_SCREEN_H, "arCade " ARCADE_VERSION_STRING);
    SetExitKey(KEY_NULL); /* ESC means "back to the hub", not "close the window" */
    if (!headless)
        SetTargetFPS(ARC_FPS);
    memset(&g, 0, sizeof(g));
    g.headless = headless;
    if (!headless)
        InitAudioDevice(); /* may fail on machines without sound: games must cope */
}

void arcade_gfx_shutdown(void) {
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
}

void arcade_loop_begin(const GameContext *ctx) {
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
    return !IsKeyPressed(KEY_ESCAPE);
}

static float axis(bool neg, bool pos) {
    return (pos ? 1.0f : 0.0f) - (neg ? 1.0f : 0.0f);
}

void arcade_poll(ArcadeInput *in) {
    if (g.headless) {
        arcade_monkey_next(&g.monkey, in);
        return;
    }
    arcade_input_clear(in);
    in->move_x = axis(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT), IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT));
    in->move_y = axis(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP), IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    Vector2 m = GetMousePosition();
    in->aim_x = m.x;
    in->aim_y = m.y;
    in->fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    in->fire_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    in->alt = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    in->alt_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    in->action = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ENTER);
    in->action_pressed = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER);
    in->left_pressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
    in->right_pressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
    in->up_pressed = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
    in->down_pressed = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
    for (int c = GetCharPressed(); c != 0; c = GetCharPressed())
        if (c > 32 && c < 127)
            in->typed = c;
    float w = GetMouseWheelMove();
    in->wheel = w > 0.0f ? 1 : (w < 0.0f ? -1 : 0);
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

void arcade_frame_end(void) {
    bool last = g.max_frames > 0 && g.frame + 1 >= g.max_frames;
    if (g.shot_path[0] != '\0' && (g.frame % 60 == 59 || last))
        snapshot();
    EndDrawing();
    g.frame++;
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
