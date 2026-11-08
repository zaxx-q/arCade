/* Sort It - raylib front-end. All rules live in sort_it_logic.c. */
#include "arcade/gfx.h"
#include "sort_it_logic.h"

static void draw(const sort_it_State *s, float t) {
    gfx_backdrop(t);
    float slot = SI_AREA_W / (float)s->n;
    float base = 520.0f, unit = 340.0f / (float)s->n;
    for (int i = 0; i < s->n; i++) {
        float h = unit * (float)s->values[i];
        float x = SI_X0 + slot * (float)i + 6.0f, w = slot - 12.0f;
        Color c = (i == s->mark) ? ARC_COL_MAGENTA : ARC_COL_CYAN;
        DrawRectangleRounded((Rectangle){x, base - h, w, h}, 0.2f, 6, Fade(c, 0.85f));
        gfx_text_center(TextFormat("%d", s->values[i]), (int)(x + w / 2.0f), (int)(base - h - 26.0f), 22,
                        ARC_COL_TEXT);
        if (i == s->cursor)
            DrawRectangle((int)x, (int)base + 10, (int)w, 6, ARC_COL_YELLOW);
    }
    gfx_text_center(TextFormat("Round %d / %d   swaps %d / %d", s->round + 1, SI_ROUNDS, s->swaps, s->budget),
                    ARC_SCREEN_W / 2, 560, 24, ARC_COL_TEXT);
    gfx_text_center(TextFormat("best possible: %d    selection sort: %d    bubble sort: %d", s->par,
                               s->selection_swaps, s->bubble_swaps),
                    ARC_SCREEN_W / 2, 600, 20, ARC_COL_DIM);
    gfx_text_center(s->mark >= 0 ? "now pick the bar to swap with"
                                 : "pick a bar (Space or click), then pick another to swap",
                    ARC_SCREEN_W / 2, 640, 18, ARC_COL_GREEN);
    if (s->last_round_score > 0)
        gfx_text_center(TextFormat("last round: +%d", s->last_round_score), ARC_SCREEN_W / 2, 80, 22,
                        ARC_COL_YELLOW);
}

static GameResult play(const GameContext *ctx) {
    static sort_it_State s;
    sort_it_init(&s, ctx->seed);
    arcade_loop_begin(ctx);
    float t = 0.0f;
    while (arcade_loop_running()) {
        ArcadeInput in;
        arcade_poll(&in);
        sort_it_step(&s, &in, ARC_DT);
        t += ARC_DT;
        BeginDrawing();
        draw(&s, t);
        gfx_hud("SORT IT", sort_it_score(&s), "");
        arcade_frame_end();
        if (sort_it_is_over(&s))
            break;
    }
    GameResult r;
    r.score = sort_it_score(&s);
    r.finished = sort_it_is_over(&s);
    return r;
}

const GameDef game_sort_it_def = {
    "sort_it",
    "Sort It",
    "Be the sorting algorithm: fewest swaps wins",
    "Left/Right or click: choose a bar   Space or click again: swap with the first pick",
    70,
    true,
    play};
