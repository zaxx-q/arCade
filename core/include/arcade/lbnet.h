/* Online leaderboard: everything that does NOT need a network. What is sent, what is received, what is
 * remembered, and when to try again. The HTTPS request itself is done by an `LbnTransport` that the app
 * provides; the tests provide a scripted fake, so all of this runs in unit tests without a server.
 * Demonstrates: structs, arrays, file I/O, string parsing and validation, function pointers, a state machine.
 *
 * WHY IT IS BUILT THIS WAY
 *  - The game never waits for the server. A finished run is written to the OUTBOX (a text file) first and
 * sent later, one request at a time, from the hub's frame loop (lbn_update never blocks).
 *  - The server can be unreachable for a long time (no internet, outage). Nothing is lost: the outbox stays
 * on disk, requests are retried with a growing delay, and the last tables received are kept in a CACHE file
 * so the Online tab still has something to show.
 *  - The server keeps only the BEST score of each player per table, so the outbox keeps only the best unsent
 *    score per (table, player), and sending the same score twice is harmless.
 *  - Nothing here is a defence against cheating. The files are plain text and the source is public: whatever
 *    the client sends, the SERVER must check it. The token only says "the same install as last time".
 *
 * FILES (next to scores.txt; the app decides the names, an empty path means "do not store")
 *  outbox.txt       unsent runs, one per line:  table|player|score|stamp   (the scores.txt line format)
 *  online_cache.txt last tables received:  "@table|fetched|players" then "rank|player|score|stamp" per row
 *  identity.txt     the install token the server gave out (one line)
 *
 * WIRE FORMAT: plain text with '|' (no JSON). Every request except register carries the header
 * "Authorization: Bearer <token>". A table is named as in leaderboard.h: "echo_maze" or
 * "beat_dodge:Dodge / Bopeebo / hard".
 *   POST /v1/register                -> 200 "ok|<token>"                    token: 1..63 of A-Z a-z 0-9 - _
 *   POST /v1/scores                  -> 200 "ok|<rank>|<players>"
 *        body: "table|player|score\n"   rank = place of that player's BEST score, players = how many players
 *   GET  /v1/top?board=<table, %-encoded>&limit=<n>
 *                                    -> 200 "ok|<players>\n" then one "rank|player|score|stamp\n" per row
 * An answer with a status of 400 or more means "no": the body is "err|<reason>". Meaning of the status:
 *   no answer, 408, 429, 5xx, 3xx  out of reach or busy: keep everything, retry later
 *   401                            the token is unknown: forget it and register again (after the usual delay)
 *   any other 4xx                  refused for good: a score is dropped from the outbox, never retried
 * A 200 whose body does not parse (a captive portal's login page, say) also counts as "out of reach".
 */
#ifndef ARCADE_LBNET_H
#define ARCADE_LBNET_H

#include <stdbool.h>
#include <stddef.h>

#include "arcade/leaderboard.h"

#define LBN_OUTBOX_MAX 50 /* unsent scores kept; when full the oldest one is dropped */
#define LBN_TOP_MAX 50    /* rows of one online table */
#define LBN_CACHE_TABLES 12
#define LBN_TOKEN_MAX 64 /* with the final '\0' */
#define LBN_BODY_MAX 4096
#define LBN_PATH_MAX 320
#define LBN_FRESH_SECS 30    /* a cached table younger than this is not asked for again */
#define LBN_BACKOFF_FIRST 30 /* seconds to wait after the first failure ... */
#define LBN_BACKOFF_MAX 900  /* ... doubling each time, never more than this */

/* ---------------------------------------------------------------- outbox */
typedef struct Outbox {
    ScoreEntry items[LBN_OUTBOX_MAX]; /* oldest first; the entry's `game` is the table */
    int count;
} Outbox;

void lbn_outbox_init(Outbox *ob);
/* Queues a score (name sanitised). One entry per (table, player, case-insensitive): a better score replaces
 * the queued one, an equal or worse one is ignored. Returns true when the outbox changed. */
