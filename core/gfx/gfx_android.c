/* Android only (core/CMakeLists.txt adds this file when the NDK toolchain is used). */
#include <android_native_app_glue.h>
#include <stddef.h>

#include "gfx_android.h"
#include "raylib.h"

/* raylib's Android platform defines this ("Add this to header (if apps really need it)") but raylib.h does
 * not declare it. */
struct android_app *GetAndroidApp(void);

const char *gfx_android_files_dir(void) {
    struct android_app *app = GetAndroidApp();
    if (app != NULL && app->activity != NULL && app->activity->internalDataPath != NULL)
        return app->activity->internalDataPath;
    return "";
}

/* raylib's own handler of the activity's events */
static void (*raylib_on_cmd)(struct android_app *, int32_t);

/* The activity's events come here first. When the app loses the screen raylib stops polling and blocks until
 * it is back, so the game stands still, but the audio device keeps playing what it already has: silence
 * it. */
static void on_cmd(struct android_app *app, int32_t cmd) {
    if (IsAudioDeviceReady()) {
        if (cmd == APP_CMD_PAUSE || cmd == APP_CMD_LOST_FOCUS)
            SetMasterVolume(0.0f);
        else if (cmd == APP_CMD_GAINED_FOCUS)
            SetMasterVolume(1.0f);
    }
    if (raylib_on_cmd != NULL)
        raylib_on_cmd(app, cmd);
}

void gfx_android_init(void) {
    struct android_app *app = GetAndroidApp();
    if (app != NULL && app->onAppCmd != on_cmd) {
        raylib_on_cmd = app->onAppCmd;
        app->onAppCmd = on_cmd;
    }
}
