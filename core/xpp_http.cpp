/* The browser front end's HTTP server, inside xppautX (xpp_http.h).
   Serve the compiled-in page (web2/dist),
   stream protocol events to it (Server-Sent Events), take its commands by
   POST, and replay what a page that (re)connects needs to draw.

   Threads: the core runs on the main thread and calls xpp_http_emit; one
   thread accepts and answers HTTP requests and pushes the page's commands
   into the inbox (xpp_inbox.h), where the core takes them; one thread
   copies what xppaut prints to the terminal and into the page's log. The
   model's folder is served as /files (xpp_files.h: listing, reading, and
   uploads streamed to a temporary file). This file includes no core header
   but those small C APIs (xpp_mem.h, xpp_inbox.h, xpp_files.h, xpp_log.h),
   so the socket and Windows headers cannot clash with core names. */
/* macOS hides the BSD names (INADDR_LOOPBACK) under _XOPEN_SOURCE=600;
   this must come before any system header. */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#include "xpp_http.h"
#include "xpp_mem.h"
#include "xpp_inbox.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
typedef SOCKET sock_t;
#define close_sock closesocket
#define dup _dup
#define dup2 _dup2
#define write _write
#define read _read
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define close_sock close
#endif

#ifdef _WIN32
extern "C" errno_t rand_s(unsigned int *); /* the C library's; stdlib.h declares it only with _CRT_RAND_S */
#endif

/* BSD and macOS have no MSG_NOSIGNAL; SIGPIPE is ignored in xpp_http_start */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

typedef struct {
    const char *path, *type;
    const unsigned char *data;
    size_t len;
} XppWebAsset;
extern "C" const XppWebAsset xpp_web_assets[]; /* web_assets.c (C): web2/dist/ at / */

#define MAX_CLIENTS 16
#define MAX_WINDOWS 32
#define LOG_KEEP 100000

static int active;
static char token[40];
static sock_t listener = INVALID_SOCKET;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t watchdog_thread;
static pthread_t http_thread, log_thread;

/* event streams and what a new one gets first */
static sock_t clients[MAX_CLIENTS];
static int nclients;

/* stream i is gone (the lock is held) */
static void drop_client(int i)
{
    close_sock(clients[i]);
    clients[i] = clients[nclients - 1];
    nclients--;
}
static char *sticky_hello, *sticky_state, *sticky_ask, *exit_event;
static struct {
    int win;
    char *line;
} windows[MAX_WINDOWS];
static char *log_text; /* the last LOG_KEEP bytes printed */
static size_t log_len;
static int saw_bye, orig_stderr = -1;

static char *copy_line(const char *s, size_t n)
{
    char *c = static_cast<char *>(xpp_malloc(n + 1));
    memcpy(c, s, n);
    c[n] = 0;
    return c;
}

static void set_sticky(char **slot, const char *s, size_t n)
{
    xpp_free(*slot);
    *slot = s ? copy_line(s, n) : NULL;
}

static int send_all(sock_t s, const char *p, size_t n)
{
    while (n > 0) {
#ifdef _WIN32
        int r = send(s, p, (int)(n > 65536 ? 65536 : n), 0);
#else
        ssize_t r = send(s, p, n, MSG_NOSIGNAL);
#endif
        if (r <= 0) return 0;
        p += r;
        n -= (size_t)r;
    }
    return 1;
}

static int send_event(sock_t s, const char *line, size_t n)
{
    return send_all(s, "data: ", 6) && send_all(s, line, n) && send_all(s, "\n\n", 2);
}

/* Closing the page used to leave the program running with its port held and
   nothing to talk to, which for `xppautX model.ode` is a process the user
   never sees again. The watchdog ends it once the last page has been gone a
   while. A closed tab is not noticed until a write to it fails, and an idle
   session writes nothing, so it also sends an SSE comment as a heartbeat.

   The wait matters: a reload or a navigation drops the connection for a
   moment and the page comes back, and serve_events() then pushes a redraw.
   Only a session that has had a page at all can time out, so a slow browser
   start is not mistaken for a closed one. */
