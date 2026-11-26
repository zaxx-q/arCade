#include <stdio.h>
#include <string.h>

#include "arcade/lbnet.h"
#include "arcade_test.h"

#define OUTBOX_FILE "arcade_test_lbnet_outbox.tmp"
#define CACHE_FILE "arcade_test_lbnet_cache.tmp"
#define TOKEN_FILE "arcade_test_lbnet_token.tmp"

#define DODGE_HARD "beat_dodge:Dodge / Bopeebo / hard"

/* ---------------------------------------------------------------- a fake network */
/* Answers are scripted in order, one per request. When the script runs out the "server" does not answer at
 * all (status 0), which is what a server that is down looks like. */
typedef struct Fake {
    struct {
        int status;
        const char *body;
    } script[16];
    int script_len;
    int next;          /* the next scripted answer */
    int pending_polls; /* how many polls say "still running" before an answer comes */
    int polls_left;
    bool refuse_start;
    int started;                                                         /* requests begun so far */
    char method[8], path[LBN_PATH_MAX], token[LBN_TOKEN_MAX], body[256]; /* of the last request */
    bool had_token;
    char paths[16][LBN_PATH_MAX]; /* of every request, in order */
} Fake;

static bool fake_start(void *ctx, const char *method, const char *path, const char *token, const char *body) {
    Fake *f = ctx;
    if (f->refuse_start)
        return false;
    snprintf(f->method, sizeof(f->method), "%s", method);
    snprintf(f->path, sizeof(f->path), "%s", path);
    snprintf(f->token, sizeof(f->token), "%s", token != NULL ? token : "");
    snprintf(f->body, sizeof(f->body), "%s", body);
    f->had_token = token != NULL;
    if (f->started < 16)
        snprintf(f->paths[f->started], LBN_PATH_MAX, "%s", path);
    f->started++;
    f->polls_left = f->pending_polls;
    return true;
}

static bool fake_poll(void *ctx, LbnReply *reply) {
    Fake *f = ctx;
    if (f->polls_left > 0) {
        f->polls_left--;
        return false;
    }
    if (f->next < f->script_len) {
        reply->status = f->script[f->next].status;
        snprintf(reply->body, sizeof(reply->body), "%s", f->script[f->next].body);
        f->next++;
    } else {
        reply->status = 0;
        reply->body[0] = '\0';
    }
    return true;
}

static void say(Fake *f, int status, const char *body) {
    f->script[f->script_len].status = status;
    f->script[f->script_len].body = body;
    f->script_len++;
}

static Fake g_fake;
static LbnClient g_c; /* ~26 KB: static */

/* a client wired to g_fake, enabled, with a token (so tests that are not about registering skip that step) */
static void fresh_client(void) {
    memset(&g_fake, 0, sizeof(g_fake));
    LbnTransport tr = {fake_start, fake_poll, &g_fake};
    lbn_init(&g_c, &tr);
    lbn_enable(&g_c, true);
    snprintf(g_c.token, sizeof(g_c.token), "tok");
}

/* One request takes two updates: one starts it, the next collects the answer. */
static void pump(long long now, int updates) {
    for (int i = 0; i < updates; i++)
        lbn_update(&g_c, now);
}

/* ---------------------------------------------------------------- encoding and parsing */
static void test_url_encode(void) {
    char out[64];
    CHECK(lbn_url_encode(out, sizeof(out), DODGE_HARD));
    CHECK_STR_EQ(out, "beat_dodge%3ADodge%20%2F%20Bopeebo%20%2F%20hard");
    CHECK(lbn_url_encode(out, sizeof(out), "AZaz09-_.~"));
    CHECK_STR_EQ(out, "AZaz09-_.~"); /* the unreserved characters stay as they are */
    CHECK(lbn_url_encode(out, sizeof(out), "100%|\\\xc3\xa9"));
    CHECK_STR_EQ(out, "100%25%7C%5C%C3%A9"); /* '%', '|', '\\' and UTF-8 bytes, upper-case hex */
    CHECK(lbn_url_encode(out, sizeof(out), ""));
    CHECK_STR_EQ(out, "");
    char tiny[5];
    CHECK(lbn_url_encode(tiny, sizeof(tiny), "abcde") == false); /* 5 characters + the '\0' = 6 > 5 */
    CHECK(lbn_url_encode(tiny, sizeof(tiny), "abcd"));           /* 4 + the '\0' = 5: just fits */
    CHECK(lbn_url_encode(tiny, sizeof(tiny), "a "));             /* "a%20" = 4 + the '\0' = 5: just fits */
    CHECK(lbn_url_encode(tiny, sizeof(tiny), "a b") == false);   /* "a%20b" = 5 + the '\0' = 6: does not */
    CHECK(lbn_url_encode(tiny, sizeof(tiny), "ab c") == false);
    CHECK(lbn_url_encode(out, 0, "x") == false);
}

static void test_build_top_path(void) {
    char path[LBN_PATH_MAX];
    CHECK(lbn_build_top_path(path, sizeof(path), "echo_maze", 20));
    CHECK_STR_EQ(path, "/v1/top?board=echo_maze&limit=20");
    CHECK(lbn_build_top_path(path, sizeof(path), DODGE_HARD, 1000)); /* limit is clamped */
    CHECK_STR_EQ(path, "/v1/top?board=beat_dodge%3ADodge%20%2F%20Bopeebo%20%2F%20hard&limit=50");
    CHECK(lbn_build_top_path(path, sizeof(path), "x", 0));
    CHECK_STR_EQ(path, "/v1/top?board=x&limit=1");
    char small[16];
    CHECK(lbn_build_top_path(small, sizeof(small), "echo_maze", 20) == false);
}

