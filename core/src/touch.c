#include <math.h>
#include <string.h>

#include "arcade/touch.h"

/* ---- layout ------------------------------------------------------------------------------------------- */

#define EDGE 48.0f       /* distance from the window edge: clears rounded corners and camera cut-outs */
#define STICK_R 84.0f    /* the stick's visible radius */
#define ACTION_R 64.0f   /* the action button's radius */
#define SMALL_R 28.0f    /* the back and keys buttons */
#define BUTTON_GAP 30.0f /* between the stick / action button and the small button above it */
#define STICK_GRAB 1.35f /* a finger landing within this many radii of the stick centre takes the stick */
#define BUTTON_GRAB 1.3f /* the same for the round buttons: a thumb is bigger than the drawing */
#define DEADZONE 0.20f   /* the stick reads 0 closer to the centre than this (fraction of the radius) */
#define DIR_ON 0.50f     /* the stick counts as pushed left/right/up/down beyond this */

void touch_layout_fit(TouchLayout *l, float left, float top, float right, float bottom) {
    l->stick = (TouchCircle){left + EDGE + STICK_R, bottom - EDGE - STICK_R, STICK_R};
    l->action = (TouchCircle){right - EDGE - ACTION_R, bottom - EDGE - ACTION_R - 20.0f, ACTION_R};
    /* back and keys sit above the stick and the action button, where the thumbs rest. The top of the screen
     * is the games' (title bar, lives, scores, help text) and stays free. */
    float back_y = l->stick.y - l->stick.r - BUTTON_GAP - SMALL_R;
    float keys_y = l->action.y - l->action.r - BUTTON_GAP - SMALL_R;
    l->back = (TouchCircle){left + EDGE + SMALL_R, arc_maxf(back_y, top + SMALL_R), SMALL_R};
    l->keys = (TouchCircle){right - EDGE - SMALL_R, arc_maxf(keys_y, top + SMALL_R), SMALL_R};
}

static bool in_circle(TouchCircle c, float grow, float x, float y) {
    float dx = x - c.x, dy = y - c.y, r = c.r * grow;
    return dx * dx + dy * dy <= r * r;
}

/* ---- state ------------------------------------------------------------------------------------------- */

void touch_init(TouchState *t) {
    memset(t, 0, sizeof(*t));
    touch_layout_fit(&t->layout, 0.0f, 0.0f, (float)ARC_SCREEN_W, (float)ARC_SCREEN_H);
    t->aim_x = (float)ARC_SCREEN_W / 2.0f;
    t->aim_y = (float)ARC_SCREEN_H / 2.0f;
}

void touch_release_all(TouchState *t) {
    t->hold_n = 0;
    memset(t->dir, 0, sizeof(t->dir));
    t->knob_x = t->knob_y = 0.0f;
    t->stick_on = t->action_on = false;
}

static int find_hold(const TouchState *t, int id) {
    for (int i = 0; i < t->hold_n; i++)
        if (t->hold[i].id == id)
            return i;
    return -1;
}

static bool kind_held(const TouchState *t, TouchKind k) {
    for (int i = 0; i < t->hold_n; i++)
        if (t->hold[i].kind == k)
            return true;
    return false;
}

static const TouchPoint *find_point(const TouchPoint *pts, int n, int id) {
    for (int i = 0; i < n; i++)
        if (pts[i].id == id)
            return &pts[i];
    return NULL;
}

/* Forgets the fingers that were lifted since the last frame. */
static void drop_lifted(TouchState *t, const TouchPoint *pts, int n) {
    int kept = 0;
    for (int i = 0; i < t->hold_n; i++)
        if (find_point(pts, n, t->hold[i].id) != NULL)
            t->hold[kept++] = t->hold[i];
    t->hold_n = kept;
}

/* A finger that has just landed: what is it? Fills the "pressed" fields of `out` for buttons and the pointer.
 */