#define ALONE_SECONDS 10

static int had_client;
static time_t alone_since;

static void *watchdog_main(void *arg)
{
    int i;
    (void)arg;
    for (;;) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
        pthread_mutex_lock(&lock);
        for (i = 0; i < nclients; i++) {
            if (!send_all(clients[i], ":\n\n", 3)) drop_client(i--); /* a comment: the page ignores it */
        }
        if (nclients > 0) alone_since = 0;
        else if (had_client && !alone_since) alone_since = time(NULL);
        if (had_client && nclients == 0 && alone_since
            && time(NULL) - alone_since >= ALONE_SECONDS) {
            pthread_mutex_unlock(&lock);
            /* exit() would run at_exit(), which tells the page the program is
               going and waits on the same lock from this thread: it hangs, and
               there is no page left to tell anyway. Flush what the core wrote,
               then go. */
            fflush(NULL);
            _exit(0);
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* the value of "key":"..." or "key":number in a flat event line */
static int field(const char *line, size_t n, const char *key, char *out, size_t max)
{
    char pat[32];
    const char *p, *end = line + n;
    size_t k = 0;
    snprintf(pat, sizeof pat, "\"%s\":", key);
    p = strstr(line, pat); /* the events put these keys first, before any user text */
    if (!p || p >= end) return 0;
    p += strlen(pat);
    if (*p == '"') p++;
    while (p < end && *p != '"' && *p != ',' && *p != '}' && k + 1 < max) out[k++] = *p++;
    out[k] = 0;
    return 1;
}

/* ---- the core's side ---------------------------------------------------------- */

int xpp_http_active(void) { return active; }

void xpp_http_emit(const char *line, size_t n)
{
    char ev[16], op[16], win[16];
    int i;
    pthread_mutex_lock(&lock);
    if (field(line, n > 40 ? 40 : n, "ev", ev, sizeof ev)) {
        if (strcmp(ev, "hello") == 0) set_sticky(&sticky_hello, line, n);
        else if (strcmp(ev, "state") == 0) set_sticky(&sticky_state, line, n);
        else if (strcmp(ev, "ask") == 0) set_sticky(&sticky_ask, line, n);
        else if (strcmp(ev, "idle") == 0) set_sticky(&sticky_ask, NULL, 0);
        else if (strcmp(ev, "bye") == 0) saw_bye = 1;
        else if (strcmp(ev, "window") == 0 && field(line, n, "op", op, sizeof op) && field(line, n, "win", win, sizeof win)) {
            int w = atoi(win), slot = -1;
            for (i = 0; i < MAX_WINDOWS; i++)
                if (windows[i].line && windows[i].win == w) slot = i;
            if (strcmp(op, "create") == 0) {
                for (i = 0; slot < 0 && i < MAX_WINDOWS; i++)
                    if (!windows[i].line) slot = i;
                if (slot >= 0) {
                    windows[slot].win = w;
                    set_sticky(&windows[slot].line, line, n);
                }
            } else if (strcmp(op, "destroy") == 0 && slot >= 0) {
                set_sticky(&windows[slot].line, NULL, 0);
            }
        }
    }
    for (i = 0; i < nclients; i++)
        if (!send_event(clients[i], line, n)) drop_client(i--);
    pthread_mutex_unlock(&lock);
}

/* a command from the page, into the inbox. Trailing blanks go and an empty
   body is ignored; a body of several lines gives several lines (a CR before
   a newline dropped), as when the core split this text itself. Called with
   the lock held: the inbox's locks nest inside it, never the other way. */
static void push_command(const char *s, size_t n)
{
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) n--;
    while (n > 0) {
        const char *nl = static_cast<const char *>(memchr(s, '\n', n));
        size_t k = nl ? (size_t)(nl - s) : n;
        xpp_inbox_push(s, k > 0 && s[k - 1] == '\r' ? k - 1 : k);
        if (!nl) break;
        s += k + 1;
        n -= k + 1;
    }
}

/* ---- what xppaut prints ----------------------------------------------------------- */

/* {"ev":"log","text":"..."} for n bytes of printed text; malloc'd, length in *len */
static char *log_event(const char *text, size_t n, size_t *len)
{
    char *line = static_cast<char *>(xpp_malloc(6 * n + 32));
    size_t i, k = (size_t)sprintf(line, "{\"ev\":\"log\",\"text\":\"");
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '"' || c == '\\') {
            line[k++] = '\\';
            line[k++] = (char)c;
        } else if (c == '\n') {
            line[k++] = '\\';
            line[k++] = 'n';
        } else if (c < 0x20 || c >= 0x80) {
            k += (size_t)sprintf(line + k, "\\u%04x", c);
        } else line[k++] = (char)c;
    }
    k += (size_t)sprintf(line + k, "\"}");
    *len = k;
    return line;
}

