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

/* ---------------------------------------------------------------- caps: every table keeps its own rows */
/* `rows` runs of different players (p0, p1, ...) on one table: scores base, base + 1, ... */
static void fill_table(const char *key, int rows, int base) {
    for (int i = 0; i < rows; i++) {
        char who[LB_NAME_LEN];
        snprintf(who, sizeof(who), "p%d", i);
        lb_add(&g_lb, key, who, base + i, i + 1);
    }
}

static void test_full_table_replaces_its_own_lowest(void) {
    lb_init(&g_lb);
    fill_table("g", LB_MAX_PER_BOARD, 100); /* scores 100 .. 149 */
    CHECK_EQ_INT(g_lb.count, LB_MAX_PER_BOARD);
    CHECK(!lb_add(&g_lb, "g", "low", 5, 0)); /* worse than every row: not stored, nothing changes */
    CHECK_EQ_INT(g_lb.count, LB_MAX_PER_BOARD);
    CHECK(lb_add(&g_lb, "g", "high", 9999, 0)); /* replaces the lowest (100) */
    CHECK_EQ_INT(g_lb.count, LB_MAX_PER_BOARD);
    int has100 = 0;
    for (int i = 0; i < g_lb.count; i++)
        has100 += g_lb.items[i].score == 100;
    CHECK_EQ_INT(has100, 0);
}

static void test_a_full_table_never_touches_another_table(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "small", "ana", 3, 1);
    lb_add(&g_lb, "small", "budi", 2, 2);
    fill_table("big", LB_MAX_PER_BOARD, 1000);
    for (int i = 0; i < 20; i++) { /* many more runs than "big" can hold */
        char who[LB_NAME_LEN];
        snprintf(who, sizeof(who), "new%d", i);
        lb_add(&g_lb, "big", who, 5000 + i, 100 + i);
    }
    ScoreEntry top[LB_MAX_PER_BOARD + 1];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "small", top, 10), 2);
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "big", top, LB_MAX_PER_BOARD + 1), LB_MAX_PER_BOARD);
    CHECK_EQ_INT(top[0].score, 5019);
}

/* The bug this replaces: with the file full, the lowest score OF THE WHOLE FILE was dropped, so a game with a
 * small score scale (3 laps) lost its rows to games with a big one (1000 per level). */
static void test_a_full_file_takes_a_row_from_the_fullest_table(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "small", "ana", 1, 1); /* the lowest score in the file */
    for (int t = 0; g_lb.count < LB_MAX_ENTRIES; t++) {
        char key[16];
        snprintf(key, sizeof(key), "big%d", t);
        int room = LB_MAX_ENTRIES - g_lb.count;
        fill_table(key, room < LB_MAX_PER_BOARD ? room : LB_MAX_PER_BOARD, 1000);
    }
    CHECK_EQ_INT(g_lb.count, LB_MAX_ENTRIES);
    CHECK(lb_add(&g_lb, "small", "budi", 2, 2));
    CHECK_EQ_INT(g_lb.count, LB_MAX_ENTRIES); /* still full: one row went to make room */
    static ScoreEntry top[LB_MAX_PER_BOARD + 1];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "small", top, 10), 2); /* ana's row survived */
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "big0", top, LB_MAX_PER_BOARD + 1), LB_MAX_PER_BOARD - 1);
    CHECK_EQ_INT(lb_best_of(&g_lb, "big0", "p0"), -1); /* the lowest row of the fullest table was the one */
}

