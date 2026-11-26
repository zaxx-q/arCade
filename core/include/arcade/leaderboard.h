/* Persistent high scores. Plain text file, one entry per line:  game|player|score|unix_time
 * Demonstrates: structs, arrays, file I/O, strings, sorting (insertion sort), searching (linear + binary).
 *
 * ROWS AND PLAYERS: every finished run is one row (so the file is also a history). A table shows the BEST row
 * of each player (lb_top_best), so one player cannot fill the whole top 10. Each table keeps at most
 * LB_MAX_PER_BOARD rows; when it is full, the row that matters least goes first: an old run that the same
 * player has beaten, otherwise the lowest score. A game with a small score scale is therefore never pushed
 * out by a game with a big one.
 *
 * BOARDS: a game that has several modes or levels can keep one high-score table per mode/level. It names the
 * table in GameContext.board (e.g. "Dodge / Bopeebo / normal") and the hub stores the score under the key
 * "<game id>:<board>" in the `game` field, so the file format does not change. A key without a ':' is a
 * plain game id: a game without boards, or the scores a game saved before it had boards. */
#ifndef ARCADE_LEADERBOARD_H
#define ARCADE_LEADERBOARD_H

#include "arcade/common.h"

#define LB_MAX_ENTRIES 2000 /* rows in the whole file: about 190 KB, so a Leaderboard is always `static` */
#define LB_MAX_PER_BOARD 50 /* rows one table (a game id or one board key) keeps */
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

/* Which rows a query looks at: one table (exactly this key), every table of one game, or everything. */
typedef enum LbScope { LB_SCOPE_TABLE, LB_SCOPE_GAME, LB_SCOPE_ALL } LbScope;

void lb_init(Leaderboard *lb);
/* Returns the number of entries loaded, or -1 if the file cannot be opened. Malformed lines are skipped. */
int lb_load(Leaderboard *lb, const char *path);
bool lb_save(const Leaderboard *lb, const char *path);
/* The text-file helpers behind lb_load/lb_save, public so other files with the same line format (the outbox
 * of lbnet.h) read and write it the same way. lb_sanitize: copies src into dst (at most size - 1 characters),
 * turning '|' and control characters into '_'. lb_parse_line: reads "game|player|score|stamp" (the line must
 * have no newline; the buffer is modified). Returns false for a malformed line, so a damaged file never
 * produces a half-filled entry. */
void lb_sanitize(char *dst, size_t size, const char *src);
bool lb_parse_line(char *line, ScoreEntry *out);

/* Adds an entry (name is sanitised). Returns true when the row was stored. When the table of `game` is full
 * (LB_MAX_PER_BOARD rows) or the whole file is full (LB_MAX_ENTRIES), one row is dropped first: an old run of
 * a player who has since done better, otherwise the lowest score; the file-full case takes it from the
 * fullest table. Returns false (nothing changes) when the new row itself is the one that matters least. */
bool lb_add(Leaderboard *lb, const char *game, const char *player, int score, long long stamp);
void lb_sort(Leaderboard *lb, LbSort order); /* stable insertion sort */
/* Copies the best `max` entries stored under exactly this key (sorted by score desc, the earlier run first on
 * a tie) into out; returns how many. Every run counts, so one player can appear several times: use
 * lb_top_best for a ranking. The key is a plain game id or a board key ("beat_dodge:Dodge / Bopeebo /
 * normal"): a table. */
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
/* ---- ranking: one row per player ---- */
/* The best `max` rows in `scope`, at most ONE per (table, player), best first (a tie goes to the earlier
 * run). `key` is a table key for LB_SCOPE_TABLE, a game id for LB_SCOPE_GAME and ignored for LB_SCOPE_ALL.
 * Players are compared case-insensitively. Returns how many rows were copied into out. */
int lb_top_best(const Leaderboard *lb, LbScope scope, const char *key, ScoreEntry *out, int max);
/* Like lb_top_best over everything, but only for players whose name contains `needle` (case-insensitive). */
int lb_search_best(const Leaderboard *lb, const char *needle, ScoreEntry *out, int max);
/* The best score of `player` in the table `key`, or -1 when that player has none there. */
int lb_best_of(const Leaderboard *lb, const char *key, const char *player);
/* 1-based place of `player` in the table `key` (the same order lb_top_best gives), 0 when that player has no
 * row there. */
int lb_rank_of(const Leaderboard *lb, const char *key, const char *player);

/* Case-insensitive substring search over player names. Returns match count; indices go to out_idx. */
int lb_find_player(const Leaderboard *lb, const char *needle, int *out_idx, int max);
/* Exact (case-insensitive) name lookup. Requires lb_sort(lb, LB_SORT_NAME_ASC) first. -1 if absent. */
int lb_binary_search_name(const Leaderboard *lb, const char *name);

#endif
