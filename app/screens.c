/* Hub screens: name entry, menu, launch, result, leaderboard. Each screen runs its own loop and
 * returns the next Screen to show (a tiny state machine driven from main.c). */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/str.h"
#include "arcade/version.h"

#define MENU_VISIBLE 8
#define CARD_X 48
#define CARD_W 560
#define CARD_H 64
#define CARD_PITCH 72
#define CARD_Y0 118
#define SAND_X 640 /* the free sand playground starts here */

static Leaderboard g_view; /* scratch copy for sorted views (28 KB: keep off the stack) */

/* Draws text word-wrapped inside max_w pixels; returns the y position below the last line. */
static int draw_wrapped(const char *text, int x, int y, int size, int max_w, Color c) {
    char line[160] = "";
    char word[64];
    const char *p = text;
    while (*p) {
        int n = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && n < (int)sizeof(word) - 1)
            word[n++] = *p++;
        word[n] = '\0';
        if (n == 0)
            break;
        char trial[240];
        snprintf(trial, sizeof(trial), "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && MeasureText(trial, size) > max_w) {
            DrawText(line, x, y, size, c);
            y += size + 4;
            arc_strlcpy(line, word, sizeof(line));
        } else {
            arc_strlcpy(line, trial, sizeof(line));
        }
    }
    if (line[0]) {
        DrawText(line, x, y, size, c);
        y += size + 4;
    }
    return y;
}

static void draw_title(const char *sub) {
    DrawText("arCade", 48, 28, 44, ARC_COL_CYAN);
    DrawText(sub, 52, 78, 18, ARC_COL_DIM);
}

/* ---------------------------------------------------------------- name entry */
Screen screen_name(AppState *app) {
    char buf[LB_NAME_LEN] = ""; /* starts empty: typing replaces the current name */
    int len = 0;
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        for (int c = GetCharPressed(); c != 0; c = GetCharPressed())
            if (c >= 32 && c < 127 && c != '|' && len < LB_NAME_LEN - 1) {
                buf[len++] = (char)c;
                buf[len] = '\0';
            }
        if (IsKeyPressed(KEY_BACKSPACE) && len > 0)
            buf[--len] = '\0';
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if (len > 0)
                arc_strlcpy(app->player, buf, sizeof(app->player));
            return SCR_MENU;
        }
        backdrop_update(false, false, 0, 0);
        BeginDrawing();
        gfx_backdrop(t);
        backdrop_draw(0.55f);
        draw_title("a collection of small, strange games");
        DrawRectangleRounded((Rectangle){390, 290, 500, 150}, 0.1f, 8, Fade(ARC_COL_PANEL, 0.92f));
        gfx_text_center("Enter your name", ARC_SCREEN_W / 2, 310, 26, ARC_COL_TEXT);
        char shown[LB_NAME_LEN + 2];
        snprintf(shown, sizeof(shown), "%s%s", buf, ((int)(t * 2.0f) & 1) ? "_" : " ");
        gfx_text_center(shown, ARC_SCREEN_W / 2, 358, 34, ARC_COL_YELLOW);
        gfx_text_center(TextFormat("[Enter] continue   (empty = keep \"%s\")", app->player), ARC_SCREEN_W / 2,
                        408, 16, ARC_COL_DIM);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- menu */
static void draw_card(const GameDef *g, int y, bool selected) {
    Rectangle r = {CARD_X, (float)y, CARD_W, CARD_H};
    DrawRectangleRounded(r, 0.2f, 8, Fade(ARC_COL_PANEL, selected ? 0.96f : 0.80f));
    if (selected)
        DrawRectangleRoundedLinesEx(r, 0.2f, 8, 2.0f, ARC_COL_CYAN);
    DrawText(g->title, CARD_X + 16, y + 8, 24, selected ? ARC_COL_CYAN : ARC_COL_TEXT);
    DrawText(g->tagline, CARD_X + 16, y + 38, 16, ARC_COL_DIM);
    if (!g->ranked)
        gfx_text_right("unranked", CARD_X + CARD_W - 14, y + 10, 14, ARC_COL_DIM);
}