static void test_old_runs_of_a_player_go_before_other_players(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "budi", 1, 1); /* one low score, but it is budi's best */
    for (int i = 0; i < 60; i++)      /* ana plays far more runs than the table holds */
        lb_add(&g_lb, "g", "ana", 100 + i, 2 + i);
    CHECK_EQ_INT(g_lb.count, LB_MAX_PER_BOARD);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "budi"), 1); /* kept: ana's own old runs were dropped instead */
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "ana"), 159);
    ScoreEntry top[LB_MAX_PER_BOARD];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "g", top, LB_MAX_PER_BOARD), LB_MAX_PER_BOARD);
    CHECK_EQ_INT(top[LB_MAX_PER_BOARD - 2].score, 111); /* ana's 49 best runs ... */
    CHECK_EQ_INT(top[LB_MAX_PER_BOARD - 1].score, 1);   /* ... then budi */
}

static void test_a_worse_run_of_the_same_player_does_not_push_others_out(void) {
    lb_init(&g_lb);
    fill_table("g", LB_MAX_PER_BOARD, 100);    /* p0 has 100, the lowest */
    CHECK(!lb_add(&g_lb, "g", "p0", 90, 999)); /* p0 did worse than before: only history, not stored */
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "p1"), 101);
    CHECK(lb_add(&g_lb, "g", "p0", 120, 999)); /* a better run: p0's old 100 is now history and goes */
    CHECK_EQ_INT(g_lb.count, LB_MAX_PER_BOARD);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "p0"), 120);
}

static void test_a_table_from_an_old_file_can_be_bigger_than_the_cap(void) {
    FILE *fp = fopen(TMP_FILE, "w"); /* scores.txt from before there was a cap per table */
    CHECK(fp != NULL);
    for (int i = 0; i < 300; i++)
        fprintf(fp, "g|p%d|%d|%d\n", i, i, i + 1);
    fclose(fp);
    CHECK_EQ_INT(lb_load(&g_lb, TMP_FILE), 300);
    remove(TMP_FILE);
    CHECK(!lb_add(&g_lb, "g", "low", 0, 999999)); /* ties the lowest row but is later: not stored */
    CHECK(lb_add(&g_lb, "g", "new", 99999, 1));   /* replaces one row; the table does not grow */
    CHECK_EQ_INT(g_lb.count, 300);
    ScoreEntry top[3];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "g", top, 3), 3);
    CHECK_EQ_INT(top[0].score, 99999);
    CHECK_EQ_INT(top[1].score, 299);
}

/* ---------------------------------------------------------------- ranking: one row per player */
static void test_top_for_game_puts_the_earlier_run_first_on_a_tie(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "late", 10, 9);
    lb_add(&g_lb, "g", "early", 10, 2);
    ScoreEntry top[2];
    CHECK_EQ_INT(lb_top_for_game(&g_lb, "g", top, 2), 2);
    CHECK_STR_EQ(top[0].player, "early");
}

static void test_top_best_is_one_row_per_player(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "Ana", 100, 5);
    lb_add(&g_lb, "g", "ana", 300, 7); /* the same player: names are compared without case */
    lb_add(&g_lb, "g", "ANA", 200, 6);
    lb_add(&g_lb, "g", "budi", 300, 3); /* ties ana's 300, but earlier */
    lb_add(&g_lb, "g", "citra", 50, 1);
    lb_add(&g_lb, "h", "ana", 999, 1); /* another table: a row of its own */
    ScoreEntry top[10];
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_TABLE, "g", top, 10), 3);
    CHECK_STR_EQ(top[0].player, "budi");
    CHECK_STR_EQ(top[1].player, "ana");
    CHECK_EQ_INT(top[1].score, 300);
    CHECK_STR_EQ(top[2].player, "citra");
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_GAME, "g", top, 10), 3); /* "h" is another game */
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_ALL, NULL, top, 10), 4);
    CHECK_STR_EQ(top[0].game, "h");
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_TABLE, "g", top, 2), 2); /* never more than max */
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_TABLE, "g", top, 0), 0);
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_TABLE, "nothing", top, 10), 0);
}