static void test_build_submit_body(void) {
    ScoreEntry e;
    memset(&e, 0, sizeof(e));
    snprintf(e.game, sizeof(e.game), "%s", DODGE_HARD);
    snprintf(e.player, sizeof(e.player), "ana");
    e.score = 1250;
    e.stamp = 1700000000LL; /* the server uses its own clock: the stamp is not sent */
    char body[128];
    CHECK(lbn_build_submit_body(body, sizeof(body), &e));
    CHECK_STR_EQ(body, "beat_dodge:Dodge / Bopeebo / hard|ana|1250\n");
    char small[10];
    CHECK(lbn_build_submit_body(small, sizeof(small), &e) == false);
}

static void test_valid_token(void) {
    CHECK(lbn_valid_token("abcXYZ_09-"));
    CHECK(!lbn_valid_token(""));
    CHECK(!lbn_valid_token("a b"));
    CHECK(!lbn_valid_token("a\r\nX-Evil: 1")); /* it goes into a header: no way to inject another one */
    CHECK(!lbn_valid_token("a|b"));
    char longest[LBN_TOKEN_MAX], too_long[LBN_TOKEN_MAX + 1];
    memset(longest, 'a', sizeof(longest) - 1);
    longest[sizeof(longest) - 1] = '\0';
    memset(too_long, 'a', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    CHECK(lbn_valid_token(longest));
    CHECK(!lbn_valid_token(too_long));
}

static void test_parse_register(void) {
    char tok[LBN_TOKEN_MAX];
    CHECK(lbn_parse_register("ok|abc123_-XYZ\n", tok, sizeof(tok)));
    CHECK_STR_EQ(tok, "abc123_-XYZ");
    CHECK(lbn_parse_register("ok|abc\r\n", tok, sizeof(tok))); /* CRLF is fine */
    CHECK(!lbn_parse_register("", tok, sizeof(tok)));
    CHECK(!lbn_parse_register("err|busy\n", tok, sizeof(tok)));
    CHECK(!lbn_parse_register("ok|\n", tok, sizeof(tok)));
    CHECK(!lbn_parse_register("ok|a b\n", tok, sizeof(tok)));
    CHECK(!lbn_parse_register("ok|abc|extra\n", tok, sizeof(tok)));
    CHECK(!lbn_parse_register("<html>please log in</html>", tok, sizeof(tok))); /* a hotspot's page */
    CHECK(lbn_parse_register("ok|abcdef\n", tok, 4) == false); /* does not fit the caller's buffer */
    char huge[200];
    memset(huge, 'a', sizeof(huge));
    memcpy(huge, "ok|", 3);
    huge[199] = '\0';
    CHECK(!lbn_parse_register(huge, tok, sizeof(tok))); /* a 196-character token is no 63-character token */
}

static void test_parse_submit(void) {
    int rank = 0, players = 0;
    CHECK(lbn_parse_submit("ok|3|12\n", &rank, &players));
    CHECK_EQ_INT(rank, 3);
    CHECK_EQ_INT(players, 12);
    CHECK(lbn_parse_submit("ok|1|1", &rank, &players));
    rank = players = -1;
    CHECK(!lbn_parse_submit("ok|0|5\n", &rank, &players)); /* ranks start at 1 */
    CHECK(!lbn_parse_submit("ok|6|5\n", &rank, &players)); /* a rank beyond the number of players */
    CHECK(!lbn_parse_submit("ok|x|5\n", &rank, &players));
    CHECK(!lbn_parse_submit("ok|3\n", &rank, &players));
    CHECK(!lbn_parse_submit("err|bad\n", &rank, &players));
    CHECK(!lbn_parse_submit("ok|99999999999|99999999999\n", &rank, &players)); /* does not fit an int */
    CHECK_EQ_INT(rank, -1); /* nothing written on failure */
    CHECK_EQ_INT(players, -1);
}

static void test_parse_top(void) {
    static OnlineTable t;
    CHECK(lbn_parse_top("ok|42\n1|ana|900|1700000001\n2|budi|800|1700000002\n", &t));
    CHECK_EQ_INT(t.players, 42);
    CHECK_EQ_INT(t.count, 2);
    CHECK_EQ_INT(t.rows[0].rank, 1);
    CHECK_STR_EQ(t.rows[0].player, "ana");
    CHECK_EQ_INT(t.rows[1].score, 800);
    CHECK_EQ_INT(t.rows[1].stamp, 1700000002LL);

    /* CRLF, an empty table, and bad rows in the middle are skipped without losing the good ones */
    CHECK(lbn_parse_top("ok|0\r\n", &t));
    CHECK_EQ_INT(t.count, 0);
    CHECK(lbn_parse_top("ok|3\n1|ana|9|1\nnonsense\n2||8|1\n0|zero|7|1\n3|cy|x|1\n4|dewi|6|1\n", &t));
    CHECK_EQ_INT(t.count, 2);
    CHECK_STR_EQ(t.rows[1].player, "dewi");

    /* the server is not trusted: control characters and over-long names are cleaned */
    CHECK(lbn_parse_top("ok|1\n1|ev\001il\033name-that-is-too-long|5|1\n", &t));
    CHECK_STR_EQ(t.rows[0].player, "ev_il_name-that"); /* 15 characters: LB_NAME_LEN - 1 */
    CHECK(strlen(t.rows[0].player) < LB_NAME_LEN);

    /* the header must be right: a login page or an error is not a table */
    CHECK(!lbn_parse_top("", &t));
    CHECK(!lbn_parse_top("err|nope\n", &t));
    CHECK(!lbn_parse_top("ok|abc\n", &t));
    CHECK(!lbn_parse_top("ok|-1\n", &t));
    CHECK(!lbn_parse_top("<html>\n", &t));

    /* more rows than a table holds: the extra ones are ignored */
    static char big[LBN_BODY_MAX];
    int n = snprintf(big, sizeof(big), "ok|99\n");
    for (int i = 1; i <= LBN_TOP_MAX + 10; i++)
        n += snprintf(big + n, sizeof(big) - (size_t)n, "%d|p%d|%d|1\n", i, i, 1000 - i);
    CHECK(lbn_parse_top(big, &t));
    CHECK_EQ_INT(t.count, LBN_TOP_MAX);
    CHECK_EQ_INT(t.players, 99);
}

/* ---------------------------------------------------------------- outbox */
static void test_outbox_keeps_only_the_best_score_per_player(void) {
    static Outbox ob;
    lbn_outbox_init(&ob);
    CHECK(lbn_outbox_add(&ob, "g", "Ana", 100, 1));
    CHECK(!lbn_outbox_add(&ob, "g", "ana", 100, 2)); /* equal: nothing to send */
    CHECK(!lbn_outbox_add(&ob, "g", "ANA", 90, 3));  /* worse: the server would ignore it */
    CHECK_EQ_INT(ob.count, 1);
    CHECK(lbn_outbox_add(&ob, "g", "ana", 150, 4)); /* better: replaces it (same player, any case) */
    CHECK_EQ_INT(ob.count, 1);
    CHECK_EQ_INT(ob.items[0].score, 150);
    CHECK_EQ_INT(ob.items[0].stamp, 4);
    CHECK(lbn_outbox_add(&ob, "other", "ana", 5, 5)); /* another table: its own entry */
    CHECK(lbn_outbox_add(&ob, "g", "budi", 5, 6));    /* another player: its own entry */
    CHECK_EQ_INT(ob.count, 3);
    CHECK(lbn_outbox_add(&ob, "g", "ev|il", 1, 7)); /* names and tables are sanitised like in scores.txt */
    CHECK_STR_EQ(ob.items[3].player, "ev_il");
    CHECK(lbn_outbox_add(&ob, "g", "", 1, 8));
    CHECK_STR_EQ(ob.items[4].player, "PLAYER");
}

static void test_outbox_full_drops_the_oldest(void) {
    static Outbox ob;
    lbn_outbox_init(&ob);
    for (int i = 0; i < LBN_OUTBOX_MAX; i++) {
        char who[LB_NAME_LEN];
        snprintf(who, sizeof(who), "p%d", i);
        CHECK(lbn_outbox_add(&ob, "g", who, 10 + i, i));
    }
    CHECK_EQ_INT(ob.count, LBN_OUTBOX_MAX);
    CHECK(lbn_outbox_add(&ob, "g", "newcomer", 1, 999));
    CHECK_EQ_INT(ob.count, LBN_OUTBOX_MAX);
    CHECK_STR_EQ(ob.items[0].player, "p1"); /* p0, the oldest, went */
    CHECK_STR_EQ(ob.items[LBN_OUTBOX_MAX - 1].player, "newcomer");
}

static void test_outbox_ack_keeps_a_better_score_queued_meanwhile(void) {
    static Outbox ob;
    lbn_outbox_init(&ob);
    lbn_outbox_add(&ob, "g", "ana", 100, 1);
    lbn_outbox_add(&ob, "g", "ana", 150, 2);      /* queued while the 100 was on its way */
    CHECK(!lbn_outbox_ack(&ob, "g", "ana", 100)); /* the server took the 100: the 150 is still owed */
    CHECK_EQ_INT(ob.count, 1);
    CHECK(lbn_outbox_ack(&ob, "g", "ANA", 150));
    CHECK_EQ_INT(ob.count, 0);
    CHECK(!lbn_outbox_ack(&ob, "g", "ana", 150)); /* nothing left to remove */
    lbn_outbox_add(&ob, "g", "ana", 100, 3);
    CHECK(lbn_outbox_ack(&ob, "g", "ana", 120)); /* an ack of a higher score also covers a lower entry */
}

static void test_outbox_save_load_and_damaged_file(void) {
    static Outbox ob, back;
    lbn_outbox_init(&ob);
    lbn_outbox_add(&ob, DODGE_HARD, "ana", 900, 1700000000LL);
    lbn_outbox_add(&ob, "echo_maze", "budi", 40, 1700000100LL);
    CHECK(lbn_outbox_save(&ob, OUTBOX_FILE));
    CHECK_EQ_INT(lbn_outbox_load(&back, OUTBOX_FILE), 2);
    CHECK_STR_EQ(back.items[0].game, DODGE_HARD);
    CHECK_EQ_INT(back.items[0].score, 900);
    CHECK_EQ_INT(back.items[1].stamp, 1700000100LL);
    remove(OUTBOX_FILE);
    CHECK_EQ_INT(lbn_outbox_load(&back, OUTBOX_FILE), -1); /* missing file */
    CHECK_EQ_INT(back.count, 0);

    /* a hand-edited file: garbage lines are skipped and the one-per-player rule still holds */
    FILE *fp = fopen(OUTBOX_FILE, "w");
    CHECK(fp != NULL);
    fputs("g|ana|10|1\nnot a score line\ng|ANA|30|2\ng|ana|20|3\n\ng|budi|x|4\n", fp);
    fclose(fp);
    CHECK_EQ_INT(lbn_outbox_load(&back, OUTBOX_FILE), 1);
    CHECK_EQ_INT(back.items[0].score, 30);
    remove(OUTBOX_FILE);
}

/* ---------------------------------------------------------------- cache */
static void make_table(OnlineTable *t, const char *key, long long fetched, int rows) {
    memset(t, 0, sizeof(*t));
    snprintf(t->key, sizeof(t->key), "%s", key);
    t->fetched = fetched;
    t->players = rows + 5;
    t->count = rows;
    for (int i = 0; i < rows; i++) {
        t->rows[i].rank = i + 1;
        snprintf(t->rows[i].player, sizeof(t->rows[i].player), "p%d", i);
        t->rows[i].score = 100 - i;
        t->rows[i].stamp = 1700000000LL + i;
    }
}

static void test_cache_put_get_replace_and_evict(void) {
    static OnlineCache c;
    static OnlineTable t;
    lbn_cache_init(&c);
    CHECK(lbn_cache_get(&c, "a") == NULL);
    make_table(&t, "a", 100, 3);
    lbn_cache_put(&c, &t);
    CHECK_EQ_INT(c.count, 1);
    CHECK_EQ_INT(lbn_cache_get(&c, "a")->count, 3);
    make_table(&t, "a", 200, 5); /* the same key replaces the old copy */
    lbn_cache_put(&c, &t);
    CHECK_EQ_INT(c.count, 1);
    CHECK_EQ_INT(lbn_cache_get(&c, "a")->count, 5);
    CHECK_EQ_INT(lbn_cache_get(&c, "a")->fetched, 200);

    /* fill it, then one more: the table fetched LONGEST AGO goes. The oldest must not sit in the first slot,
     * or "drop the first" would pass for "drop the oldest": refresh "a" (slot 0), so "t1" becomes the oldest
     */
    for (int i = 1; i < LBN_CACHE_TABLES; i++) {
        char key[8];
        snprintf(key, sizeof(key), "t%d", i);
        make_table(&t, key, 1000 + i, 1);
        lbn_cache_put(&c, &t);
    }
    CHECK_EQ_INT(c.count, LBN_CACHE_TABLES);
    make_table(&t, "a", 5000, 1);
    lbn_cache_put(&c, &t);
    make_table(&t, "new", 6000, 1);
    lbn_cache_put(&c, &t);
    CHECK_EQ_INT(c.count, LBN_CACHE_TABLES);
    CHECK(lbn_cache_get(&c, "t1") == NULL); /* fetched at 1001: the oldest */
    CHECK(lbn_cache_get(&c, "a") != NULL);  /* the first slot, but freshly fetched: it stays */
    CHECK(lbn_cache_get(&c, "t2") != NULL);
    CHECK(lbn_cache_get(&c, "new") != NULL);
}

static void test_cache_save_load_and_damaged_file(void) {
    static OnlineCache c, back;
    static OnlineTable t;
    lbn_cache_init(&c);
    make_table(&t, DODGE_HARD, 1700000500LL, 4);
    lbn_cache_put(&c, &t);
    make_table(&t, "empty_board", 1700000600LL, 0); /* a table nobody has played: no rows, still remembered */
    lbn_cache_put(&c, &t);
    CHECK(lbn_cache_save(&c, CACHE_FILE));
    CHECK_EQ_INT(lbn_cache_load(&back, CACHE_FILE), 2);
    const OnlineTable *got = lbn_cache_get(&back, DODGE_HARD);
    CHECK(got != NULL);
    CHECK_EQ_INT(got->fetched, 1700000500LL);
    CHECK_EQ_INT(got->players, 9);
    CHECK_EQ_INT(got->count, 4);
    CHECK_STR_EQ(got->rows[3].player, "p3");
    CHECK_EQ_INT(got->rows[3].score, 97);
    CHECK_EQ_INT(lbn_cache_get(&back, "empty_board")->count, 0);
    remove(CACHE_FILE);
    CHECK_EQ_INT(lbn_cache_load(&back, CACHE_FILE), -1);

    /* damaged: rows before any header, a bad header (its rows are skipped too), bad rows, a good table */
    FILE *fp = fopen(CACHE_FILE, "w");
    CHECK(fp != NULL);
    fputs("1|orphan|5|1\n@broken-header\n1|lost|5|1\n@good|1700000000|7\n1|ana|9|1\ngarbage\n2|budi|8|2\n",
          fp);
    fclose(fp);
    CHECK_EQ_INT(lbn_cache_load(&back, CACHE_FILE), 1);
    CHECK_EQ_INT(back.tables[0].players, 7);
    CHECK_EQ_INT(back.tables[0].count, 2);
    CHECK_STR_EQ(back.tables[0].rows[1].player, "budi");
    remove(CACHE_FILE);
}

/* ---------------------------------------------------------------- the client */
static void test_client_without_opt_in_or_work_sends_nothing(void) {
    memset(&g_fake, 0, sizeof(g_fake));
    LbnTransport tr = {fake_start, fake_poll, &g_fake};
    lbn_init(&g_c, &tr);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_DISABLED);
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 100); /* not opted in: not even queued */
    lbn_want_table(&g_c, "g", 100);
    pump(100, 5);
    CHECK_EQ_INT(g_c.outbox.count, 0);
    CHECK_EQ_INT(g_fake.started, 0);

    lbn_enable(&g_c, true); /* opted in, but nothing to send or fetch: stays quiet, makes no requests */
    pump(100, 50);
    pump(100000, 50);
    CHECK_EQ_INT(g_fake.started, 0);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_ONLINE);

    LbnClient *no_transport = &g_c; /* a client without a transport can never be enabled */
    lbn_init(no_transport, NULL);
    lbn_enable(no_transport, true);
    CHECK_EQ_INT(lbn_status(no_transport), LBN_DISABLED);
}

