/* Persistent high scores. Plain text file, one entry per line:  game|player|score|unix_time
 * Demonstrates: structs, arrays, file I/O, strings, sorting (insertion sort), searching (linear + binary).
 *
 * BOARDS: a game that has several modes or levels can keep one high-score table per mode/level. It names the
 * table in GameContext.board (e.g. "Dodge / Bopeebo / normal") and the hub stores the score under the key
 * "<game id>:<board>" in the `game` field, so the file format does not change. A key without a ':' is a
 * plain game id: a game without boards, or the scores a game saved before it had boards. */
#ifndef ARCADE_LEADERBOARD_H
#define ARCADE_LEADERBOARD_H

#include "arcade/common.h"

#define LB_MAX_ENTRIES 500
#define LB_NAME_LEN 16
#define LB_GAME_LEN 64    /* room for "<game id>:<board>" (a game id has at most ~15 characters) */
#define LB_MAX_BOARDS 128 /* tables of one game that lb_boards_for_game can list */

typedef struct ScoreEntry {
    char game[LB_GAME_LEN];
    char player[LB_NAME_LEN];
    int score;
    long long stamp; /* unix time, 0 if unknown */
} ScoreEntry;

typedef struct Leaderboard {
    ScoreEntry items[LB_MAX_ENTRIES];
    int count;
} Leaderboard;

typedef enum LbSort { LB_SORT_SCORE_DESC, LB_SORT_NAME_ASC, LB_SORT_RECENT_FIRST } LbSort;

void lb_init(Leaderboard *lb);
/* Returns the number of entries loaded, or -1 if the file cannot be opened. Malformed lines are skipped. */
int lb_load(Leaderboard *lb, const char *path);
bool lb_save(const Leaderboard *lb, const char *path);
/* Adds an entry (name is sanitised). When full, replaces the lowest score if the new one is higher. */
bool lb_add(Leaderboard *lb, const char *game, const char *player, int score, long long stamp);
void lb_sort(Leaderboard *lb, LbSort order); /* stable insertion sort */
/* Copies the best `max` entries stored under exactly this key (sorted by score desc) into out; returns how
 * many. The key is a plain game id or a board key ("beat_dodge:Dodge / Bopeebo / normal"): a table. */
int lb_top_for_game(const Leaderboard *lb, const char *game, ScoreEntry *out, int max);

/* ---- boards (see the top of this file) ---- */
/* Builds the key of a table: "<game>:<board>", or just "<game>" when `board` is NULL or empty. Characters
 * that cannot be stored ('|', control characters) are replaced exactly like lb_add does, so the key can be
 * used to look up what lb_add stored. */
void lb_board_key(char *out, size_t size, const char *game, const char *board);
/* The board part of a stored key ("Dodge / Bopeebo / normal"), or "" when the key is a plain game id. */
const char *lb_key_board(const char *key);
/* true when `key` belongs to `game`: the plain id itself or any "<game>:<board>". "beat_dodge2" does not. */
bool lb_key_in_game(const char *key, const char *game);
/* The tables of one game that hold at least one score (the plain id counts as one), each key once, sorted
 * A-Z. At most `max` keys are copied into keys[]; returns how many. */
int lb_boards_for_game(const Leaderboard *lb, const char *game, char keys[][LB_GAME_LEN], int max);
/* Like lb_top_for_game, but over every table of the game at once: the plain id and all its boards. */
int lb_top_for_all_boards(const Leaderboard *lb, const char *game, ScoreEntry *out, int max);
/* Case-insensitive substring search over player names. Returns match count; indices go to out_idx. */
int lb_find_player(const Leaderboard *lb, const char *needle, int *out_idx, int max);
/* Exact (case-insensitive) name lookup. Requires lb_sort(lb, LB_SORT_NAME_ASC) first. -1 if absent. */
int lb_binary_search_name(const Leaderboard *lb, const char *name);

#endif
