/* libcurl behind the LbnTransport interface. One request at a time; net_poll is called once per frame and
 * never waits: libcurl's multi interface does the connecting, the TLS handshake and the transfer in small
 * steps (the name lookup runs in a thread of libcurl, where the library was built that way). */
#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

#include "arcade/str.h"
#include "arcade/version.h"
#include "net.h"

#define NET_CONNECT_MS 4000L /* give up on connecting (name lookup + TCP + TLS) after this long ... */
#define NET_TOTAL_MS 8000L   /* ... and on the whole request after this long */

static CURLM *g_multi;
static CURL *g_easy; /* the request in flight; NULL when idle */
static struct curl_slist *g_headers;
static char g_base[256];
static bool g_tls;                /* the server address is https (else: http on this computer) */
static char g_post[LBN_BODY_MAX]; /* libcurl keeps a pointer to the POST body until the request ends */
static char g_answer[LBN_BODY_MAX];
static size_t g_answer_len;

/* libcurl hands the answer over in pieces. An answer bigger than ours is not an answer of this server:
 * returning 0 aborts the transfer, which comes out as "no answer" (status 0). */
static size_t on_data(char *chunk, size_t size, size_t count, void *unused) {
    (void)unused;
    size_t n = size * count;
    if (g_answer_len + n >= sizeof(g_answer))
        return 0;
    memcpy(g_answer + g_answer_len, chunk, n);
    g_answer_len += n;
    g_answer[g_answer_len] = '\0';
    return n;
}

bool net_open(const char *base_url) {
    if (g_multi != NULL)
        return true;
    if (!lbn_valid_base_url(base_url)) {
        fprintf(stderr,
                "online: refusing the server address \"%s\" (it must be https://host, or http://localhost)\n",
                base_url);
        return false;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "online: libcurl could not start\n");
        return false;
    }
    g_multi = curl_multi_init();
    if (g_multi == NULL) {
        curl_global_cleanup();
        return false;
    }
    arc_strlcpy(g_base, base_url, sizeof(g_base));
    g_tls = strncmp(base_url, "https://", 8) == 0;
    return true;
}

/* Ends the request in flight, if any, and frees what it used. */
static void release(void) {
    if (g_easy != NULL) {
        curl_multi_remove_handle(g_multi, g_easy);
        curl_easy_cleanup(g_easy);
        g_easy = NULL;
    }
    curl_slist_free_all(g_headers);
    g_headers = NULL;
}

void net_close(void) {
    if (g_multi == NULL)
        return;
    release();
    curl_multi_cleanup(g_multi);
    g_multi = NULL;
    curl_global_cleanup();
}

/* Only the protocol of the address is allowed, whatever the server answers. */
static void limit_protocols(CURL *h) {
#if LIBCURL_VERSION_NUM >= 0x075500 /* 7.85.0 */
    curl_easy_setopt(h, CURLOPT_PROTOCOLS_STR, g_tls ? "https" : "http");
#else
    curl_easy_setopt(h, CURLOPT_PROTOCOLS, g_tls ? (long)CURLPROTO_HTTPS : (long)CURLPROTO_HTTP);
#endif
}

static bool net_start(void *ctx, const char *method, const char *path, const char *token, const char *body) {
    (void)ctx;
    if (g_multi == NULL || g_easy != NULL)
        return false;
    char url[sizeof(g_base) + LBN_PATH_MAX];
    int n = snprintf(url, sizeof(url), "%s%s", g_base, path);
    if (n < 0 || (size_t)n >= sizeof(url))
        return false;
    g_easy = curl_easy_init();
    if (g_easy == NULL)
        return false;
    g_answer_len = 0;
    g_answer[0] = '\0';

    g_headers = curl_slist_append(NULL, "Content-Type: text/plain; charset=utf-8");
    if (g_headers != NULL)
        g_headers = curl_slist_append(g_headers, "Expect:"); /* no "100 Continue" round trip */
    if (g_headers != NULL &&
        token != NULL) { /* a token is only A-Z a-z 0-9 - _ (lbn_valid_token): no header trick */
        char line[LBN_TOKEN_MAX + 32];
        snprintf(line, sizeof(line), "Authorization: Bearer %s", token);
        g_headers = curl_slist_append(g_headers, line);
    }
    if (g_headers == NULL) {
        release();
        return false;
    }

    curl_easy_setopt(g_easy, CURLOPT_URL, url);
    limit_protocols(g_easy);
    curl_easy_setopt(g_easy, CURLOPT_FOLLOWLOCATION,
                     0L); /* a redirect is a hotspot's login page: not our answer */
    curl_easy_setopt(g_easy, CURLOPT_HTTPHEADER, g_headers);
    curl_easy_setopt(g_easy, CURLOPT_USERAGENT, "arCade/" ARCADE_VERSION_STRING);
    curl_easy_setopt(g_easy, CURLOPT_CONNECTTIMEOUT_MS, NET_CONNECT_MS);
    curl_easy_setopt(g_easy, CURLOPT_TIMEOUT_MS, NET_TOTAL_MS);
    curl_easy_setopt(g_easy, CURLOPT_NOSIGNAL, 1L); /* no signals: the timeouts work in any thread setup */
    curl_easy_setopt(g_easy, CURLOPT_WRITEFUNCTION, on_data);
    if (strcmp(method, "POST") == 0) {
        arc_strlcpy(g_post, body, sizeof(g_post));
        curl_easy_setopt(g_easy, CURLOPT_POST, 1L);
        curl_easy_setopt(g_easy, CURLOPT_POSTFIELDS, g_post);
        curl_easy_setopt(g_easy, CURLOPT_POSTFIELDSIZE, (long)strlen(g_post));
    } else {
        curl_easy_setopt(g_easy, CURLOPT_HTTPGET, 1L);
    }
    if (curl_multi_add_handle(g_multi, g_easy) != CURLM_OK) {
        release();
        return false;
    }
    return true;
}

static bool net_poll(void *ctx, LbnReply *reply) {
    (void)ctx;
    reply->status = 0;
    reply->body[0] = '\0';
    if (g_easy == NULL) /* nothing is running: report "no answer" rather than wait for ever */
        return true;
    int running = 0;
    if (curl_multi_perform(g_multi, &running) == CURLM_OK && running > 0)
        return false; /* still on its way */

    CURLcode result = CURLE_FAILED_INIT;
    int queued = 0;
    for (CURLMsg *m = curl_multi_info_read(g_multi, &queued); m != NULL;
         m = curl_multi_info_read(g_multi, &queued))
        if (m->msg == CURLMSG_DONE && m->easy_handle == g_easy)
            result = m->data.result;
    if (result == CURLE_OK) { /* no answer at all (connection, TLS or timeout error) stays at status 0 */
        long code = 0;
        curl_easy_getinfo(g_easy, CURLINFO_RESPONSE_CODE, &code);
        reply->status = (int)code;
        arc_strlcpy(reply->body, g_answer, sizeof(reply->body));
    }
    release();
    return true;
}

LbnTransport net_transport(void) {
    LbnTransport t = {net_start, net_poll, NULL};
    return t;
}