bool lbn_outbox_add(Outbox *ob, const char *table, const char *player, int score, long long stamp);
/* The server has accepted `sent_score`: removes that entry, unless a BETTER score for the same (table,
 * player) was queued in the meantime (that one still has to be sent). Returns true when an entry was removed.
 */
bool lbn_outbox_ack(Outbox *ob, const char *table, const char *player, int sent_score);
/* Like lb_load/lb_save: -1 when the file is missing, bad lines are skipped, save goes through a temp file. */
int lbn_outbox_load(Outbox *ob, const char *path);
bool lbn_outbox_save(const Outbox *ob, const char *path);

/* ---------------------------------------------------------------- tables received from the server */
typedef struct OnlineRow {
    int rank;
    char player[LB_NAME_LEN];
    int score;
    long long stamp; /* server time when this score was set */
} OnlineRow;

typedef struct OnlineTable {
    char key[LB_GAME_LEN];
    long long fetched; /* client time when it was received */
    int players;       /* players on the whole table at the server (rows holds only the first `count`) */
    int count;
    OnlineRow rows[LBN_TOP_MAX];
} OnlineTable;

typedef struct OnlineCache {
    OnlineTable tables[LBN_CACHE_TABLES];
    int count;
} OnlineCache;

void lbn_cache_init(OnlineCache *c);
/* Stores a table, replacing the one with the same key; when the cache is full the table fetched longest ago
 * goes. */
void lbn_cache_put(OnlineCache *c, const OnlineTable *t);
const OnlineTable *lbn_cache_get(const OnlineCache *c, const char *key); /* NULL when not cached */
int lbn_cache_load(OnlineCache *c, const char *path);
bool lbn_cache_save(const OnlineCache *c, const char *path);

/* ---------------------------------------------------------------- requests and answers (pure text) */
/* Percent-encodes src (RFC 3986: letters, digits and - _ . ~ stay). false when dst is too small. */
bool lbn_url_encode(char *dst, size_t size, const char *src);
/* "/v1/top?board=<encoded table>&limit=<n>" with n limited to 1..LBN_TOP_MAX. false when it does not fit. */
bool lbn_build_top_path(char *dst, size_t size, const char *table, int limit);
/* "table|player|score\n". false when it does not fit. */
bool lbn_build_submit_body(char *dst, size_t size, const ScoreEntry *e);
/* A token is 1..LBN_TOKEN_MAX-1 characters of A-Z a-z 0-9 - _ : it goes into an HTTP header, so nothing else.
 */
bool lbn_valid_token(const char *s);
/* May the app talk to this server? "https://host[:port]" with nothing after it (no path, no user:password@,
 * no query), or "http://localhost[:port]" / "http://127.0.0.1[:port]" for a server on the same computer.
 * Anything else, above all plain http to a real server, is refused: the token and the scores would travel in
 * the clear. */
bool lbn_valid_base_url(const char *url);
/* Each returns false unless the body is exactly what the format above says; nothing is written then (for
 * parse_top: out may be half filled, throw it away). Names from the server are sanitised: it is not trusted.
 */
bool lbn_parse_register(const char *body, char *token, size_t size);
bool lbn_parse_submit(const char *body, int *rank, int *players);
/* Fills players, count and rows; key and fetched are the caller's job. Bad rows are skipped, extra rows
 * beyond LBN_TOP_MAX ignored. */
bool lbn_parse_top(const char *body, OnlineTable *out);

/* ---------------------------------------------------------------- the client: what to send, when */
/* One finished HTTP request. status 0 = no answer at all (offline, timeout, TLS error). */
typedef struct LbnReply {
    int status;
    char body[LBN_BODY_MAX]; /* always '\0'-terminated */
} LbnReply;

