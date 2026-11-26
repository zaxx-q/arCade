#include "arcade/lbnet.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade/str.h"

/* ---------------------------------------------------------------- small helpers */
/* Splits `line` at '|' in place. Returns the number of fields (at most `max`; the last one keeps any further
 * '|'). */
static int split_fields(char *line, char **fields, int max) {
    int n = 0;
    fields[n++] = line;
    for (char *p = line; *p != '\0' && n < max; p++)
        if (*p == '|') {
            *p = '\0';
            fields[n++] = p + 1;
        }
    return n;
}

/* A whole number in the range of long long / int; false for empty text, letters, or a value too big. */
static bool parse_ll(const char *s, long long *out) {
    char *end = NULL;
    if (*s == '\0')
        return false;
    long long v = strtoll(s, &end, 10);
    if (*end != '\0')
        return false;
    *out = v;
    return true;
}

static bool parse_int(const char *s, int *out) {
    long long v = 0;
    if (!parse_ll(s, &v) || v < INT_MIN || v > INT_MAX)
        return false;
    *out = (int)v;
    return true;
}

static void chomp(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* Copies the next line of *text into `line` (cut at size - 1, without "\r\n") and moves *text past it.
 * false at the end of the text. */
static bool next_line(const char **text, char *line, size_t size) {
    const char *p = *text;
    if (*p == '\0')
        return false;
    size_t n = 0;
    while (*p != '\0' && *p != '\n') {
        if (n + 1 < size)
            line[n++] = *p;
        p++;
    }
    if (*p == '\n')
        p++;
    while (n > 0 && line[n - 1] == '\r')
        n--;
    line[n] = '\0';
    *text = p;
    return true;
}

/* Files are written to "<path>.tmp" first and renamed, so a crash never leaves half a file (same as lb_save).
 */
static FILE *open_tmp(const char *path, char *tmp, size_t size) {
    snprintf(tmp, size, "%s.tmp", path);
    return fopen(tmp, "w");
}

static bool commit_tmp(FILE *fp, const char *tmp, const char *path) {
    if (fclose(fp) != 0)
        return false;
    remove(path); /* Windows rename() fails if the target exists */
    return rename(tmp, path) == 0;
}

/* ---------------------------------------------------------------- outbox */
void lbn_outbox_init(Outbox *ob) {
    ob->count = 0;
}

static int outbox_find(const Outbox *ob, const char *table, const char *player) {
    for (int i = 0; i < ob->count; i++)
        if (strcmp(ob->items[i].game, table) == 0 && arc_stricmp(ob->items[i].player, player) == 0)
            return i;
    return -1;
}

static void outbox_remove(Outbox *ob, int index) {
    for (int i = index; i + 1 < ob->count; i++)
        ob->items[i] = ob->items[i + 1];
    ob->count--;
}

bool lbn_outbox_add(Outbox *ob, const char *table, const char *player, int score, long long stamp) {
    ScoreEntry e;
    memset(&e, 0, sizeof(e));
    lb_sanitize(e.game, sizeof(e.game), table);
    lb_sanitize(e.player, sizeof(e.player), (player && player[0]) ? player : "PLAYER");
    e.score = score;
    e.stamp = stamp;
    int i = outbox_find(ob, e.game, e.player);
    if (i >= 0) { /* the server keeps only the best score: queueing a worse one would be wasted traffic */
        if (score <= ob->items[i].score)
            return false;
        ob->items[i] = e;
        return true;
    }
    if (ob->count == LBN_OUTBOX_MAX)
        outbox_remove(ob, 0); /* the oldest goes */
    ob->items[ob->count++] = e;
    return true;
}

bool lbn_outbox_ack(Outbox *ob, const char *table, const char *player, int sent_score) {
    int i = outbox_find(ob, table, player);
    if (i < 0 || ob->items[i].score > sent_score)
        return false; /* a better score was queued while this one was on its way: it still has to go */
    outbox_remove(ob, i);
    return true;
}

int lbn_outbox_load(Outbox *ob, const char *path) {
    lbn_outbox_init(ob);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        chomp(line);
        ScoreEntry e;
        if (lb_parse_line(line,
                          &e)) /* through add(): the one-per-player rule holds even for an edited file */
            lbn_outbox_add(ob, e.game, e.player, e.score, e.stamp);
    }
    fclose(fp);
    return ob->count;
}