static void *log_main(void *arg)
{
    int fd = *(int *)arg;
    char chunk[4096];
    for (;;) {
        size_t k;
        char *line;
        int r = read(fd, chunk, sizeof chunk);
        if (r <= 0) break;
        if (orig_stderr >= 0 && write(orig_stderr, chunk, (unsigned)r) < 0) orig_stderr = -1;
        pthread_mutex_lock(&lock);
        log_text = static_cast<char *>(xpp_realloc(log_text, log_len + (size_t)r));
        memcpy(log_text + log_len, chunk, (size_t)r);
        log_len += (size_t)r;
        if (log_len > LOG_KEEP) { /* keep the last lines */
            size_t cut = log_len - LOG_KEEP;
            while (cut < log_len && log_text[cut - 1] != '\n') cut++;
            memmove(log_text, log_text + cut, log_len - cut);
            log_len -= cut;
        }
        pthread_mutex_unlock(&lock);
        line = log_event(chunk, (size_t)r, &k);
        xpp_http_emit(line, k);
        xpp_free(line);
    }
    return NULL;
}

/* ---- HTTP ---------------------------------------------------------------------------- */

static void reply(sock_t s, const char *status, const char *type, const unsigned char *body, size_t len)
{
    char head[256];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nCache-Control: no-store\r\n"
                     "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n", status, type, (unsigned long)len);
    if (send_all(s, head, (size_t)n) && len) send_all(s, (const char *)body, len);
}

static void reply_text(sock_t s, const char *status, const char *text)
{
    reply(s, status, "text/plain", (const unsigned char *)text, strlen(text));
}

/* the query's t= is the token, compared in full and in constant time */
static int token_ok(const char *target)
{
    size_t n = strlen(token), i;
    const char *q = strchr(target, '?');
    for (; q; q = strchr(q + 1, '&')) {
        const char *v = q + 1;
        unsigned diff = 0;
        if (strncmp(v, "t=", 2) != 0) continue;
        v += 2;
        if (n == 0 || strcspn(v, "&") != n) continue;
        for (i = 0; i < n; i++) diff |= (unsigned char)(v[i] ^ token[i]);
        if (!diff) return 1;
    }
    return 0;
}

