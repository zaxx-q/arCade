/* Echo Maze - raylib front-end. All rules live in echo_maze_logic.c. */
#include "arcade/gfx.h"
#include "echo_maze_logic.h"

static void draw(const echo_maze_State *s) {
    ClearBackground(BLACK);
    for (int y = 0; y < EM_H; y++)
        for (int x = 0; x < EM_W; x++) {
            float k = s->reveal[y][x] / EM_REVEAL_TIME;
            if (!s->wall[y][x] || k <= 0.0f)
                continue;
            Rectangle r = {EM_OX + (float)x * EM_TILE, EM_OY + (float)y * EM_TILE, EM_TILE, EM_TILE};
            DrawRectangleRec(r, Fade(ARC_COL_CYAN, 0.10f + 0.55f * k));
            DrawRectangleLinesEx(r, 1.0f, Fade(ARC_COL_CYAN, 0.9f * k));
        }
    if (s->exit_glow > 0.0f) {
        Vec2 e = echo_maze_tile_center(EM_W - 2, EM_H - 2);
        gfx_glow_circle(e.x, e.y, 12.0f, Fade(ARC_COL_GREEN, s->exit_glow / EM_REVEAL_TIME));
    }
    if (s->pinging) {
        float fade = 1.0f - s->ping_r / EM_PING_RANGE;
        for (int i = 0; i < 3; i++)
            DrawCircleLines((int)s->ping_origin.x, (int)s->ping_origin.y, s->ping_r - (float)i * 3.0f,
                            Fade(ARC_COL_CYAN, 0.6f * fade));
    }
    gfx_glow_circle(s->player.x, s->player.y, 6.0f, ARC_COL_TEXT);
}

static GameResult play(const GameContext *ctx) {
    static echo_maze_State s;
    echo_maze_init(&s, ctx->seed);
    arcade_loop_begin(ctx);
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        echo_maze_step(&s, &in, ARC_DT);
        BeginDrawing();
        draw(&s);
        gfx_hud("ECHO MAZE", echo_maze_score(&s), TextFormat("pings %d   %.0fs", s.pings, s.time));
        if (s.pings == 0)
            gfx_text_center("press SPACE to ping", ARC_SCREEN_W / 2, ARC_SCREEN_H - 40, 22, ARC_COL_DIM);
        arcade_frame_end();
        if (echo_maze_is_over(&s))
            break;
    }
    GameResult r;
    r.score = echo_maze_score(&s);
    r.finished = echo_maze_is_over(&s);
    return r;
}

const GameDef game_echo_maze_def = {
    "echo_maze",
    "Echo Maze",
    "Navigate a pitch-black maze by sonar",
    "WASD: move   Space: send a sonar ping (1 s cooldown)   reach the green exit, fewer pings = more points",
    20,
    true,
    play};