bool lbn_outbox_save(const Outbox *ob, const char *path) {
    char tmp[300];
    FILE *fp = open_tmp(path, tmp, sizeof(tmp));
    if (!fp)
        return false;
    for (int i = 0; i < ob->count; i++)
        fprintf(fp, "%s|%s|%d|%lld\n", ob->items[i].game, ob->items[i].player, ob->items[i].score,
                ob->items[i].stamp);
    return commit_tmp(fp, tmp, path);
}

/* ---------------------------------------------------------------- requests and answers */
bool lbn_url_encode(char *dst, size_t size, const char *src) {
    static const char hex[] = "0123456789ABCDEF";
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0'; i++) {
        unsigned char c = (unsigned char)src[i];
        bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                     c == '_' || c == '.' || c == '~';
        size_t need = plain ? 1 : 3;
        if (j + need + 1 > size) {
            if (size > 0)
                dst[0] = '\0';
            return false;
        }
        if (plain) {
            dst[j++] = (char)c;
        } else {
            dst[j++] = '%';
            dst[j++] = hex[c >> 4];
            dst[j++] = hex[c & 15];
        }
    }
    if (size == 0)
        return false;
    dst[j] = '\0';
    return true;
}

bool lbn_build_top_path(char *dst, size_t size, const char *table, int limit) {
    char enc[LB_GAME_LEN * 3];
    if (!lbn_url_encode(enc, sizeof(enc), table))
        return false;
    if (limit < 1)
        limit = 1;
    if (limit > LBN_TOP_MAX)
        limit = LBN_TOP_MAX;
    int n = snprintf(dst, size, "/v1/top?board=%s&limit=%d", enc, limit);
    return n > 0 && (size_t)n < size;
}

bool lbn_build_submit_body(char *dst, size_t size, const ScoreEntry *e) {
    int n = snprintf(dst, size, "%s|%s|%d\n", e->game, e->player, e->score);
    return n > 0 && (size_t)n < size;
}

bool lbn_valid_token(const char *s) {
    size_t n = strlen(s);
    if (n == 0 || n >= LBN_TOKEN_MAX)
        return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                  c == '_';
        if (!ok)
            return false;
    }
    return true;
}

bool lbn_valid_base_url(const char *url) {
    const char *host;
    bool tls;
    if (strlen(url) == 0 || strlen(url) > 200)
        return false;
    if (strncmp(url, "https://", 8) == 0) {
        tls = true;
        host = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        tls = false;
        host = url + 7;
    } else {
        return false;
    }
    size_t n = 0; /* length of the host name */
    for (; host[n] != '\0' && host[n] != ':'; n++) {
        char c = host[n];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
                  c == '.';
        if (!ok) /* '/', '@', '?', '#', spaces ...: a path, a login or a query in a base address */
            return false;
    }
    if (n == 0 || host[0] == '.' || host[n - 1] == '.')
        return false;
    if (host[n] == ':') { /* a port: 1..65535 and nothing after it */
        long port = 0;
        size_t digits = 0;
        for (const char *p = host + n + 1; *p != '\0'; p++, digits++) {
            if (*p < '0' || *p > '9' || digits >= 5)
                return false;
            port = port * 10 + (*p - '0');
        }
        if (digits == 0 || port < 1 || port > 65535)
            return false;
    }
    if (!tls) /* plain http only to this computer; compared with the exact name, "localhost.evil.com" is no
                 match */
        return (n == 9 && strncmp(host, "localhost", 9) == 0) ||
               (n == 9 && strncmp(host, "127.0.0.1", 9) == 0);
    return true;
}

