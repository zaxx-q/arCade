/* Touch front-end: what the fingers on the glass do to the hub and to the games.
 *
 * The rules of the virtual controls and of the on-screen keyboard are plain C in core/src/touch.c and
 * core/src/osk.c (unit-tested). This file only connects them to raylib:
 *   - once per frame (after EndDrawing, when raylib has polled the events) it reads the fingers, converts
 * them to design units, and lets the keyboard, the tap zones and the virtual controls each take their
 * fingers;
 *   - arcade_poll() folds the result into the ArcadeInput of the game;
 *   - the hub asks for "virtual key presses" (arcade_key_pressed) and typed characters (arcade_text_char), so
 *     the same screens work with a keyboard and with a finger.
 *
 * A finger belongs to ONE of these until it is lifted, so a tap on a key never also fires a bullet. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_touch.h"
#include "arcade/osk.h"
#include "arcade/touch.h"

#define MAX_ZONES 24
#define MAX_VKEYS 16
#define TEXT_QUEUE 16
#define BACK_ARM_FRAMES (2 * ARC_FPS) /* a second tap on "back" within 2 s leaves the game */
#define OSK_MAX_W_GAME 840.0f /* inside a game: narrow, so it clears the stick and the action button */
#define OSK_MAX_W_HUB 1000.0f

/* A set of finger ids. Small and linear: there are never more than TOUCH_MAX_POINTS fingers. */
typedef struct IdSet {
    int id[TOUCH_MAX_POINTS];
    int n;
} IdSet;

static bool set_has(const IdSet *s, int id) {
    for (int i = 0; i < s->n; i++)
        if (s->id[i] == id)
            return true;
    return false;
}

static void set_add(IdSet *s, int id) {
    if (!set_has(s, id) && s->n < TOUCH_MAX_POINTS)
        s->id[s->n++] = id;
}

/* Keeps only the ids that are still on the glass. */
static void set_keep(IdSet *s, const TouchPoint *pts, int n) {
    int kept = 0;
    for (int i = 0; i < s->n; i++)
        for (int j = 0; j < n; j++)
            if (pts[j].id == s->id[i]) {
                s->id[kept++] = s->id[i];
                break;
            }
    s->n = kept;
}

typedef struct Zone {
    Rectangle r; /* design units */
    int key;     /* the raylib key a tap in the rectangle stands for */
} Zone;

static struct {
    bool on;   /* the touch UI exists in this run */
    bool game; /* inside a game loop: the virtual controls are live */
    ArcView view;
    TouchState ts; /* the virtual controls */
    TouchOut out;  /* what the fingers meant in the frame that just ended */
    Osk osk;
    bool osk_open;
    int osk_down; /* the key a finger is holding now (to draw it pressed), or -1 */
    IdSet known;  /* fingers seen in the last frame */
    IdSet skip;   /* fingers that belong to the keyboard, a tap zone, or a screen change: not the controls' */
    Zone zones[MAX_ZONES];
    int zone_n;
    int vkeys[MAX_VKEYS]; /* "key pressed" by a tap, valid for the logic that runs next */
    int vkey_n;
    int text[TEXT_QUEUE]; /* characters typed on the on-screen keyboard, not yet read */
    int text_n;
    int back_arm; /* frames left in which a second "back" leaves the game */
    bool test;    /* a smoke test injects the fingers (headless builds only) */
    TouchPoint injected[TOUCH_MAX_POINTS];
    int injected_n;
} t;

static bool env_wants_touch_ui(void) {
#ifdef ARCADE_ANDROID
    return true;
#else
    const char *env = getenv("ARCADE_TOUCH_UI");
    return env != NULL && env[0] == '1'; /* a headless smoke run draws the controls too, for screenshots */
#endif
}

/* ---- setup ------------------------------------------------------------------------------------------- */

static void layout_refresh(void) {
    const ArcView *v = &t.view;
    touch_layout_fit(&t.ts.layout, v->left, v->top, v->right, v->bottom);
    float room = v->right - v->left - 40.0f;
    float max_w = arc_minf(t.game ? OSK_MAX_W_GAME : OSK_MAX_W_HUB, room);
    osk_layout(&t.osk, (v->left + v->right) / 2.0f, v->bottom, max_w);
}

void gfx_touch_init(bool headless, const ArcView *v) {
    memset(&t, 0, sizeof(t));
    t.view = *v;
    touch_init(&t.ts);
    osk_init(&t.osk);
    t.osk_down = -1;
    (void)headless;
    t.on = env_wants_touch_ui();
    layout_refresh();
}

/* ---- the fingers -------------------------------------------------------------------------------------- */

