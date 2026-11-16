#include <stdio.h>
#include <string.h>

#include "arcade/game.h"
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

/* ---------------------------------------------------------------- boards (one table per mode / level) */
#define BD_DODGE_HARD "beat_dodge:Dodge / Bopeebo / hard"
#define BD_CATCH_HARD "beat_dodge:Catch / Bopeebo / hard"

static void test_board_key(void) {
    char key[LB_GAME_LEN];
    lb_board_key(key, sizeof(key), "beat_dodge", "Dodge / Bopeebo / normal");
    CHECK_STR_EQ(key, "beat_dodge:Dodge / Bopeebo / normal");
    CHECK_STR_EQ(lb_key_board(key), "Dodge / Bopeebo / normal");
    lb_board_key(key, sizeof(key), "echo_maze", ""); /* no board: the plain game id, as before */
    CHECK_STR_EQ(key, "echo_maze");
    CHECK_STR_EQ(lb_key_board(key), "");
    lb_board_key(key, sizeof(key), "echo_maze", NULL);
    CHECK_STR_EQ(key, "echo_maze");
    CHECK_STR_EQ(lb_key_board("a:b:c"), "b:c"); /* only the first ':' ends the game id */
}

static void test_board_key_matches_what_lb_add_stores(void) {
    /* '|' would break the file: the key is cleaned like lb_add cleans it, so the rank lookup finds the score
     */
    char key[LB_GAME_LEN];
    lb_board_key(key, sizeof(key), "g", "Re|Zero / hard");
    CHECK_STR_EQ(key, "g:Re_Zero / hard");
    lb_init(&g_lb);
    CHECK(lb_add(&g_lb, key, "ana", 7, 0));
    CHECK_STR_EQ(g_lb.items[0].game, key);
    ScoreEntry top[2];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, key, top, 2), 1);
    /* the longest key that can exist is stored whole: a 15-character game id + ':' + a full board name */
    char board[GAME_BOARD_LEN];
    memset(board, 'x', sizeof(board) - 1);
    board[sizeof(board) - 1] = '\0';
    CHECK(LB_GAME_LEN >= 15 + 1 + GAME_BOARD_LEN);
    lb_board_key(key, sizeof(key), "different_color", board);
    CHECK_EQ_INT(strlen(key), 15 + 1 + (GAME_BOARD_LEN - 1));
    CHECK(lb_add(&g_lb, key, "ana", 8, 0));
    CHECK_STR_EQ(g_lb.items[1].game, key);
}

static void test_key_in_game(void) {
    CHECK(lb_key_in_game("beat_dodge", "beat_dodge")); /* the plain id: old scores / no boards */
    CHECK(lb_key_in_game(BD_DODGE_HARD, "beat_dodge"));
    CHECK(!lb_key_in_game("beat_dodge2", "beat_dodge")); /* another game that starts with the same letters */
    CHECK(!lb_key_in_game("beat_dodge2:Dodge / x / easy", "beat_dodge"));
    CHECK(!lb_key_in_game("beat", "beat_dodge"));
    CHECK(!lb_key_in_game("echo_maze", "beat_dodge"));
}

static void test_boards_for_game_lists_each_table_once_sorted(void) {
    static char keys[LB_MAX_BOARDS][LB_GAME_LEN];
    lb_init(&g_lb);
    lb_add(&g_lb, BD_DODGE_HARD, "ana", 900, 1);
    lb_add(&g_lb, BD_DODGE_HARD, "budi", 800, 2); /* same table again: listed once */
    lb_add(&g_lb, "beat_dodge:Catch / Bopeebo / easy", "ana", 700, 3);
    lb_add(&g_lb, "beat_dodge", "old", 600, 4); /* scores from before boards existed */
    lb_add(&g_lb, "beat_dodge2:Dodge / x / easy", "zed", 500, 5);
    lb_add(&g_lb, "echo_maze", "ana", 400, 6);
    CHECK_EQ_INT(lb_boards_for_game(&g_lb, "beat_dodge", keys, LB_MAX_BOARDS), 3);
    CHECK_STR_EQ(keys[0], "beat_dodge");
    CHECK_STR_EQ(keys[1], "beat_dodge:Catch / Bopeebo / easy");
    CHECK_STR_EQ(keys[2], BD_DODGE_HARD);
    CHECK_EQ_INT(lb_boards_for_game(&g_lb, "beat_dodge", keys, 2), 2); /* never more than `max` */
    CHECK_EQ_INT(lb_boards_for_game(&g_lb, "echo_maze", keys, LB_MAX_BOARDS), 1);
    CHECK_EQ_INT(lb_boards_for_game(&g_lb, "nothing", keys, LB_MAX_BOARDS), 0);
}