static void test_client_first_run_registers_then_submits(void) {
    fresh_client();
    g_c.token[0] = '\0'; /* a new install */
    lbn_set_paths(&g_c, OUTBOX_FILE, "", TOKEN_FILE);
    say(&g_fake, 200, "ok|newtoken_01\n");
    say(&g_fake, 200, "ok|3|17\n");
    lbn_queue_score(&g_c, DODGE_HARD, "ana", 1250, 1700000000LL, 1000);
    CHECK_EQ_INT(g_c.outbox.count, 1);

    pump(1000, 1);
    CHECK_EQ_INT(g_fake.started, 1); /* register comes first: there is no token yet */
    CHECK_STR_EQ(g_fake.method, "POST");
    CHECK_STR_EQ(g_fake.path, "/v1/register");
    CHECK(!g_fake.had_token);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_SYNCING);
    pump(1000, 1);
    CHECK_STR_EQ(g_c.token, "newtoken_01");

    pump(1000, 1); /* the score goes out with the token */
    CHECK_EQ_INT(g_fake.started, 2);
    CHECK_STR_EQ(g_fake.method, "POST");
    CHECK_STR_EQ(g_fake.path, "/v1/scores");
    CHECK_STR_EQ(g_fake.token, "newtoken_01");
    CHECK_STR_EQ(g_fake.body, "beat_dodge:Dodge / Bopeebo / hard|ana|1250\n");
    pump(1000, 1);
    CHECK_EQ_INT(g_c.outbox.count, 0); /* accepted: off the list */
    CHECK(g_c.ack.valid);
    CHECK_STR_EQ(g_c.ack.table, DODGE_HARD);
    CHECK_STR_EQ(g_c.ack.player, "ana");
    CHECK_EQ_INT(g_c.ack.score, 1250);
    CHECK_EQ_INT(g_c.ack.rank, 3);
    CHECK_EQ_INT(g_c.ack.players, 17);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_ONLINE);
    CHECK(!g_c.storage_error);

    /* the token and the (now empty) outbox are on disk */
    static LbnClient back;
    LbnTransport tr = {fake_start, fake_poll, &g_fake};
    lbn_init(&back, &tr);
    lbn_set_paths(&back, OUTBOX_FILE, "", TOKEN_FILE);
    lbn_load(&back);
    CHECK_STR_EQ(back.token, "newtoken_01");
    CHECK_EQ_INT(back.outbox.count, 0);
    remove(OUTBOX_FILE);
    remove(TOKEN_FILE);

    pump(1000, 20); /* idle again: no more requests */
    CHECK_EQ_INT(g_fake.started, 2);
}

