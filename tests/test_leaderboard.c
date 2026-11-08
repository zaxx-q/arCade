#include <stdio.h>
#include <string.h>

#include "arcade/leaderboard.h"
#include "arcade_test.h"

#define TMP_FILE "arcade_test_scores.tmp"

static Leaderboard g_lb; /* big struct: keep it off the stack */

static void test_add_and_sort(void) {
    lb_init(&g_lb);
    CHECK(lb_add(&g_lb, "a", "carol", 50, 3));
    CHECK(lb_add(&g_lb, "a", "alice", 90, 1));
    CHECK(lb_add(&g_lb, "b", "Bob", 70, 2));
    lb_sort(&g_lb, LB_SORT_SCORE_DESC);
    CHECK_EQ_INT(g_lb.items[0].score, 90);
    CHECK_EQ_INT(g_lb.items[2].score, 50);
    lb_sort(&g_lb, LB_SORT_NAME_ASC);
    CHECK_STR_EQ(g_lb.items[0].player, "alice");
    CHECK_STR_EQ(g_lb.items[1].player, "Bob"); /* case-insensitive order */
    lb_sort(&g_lb, LB_SORT_RECENT_FIRST);
    CHECK_STR_EQ(g_lb.items[0].player, "carol");
}

static void test_sort_is_stable(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "first", 10, 1);
    lb_add(&g_lb, "g", "second", 10, 2);
    lb_add(&g_lb, "g", "third", 10, 3);
    lb_sort(&g_lb, LB_SORT_SCORE_DESC);
    CHECK_STR_EQ(g_lb.items[0].player, "first");
    CHECK_STR_EQ(g_lb.items[2].player, "third");
}

static void test_sanitise_names(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g|x", "ev|il\nname", 1, 0);
    lb_add(&g_lb, "g", "", 2, 0);
    lb_add(&g_lb, "g", "ThisNameIsWayTooLongForTheField", 3, 0);
    CHECK_STR_EQ(g_lb.items[0].player, "ev_il_name");
    CHECK_STR_EQ(g_lb.items[0].game, "g_x");
    CHECK_STR_EQ(g_lb.items[1].player, "PLAYER");
    CHECK_EQ_INT(strlen(g_lb.items[2].player), LB_NAME_LEN - 1);
}

static void test_save_and_load_roundtrip(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "echo_maze", "ana", 120, 1700000000LL);
    lb_add(&g_lb, "sort_it", "budi", 80, 1700000100LL);
    CHECK(lb_save(&g_lb, TMP_FILE));
    Leaderboard *back = &g_lb;
    static Leaderboard other;
    CHECK_EQ_INT(lb_load(&other, TMP_FILE), 2);
    CHECK_STR_EQ(other.items[1].game, "sort_it");
    CHECK_EQ_INT(other.items[0].score, 120);
    CHECK_EQ_INT(other.items[1].stamp, 1700000100LL);
    (void)back;
    remove(TMP_FILE);
}

static void test_load_missing_and_corrupt(void) {
    CHECK_EQ_INT(lb_load(&g_lb, "definitely_missing_file.tmp"), -1);
    CHECK_EQ_INT(g_lb.count, 0);
    FILE *fp = fopen(TMP_FILE, "w");
    CHECK(fp != NULL);
    fputs("game|ok|10|1\n", fp);
    fputs("garbage line\n", fp);
    fputs("game|bad|notanumber|1\n", fp);
    fputs("|noname|5|1\n", fp);
    fputs("game|crlf|7|2\r\n", fp);
    fputs("a|b|c\n", fp);
    fclose(fp);
    CHECK_EQ_INT(lb_load(&g_lb, TMP_FILE), 2); /* only the two valid lines survive */
    CHECK_STR_EQ(g_lb.items[1].player, "crlf");
    remove(TMP_FILE);
}

static void test_full_board_replaces_lowest(void) {
    lb_init(&g_lb);
    for (int i = 0; i < LB_MAX_ENTRIES; i++)
        lb_add(&g_lb, "g", "p", 100 + i, 0);
    CHECK_EQ_INT(g_lb.count, LB_MAX_ENTRIES);
    CHECK(!lb_add(&g_lb, "g", "low", 5, 0));    /* worse than everything: rejected */
    CHECK(lb_add(&g_lb, "g", "high", 9999, 0)); /* replaces the lowest (100) */
    CHECK_EQ_INT(g_lb.count, LB_MAX_ENTRIES);
    int has100 = 0;
    for (int i = 0; i < g_lb.count; i++)
        has100 += g_lb.items[i].score == 100;
    CHECK_EQ_INT(has100, 0);
}

static void test_top_for_game(void) {
    lb_init(&g_lb);
    for (int i = 0; i < 20; i++)
        lb_add(&g_lb, i % 2 ? "odd" : "even", "p", i * 10, 0);
    ScoreEntry top[5];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "odd", top, 5), 5);
    CHECK_EQ_INT(top[0].score, 190);
    CHECK_EQ_INT(top[4].score, 110);
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "nothing", top, 5), 0);
    ScoreEntry big[20];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "even", big, 20), 10);
    CHECK_EQ_INT(big[0].score, 180);
}

static void test_search(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "Alice", 1, 0);
    lb_add(&g_lb, "g", "alicia", 2, 0);
    lb_add(&g_lb, "g", "Bob", 3, 0);
    lb_add(&g_lb, "g", "Zed", 4, 0);
    int idx[8];
    CHECK_EQ_INT(lb_find_player(&g_lb, "ALI", idx, 8), 2);
    CHECK_EQ_INT(lb_find_player(&g_lb, "xyz", idx, 8), 0);
    lb_sort(&g_lb, LB_SORT_NAME_ASC);
    int i = lb_binary_search_name(&g_lb, "bob");
    CHECK(i >= 0);
    CHECK_STR_EQ(g_lb.items[i].player, "Bob");
    CHECK_EQ_INT(lb_binary_search_name(&g_lb, "nobody"), -1);
    lb_add(&g_lb, "g", "bob", 9, 0);
    lb_sort(&g_lb, LB_SORT_NAME_ASC);
    i = lb_binary_search_name(&g_lb, "BOB");
    CHECK(i >= 0 && i + 1 < g_lb.count);
}

int main(void) {
    RUN(test_add_and_sort);
    RUN(test_sort_is_stable);
    RUN(test_sanitise_names);
    RUN(test_save_and_load_roundtrip);
    RUN(test_load_missing_and_corrupt);
    RUN(test_full_board_replaces_lowest);
    RUN(test_top_for_game);
    RUN(test_search);
    return TEST_SUMMARY();
}