static void test_each_board_keeps_its_own_high_scores(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, BD_DODGE_HARD, "ana", 900, 1);
    lb_add(&g_lb, BD_CATCH_HARD, "budi", 1500, 2);
    lb_add(&g_lb, BD_DODGE_HARD, "citra", 400, 3);
    lb_add(&g_lb, "beat_dodge", "old", 9999, 4);
    ScoreEntry top[10];
    /* one table: only its own scores, in order */
    CHECK_EQ_INT(lb_top_for_game(&g_lb, BD_DODGE_HARD, top, 10), 2);
    CHECK_EQ_INT(top[0].score, 900);
    CHECK_EQ_INT(top[1].score, 400);
    CHECK_EQ_INT(lb_top_for_game(&g_lb, BD_CATCH_HARD, top, 10), 1);
    CHECK_EQ_INT(top[0].score, 1500);
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "beat_dodge", top, 10), 1); /* the old scores stay their own table */
    CHECK_EQ_INT(top[0].score, 9999);
    /* a new best in one table is not held back by a higher score in another */
    lb_add(&g_lb, BD_DODGE_HARD, "dewi", 1000, 5);
    CHECK_EQ_INT(lb_top_for_game(&g_lb, BD_DODGE_HARD, top, 10), 3);
    CHECK_STR_EQ(top[0].player, "dewi"); /* rank 1 here although Catch has 1500 */
    /* the whole game at once: every table, best first */
    CHECK_EQ_INT(lb_top_for_all_boards(&g_lb, "beat_dodge", top, 10), 5);
    CHECK_EQ_INT(top[0].score, 9999);
    CHECK_EQ_INT(top[1].score, 1500);
    CHECK_EQ_INT(top[4].score, 400);
}

static void test_top_for_all_boards_is_the_old_behaviour_for_a_game_without_boards(void) {
    lb_init(&g_lb);
    for (int i = 0; i < 12; i++)
        lb_add(&g_lb, i % 3 == 0 ? "echo_maze" : "sort_it", "p", i * 10, i);
    ScoreEntry a[4], b[4];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "echo_maze", a, 4), 4);
    CHECK_EQ_INT(lb_top_for_all_boards(&g_lb, "echo_maze", b, 4), 4);
    for (int i = 0; i < 4; i++)
        CHECK_EQ_INT(a[i].score, b[i].score);
    CHECK_EQ_INT(b[0].score, 90);
    CHECK_EQ_INT(lb_top_for_all_boards(&g_lb, "nothing", b, 4), 0);
}

static void test_board_scores_survive_save_and_load(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, BD_DODGE_HARD, "ana", 900, 1700000000LL);
    lb_add(&g_lb, BD_CATCH_HARD, "budi", 1500, 1700000100LL);
    CHECK(lb_save(&g_lb, TMP_FILE));
    static Leaderboard other;
    CHECK_EQ_INT(lb_load(&other, TMP_FILE), 2);
    CHECK_STR_EQ(other.items[0].game, BD_DODGE_HARD);
    ScoreEntry top[2];
    CHECK_EQ_INT(lb_top_for_game(&other, BD_CATCH_HARD, top, 2), 1);
    CHECK_EQ_INT(top[0].score, 1500);
    remove(TMP_FILE);
    /* a file written before boards existed still loads: a plain id is a table of its own */
    FILE *fp = fopen(TMP_FILE, "w");
    CHECK(fp != NULL);
    fputs("beat_dodge|ana|1200|1700000000\n", fp);
    fclose(fp);
    CHECK_EQ_INT(lb_load(&other, TMP_FILE), 1);
    CHECK_EQ_INT(lb_top_for_game(&other, "beat_dodge", top, 2), 1);
    remove(TMP_FILE);
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
    RUN(test_board_key);
    RUN(test_board_key_matches_what_lb_add_stores);
    RUN(test_key_in_game);
    RUN(test_boards_for_game_lists_each_table_once_sorted);
    RUN(test_each_board_keeps_its_own_high_scores);
    RUN(test_top_for_all_boards_is_the_old_behaviour_for_a_game_without_boards);
    RUN(test_board_scores_survive_save_and_load);
    return TEST_SUMMARY();
}
