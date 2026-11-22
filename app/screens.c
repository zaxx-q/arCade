/* Hub screens: name entry, menu, launch, result, leaderboard. Each screen runs its own loop and
 * returns the next Screen to show (a tiny state machine driven from main.c).
 *
 * Look: a quiet left rail (wordmark + one panel of compact rows) over a live scene (see backdrop.c): by
 * default the Synthwave City game flying itself, or, switched with the button at the top right, the sand toy
 * with the vault that opens the archive.
 *
 * In the city, a click on the background hides the whole menu and hands the camera to the player; Esc brings
 * the menu back. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"
#include "arcade/gfx.h"
#include "arcade/str.h"
#include "arcade/version.h"

#define MARGIN 48 /* left/right screen margin shared by every hub screen */

/* menu list: one panel of single-line rows */
#define LIST_X MARGIN
#define LIST_Y 100
#define LIST_W 720
#define LIST_PAD 8 /* panel padding above the first and below the last row */
#define ROW_H 36   /* was a 64 px card with a second line of text */
#define DIV_H 30   /* the "archive" divider between the games/ rows and the deferred/ rows */
#define MENU_VISIBLE 10
#define TITLE_OFF 24 /* title column, from the panel's left edge */
#define TAG_OFF 250  /* tagline column */
#define HINT_SIZE 14
#define HINT_KEY_SIZE 13
#define HINT_GAP 22

static Leaderboard g_view; /* scratch copy for sorted views (28 KB: keep off the stack) */

typedef struct Hint {
    const char *key;  /* drawn on a keycap */
    const char *what; /* what the key does */
} Hint;

/* raylib wants a corner "roundness" (0..1 of half the short side); this turns a radius in pixels into that.
 */
static float round_for(Rectangle r, float radius_px) {
    float shortest = r.width < r.height ? r.width : r.height;
    return shortest > 0.0f ? arc_clampf(2.0f * radius_px / shortest, 0.0f, 1.0f) : 0.0f;
}

static void draw_panel(Rectangle r, float alpha) {
    float rd = round_for(r, 10.0f);
    DrawRectangleRounded(r, rd, 8, Fade(ARC_COL_PANEL, alpha));
    DrawRectangleRoundedLinesEx(r, rd, 8, 1.0f, Fade(ARC_COL_DIM, 0.30f));
}

static int hint_row_width(const Hint *hints, int n) {
    int w = 0;
    for (int i = 0; i < n; i++)
        w += gfx_text_width(hints[i].key, HINT_KEY_SIZE) + 14 + 8 + gfx_text_width(hints[i].what, HINT_SIZE) +
             (i + 1 < n ? HINT_GAP : 0);
    return w;
}

/* A row of footer hints: each is a small keycap plus what the key does. */
static void draw_hint_row(int x, int y, const Hint *hints, int n) {
    for (int i = 0; i < n; i++) {
        int kw = gfx_text_width(hints[i].key, HINT_KEY_SIZE) + 14;
        Rectangle cap = {(float)x, (float)y, (float)kw, 24.0f};
        DrawRectangleRounded(cap, round_for(cap, 6.0f), 6, Fade(ARC_COL_PANEL, 0.95f));
        DrawRectangleRoundedLinesEx(cap, round_for(cap, 6.0f), 6, 1.0f, Fade(ARC_COL_DIM, 0.60f));
        gfx_text(hints[i].key, x + 7, y + 2, HINT_KEY_SIZE, ARC_COL_TEXT);
        gfx_text(hints[i].what, x + kw + 8, y + 3, HINT_SIZE, ARC_COL_DIM);
        x += kw + 8 + gfx_text_width(hints[i].what, HINT_SIZE) + HINT_GAP;
    }
}

static void draw_hint_row_centered(int y, const Hint *hints, int n) {
    draw_hint_row((ARC_SCREEN_W - hint_row_width(hints, n)) / 2, y, hints, n);
}

static void draw_title(const char *sub) {
    gfx_text("arCade", MARGIN, 18, 44, ARC_COL_CYAN);
    if (sub != NULL)
        gfx_text(sub, MARGIN + 4, 80, 18, ARC_COL_DIM);
}

