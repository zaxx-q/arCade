/* Ghost Loop - raylib front-end. All rules live in ghost_loop_logic.c. */
#include "arcade/gfx.h"
#include "ghost_loop_logic.h"

/* Each pad has its own colour; the door it controls is drawn in the same colour. */
static Color pad_color(int i) {
    switch (i) {
    case 0:
        return ARC_COL_YELLOW;
    case 1:
        return ARC_COL_MAGENTA;
    case 2:
        return gfx_rgba(255, 150, 60, 255);
    default:
        return gfx_rgba(160, 130, 255, 255);
    }
}

static Rectangle to_rect(gl_Rect r) {
    Rectangle out = {r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0};
    return out;
}

static void draw_room(const ghost_loop_State *s) {
    const gl_Level *lv = ghost_loop_level(s->level);
    Rectangle room = {GL_ROOM_X0, GL_ROOM_Y0, GL_ROOM_X1 - GL_ROOM_X0, GL_ROOM_Y1 - GL_ROOM_Y0};
    DrawRectangleLinesEx(room, 3.0f, Fade(ARC_COL_DIM, 0.6f));

    for (int i = 0; i < lv->wall_count; i++) {
        Rectangle w = to_rect(lv->walls[i]);
        DrawRectangleRec(w, Fade(ARC_COL_DIM, 0.35f)); /* bright enough to read against the dark room */
        DrawRectangleLinesEx(w, 2.0f, Fade(ARC_COL_DIM, 0.9f));
    }
    DrawCircleLines((int)lv->start.x, (int)lv->start.y, GL_R + 6.0f, Fade(ARC_COL_TEXT, 0.3f));
    for (int i = 0; i < lv->door_count; i++) {
        Color c = pad_color(lv->doors[i].pad);
        Rectangle d = to_rect(lv->doors[i].box);
        DrawRectangleRec(d, Fade(c, s->door_open[i] ? 0.10f : 0.85f));
        DrawRectangleLinesEx(d, 2.0f, Fade(c, s->door_open[i] ? 0.35f : 1.0f));
        gfx_text_center(TextFormat("%c", 'A' + lv->doors[i].pad), (int)((d.x + d.width / 2)),
                        (int)(d.y + d.height / 2 - 9), 18, s->door_open[i] ? Fade(c, 0.5f) : ARC_COL_BG);
    }
    for (int i = 0; i < lv->pad_count; i++) {
        Color c = pad_color(i);
        bool down = s->pad_down[i];
        DrawCircle((int)lv->pads[i].x, (int)lv->pads[i].y, GL_PAD_R, Fade(c, down ? 0.45f : 0.16f));
        DrawCircleLines((int)lv->pads[i].x, (int)lv->pads[i].y, GL_PAD_R, c);
        gfx_text_center(TextFormat("%c", 'A' + i), (int)lv->pads[i].x, (int)lv->pads[i].y - 9, 18, c);
    }
    DrawCircleLines((int)lv->exit.x, (int)lv->exit.y, GL_EXIT_R, ARC_COL_GREEN);
    gfx_text_center("EXIT", (int)lv->exit.x, (int)lv->exit.y - 8, 16, ARC_COL_GREEN);
}

/* The round timer: a bar that drains, with a tick for every second. */
static void draw_timer(const ghost_loop_State *s) {
    float full = GL_ROOM_X1 - GL_ROOM_X0;
    float k = (float)s->step / (float)GL_ROUND_STEPS;
    DrawRectangle((int)GL_ROOM_X0, 690, (int)(full * (1.0f - k)), 8, ARC_COL_CYAN);
    for (int i = 1; i < GL_ROUND_STEPS / 60; i++)
        DrawRectangle((int)(GL_ROOM_X0 + full * (float)i * 60.0f / (float)GL_ROUND_STEPS) - 1, 686, 2, 16,
                      Fade(ARC_COL_BG, 0.8f));
}