bool lbn_parse_register(const char *body, char *token, size_t size) {
    /* the buffer is a little longer than "ok|" + the longest token: a longer line is cut and then fails the
     * length check of lbn_valid_token, it can never pass as a shorter token */
    char line[LBN_TOKEN_MAX + 8];
    char *f[3];
    const char *p = body;
    if (!next_line(&p, line, sizeof(line)))
        return false;
    if (split_fields(line, f, 3) != 2 || strcmp(f[0], "ok") != 0 || !lbn_valid_token(f[1]) ||
        strlen(f[1]) >= size)
        return false;
    arc_strlcpy(token, f[1], size);
    return true;
}

bool lbn_parse_submit(const char *body, int *rank, int *players) {
    char line[64];
    char *f[4];
    const char *p = body;
    int r = 0, n = 0;
    if (!next_line(&p, line, sizeof(line)))
        return false;
    if (split_fields(line, f, 4) != 3 || strcmp(f[0], "ok") != 0 || !parse_int(f[1], &r) ||
        !parse_int(f[2], &n) || r < 1 || n < r)
        return false;
    *rank = r;
    *players = n;
    return true;
}

/* "rank|player|score|stamp" -> row. The server is not trusted: the name is cleaned like a typed one. */
static bool parse_row(char *line, OnlineRow *row) {
    char *f[5];
    long long stamp = 0;
    int rank = 0, score = 0;
    if (split_fields(line, f, 5) != 4 || !parse_int(f[0], &rank) || rank < 1 || f[1][0] == '\0' ||
        !parse_int(f[2], &score) || !parse_ll(f[3], &stamp))
        return false;
    memset(row, 0, sizeof(*row));
    row->rank = rank;
    lb_sanitize(row->player, sizeof(row->player), f[1]);
    row->score = score;
    row->stamp = stamp;
    return true;
}

bool lbn_parse_top(const char *body, OnlineTable *out) {
    char line[160];
    char *f[3];
    const char *p = body;
    int players = 0;
    out->players = 0;
    out->count = 0;
    if (!next_line(&p, line, sizeof(line)))
        return false;
    if (split_fields(line, f, 3) != 2 || strcmp(f[0], "ok") != 0 || !parse_int(f[1], &players) || players < 0)
        return false;
    out->players = players;
    while (out->count < LBN_TOP_MAX && next_line(&p, line, sizeof(line)))
        if (parse_row(line, &out->rows[out->count]))
            out->count++;
    return true;
}

/* ---------------------------------------------------------------- cache */
void lbn_cache_init(OnlineCache *c) {
    c->count = 0;
}

void lbn_cache_put(OnlineCache *c, const OnlineTable *t) {
    int slot = -1;
    for (int i = 0; i < c->count && slot < 0; i++)
        if (strcmp(c->tables[i].key, t->key) == 0)
            slot = i;
    if (slot < 0 && c->count < LBN_CACHE_TABLES)
        slot = c->count++;
    if (slot < 0) { /* full: replace the table fetched longest ago */
        slot = 0;
        for (int i = 1; i < c->count; i++)
            if (c->tables[i].fetched < c->tables[slot].fetched)
                slot = i;
    }
    c->tables[slot] = *t;
}

const OnlineTable *lbn_cache_get(const OnlineCache *c, const char *key) {
    for (int i = 0; i < c->count; i++)
        if (strcmp(c->tables[i].key, key) == 0)
            return &c->tables[i];
    return NULL;
}