Screen screen_menu(AppState *app) {
    int first = 0;
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        Vector2 m = GetMousePosition();
        bool over_cards = m.x < CARD_X + CARD_W + 20;

        /* keyboard selection */
        if (app->game_count > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
                app->selected = (app->selected + 1) % app->game_count;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
                app->selected = (app->selected + app->game_count - 1) % app->game_count;
            /* mouse hover selection */
            if (over_cards)
                for (int i = 0; i < MENU_VISIBLE && first + i < app->game_count; i++) {
                    int y = CARD_Y0 + i * CARD_PITCH;
                    if (m.y >= (float)y && m.y < (float)(y + CARD_H) && (m.x != 0.0f))
                        if (GetMouseDelta().x != 0.0f || GetMouseDelta().y != 0.0f)
                            app->selected = first + i;
                }
            if (app->selected < first)
                first = app->selected;
            if (app->selected >= first + MENU_VISIBLE)
                first = app->selected - MENU_VISIBLE + 1;
        }

        bool click_card = over_cards && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && m.y >= CARD_Y0 &&
                          m.y < CARD_Y0 + MENU_VISIBLE * CARD_PITCH;
        if (app->game_count > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || click_card)) {
            app->launch = app->games[app->selected];
            return SCR_LAUNCH;
        }
        if (IsKeyPressed(KEY_L))
            return SCR_LEADERBOARD;
        if (IsKeyPressed(KEY_N))
            return SCR_NAME;
        if (IsKeyPressed(KEY_ESCAPE))
            return SCR_QUIT;

        bool paint = !over_cards;
        backdrop_update(paint && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                        paint && IsMouseButtonDown(MOUSE_BUTTON_RIGHT), (int)m.x, (int)m.y);

        BeginDrawing();
        gfx_backdrop(t);
        backdrop_draw(0.85f);
        draw_title("pick a game");
        for (int i = 0; i < MENU_VISIBLE && first + i < app->game_count; i++)
            draw_card(app->games[first + i], CARD_Y0 + i * CARD_PITCH, first + i == app->selected);
        if (app->game_count > MENU_VISIBLE) {
            char pos[32];
            snprintf(pos, sizeof(pos), "%d / %d", app->selected + 1, app->game_count);
            DrawText(pos, CARD_X + CARD_W - 60, 92, 16, ARC_COL_DIM);
        }

        /* info panel for the selected game */
        if (app->game_count > 0) {
            const GameDef *g = app->games[app->selected];
            DrawRectangleRounded((Rectangle){SAND_X, 118, 592, 150}, 0.1f, 8, Fade(ARC_COL_PANEL, 0.82f));
            DrawText(g->title, SAND_X + 18, 130, 26, ARC_COL_YELLOW);
            (void)draw_wrapped(g->controls, SAND_X + 18, 166, 17, 556, ARC_COL_TEXT);
            DrawText(g->ranked ? "ranked: your score goes to the leaderboard" : "unranked: just for fun",
                     SAND_X + 18, 242, 15, ARC_COL_DIM);
        } else {
            gfx_text_center("No games registered - see docs/ADDING_A_GAME.md", ARC_SCREEN_W / 2, 300, 22,
                            ARC_COL_RED);
        }
        DrawText("left mouse: sand   right mouse: water", SAND_X + 4, ARC_SCREEN_H - 62, 16, ARC_COL_DIM);

        DrawText(TextFormat("player: %s", app->player), SAND_X + 4, 40, 20, ARC_COL_GREEN);
        DrawText("[Up/Down] choose  [Enter] play  [L] leaderboard  [N] change name  [Esc] quit", 48,
                 ARC_SCREEN_H - 30, 16, ARC_COL_DIM);
        gfx_text_right("v" ARCADE_VERSION_STRING, ARC_SCREEN_W - 16, ARC_SCREEN_H - 30, 16, ARC_COL_DIM);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- launch + result */
Screen screen_launch(AppState *app) {
    const GameDef *g = app->launch;
    if (g == NULL)
        return SCR_MENU;

    GameContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.player = app->player;
    ctx.seed = (unsigned)time(NULL) + app->launch_counter++;
    ctx.data_dir = app->data_dir;
    ctx.assets_dir = app->assets_dir;

    GameResult r = g->play(&ctx);
    arcade_flush_input();
    if (arcade_quit_requested())
        return SCR_QUIT;
    if (!r.finished)
        return SCR_MENU; /* player pressed ESC: nothing is recorded */

    app->last_game = g;
    app->last_result = r;
    app->last_rank = 0;
    if (g->ranked) {
        lb_add(&app->lb, g->id, app->player, r.score, (long long)time(NULL));
        if (!lb_save(&app->lb, app->scores_path))
            fprintf(stderr, "warning: could not write %s\n", app->scores_path);
        ScoreEntry top[LB_MAX_ENTRIES];
        int n = lb_top_for_game(&app->lb, g->id, top, LB_MAX_ENTRIES);
        for (int i = 0; i < n; i++)
            if (top[i].score <= r.score) {
                app->last_rank = i + 1;
                break;
            }
    }
    return SCR_RESULT;
}

