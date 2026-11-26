/* The player's side of the online leaderboard (app/online.c): on by default, the one-time notice that comes
 * before anything is sent, what is remembered, and what is forgotten when it is turned off. No network is
 * used: nothing here calls online_tick(). */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "arcade_test.h"
#include "online.h"

static AppState g_app; /* static: the leaderboard and the online client inside it are big */

static void forget_files(void) {
    remove("online.cfg");
    remove("outbox.txt");
    remove("online_cache.txt");
    remove("identity.txt");
}

/* A run of the program: a fresh AppState, as main() makes it, and online_init like main() calls it. */
static void start_program(void) {
    memset(&g_app, 0, sizeof(g_app));
    snprintf(g_app.data_dir, sizeof(g_app.data_dir), ".");
    snprintf(g_app.player, sizeof(g_app.player), "Ana");
    online_init(&g_app);
}

static void end_program(void) {
    online_shutdown(&g_app);
}

static bool file_exists(const char *path) {
    FILE *fp = fopen(path, "r");
    if (fp)
        fclose(fp);
    return fp != NULL;
}

/* The first line of a file, "" when there is none. */
static const char *first_line(const char *path) {
    static char line[128];
    line[0] = '\0';
    FILE *fp = fopen(path, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp) == NULL)
            line[0] = '\0';
        fclose(fp);
    }
    return line;
}

static void test_on_by_default_but_nothing_moves_before_the_player_has_been_told(void) {
    forget_files();
    start_program();
    CHECK(online_ready(&g_app)); /* this test is built only with libcurl, and the address is a local one */
    CHECK(online_first_run(&g_app)); /* no online.cfg yet: the notice is due */
    CHECK(g_app.lbn.enabled);        /* "on" is the default ... */
    CHECK(!online_enabled(&g_app));  /* ... but it does not act until the notice has been answered */
    online_submit(&g_app, "loop_lap", 42);
    online_want(&g_app, "loop_lap");
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);
    CHECK_EQ_INT(g_app.lbn.wanted[0], '\0');
    CHECK(!file_exists("outbox.txt"));
    CHECK(!file_exists("identity.txt"));
    CHECK(!file_exists("online.cfg")); /* nothing is decided for the player behind their back */
    end_program();
    forget_files();
}

static void test_keeping_it_on_is_remembered_and_the_notice_is_not_shown_again(void) {
    forget_files();
    start_program();
    online_answer_first_run(&g_app, true);
    CHECK(!online_first_run(&g_app));
    CHECK(online_enabled(&g_app));
    CHECK_STR_EQ(first_line("online.cfg"), "enabled|1\n");
    online_submit(&g_app, "loop_lap", 42);
    CHECK_EQ_INT(g_app.lbn.outbox.count, 1);
    CHECK_STR_EQ(g_app.lbn.outbox.items[0].player, "Ana");
    CHECK(strncmp(first_line("outbox.txt"), "loop_lap|Ana|42|", 16) ==
          0); /* on disk before anything is sent */
    end_program();

    start_program(); /* the next run of the game */
    CHECK(!online_first_run(&g_app));
    CHECK(online_enabled(&g_app));
    CHECK_EQ_INT(g_app.lbn.outbox.count, 1);
    CHECK_EQ_INT(g_app.lbn.outbox.items[0].score, 42);
    end_program();
    forget_files();
}

static void test_turning_it_off_on_the_first_run_means_nothing_is_ever_sent(void) {
    forget_files();
    start_program();
    online_answer_first_run(&g_app, false);
    CHECK(!online_first_run(&g_app));
    CHECK(!online_enabled(&g_app));
    CHECK_STR_EQ(first_line("online.cfg"), "enabled|0\n");
    online_submit(&g_app, "loop_lap", 42);
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);
    CHECK_STR_EQ(first_line("outbox.txt"),
                 ""); /* nothing is waiting (turning it off stores an empty outbox) */
    end_program();

    start_program(); /* and it stays off: no notice, no scores queued */
    CHECK(!online_first_run(&g_app));
    CHECK(!online_enabled(&g_app));
    online_submit(&g_app, "loop_lap", 42);
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);
    end_program();
    forget_files();
}

static void test_closing_the_window_without_answering_asks_again_next_time(void) {
    forget_files();
    start_program();
    CHECK(online_first_run(&g_app));
    end_program(); /* the window was closed on the notice: no answer, so nothing was saved */
    CHECK(!file_exists("online.cfg"));
    start_program();
    CHECK(online_first_run(&g_app));
    CHECK(!online_enabled(&g_app));
    end_program();
    forget_files();
}

