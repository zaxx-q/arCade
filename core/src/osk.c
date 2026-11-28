#include <string.h>

#include "arcade/osk.h"
#include "arcade/str.h"

#define KEY_H 46.0f
#define KEY_GAP 6.0f
#define KEY_W_MAX 80.0f
#define PAD 10.0f /* panel padding around the keys */

/* The rows of each page. A row of characters becomes one key each; the special keys are added by osk_layout.
 */
static const char *const LETTER_ROWS[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
static const char *const SYMBOL_ROWS[] = {"1234567890", "-+=[]<>/^#", ".,:;!?'\"_", "@$%&*()~"};

static bool is_letter(char c) {
    return c >= 'a' && c <= 'z';
}

static void add_key(Osk *k, float x, float y, float w, OskKind kind, char ch, const char *label) {
    if (k->count >= OSK_MAX_KEYS)
        return;
    OskKey *key = &k->keys[k->count++];
    key->x = x;
    key->y = y;
    key->w = w;
    key->h = KEY_H;
    key->kind = kind;
    key->ch = ch;
    arc_strlcpy(key->label, label, sizeof(key->label));
}

/* The label of a character key: upper case while shift is on. */
static void char_label(const Osk *k, char ch, char *out) {
    out[0] = (k->shift && is_letter(ch)) ? (char)(ch - 'a' + 'A') : ch;
    out[1] = '\0';
}

void osk_init(Osk *k) {
    memset(k, 0, sizeof(*k));
}

/* One row of character keys, centred on cx; the keys at the ends can be wider (shift / backspace). */
static void add_char_row(Osk *k, const char *row, float cx, float y, float kw, OskKind left, OskKind right) {
    int n = (int)strlen(row);
    int extra = (left != OSK_CHAR ? 1 : 0) + (right != OSK_CHAR ? 1 : 0);
    float wide = kw * 1.5f; /* shift and backspace are a little wider than a letter */
    float total = (float)n * kw + (float)(n - 1) * KEY_GAP;
    if (extra > 0)
        total += (float)extra * (wide + KEY_GAP);
    float x = cx - total / 2.0f;
    if (left == OSK_SHIFT) {
        add_key(k, x, y, wide, OSK_SHIFT, 0, "Shift");
        x += wide + KEY_GAP;
    }
    for (int i = 0; i < n; i++) {
        char label[2];
        char_label(k, row[i], label);
        add_key(k, x, y, kw, OSK_CHAR, row[i], label);
        x += kw + KEY_GAP;
    }
    if (right == OSK_BACKSPACE)
        add_key(k, x, y, wide, OSK_BACKSPACE, 0, "Del");
}

void osk_layout(Osk *k, float cx, float bottom, float max_w) {
    k->cx = cx;
    k->bottom = bottom;
    k->max_w = max_w;
    k->count = 0;
    const char *const *rows = k->symbols ? SYMBOL_ROWS : LETTER_ROWS;

    /* the widest row has 10 keys: the key width follows from the room we have */
    float kw = (max_w - 2.0f * PAD - 9.0f * KEY_GAP) / 10.0f;
    if (kw > KEY_W_MAX)
        kw = KEY_W_MAX;
    float rows_h = 5.0f * KEY_H + 4.0f * KEY_GAP;
    float top = bottom - PAD - rows_h;

    for (int r = 0; r < 4; r++) {
        float y = top + (float)r * (KEY_H + KEY_GAP);
        if (r == 3) /* the last character row also has the wide keys */
            add_char_row(k, rows[r], cx, y, kw, k->symbols ? OSK_CHAR : OSK_SHIFT, OSK_BACKSPACE);
        else
            add_char_row(k, rows[r], cx, y, kw, OSK_CHAR, OSK_CHAR);
    }

    /* the bottom row: page switch, space bar, enter */
    float y = top + 4.0f * (KEY_H + KEY_GAP);
    float page_w = kw * 2.0f, enter_w = kw * 2.0f;
    float space_w = 10.0f * kw + 9.0f * KEY_GAP - page_w - enter_w - 2.0f * KEY_GAP;
    float x = cx - (10.0f * kw + 9.0f * KEY_GAP) / 2.0f;
    add_key(k, x, y, page_w, OSK_PAGE, 0, k->symbols ? "abc" : "?123");
    add_key(k, x + page_w + KEY_GAP, y, space_w, OSK_SPACE, ' ', "space");
    add_key(k, x + page_w + space_w + 2.0f * KEY_GAP, y, enter_w, OSK_ENTER, 0, "Enter");

    k->w = 10.0f * kw + 9.0f * KEY_GAP + 2.0f * PAD;
    k->h = rows_h + 2.0f * PAD;
    k->x = cx - k->w / 2.0f;
    k->y = bottom - k->h;
}

int osk_hit(const Osk *k, float x, float y) {
    /* A tap in the gap between two keys goes to the nearest one: thumbs are not precise. */
    int best = -1;
    float best_d = 0.0f;
    for (int i = 0; i < k->count; i++) {
        const OskKey *key = &k->keys[i];
        float grow = KEY_GAP / 2.0f;
        if (x < key->x - grow || x > key->x + key->w + grow || y < key->y - grow ||
            y > key->y + key->h + grow)
            continue;
        float dx = x - (key->x + key->w / 2.0f), dy = y - (key->y + key->h / 2.0f);
        float d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

void osk_press(Osk *k, int index, OskEvent *ev) {
    OskEvent none = {0, false, false};
    if (ev != NULL)
        *ev = none;
    if (index < 0 || index >= k->count)
        return;
    OskKey key = k->keys[index]; /* a copy: a page change rebuilds the array */
    switch (key.kind) {
    case OSK_CHAR:
        if (ev != NULL)
            ev->ch = (k->shift && is_letter(key.ch)) ? key.ch - 'a' + 'A' : key.ch;
        if (is_letter(key.ch) && k->shift) {
            k->shift = false; /* one-shot */
            osk_layout(k, k->cx, k->bottom, k->max_w);
        }
        break;
    case OSK_SPACE:
        if (ev != NULL)
            ev->ch = ' ';
        break;
    case OSK_BACKSPACE:
        if (ev != NULL)
            ev->backspace = true;
        break;
    case OSK_ENTER:
        if (ev != NULL)
            ev->enter = true;
        break;
    case OSK_SHIFT:
        k->shift = !k->shift;
        osk_layout(k, k->cx, k->bottom, k->max_w);
        break;
    case OSK_PAGE:
        k->symbols = !k->symbols;
        k->shift = false;
        osk_layout(k, k->cx, k->bottom, k->max_w);
        break;
    }
}
