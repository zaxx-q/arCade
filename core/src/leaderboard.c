#include "arcade/leaderboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

void lb_init(Leaderboard *lb) {
    lb->count = 0;
}

/* Names are stored in a '|' separated text file: strip separators and control characters. */
static void sanitize(char *dst, size_t size, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < size; i++) {
        unsigned char c = (unsigned char)src[i];
        dst[j++] = (c < 32 || c == '|' || c == 127) ? '_' : (char)c;
    }
    dst[j] = '\0';
}

bool lb_add(Leaderboard *lb, const char *game, const char *player, int score, long long stamp) {
    ScoreEntry e;
    memset(&e, 0, sizeof(e));
    sanitize(e.game, sizeof(e.game), game);
    sanitize(e.player, sizeof(e.player), (player && player[0]) ? player : "PLAYER");
    e.score = score;
    e.stamp = stamp;

    if (lb->count < LB_MAX_ENTRIES) {
        lb->items[lb->count++] = e;
        return true;
    }
    int lowest = 0;
    for (int i = 1; i < lb->count; i++)
        if (lb->items[i].score < lb->items[lowest].score)
            lowest = i;
    if (score > lb->items[lowest].score) {
        lb->items[lowest] = e;
        return true;
    }
    return false;
}

/* Parses "game|player|score|stamp". Returns false for malformed lines. */
static bool parse_line(char *line, ScoreEntry *out) {
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
        if (parse_line(line, &e))
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

int lb_top_for_game(const Leaderboard *lb, const char *game, ScoreEntry *out, int max) {
    int n = 0;
    for (int i = 0; i < lb->count && n < max; i++)
        if (strcmp(lb->items[i].game, game) == 0)
            out[n++] = lb->items[i];
    /* the copy above stops at max, so gather ALL matches first when more exist */
    if (n == max) {
        ScoreEntry all[LB_MAX_ENTRIES];
        int m = 0;
        for (int i = 0; i < lb->count; i++)
            if (strcmp(lb->items[i].game, game) == 0)
                all[m++] = lb->items[i];
        insertion_sort(all, m, LB_SORT_SCORE_DESC);
        n = m < max ? m : max;
        memcpy(out, all, (size_t)n * sizeof(ScoreEntry));
        return n;
    }
    insertion_sort(out, n, LB_SORT_SCORE_DESC);
    return n;
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
