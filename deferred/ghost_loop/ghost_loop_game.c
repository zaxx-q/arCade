/* Ghost Loop - raylib front-end. All rules live in ghost_loop_logic.c. */
#include "arcade/gfx.h"
#include "ghost_loop_logic.h"

static void draw(const ghost_loop_State *s, float t) {
    gfx_backdrop(t * 0.3f);
    Rectangle room = {GL_ROOM_X0, GL_ROOM_Y0, GL_ROOM_X1 - GL_ROOM_X0, GL_ROOM_Y1 - GL_ROOM_Y0};
    DrawRectangleLinesEx(room, 3.0f, Fade(ARC_COL_DIM, 0.6f));

    Color pad_col = s->door_open ? ARC_COL_GREEN : ARC_COL_YELLOW;
    DrawCircle((int)GL_PAD_X, (int)GL_PAD_Y, GL_PAD_R, Fade(pad_col, 0.18f));
    DrawCircleLines((int)GL_PAD_X, (int)GL_PAD_Y, GL_PAD_R, pad_col);
    gfx_text_center("PAD", (int)GL_PAD_X, (int)GL_PAD_Y - 8, 16, pad_col);

    Rectangle door = {GL_DOOR_X, GL_DOOR_Y0, GL_DOOR_W, GL_DOOR_Y1 - GL_DOOR_Y0};
    DrawRectangleRec(door, Fade(ARC_COL_MAGENTA, s->door_open ? 0.12f : 0.85f));
    DrawCircleLines((int)GL_EXIT_X, (int)GL_EXIT_Y, GL_EXIT_R, ARC_COL_GREEN);
    gfx_text_center("EXIT", (int)GL_EXIT_X, (int)GL_EXIT_Y - 8, 16, ARC_COL_GREEN);

    for (int g = 0; g < s->ghost_count; g++)
        gfx_glow_circle(s->ghost_pos[g].x, s->ghost_pos[g].y, GL_R, Fade(ARC_COL_CYAN, 0.55f));
    gfx_glow_circle(s->player.x, s->player.y, GL_R, ARC_COL_TEXT);

    float k = (float)s->step / (float)GL_ROUND_STEPS; /* round timer */
    DrawRectangle(GL_ROOM_X0, 690, (int)((GL_ROOM_X1 - GL_ROOM_X0) * (1.0f - k)), 8, ARC_COL_CYAN);
}

static GameResult play(const GameContext *ctx) {
    static ghost_loop_State s;
    ghost_loop_init(&s, ctx->seed);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        ghost_loop_step(&s, &in, ARC_DT);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_hud("GHOST LOOP", ghost_loop_score(&s),
                TextFormat("round %d   ghosts %d", s.round + 1, s.ghost_count));
        arcade_frame_end();
        if (ghost_loop_is_over(&s))
            break;
    }
    GameResult r;
    r.score = ghost_loop_score(&s);
    r.finished = ghost_loop_is_over(&s);
    return r;
}

const GameDef game_ghost_loop_def = {"ghost_loop",
                                     "Ghost Loop",
                                     "Your past runs replay as ghosts: team up with yourself",
                                     "WASD: move   each round is 8 s; next round a ghost repeats your last "
                                     "run   hold the pad to open the door",
                                     40,
                                     true,
                                     play};