static void test_top_best_across_the_boards_of_one_game(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, BD_DODGE_HARD, "ana", 900, 1);
    lb_add(&g_lb, BD_CATCH_HARD, "ana", 1500, 2); /* the same player on another board: its own row */
    lb_add(&g_lb, BD_DODGE_HARD, "ana", 400, 3);
    lb_add(&g_lb, "beat_dodge2:Dodge / x / easy", "ana", 5, 4);
    ScoreEntry top[10];
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_GAME, "beat_dodge", top, 10), 2);
    CHECK_EQ_INT(top[0].score, 1500);
    CHECK_EQ_INT(top[1].score, 900);
}

/* With max smaller than the number of players, a player pushed out of the short list and met again later must
 * still end up with his BEST row. */
static void test_top_best_with_a_short_list_keeps_the_true_best(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "a", 10, 1);
    lb_add(&g_lb, "g", "b", 20, 2);
    lb_add(&g_lb, "g", "c", 30, 3); /* list of 2: c, b */
    lb_add(&g_lb, "g", "a", 50, 4); /* a was never in the list: a, c */
    lb_add(&g_lb, "g", "b", 40, 5); /* b was pushed out: a, b */
    ScoreEntry top[2];
    CHECK_EQ_INT(lb_top_best(&g_lb, LB_SCOPE_TABLE, "g", top, 2), 2);
    CHECK_STR_EQ(top[0].player, "a");
    CHECK_EQ_INT(top[0].score, 50);
    CHECK_STR_EQ(top[1].player, "b");
    CHECK_EQ_INT(top[1].score, 40);
}

static void test_search_best(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "Alice", 10, 1);
    lb_add(&g_lb, "g", "Alice", 30, 2);
    lb_add(&g_lb, "g", "alicia", 20, 3);
    lb_add(&g_lb, "g", "Bob", 99, 4);
    lb_add(&g_lb, "h", "Alice", 5, 5);
    ScoreEntry top[10];
    CHECK_EQ_INT(lb_search_best(&g_lb, "ALI", top, 10), 3); /* Alice twice (two tables), alicia once */
    CHECK_EQ_INT(top[0].score, 30);
    CHECK_EQ_INT(top[1].score, 20);
    CHECK_EQ_INT(top[2].score, 5);
    CHECK_EQ_INT(lb_search_best(&g_lb, "zzz", top, 10), 0);
}

static void test_best_of_and_rank_of(void) {
    lb_init(&g_lb);
    lb_add(&g_lb, "g", "ana", 300, 1);
    lb_add(&g_lb, "g", "budi", 500, 2);
    lb_add(&g_lb, "g", "ana", 100, 3); /* an older, worse run does not change anything */
    lb_add(&g_lb, "g", "citra", 300, 4);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "ana"), 300);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "ANA"), 300);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", "nobody"), -1);
    CHECK_EQ_INT(lb_best_of(&g_lb, "other", "ana"), -1);
    CHECK_EQ_INT(lb_rank_of(&g_lb, "g", "budi"), 1);
    CHECK_EQ_INT(lb_rank_of(&g_lb, "g", "ana"), 2);   /* ties citra's 300, but ana got there first */
    CHECK_EQ_INT(lb_rank_of(&g_lb, "g", "citra"), 3); /* ana's two runs count as ONE player ahead */
    CHECK_EQ_INT(lb_rank_of(&g_lb, "g", "nobody"), 0);
    CHECK_EQ_INT(lb_rank_of(&g_lb, "other", "ana"), 0);
    lb_add(&g_lb, "g", "", 50, 5); /* an empty name is stored as PLAYER: a lookup finds it the same way */
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", ""), 50);
    CHECK_EQ_INT(lb_best_of(&g_lb, "g", NULL), 50);
    CHECK_EQ_INT(lb_rank_of(&g_lb, "g", "PLAYER"), 4);
}

/* The place lb_rank_of gives must be exactly the row lb_top_best shows: the result screen and the list agree.
 */
