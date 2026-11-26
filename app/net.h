/* The network side of the online leaderboard: an LbnTransport (core/include/arcade/lbnet.h) over libcurl.
 * Compiled only when libcurl is found (ARCADE_HAVE_CURL, see app/CMakeLists.txt); it is the one file of the
 * project that talks to the network, and the rest of the hub never includes <curl/curl.h>. */
#ifndef ARCADE_NET_H
#define ARCADE_NET_H

#include <stdbool.h>

#include "arcade/lbnet.h"

/* Starts libcurl for one server. base_url is "https://host[:port]" with nothing after it; plain http is only
 * accepted for localhost and 127.0.0.1 (lbn_valid_base_url), so a real server is never reached without TLS.
 * Returns false, after a message on stderr, for a refused address or when libcurl cannot start. */
bool net_open(const char *base_url);
void net_close(void);
/* The transport for lbn_init: one request at a time, never blocks (the multi interface), follows no
 * redirects, and gives up by itself after a few seconds with status 0. */
LbnTransport net_transport(void);

#endif
