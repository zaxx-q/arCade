/* Internal to core/gfx, Android only: what raylib's Android platform does not do for us. */
#ifndef ARCADE_GFX_ANDROID_H
#define ARCADE_GFX_ANDROID_H

/* The app's private folder (/data/user/0/<package>/files): the one place a program may write on Android.
 * Valid from the first line of main(), before the window exists. "" if the activity cannot be reached. */
const char *gfx_android_files_dir(void);

/* After InitWindow: mutes the sound while the app is not in front (raylib stops running the game then, and
 * the sound that was already playing would go on). */
void gfx_android_init(void);

#endif