/* The app's side of the network (libcurl in the real game, a scripted fake in tests).
 *  start: begins ONE request and returns at once; false = could not even begin. method is "GET" or "POST";
 *         path starts with '/'; token is NULL for register; body is "" for GET. The strings are only valid
 *         during the call: copy them.
 *  poll:  called once per frame while a request runs. false = still running. true = finished, and *reply is
 *         filled in. It must give up by itself after a few seconds (status 0): the client has no timer. */
typedef struct LbnTransport {
    bool (*start)(void *ctx, const char *method, const char *path, const char *token, const char *body);
    bool (*poll)(void *ctx, LbnReply *reply);
    void *ctx;
} LbnTransport;

typedef enum LbnJob { LBN_JOB_NONE, LBN_JOB_REGISTER, LBN_JOB_SUBMIT, LBN_JOB_TOP } LbnJob;
/* What the Online tab shows: DISABLED = the player did not opt in, SYNCING = a request is running,
 * OFFLINE = the last request failed (waiting to retry), ONLINE = everything else. */
typedef enum LbnStatus { LBN_DISABLED, LBN_ONLINE, LBN_SYNCING, LBN_OFFLINE } LbnStatus;

/* The answer to the last accepted score, for the result screen ("#3 worldwide"). */
typedef struct LbnAck {
    bool valid;
    char table[LB_GAME_LEN];
    char player[LB_NAME_LEN];
    int score;
    int rank;
    int players;
} LbnAck;

typedef struct LbnClient {
    LbnTransport transport;
    bool enabled;
    char token[LBN_TOKEN_MAX]; /* "" = not registered yet */
    Outbox outbox;
    OnlineCache cache;
    char outbox_path[256], cache_path[256], token_path[256]; /* "" = do not store */
    bool storage_error; /* a file could not be written (the app may warn: the data lives in memory only) */
    LbnJob job;         /* the request that is running, if any */
    ScoreEntry sending; /* a copy of the outbox entry being sent */
    char requesting[LB_GAME_LEN]; /* the table being fetched */
    char wanted[LB_GAME_LEN];     /* a table the screen asked for ("" = none) */
    int failures;                 /* failed requests in a row */
    long long next_try;           /* no new request before this time (the back-off) */
    int refused;                  /* scores the server refused for good (dropped from the outbox) */
    LbnAck ack;
} LbnClient;

/* Starts disabled and empty. The transport is copied. */
void lbn_init(LbnClient *c, const LbnTransport *transport);
/* Where the three files live ("" or NULL = keep that one in memory only), then load whatever exists. */
void lbn_set_paths(LbnClient *c, const char *outbox, const char *cache, const char *token);
void lbn_load(LbnClient *c);
/* The player's choice. Disabled: nothing is queued, nothing is sent. A client without a transport stays
 * disabled. */
void lbn_enable(LbnClient *c, bool on);
/* The player turned online scores off: forgets every score still waiting to be sent (and stores the empty
 * outbox), so nothing leaves the computer later without the player's say-so. */
void lbn_clear_outbox(LbnClient *c);
/* A run finished: queue it (and store the outbox) and allow a request right now. `now` is unix time in
 * seconds; the core never reads the clock itself. */
void lbn_queue_score(LbnClient *c, const char *table, const char *player, int score, long long stamp,
                     long long now);
/* The screen wants a fresh copy of a table. Does nothing when the cached one is younger than LBN_FRESH_SECS.
 */
void lbn_want_table(LbnClient *c, const char *table, long long now);
/* Like lbn_want_table, but also when the cached copy is fresh (the player pressed "refresh"). */
void lbn_refresh_table(LbnClient *c, const char *table, long long now);
/* Allow a request right now instead of waiting for the back-off (a screen was opened, a key was pressed). */
void lbn_kick(LbnClient *c, long long now);
/* Once per frame from the hub. Never blocks. Order of work: register, send queued scores, fetch the wanted
 * table; at most one request runs at a time. */
void lbn_update(LbnClient *c, long long now);
LbnStatus lbn_status(const LbnClient *c);
/* The cached copy of a table, NULL if there is none. */
const OnlineTable *lbn_table(const LbnClient *c, const char *table);

#endif