static void test_client_never_blocks_and_runs_one_request_at_a_time(void) {
    fresh_client();
    g_fake.pending_polls = 3;
    say(&g_fake, 200, "ok|1|1\n");
    say(&g_fake, 200, "ok|1|1\n");
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 50);
    lbn_queue_score(&g_c, "g", "budi", 20, 2, 50);
    pump(50, 1);
    CHECK_EQ_INT(g_fake.started, 1);
    for (int i = 0; i < 3; i++) { /* the answer is not there yet: every update returns at once */
        pump(50, 1);
        CHECK_EQ_INT(g_fake.started, 1); /* and no second request is started on top of the first */
        CHECK_EQ_INT(lbn_status(&g_c), LBN_SYNCING);
        CHECK_EQ_INT(g_c.outbox.count, 2);
    }
    pump(50, 1); /* now it arrives */
    CHECK_EQ_INT(g_c.outbox.count, 1);
    pump(50, 5); /* the second one follows, again one at a time */
    CHECK_EQ_INT(g_fake.started, 2);
}

static void test_client_server_down_keeps_scores_and_backs_off(void) {
    fresh_client();
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 2); /* no answer (script is empty): status 0 */
    CHECK_EQ_INT(g_fake.started, 1);
    CHECK_EQ_INT(g_c.outbox.count, 1); /* kept */
    CHECK_EQ_INT(lbn_status(&g_c), LBN_OFFLINE);
    CHECK_EQ_INT(g_c.failures, 1);
    CHECK_EQ_INT(g_c.next_try, 1000 + LBN_BACKOFF_FIRST);

    pump(1000 + LBN_BACKOFF_FIRST - 1, 10); /* too early: it does not hammer a dead server */
    CHECK_EQ_INT(g_fake.started, 1);
    pump(1000 + LBN_BACKOFF_FIRST, 2); /* time is up: one more try, which fails again */
    CHECK_EQ_INT(g_fake.started, 2);
    CHECK_EQ_INT(g_c.failures, 2);
    CHECK_EQ_INT(g_c.next_try, 1030 + 2 * LBN_BACKOFF_FIRST); /* the wait doubles */

    for (int i = 0; i < 20; i++) { /* it never waits longer than the maximum, however long the outage */
        long long t = g_c.next_try;
        pump(t, 2);
        CHECK(g_c.next_try - t <= LBN_BACKOFF_MAX);
    }
    CHECK(g_c.failures >= 10);
    CHECK_EQ_INT(g_c.next_try - (g_c.next_try - LBN_BACKOFF_MAX), LBN_BACKOFF_MAX);
    CHECK_EQ_INT(g_c.outbox.count, 1); /* still there after all of it */

    say(&g_fake, 200, "ok|1|4\n"); /* the server is back */
    pump(g_c.next_try, 2);
    CHECK_EQ_INT(g_c.outbox.count, 0);
    CHECK_EQ_INT(g_c.failures, 0);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_ONLINE);
}