static void open_events(sock_t s)
{
    static const char head[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n\r\n";
    int i, ok;
    pthread_mutex_lock(&lock);
    ok = send_all(s, head, sizeof head - 1);
    if (ok && log_len) {
        size_t k;
        char *line = log_event(log_text, log_len, &k);
        ok = send_event(s, line, k);
        xpp_free(line);
    }
    if (ok && sticky_hello) ok = send_event(s, sticky_hello, strlen(sticky_hello));
    for (i = 0; ok && i < MAX_WINDOWS; i++)
        if (windows[i].line) ok = send_event(s, windows[i].line, strlen(windows[i].line));
    if (ok && sticky_state) ok = send_event(s, sticky_state, strlen(sticky_state));
    if (ok && sticky_ask) ok = send_event(s, sticky_ask, strlen(sticky_ask));
    if (ok && exit_event) ok = send_event(s, exit_event, strlen(exit_event));
    if (ok && nclients < MAX_CLIENTS) {
        had_client = 1;
        alone_since = 0;
        clients[nclients++] = s;
        /* the page draws from scratch; a redraw would wait behind an open prompt */
        if (sticky_hello && !sticky_ask && !exit_event) push_command("{\"cmd\":\"redraw\"}", 16);
    } else close_sock(s);
    pthread_mutex_unlock(&lock);
}

/* ---- a request: its head, then a body read as it comes -------------------------------- */

#define HEAD_MAX 8192       /* request line and headers */
#define CMD_MAX (1UL << 20) /* a POST /cmd body */
#define RECV_SECONDS 30     /* a client that stops sending mid-request is dropped */
#define CHUNK 65536

typedef struct {
    sock_t s;
    char head[HEAD_MAX + 1];
    char method[8], target[1024];
    size_t head_len;   /* the head, up to and with its blank line */
    const char *body0; /* body bytes that arrived with the head */
    size_t have;       /* how many */
    int has_length;    /* a Content-Length was sent */
    unsigned long long length;
    unsigned long long consumed; /* body bytes read so far */
} Request;

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* header `name` (lower case) of the head: its value, blanks trimmed */
static int header(const Request *q, const char *name, char *out, size_t max)
{
    size_t n = strlen(name), i;
    const char *p = q->head, *end = q->head + q->head_len;
    while ((p = static_cast<const char *>(memchr(p, '\n', (size_t)(end - p)))) != NULL) {
        size_t k = 0;
        p++;
        if ((size_t)(end - p) <= n || p[n] != ':') continue;
        for (i = 0; i < n && lower((unsigned char)p[i]) == name[i]; i++) {}
        if (i < n) continue;
        p += n + 1;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        while (p < end && *p != '\r' && *p != '\n' && k + 1 < max) out[k++] = *p++;
        while (k > 0 && (out[k - 1] == ' ' || out[k - 1] == '\t')) k--;
        out[k] = 0;
        return 1;
    }
    return 0;
}

/* reads the head; 0 when the connection is not a request worth an answer */
static int read_head(Request *q)
{
    size_t got = 0, i = 0;
    char v[32];
    while (got < HEAD_MAX) {
        int r = recv(q->s, q->head + got, (int)(HEAD_MAX - got), 0);
        if (r <= 0) return 0;
        got += (size_t)r;
        for (i = got >= (size_t)r + 3 ? got - (size_t)r - 3 : 0; i + 4 <= got; i++)
            if (memcmp(q->head + i, "\r\n\r\n", 4) == 0) break;
        if (i + 4 <= got) {
            q->head_len = i + 4;
            break;
        }
    }
    if (!q->head_len) return 0;
    q->body0 = q->head + q->head_len;
    q->have = got - q->head_len;
    q->head[q->head_len - 2] = 0; /* the head as a string, for sscanf (the body starts after it) */
    if (sscanf(q->head, "%7s %1023s", q->method, q->target) != 2) return 0;
    q->has_length = header(q, "content-length", v, sizeof v);
    if (q->has_length) {
        if (!v[0] || strspn(v, "0123456789") != strlen(v) || strlen(v) > 18) q->length = ~0ULL;
        else q->length = strtoull(v, NULL, 10);
        q->consumed = q->have < q->length ? q->have : q->length;
    }
    return 1;
}

/* up to n more bytes of the body */
static int take(Request *q, char *buf, size_t n)
{
    int r = recv(q->s, buf, (int)n, 0);
    if (r > 0) q->consumed += (unsigned long long)r;
    return r;
}

/* the query-less path of the target */
static size_t path_len(const char *target) { return strcspn(target, "?"); }

/* a %-encoded name; 0 for a bad escape, a NUL or no room */
static int url_decode(const char *s, size_t n, char *out, size_t max)
{
    size_t i, k = 0;
    for (i = 0; i < n; i++) {
        int c = (unsigned char)s[i];
        if (c == '%') {
            int h = 0, j;
            for (j = 1; j <= 2; j++) {
                int d;
                if (i + (size_t)j >= n) return 0;
                d = lower((unsigned char)s[i + (size_t)j]);
                d = d >= '0' && d <= '9' ? d - '0' : d >= 'a' && d <= 'f' ? d - 'a' + 10 : -1;
                if (d < 0) return 0;
                h = h * 16 + d;
            }
            if (h == 0) return 0;
            c = h;
            i += 2;
        }
        if (k + 1 >= max) return 0;
        out[k++] = (char)c;
    }
    out[k] = 0;
    return 1;
}

/* POST /cmd: the whole body, then into the inbox */
static void serve_cmd(Request *q)
{
    unsigned long long n = q->has_length ? q->length : q->have;
    char *body;
    size_t got;
    if (!token_ok(q->target)) {
        reply_text(q->s, "403 Forbidden", "bad token");
        return;
    }
    if (n > CMD_MAX) {
        reply_text(q->s, "413 Payload Too Large", "command too long");
        return;
    }
    body = static_cast<char *>(xpp_malloc((size_t)n + 1));
    got = q->have < n ? q->have : (size_t)n;
    memcpy(body, q->body0, got);
    while (got < n) {
        int r = take(q, body + got, (size_t)n - got);
        if (r <= 0) break;
        got += (size_t)r;
    }
    body[got] = 0;
    if (got < n) {
        reply_text(q->s, "400 Bad Request", "incomplete body");
    } else {
        pthread_mutex_lock(&lock);
        if (strstr(body, "\"cmd\":\"answer\"")) set_sticky(&sticky_ask, NULL, 0);
        if (!exit_event) push_command(body, strlen(body));
        pthread_mutex_unlock(&lock);
        reply(q->s, "204 No Content", "text/plain", NULL, 0);
    }
    xpp_free(body);
}

static const char *files_status(int st)
{
    switch (st) {
    case XPP_FILES_BAD_NAME: return "400 Bad Request";
    case XPP_FILES_NOT_FOUND: return "404 Not Found";
    case XPP_FILES_REFUSED: return "403 Forbidden";
    case XPP_FILES_TOO_LARGE: return "413 Payload Too Large";
    default: return "500 Internal Server Error";
    }
}

static void get_file(sock_t s, const char *name)
{
    FILE *fp;
    unsigned long long size;
    char head[256], *buf;
    size_t n;
    int ok, st = xpp_files_open(name, &fp, &size);
    if (st != XPP_FILES_OK) {
        reply_text(s, files_status(st), xpp_files_status_text(st));
        return;
    }
    n = (size_t)snprintf(head, sizeof head,
                         "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %llu\r\n"
                         "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
                         size);
    ok = send_all(s, head, n);
    buf = static_cast<char *>(xpp_malloc(CHUNK));
    while (ok && (n = fread(buf, 1, CHUNK, fp)) > 0) ok = send_all(s, buf, n);
    xpp_free(buf);
    fclose(fp);
}

/* PUT /files/NAME: the body streams into a temporary file that becomes
   NAME only once all of it arrived (xpp_files.h) */
static void put_file(Request *q, const char *name)
{
    XppFilePut *put;
    unsigned long long left, size;
    char sha[65], v[32], *buf;
    int st;
    if (!q->has_length) {
        reply_text(q->s, "411 Length Required", "a Content-Length is required");
        return;
    }
    if (q->length > XPP_FILES_CAP) { /* refused before a byte of the body is read */
        reply_text(q->s, "413 Payload Too Large", xpp_files_status_text(XPP_FILES_TOO_LARGE));
        return;
    }
    st = xpp_files_put_begin(name, XPP_FILES_CAP, &put);
    if (st != XPP_FILES_OK) {
        reply_text(q->s, files_status(st), xpp_files_status_text(st));
        return;
    }
    if (header(q, "expect", v, sizeof v) && strcmp(v, "100-continue") == 0)
        send_all(q->s, "HTTP/1.1 100 Continue\r\n\r\n", 25);
    left = q->length;
    {
        size_t first = q->have < left ? q->have : (size_t)left;
        st = xpp_files_put_write(put, q->body0, first);
        left -= first;
    }
    buf = static_cast<char *>(xpp_malloc(CHUNK));
    while (st == XPP_FILES_OK && left > 0) {
        int r = take(q, buf, (size_t)(left < CHUNK ? left : CHUNK));
        if (r <= 0) break; /* cut short, or the client stopped sending */
        st = xpp_files_put_write(put, buf, (size_t)r);
        left -= (unsigned long long)r;
    }
    xpp_free(buf);
    if (st != XPP_FILES_OK || left > 0) {
        xpp_files_put_abort(put);
        if (st != XPP_FILES_OK) reply_text(q->s, files_status(st), xpp_files_status_text(st));
        else reply_text(q->s, "400 Bad Request", "incomplete body");
        return;
    }
    st = xpp_files_put_commit(put, &size, sha);
    if (st != XPP_FILES_OK) {
        reply_text(q->s, files_status(st), xpp_files_status_text(st));
        return;
    }
    {
        /* the name passed xpp_files_name_ok: no quote, backslash or control character */
        size_t n = strlen(name) + 160;
        char *json = static_cast<char *>(xpp_malloc(n));
        snprintf(json, n, "{\"name\":\"%s\",\"size\":%llu,\"sha256\":\"%s\"}", name, size, sha);
        reply(q->s, "200 OK", "application/json", (const unsigned char *)json, strlen(json));
        xpp_free(json);
    }
}

/* /files (the listing), /files/NAME (GET, PUT): docs/protocol.md "Files" */
static void serve_files(Request *q)
{
    size_t n = path_len(q->target);
    char name[1024];
    if (!token_ok(q->target)) {
        reply_text(q->s, "403 Forbidden", "bad token");
        return;
    }
    if (n == 6 || (n == 7 && q->target[6] == '/')) {
        if (strcmp(q->method, "GET") == 0) {
            size_t len;
            char *json = xpp_files_list_json(&len);
            reply(q->s, "200 OK", "application/json", (const unsigned char *)json, len);
            xpp_free(json);
        } else reply_text(q->s, "405 Method Not Allowed", "GET only");
        return;
    }
    if (!url_decode(q->target + 7, n - 7, name, sizeof name) || !xpp_files_name_ok(name)) {
        reply_text(q->s, files_status(XPP_FILES_BAD_NAME), xpp_files_status_text(XPP_FILES_BAD_NAME));
        return;
    }
    if (strcmp(q->method, "GET") == 0) get_file(q->s, name);
    else if (strcmp(q->method, "PUT") == 0) put_file(q, name);
    else reply_text(q->s, "405 Method Not Allowed", "GET or PUT");
}

/* a bookmark of an old path, /v2/ (web2 moved to / at T17) or /v1/ (the
   classic page, removed at T18): redirect it to the same path under /,
   keeping the query string (the token). */
static void redirect(sock_t s, const char *location)
{
    char head[700]; /* the location (under 560, see its caller) and the fixed lines */
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\nCache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n", location);
    if (n < 0) return;
    send_all(s, head, n < (int)sizeof head ? (size_t)n : sizeof head - 1); /* cut, never past the buffer */
}

static void serve_asset(Request *q)
{
    char path[512];
    const XppWebAsset *a;
    int i;
    for (i = 0; q->target[i] && q->target[i] != '?' && i < (int)sizeof path - 1; i++) path[i] = q->target[i];
    path[i] = 0;
    if (strcmp(path, "/v1") == 0 || strncmp(path, "/v1/", 4) == 0 || strcmp(path, "/v2") == 0 ||
        strncmp(path, "/v2/", 4) == 0) {
        char location[560];
        const char *rest = path[3] == '/' ? path + 4 : "";
        const char *query = strchr(q->target, '?');
        snprintf(location, sizeof location, "/%s%s", rest, query ? query : "");
        redirect(q->s, location);
        return;
    }
    if (strcmp(path, "/index.html") == 0) strcpy(path, "/");
    for (a = xpp_web_assets; a->path; a++)
        if (strcmp(a->path, path) == 0) break;
    if (a->path) reply(q->s, "200 OK", a->type, a->data, a->len);
    else reply_text(q->s, "404 Not Found", "not found");
}

/* a client that stops sending (a stalled upload) must not hold the one
   thread that answers requests */
static void set_recv_timeout(sock_t s, int seconds)
{
#ifdef _WIN32
    DWORD ms = (DWORD)seconds * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof ms);
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
#endif
}

