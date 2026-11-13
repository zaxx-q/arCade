/* Gritty Golf - raylib front-end. All rules live in gritty_golf_logic.c and the vendored engine in gg/.
 *
 * Per frame: read input, take one logic step, draw the 256x256 canvas into its render texture, then draw the
 * arCade screen around it. Progress is saved per player in ctx->data_dir; headless smoke runs keep
 * everything in memory and write nothing. */
#include <stdio.h>

#include "arcade/gfx.h"
#include "gritty_golf_front.h"

#define SOUND_VOLUME 70 /* percent: how loud the effects are, mixed under the hub's own volume */

static GameResult play(const GameContext *ctx) {
    static gritty_golf_State s;
    GameResult r = {0, false};

    char dir[512];
    snprintf(dir, sizeof(dir), "%s/gritty_golf", ctx->assets_dir ? ctx->assets_dir : "assets");
    if (!gritty_golf_front_init(dir)) {
        fprintf(stderr, "gritty_golf: could not load its assets from %s\n", dir);
        return r; /* the hub shows the menu again */
    }

    gritty_golf_init(&s, ctx->seed);
    bool audio = false;
    if (!ctx->headless) {
        gritty_golf_open_profile(&s, ctx->player ? ctx->player : "Player", ctx->data_dir);
        gritty_golf_show_splash(&s);
        audio = Audio_Init(dir, SOUND_VOLUME); /* false when there is no sound device */
    }

    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        gritty_golf_step(&s, &in, ARC_DT);
        if (s.mute_toggled) {
            Audio_ToggleMute();
            s.mute_toggled = false;
        }
        t += ARC_DT;

        /* the picture first (it needs its own render texture), then the screen around it */
        gritty_golf_front_render_canvas(&s, gritty_golf_to_canvas(in.aim_x, in.aim_y));
        BeginDrawing();
        gritty_golf_front_draw_screen(&s, vec2(in.aim_x, in.aim_y), t);
        arcade_frame_end();
        if (gritty_golf_is_over(&s))
            break;
    }

    r.score = gritty_golf_score(&s);
    r.finished =
        gritty_golf_is_over(&s) && r.score > 0; /* ESC, or finishing with nothing cleared, ranks nothing */
    if (audio)
        Audio_Shutdown();
    gritty_golf_front_shutdown();
    gritty_golf_shutdown(&s); /* also saves progress that was not saved yet */
    return r;
}

const GameDef game_gritty_golf_def = {
    "gritty_golf",
    "Gritty Golf",
    "Pixel mini-golf: 25 hand-made courses, few shots, three strikes",
    "Space / Enter / click: stop and aim, lock the angle, lock the power   R restart   L stages   F finish",
    85,
    true,
    play};