static void test_kick_skips_the_wait(void) {
    fresh_client();
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 2); /* fails: next try in 30 s */
    CHECK_EQ_INT(g_fake.started, 1);
    say(&g_fake, 200, "ok|1|1\n");
    lbn_kick(&g_c, 1005); /* the player opened the Online tab */
    pump(1005, 2);
    CHECK_EQ_INT(g_fake.started, 2);
    CHECK_EQ_INT(g_c.outbox.count, 0);
    fresh_client();
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 2);
    lbn_queue_score(&g_c, "g", "ana", 20, 2, 1010); /* a better run also allows an immediate try */
    pump(1010, 2);
    CHECK_EQ_INT(g_fake.started, 2);
}

static void test_client_start_failure_counts_as_failure(void) {
    fresh_client();
    g_fake.refuse_start = true;
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 3);
    CHECK_EQ_INT(g_fake.started, 0);
    CHECK_EQ_INT(g_c.failures, 1);
    CHECK_EQ_INT(g_c.outbox.count, 1);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_OFFLINE);
}

/* A score is thrown away only when the server really refuses it. Everything else keeps it for later. */
static void test_only_a_real_refusal_drops_a_score(void) {
    static const int keep_statuses[] = {500, 502, 503, 408, 429, 302, 301, 204, 100};
    for (size_t i = 0; i < sizeof(keep_statuses) / sizeof(keep_statuses[0]); i++) {
        fresh_client();
        say(&g_fake, keep_statuses[i], "err|whatever\n");
        lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
        pump(1000, 2);
        CHECK_EQ_INT(g_c.outbox.count, 1);
        CHECK_EQ_INT(g_c.failures, 1);
        CHECK_EQ_INT(g_c.refused, 0);
    }
    /* a 200 that is not our answer (a hotspot's login page) is not "delivered" either */
    fresh_client();
    say(&g_fake, 200, "<html><body>Please log in to the wifi</body></html>");
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 2);
    CHECK_EQ_INT(g_c.outbox.count, 1);
    CHECK_EQ_INT(g_c.failures, 1);
    CHECK(!g_c.ack.valid);

    static const int refuse_statuses[] = {400, 403, 404, 413, 422};
    for (size_t i = 0; i < sizeof(refuse_statuses) / sizeof(refuse_statuses[0]); i++) {
        fresh_client();
        say(&g_fake, refuse_statuses[i], "err|invalid_score\n");
        lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
        pump(1000, 2);
        CHECK_EQ_INT(g_c.outbox.count, 0); /* dropped: it would be refused again, forever */
        CHECK_EQ_INT(g_c.refused, 1);
        CHECK_EQ_INT(g_c.failures, 0); /* the server answered, so it is reachable */
        pump(1000, 20);
        CHECK_EQ_INT(g_fake.started, 1); /* and it is not retried */
    }
}

