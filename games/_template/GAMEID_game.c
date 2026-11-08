/* @TITLE@ - raylib front-end: window loop and drawing ONLY. All rules live in @ID@_logic.c. */
#include "@ID@_logic.h"
#include "arcade/gfx.h"

static void draw(const @ID@_State *s, float t) {
    gfx_backdrop(t);
    gfx_glow_circle(s->pellet.x, s->pellet.y, 10.0f, ARC_COL_YELLOW);
    gfx_glow_circle(s->pos.x, s->pos.y, 14.0f, ARC_COL_CYAN);
}

static GameResult play(const GameContext *ctx) {
    static @ID@_State s; /* static: keeps large game state off the stack */
    @ID@_init(&s, ctx->seed);
    arcade_loop_begin(ctx); /* REQUIRED: resets the frame counter and smoke-test state */
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        @ID@_step(&s, &in, ARC_DT);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_hud("@TITLE@", @ID@_score(&s), TextFormat("%.0fs", s.time_left > 0 ? s.time_left : 0));
        arcade_frame_end(); /* REQUIRED instead of EndDrawing() */
        if (@ID@_is_over(&s)) break;
    }
    GameResult r;
    r.score = @ID@_score(&s);
    r.finished = @ID@_is_over(&s);
    return r;
}

/* The hub finds this symbol through the generated registry. The name MUST be game_<id>_def. */
const GameDef game_@ID@_def = {
    "@ID@",                         /* id: equals the folder name       */
    "@TITLE@",                      /* title                            */
    "TODO: one-line hook",          /* tagline                          */
    "WASD / arrows: move",          /* controls hint                    */
    100,                            /* menu order (lower = earlier)     */
    true,                           /* ranked: score goes to leaderboard */
    play};