/* The fingers on the glass right now, in design units. */
static int read_fingers(TouchPoint *out) {
    int n = 0;
#ifdef ARCADE_HEADLESS_BUILD
    if (t.test) {
        memcpy(out, t.injected, (size_t)t.injected_n * sizeof(out[0]));
        return t.injected_n;
    }
#endif
#ifdef ARCADE_ANDROID
    int count = GetTouchPointCount();
    for (int i = 0; i < count && n < TOUCH_MAX_POINTS; i++) {
        Vector2 p = GetTouchPosition(i); /* window pixels, not mapped like the mouse */
        Vec2 d = arc_view_to_design(&t.view, p.x, p.y);
        out[n].id = GetTouchPointId(i);
        out[n].x = d.x;
        out[n].y = d.y;
        n++;
    }
#else
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { /* the test mode: the mouse is one finger */
        Vector2 m = GetMousePosition();         /* already in design units: see view_sync() in gfx.c */
        out[n].id = 0;
        out[n].x = m.x;
        out[n].y = m.y;
        n++;
    }
#endif
    return n;
}

static void push_vkey(int key) {
    if (t.vkey_n < MAX_VKEYS)
        t.vkeys[t.vkey_n++] = key;
}

static void push_text(int ch) {
    if (t.text_n < TEXT_QUEUE)
        t.text[t.text_n++] = ch;
}

static bool in_rect(Rectangle r, float x, float y) {
    return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}

/* ---- the on-screen keyboard ---------------------------------------------------------------------------- */

static Rectangle osk_panel(void) {
    Rectangle r = {t.osk.x, t.osk.y, t.osk.w, t.osk.h};
    return r;
}

/* A finger that lands on the keyboard presses the key under it at once (like a phone keyboard) and the
 * keyboard keeps the finger: it never reaches the game, even in the gaps between the keys. */
static bool osk_take(const TouchPoint *p) {
    if (!t.osk_open || !in_rect(osk_panel(), p->x, p->y))
        return false;
    OskEvent ev;
    osk_press(&t.osk, osk_hit(&t.osk, p->x, p->y), &ev);
    if (ev.ch != 0)
        push_text(ev.ch);
    if (ev.backspace)
        push_vkey(KEY_BACKSPACE);
    if (ev.enter)
        push_vkey(KEY_ENTER);
    return true;
}

/* ---- tap zones: the hub's footer hints act as keys
 * --------------------------------------------------------- */

static bool zone_take(const TouchPoint *p) {
    for (int i = t.zone_n - 1; i >= 0; i--) /* the one drawn last is on top */
        if (in_rect(t.zones[i].r, p->x, p->y)) {
            push_vkey(t.zones[i].key);
            return true;
        }
    return false;
}

/* ---- one frame ------------------------------------------------------------------------------------------
 */

/* "Back" (the button, or the system's back key) inside a game: closes the keyboard first, then asks twice, so
 * an edge swipe of the gesture navigation does not throw away a run. */
static void back_in_game(void) {
    if (t.osk_open) {
        t.osk_open = false;
        return;
    }
    if (t.back_arm > 0) {
        push_vkey(KEY_ESCAPE);
        t.back_arm = 0;
    } else {
        t.back_arm = BACK_ARM_FRAMES;
    }
}

/* swallow: the fingers that are down are not new presses (a screen has just changed under them). */
static void process(bool swallow) {
    t.vkey_n = 0;
    memset(&t.out, 0, sizeof(t.out));
    t.osk_down = -1;
    if (t.back_arm > 0)
        t.back_arm--;

    TouchPoint pts[TOUCH_MAX_POINTS], free_pts[TOUCH_MAX_POINTS];
    int n = read_fingers(pts), free_n = 0;
    set_keep(&t.known, pts, n);
    set_keep(&t.skip, pts, n);
    layout_refresh();

    for (int i = 0; i < n; i++) {
        int id = pts[i].id;
        if (!set_has(&t.known, id)) { /* a finger that has just landed */
            set_add(&t.known, id);
            if (swallow || osk_take(&pts[i]) || zone_take(&pts[i]))
                set_add(&t.skip, id);
        }
        if (!set_has(&t.skip, id))
            free_pts[free_n++] = pts[i];
        else if (t.osk_open && in_rect(osk_panel(), pts[i].x, pts[i].y))
            t.osk_down = osk_hit(&t.osk, pts[i].x, pts[i].y); /* drawn as pressed while it stays */
    }

    if (t.game) {
        touch_update(&t.ts, free_pts, free_n, &t.out);
        if (t.out.keys_pressed)
            t.osk_open = !t.osk_open;
        if (t.out.back_pressed || IsKeyPressed(KEY_BACK))
            back_in_game();
    } else if (IsKeyPressed(KEY_BACK)) {
        push_vkey(KEY_ESCAPE); /* in the hub, back is Esc: leave the screen */
    }
}