static void draw_player(const AppState *app) {
    int right = ARC_SCREEN_W - MARGIN;
    gfx_text_right(app->player, right, 30, 22, ARC_COL_GREEN);
    gfx_text_right("player", right - gfx_text_width(app->player, 22) - 12, 36, 14, ARC_COL_DIM);
}

/* Copies `src` into `out`, shortening it with "..." until it is at most max_w pixels wide. */
static void fit_text(const char *src, int size, int max_w, char *out, size_t cap) {
    arc_strlcpy(out, src, cap);
    if (gfx_text_width(out, size) <= max_w)
        return;
    char trial[160];
    for (size_t n = strlen(out); n > 3; n--) {
        out[n - 1] = '\0';
        snprintf(trial, sizeof(trial), "%s...", out);
        if (gfx_text_width(trial, size) <= max_w) {
            arc_strlcpy(out, trial, cap);
            return;
        }
    }
}

/* Draws `text` inside a column max_w pixels wide. Text that is too wide is drawn a little smaller first, so a
 * long board name keeps its last words (the difficulty); only if it is still too wide is it cut with "...".
 */
static void draw_in_column(const char *text, int x, int y, int size, int max_w, Color c) {
    int drawn = gfx_text_width(text, size) > max_w ? size - 4 : size;
    char cell[LB_GAME_LEN];
    fit_text(text, drawn, max_w, cell, sizeof(cell));
    gfx_text(cell, x, y + (size - drawn) / 2, drawn, c); /* the smaller text stays centred on the row */
}

/* ---------------------------------------------------------------- name entry */
static void draw_name_view(const AppState *app, const char *buf, float t) {
    gfx_backdrop(t);
    backdrop_draw(0.55f);
    draw_title("a collection of small, strange games");
    draw_panel((Rectangle){390, 290, 500, 150}, 0.92f);
    gfx_text_center("Enter your name", ARC_SCREEN_W / 2, 310, 26, ARC_COL_TEXT);
    char shown[LB_NAME_LEN + 2];
    snprintf(shown, sizeof(shown), "%s%s", buf, ((int)(t * 2.0f) & 1) ? "_" : " ");
    gfx_text_center(shown, ARC_SCREEN_W / 2, 352, 34, ARC_COL_YELLOW);
    gfx_text_center(TextFormat("empty keeps \"%s\"", app->player), ARC_SCREEN_W / 2, 398, 15, ARC_COL_DIM);
    const Hint hint[] = {{"Enter", "continue"}};
    draw_hint_row_centered(ARC_SCREEN_H - 38, hint, 1);
}

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
        draw_name_view(app, buf, t);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- backdrop switch */
/* The small button under the player's name that swaps the backdrop between the city and the sand scene. It is
 * only part of the menu: it is gone while the menu is hidden. */
#define CLICK_SLOP 6.0f /* px the pointer may move while the click that hid the menu is still down */
#define SWITCH_Y 70
#define SWITCH_H 32

static const char *switch_label(void) {
    return backdrop_mode() == BACKDROP_CITY ? "switch to sandbox" : "switch to city";
}

static int switch_cap_w(void) {
    return gfx_text_width("B", HINT_KEY_SIZE) + 14; /* the keycap: same recipe as the footer hints */
}

static Rectangle switch_rect(void) {
    int w = 10 + switch_cap_w() + 8 + gfx_text_width(switch_label(), HINT_SIZE) + 14;
    return (Rectangle){(float)(ARC_SCREEN_W - MARGIN - w), (float)SWITCH_Y, (float)w, (float)SWITCH_H};
}