int lbn_cache_load(OnlineCache *c, const char *path) {
    lbn_cache_init(c);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;
    char line[256];
    OnlineTable *cur = NULL; /* the table whose rows are being read; NULL after a bad header: skip its rows */
    while (fgets(line, sizeof(line), fp)) {
        chomp(line);
        if (line[0] == '@') {
            char *f[4];
            long long fetched = 0;
            int players = 0;
            cur = NULL;
            if (c->count < LBN_CACHE_TABLES && split_fields(line + 1, f, 4) == 3 && f[0][0] != '\0' &&
                parse_ll(f[1], &fetched) && parse_int(f[2], &players) && players >= 0) {
                cur = &c->tables[c->count++];
                memset(cur, 0, sizeof(*cur));
                lb_sanitize(cur->key, sizeof(cur->key), f[0]);
                cur->fetched = fetched;
                cur->players = players;
            }
        } else if (cur != NULL && cur->count < LBN_TOP_MAX && parse_row(line, &cur->rows[cur->count])) {
            cur->count++;
        }
    }
    fclose(fp);
    return c->count;
}

bool lbn_cache_save(const OnlineCache *c, const char *path) {
    char tmp[300];
    FILE *fp = open_tmp(path, tmp, sizeof(tmp));
    if (!fp)
        return false;
    for (int i = 0; i < c->count; i++) {
        const OnlineTable *t = &c->tables[i];
        fprintf(fp, "@%s|%lld|%d\n", t->key, t->fetched, t->players);
        for (int k = 0; k < t->count; k++)
            fprintf(fp, "%d|%s|%d|%lld\n", t->rows[k].rank, t->rows[k].player, t->rows[k].score,
                    t->rows[k].stamp);
    }
    return commit_tmp(fp, tmp, path);
}

/* ---------------------------------------------------------------- the client */
static void save_outbox(LbnClient *c) {
    if (c->outbox_path[0] != '\0' && !lbn_outbox_save(&c->outbox, c->outbox_path))
        c->storage_error = true;
}

static void save_cache(LbnClient *c) {
    if (c->cache_path[0] != '\0' && !lbn_cache_save(&c->cache, c->cache_path))
        c->storage_error = true;
}

static void save_token(LbnClient *c) {
    if (c->token_path[0] == '\0')
        return;
    FILE *fp = fopen(c->token_path, "w");
    if (!fp) {
        c->storage_error = true;
        return;
    }
    fprintf(fp, "%s\n", c->token);
    if (fclose(fp) != 0)
        c->storage_error = true;
}

static void load_token(LbnClient *c) {
    FILE *fp = c->token_path[0] != '\0' ? fopen(c->token_path, "r") : NULL;
    if (!fp)
        return;
    char line[LBN_TOKEN_MAX + 8];
    if (fgets(line, sizeof(line), fp) != NULL) {
        chomp(line);
        if (lbn_valid_token(line))
            arc_strlcpy(c->token, line, sizeof(c->token));
    }
    fclose(fp);
}

void lbn_init(LbnClient *c, const LbnTransport *transport) {
    memset(c, 0, sizeof(*c));
    if (transport != NULL)
        c->transport = *transport;
}

static void copy_path(char *dst, size_t size, const char *src) {
    arc_strlcpy(dst, src != NULL ? src : "", size);
}

void lbn_set_paths(LbnClient *c, const char *outbox, const char *cache, const char *token) {
    copy_path(c->outbox_path, sizeof(c->outbox_path), outbox);
    copy_path(c->cache_path, sizeof(c->cache_path), cache);
    copy_path(c->token_path, sizeof(c->token_path), token);
}

void lbn_load(LbnClient *c) {
    if (c->outbox_path[0] != '\0')
        lbn_outbox_load(&c->outbox, c->outbox_path);
    if (c->cache_path[0] != '\0')
        lbn_cache_load(&c->cache, c->cache_path);
    load_token(c);
}

void lbn_enable(LbnClient *c, bool on) {
    c->enabled = on && c->transport.start != NULL && c->transport.poll != NULL;
}

void lbn_kick(LbnClient *c, long long now) {
    c->next_try = now;
}