static void test_unknown_token_means_register_again(void) {
    fresh_client();
    lbn_set_paths(&g_c, "", "", TOKEN_FILE);
    say(&g_fake, 401, "err|unknown_token\n");
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    pump(1000, 2);
    CHECK_EQ_INT(g_c.token[0], '\0');  /* forgotten */
    CHECK_EQ_INT(g_c.outbox.count, 1); /* the score is still owed */
    CHECK_EQ_INT(g_c.failures, 1);     /* the usual wait, so a server that always says 401 is not hammered */
    pump(1000 + 10, 5);
    CHECK_EQ_INT(g_fake.started, 1);
    say(&g_fake, 200, "ok|freshtoken\n");
    say(&g_fake, 200, "ok|2|2\n");
    pump(1000 + LBN_BACKOFF_FIRST, 4);
    CHECK_STR_EQ(g_fake.paths[1], "/v1/register");
    CHECK_STR_EQ(g_fake.paths[2], "/v1/scores");
    CHECK_STR_EQ(g_c.token, "freshtoken");
    CHECK_EQ_INT(g_c.outbox.count, 0);
    remove(TOKEN_FILE);
}

static void test_a_better_score_queued_while_sending_is_still_sent(void) {
    fresh_client();
    g_fake.pending_polls = 2;
    say(&g_fake, 200, "ok|5|9\n");
    say(&g_fake, 200, "ok|2|9\n");
    lbn_queue_score(&g_c, "g", "ana", 100, 1, 1000);
    pump(1000, 1);                                   /* the 100 is on its way */
    lbn_queue_score(&g_c, "g", "ana", 150, 2, 1000); /* meanwhile a better run ends */
    pump(1000, 3);                                   /* the answer for the 100 arrives */
    CHECK_EQ_INT(g_c.outbox.count, 1);               /* the 150 is still queued */
    CHECK_EQ_INT(g_c.outbox.items[0].score, 150);
    pump(1000, 5);
    CHECK_STR_EQ(g_fake.body, "g|ana|150\n");
    CHECK_EQ_INT(g_c.outbox.count, 0);
    CHECK_EQ_INT(g_c.ack.rank, 2);
}

static void test_fetching_a_table_after_the_outbox_and_caching_it(void) {
    fresh_client();
    say(&g_fake, 200, "ok|1|3\n");
    say(&g_fake, 200, "ok|3\n1|ana|900|1700000001\n2|budi|800|1700000002\n3|citra|700|1700000003\n");
    lbn_queue_score(&g_c, DODGE_HARD, "ana", 900, 1, 2000);
    lbn_want_table(&g_c, DODGE_HARD, 2000);
    pump(2000, 6);
    CHECK_EQ_INT(g_fake.started, 2);
    CHECK_STR_EQ(g_fake.paths[0], "/v1/scores"); /* what is owed goes first ... */
    CHECK_STR_EQ(g_fake.paths[1], "/v1/top?board=beat_dodge%3ADodge%20%2F%20Bopeebo%20%2F%20hard&limit=50");
    CHECK_STR_EQ(g_fake.method, "GET");
    CHECK_STR_EQ(g_fake.token, "tok");
    const OnlineTable *t = lbn_table(&g_c, DODGE_HARD); /* ... and the table is cached afterwards */
    CHECK(t != NULL);
    CHECK_EQ_INT(t->count, 3);
    CHECK_EQ_INT(t->players, 3);
    CHECK_EQ_INT(t->fetched, 2000);
    CHECK_STR_EQ(t->rows[2].player, "citra");

    lbn_want_table(&g_c, DODGE_HARD, 2000 + LBN_FRESH_SECS - 1); /* fresh enough: no new request */
    pump(2000 + LBN_FRESH_SECS - 1, 6);
    CHECK_EQ_INT(g_fake.started, 2);
    say(&g_fake, 200, "ok|4\n1|ana|900|1\n");
    lbn_want_table(&g_c, DODGE_HARD, 2000 + LBN_FRESH_SECS); /* stale: fetched again */
    pump(2000 + LBN_FRESH_SECS, 3);
    CHECK_EQ_INT(g_fake.started, 3);
    CHECK_EQ_INT(lbn_table(&g_c, DODGE_HARD)->players, 4);
    CHECK_EQ_INT(lbn_table(&g_c, DODGE_HARD)->count, 1);
}

