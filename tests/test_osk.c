/* On-screen keyboard (core/src/osk.c): layout, hit test, shift and pages. */
#include <string.h>

#include "arcade/osk.h"
#include "arcade_test.h"

static Osk kb;

static void setup(void) {
    osk_init(&kb);
    osk_layout(&kb, 640.0f, 720.0f, 840.0f);
}

/* The index of the character key `ch` on the page showing now, or -1. */
static int find_char(char ch) {
    for (int i = 0; i < kb.count; i++)
        if (kb.keys[i].kind == OSK_CHAR && kb.keys[i].ch == ch)
            return i;
    return -1;
}

static int find_kind(OskKind kind) {
    for (int i = 0; i < kb.count; i++)
        if (kb.keys[i].kind == kind)
            return i;
    return -1;
}

static int type(char ch) {
    OskEvent ev;
    int i = find_char(ch);
    if (i < 0)
        return -1;
    osk_press(&kb, i, &ev);
    return ev.ch;
}

static void test_keys_fit_and_do_not_overlap(void) {
    for (int page = 0; page < 2; page++) {
        setup();
        if (page == 1)
            osk_press(&kb, find_kind(OSK_PAGE), NULL);
        CHECK(kb.count > 0 && kb.count <= OSK_MAX_KEYS);
        for (int i = 0; i < kb.count; i++) {
            const OskKey *a = &kb.keys[i];
            CHECK(a->x >= kb.x && a->x + a->w <= kb.x + kb.w);
            CHECK(a->y >= kb.y && a->y + a->h <= 720.0f);
            for (int j = i + 1; j < kb.count; j++) {
                const OskKey *b = &kb.keys[j];
                bool apart =
                    a->x + a->w <= b->x || b->x + b->w <= a->x || a->y + a->h <= b->y || b->y + b->h <= a->y;
                CHECK(apart);
            }
        }
    }
}

static void test_everything_the_games_and_names_need_can_be_typed(void) {
    const char *wanted = "abcdefghijklmnopqrstuvwxyz0123456789 -+=[]<>/^#.,:;!?'\"_@$%&*()~";
    for (const char *c = wanted; *c != '\0'; c++) {
        setup();
        int got = 0;
        if (*c == ' ') {
            OskEvent ev;
            osk_press(&kb, find_kind(OSK_SPACE), &ev);
            got = ev.ch;
        } else {
            got = type(*c);
            if (got < 0) { /* not on the letters page: switch and try again */
                osk_press(&kb, find_kind(OSK_PAGE), NULL);
                got = type(*c);
            }
        }
        CHECK_EQ_INT(got, *c);
    }
}

static void test_shift_is_one_shot_and_changes_the_labels(void) {
    setup();
    CHECK_STR_EQ(kb.keys[find_char('q')].label, "q");
    osk_press(&kb, find_kind(OSK_SHIFT), NULL);
    CHECK(kb.shift);
    CHECK_STR_EQ(kb.keys[find_char('q')].label, "Q");
    CHECK_EQ_INT(type('q'), 'Q');
    CHECK(!kb.shift); /* used up */
    CHECK_EQ_INT(type('q'), 'q');
    osk_press(&kb, find_kind(OSK_SHIFT), NULL); /* shift twice cancels */
    osk_press(&kb, find_kind(OSK_SHIFT), NULL);
    CHECK_EQ_INT(type('w'), 'w');
}

static void test_digits_ignore_shift_and_keep_it(void) {
    setup();
    osk_press(&kb, find_kind(OSK_SHIFT), NULL);
    CHECK_EQ_INT(type('5'), '5');
    CHECK(kb.shift); /* only a letter uses the shift up */
}

static void test_page_switch(void) {
    setup();
    CHECK(find_char('#') < 0);
    CHECK_STR_EQ(kb.keys[find_kind(OSK_PAGE)].label, "?123");
    osk_press(&kb, find_kind(OSK_PAGE), NULL);
    CHECK(kb.symbols);
    CHECK(find_char('#') >= 0);
    CHECK(find_char('q') < 0);
    CHECK_STR_EQ(kb.keys[find_kind(OSK_PAGE)].label, "abc");
    CHECK(find_kind(OSK_SHIFT) < 0); /* no shift on the symbols page */
    osk_press(&kb, find_kind(OSK_PAGE), NULL);
    CHECK(!kb.symbols);
    CHECK(find_char('q') >= 0);
}

static void test_backspace_enter_space_events(void) {
    setup();
    OskEvent ev;
    osk_press(&kb, find_kind(OSK_BACKSPACE), &ev);
    CHECK(ev.backspace && !ev.enter && ev.ch == 0);
    osk_press(&kb, find_kind(OSK_ENTER), &ev);
    CHECK(ev.enter && !ev.backspace && ev.ch == 0);
    osk_press(&kb, find_kind(OSK_SPACE), &ev);
    CHECK_EQ_INT(ev.ch, ' ');
    osk_press(&kb, -1, &ev); /* a miss is nothing */
    CHECK(ev.ch == 0 && !ev.enter && !ev.backspace);
    osk_press(&kb, 999, NULL); /* and never crashes */
}

static void test_hit_test_centre_gap_and_outside(void) {
    setup();
    for (int i = 0; i < kb.count; i++) {
        const OskKey *k = &kb.keys[i];
        CHECK_EQ_INT(osk_hit(&kb, k->x + k->w / 2.0f, k->y + k->h / 2.0f), i);
    }
    /* just past the right edge of a key, inside the gap: still that key (or its neighbour), never -1 */
    const OskKey *q = &kb.keys[find_char('q')];
    CHECK(osk_hit(&kb, q->x + q->w + 2.0f, q->y + q->h / 2.0f) >= 0);
    CHECK_EQ_INT(osk_hit(&kb, 5.0f, 5.0f), -1);
    CHECK_EQ_INT(osk_hit(&kb, 640.0f, 100.0f), -1);
}

static void test_narrow_room_makes_smaller_keys(void) {
    osk_init(&kb);
    osk_layout(&kb, 400.0f, 700.0f, 500.0f);
    CHECK(kb.w <= 500.0f + 0.01f);
    CHECK(kb.x >= 150.0f - 0.01f);
}

int main(void) {
    RUN(test_keys_fit_and_do_not_overlap);
    RUN(test_everything_the_games_and_names_need_can_be_typed);
    RUN(test_shift_is_one_shot_and_changes_the_labels);
    RUN(test_digits_ignore_shift_and_keep_it);
    RUN(test_page_switch);
    RUN(test_backspace_enter_space_events);
    RUN(test_hit_test_centre_gap_and_outside);
    RUN(test_narrow_room_makes_smaller_keys);
    return TEST_SUMMARY();
}
