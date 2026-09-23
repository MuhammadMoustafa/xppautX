/* The browser front end's HTTP server, inside xppautX (xpp_http.h).
   The same job as web/serve.js without Node: serve the compiled-in page,
   stream protocol events to it (Server-Sent Events), take its commands by
   POST, and replay what a page that (re)connects needs to draw.

   Threads: the core runs on the main thread and calls xpp_http_emit and
   xpp_http_read; one thread accepts and answers HTTP requests; one thread
   copies what xppaut prints to the terminal and into the page's log. This
   file includes no core header, so the socket and Windows headers cannot
   clash with core names. */
/* macOS hides the BSD names (INADDR_LOOPBACK) under _XOPEN_SOURCE=600;
   this must come before any system header. */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#include "xpp_http.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
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
#include <unistd.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define close_sock close
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
extern const XppWebAsset xpp_web_assets[]; /* web_assets.c, from web/ */

#define MAX_CLIENTS 16
#define MAX_WINDOWS 32
#define LOG_KEEP 100000

static int active;
static char token[40];
static sock_t listener = INVALID_SOCKET;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t input_ready = PTHREAD_COND_INITIALIZER;
static pthread_t watchdog_thread;
static pthread_t http_thread, log_thread;

/* commands from the page, newline separated, read by the core */
static char *input;
static size_t input_len, input_cap;

/* event streams and what a new one gets first */
static sock_t clients[MAX_CLIENTS];
static int nclients;
static char *sticky_hello, *sticky_palette, *sticky_state, *sticky_ask, *exit_event;
static struct {
    int win;
    char *line;
} windows[MAX_WINDOWS];
static char *log_text; /* the last LOG_KEEP bytes printed */
static size_t log_len;
static int saw_bye, orig_stderr = -1;

static void grow(char **s, size_t *cap, size_t need)
{
    if (need <= *cap) return;
    *cap = need * 2 + 1024;
    *s = realloc(*s, *cap);
}

static char *copy_line(const char *s, size_t n)
{
    char *c = malloc(n + 1);
    memcpy(c, s, n);
    c[n] = 0;
    return c;
}

static void set_sticky(char **slot, const char *s, size_t n)
{
    free(*slot);
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
            if (!send_all(clients[i], ":\n\n", 3)) { /* a comment: the page ignores it */
                close_sock(clients[i]);
                clients[i--] = clients[--nclients];
            }
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
        else if (strcmp(ev, "palette") == 0) set_sticky(&sticky_palette, line, n);
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
    for (i = 0; i < nclients; i++) {
        if (!send_event(clients[i], line, n)) {
            close_sock(clients[i]);
            clients[i--] = clients[--nclients];
        }
    }
    pthread_mutex_unlock(&lock);
}

int xpp_http_read(char *buf, int n, int wait_ms)
{
    int got;
    pthread_mutex_lock(&lock);
    if (wait_ms < 0) {
        while (input_len == 0) pthread_cond_wait(&input_ready, &lock);
    } else if (input_len == 0 && wait_ms > 0) {
        struct timeval now;
        struct timespec until;
        gettimeofday(&now, NULL);
        until.tv_sec = now.tv_sec + wait_ms / 1000;
        until.tv_nsec = now.tv_usec * 1000L + (wait_ms % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) {
            until.tv_sec++;
            until.tv_nsec -= 1000000000L;
        }
        while (input_len == 0)
            if (pthread_cond_timedwait(&input_ready, &lock, &until) != 0) break;
    }
    got = input_len < (size_t)n ? (int)input_len : n;
    memcpy(buf, input, (size_t)got);
    memmove(input, input + got, input_len - (size_t)got);
    input_len -= (size_t)got;
    pthread_mutex_unlock(&lock);
    return got;
}

/* call with the lock held */
static void push_command(const char *s, size_t n)
{
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) n--;
    if (n == 0) return;
    grow(&input, &input_cap, input_len + n + 1);
    memcpy(input + input_len, s, n);
    input_len += n;
    input[input_len++] = '\n';
    pthread_cond_signal(&input_ready);
}

/* ---- what xppaut prints ----------------------------------------------------------- */