static void draw_switch(bool hot) {
    Rectangle r = switch_rect();
    float rd = round_for(r, 10.0f);
    DrawRectangleRounded(r, rd, 8, Fade(ARC_COL_PANEL, 0.92f));
    DrawRectangleRoundedLinesEx(r, rd, 8, 1.0f, hot ? Fade(ARC_COL_CYAN, 0.85f) : Fade(ARC_COL_DIM, 0.45f));
    Rectangle cap = {r.x + 10.0f, r.y + 4.0f, (float)switch_cap_w(), 24.0f};
    DrawRectangleRounded(cap, round_for(cap, 6.0f), 6, Fade(ARC_COL_BG, 0.90f));
    DrawRectangleRoundedLinesEx(cap, round_for(cap, 6.0f), 6, 1.0f, Fade(ARC_COL_DIM, 0.60f));
    gfx_text("B", (int)cap.x + 7, (int)cap.y + 2, HINT_KEY_SIZE, ARC_COL_TEXT);
    gfx_text(switch_label(), (int)(cap.x + cap.width) + 8, (int)r.y + 8, HINT_SIZE,
             hot ? ARC_COL_TEXT : ARC_COL_DIM);
}

/* ---------------------------------------------------------------- menu */
/* Layout rules, shared by drawing and mouse hit-testing so they can never disagree:
 *  - rows 0..listed_count-1 come from games/; once the archive is open, the deferred/ rows follow a divider;
 *  - at most MENU_VISIBLE rows show at once; `first` is the index of the top one;
 *  - the panel grows to fit its rows. It eases to its new height, so opening the archive unfolds the list. */
static float g_panel_h; /* animated panel height in px; 0 = use the target (first frame, previews) */

static int window_rows(const AppState *app) {
    return arc_mini(app->game_count, MENU_VISIBLE);
}

/* Is the divider inside the visible window? (Scrolled past it, it stays pinned at the top as a header.) */
static bool divider_shown(const AppState *app, int first) {
    return app->archive_open && app->listed_count < app->game_count &&
           app->listed_count < first + window_rows(app);
}

static int divider_y(const AppState *app, int first) {
    return LIST_Y + LIST_PAD + arc_maxi(app->listed_count - first, 0) * ROW_H;
}

static int row_y(const AppState *app, int first, int i) {
    int y = LIST_Y + LIST_PAD + (i - first) * ROW_H;
    if (divider_shown(app, first) && i >= app->listed_count)
        y += DIV_H;
    return y;
}

static float panel_target_h(const AppState *app, int first) {
    return (float)(2 * LIST_PAD + window_rows(app) * ROW_H + (divider_shown(app, first) ? DIV_H : 0));
}

/* Keeps the selected row inside the window of MENU_VISIBLE rows (the window only moves when it has to). */
static int first_for(const AppState *app, int first) {
    if (app->selected < first)
        first = app->selected;
    if (app->selected >= first + MENU_VISIBLE)
        first = app->selected - MENU_VISIBLE + 1;
    return arc_clampi(first, 0, arc_maxi(app->game_count - MENU_VISIBLE, 0));
}

/* One game: title, then its one-line hook in a second column. The old description box is gone. */
static void draw_row(const GameDef *g, int y, bool selected, bool archived) {
    Rectangle r = {(float)LIST_X + 6.0f, (float)y, (float)LIST_W - 12.0f, (float)ROW_H};
    if (selected) {
        DrawRectangleRounded(r, round_for(r, 7.0f), 6, Fade(ARC_COL_CYAN, 0.10f));
        DrawRectangle(LIST_X + 6, y + 8, 3, ROW_H - 16, ARC_COL_CYAN);
    }
    Color title_c = selected ? ARC_COL_CYAN : (archived ? Fade(ARC_COL_TEXT, 0.82f) : ARC_COL_TEXT);
    gfx_text(g->title, LIST_X + TITLE_OFF, y + 4, 20, title_c);

    char tag[128];
    int room =
        LIST_W - TAG_OFF - 30 - (g->ranked ? 0 : 70); /* unranked games reserve space for their label */
    fit_text(g->tagline, 15, room, tag, sizeof(tag));
    gfx_text(tag, LIST_X + TAG_OFF, y + 8, 15,
             selected ? ARC_COL_TEXT : ColorLerp(ARC_COL_DIM, ARC_COL_TEXT, 0.22f));
    if (!g->ranked)
        gfx_text_right("unranked", LIST_X + LIST_W - 24, y + 10, 13, ARC_COL_DIM);
}