static void test_rank_of_matches_the_order_of_top_best(void) {
    lb_init(&g_lb);
    for (int i = 0; i < 30; i++) {
        char who[LB_NAME_LEN];
        snprintf(who, sizeof(who), "p%d", i % 11); /* 11 players, several runs each, plenty of ties */
        lb_add(&g_lb, "g", who, (i * 7) % 13, i);
    }
    ScoreEntry top[LB_MAX_PER_BOARD];
    int n = lb_top_best(&g_lb, LB_SCOPE_TABLE, "g", top, LB_MAX_PER_BOARD);
    CHECK_EQ_INT(n, 11);
    for (int i = 0; i < n; i++)
        CHECK_EQ_INT(lb_rank_of(&g_lb, "g", top[i].player), i + 1);
}

static void test_lb_sanitize(void) {
    char out[8];
    lb_sanitize(out, sizeof(out), "a|b\nc");
    CHECK_STR_EQ(out, "a_b_c");
    lb_sanitize(out, sizeof(out), "0123456789"); /* never more than size - 1 characters */
    CHECK_STR_EQ(out, "0123456");
    lb_sanitize(out, sizeof(out), "");
    CHECK_STR_EQ(out, "");
}

static void test_lb_parse_line(void) {
    ScoreEntry e;
    char ok[] = "echo_maze|ana|120|1700000000";
    CHECK(lb_parse_line(ok, &e));
    CHECK_STR_EQ(e.game, "echo_maze");
    CHECK_STR_EQ(e.player, "ana");
    CHECK_EQ_INT(e.score, 120);
    CHECK_EQ_INT(e.stamp, 1700000000LL);
    char few[] = "echo_maze|ana|120";     /* too few fields */
    char word[] = "echo_maze|ana|lots|1"; /* a score that is not a number */
    char empty_name[] = "echo_maze||5|1"; /* no player */
    char empty_game[] = "|ana|5|1";       /* no table */
    char junk[] = "echo_maze|ana|12x|1";  /* trailing garbage after the number */
    CHECK(!lb_parse_line(few, &e));
    CHECK(!lb_parse_line(word, &e));
    CHECK(!lb_parse_line(empty_name, &e));
    CHECK(!lb_parse_line(empty_game, &e));
    CHECK(!lb_parse_line(junk, &e));
}

int main(void) {
    RUN(test_add_and_sort);
    RUN(test_sort_is_stable);
    RUN(test_sanitise_names);
    RUN(test_save_and_load_roundtrip);
    RUN(test_load_missing_and_corrupt);
    RUN(test_full_table_replaces_its_own_lowest);
    RUN(test_a_full_table_never_touches_another_table);
    RUN(test_a_full_file_takes_a_row_from_the_fullest_table);
    RUN(test_old_runs_of_a_player_go_before_other_players);
    RUN(test_a_worse_run_of_the_same_player_does_not_push_others_out);
    RUN(test_a_table_from_an_old_file_can_be_bigger_than_the_cap);
    RUN(test_top_for_game);
    RUN(test_search);
    RUN(test_board_key);
    RUN(test_board_key_matches_what_lb_add_stores);
    RUN(test_key_in_game);
    RUN(test_boards_for_game_lists_each_table_once_sorted);
    RUN(test_each_board_keeps_its_own_high_scores);
    RUN(test_top_for_all_boards_is_the_old_behaviour_for_a_game_without_boards);
    RUN(test_board_scores_survive_save_and_load);
    RUN(test_top_for_game_puts_the_earlier_run_first_on_a_tie);
    RUN(test_top_best_is_one_row_per_player);
    RUN(test_top_best_across_the_boards_of_one_game);
    RUN(test_top_best_with_a_short_list_keeps_the_true_best);
    RUN(test_search_best);
    RUN(test_best_of_and_rank_of);
    RUN(test_rank_of_matches_the_order_of_top_best);
    RUN(test_lb_sanitize);
    RUN(test_lb_parse_line);
    return TEST_SUMMARY();
}