/* {"ev":"log","text":"..."} for n bytes of printed text; malloc'd, length in *len */
static char *log_event(const char *text, size_t n, size_t *len)
{
    char *line = malloc(6 * n + 32);
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
        log_text = realloc(log_text, log_len + (size_t)r);
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
        free(line);
    }
    return NULL;
}

/* ---- HTTP ---------------------------------------------------------------------------- */

static void reply(sock_t s, const char *status, const char *type, const unsigned char *body, size_t len)
{
    char head[256];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nCache-Control: no-store\r\n"
                     "Connection: close\r\n\r\n", status, type, (unsigned long)len);
    if (send_all(s, head, (size_t)n) && len) send_all(s, (const char *)body, len);
}

static int token_ok(const char *target)
{
    const char *q = strstr(target, "t=");
    return q && strncmp(q + 2, token, strlen(token)) == 0;
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
        free(line);
    }
    if (ok && sticky_hello) ok = send_event(s, sticky_hello, strlen(sticky_hello));
    if (ok && sticky_palette) ok = send_event(s, sticky_palette, strlen(sticky_palette));
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

static void handle(sock_t s)
{
    char req[8192], method[8], target[512];
    int got = 0, r, header_end = -1, length = 0, i;
    const XppWebAsset *a;
    /* the request line, headers and a small body */
    while (got < (int)sizeof req - 1) {
        char *p;
        r = recv(s, req + got, (int)sizeof req - 1 - got, 0);
        if (r <= 0) break;
        got += r;
        req[got] = 0;
        if (header_end < 0 && (p = strstr(req, "\r\n\r\n")) != NULL) {
            char *cl = strstr(req, "Content-Length:");
            if (!cl) cl = strstr(req, "content-length:");
            header_end = (int)(p - req) + 4;
            length = cl && cl < p ? atoi(cl + 15) : 0;
        }
        if (header_end >= 0 && got >= header_end + length) break;
    }
    req[got] = 0;
    if (header_end < 0 || sscanf(req, "%7s %511s", method, target) != 2) {
        close_sock(s);
        return;
    }
    if (strcmp(method, "GET") == 0 && strncmp(target, "/events", 7) == 0) {
        if (token_ok(target)) {
            open_events(s);
            return;
        }
        reply(s, "403 Forbidden", "text/plain", (const unsigned char *)"bad token", 9);
    } else if (strcmp(method, "POST") == 0 && strncmp(target, "/cmd", 4) == 0) {
        if (token_ok(target)) {
            const char *body = req + header_end;
            pthread_mutex_lock(&lock);
            if (strstr(body, "\"cmd\":\"answer\"")) set_sticky(&sticky_ask, NULL, 0);
            if (!exit_event) push_command(body, strlen(body));
            pthread_mutex_unlock(&lock);
            reply(s, "204 No Content", "text/plain", NULL, 0);
        } else reply(s, "403 Forbidden", "text/plain", (const unsigned char *)"bad token", 9);
    } else if (strcmp(method, "GET") == 0) {
        char path[512];
        for (i = 0; target[i] && target[i] != '?' && i < (int)sizeof path - 1; i++) path[i] = target[i];
        path[i] = 0;
        if (strcmp(path, "/index.html") == 0) strcpy(path, "/");
        for (a = xpp_web_assets; a->path; a++)
            if (strcmp(a->path, path) == 0) break;
        if (a->path) reply(s, "200 OK", a->type, a->data, a->len);
        else reply(s, "404 Not Found", "text/plain", (const unsigned char *)"not found", 9);
    } else reply(s, "405 Method Not Allowed", "text/plain", NULL, 0);
    close_sock(s);
}

static void *http_main(void *arg)
{
    (void)arg;
    for (;;) {
        sock_t s = accept(listener, NULL, NULL);
        if (s == INVALID_SOCKET) continue;
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
        errno_t rand_s(unsigned int *);
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
    if (system(cmd) != 0) fprintf(stderr, "open %s in a browser\n", url);
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
        fprintf(stderr, "xppautX: cannot open a port on 127.0.0.1\n");
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
