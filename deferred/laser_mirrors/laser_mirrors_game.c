/* Laser Mirrors - raylib front-end. All rules live in laser_mirrors_logic.c. */
#include "arcade/gfx.h"
#include "laser_mirrors_logic.h"

static Vec2 center(int x, int y) {
    return vec2(LM_OX + ((float)x + 0.5f) * LM_TILE, LM_OY + ((float)y + 0.5f) * LM_TILE);
}

static void draw_beams(const laser_mirrors_State *s) {
    static const float dx[4] = {1, 0, -1, 0}, dy[4] = {0, 1, 0, -1};
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++)
            for (int d = 0; d < 4; d++) {
                if (!(s->beam[y][x] & (1u << d)))
                    continue;
                Vec2 c = center(x, y);
                float half = LM_TILE / 2.0f;
                /* incoming half: from the edge the beam enters through, to the tile centre */
                gfx_glow_line(vec2(c.x - dx[d] * half, c.y - dy[d] * half), c, 3.0f, ARC_COL_RED);
                /* outgoing half(s): from the centre toward the direction(s) the beam leaves in */
                int out = d, branch = -1;
                switch (s->cell[y][x]) {
                case LM_TARGET:
                    continue; /* absorbed */
                case LM_SLASH:
                    out = 3 - d;
                    break;
                case LM_BACK:
                    out = d ^ 1;
                    break;
                case LM_SPLIT:
                    branch = 3 - d;
                    break;
                default:
                    break;
                }
                gfx_glow_line(c, vec2(c.x + dx[out] * half, c.y + dy[out] * half), 3.0f, ARC_COL_RED);
                if (branch >= 0)
                    gfx_glow_line(c, vec2(c.x + dx[branch] * half, c.y + dy[branch] * half), 3.0f,
                                  ARC_COL_RED);
            }
}

static void draw(const laser_mirrors_State *s, float t) {
    gfx_backdrop(t * 0.2f);
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++) {
            Rectangle r = {LM_OX + (float)x * LM_TILE, LM_OY + (float)y * LM_TILE, LM_TILE, LM_TILE};
            DrawRectangleLinesEx(r, 1.0f, Fade(ARC_COL_DIM, 0.25f));
        }
    draw_beams(s);
    for (int y = 0; y < LM_H; y++)
        for (int x = 0; x < LM_W; x++) {
            Vec2 c = center(x, y);
            float h = LM_TILE * 0.34f;
            Color mc = s->rotatable[y][x] ? ARC_COL_YELLOW : ARC_COL_CYAN;
            switch (s->cell[y][x]) {
            case LM_WALL:
                DrawRectangle((int)(c.x - LM_TILE / 2 + 4), (int)(c.y - LM_TILE / 2 + 4), (int)LM_TILE - 8,
                              (int)LM_TILE - 8, ARC_COL_PANEL);
                break;
            case LM_SLASH:
                gfx_glow_line(vec2(c.x - h, c.y + h), vec2(c.x + h, c.y - h), 5.0f, mc);
                break;
            case LM_BACK:
                gfx_glow_line(vec2(c.x - h, c.y - h), vec2(c.x + h, c.y + h), 5.0f, mc);
                break;
            case LM_SPLIT:
                DrawPoly((Vector2){c.x, c.y}, 4, h, 45.0f, ARC_COL_MAGENTA);
                break;
            case LM_TARGET:
                gfx_glow_circle(c.x, c.y, 14.0f, s->lit[y][x] ? ARC_COL_GREEN : ARC_COL_DIM);
                break;
            default:
                break;
            }
            if (s->rotatable[y][x])
                DrawCircleLines((int)c.x, (int)c.y, LM_TILE * 0.42f, Fade(ARC_COL_YELLOW, 0.35f));
        }
    Vec2 e = center(s->emitter_x, s->emitter_y);
    DrawRectangle((int)(e.x - 18), (int)(e.y - 18), 36, 36, ARC_COL_RED);
}

static GameResult play(const GameContext *ctx) {
    static laser_mirrors_State s;
    laser_mirrors_init(&s, ctx->seed);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        laser_mirrors_step(&s, &in, ARC_DT);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_hud("LASER MIRRORS", laser_mirrors_score(&s),
                TextFormat("targets %d/%d   moves %d", s.targets_lit, s.targets, s.moves));
        gfx_text_center("click a yellow mirror to rotate it", ARC_SCREEN_W / 2, ARC_SCREEN_H - 36, 20,
                        ARC_COL_DIM);
        arcade_frame_end();
        if (laser_mirrors_is_over(&s))
            break;
    }
    GameResult r;
    r.score = laser_mirrors_score(&s);
    r.finished = laser_mirrors_is_over(&s);
    return r;
}

const GameDef game_laser_mirrors_def = {
    "laser_mirrors",
    "Laser Mirrors",
    "Bounce the beam onto every target",
    "left click a yellow mirror: rotate it   light all targets in as few moves as possible",
    30,
    true,
    play};
