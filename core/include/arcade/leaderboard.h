/* Persistent high scores. Plain text file, one entry per line:  game|player|score|unix_time
 * Demonstrates: structs, arrays, file I/O, strings, sorting (insertion sort), searching (linear + binary). */
#ifndef ARCADE_LEADERBOARD_H
#define ARCADE_LEADERBOARD_H

#include "arcade/common.h"

#define LB_MAX_ENTRIES 500
#define LB_NAME_LEN 16
#define LB_GAME_LEN 32

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
/* Copies the best `max` entries of one game (sorted by score desc) into out; returns how many. */
int lb_top_for_game(const Leaderboard *lb, const char *game, ScoreEntry *out, int max);
/* Case-insensitive substring search over player names. Returns match count; indices go to out_idx. */
int lb_find_player(const Leaderboard *lb, const char *needle, int *out_idx, int max);
/* Exact (case-insensitive) name lookup. Requires lb_sort(lb, LB_SORT_NAME_ASC) first. -1 if absent. */
int lb_binary_search_name(const Leaderboard *lb, const char *name);

#endif