static void test_turning_it_off_later_forgets_what_was_waiting(void) {
    forget_files();
    start_program();
    online_answer_first_run(&g_app, true);
    online_submit(&g_app, "loop_lap", 42);
    online_submit(&g_app, "different_color", 7);
    CHECK_EQ_INT(g_app.lbn.outbox.count, 2);
    online_set_enabled(&g_app, false); /* the O key on the Online tab */
    CHECK(!online_enabled(&g_app));
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);    /* gone from memory ... */
    CHECK_STR_EQ(first_line("outbox.txt"), ""); /* ... and from the disk */
    CHECK_STR_EQ(first_line("online.cfg"), "enabled|0\n");
    online_submit(&g_app, "loop_lap", 99); /* turned off: a new run is not queued either */
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);
    end_program();

    start_program(); /* and it stays off, and nothing comes back, after a restart */
    CHECK(!online_enabled(&g_app));
    CHECK_EQ_INT(g_app.lbn.outbox.count, 0);
    end_program();
    forget_files();
}

static void test_a_damaged_or_foreign_config_means_off_and_never_asks(void) {
    for (const char **text = (const char *[]){"", "garbage\n", "enabled|2\n", "enabled|10\n", "enabled\n",
                                              "ENABLED|1\n", " enabled|1\n", "enabled|0\n", NULL};
         *text != NULL; text++) {
        forget_files();
        FILE *fp = fopen("online.cfg", "w");
        CHECK(fp != NULL);
        fputs(*text, fp);
        fclose(fp);
        start_program();
        /* A file that is there but not exactly "enabled|1" is off: a damaged file must never turn back on
         * what the player turned off. And a file means the player has already been asked. */
        CHECK(!online_enabled(&g_app));
        CHECK(!online_first_run(&g_app));
        end_program();
    }
    for (const char **text = (const char *[]){"enabled|1", "enabled|1\n", "enabled|1\r\n", NULL};
         *text != NULL; text++) {
        forget_files();
        FILE *fp = fopen("online.cfg", "w");
        CHECK(fp != NULL);
        fputs(*text, fp);
        fclose(fp);
        start_program();
        CHECK(online_enabled(&g_app)); /* exactly "enabled|1", with or without a line end, is on */
        end_program();
    }
    forget_files();
}

/* A fake menu: `n` games listed, row `selected` highlighted. Only game_count and selected are read. */
static void fake_menu(int n, int selected) {
    g_app.game_count = n;
    g_app.selected = selected;
}

static void test_the_leaderboard_opens_on_the_online_tab_when_online_scores_are_on(void) {
    forget_files();
    start_program();
    int tab = -1;
    /* Before the player has answered the first-run notice nothing is on, so nothing online is shown either.
     */
    fake_menu(5, 3);
    CHECK(!online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 0); /* the local "all games" tab */

    online_answer_first_run(&g_app, true);
    fake_menu(5, 3);
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 4); /* the game highlighted in the menu: tabs 1.. are games[0..] */
    fake_menu(5, 0);
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 1);
    fake_menu(5, 4);
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 5); /* the last game */
    fake_menu(5, 5);      /* a selection outside the list never points outside the tabs */
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 1);
    fake_menu(5, 9);
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 1);
    fake_menu(5, -1);
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 1);
    fake_menu(0, 0); /* no game at all: there is no table to show, the tab stays "all" */
    CHECK(online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 0);

    online_set_enabled(&g_app,
                       false); /* the player turned it off (O): the next visit opens on the local tab */
    fake_menu(5, 3);
    CHECK(!online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 0);
    end_program();

    start_program(); /* and after a restart too: the choice is remembered */
    fake_menu(5, 3);
    CHECK(!online_leaderboard_start(&g_app, &tab));
    CHECK_EQ_INT(tab, 0);
    end_program();
    forget_files();
}

static void test_the_server_address_is_never_blank(void) {
    start_program();
    CHECK_STR_EQ(online_server(), "http://127.0.0.1:9"); /* what this test was built with */
    end_program();
    forget_files();
}

int main(void) {
    RUN(test_on_by_default_but_nothing_moves_before_the_player_has_been_told);
    RUN(test_keeping_it_on_is_remembered_and_the_notice_is_not_shown_again);
    RUN(test_turning_it_off_on_the_first_run_means_nothing_is_ever_sent);
    RUN(test_closing_the_window_without_answering_asks_again_next_time);
    RUN(test_turning_it_off_later_forgets_what_was_waiting);
    RUN(test_a_damaged_or_foreign_config_means_off_and_never_asks);
    RUN(test_the_leaderboard_opens_on_the_online_tab_when_online_scores_are_on);
    RUN(test_the_server_address_is_never_blank);
    return TEST_SUMMARY();
}
