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
#include "online.h"
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

typedef struct Hint {
    const char *key;  /* drawn on a keycap */
    const char *what; /* what the key does */
    int tap;          /* the key a tap on this hint presses on a touch screen (0 = not tappable) */
    int tap2;         /* a hint of two keys ("Left/Right"): a tap on its right half presses this one */
} Hint;

/* "Esc" on a computer; on a phone the same key is the back button of the system. */
static const char *esc_label(void) {
    return arcade_touch_ui() ? "Back" : "Esc";
}

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

/* On a touch screen a hint is also a button. The tap zone is much bigger than the keycap (a thumb is not a
 * mouse pointer): half a gap to each side and 64 px high. */
static void add_hint_zones(const Hint *h, int x, int y, int w) {
    if (h->tap == 0)
        return;
    Rectangle z = {(float)(x - HINT_GAP / 2), (float)(y - 20), (float)(w + HINT_GAP), 64.0f};
    if (h->tap2 == 0) {
        arcade_touch_key_zone(z, h->tap);
        return;
    }
    z.width /= 2.0f;
    arcade_touch_key_zone(z, h->tap);
    z.x += z.width;
    arcade_touch_key_zone(z, h->tap2);
}

/* A row of footer hints: each is a small keycap plus what the key does. */
static void draw_hint_row(int x, int y, const Hint *hints, int n) {
    for (int i = 0; i < n; i++) {
        int kw = gfx_text_width(hints[i].key, HINT_KEY_SIZE) + 14;
        add_hint_zones(&hints[i], x, y, kw + 8 + gfx_text_width(hints[i].what, HINT_SIZE));
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
/* The hub starts without a name (main.c leaves app->player empty), and this screen does not let go until one
 * is typed, so nobody lands on the boards under a default. Later (N in the menu) a player who already has a
 * name may press Enter on an empty line to keep it. A name needs a visible character: spaces alone would show
 * as an empty row on the boards. */
#define NAME_NUDGE_SECS 0.7f /* how long the "a name is required" line stays red after an empty Enter */

static bool has_visible_char(const char *s) {
    for (; *s != '\0'; s++)
        if (*s != ' ')
            return true;
    return false;
}

/* Copies `src` into `dst` without the spaces at either end. */
static void copy_trimmed(char *dst, size_t cap, const char *src) {
    while (*src == ' ')
        src++;
    size_t n = strlen(src);
    while (n > 0 && src[n - 1] == ' ')
        n--;
    if (n >= cap)
        n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* With the on-screen keyboard open (it covers the bottom half) the panel moves up out of its way. */
#define NAME_OSK_SHIFT (-150)

static void draw_name_view(const AppState *app, const char *buf, float nudge, float t) {
    const bool required = app->player[0] == '\0'; /* no name yet: an empty line cannot be accepted */
    const int dy = arcade_osk_is_open() ? NAME_OSK_SHIFT : 0;
    gfx_backdrop(t);
    backdrop_draw(0.55f);
    draw_title("a collection of small, strange games");
    draw_panel((Rectangle){390, (float)(290 + dy), 500, 150}, 0.92f);
    gfx_text_center("Enter your name", ARC_SCREEN_W / 2, 310 + dy, 26, ARC_COL_TEXT);
    char shown[LB_NAME_LEN + 2];
    snprintf(shown, sizeof(shown), "%s%s", buf, ((int)(t * 2.0f) & 1) ? "_" : " ");
    gfx_text_center(shown, ARC_SCREEN_W / 2, 352 + dy, 34, ARC_COL_YELLOW);
    if (required)
        gfx_text_center("type a name to continue", ARC_SCREEN_W / 2, 398 + dy, 15,
                        nudge > 0.0f ? ARC_COL_RED : ARC_COL_DIM);
    else
        gfx_text_center(TextFormat("empty keeps \"%s\"", app->player), ARC_SCREEN_W / 2, 398 + dy, 15,
                        ARC_COL_DIM);
    /* Enter only shows as a key once it would do something; the on-screen keyboard has its own Enter */
    if (!arcade_osk_is_open() && (!required || has_visible_char(buf))) {
        const Hint hint[] = {{"Enter", "continue", KEY_ENTER, 0}};
        draw_hint_row_centered(ARC_SCREEN_H - 38, hint, 1);
    }
}

Screen screen_name(AppState *app) {
    char buf[LB_NAME_LEN] = ""; /* starts empty: typing replaces the current name */
    int len = 0;
    float t = 0.0f;
    float nudge = 0.0f; /* seconds left of the red "type a name" line after Enter on an empty line */
    arcade_flush_input();
    arcade_osk_open(true); /* a touch screen has no keyboard (the next screen's flush closes it again) */
    while (!arcade_quit_requested()) {
        online_tick(app);
        t += ARC_DT;
        if (nudge > 0.0f)
            nudge -= ARC_DT;
        for (int c = arcade_text_char(); c != 0; c = arcade_text_char())
            if (c >= 32 && c < 127 && c != '|' && len < LB_NAME_LEN - 1) {
                buf[len++] = (char)c;
                buf[len] = '\0';
            }
        if (arcade_key_pressed(KEY_BACKSPACE) && len > 0)
            buf[--len] = '\0';
        if (arcade_key_pressed(KEY_ENTER) || arcade_key_pressed(KEY_KP_ENTER)) {
            if (has_visible_char(buf)) {
                copy_trimmed(app->player, sizeof(app->player), buf);
                return SCR_MENU;
            }
            if (app->player[0] != '\0') /* changing the name with N: an empty line keeps the current one */
                return SCR_MENU;
            nudge = NAME_NUDGE_SECS; /* the first name is required: stay here */
        }
        backdrop_update(false, false, 0, 0);
        arcade_frame_begin();
        draw_name_view(app, buf, nudge, t);
        arcade_frame_end();
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
    fit_text(g->tagline, 15, LIST_W - TAG_OFF - 30, tag, sizeof(tag));
    gfx_text(tag, LIST_X + TAG_OFF, y + 8, 15,
             selected ? ARC_COL_TEXT : ColorLerp(ARC_COL_DIM, ARC_COL_TEXT, 0.22f));
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
    if (arcade_touch_ui()) /* no flying without a keyboard (see screen_menu), and no right button for water */
        toy = city ? "" : "drag a finger to pour sand";
    if (toy[0] != '\0') {
        int toy_w = gfx_text_width(toy, 15) + 28;
        Rectangle pill = {(float)(ARC_SCREEN_W - MARGIN - toy_w), (float)ARC_SCREEN_H - 84.0f, (float)toy_w,
                          32.0f};
        DrawRectangleRounded(pill, round_for(pill, 16.0f), 8, Fade(ARC_COL_BG, 0.88f));
        gfx_text_right(toy, ARC_SCREEN_W - MARGIN - 14, ARC_SCREEN_H - 78, 15, ARC_COL_DIM);
    }

    Hint hints[6];
    int hint_n = 0;
    if (!arcade_touch_ui()) { /* a finger taps a row: no cursor to move */
        hints[hint_n++] = (Hint){"Up/Down", "choose", 0, 0};
        hints[hint_n++] = (Hint){"Enter", "play", 0, 0};
    }
    hints[hint_n++] = (Hint){"L", "leaderboard", KEY_L, 0};
    hints[hint_n++] = (Hint){"N", "change name", KEY_N, 0};
    if (city) /* the city has a soundtrack to silence */
        hints[hint_n++] = (Hint){"M", "mute music", KEY_M, 0};
    hints[hint_n++] = (Hint){esc_label(), "quit", KEY_ESCAPE, 0};
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
        online_tick(app);
        t += ARC_DT;
        bool esc = arcade_key_pressed(KEY_ESCAPE);

        if (flying) {
            if (esc) {
                if (backdrop_city_grabbed()) {
                    /* ESC frees the mouse first; the next ESC gives the menu back */
                    backdrop_city_mouse_free();
                } else {
                    flying = false;
                    backdrop_set_flying(false);
                }
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
                arcade_frame_begin();
                backdrop_draw(1.0f);
                arcade_frame_end();
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
            if (arcade_key_pressed(KEY_DOWN) || arcade_key_pressed(KEY_S))
                app->selected = (app->selected + 1) % app->game_count;
            if (arcade_key_pressed(KEY_UP) || arcade_key_pressed(KEY_W))
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

        if (app->game_count > 0 && (arcade_key_pressed(KEY_ENTER) || arcade_key_pressed(KEY_KP_ENTER) ||
                                    (hover >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))) {
            if (hover >= 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
                app->selected = hover;
            app->launch = app->games[app->selected];
            return SCR_LAUNCH;
        }
        if (arcade_key_pressed(KEY_L))
            return SCR_LEADERBOARD;
        if (arcade_key_pressed(KEY_N))
            return SCR_NAME;
        if (esc)
            return SCR_QUIT;

        /* the backdrop: B or the small button swaps the city and the sand toy */
        bool city = backdrop_mode() == BACKDROP_CITY;
        bool over_switch = CheckCollisionPointRec(m, switch_rect());
        if (arcade_key_pressed(KEY_B) || (over_switch && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))
            backdrop_set_mode(city ? BACKDROP_SANDBOX : BACKDROP_CITY);
        else if (city && arcade_key_pressed(KEY_M))
            backdrop_toggle_mute();
        else if (city && !arcade_touch_ui() && !over_panel && !over_switch &&
                 IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            flying = true; /* a click on the background: the menu hides (from the next frame) */
            click_held = true;
            click_at = m;
        }

        bool paint = !over_panel && !over_switch;
        backdrop_update(paint && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                        paint && IsMouseButtonDown(MOUSE_BUTTON_RIGHT), (int)m.x, (int)m.y);

        arcade_frame_begin();
        draw_menu_view(app, first, t);
        arcade_frame_end();
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
        return SCR_MENU; /* the player abandoned the run with ESC: nothing is recorded */

    app->last_game = g;
    app->last_result = r;
    app->last_rank = 0;
    app->last_best = 0;
    app->last_improved = false;
    arc_strlcpy(app->last_board, board, sizeof(app->last_board));
    /* the score goes into the table of this mode/level ("beat_dodge:Dodge / Bopeebo / normal"), or into the
     * game's one table ("echo_maze") when the game did not name a board */
    char key[LB_GAME_LEN];
    lb_board_key(key, sizeof(key), g->id, board);
    int before = lb_best_of(&app->lb, key, app->player); /* -1: the first run of this player in the table */
    lb_add(&app->lb, key, app->player, r.score, (long long)time(NULL));
    if (!lb_save(&app->lb, app->scores_path))
        fprintf(stderr, "warning: could not write %s\n", app->scores_path);
    online_submit(app, key, r.score); /* queued for the server, when the player turned online scores on */
    /* the place of this player's BEST run among the players of THIS table only; 0 when the run did not make
     * the table (it was full and this was the lowest score) */
    app->last_rank = lb_rank_of(&app->lb, key, app->player);
    app->last_best = lb_best_of(&app->lb, key, app->player);
    app->last_improved = app->last_rank > 0 && r.score > before;
    return SCR_RESULT;
}

static void draw_result_view(const AppState *app, float t) {
    gfx_backdrop(t);
    backdrop_draw(0.5f);
    draw_title(NULL);
    draw_panel((Rectangle){360, 190, 560, 360}, 0.94f);
    gfx_text_center(app->last_game->title, ARC_SCREEN_W / 2, 214, 30, ARC_COL_CYAN);
    gfx_text_center("Game over", ARC_SCREEN_W / 2, 262, 22, ARC_COL_DIM);
    gfx_text_center(TextFormat("%d", app->last_result.score), ARC_SCREEN_W / 2, 306, 80, ARC_COL_YELLOW);
    bool boarded = app->last_board[0] != '\0'; /* the rank is within one mode/level, not the whole game */
    if (app->last_rank > 0) {
        const char *where = boarded ? "on this board" : "for this game";
        const char *line;
        if (app->last_improved && app->last_rank == 1)
            line = "New high score!";
        else if (app->last_improved)
            line = TextFormat("Personal best! rank #%d %s", app->last_rank, where);
        else /* a worse run than before: the rank is that of the player's best */
            line = TextFormat("rank #%d %s - your best: %d", app->last_rank, where, app->last_best);
        gfx_text_center(line, ARC_SCREEN_W / 2, 410, 24, ARC_COL_GREEN);
    }
    if (boarded) {
        char label[GAME_BOARD_LEN];
        fit_text(app->last_board, 18, 500, label, sizeof(label));
        gfx_text_center(label, ARC_SCREEN_W / 2, 442, 18, ARC_COL_DIM);
    }
    if (online_enabled(app)) { /* what became of this score online */
        char key[LB_GAME_LEN];
        lb_board_key(key, sizeof(key), app->last_game->id, app->last_board);
        const LbnAck *ack = &app->lbn.ack;
        if (ack->valid && strcmp(ack->table, key) == 0 && arc_stricmp(ack->player, app->player) == 0 &&
            ack->score == app->last_result.score)
            gfx_text_center(TextFormat("worldwide: #%d of %d players", ack->rank, ack->players),
                            ARC_SCREEN_W / 2, 470, 18, ARC_COL_CYAN);
        else if (app->lbn.outbox.count > 0)
            gfx_text_center(lbn_status(&app->lbn) == LBN_OFFLINE ? "offline - this score will be sent later"
                                                                 : "sending to the online leaderboard...",
                            ARC_SCREEN_W / 2, 470, 18, ARC_COL_DIM);
    }
    const Hint hints[] = {
        {"R", "play again", KEY_R, 0}, {"Enter", "menu", KEY_ENTER, 0}, {"L", "leaderboard", KEY_L, 0}};
    draw_hint_row_centered(506, hints, (int)ARC_ARRAY_LEN(hints));
}

Screen screen_result(AppState *app) {
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        online_tick(app);
        t += ARC_DT;
        if (arcade_key_pressed(KEY_ENTER) || arcade_key_pressed(KEY_ESCAPE))
            return SCR_MENU;
        if (arcade_key_pressed(KEY_R)) {
            app->launch = app->last_game;
            return SCR_LAUNCH;
        }
        if (arcade_key_pressed(KEY_L))
            return SCR_LEADERBOARD;
        backdrop_update(false, false, 0, 0);
        arcade_frame_begin();
        draw_result_view(app, t);
        arcade_frame_end();
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

/* Every list shows the BEST row of each player (lb_top_best). The rows are built once when the tab, the board
 * or the search changes (not every frame) and kept in g_lbv; Up/Down (on a list without boards), PgUp/PgDn
 * and the mouse wheel scroll through them. */
#define VIEW_MAX 100 /* rows a list keeps */
#define VIEW_ROWS 10 /* rows on screen at once */
#define VIEW_Y 222   /* y of the first row */
#define VIEW_ROW_H 34

/* ---------------------------------------------------------------- first launch */
/* Online scores are ON by default, so the first time the game runs the player is told what is shared and
 * where it goes, and can turn it off before anything is sent. */
static void draw_first_run(float t) {
    gfx_backdrop(t);
    backdrop_draw(0.4f);
    draw_title("online scores");
    draw_panel((Rectangle){180, 120, 920, 520}, 0.93f);
    const int cx = ARC_SCREEN_W / 2;
    char server[96];
    fit_text(online_server(), 18, 780, server, sizeof(server));
    gfx_text_center("Online leaderboard is on", cx, 150, 32, ARC_COL_YELLOW);
    gfx_text_center("arCade shares your scores with other players, so you can see where you rank.", cx, 226,
                    18, ARC_COL_TEXT);
    gfx_text_center("What is sent: your player name, the game and your score, and a random", cx, 280, 18,
                    ARC_COL_DIM);
    gfx_text_center("install ID that the server gives you. Nothing else. Names and scores are", cx, 306, 18,
                    ARC_COL_DIM);
    gfx_text_center("visible to everyone, and only runs from now on are sent.", cx, 332, 18, ARC_COL_DIM);
    gfx_text_center("Server:", cx, 380, 18, ARC_COL_DIM);
    gfx_text_center(server, cx, 406, 18, ARC_COL_CYAN);
    gfx_text_center("Your scores on this computer always stay here. You can change this at any time:", cx,
                    454, 18, ARC_COL_DIM);
    gfx_text_center("press L for the leaderboard (it opens on Online), then O.", cx, 480, 18, ARC_COL_DIM);
    const Hint hints[] = {{"Enter", "keep it on", KEY_ENTER, 0}, {"N", "turn it off", KEY_N, 0}};
    draw_hint_row_centered(600, hints, (int)ARC_ARRAY_LEN(hints));
}

/* Called once from main(), before the first screen. Enter keeps it on; N or Esc turns it off; closing the
 * window answers nothing, so the next start asks again. Nothing is sent before the answer (online_enabled()).
 */
void screen_online_first_run(AppState *app) {
    float t = 0.0f;
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        if (arcade_key_pressed(KEY_ENTER)) {
            online_answer_first_run(app, true);
            return;
        }
        if (arcade_key_pressed(KEY_N) || arcade_key_pressed(KEY_ESCAPE)) {
            online_answer_first_run(app, false);
            return;
        }
        backdrop_update(false, false, 0, 0);
        arcade_frame_begin();
        draw_first_run(t);
        arcade_frame_end();
    }
}

typedef enum LbMode {
    LV_LOCAL,    /* the scores on this computer */
    LV_ONLINE,   /* one table from the server (what lbnet cached) */
    LV_CONSENT,  /* the Online tab, but the player has not turned online scores on */
    LV_NO_ONLINE /* the Online tab, but this build has no libcurl */
} LbMode;

typedef struct LbView {
    LbMode mode;
    ScoreEntry rows[VIEW_MAX];
    int count;
    int scroll; /* index of the first row on screen */
    const char *heading;
    char sub[GAME_BOARD_LEN + 24]; /* small line under the heading: which board is shown */
    int boards;                    /* tables of this game (0 = no selector) */
    bool one_table;                /* every row is from the same table: no need for a game/board column */
    int you_rank;          /* the current player's place in this one table, 0 = not in it (or no table) */
    ScoreEntry you;        /* ... and the row that goes with it (player, score, table) */
    char key[LB_GAME_LEN]; /* online: the table shown, which is also the one to ask the server for */
    long long fetched;     /* online: when that copy was received (0 = none); a newer one rebuilds the view */
    int online_players;    /* online: players on the whole table at the server */
} LbView;
static LbView g_lbv; /* about 10 KB: static, like g_boards */

static void build_view(const AppState *app, LbView *v, int tab, int board, bool searching,
                       const char *query) {
    v->mode = LV_LOCAL;
    v->key[0] = '\0';
    v->fetched = 0;
    v->online_players = 0;
    v->count = 0;
    v->scroll = 0;
    v->sub[0] = '\0';
    v->boards = 0;
    v->one_table = false;
    v->you_rank = 0;
    const char *you_key = NULL; /* the table that "you" is looked up in: only when the list IS one table */
    if (searching || query[0] != '\0') {
        v->count = lb_search_best(&app->lb, query, v->rows, VIEW_MAX);
        v->heading = "search results";
    } else if (tab == 0) {
        v->count = lb_top_best(&app->lb, LB_SCOPE_ALL, NULL, v->rows, VIEW_MAX);
        v->heading = "all games";
    } else {
        const GameDef *g = app->games[tab - 1];
        v->heading = g->title;
        v->boards = tab_boards(app, tab);
        if (v->boards > 0 && board >= 1 && board <= v->boards) {
            const char *name = lb_key_board(g_boards[board - 1]);
            v->count = lb_top_best(&app->lb, LB_SCOPE_TABLE, g_boards[board - 1], v->rows, VIEW_MAX);
            v->one_table = true;
            you_key = g_boards[board - 1];
            snprintf(v->sub, sizeof(v->sub), "board %d/%d: %s", board, v->boards,
                     name[0] != '\0' ? name : "older scores");
        } else {
            v->count = lb_top_best(&app->lb, LB_SCOPE_GAME, g->id, v->rows, VIEW_MAX);
            if (v->boards > 0)
                snprintf(v->sub, sizeof(v->sub), "all boards (%d)", v->boards);
            else { /* a game without boards has one table: no column to say which */
                v->one_table = true;
                you_key = g->id;
            }
        }
    }
    if (you_key != NULL) {
        v->you_rank = lb_rank_of(&app->lb, you_key, app->player);
        memset(&v->you, 0, sizeof(v->you));
        arc_strlcpy(v->you.game, you_key, sizeof(v->you.game));
        arc_strlcpy(v->you.player, app->player, sizeof(v->you.player));
        v->you.score = lb_best_of(&app->lb, you_key, app->player);
    }
}

/* ---- the Online tab ---- */
/* The boards of a game that the Online tab can show: the ones with scores on this computer (that is how the
 * hub knows their names), without the old plain table of a game that has boards: the server has no such
 * table. They are left in g_boards. */
static int online_boards(const AppState *app, int tab) {
    int n = tab_boards(app, tab);
    int kept = 0;
    for (int i = 0; i < n; i++)
        if (lb_key_board(g_boards[i])[0] != '\0') {
            if (kept != i)
                arc_strlcpy(g_boards[kept], g_boards[i], LB_GAME_LEN);
            kept++;
        }
    return kept;
}

/* A table from the server, as the same kind of rows the local lists use. tab is 1.., board is 1.. */
static void build_online_view(const AppState *app, LbView *v, int tab, int board) {
    v->mode = LV_ONLINE;
    v->count = 0;
    v->scroll = 0;
    v->sub[0] = '\0';
    v->one_table = true;
    v->you_rank = 0;
    v->fetched = 0;
    v->online_players = 0;
    v->key[0] = '\0';
    v->boards = 0;
    if (tab < 1 || tab > app->game_count) {
        v->heading = "online";
        return;
    }
    const GameDef *g = app->games[tab - 1];
    v->heading = g->title;
    v->boards = online_boards(app, tab);
    if (v->boards > 0) {
        if (board < 1 || board > v->boards)
            board = 1;
        arc_strlcpy(v->key, g_boards[board - 1], sizeof(v->key));
        snprintf(v->sub, sizeof(v->sub), "board %d/%d: %s", board, v->boards, lb_key_board(v->key));
    } else {
        arc_strlcpy(v->key, g->id, sizeof(v->key));
    }
    const OnlineTable *t = lbn_table(&app->lbn, v->key);
    if (t == NULL)
        return;
    v->fetched = t->fetched;
    v->online_players = t->players;
    for (int i = 0; i < t->count && i < VIEW_MAX; i++) {
        ScoreEntry *e = &v->rows[v->count++];
        memset(e, 0, sizeof(*e));
        arc_strlcpy(e->game, v->key, sizeof(e->game));
        arc_strlcpy(e->player, t->rows[i].player, sizeof(e->player));
        e->score = t->rows[i].score;
        e->stamp = t->rows[i].stamp;
        if (v->you_rank == 0 &&
            arc_stricmp(e->player, app->player) == 0) { /* the first row is the best one */
            v->you_rank = i + 1;
            v->you = *e;
        }
    }
}

/* The Online tab without a table: why there is none, and what to do about it. */
static void build_notice_view(LbView *v, LbMode mode) {
    v->mode = mode;
    v->heading = "online scores";
    v->sub[0] = '\0';
    v->key[0] = '\0';
    v->count = 0;
    v->scroll = 0;
    v->boards = 0;
    v->one_table = true;
    v->you_rank = 0;
    v->fetched = 0;
    v->online_players = 0;
}

/* Rebuilds the view after a change: the local lists, a table from the server, or the notice. */
static void rebuild_view(const AppState *app, bool online, int tab, int board, bool searching,
                         const char *query) {
    if (!online)
        build_view(app, &g_lbv, tab, board, searching, query);
    else if (!online_ready(app))
        build_notice_view(&g_lbv, LV_NO_ONLINE);
    else if (!online_enabled(app))
        build_notice_view(&g_lbv, LV_CONSENT);
    else
        build_online_view(app, &g_lbv, tab, board);
}

static void scroll_view(LbView *v, int delta) {
    int max_scroll = v->count > VIEW_ROWS ? v->count - VIEW_ROWS : 0;
    v->scroll = (int)arc_clampf((float)(v->scroll + delta), 0.0f, (float)max_scroll);
}

/* One row of the list: place, player, (board), score. */
static void draw_lb_row(const LbView *v, const ScoreEntry *e, int place, int y, Color c) {
    gfx_text(TextFormat("%d", place), 220, y, 22, c);
    gfx_text(e->player, 270, y, 22, c);
    if (!v->one_table) {
        /* a plain game id, or "id:board" fitted so that it never runs into the score; on a game's own tab the
         * board name alone is enough */
        const char *shown = e->game;
        if (v->boards > 0) /* a game with boards: the board name, or "older scores" for the plain id */
            shown = lb_key_board(shown)[0] != '\0' ? lb_key_board(shown) : "older scores";
        draw_in_column(shown, 520, y, 22, 440, ARC_COL_DIM);
    }
    gfx_text_right(TextFormat("%d", e->score), 1060, y, 22, c);
}

/* "just now", "40 s ago", "5 min ago", "3 h ago", "2 d ago" */
static void format_age(char *out, size_t size, long long secs) {
    if (secs < 10)
        snprintf(out, size, "just now");
    else if (secs < 60)
        snprintf(out, size, "%lld s ago", secs);
    else if (secs < 3600)
        snprintf(out, size, "%lld min ago", secs / 60);
    else if (secs < 86400)
        snprintf(out, size, "%lld h ago", secs / 3600);
    else
        snprintf(out, size, "%lld d ago", secs / 86400);
}

/* Top right of the Online tab: is the server reachable, how old is this copy, what is still waiting. */
static void draw_online_status(const AppState *app, const LbView *v) {
    const LbnClient *c = &app->lbn;
    LbnStatus st = lbn_status(c);
    const char *what = "online";
    Color col = ARC_COL_GREEN;
    if (st == LBN_SYNCING) {
        what = "syncing...";
        col = ARC_COL_CYAN;
    } else if (st == LBN_OFFLINE) {
        what = "offline";
        col = ARC_COL_YELLOW;
    }
    char line[112];
    int n = snprintf(line, sizeof(line), "%s", what);
    if (v->fetched > 0) {
        char age[24];
        format_age(age, sizeof(age), (long long)time(NULL) - v->fetched);
        n += snprintf(line + n, sizeof(line) - (size_t)n, ", updated %s", age);
    }
    if (c->outbox.count > 0)
        snprintf(line + n, sizeof(line) - (size_t)n, ", %d waiting", c->outbox.count);
    gfx_text_right(line, 1060, 138, 17, col);
}

/* What an empty online list means. */
static const char *online_empty_message(const AppState *app, const LbView *v) {
    LbnStatus st = lbn_status(&app->lbn);
    if (v->fetched > 0)
        return "nobody has a score here yet";
    if (st == LBN_SYNCING || app->lbn.wanted[0] != '\0')
        return "loading...";
    if (st == LBN_OFFLINE)
        return "offline - no copy of this table yet";
    return "no online table here yet - play a run first";
}

static void draw_notice(const AppState *app, const LbView *v) {
    (void)app;
    const int cx = ARC_SCREEN_W / 2;
    if (v->mode == LV_NO_ONLINE) {
        gfx_text_center("Online scores are not available in this build.", cx, 270, 24, ARC_COL_TEXT);
        gfx_text_center("It was built without libcurl, the library that talks to the server.", cx, 320, 18,
                        ARC_COL_DIM);
        return;
    }
    char server[96];
    fit_text(online_server(), 18, 780, server, sizeof(server));
    gfx_text_center("Share your scores with other players?", cx, 210, 28, ARC_COL_TEXT);
    gfx_text_center("What is sent: your player name, the game and your score, and a random", cx, 268, 18,
                    ARC_COL_DIM);
    gfx_text_center("install ID that the server gives you. Nothing else, and only for runs you", cx, 294, 18,
                    ARC_COL_DIM);
    gfx_text_center("finish after turning this on. Names and scores are visible to everyone.", cx, 320, 18,
                    ARC_COL_DIM);
    gfx_text_center("Server:", cx, 366, 18, ARC_COL_DIM);
    gfx_text_center(server, cx, 392, 18, ARC_COL_CYAN);
    gfx_text_center("Your scores on this computer always stay here. You can turn this off again at any time.",
                    cx, 436, 18, ARC_COL_DIM);
    gfx_text_center("Press Enter to turn online scores on", cx, 490, 24, ARC_COL_GREEN);
}

static void draw_leaderboard_view(const AppState *app, const LbView *v, bool searching, const char *query,
                                  float t) {
    gfx_backdrop(t);
    backdrop_draw(0.4f);
    draw_title("leaderboard");
    draw_panel((Rectangle){180, 120, 920, 520}, 0.93f);
    bool on_online_tab = v->mode != LV_LOCAL;
    gfx_text("LOCAL", 200, 138, 17, on_online_tab ? ARC_COL_DIM : ARC_COL_CYAN);
    gfx_text("ONLINE", 262, 138, 17, on_online_tab ? ARC_COL_CYAN : ARC_COL_DIM);
    gfx_text_center(v->heading, ARC_SCREEN_W / 2, 130, 28, ARC_COL_YELLOW);
    if (v->mode == LV_CONSENT || v->mode == LV_NO_ONLINE) {
        draw_notice(app, v);
        const Hint consent[] = {{"Enter", "turn on", KEY_ENTER, 0},
                                {"Tab", "local scores", KEY_TAB, 0},
                                {esc_label(), "back", KEY_ESCAPE, 0}};
        const Hint none[] = {{"Tab", "local scores", KEY_TAB, 0}};
        if (v->mode == LV_CONSENT)
            draw_hint_row_centered(600, consent, (int)ARC_ARRAY_LEN(consent));
        else
            draw_hint_row_centered(600, none, (int)ARC_ARRAY_LEN(none));
        return;
    }
    if (v->sub[0] != '\0')
        gfx_text_center(v->sub, ARC_SCREEN_W / 2, 166, 17, ARC_COL_CYAN);
    if (v->mode == LV_ONLINE)
        draw_online_status(app, v);
    if (v->count > VIEW_ROWS) { /* which rows are on screen */
        int last = v->scroll + VIEW_ROWS < v->count ? v->scroll + VIEW_ROWS : v->count;
        gfx_text_right(TextFormat("%d-%d of %d", v->scroll + 1, last, v->count), 1060, 166, 17, ARC_COL_DIM);
    }
    gfx_text("#", 220, 190, 18, ARC_COL_DIM);
    gfx_text("player", 270, 190, 18, ARC_COL_DIM);
    if (!v->one_table)
        gfx_text(v->boards > 0 ? "board" : "game", 520, 190, 18, ARC_COL_DIM);
    gfx_text_right("score", 1060, 190, 18, ARC_COL_DIM);
    for (int i = 0; i < VIEW_ROWS && v->scroll + i < v->count; i++) {
        int place = v->scroll + i + 1;
        int y = VIEW_Y + i * VIEW_ROW_H;
        bool mine = arc_stricmp(v->rows[place - 1].player, app->player) == 0;
        if (mine) /* the current player's rows stand out */
            DrawRectangleRounded((Rectangle){196.0f, (float)y - 4.0f, 888.0f, 32.0f}, 0.3f, 6,
                                 Fade(ARC_COL_GREEN, 0.12f));
        draw_lb_row(v, &v->rows[place - 1], place, y, place == 1 ? ARC_COL_YELLOW : ARC_COL_TEXT);
    }
    if (v->count == 0) {
        const char *message = "no scores yet - go play something!";
        if (searching || query[0] != '\0')
            message = "no player matches";
        else if (v->mode == LV_ONLINE)
            message = online_empty_message(app, v);
        gfx_text_center(message, ARC_SCREEN_W / 2, 330, 22, ARC_COL_DIM);
    }
    /* "you" stays visible when the player's own row is scrolled out of the window */
    if (v->you_rank > 0 && (v->you_rank <= v->scroll || v->you_rank > v->scroll + VIEW_ROWS)) {
        DrawRectangle(200, 557, 880, 1, Fade(ARC_COL_DIM, 0.40f));
        DrawRectangleRounded((Rectangle){196.0f, 561.0f, 888.0f, 32.0f}, 0.3f, 6, Fade(ARC_COL_GREEN, 0.12f));
        draw_lb_row(v, &v->you, v->you_rank, 565, ARC_COL_GREEN);
    }
    if (searching || query[0] != '\0') {
        /* the on-screen keyboard covers the bottom half: the line moves up under the heading */
        gfx_text_center(TextFormat("find player: %s%s", query, searching ? "_" : ""), ARC_SCREEN_W / 2,
                        arcade_osk_is_open() ? 160 : 600, 20, ARC_COL_GREEN);
        return;
    }
    Hint hints[7];
    int hn = 0;
    hints[hn++] = (Hint){"Left/Right", v->mode == LV_ONLINE ? "game" : "switch game", KEY_LEFT, KEY_RIGHT};
    if (v->boards > 0)
        hints[hn++] = (Hint){"Up/Down", v->mode == LV_ONLINE ? "board" : "switch board", KEY_UP, KEY_DOWN};
    if (v->count > VIEW_ROWS)
        hints[hn++] = (Hint){"PgUp/PgDn", "scroll", KEY_PAGE_UP, KEY_PAGE_DOWN};
    if (v->mode == LV_ONLINE) {
        hints[hn++] = (Hint){"R", "refresh", KEY_R, 0};
        hints[hn++] = (Hint){"O", "turn off", KEY_O, 0};
        hints[hn++] = (Hint){"Tab", "local", KEY_TAB, 0};
    } else {
        hints[hn++] = (Hint){"F", "find player", KEY_F, 0};
        hints[hn++] = (Hint){"Tab", "online", KEY_TAB, 0};
    }
    hints[hn++] = (Hint){esc_label(), "back", KEY_ESCAPE, 0};
    draw_hint_row_centered(600, hints, hn);
}

Screen screen_leaderboard(AppState *app) {
    int tab = 0; /* 0 = all games, 1.. = one game from the registry (the Online tab has no "all games") */
    /* The Online tab opens first when online scores are on (see online_leaderboard_start), else the local one
     */
    bool online = online_leaderboard_start(app, &tab);
    int board = online ? 1 : 0; /* on a game with boards: 0 = all boards, 1.. = one board (see tab_boards) */
    bool ask_first = online;    /* the table on screen is asked for in the first frame */
    bool searching = false;
    char query[LB_NAME_LEN] = "";
    int qlen = 0;
    float t = 0.0f;
    rebuild_view(app, online, tab, board, searching, query);
    arcade_flush_input();
    while (!arcade_quit_requested()) {
        t += ARC_DT;
        online_tick(app);
        bool rebuild = false; /* the tab, the board, the search text or the mode changed */
        bool ask = ask_first; /* ask the server for the table now shown */
        bool refresh = false; /* ... even if the copy is fresh */
        ask_first = false;
        int scroll = 0; /* rows to scroll this frame */
        if (searching) {
            for (int c = arcade_text_char(); c != 0; c = arcade_text_char())
                if (c >= 32 && c < 127 && c != '|' && qlen < LB_NAME_LEN - 1) {
                    query[qlen++] = (char)c;
                    query[qlen] = '\0';
                    rebuild = true;
                }
            if (arcade_key_pressed(KEY_BACKSPACE) && qlen > 0) {
                query[--qlen] = '\0';
                rebuild = true;
            }
            if (arcade_key_pressed(KEY_ENTER) || arcade_key_pressed(KEY_ESCAPE)) {
                searching = false;
                arcade_osk_open(false);
            }
        } else if (g_lbv.mode == LV_CONSENT || g_lbv.mode == LV_NO_ONLINE) {
            if (arcade_key_pressed(KEY_TAB) || arcade_key_pressed(KEY_ESCAPE) ||
                (g_lbv.mode == LV_NO_ONLINE && arcade_key_pressed(KEY_ENTER))) {
                online = false; /* back to the local scores; the next Esc leaves the screen */
                board = 0;
                rebuild = true;
            } else if (g_lbv.mode == LV_CONSENT && arcade_key_pressed(KEY_ENTER)) {
                online_set_enabled(app, true); /* the player's choice, remembered in online.cfg */
                if (tab == 0 && app->game_count > 0)
                    tab = 1;
                board = 1;
                rebuild = true;
                ask = true;
            }
        } else if (arcade_key_pressed(KEY_TAB)) {
            online = !online;
            if (online && tab == 0 && app->game_count > 0)
                tab = 1; /* tables are per game: there is no "all games" online */
            board = online ? 1 : 0;
            query[0] = '\0';
            qlen = 0;
            rebuild = true;
            ask = online;
        } else if (g_lbv.mode == LV_ONLINE) {
            if (arcade_key_pressed(KEY_ESCAPE) || arcade_key_pressed(KEY_ENTER))
                return SCR_MENU;
            if (app->game_count > 0 && (arcade_key_pressed(KEY_RIGHT) || arcade_key_pressed(KEY_LEFT))) {
                int n = app->game_count; /* tabs 1..n */
                tab = arcade_key_pressed(KEY_RIGHT) ? tab % n + 1 : (tab + n - 2) % n + 1;
                board = 1;
                rebuild = true;
                ask = true;
            }
            if (g_lbv.boards > 0 && arcade_key_pressed(KEY_DOWN)) {
                board = board % g_lbv.boards + 1;
                rebuild = true;
                ask = true;
            }
            if (g_lbv.boards > 0 && arcade_key_pressed(KEY_UP)) {
                board = (board + g_lbv.boards - 2) % g_lbv.boards + 1;
                rebuild = true;
                ask = true;
            }
            if (g_lbv.boards == 0) { /* no board selector: Up/Down scroll the list */
                scroll += arcade_key_pressed(KEY_DOWN) ? 1 : 0;
                scroll -= arcade_key_pressed(KEY_UP) ? 1 : 0;
            }
            scroll += arcade_key_pressed(KEY_PAGE_DOWN) ? VIEW_ROWS : 0;
            scroll -= arcade_key_pressed(KEY_PAGE_UP) ? VIEW_ROWS : 0;
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
                scroll += wheel > 0.0f ? -3 : 3;
            refresh = arcade_key_pressed(KEY_R);
            if (arcade_key_pressed(KEY_O)) {
                online_set_enabled(app, false); /* off: the next page offers to turn it on again */
                rebuild = true;
            }
        } else { /* LV_LOCAL */
            if (arcade_key_pressed(KEY_ESCAPE) || arcade_key_pressed(KEY_ENTER))
                return SCR_MENU;
            if (arcade_key_pressed(KEY_RIGHT) || arcade_key_pressed(KEY_LEFT)) {
                int step =
                    arcade_key_pressed(KEY_RIGHT) ? 1 : app->game_count; /* +1, or -1 modulo the tab count */
                tab = (tab + step) % (app->game_count + 1);
                board = 0;       /* every game starts on "all boards" */
                query[0] = '\0'; /* moving to another game ends a search */
                qlen = 0;
                rebuild = true;
            }
            if (g_lbv.boards > 0 && arcade_key_pressed(KEY_DOWN)) {
                board = (board + 1) % (g_lbv.boards + 1);
                rebuild = true;
            }
            if (g_lbv.boards > 0 && arcade_key_pressed(KEY_UP)) {
                board = (board + g_lbv.boards) % (g_lbv.boards + 1);
                rebuild = true;
            }
            if (g_lbv.boards == 0) { /* no board selector: Up/Down scroll the list */
                scroll += arcade_key_pressed(KEY_DOWN) ? 1 : 0;
                scroll -= arcade_key_pressed(KEY_UP) ? 1 : 0;
            }
            scroll += arcade_key_pressed(KEY_PAGE_DOWN) ? VIEW_ROWS : 0;
            scroll -= arcade_key_pressed(KEY_PAGE_UP) ? VIEW_ROWS : 0;
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
                scroll += wheel > 0.0f ? -3 : 3;
            if (arcade_key_pressed(KEY_F) || arcade_key_pressed(KEY_SLASH)) {
                searching = true;
                arcade_osk_open(true); /* a touch screen has no keyboard */
                qlen = 0;
                query[0] = '\0';
                rebuild = true;
                (void)arcade_text_char(); /* swallow the key that opened the search box */
            }
        }
        if (rebuild) {
            rebuild_view(app, online, tab, board, searching, query);
        } else if (g_lbv.mode ==
                   LV_ONLINE) { /* did a newer copy of the table arrive? keep the scroll position */
            const OnlineTable *copy = lbn_table(&app->lbn, g_lbv.key);
            if ((copy != NULL ? copy->fetched : 0) != g_lbv.fetched) {
                int keep = g_lbv.scroll;
                rebuild_view(app, online, tab, board, searching, query);
                scroll_view(&g_lbv, keep);
            }
        }
        if (g_lbv.mode == LV_ONLINE && g_lbv.key[0] != '\0') {
            if (refresh)
                online_refresh(app, g_lbv.key);
            else if (ask)
                online_want(app, g_lbv.key);
        }
        if (scroll != 0)
            scroll_view(&g_lbv, scroll);
        backdrop_update(false, false, 0, 0);

        arcade_frame_begin();
        draw_leaderboard_view(app, &g_lbv, searching, query, t);
        arcade_frame_end();
    }
    return SCR_QUIT;
}

/* ---------------------------------------------------------------- previews (smoke runner / screenshots) */
const char *screens_preview_name(int which) {
    static const char *const names[PREVIEW_COUNT] = {"hub_name",
                                                     "hub_menu",
                                                     "hub_menu_sandbox",
                                                     "hub_archive",
                                                     "hub_archive_end",
                                                     "hub_result",
                                                     "hub_leaderboard",
                                                     "hub_leaderboard_board",
                                                     "hub_leaderboard_you",
                                                     "hub_leaderboard_open",
                                                     "hub_online_off",
                                                     "hub_online_table",
                                                     "hub_online_offline",
                                                     "hub_online_first_run",
                                                     "hub_fly"};
    return (which >= 0 && which < PREVIEW_COUNT) ? names[which] : "hub_unknown";
}

void screens_draw_preview(AppState *app, int which, float t) {
    switch (which) {
    case PREVIEW_NAME: {
        /* the screen as it is at startup: no name yet, so one is required */
        char kept[LB_NAME_LEN];
        arc_strlcpy(kept, app->player, sizeof(kept));
        app->player[0] = '\0';
        arcade_osk_open(true); /* only does something when the touch UI is on */
        draw_name_view(app, "Ana", 0.0f, t);
        arc_strlcpy(app->player, kept, sizeof(app->player));
        break;
    }
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
        build_view(app, &g_lbv, 0, 0, false, "");
        draw_leaderboard_view(app, &g_lbv, false, "", t);
        break;
    case PREVIEW_LEADERBOARD_BOARD:
    case PREVIEW_LEADERBOARD_YOU: {            /* the first game that has boards */
        int tab = app->game_count > 0 ? 1 : 0; /* tab 0 = all games, also what an empty registry shows */
        while (tab > 0 && tab < app->game_count && tab_boards(app, tab) == 0)
            tab++;
        int boards = tab_boards(app, tab);
        /* BOARD: its second board. YOU: its last board, which has more players than fit on screen, with the
         * current player below the visible rows */
        int board = which == PREVIEW_LEADERBOARD_YOU ? boards : (boards > 1 ? 2 : 0);
        build_view(app, &g_lbv, tab, board, false, "");
        draw_leaderboard_view(app, &g_lbv, false, "", t);
        break;
    }
    case PREVIEW_LEADERBOARD_OPEN: {
        /* The screen as screen_leaderboard() starts it, with the game whose table smoke.c cached selected. */
        const int was_selected = app->selected;
        for (int i = 0; i < app->game_count; i++)
            if (strcmp(app->games[i]->id, "different_color") == 0)
                app->selected = i;
        int tab = 0;
        bool online = online_leaderboard_start(app, &tab);
        rebuild_view(app, online, tab, online ? 1 : 0, false, "");
        draw_leaderboard_view(app, &g_lbv, false, "", t);
        app->selected = was_selected;
        break;
    }
    case PREVIEW_ONLINE_FIRST_RUN:
        draw_first_run(t);
        break;
    case PREVIEW_ONLINE_OFF:
    case PREVIEW_ONLINE_TABLE:
    case PREVIEW_ONLINE_OFFLINE: {
        /* The Online tab in its three states, from a client that has no network (see smoke.c). The state that
         * a state needs is set for this one frame and put back. */
        LbnClient *c = &app->lbn;
        const bool was_enabled = c->enabled;
        const int was_failures = c->failures;
        static Outbox saved;
        saved = c->outbox;
        int tab = 1; /* the game whose table smoke.c cached: different_color, or loop_lap when offline */
        const char *wanted = which == PREVIEW_ONLINE_OFFLINE ? "loop_lap" : "different_color";
        for (int i = 0; i < app->game_count; i++)
            if (strcmp(app->games[i]->id, wanted) == 0)
                tab = i + 1;
        if (which == PREVIEW_ONLINE_OFF)
            c->enabled = false;
        if (which == PREVIEW_ONLINE_OFFLINE) {
            c->failures = 3;
            lbn_outbox_add(&c->outbox, "different_color", app->player, 250, 0);
            lbn_outbox_add(&c->outbox, "loop_lap", app->player, 40, 0);
        }
        rebuild_view(app, true, tab, 1, false, "");
        draw_leaderboard_view(app, &g_lbv, false, "", t);
        c->enabled = was_enabled;
        c->failures = was_failures;
        c->outbox = saved;
        break;
    }
    case PREVIEW_FLY:
        backdrop_draw(1.0f); /* the menu is hidden: only the game */
        break;
    default:
        break;
    }
}