/* A request refused before its body was read (a bad token, a bad name)
   still has the body coming: closing on unread data resets the connection,
   and the client may lose the answer. A small rest is read and dropped
   first; a large one (an upload over the cap) is not waited for. */
#define DRAIN_MAX (1ULL << 20)
static void drain(Request *q)
{
    char buf[4096];
    unsigned long long left;
    if (!q->has_length || q->length == ~0ULL || q->consumed >= q->length) return;
    left = q->length - q->consumed;
    if (left > DRAIN_MAX) return;
    set_recv_timeout(q->s, 2);
    while (left > 0) {
        int r = take(q, buf, left < sizeof buf ? (size_t)left : sizeof buf);
        if (r <= 0) break;
        left -= (unsigned long long)r;
    }
}

static void handle(sock_t s)
{
    Request *q = static_cast<Request *>(xpp_calloc(1, sizeof *q));
    size_t n;
    q->s = s;
    if (!read_head(q)) {
        close_sock(s);
        xpp_free(q);
        return;
    }
    n = path_len(q->target);
    if (strlen(q->target) >= sizeof q->target - 1) {
        reply_text(s, "414 URI Too Long", "address too long");
    } else if (q->has_length && q->length == ~0ULL) {
        reply_text(s, "400 Bad Request", "bad Content-Length");
    } else if (strcmp(q->method, "GET") == 0 && strncmp(q->target, "/events", 7) == 0) {
        if (token_ok(q->target)) {
            open_events(s);
            xpp_free(q);
            return;
        }
        reply_text(s, "403 Forbidden", "bad token");
    } else if (strcmp(q->method, "POST") == 0 && strncmp(q->target, "/cmd", 4) == 0) {
        serve_cmd(q);
    } else if (n >= 6 && strncmp(q->target, "/files", 6) == 0 && (n == 6 || q->target[6] == '/')) {
        serve_files(q);
    } else if (strcmp(q->method, "GET") == 0) {
        serve_asset(q);
    } else reply(s, "405 Method Not Allowed", "text/plain", NULL, 0);
    drain(q);
    close_sock(s);
    xpp_free(q);
}