static void draw_divider(int y, int count) {
    gfx_text("archive", LIST_X + TITLE_OFF, y + 5, 14, ARC_COL_DIM);
    int line_x = LIST_X + TITLE_OFF + gfx_text_width("archive", 14) + 14;
    DrawRectangle(line_x, y + DIV_H / 2 + 1, LIST_X + LIST_W - 24 - line_x, 1, Fade(ARC_COL_DIM, 0.35f));
    gfx_text_right(TextFormat("%d games", count), LIST_X + LIST_W - 24, y + 5, 13, ARC_COL_DIM);
}

static void draw_scrollbar(const AppState *app, int first, int panel_bottom) {
    int top = LIST_Y + LIST_PAD, h = panel_bottom - LIST_PAD - top;
    int thumb_h = arc_maxi(h * MENU_VISIBLE / app->game_count, 18);
    int thumb_y = top + (h - thumb_h) * first / arc_maxi(app->game_count - MENU_VISIBLE, 1);
    DrawRectangle(LIST_X + LIST_W - 10, top, 2, h, Fade(ARC_COL_DIM, 0.15f));
    DrawRectangle(LIST_X + LIST_W - 10, thumb_y, 2, thumb_h, Fade(ARC_COL_DIM, 0.70f));
}

static void draw_menu_view(const AppState *app, int first, float t) {
    gfx_backdrop(t);
    backdrop_draw(0.9f);
    backdrop_vault_draw(app->archive_open, t);
    draw_title(NULL);
    draw_player(app);
    draw_switch(CheckCollisionPointRec(GetMousePosition(), switch_rect()));

    if (app->game_count > 0) {
        float ph = g_panel_h > 0.0f ? g_panel_h : panel_target_h(app, first);
        int panel_bottom = LIST_Y + (int)ph;
        draw_panel((Rectangle){(float)LIST_X, (float)LIST_Y, (float)LIST_W, ph}, 0.90f);
        /* rows that do not fit yet (while the panel is still unfolding) are simply not drawn */
        for (int i = first; i < first + window_rows(app); i++) {
            int y = row_y(app, first, i);
            if (y + ROW_H > panel_bottom - LIST_PAD)
                break;
            draw_row(app->games[i], y, i == app->selected, i >= app->listed_count);
        }
        if (divider_shown(app, first) && divider_y(app, first) + DIV_H <= panel_bottom - LIST_PAD)
            draw_divider(divider_y(app, first), app->deferred_count);
        if (app->game_count > MENU_VISIBLE)
            draw_scrollbar(app, first, panel_bottom);

        if (app->archive_toast > 0.0f) { /* the reward for filling the vault: says what just happened */
            float a = arc_minf(1.0f, app->archive_toast / 0.8f);
            const char *msg = TextFormat("archive open: %d more games", app->deferred_count);
            Rectangle r = {(float)LIST_X, (float)panel_bottom + 14.0f, (float)gfx_text_width(msg, 16) + 32.0f,
                           34.0f};
            DrawRectangleRounded(r, round_for(r, 10.0f), 8, Fade(ARC_COL_PANEL, 0.92f * a));
            DrawRectangleRoundedLinesEx(r, round_for(r, 10.0f), 8, 1.0f, Fade(ARC_COL_GREEN, 0.60f * a));
            gfx_text(msg, LIST_X + 16, panel_bottom + 14 + 8, 16, Fade(ARC_COL_GREEN, a));
        }
    } else {
        gfx_text("No games registered - see docs/ADDING_A_GAME.md", LIST_X, LIST_Y + 20, 22, ARC_COL_RED);
    }

    /* what a click on the background does: it flies the city, or it paints the sand toy */
    bool city = backdrop_mode() == BACKDROP_CITY;
    const char *toy = city ? "click the background to fly" : "left mouse: sand    right mouse: water";
    int toy_w = gfx_text_width(toy, 15) + 28;
    Rectangle pill = {(float)(ARC_SCREEN_W - MARGIN - toy_w), (float)ARC_SCREEN_H - 84.0f, (float)toy_w,
                      32.0f};
    DrawRectangleRounded(pill, round_for(pill, 16.0f), 8, Fade(ARC_COL_BG, 0.88f));
    gfx_text_right(toy, ARC_SCREEN_W - MARGIN - 14, ARC_SCREEN_H - 78, 15, ARC_COL_DIM);

    Hint hints[6] = {{"Up/Down", "choose"}, {"Enter", "play"}, {"L", "leaderboard"}, {"N", "change name"}};
    int hint_n = 4;
    if (city) /* the city has a soundtrack to silence */
        hints[hint_n++] = (Hint){"M", "mute music"};
    hints[hint_n++] = (Hint){"Esc", "quit"};
    draw_hint_row(MARGIN, ARC_SCREEN_H - 38, hints, hint_n);
    gfx_text_right("v" ARCADE_VERSION_STRING, ARC_SCREEN_W - MARGIN, ARC_SCREEN_H - 35, 14, ARC_COL_DIM);
}