void gfx_touch_frame_end(const ArcView *v) {
    if (!t.on)
        return;
    t.view = *v;
    process(false);
}

void gfx_touch_flush(const ArcView *v) {
    if (!t.on)
        return;
    t.view = *v;
    t.game = false;
    t.osk_open = false;
    t.zone_n = 0;
    t.text_n = 0;
    t.back_arm = 0;
    touch_release_all(&t.ts);
    process(true);
}

void gfx_touch_game_begin(void) {
    if (!t.on || t.game)
        return;
    t.game = true; /* a loop that starts again inside the same game keeps its fingers */
    touch_release_all(&t.ts);
    t.back_arm = 0;
}

void gfx_touch_frame_begin(void) {
    t.zone_n = 0;
}

void gfx_touch_merge(ArcadeInput *in) {
    if (!t.on || !t.game)
        return;
    const TouchOut *o = &t.out;
    /* the stick adds to the keys: whichever pushes harder wins */
    if (fabsf(o->move_x) > fabsf(in->move_x))
        in->move_x = o->move_x;
    if (fabsf(o->move_y) > fabsf(in->move_y))
        in->move_y = o->move_y;
    in->left_pressed = in->left_pressed || o->left_pressed;
    in->right_pressed = in->right_pressed || o->right_pressed;
    in->up_pressed = in->up_pressed || o->up_pressed;
    in->down_pressed = in->down_pressed || o->down_pressed;
    in->action = in->action || o->action;
    in->action_pressed = in->action_pressed || o->action_pressed;
    /* The pointer is a finger of its own. raylib's emulated mouse follows the FIRST finger, which may well be
     * the one on the stick, so it is not used at all. */
    in->aim_x = o->aim_x;
    in->aim_y = o->aim_y;
    in->fire = o->fire;
    in->fire_pressed = o->fire_pressed;
    in->alt = false;
    in->alt_pressed = false;
}

/* ---- drawing ----------------------------------------------------------------------------------------------
 */

static void draw_button_ring(TouchCircle c, bool lit) {
    Vector2 p = {c.x, c.y};
    DrawCircleV(p, c.r, Fade(ARC_COL_CYAN, lit ? 0.34f : 0.09f));
    DrawRing(p, c.r - 2.0f, c.r, 0.0f, 360.0f, 48, Fade(ARC_COL_CYAN, lit ? 0.90f : 0.45f));
}

static void draw_controls(void) {
    const TouchLayout *l = &t.ts.layout;

    /* the stick: a ring, and a knob that follows the finger */
    Vector2 s = {l->stick.x, l->stick.y};
    draw_button_ring(l->stick, t.ts.stick_on);
    Vector2 knob = {s.x + t.ts.knob_x * l->stick.r * 0.55f, s.y + t.ts.knob_y * l->stick.r * 0.55f};
    DrawCircleV(knob, l->stick.r * 0.42f, Fade(ARC_COL_CYAN, t.ts.stick_on ? 0.65f : 0.28f));

    /* the action button: a ring with a smaller disc inside */
    Vector2 a = {l->action.x, l->action.y};
    draw_button_ring(l->action, t.ts.action_on);
    DrawCircleV(a, l->action.r * 0.5f, Fade(ARC_COL_CYAN, t.ts.action_on ? 0.65f : 0.28f));

    /* back: a chevron */
    Vector2 b = {l->back.x, l->back.y};
    draw_button_ring(l->back, t.back_arm > 0);
    Color chev = Fade(ARC_COL_TEXT, 0.80f);
    DrawLineEx((Vector2){b.x + 5.0f, b.y - 11.0f}, (Vector2){b.x - 6.0f, b.y}, 3.0f, chev);
    DrawLineEx((Vector2){b.x - 6.0f, b.y}, (Vector2){b.x + 5.0f, b.y + 11.0f}, 3.0f, chev);

    /* keys: a little keyboard */
    Vector2 k = {l->keys.x, l->keys.y};
    draw_button_ring(l->keys, t.osk_open);
    Rectangle body = {k.x - 14.0f, k.y - 9.0f, 28.0f, 18.0f};
    DrawRectangleRoundedLinesEx(body, 0.25f, 4, 1.5f, chev);
    for (int i = 0; i < 4; i++)
        DrawCircleV((Vector2){body.x + 5.0f + (float)i * 6.0f, k.y - 3.0f}, 1.4f, chev);
    DrawRectangle((int)k.x - 8, (int)k.y + 3, 16, 2, chev);

    if (t.back_arm > 0) {                            /* what a second tap on back would do */
        const char *msg = "tap back again to leave"; /* next to the button, where the thumb is */
        int w = gfx_text_width(msg, 18) + 32;
        Rectangle pill = {l->back.x + l->back.r + 12.0f, l->back.y - 17.0f, (float)w, 34.0f};
        DrawRectangleRounded(pill, 0.5f, 8, Fade(ARC_COL_BG, 0.90f));
        DrawRectangleRoundedLinesEx(pill, 0.5f, 8, 1.0f, Fade(ARC_COL_YELLOW, 0.70f));
        gfx_text((const char *)msg, (int)pill.x + 16, (int)pill.y + 6, 18, ARC_COL_YELLOW);
    }
}