static void *http_main(void *arg)
{
    (void)arg;
    for (;;) {
        sock_t s = accept(listener, NULL, NULL);
        if (s == INVALID_SOCKET) continue;
        set_recv_timeout(s, RECV_SECONDS);
        handle(s);
    }
    return NULL;
}

/* the model stopped: say so in the page. After an error (no bye) keep
   serving so the page can show what xppaut printed. */
static void at_exit(void)
{
    char line[64];
    fflush(stdout);
    fflush(stderr);
#ifdef _WIN32
    Sleep(200);
#else
    usleep(200000);
#endif
    snprintf(line, sizeof line, "{\"ev\":\"exit\",\"code\":%d}", saw_bye ? 0 : 1);
    pthread_mutex_lock(&lock);
    set_sticky(&exit_event, line, strlen(line));
    pthread_mutex_unlock(&lock);
    xpp_http_emit(line, strlen(line));
    if (saw_bye) return;
    if (orig_stderr >= 0) {
        static const char msg[] = "xppautX: the model stopped; the page shows what it printed. Ctrl+C quits.\n";
        if (write(orig_stderr, msg, sizeof msg - 1) < 0) orig_stderr = -1;
    }
    pthread_join(http_thread, NULL); /* until Ctrl+C */
}

static void make_token(void)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char bytes[16];
    int i;