void lbn_clear_outbox(LbnClient *c) {
    lbn_outbox_init(&c->outbox);
    save_outbox(c);
}

void lbn_queue_score(LbnClient *c, const char *table, const char *player, int score, long long stamp,
                     long long now) {
    if (!c->enabled)
        return;
    if (lbn_outbox_add(&c->outbox, table, player, score, stamp)) {
        save_outbox(c); /* on disk BEFORE anything is sent: a crash or a closed window loses nothing */
        lbn_kick(c, now);
    }
}

static void want_table(LbnClient *c, const char *table, long long now, bool force) {
    if (!c->enabled)
        return;
    char key[LB_GAME_LEN];
    lb_sanitize(key, sizeof(key), table);
    const OnlineTable *t = lbn_cache_get(&c->cache, key);
    long long age = t != NULL ? now - t->fetched : -1;
    if (!force && t != NULL && age >= 0 && age < LBN_FRESH_SECS)
        return;
    arc_strlcpy(c->wanted, key, sizeof(c->wanted));
    lbn_kick(c, now);
}

void lbn_want_table(LbnClient *c, const char *table, long long now) {
    want_table(c, table, now, false);
}

void lbn_refresh_table(LbnClient *c, const char *table, long long now) {
    want_table(c, table, now, true);
}

/* Seconds to wait after `failures` failed requests in a row: 30, 60, 120 ... never more than 15 minutes. */
static long long backoff_seconds(int failures) {
    long long wait = LBN_BACKOFF_FIRST;
    for (int i = 1; i < failures && wait < LBN_BACKOFF_MAX; i++)
        wait *= 2;
    return wait < LBN_BACKOFF_MAX ? wait : LBN_BACKOFF_MAX;
}

static void succeeded(LbnClient *c, long long now) {
    c->failures = 0;
    c->next_try = now; /* more work (the next queued score) may follow at once */
}

static void failed(LbnClient *c, long long now) {
    c->failures++;
    c->next_try = now + backoff_seconds(c->failures);
}

static bool begin(LbnClient *c, LbnJob job, const char *method, const char *path, const char *token,
                  const char *body, long long now) {
    if (!c->transport.start(c->transport.ctx, method, path, token, body)) {
        failed(c, now);
        return false;
    }
    c->job = job;
    return true;
}

static void start_next_job(LbnClient *c, long long now) {
    if (c->token[0] == '\0') { /* first contact: the server hands out this install's token */
        if (c->outbox.count > 0 || c->wanted[0] != '\0')
            begin(c, LBN_JOB_REGISTER, "POST", "/v1/register", NULL, "", now);
        return;
    }
    if (c->outbox.count > 0) {
        char body[LB_GAME_LEN + LB_NAME_LEN + 24];
        c->sending = c->outbox.items[0]; /* a copy: the outbox may change while the request runs */
        if (!lbn_build_submit_body(body, sizeof(body), &c->sending)) { /* cannot happen; never loop on it */
            lbn_outbox_ack(&c->outbox, c->sending.game, c->sending.player, c->sending.score);
            save_outbox(c);
            return;
        }
        begin(c, LBN_JOB_SUBMIT, "POST", "/v1/scores", c->token, body, now);
        return;
    }
    if (c->wanted[0] != '\0') {
        char path[LBN_PATH_MAX];
        arc_strlcpy(c->requesting, c->wanted, sizeof(c->requesting));
        c->wanted[0] = '\0'; /* asked once; the screen asks again when it needs it again */
        if (lbn_build_top_path(path, sizeof(path), c->requesting, LBN_TOP_MAX))
            begin(c, LBN_JOB_TOP, "GET", path, c->token, "", now);
    }
}

static void forget_token(LbnClient *c) {
    c->token[0] = '\0';
    save_token(c);
}