static void draw_osk(void) {
    const Osk *k = &t.osk;
    Rectangle panel = osk_panel();
    DrawRectangleRounded(panel, 0.05f, 8, Fade(ARC_COL_BG, 0.97f));
    DrawRectangleRoundedLinesEx(panel, 0.05f, 8, 1.0f, Fade(ARC_COL_DIM, 0.55f));
    for (int i = 0; i < k->count; i++) {
        const OskKey *key = &k->keys[i];
        Rectangle r = {key->x, key->y, key->w, key->h};
        bool special = key->kind != OSK_CHAR;
        bool lit = key->kind == OSK_SHIFT && k->shift;
        Color fill = special ? ARC_COL_PANEL : gfx_rgba(30, 36, 70, 255);
        if (i == t.osk_down)
            fill = Fade(ARC_COL_CYAN, 0.60f);
        DrawRectangleRounded(r, 0.22f, 6, fill);
        DrawRectangleRoundedLinesEx(r, 0.22f, 6, lit ? 2.0f : 1.0f,
                                    lit ? ARC_COL_YELLOW : Fade(ARC_COL_DIM, 0.60f));
        int size = special ? 15 : 24;
        gfx_text_center(key->label, (int)(key->x + key->w / 2.0f),
                        (int)(key->y + (key->h - (float)size * 1.25f) / 2.0f), size,
                        i == t.osk_down ? ARC_COL_BG : ARC_COL_TEXT);
    }
}

void gfx_touch_draw(void) {
    if (!t.on)
        return;
    if (t.game)
        draw_controls();
    if (t.osk_open)
        draw_osk();
}

/* ---- the functions of gfx.h
 * ------------------------------------------------------------------------------------ */

bool arcade_touch_ui(void) {
    return t.on;
}

bool gfx_touch_testing(void) {
    return t.test;
}

void arcade_touch_test_begin(void) {
    t.test = true;
    t.on = true;
    t.injected_n = 0;
    layout_refresh();
}

void arcade_touch_test_fingers(const TouchPoint *pts, int n) {
    t.injected_n = n < TOUCH_MAX_POINTS ? n : TOUCH_MAX_POINTS;
    if (t.injected_n > 0)
        memcpy(t.injected, pts, (size_t)t.injected_n * sizeof(t.injected[0]));
}

void arcade_touch_test_end(void) {
    t.test = false;
    t.on = env_wants_touch_ui();
    t.injected_n = 0;
}

static bool vkey_has(int key) {
    for (int i = 0; i < t.vkey_n; i++)
        if (t.vkeys[i] == key)
            return true;
    return false;
}

bool arcade_key_pressed(int key) {
    return IsKeyPressed(key) || vkey_has(key);
}

int arcade_text_char(void) {
    if (t.text_n > 0) {
        int c = t.text[0];
        memmove(t.text, t.text + 1, (size_t)(t.text_n - 1) * sizeof(t.text[0]));
        t.text_n--;
        return c;
    }
    return GetCharPressed();
}

void arcade_touch_key_zone(Rectangle r, int key) {
    if (t.on && t.zone_n < MAX_ZONES) {
        t.zones[t.zone_n].r = r;
        t.zones[t.zone_n].key = key;
        t.zone_n++;
    }
}

void arcade_osk_open(bool open) {
    if (!t.on)
        return;
    if (open && !t.osk_open) { /* every opening starts on the letters, without shift */
        t.osk.symbols = false;
        t.osk.shift = false;
    }
    t.osk_open = open;
    layout_refresh();
}

bool arcade_osk_is_open(void) {
    return t.on && t.osk_open;
}

float arcade_osk_top(void) {
    return t.osk.y;
}