static TouchKind classify_new(const TouchState *t, const TouchPoint *p, TouchOut *out) {
    const TouchLayout *l = &t->layout;
    if (in_circle(l->stick, STICK_GRAB, p->x, p->y))
        return kind_held(t, TOUCH_STICK) ? TOUCH_IGNORED : TOUCH_STICK;
    if (in_circle(l->action, BUTTON_GRAB, p->x, p->y)) {
        if (kind_held(t, TOUCH_ACTION))
            return TOUCH_IGNORED;
        out->action_pressed = true;
        return TOUCH_ACTION;
    }
    if (in_circle(l->back, BUTTON_GRAB, p->x, p->y)) {
        out->back_pressed = true;
        return TOUCH_IGNORED; /* a button: it acts once, and the finger is no pointer afterwards */
    }
    if (in_circle(l->keys, BUTTON_GRAB, p->x, p->y)) {
        out->keys_pressed = true;
        return TOUCH_IGNORED;
    }
    if (kind_held(t, TOUCH_POINTER))
        return TOUCH_IGNORED; /* one pointer is enough */
    out->fire_pressed = true;
    return TOUCH_POINTER;
}

/* The stick: the finger's offset from the centre, as a fraction of the radius, with a dead zone. */
static void read_stick(TouchState *t, const TouchPoint *p, TouchOut *out) {
    float dx = (p->x - t->layout.stick.x) / t->layout.stick.r;
    float dy = (p->y - t->layout.stick.y) / t->layout.stick.r;
    float len = sqrtf(dx * dx + dy * dy);
    if (len > 1.0f) { /* outside the circle: same direction, full push */
        dx /= len;
        dy /= len;
        len = 1.0f;
    }
    t->knob_x = dx;
    t->knob_y = dy;
    if (len < DEADZONE)
        return; /* move_x / move_y stay 0 */
    out->move_x = dx;
    out->move_y = dy;
}

void touch_update(TouchState *t, const TouchPoint *pts, int n, TouchOut *out) {
    memset(out, 0, sizeof(*out));
    if (n > TOUCH_MAX_POINTS)
        n = TOUCH_MAX_POINTS;
    drop_lifted(t, pts, n);

    /* new fingers */
    for (int i = 0; i < n; i++) {
        if (find_hold(t, pts[i].id) >= 0 || t->hold_n >= TOUCH_MAX_POINTS)
            continue;
        t->hold[t->hold_n].id = pts[i].id;
        t->hold[t->hold_n].kind = classify_new(t, &pts[i], out);
        t->hold_n++;
    }

    /* what the fingers that are held say right now */
    t->knob_x = t->knob_y = 0.0f;
    t->stick_on = t->action_on = false;
    out->fire = false;
    for (int i = 0; i < t->hold_n; i++) {
        const TouchPoint *p = find_point(pts, n, t->hold[i].id);
        if (p == NULL)
            continue;
        switch (t->hold[i].kind) {
        case TOUCH_STICK:
            t->stick_on = true;
            read_stick(t, p, out);
            break;
        case TOUCH_ACTION:
            t->action_on = true;
            out->action = true;
            break;
        case TOUCH_POINTER:
            t->aim_x = p->x;
            t->aim_y = p->y;
            out->fire = true;
            break;
        default:
            break;
        }
    }
    out->aim_x = t->aim_x;
    out->aim_y = t->aim_y;

    /* one-shot direction presses: the frame the stick crosses the threshold */
    bool dir[4] = {out->move_x<-DIR_ON, out->move_x> DIR_ON, out->move_y<-DIR_ON, out->move_y> DIR_ON};
    out->left_pressed = dir[0] && !t->dir[0];
    out->right_pressed = dir[1] && !t->dir[1];
    out->up_pressed = dir[2] && !t->dir[2];
    out->down_pressed = dir[3] && !t->dir[3];
    memcpy(t->dir, dir, sizeof(dir));
}