static void finish_register(LbnClient *c, long long now, const LbnReply *r) {
    char token[LBN_TOKEN_MAX];
    if (r->status == 200 && lbn_parse_register(r->body, token, sizeof(token))) {
        arc_strlcpy(c->token, token, sizeof(c->token));
        save_token(c);
        succeeded(c, now);
    } else {
        failed(c, now);
    }
}

static void finish_submit(LbnClient *c, long long now, const LbnReply *r) {
    int rank = 0, players = 0;
    if (r->status == 200 && lbn_parse_submit(r->body, &rank, &players)) {
        c->ack.valid = true;
        arc_strlcpy(c->ack.table, c->sending.game, sizeof(c->ack.table));
        arc_strlcpy(c->ack.player, c->sending.player, sizeof(c->ack.player));
        c->ack.score = c->sending.score;
        c->ack.rank = rank;
        c->ack.players = players;
    } else if (r->status == 200) { /* an answer that is not ours (a login page of a hotspot): not delivered */
        failed(c, now);
        return;
    } else if (r->status == 401) { /* the server does not know this token */
        forget_token(c);
        failed(c, now);
        return;
    } else { /* refused for good (a 4xx): sending it again would only be refused again */
        c->refused++;
    }
    lbn_outbox_ack(&c->outbox, c->sending.game, c->sending.player, c->sending.score);
    save_outbox(c);
    succeeded(c, now);
}

static void finish_top(LbnClient *c, long long now, const LbnReply *r) {
    OnlineTable t; /* ~1.7 KB */
    if (r->status == 200 && lbn_parse_top(r->body, &t)) {
        arc_strlcpy(t.key, c->requesting, sizeof(t.key));
        t.fetched = now;
        lbn_cache_put(&c->cache, &t);
        save_cache(c);
        succeeded(c, now);
    } else if (r->status == 200) {
        failed(c, now);
    } else if (r->status == 401) {
        forget_token(c);
        failed(c, now);
    } else { /* a refused question (unknown table, say): the server is fine, there is just nothing to show */
        succeeded(c, now);
    }
}

static void finish(LbnClient *c, long long now, const LbnReply *r) {
    LbnJob job = c->job;
    c->job = LBN_JOB_NONE;
    /* Out of reach or busy: keep everything and try again later. That includes a 3xx: a hotspot's login page
     * redirects, and a score must never be thrown away because of it. Only a real 4xx refusal (below) is
     * final. */
    bool out_of_reach = r->status == 0 || r->status == 408 || r->status == 429 || r->status >= 500 ||
                        (r->status != 200 && r->status < 400);
    if (out_of_reach) {
        failed(c, now);
        return;
    }
    if (job == LBN_JOB_REGISTER)
        finish_register(c, now, r);
    else if (job == LBN_JOB_SUBMIT)
        finish_submit(c, now, r);
    else if (job == LBN_JOB_TOP)
        finish_top(c, now, r);
}

void lbn_update(LbnClient *c, long long now) {
    if (!c->enabled)
        return;
    if (c->job != LBN_JOB_NONE) {
        LbnReply reply;
        reply.status = 0;
        reply.body[0] = '\0';
        if (!c->transport.poll(c->transport.ctx, &reply))
            return; /* still running: the frame goes on */
        reply.body[LBN_BODY_MAX - 1] = '\0';
        finish(c, now, &reply);
        return;
    }
    if (now >= c->next_try)
        start_next_job(c, now);
}

LbnStatus lbn_status(const LbnClient *c) {
    if (!c->enabled)
        return LBN_DISABLED;
    if (c->job != LBN_JOB_NONE)
        return LBN_SYNCING;
    return c->failures > 0 ? LBN_OFFLINE : LBN_ONLINE;
}

const OnlineTable *lbn_table(const LbnClient *c, const char *table) {
    char key[LB_GAME_LEN];
    lb_sanitize(key, sizeof(key), table);
    return lbn_cache_get(&c->cache, key);
}
