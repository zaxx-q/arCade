#include "arcade/leaderboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

void lb_init(Leaderboard *lb) {
    lb->count = 0;
}

/* Names are stored in a '|' separated text file: strip separators and control characters. */
void lb_sanitize(char *dst, size_t size, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < size; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[j++] = (c < 32 || c == '|' || c == 127) ? '_' : (char)c;
    }
    dst[j] = '\0';
}

/* ---- the order of rows ---- */
/* Does x rank above y? A higher score wins; on a tie the earlier run wins (stamp 0 = unknown counts as
 * earliest). */
static bool outranks(const ScoreEntry *x, const ScoreEntry *y) {
    if (x->score != y->score)
        return x->score > y->score;
    return x->stamp < y->stamp;
}

/* Strict total order for picking a row among equals: outranks(), then the lower position in the file. */
static bool beats(const ScoreEntry *x, int xi, const ScoreEntry *y, int yi) {
    if (outranks(x, y))
        return true;
    if (outranks(y, x))
        return false;
    return xi < yi;
}

static bool same_player(const ScoreEntry *a, const ScoreEntry *b) {
    return strcmp(a->game, b->game) == 0 && arc_stricmp(a->player, b->player) == 0;
}

/* ---- making room (used by lb_add) ----
 * Row i of 0..lb->count: the stored rows, plus (index lb->count) `extra`, a row that is not stored yet. */
static const ScoreEntry *row_at(const Leaderboard *lb, const ScoreEntry *extra, int i) {
    return i < lb->count ? &lb->items[i] : extra;
}

/* Has the same player a better row in the same table? Then row i is only history. */
static bool is_history(const Leaderboard *lb, const ScoreEntry *extra, int total, int i) {
    const ScoreEntry *a = row_at(lb, extra, i);
    for (int j = 0; j < total; j++)
        if (j != i && same_player(row_at(lb, extra, j), a) && beats(row_at(lb, extra, j), j, a, i))
            return true;
    return false;
}

/* The row of table `key` that matters least: a history row if there is one, otherwise the lowest score; the
 * worse of two rows of the same kind is the one the other beats. Index lb->count means `extra` itself. */
static int pick_victim(const Leaderboard *lb, const char *key, const ScoreEntry *extra) {
    int total = lb->count + (extra != NULL ? 1 : 0);
    int victim = -1;
    bool victim_is_history = false;
    for (int i = 0; i < total; i++) {
        const ScoreEntry *a = row_at(lb, extra, i);
        if (strcmp(a->game, key) != 0)
            continue;
        bool history = is_history(lb, extra, total, i);
        if (victim < 0 || (history && !victim_is_history) ||
            (history == victim_is_history && beats(row_at(lb, extra, victim), victim, a, i))) {
            victim = i;
            victim_is_history = history;
        }
    }
    return victim;
}

static int rows_in_table(const Leaderboard *lb, const char *key) {
    int n = 0;
    for (int i = 0; i < lb->count; i++)
        n += strcmp(lb->items[i].game, key) == 0;
    return n;
}

/* The key of the table with the most rows (the first one on a tie). The file must not be empty. */
static const char *fullest_table(const Leaderboard *lb) {
    const char *best = lb->items[0].game;
    int best_n = 0;
    for (int i = 0; i < lb->count; i++) {
        int n = rows_in_table(lb, lb->items[i].game);
        if (n > best_n) {
            best_n = n;
            best = lb->items[i].game;
        }
    }
    return best;
}

bool lb_add(Leaderboard *lb, const char *game, const char *player, int score, long long stamp) {
    ScoreEntry e;
    memset(&e, 0, sizeof(e));
    lb_sanitize(e.game, sizeof(e.game), game);
    lb_sanitize(e.player, sizeof(e.player), (player && player[0]) ? player : "PLAYER");
    e.score = score;
    e.stamp = stamp;

    bool table_full = rows_in_table(lb, e.game) >= LB_MAX_PER_BOARD;
    if (!table_full && lb->count < LB_MAX_ENTRIES) {
        lb->items[lb->count++] = e;
        return true;
    }
    /* Somebody has to go. A full table makes room for its own new row; a full file takes it from the fullest
     * table, so a table with few rows (and a small score scale) is not the one that pays. */
    int victim;
    if (table_full) {
        victim = pick_victim(lb, e.game, &e);
        if (victim == lb->count) /* the new row is the one that matters least: do not store it */
            return false;
    } else {
        victim = pick_victim(lb, fullest_table(lb), NULL);
    }
    lb->items[victim] = e;
    return true;
}

