/* Developer tool: runs the real client code (lbnet + libcurl) against a real server and reports each step.
 *
 *   arcade_online_probe <server> <table> <player> <score>
 *   arcade_online_probe https://arcade-leaderboard.<account>.workers.dev different_color probe-1 7
 *
 * It registers a new install, sends the score, fetches the table and prints what came back; nothing is
 * stored on disk. Exit code 0 = every step worked, 1 = a step failed, 2 = bad arguments. The row it adds is
 * real: remove it afterwards (see the server's README). Not part of ctest, because it needs a network. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arcade/lbnet.h"
#include "net.h"

#define PROBE_SECONDS 25

/* A short pause between updates without any OS call: spins until the processor clock has moved on. */
static void pause_ms(int ms) {
    clock_t end = clock() + (clock_t)ms * CLOCKS_PER_SEC / 1000;
    while (clock() < end) {
    }
}

static int step(bool ok, const char *what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    return ok ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <server> <table> <player> <score>\n", argv[0]);
        return 2;
    }
    const char *table = argv[2];
    static LbnClient c; /* about 26 KB: static */
    if (!net_open(argv[1]))
        return 2;
    LbnTransport transport = net_transport();
    lbn_init(&c, &transport);
    lbn_enable(&c, true);

    long long start = (long long)time(NULL);
    lbn_queue_score(&c, table, argv[3], atoi(argv[4]), start, start);
    lbn_want_table(&c, table, start);
    for (long long now = start; now - start < PROBE_SECONDS; now = (long long)time(NULL)) {
        lbn_update(&c, now);
        if (c.job == LBN_JOB_NONE && c.token[0] != '\0' && c.outbox.count == 0 && c.wanted[0] == '\0' &&
            lbn_table(&c, table) != NULL)
            break; /* everything is done */
        pause_ms(5);
    }

    int bad = 0;
    printf("server %s, %lld s\n", argv[1], (long long)time(NULL) - start);
    bad += step(c.token[0] != '\0', "registered (the server gave this install a token)");
    bad += step(c.outbox.count == 0 && c.ack.valid, "the score was accepted and left the outbox");
    if (c.ack.valid)
        printf("     rank %d of %d players on %s\n", c.ack.rank, c.ack.players, c.ack.table);
    const OnlineTable *t = lbn_table(&c, table);
    bad += step(t != NULL, "the table came back");
    if (t != NULL) {
        printf("     %d player(s), %d row(s) received\n", t->players, t->count);
        for (int i = 0; i < t->count && i < 10; i++)
            printf("     %2d  %-15s %d\n", t->rows[i].rank, t->rows[i].player, t->rows[i].score);
    }
    if (bad != 0)
        printf("not delivered: %d score(s) still queued, %d failure(s) in a row, %d refused\n",
               c.outbox.count, c.failures, c.refused);
    net_close();
    return bad == 0 ? 0 : 1;
}