/* Index of the row under the mouse, or -1. */
static int row_at(const AppState *app, int first, Vector2 m) {
    if (m.x < (float)LIST_X || m.x >= (float)(LIST_X + LIST_W))
        return -1;
    for (int i = first; i < first + window_rows(app); i++) {
        int y = row_y(app, first, i);
        if (m.y >= (float)y && m.y < (float)(y + ROW_H))
            return i;
    }
    return -1;
}

Screen screen_menu(AppState *app) {
    int first = first_for(app, 0);
    float t = 0.0f;
    bool flying = false;     /* the menu is hidden and the player flies the city behind it */
    bool click_held = false; /* the click that hid the menu is still down, and has not moved yet */
    Vector2 click_at = {0.0f, 0.0f};
    arcade_flush_input();
    backdrop_set_flying(false); /* arriving (or coming back from a game): the menu is up, the drone flies */
    g_panel_h =
        panel_target_h(app, first); /* coming back from a game: no unfolding, just the panel as it was */
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        bool esc = IsKeyPressed(KEY_ESCAPE);

        if (flying) {
            if (esc) { /* Esc gives the menu back; the same press must not also quit the hub */
                flying = false;
                backdrop_set_flying(false);
                esc = false;
            } else {
                /* The whole screen is the game: no menu, no hints, just the city and its own HUD. The first
                 * frame after the click is where the camera changes hands (set_flying is idempotent). */
                backdrop_set_flying(true);
                ArcadeInput in;
                arcade_poll(&in);
                if (click_held) {
                    /* Holding the left button is a look-drag, and any flight control switches the drone off.
                     * So the click that hid the menu must not count as one while it is only a click, or
                     * every click would stop the tour. Once it is let go, or dragged away, it is the
                     * player's. */
                    float dx = in.aim_x - click_at.x, dy = in.aim_y - click_at.y;
                    if (in.fire && dx * dx + dy * dy < CLICK_SLOP * CLICK_SLOP)
                        in.fire = in.fire_pressed = false;
                    else
                        click_held = false;
                }
                backdrop_fly(&in);
                BeginDrawing();
                backdrop_draw(1.0f);
                EndDrawing();
                continue;
            }
        }

        /* the vault was filled with the sand toy: unlock the archive and announce it */
        if (!app->archive_open && backdrop_vault_is_open()) {
            app_open_archive(app);
            app->archive_toast = 4.0f;
        }
        if (app->archive_toast > 0.0f)
            app->archive_toast -= ARC_DT;
        float target = panel_target_h(app, first);
        g_panel_h += (target - g_panel_h) * 0.16f; /* ease towards the target height */
        if (fabsf(target - g_panel_h) < 0.5f)
            g_panel_h = target;

        Vector2 m = GetMousePosition();
        Rectangle panel_zone = {(float)LIST_X - 12.0f, (float)LIST_Y - 12.0f, (float)LIST_W + 24.0f,
                                g_panel_h + 24.0f};
        bool over_panel = CheckCollisionPointRec(m, panel_zone);
        int hover = row_at(app, first, m);

        /* selection: keyboard, mouse wheel, hover */
        if (app->game_count > 0) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
                app->selected = (app->selected + 1) % app->game_count;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
                app->selected = (app->selected + app->game_count - 1) % app->game_count;
            float wheel = GetMouseWheelMove();
            if (over_panel && wheel > 0.0f && app->selected > 0)
                app->selected--;
            if (over_panel && wheel < 0.0f && app->selected < app->game_count - 1)
                app->selected++;
            if (hover >= 0 && (GetMouseDelta().x != 0.0f || GetMouseDelta().y != 0.0f))
                app->selected = hover;
            first = first_for(app, first);
        }

        if (app->game_count > 0 && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
                                    (hover >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))) {
            if (hover >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                app->selected = hover;
            app->launch = app->games[app->selected];
            return SCR_LAUNCH;
        }
        if (IsKeyPressed(KEY_L))
            return SCR_LEADERBOARD;
        if (IsKeyPressed(KEY_N))
            return SCR_NAME;
        if (esc)
            return SCR_QUIT;

        /* the backdrop: B or the small button swaps the city and the sand toy */
        bool city = backdrop_mode() == BACKDROP_CITY;
        bool over_switch = CheckCollisionPointRec(m, switch_rect());
        if (IsKeyPressed(KEY_B) || (over_switch && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
            backdrop_set_mode(city ? BACKDROP_SANDBOX : BACKDROP_CITY);
        else if (city && IsKeyPressed(KEY_M))
            backdrop_toggle_mute();
        else if (city && !over_panel && !over_switch && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            flying = true; /* a click on the background: the menu hides (from the next frame) */
            click_held = true;
            click_at = m;
        }

        bool paint = !over_panel && !over_switch;
        backdrop_update(paint && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                        paint && IsMouseButtonDown(MOUSE_BUTTON_RIGHT), (int)m.x, (int)m.y);

        BeginDrawing();
        draw_menu_view(app, first, t);
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
    char board[GAME_BOARD_LEN] = ""; /* a game with one table per mode/level names this run's table in it */
    ctx.board = board;

    arcade_flush_input(); /* consume the Enter/click that launched the game, or its first frame would see it
                             too */
    backdrop_pause_audio(true); /* the city's soundtrack must not play under another game */
    GameResult r = g->play(&ctx);
    backdrop_pause_audio(false);
    arcade_flush_input();
    if (arcade_quit_requested())
        return SCR_QUIT;
    if (!r.finished)
        return SCR_MENU; /* player pressed ESC: nothing is recorded */

    app->last_game = g;
    app->last_result = r;
    app->last_rank = 0;
    arc_strlcpy(app->last_board, board, sizeof(app->last_board));
    if (g->ranked) {
        /* the score goes into the table of this mode/level ("beat_dodge:Dodge / Bopeebo / normal"), or into
         * the game's one table ("echo_maze") when the game did not name a board */
        char key[LB_GAME_LEN];
        lb_board_key(key, sizeof(key), g->id, board);
        lb_add(&app->lb, key, app->player, r.score, (long long)time(NULL));
        if (!lb_save(&app->lb, app->scores_path))
            fprintf(stderr, "warning: could not write %s\n", app->scores_path);
        ScoreEntry top[LB_MAX_ENTRIES];
        int n = lb_top_for_game(&app->lb, key, top, LB_MAX_ENTRIES); /* rank among THIS table only */
        for (int i = 0; i < n; i++)
            if (top[i].score <= r.score) {
                app->last_rank = i + 1;
                break;
            }
    }
    return SCR_RESULT;
}

static void draw_result_view(const AppState *app, float t) {
    gfx_backdrop(t);
    backdrop_draw(0.5f);
    draw_title(NULL);
    draw_panel((Rectangle){360, 190, 560, 340}, 0.94f);
    gfx_text_center(app->last_game->title, ARC_SCREEN_W / 2, 214, 30, ARC_COL_CYAN);
    gfx_text_center("Game over", ARC_SCREEN_W / 2, 262, 22, ARC_COL_DIM);
    gfx_text_center(TextFormat("%d", app->last_result.score), ARC_SCREEN_W / 2, 306, 80, ARC_COL_YELLOW);
    bool boarded = app->last_board[0] != '\0'; /* the rank is within one mode/level, not the whole game */
    if (app->last_game->ranked && app->last_rank > 0)
        gfx_text_center(
            app->last_rank == 1
                ? "New high score!"
                : TextFormat(boarded ? "rank #%d on this board" : "rank #%d for this game", app->last_rank),
            ARC_SCREEN_W / 2, 410, 24, ARC_COL_GREEN);
    if (boarded) {
        char label[GAME_BOARD_LEN];
        fit_text(app->last_board, 18, 500, label, sizeof(label));
        gfx_text_center(label, ARC_SCREEN_W / 2, 442, 18, ARC_COL_DIM);
    }
    const Hint hints[] = {{"R", "play again"}, {"Enter", "menu"}, {"L", "leaderboard"}};
    draw_hint_row_centered(478, hints, (int)ARC_ARRAY_LEN(hints));
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
        draw_result_view(app, t);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- leaderboard */
/* A game with boards (Beat Dodge & Catch: one table per mode, song and difficulty) gets a second level: on
 * its tab Up/Down step through "all boards" and then each table that has scores. `board` is 0 for "all
 * boards", or 1.. for g_boards[board - 1]. Games without boards never show the selector. */
static char g_boards[LB_MAX_BOARDS][LB_GAME_LEN]; /* 8 KB: static, like g_view */

/* Lists the tables of the game on `tab` in g_boards and returns how many. Returns 0 (and the selector stays
 * hidden) for the "all games" tab and for a game whose scores are all in one plain table. */
static int tab_boards(const AppState *app, int tab) {
    if (tab <= 0 || tab > app->game_count)
        return 0;
    int n = lb_boards_for_game(&app->lb, app->games[tab - 1]->id, g_boards, LB_MAX_BOARDS);
    for (int i = 0; i < n; i++)
        if (lb_key_board(g_boards[i])[0] != '\0')
            return n;
    return 0;
}

static void draw_leaderboard_view(AppState *app, int tab, int board, bool searching, const char *query,
                                  float t) {
    /* build the rows to show */
    ScoreEntry rows[10];
    int n = 0;
    const char *heading;
    char sub[GAME_BOARD_LEN + 24] = ""; /* small line under the heading: which board is shown */
    int boards = 0;                     /* tables of this game (0 = no selector) */
    bool one_table = false;             /* every row is from the same table: no need for a game column */
    if (searching || query[0] != '\0') {
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
        const GameDef *g = app->games[tab - 1];
        heading = g->title;
        boards = tab_boards(app, tab);
        if (boards > 0 && board >= 1 && board <= boards) {
            const char *name = lb_key_board(g_boards[board - 1]);
            n = lb_top_for_game(&app->lb, g_boards[board - 1], rows, 10);
            one_table = true;
            snprintf(sub, sizeof(sub), "board %d/%d: %s", board, boards,
                     name[0] != '\0' ? name : "older scores");
        } else {
            n = lb_top_for_all_boards(&app->lb, g->id, rows,
                                      10); /* the same as before for games without boards */
            if (boards > 0)
                snprintf(sub, sizeof(sub), "all boards (%d)", boards);
        }
    }

    gfx_backdrop(t);
    backdrop_draw(0.4f);
    draw_title("leaderboard");
    draw_panel((Rectangle){180, 120, 920, 520}, 0.93f);
    gfx_text_center(heading, ARC_SCREEN_W / 2, 130, 28, ARC_COL_YELLOW);
    if (sub[0] != '\0')
        gfx_text_center(sub, ARC_SCREEN_W / 2, 166, 17, ARC_COL_CYAN);
    gfx_text("#", 220, 190, 18, ARC_COL_DIM);
    gfx_text("player", 270, 190, 18, ARC_COL_DIM);
    if (!one_table)
        gfx_text(boards > 0 ? "board" : "game", 520, 190, 18, ARC_COL_DIM);
    gfx_text_right("score", 1060, 190, 18, ARC_COL_DIM);
    for (int i = 0; i < n; i++) {
        int y = 224 + i * 38;
        Color c = i == 0 ? ARC_COL_YELLOW : ARC_COL_TEXT;
        gfx_text(TextFormat("%d", i + 1), 220, y, 22, c);
        gfx_text(rows[i].player, 270, y, 22, c);
        if (!one_table) {
            /* a plain game id, or "id:board" fitted so that it never runs into the score; on a game's own tab
             * the board name alone is enough */
            const char *shown = rows[i].game;
            if (boards > 0) /* a game with boards: the board name, or "older scores" for the plain id */
                shown = lb_key_board(shown)[0] != '\0' ? lb_key_board(shown) : "older scores";
            draw_in_column(shown, 520, y, 22, 440, ARC_COL_DIM);
        }
        gfx_text_right(TextFormat("%d", rows[i].score), 1060, y, 22, c);
    }
    if (n == 0)
        gfx_text_center("no scores yet - go play something!", ARC_SCREEN_W / 2, 330, 22, ARC_COL_DIM);
    if (searching || query[0] != '\0')
        gfx_text_center(TextFormat("find player: %s%s", query, searching ? "_" : ""), ARC_SCREEN_W / 2, 600,
                        20, ARC_COL_GREEN);
    else {
        Hint hints[4];
        int hn = 0;
        hints[hn++] = (Hint){"Left/Right", "switch game"};
        if (boards > 0)
            hints[hn++] = (Hint){"Up/Down", "switch board"};
        hints[hn++] = (Hint){"F", "find player"};
        hints[hn++] = (Hint){"Esc", "back"};
        draw_hint_row_centered(596, hints, hn);
    }
}

Screen screen_leaderboard(AppState *app) {
    int tab = 0;   /* 0 = all games, 1.. = one game from the registry */
    int board = 0; /* on a game with boards: 0 = all boards, 1.. = one board (see tab_boards) */
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
            if (IsKeyPressed(KEY_RIGHT)) {
                tab = (tab + 1) % (app->game_count + 1);
                board = 0; /* every game starts on "all boards" */
            }
            if (IsKeyPressed(KEY_LEFT)) {
                tab = (tab + app->game_count) % (app->game_count + 1);
                board = 0;
            }
            int boards = tab_boards(app, tab);
            if (boards > 0 && IsKeyPressed(KEY_DOWN))
                board = (board + 1) % (boards + 1);
            if (boards > 0 && IsKeyPressed(KEY_UP))
                board = (board + boards) % (boards + 1);
            if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_SLASH)) {
                searching = true;
                qlen = 0;
                query[0] = '\0';
                (void)GetCharPressed(); /* swallow the key that opened the search box */
            }
        }
        backdrop_update(false, false, 0, 0);

        BeginDrawing();
        draw_leaderboard_view(app, tab, board, searching, query, t);
        EndDrawing();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- previews (smoke runner / screenshots) */
const char *screens_preview_name(int which) {
    static const char *const names[PREVIEW_COUNT] = {
        "hub_name",   "hub_menu",        "hub_menu_sandbox",      "hub_archive", "hub_archive_end",
        "hub_result", "hub_leaderboard", "hub_leaderboard_board", "hub_fly"};
    return (which >= 0 && which < PREVIEW_COUNT) ? names[which] : "hub_unknown";
}

void screens_draw_preview(AppState *app, int which, float t) {
    switch (which) {
    case PREVIEW_NAME:
        draw_name_view(app, "Ana", t);
        break;
    case PREVIEW_MENU:
    case PREVIEW_MENU_SANDBOX:
    case PREVIEW_ARCHIVE:
    case PREVIEW_ARCHIVE_END:
        g_panel_h = 0.0f; /* draw at the final height: previews do not animate */
        draw_menu_view(app, first_for(app, 0), t);
        break;
    case PREVIEW_RESULT:
        draw_result_view(app, t);
        break;
    case PREVIEW_LEADERBOARD:
        draw_leaderboard_view(app, 0, 0, false, "", t);
        break;
    case PREVIEW_LEADERBOARD_BOARD: {          /* the first game that has boards, on its second board */
        int tab = app->game_count > 0 ? 1 : 0; /* tab 0 = all games, also what an empty registry shows */
        while (tab > 0 && tab < app->game_count && tab_boards(app, tab) == 0)
            tab++;
        draw_leaderboard_view(app, tab, tab_boards(app, tab) > 1 ? 2 : 0, false, "", t);
        break;
    }
    case PREVIEW_FLY:
        backdrop_draw(1.0f); /* the menu is hidden: only the game */
        break;
    default:
        break;
    }
}