/* Parses "game|player|score|stamp". Returns false for malformed lines. */
bool lb_parse_line(char *line, ScoreEntry *out) {
    char *f[4];
    int n = 0;
    f[n++] = line;
    for (char *p = line; *p && n < 4; p++)
        if (*p == '|') {
            *p = '\0';
            f[n++] = p + 1;
        }
    if (n != 4)
        return false;
    char *end = NULL;
    long score = strtol(f[2], &end, 10);
    if (end == f[2] || *end != '\0')
        return false;
    long long stamp = strtoll(f[3], &end, 10);
    if (end == f[3] || *end != '\0')
        return false;
    if (f[0][0] == '\0' || f[1][0] == '\0')
        return false;
    memset(out, 0, sizeof(*out));
    arc_strlcpy(out->game, f[0], sizeof(out->game));
    arc_strlcpy(out->player, f[1], sizeof(out->player));
    out->score = (int)score;
    out->stamp = stamp;
    return true;
}

int lb_load(Leaderboard *lb, const char *path) {
    lb_init(lb);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    char line[256];
    while (fgets(line, sizeof(line), fp) && lb->count < LB_MAX_ENTRIES) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        ScoreEntry e;
        if (lb_parse_line(line, &e))
            lb->items[lb->count++] = e;
    }
    fclose(fp);
    return lb->count;
}

bool lb_save(const Leaderboard *lb, const char *path) {
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *fp = fopen(tmp, "w");
    if (!fp)
        return false;
    for (int i = 0; i < lb->count; i++) {
        const ScoreEntry *e = &lb->items[i];
        fprintf(fp, "%s|%s|%d|%lld\n", e->game, e->player, e->score, e->stamp);
    }
    if (fclose(fp) != 0)
        return false;
    remove(path); /* Windows rename() fails if the target exists */
    return rename(tmp, path) == 0;
}

/* true when a should be placed BEFORE b for the given order */
static bool before(const ScoreEntry *a, const ScoreEntry *b, LbSort order) {
    switch (order) {
    case LB_SORT_SCORE_DESC:
        return a->score > b->score;
    case LB_SORT_NAME_ASC:
        return arc_stricmp(a->player, b->player) < 0;
    case LB_SORT_RECENT_FIRST:
        return a->stamp > b->stamp;
    }
    return false;
}

