/* The hub's side of the online leaderboard: the player's choice, where the files live, and the once-per-frame
 * tick. The protocol, the outbox and the retries are in core/include/arcade/lbnet.h; the network is
 * app/net.h. Without libcurl in the build (ARCADE_HAVE_CURL not set) everything here is a quiet no-op and
 * online_ready() stays false, so the Online tab explains that this build has no online support.
 *
 * Online scores are ON by default. To keep that honest, the first time the game runs (there is no online.cfg
 * yet) the player is told what is shared and where it goes, and can turn it off BEFORE anything is sent:
 * until online_answer_first_run() has been called, online_enabled() is false and nothing is queued or sent.
 */
#ifndef ARCADE_ONLINE_H
#define ARCADE_ONLINE_H

#include "app.h"

/* Once, after app->data_dir is known: file paths, the transport, the saved choice. The address is
 * $ARCADE_ONLINE_URL or, when that is not set, the one built in. */
void online_init(AppState *app);
void online_shutdown(AppState *app);

bool online_ready(const AppState *app); /* this build can reach a server */
bool online_enabled(
    const AppState *app);        /* ... and online scores are on (and the first-run notice is done) */
const char *online_server(void); /* the address the player is told about (never blank) */
/* The player's choice, saved in online.cfg. Turning it off also forgets every score still waiting to be sent.
 */
void online_set_enabled(AppState *app, bool on);

/* True until the player has answered the first-run notice (no online.cfg yet, and a build that can go
 * online). */
bool online_first_run(const AppState *app);
/* The answer: keep it on, or turn it off. Saved, so the notice is not shown again. */
void online_answer_first_run(AppState *app, bool keep_on);

/* Where the leaderboard screen opens. With online scores on it opens on the Online tab, on the game that is
 * selected in the menu (that is also the game just played when it comes from the result screen: tables are
 * per game). Otherwise (turned off, the first-run notice not answered yet, or a build without libcurl) it
 * opens on the local tab, "all games". Returns true for the Online tab; *tab is the screen's tab number (0 =
 * all games, 1.. = app->games[*tab - 1]). */
bool online_leaderboard_start(const AppState *app, int *tab);

void online_tick(AppState *app); /* once per hub frame; never blocks */
void online_submit(AppState *app, const char *table,
                   int score);                         /* a ranked run finished (player = app->player) */
void online_want(AppState *app, const char *table);    /* the Online tab shows this table */
void online_refresh(AppState *app, const char *table); /* ... and the player pressed refresh */

#endif