static void test_failed_fetch_keeps_the_old_copy(void) {
    fresh_client();
    say(&g_fake, 200, "ok|1\n1|ana|900|1\n");
    lbn_want_table(&g_c, "g", 100);
    pump(100, 3);
    CHECK_EQ_INT(lbn_table(&g_c, "g")->count, 1);
    say(&g_fake, 500, "err|oops\n");
    lbn_want_table(&g_c, "g", 500);
    pump(500, 3);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_OFFLINE);
    CHECK(lbn_table(&g_c, "g") != NULL); /* the player still has something to look at */
    CHECK_EQ_INT(lbn_table(&g_c, "g")->fetched, 100);
    say(&g_fake, 404, "err|no_such_table\n"); /* a refused question is not an outage and caches nothing new */
    lbn_kick(&g_c, 600);
    lbn_want_table(&g_c, "nothing", 600);
    pump(600, 3);
    CHECK_EQ_INT(lbn_status(&g_c), LBN_ONLINE);
    CHECK(lbn_table(&g_c, "nothing") == NULL);
}

static void test_everything_survives_a_restart(void) {
    /* session 1: online, a table is fetched (the first request registers this install) */
    fresh_client();
    g_c.token[0] = '\0';
    lbn_set_paths(&g_c, OUTBOX_FILE, CACHE_FILE, TOKEN_FILE);
    say(&g_fake, 200, "ok|installtoken\n");
    say(&g_fake, 200, "ok|2\n1|ana|900|1700000001\n2|budi|800|1700000002\n");
    lbn_want_table(&g_c, "g", 3000);
    pump(3000, 6);
    CHECK_EQ_INT(g_fake.started, 2);

    /* the server goes away; the player finishes two runs */
    lbn_queue_score(&g_c, "g", "ana", 950, 3100, 3100);
    lbn_queue_score(&g_c, "h", "ana", 40, 3200, 3200);
    pump(3200, 4);
    CHECK_EQ_INT(g_c.outbox.count, 2);
    CHECK(!g_c.storage_error);

    /* session 2: a new process, same files, still no server */
    static LbnClient b;
    LbnTransport tr = {fake_start, fake_poll, &g_fake};
    lbn_init(&b, &tr);
    lbn_set_paths(&b, OUTBOX_FILE, CACHE_FILE, TOKEN_FILE);
    lbn_load(&b);
    lbn_enable(&b, true);
    CHECK_STR_EQ(b.token, "installtoken");
    CHECK_EQ_INT(b.outbox.count, 2);
    CHECK_STR_EQ(b.outbox.items[0].player, "ana");
    CHECK_EQ_INT(b.outbox.items[0].score, 950);
    const OnlineTable *t = lbn_table(&b, "g"); /* the Online tab can show the last table it knew */
    CHECK(t != NULL);
    CHECK_EQ_INT(t->count, 2);
    CHECK_EQ_INT(t->fetched, 3000);
    remove(OUTBOX_FILE);
    remove(CACHE_FILE);
    remove(TOKEN_FILE);
}

static void test_unwritable_storage_is_reported_but_nothing_breaks(void) {
    fresh_client();
    lbn_set_paths(&g_c, "no_such_dir/outbox.txt", "", "");
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 1000);
    CHECK(g_c.storage_error); /* the app can warn: the score lives in memory only */
    CHECK_EQ_INT(g_c.outbox.count, 1);
    say(&g_fake, 200, "ok|1|1\n");
    pump(1000, 2);
    CHECK_EQ_INT(g_c.outbox.count, 0); /* and it is still delivered */
}

/* The exact bytes the deployed Cloudflare Worker (the arcade-leaderboard-server repository) answered,
 * captured with curl on 2026-11-18. If the server's format and this parser ever drift apart, this is the test
 * that says so. The register token is made up (a real one is a credential) but has the real shape: 32
 * characters of base64url, so it also uses '-' and '_'. */
static void test_parse_what_the_real_server_sent(void) {
    char token[LBN_TOKEN_MAX];
    CHECK(lbn_parse_register("ok|a1B2c3D4e5F6g7H8i9J0k-L_m-N_o-P_\n", token, sizeof(token)));
    CHECK_STR_EQ(token, "a1B2c3D4e5F6g7H8i9J0k-L_m-N_o-P_");

    int rank = 0, players = 0;
    CHECK(lbn_parse_submit("ok|1|3\n", &rank, &players));
    CHECK_EQ_INT(rank, 1);
    CHECK_EQ_INT(players, 3);

    static OnlineTable t;
    CHECK(lbn_parse_top("ok|3\n"
                        "1|e2e-golden|11|1794990891\n"
                        "2|e2e-test|7|1794972931\n"
                        "3|e2e-test|7|1794990890\n",
                        &t));
    CHECK_EQ_INT(t.players, 3);
    CHECK_EQ_INT(t.count, 3);
    CHECK_EQ_INT(t.rows[0].rank, 1);
    CHECK_STR_EQ(t.rows[0].player, "e2e-golden"); /* '-' in a name survives the sanitising */
    CHECK_EQ_INT(t.rows[0].score, 11);
    CHECK_EQ_INT(t.rows[0].stamp, 1794990891LL);
    CHECK_EQ_INT(t.rows[2].stamp, 1794990890LL);
    CHECK(t.rows[1].stamp < t.rows[2].stamp); /* a tie at 7: the earlier run is ranked higher */

    /* an empty table, and the refusal the server sent for an unknown game (HTTP 400, "err|bad_table") */
    CHECK(lbn_parse_top("ok|0\n", &t));
    CHECK_EQ_INT(t.count, 0);
    CHECK(!lbn_parse_submit("err|bad_table\n", &rank, &players));
    CHECK(!lbn_parse_top("err|bad_table\n", &t));
}