static void insertion_sort(ScoreEntry *a, int n, LbSort order) {
    for (int i = 1; i < n; i++) {
        ScoreEntry key = a[i];
        int j = i - 1;
        while (j >= 0 && before(&key, &a[j], order)) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

void lb_sort(Leaderboard *lb, LbSort order) {
    insertion_sort(lb->items, lb->count, order);
}

/* ---- boards ---- */
void lb_board_key(char *out, size_t size, const char *game, const char *board) {
    char raw[LB_GAME_LEN + 64]; /* long enough for any board: lb_sanitize() below cuts it to `size` anyway */
    if (board != NULL && board[0] != '\0')
        snprintf(raw, sizeof(raw), "%s:%s", game, board);
    else
        arc_strlcpy(raw, game, sizeof(raw));
    lb_sanitize(out, size, raw);
}

const char *lb_key_board(const char *key) {
    const char *colon = strchr(key, ':'); /* game ids never contain ':', so the first one ends the id */
    return colon != NULL ? colon + 1 : "";
}

bool lb_key_in_game(const char *key, const char *game) {
    size_t n = strlen(game);
    /* the id must end right there: "beat_dodge2" is another game, "beat_dodge:Dodge..." is a board */
    return strncmp(key, game, n) == 0 && (key[n] == '\0' || key[n] == ':');
}

int lb_boards_for_game(const Leaderboard *lb, const char *game, char keys[][LB_GAME_LEN], int max) {
    int n = 0;
    for (int i = 0; i < lb->count && n < max; i++) {
        if (!lb_key_in_game(lb->items[i].game, game))
            continue;
        bool known = false; /* linear search: has this table been listed already? */
        for (int k = 0; k < n && !known; k++)
            known = strcmp(keys[k], lb->items[i].game) == 0;
        if (!known)
            arc_strlcpy(keys[n++], lb->items[i].game, LB_GAME_LEN);
    }
    for (int i = 1; i < n; i++) { /* insertion sort A-Z */
        char key[LB_GAME_LEN];
        arc_strlcpy(key, keys[i], sizeof(key));
        int j = i - 1;
        while (j >= 0 && arc_stricmp(key, keys[j]) < 0) {
            arc_strlcpy(keys[j + 1], keys[j], LB_GAME_LEN);
            j--;
        }
        arc_strlcpy(keys[j + 1], key, LB_GAME_LEN);
    }
    return n;
}

static bool in_scope(const ScoreEntry *e, LbScope scope, const char *key) {
    switch (scope) {
    case LB_SCOPE_TABLE:
        return strcmp(e->game, key) == 0;
    case LB_SCOPE_GAME:
        return lb_key_in_game(e->game, key);
    case LB_SCOPE_ALL:
        return true;
    }
    return false;
}

/* The best `max` matching rows, kept sorted in out[] while the file is read once (insertion into a short
 * sorted list: no copy of the whole file, so nothing big on the stack). With best_only, a player who is
 * already in out[] only moves up when this row is better than the one stored: one row per (table, player). */
static int collect_top(const Leaderboard *lb, LbScope scope, const char *key, const char *needle,
                       bool best_only, ScoreEntry *out, int max) {
    int n = 0;
    for (int i = 0; i < lb->count && max > 0; i++) {
        const ScoreEntry *e = &lb->items[i];
        if (!in_scope(e, scope, key) || (needle != NULL && !arc_contains_nocase(e->player, needle)))
            continue;
        if (best_only) {
            int j = 0;
            while (j < n && !same_player(&out[j], e))
                j++;
            if (j < n) {
                if (!outranks(e, &out[j]))
                    continue;
                for (int k = j; k + 1 < n; k++) /* the old row of this player leaves the list */
                    out[k] = out[k + 1];
                n--;
            }
        }
        if (n == max && !outranks(e, &out[n - 1]))
            continue;                    /* list full and this row is not good enough */
        int pos = n < max ? n : max - 1; /* when full, the last row is overwritten (it drops out) */
        while (pos > 0 && outranks(e, &out[pos - 1])) {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = *e;
        if (n < max)
            n++;
    }
    return n;
}

int lb_top_for_game(const Leaderboard *lb, const char *game, ScoreEntry *out, int max) {
    return collect_top(lb, LB_SCOPE_TABLE, game, NULL, false, out, max);
}

int lb_top_for_all_boards(const Leaderboard *lb, const char *game, ScoreEntry *out, int max) {
    return collect_top(lb, LB_SCOPE_GAME, game, NULL, false, out, max);
}

int lb_top_best(const Leaderboard *lb, LbScope scope, const char *key, ScoreEntry *out, int max) {
    return collect_top(lb, scope, key, NULL, true, out, max);
}

int lb_search_best(const Leaderboard *lb, const char *needle, ScoreEntry *out, int max) {
    return collect_top(lb, LB_SCOPE_ALL, NULL, needle, true, out, max);
}

/* The name as lb_add stored it, so a lookup finds what was saved. */
static void stored_name(char *dst, size_t size, const char *player) {
    lb_sanitize(dst, size, (player && player[0]) ? player : "PLAYER");
}

/* Index of the best row of `who` in table `key`, or -1. */
static int best_row_index(const Leaderboard *lb, const char *key, const char *who) {
    int best = -1;
    for (int i = 0; i < lb->count; i++) {
        const ScoreEntry *e = &lb->items[i];
        if (strcmp(e->game, key) == 0 && arc_stricmp(e->player, who) == 0 &&
            (best < 0 || beats(e, i, &lb->items[best], best)))
            best = i;
    }
    return best;
}

int lb_best_of(const Leaderboard *lb, const char *key, const char *player) {
    char who[LB_NAME_LEN];
    stored_name(who, sizeof(who), player);
    int i = best_row_index(lb, key, who);
    return i < 0 ? -1 : lb->items[i].score;
}

int lb_rank_of(const Leaderboard *lb, const char *key, const char *player) {
    char who[LB_NAME_LEN];
    stored_name(who, sizeof(who), player);
    int mine = best_row_index(lb, key, who);
    if (mine < 0)
        return 0;
    int rank = 1;
    for (int i = 0; i < lb->count; i++) {
        const ScoreEntry *e = &lb->items[i];
        if (strcmp(e->game, key) != 0 || arc_stricmp(e->player, who) == 0)
            continue;
        /* count another player once: through their best row, and only if it beats mine */
        if (beats(e, i, &lb->items[mine], mine) && best_row_index(lb, key, e->player) == i)
            rank++;
    }
    return rank;
}

int lb_find_player(const Leaderboard *lb, const char *needle, int *out_idx, int max) {
    int n = 0;
    for (int i = 0; i < lb->count && n < max; i++)
        if (arc_contains_nocase(lb->items[i].player, needle))
            out_idx[n++] = i;
    return n;
}

int lb_binary_search_name(const Leaderboard *lb, const char *name) {
    int lo = 0, hi = lb->count - 1, found = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int c = arc_stricmp(lb->items[mid].player, name);
        if (c == 0) {
            found = mid;
            hi = mid - 1; /* keep going left: return the FIRST match */
        } else if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return found;
}