Screen screen_result(AppState *app) {
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
            return SCR_MENU;
        if (IsKeyPressed(KEY_R)) {
            app->launch = app->last_game;
            return SCR_LAUNCH;
        }
        if (IsKeyPressed(KEY_L))
            return SCR_LEADERBOARD;
        backdrop_update(false, false, 0, 0);
        BeginDrawing();
        gfx_backdrop(t);
        backdrop_draw(0.5f);
        DrawRectangleRounded((Rectangle){360, 190, 560, 340}, 0.08f, 8, Fade(ARC_COL_PANEL, 0.94f));
        gfx_text_center(app->last_game->title, ARC_SCREEN_W / 2, 214, 30, ARC_COL_CYAN);
        gfx_text_center("GAME OVER", ARC_SCREEN_W / 2, 262, 22, ARC_COL_DIM);
        gfx_text_center(TextFormat("%d", app->last_result.score), ARC_SCREEN_W / 2, 306, 80, ARC_COL_YELLOW);
        if (app->last_game->ranked && app->last_rank > 0)
            gfx_text_center(app->last_rank == 1 ? "NEW HIGH SCORE!"
                                                : TextFormat("rank #%d for this game", app->last_rank),
                            ARC_SCREEN_W / 2, 410, 24, ARC_COL_GREEN);
        gfx_text_center("[R] play again   [Enter] menu   [L] leaderboard", ARC_SCREEN_W / 2, 480, 18,
                        ARC_COL_DIM);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- leaderboard */
Screen screen_leaderboard(AppState *app) {
    int tab = 0; /* 0 = all games, 1.. = one game from the registry */
    bool searching = false;
    char query[LB_NAME_LEN] = "";
    int qlen = 0;
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        if (searching) {
            for (int c = GetCharPressed(); c != 0; c = GetCharPressed())
                if (c >= 32 && c < 127 && c != '|' && qlen < LB_NAME_LEN - 1) {
                    query[qlen++] = (char)c;
                    query[qlen] = '\0';
                }
            if (IsKeyPressed(KEY_BACKSPACE) && qlen > 0)
                query[--qlen] = '\0';
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE))
                searching = false;
        } else {
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER))
                return SCR_MENU;
            if (IsKeyPressed(KEY_RIGHT))
                tab = (tab + 1) % (app->game_count + 1);
            if (IsKeyPressed(KEY_LEFT))
                tab = (tab + app->game_count) % (app->game_count + 1);
            if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_SLASH)) {
                searching = true;
                qlen = 0;
                query[0] = '\0';
                (void)GetCharPressed(); /* swallow the key that opened the search box */
            }
        }
        backdrop_update(false, false, 0, 0);

        /* build the rows to show */
        ScoreEntry rows[10];
        int n = 0;
        const char *heading;
        if (searching || qlen > 0) {
            int idx[10];
            int found = lb_find_player(&app->lb, query, idx, 10);
            for (int i = 0; i < found; i++)
                rows[n++] = app->lb.items[idx[i]];
            heading = "search results";
        } else if (tab == 0) {
            g_view = app->lb;
            lb_sort(&g_view, LB_SORT_SCORE_DESC);
            n = g_view.count < 10 ? g_view.count : 10;
            memcpy(rows, g_view.items, (size_t)n * sizeof(ScoreEntry));
            heading = "all games";
        } else {
            n = lb_top_for_game(&app->lb, app->games[tab - 1]->id, rows, 10);
            heading = app->games[tab - 1]->title;
        }

        BeginDrawing();
        gfx_backdrop(t);
        backdrop_draw(0.4f);
        draw_title("leaderboard");
        DrawRectangleRounded((Rectangle){180, 120, 920, 520}, 0.04f, 8, Fade(ARC_COL_PANEL, 0.93f));
        gfx_text_center(heading, ARC_SCREEN_W / 2, 138, 28, ARC_COL_YELLOW);
        DrawText("#", 220, 190, 18, ARC_COL_DIM);
        DrawText("PLAYER", 270, 190, 18, ARC_COL_DIM);
        DrawText("GAME", 520, 190, 18, ARC_COL_DIM);
        gfx_text_right("SCORE", 1060, 190, 18, ARC_COL_DIM);
        for (int i = 0; i < n; i++) {
            int y = 224 + i * 38;
            Color c = i == 0 ? ARC_COL_YELLOW : ARC_COL_TEXT;
            DrawText(TextFormat("%d", i + 1), 220, y, 22, c);
            DrawText(rows[i].player, 270, y, 22, c);
            DrawText(rows[i].game, 520, y, 22, ARC_COL_DIM);
            gfx_text_right(TextFormat("%d", rows[i].score), 1060, y, 22, c);
        }
        if (n == 0)
            gfx_text_center("no scores yet - go play something!", ARC_SCREEN_W / 2, 330, 22, ARC_COL_DIM);
        if (searching || qlen > 0)
            gfx_text_center(TextFormat("find player: %s%s", query, searching ? "_" : ""), ARC_SCREEN_W / 2,
                            600, 20, ARC_COL_GREEN);
        else
            gfx_text_center("[Left/Right] switch game   [F] find player   [Esc] back", ARC_SCREEN_W / 2, 600,
                            18, ARC_COL_DIM);
        EndDrawing();
    }
    return SCR_QUIT;
}