#ifdef _WIN32
    for (i = 0; i < 16; i++) {
        unsigned int v = 0;
        rand_s(&v);
        bytes[i] = (unsigned char)v;
    }
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(bytes, 1, 16, f) != 16) {
        srand((unsigned)time(NULL) ^ (unsigned)getpid());
        for (i = 0; i < 16; i++) bytes[i] = (unsigned char)rand();
    }
    if (f) fclose(f);
#endif
    for (i = 0; i < 16; i++) {
        token[2 * i] = hex[bytes[i] >> 4];
        token[2 * i + 1] = hex[bytes[i] & 15];
    }
    token[32] = 0;
}

static void open_in_browser(const char *url)
{
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    char cmd[600];
    const char *opener = "xdg-open";
#ifdef __APPLE__
    opener = "open";
#endif
    if (getenv("WSL_DISTRO_NAME")) opener = "cmd.exe /c start";
    snprintf(cmd, sizeof cmd, "%s '%s' >/dev/null 2>&1 &", opener, url);
    if (system(cmd) != 0) xpp_log(XPP_LOG_WARN, "open %s in a browser\n", url);
#endif
}

static int listen_on(int port)
{
    struct sockaddr_in addr;
    int one = 1;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return -1;
#ifndef _WIN32
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
#else
    (void)one;
#endif
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* this machine only */
    addr.sin_port = htons((unsigned short)port);
    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(listener, 16) != 0) {
        close_sock(listener);
        listener = INVALID_SOCKET;
        return -1;
    }
    {
        socklen_t len = sizeof addr;
        getsockname(listener, (struct sockaddr *)&addr, &len);
    }
    return ntohs(addr.sin_port);
}

int xpp_http_start(int port, int open_browser)
{
    static int log_pipe[2];
    char url[128];
    int got;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    got = listen_on(port);
    if (got < 0 && port != 0) got = listen_on(0); /* taken: any free port */
    if (got < 0) {
        xpp_log(XPP_LOG_ERROR, "xppautX: cannot open a port on 127.0.0.1\n");
        return 0;
    }
    make_token();
    snprintf(url, sizeof url, "http://127.0.0.1:%d/?t=%s", got, token);
    printf("XPP: %s\n", url);
    fflush(stdout);

    /* what xppaut prints: to the terminal and the page */
    orig_stderr = dup(2);
#ifdef _WIN32
    if (_pipe(log_pipe, 65536, _O_BINARY) == 0) {
#else
    if (pipe(log_pipe) == 0) {
#endif
        dup2(log_pipe[1], 1);
        dup2(log_pipe[1], 2);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        pthread_create(&log_thread, NULL, log_main, &log_pipe[0]);
    }
    active = 1;
    pthread_create(&http_thread, NULL, http_main, NULL);
    pthread_create(&watchdog_thread, NULL, watchdog_main, NULL);
    atexit(at_exit);
    if (open_browser) open_in_browser(url);
    return 1;
}