static void draw_notice(const ghost_loop_State *s) {
    if (s->notice == GL_NOTICE_NONE)
        return;
    float a = arc_minf(1.0f, (float)s->notice_steps / 40.0f); /* fades out at the end */
    const char *msg = s->notice == GL_NOTICE_TIME_UP
                          ? "TIME UP - same round again.  U undoes the last ghost, C restarts the level"
                          : "NO GHOSTS LEFT - R retries the round, U undoes a ghost, C restarts";
    gfx_text_center(msg, ARC_SCREEN_W / 2, 100, 26, Fade(ARC_COL_YELLOW, a));
}

/* The level-clear banner: a dark band across the room so the final positions stay visible around it. */
static void draw_clear_banner(const ghost_loop_State *s, float t) {
    const gl_Level *lv = ghost_loop_level(s->level);
    bool last = s->level + 1 >= GL_LEVEL_COUNT;
    DrawRectangle(0, 270, ARC_SCREEN_W, 170, Fade(ARC_COL_BG, 0.88f));
    gfx_text_center(last ? "ALL LEVELS CLEARED" : "LEVEL CLEARED", ARC_SCREEN_W / 2, 284, 44, ARC_COL_GREEN);
    gfx_text_center(TextFormat("+%d   %d ghosts (par %d)   %.1f s", s->level_points, s->ghost_count,
                               lv->par_ghosts, (float)s->step / 60.0f),
                    ARC_SCREEN_W / 2, 346, 26, ARC_COL_YELLOW);
    if (s->clear_hold >= GL_CLEAR_HOLD)
        gfx_text_center(last ? "press SPACE to finish" : "press SPACE for the next level", ARC_SCREEN_W / 2,
                        394, 22, Fade(ARC_COL_TEXT, 0.55f + 0.45f * sinf(t * 5.0f)));
}

static void draw(const ghost_loop_State *s, float t) {
    gfx_backdrop(t * 0.3f);
    draw_room(s);
    for (int g = 0; g < s->ghost_count; g++) {
        gfx_glow_circle(s->ghost_pos[g].x, s->ghost_pos[g].y, GL_R, Fade(ARC_COL_CYAN, 0.55f));
        gfx_text_center(TextFormat("%d", g + 1), (int)s->ghost_pos[g].x, (int)s->ghost_pos[g].y - 8, 16,
                        ARC_COL_BG);
    }
    gfx_glow_circle(s->player.x, s->player.y, GL_R, ARC_COL_TEXT);
    draw_timer(s);
    draw_notice(s);
    if (s->cleared)
        draw_clear_banner(s, t);
}

/* One whole frame: the room, the HUD, the strip under it with the level name and hint, and the key legend. */
static void draw_frame(const ghost_loop_State *s, float t) {
    const gl_Level *lv = ghost_loop_level(s->level);
    draw(s, t);
    gfx_hud("GHOST LOOP", ghost_loop_score(s),
            TextFormat("level %d/%d   round %d/%d   ghosts %d/%d   %.1f s", s->level + 1, GL_LEVEL_COUNT,
                       s->round + 1, lv->max_ghosts + 1, s->ghost_count, lv->max_ghosts,
                       (float)s->step / 60.0f));
    gfx_text_center(TextFormat("%s  -  %s", lv->name, lv->hint), ARC_SCREEN_W / 2, 36, 18, ARC_COL_DIM);
    gfx_text_center("SPACE: end round, your run becomes a ghost    R: redo round    U: undo last ghost    C: "
                    "restart level",
                    ARC_SCREEN_W / 2, 700, 16, Fade(ARC_COL_DIM, 0.8f));
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
        draw_frame(&s, t);
        arcade_frame_end();
        if (ghost_loop_is_over(&s))
            break;
    }
    GameResult r;
    r.score = ghost_loop_score(&s);
    r.finished = ghost_loop_is_over(&s);
    return r;
}

const GameDef game_ghost_loop_def = {
    "ghost_loop",
    "Ghost Loop",
    "Your past runs replay as ghosts: team up with yourself",
    "WASD: move   SPACE: end the round early   R: redo   U: undo   C: restart   "
    "a ghost on a pad holds its door open for you",
    40,
    play};