static void test_clearing_the_outbox_forgets_what_was_waiting(void) {
    fresh_client();
    lbn_set_paths(&g_c, OUTBOX_FILE, "", "");
    lbn_queue_score(&g_c, "g", "ana", 10, 1, 100);
    lbn_queue_score(&g_c, "h", "ana", 20, 2, 100);
    CHECK_EQ_INT(g_c.outbox.count, 2);
    lbn_clear_outbox(&g_c);
    CHECK_EQ_INT(g_c.outbox.count, 0);
    pump(100, 5);
    CHECK_EQ_INT(g_fake.started, 0); /* nothing is sent any more */
    static Outbox back; /* and the file on disk is empty too, so a restart does not bring the scores back */
    CHECK_EQ_INT(lbn_outbox_load(&back, OUTBOX_FILE), 0);
    remove(OUTBOX_FILE);
}

static void test_which_server_addresses_are_allowed(void) {
    for (const char **u =
             (const char *[]){"https://arcade-leaderboard.example.workers.dev", "https://example.com:8443",
                              "https://a.b-c.d", "https://203.0.113.7", "http://localhost",
                              "http://localhost:8787", "http://127.0.0.1", "http://127.0.0.1:8787", NULL};
         *u != NULL; u++)
        CHECK(lbn_valid_base_url(*u));
    for (const char **u =
             (const char *[]){
                 "",
                 "https://",
                 "http://example.com", /* plain http to a real server: the token would travel in the clear */
                 "http://203.0.113.7",
                 "http://localhost.evil.com", /* only the exact names count as this computer */
                 "http://127.0.0.1.evil.com",
                 "http://127.0.0.10",
                 "http://localhost:",
                 "ftp://example.com",
                 "example.com",
                 "HTTPS://example.com", /* the scheme is matched exactly: we never guess */
                 "https://example.com/",
                 "https://example.com/v1",
                 "https://user:pass@example.com", /* credentials in a URL end up in logs */
                 "https://user@example.com",
                 "https://example.com?x=1",
                 "https://example.com#x",
                 "https://exa mple.com",
                 "https://example.com:0",
                 "https://example.com:65536",
                 "https://example.com:123456",
                 "https://example.com:80a",
                 "https://.example.com",
                 "https://example.com.",
                 "https://[::1]",
                 "https://example.com\n",
                 NULL};
         *u != NULL; u++)
        CHECK(!lbn_valid_base_url(*u));
    char long_url[260];
    memset(long_url, 'a', sizeof(long_url));
    memcpy(long_url, "https://", 8);
    long_url[250] = '\0';
    CHECK(!lbn_valid_base_url(long_url));
}

static void test_refresh_asks_again_even_when_the_copy_is_fresh(void) {
    fresh_client();
    say(&g_fake, 200, "ok|1\n1|ana|9|1\n");
    say(&g_fake, 200, "ok|2\n1|ana|9|1\n2|budi|8|2\n");
    lbn_want_table(&g_c, "g", 1000);
    pump(1000, 3);
    CHECK_EQ_INT(g_fake.started, 1);
    lbn_want_table(&g_c, "g", 1005); /* fresh: nothing */
    pump(1005, 4);
    CHECK_EQ_INT(g_fake.started, 1);
    lbn_refresh_table(&g_c, "g", 1005); /* the player pressed "refresh" */
    pump(1005, 3);
    CHECK_EQ_INT(g_fake.started, 2);
    CHECK_EQ_INT(lbn_table(&g_c, "g")->players, 2);
    fresh_client(); /* not opted in: refreshing does nothing either */
    lbn_enable(&g_c, false);
    lbn_refresh_table(&g_c, "g", 1);
    pump(1, 4);
    CHECK_EQ_INT(g_fake.started, 0);
}

int main(void) {
    RUN(test_url_encode);
    RUN(test_build_top_path);
    RUN(test_build_submit_body);
    RUN(test_valid_token);
    RUN(test_parse_register);
    RUN(test_parse_submit);
    RUN(test_parse_top);
    RUN(test_outbox_keeps_only_the_best_score_per_player);
    RUN(test_outbox_full_drops_the_oldest);
    RUN(test_outbox_ack_keeps_a_better_score_queued_meanwhile);
    RUN(test_outbox_save_load_and_damaged_file);
    RUN(test_cache_put_get_replace_and_evict);
    RUN(test_cache_save_load_and_damaged_file);
    RUN(test_parse_what_the_real_server_sent);
    RUN(test_client_without_opt_in_or_work_sends_nothing);
    RUN(test_client_first_run_registers_then_submits);
    RUN(test_client_never_blocks_and_runs_one_request_at_a_time);
    RUN(test_client_server_down_keeps_scores_and_backs_off);
    RUN(test_kick_skips_the_wait);
    RUN(test_client_start_failure_counts_as_failure);
    RUN(test_only_a_real_refusal_drops_a_score);
    RUN(test_unknown_token_means_register_again);
    RUN(test_a_better_score_queued_while_sending_is_still_sent);
    RUN(test_fetching_a_table_after_the_outbox_and_caching_it);
    RUN(test_failed_fetch_keeps_the_old_copy);
    RUN(test_everything_survives_a_restart);
    RUN(test_unwritable_storage_is_reported_but_nothing_breaks);
    RUN(test_clearing_the_outbox_forgets_what_was_waiting);
    RUN(test_which_server_addresses_are_allowed);
    RUN(test_refresh_asks_again_even_when_the_copy_is_fresh);
    return TEST_SUMMARY();
}
