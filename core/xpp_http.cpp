/* The browser front end's HTTP server, inside xppautX (xpp_http.h).
   Serve the compiled-in page (web2/dist),
   stream protocol events to it (Server-Sent Events), take its commands by
   POST, and replay what a page that (re)connects needs to draw.

   Threads: the core runs on the main thread and calls http::emit; one
   thread accepts connections, each answered on a thread of its own (see
   handle()), which pushes the page's commands into the inbox (xpp_inbox.h),
   where the core takes them; one thread copies what xppaut prints to the
   terminal and into the page's log. The
   model's folder is served as /files (xpp_files.h: listing, reading, and
   uploads streamed to a temporary file). This file includes no core header
   but those small APIs (xpp_inbox.h, xpp_files.h, xpp_log.h, xpp_io.h,
   xpp_mem.h), so the socket and Windows headers cannot clash with core
   names. */
/* macOS hides the BSD names (INADDR_LOOPBACK) under _XOPEN_SOURCE=600;
   this must come before any system header. */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <pthread.h>
#include <string>
#include <string_view>
#include <vector>
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
#include <fcntl.h>
#include <fstream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define close_sock close
/* BSD and macOS have no MSG_NOSIGNAL; SIGPIPE is ignored in http::start */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

#ifdef _WIN32
extern "C" errno_t rand_s(unsigned int *); /* the C library's; stdlib.h declares it only with _CRT_RAND_S */
#endif

typedef struct {
    const char *path, *type;
    const unsigned char *data;
    size_t len;
} XppWebAsset;
extern "C" const XppWebAsset xpp_web_assets[]; /* web_assets.c (C): web2/dist/ at / */

/* No exception may leave a thread: a failed allocation ends the program
   with xpp::out_of_memory_now (not out_of_memory: at_exit would wait on
   the lock this thread may hold). */

namespace {

constexpr const char *BAD_TOKEN = "bad token"; /* one response for every protected route with a rejected token */
constexpr int MAX_CLIENTS = 16; /* bound simultaneous event streams and their socket storage */
constexpr int MAX_WINDOWS = 32; /* bound window events retained for reconnecting pages */
constexpr size_t LOG_KEEP = 100000; /* bound printed text retained for reconnecting pages */
constexpr size_t CHUNK = 65536; /* bound socket sends, file transfers and the Windows log pipe buffer */

/* a window's create event, replayed to a page that (re)connects */
struct WindowLine {
    int win = 0;
    std::string line; /* empty: a free slot */
};

/* The pages whose numbered commands are remembered (serve_cmd): one per
   open tab or window, more than MAX_CLIENTS streams can hold, so a page is
   only forgotten once it has long stopped sending. */
constexpr int MAX_PAGES = 32;
constexpr int MAX_CONNECTION_THREADS = 256; /* room for MAX_CLIENTS streams and six parallel requests per MAX_PAGES tab, plus spare sockets */
constexpr int MAX_UPLOADS = 32; /* one per MAX_PAGES tab: the page uploads its picked files sequentially */
constexpr size_t PAGE_ID_MAX = 64; /* web2's ids are 32 hex digits */

/* the last command a page numbered that reached the inbox */
struct PageCommands {
    std::string page;              /* its p=; empty: a free slot */
    unsigned long long last = 0;   /* its n= */
    unsigned long long used = 0;   /* Server::page_clock when it last sent: the oldest is forgotten first */
};

/* What the threads share, under `lock` unless said otherwise. Never
   destroyed: the connection, log and watchdog threads still run while
   exit() destroys statics (after at_exit), so it must outlive them. */
struct Server {
    std::string token;    /* set once before the threads start */
    std::string page_url; /* likewise */
    /* event streams and what a new one gets first (an empty line: none) */
    std::array<sock_t, MAX_CLIENTS> clients{};
    int nclients = 0;
    int nconnections = 0, nuploads = 0;
    std::string sticky_hello, sticky_state, sticky_ask, exit_event;
    std::string sticky_computing; /* the running command computes: until its idle */
    std::string load_error; /* the error event of a model that did not load */
    std::array<WindowLine, MAX_WINDOWS> windows;
    std::string log_text; /* the last LOG_KEEP bytes printed */
    std::array<PageCommands, MAX_PAGES> pages;
    unsigned long long page_clock = 0;
};
Server &srv = *new Server;

bool serving; /* http::active() */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* Non-command routes and upload admission/commit are serialized; upload
   bodies use independent xpp::files::Put writers outside this mutex. */
pthread_mutex_t serve_lock = PTHREAD_MUTEX_INITIALIZER;

/* Admission and release use the server's existing counter lock. A thread
   adopts the connection slot reserved before pthread_create. */
struct RequestSlot {
    int *counter = nullptr;
    explicit RequestSlot(int *reserved = nullptr) : counter(reserved) {}
    RequestSlot(const RequestSlot &) = delete;
    RequestSlot &operator=(const RequestSlot &) = delete;
    bool acquire(int &count, int limit)
    {
        pthread_mutex_lock(&lock);
        if (count < limit) { count++; counter = &count; }
        pthread_mutex_unlock(&lock);
        return counter != nullptr;
    }
    ~RequestSlot()
    {
        if (!counter) return;
        pthread_mutex_lock(&lock);
        --*counter;
        pthread_mutex_unlock(&lock);
    }
};
pthread_t watchdog_thread;
pthread_t http_thread, log_thread;
bool saw_bye;
int orig_stderr = -1;
/* at_exit's wait after an error ends when this is set (http::release) */
bool released;
pthread_cond_t released_cond = PTHREAD_COND_INITIALIZER;

/* stream i is gone (the lock is held) */
void drop_client(int i)
{
    close_sock(srv.clients[i]);
    srv.clients[i] = srv.clients[srv.nclients - 1];
    srv.nclients--;
}

bool send_all(sock_t s, std::string_view data)
{
    const char *p = data.data();
    size_t n = data.size();
    while (n > 0) {
#ifdef _WIN32
        int r = send(s, p, static_cast<int>(n > CHUNK ? CHUNK : n), 0);
#else
        ssize_t r = send(s, p, n, MSG_NOSIGNAL);
#endif
        if (r <= 0) return false;
        p += r;
        n -= static_cast<size_t>(r);
    }
    return true;
}

/* an event as one Server-Sent Event: its frame and the line in one gathered
   write (no copy of the line, and a small event goes out in one packet) */
bool send_event(sock_t s, std::string_view line)
{
    std::array<std::string_view, 3> part = {"data: ", line, "\n\n"};
    std::size_t k = 0;
    while (k < part.size()) {
#ifdef _WIN32
        std::array<WSABUF, 3> buf;
        DWORD n = 0, sent = 0;
        for (std::size_t i = k; i < part.size(); i++, n++) {
            buf[n].buf = const_cast<char *>(part[i].data()); /* WSABUF's own type: WSASend only reads it */
            buf[n].len = static_cast<ULONG>(part[i].size());
        }
        if (WSASend(s, buf.data(), n, &sent, 0, nullptr, nullptr) != 0 || sent == 0) return false;
        std::size_t r = sent;
#else
        std::array<iovec, 3> buf;
        msghdr m{};
        for (std::size_t i = k; i < part.size(); i++, m.msg_iovlen++) {
            buf[m.msg_iovlen].iov_base = const_cast<char *>(part[i].data()); /* iovec's own type: sendmsg only reads it */
            buf[m.msg_iovlen].iov_len = part[i].size();
        }
        m.msg_iov = buf.data();
        const ssize_t sent = sendmsg(s, &m, MSG_NOSIGNAL);
        if (sent <= 0) return false;
        std::size_t r = static_cast<std::size_t>(sent);
#endif
        /* a partial write: on from where it stopped */
        while (k < part.size() && r >= part[k].size()) r -= part[k++].size();
        if (k < part.size()) part[k].remove_prefix(r);
    }
    return true;
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
constexpr int ALONE_SECONDS = 10;

/* A page that says it is leaving (POST /leave, from web2's pagehide) is
   waited for only this long: a reload comes back within it (its new event
   stream cancels the wait), a closed tab does not. */
constexpr long LEAVE_MS = 2000;
constexpr long TICK_MS = 100;       /* the watchdog's poll */
constexpr long HEARTBEAT_MS = 2000; /* between its comments to the streams */

int had_client;
time_t alone_since;
long long leave_at; /* steady_clock ms of the last /leave, 0: none pending (the lock) */

long long now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() + 1;
}

/* a comment on every stream: one that is gone fails the write and is dropped
   (the first write to a closed peer can still succeed; the next fails) */
void heartbeat()
{
    for (int i = 0; i < srv.nclients; i++) {
        if (!send_all(srv.clients[i], ":\n\n")) drop_client(i--); /* a comment: the page ignores it */
    }
}

[[noreturn]] void end_program()
{
    /* exit() would run at_exit(), which tells the page the program is
       going and waits on the same lock from this thread: it hangs, and
       there is no page left to tell anyway. Flush what the core wrote,
       then go. */
    std::fflush(nullptr);
#ifdef _WIN32
    /* the core thread may sit in exit() (at_exit waiting for a page that is
       gone: a stopped model's log), holding the C runtime's exit lock, which
       _exit() would wait on forever (W112) */
    TerminateProcess(GetCurrentProcess(), 0);
#endif
    _exit(0);
}

void *watchdog_main(void *)
{
    long long next_beat = now_ms() + HEARTBEAT_MS;
    for (;;) {
#ifdef _WIN32
        Sleep(TICK_MS);
#else
        usleep(TICK_MS * 1000);
#endif
        pthread_mutex_lock(&lock);
        long long now = now_ms();
        if (now >= next_beat) {
            next_beat = now + HEARTBEAT_MS;
            heartbeat();
            if (srv.nclients > 0) alone_since = 0;
            else if (had_client && !alone_since) alone_since = time(nullptr);
            if (had_client && srv.nclients == 0 && alone_since
                && time(nullptr) - alone_since >= ALONE_SECONDS) {
                pthread_mutex_unlock(&lock);
                end_program();
            }
        }
        if (leave_at && now - leave_at >= LEAVE_MS) {
            heartbeat(); /* the page's closed stream fails on the second write */
            heartbeat();
            if (srv.nclients == 0) {
                pthread_mutex_unlock(&lock);
                end_program();
            }
            leave_at = 0; /* another page is still connected */
        }
        pthread_mutex_unlock(&lock);
    }
    return nullptr;
}

/* the value of "key":"..." or "key":number in a flat event line (the
   events put these keys first, before any user text) */
std::optional<std::string_view> field(std::string_view line, std::string_view key)
{
    std::string pat = xpp::format("\"{}\":", key);
    size_t p = line.find(pat);
    if (p == std::string_view::npos) return std::nullopt;
    p += pat.size();
    if (p < line.size() && line[p] == '"') p++;
    size_t e = p;
    while (e < line.size() && line[e] != '"' && line[e] != ',' && line[e] != '}') e++;
    return line.substr(p, e - p);
}

/* the window table's side of a `window` event (the lock is held) */
void track_window(std::string_view line, std::string_view op, std::string_view win)
{
    int w = std::atoi(std::string(win).c_str()), slot = -1;
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!srv.windows[i].line.empty() && srv.windows[i].win == w) slot = i;
    if (op == "create") {
        for (int i = 0; slot < 0 && i < MAX_WINDOWS; i++)
            if (srv.windows[i].line.empty()) slot = i;
        if (slot >= 0) {
            srv.windows[slot].win = w;
            srv.windows[slot].line = line;
        }
    } else if (op == "destroy" && slot >= 0) {
        srv.windows[slot].line.clear();
    }
}

void emit_event(std::string_view line)
{
    pthread_mutex_lock(&lock);
    if (std::optional<std::string_view> ev = field(line.substr(0, 40), "ev")) {
        if (*ev == "hello") srv.sticky_hello = line;
        else if (*ev == "state") srv.sticky_state = line;
        else if (*ev == "ask") srv.sticky_ask = line;
        else if (*ev == "computing") srv.sticky_computing = line;
        else if (*ev == "idle") {
            srv.sticky_ask.clear();
            srv.sticky_computing.clear();
        }
        else if (*ev == "bye") saw_bye = true;
        else if (*ev == "error") srv.load_error = line;
        else if (*ev == "window") {
            std::optional<std::string_view> op = field(line, "op"), win = field(line, "win");
            if (op && win) track_window(line, *op, *win);
        }
    }
    for (int i = 0; i < srv.nclients; i++)
        if (!send_event(srv.clients[i], line)) drop_client(i--);
    pthread_mutex_unlock(&lock);
}

/* a command from the page, into the inbox. Trailing blanks go and an empty
   body is ignored; a body of several lines gives several lines (a CR before
   a newline dropped), as when the core split this text itself. Called with
   the lock held: the inbox's locks nest inside it, never the other way. */
void push_command(std::string_view s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.remove_suffix(1);
    while (!s.empty()) {
        size_t nl = s.find('\n');
        std::string_view one = s.substr(0, nl);
        if (!one.empty() && one.back() == '\r') one.remove_suffix(1);
        xpp::inbox::push(one);
        if (nl == std::string_view::npos) break;
        s.remove_prefix(nl + 1);
    }
}

/* ---- what xppaut prints ----------------------------------------------------------- */

/* {"ev":"log","text":"..."} for printed text */
std::string log_event(std::string_view text)
{
    std::string line = "{\"ev\":\"log\",\"text\":";
    xpp::json_append_string(line, text);
    line += '}';
    return line;
}

void *log_main(void *arg)
{
    int fd = *static_cast<int *>(arg);
    constexpr size_t LOG_READ_CHUNK = 4096; /* bound each log pipe read and its emitted event */
    std::array<char, LOG_READ_CHUNK> chunk;
    try {
        for (;;) {
            int r = read(fd, chunk.data(), chunk.size());
            if (r <= 0) break;
            if (orig_stderr >= 0 && write(orig_stderr, chunk.data(), static_cast<unsigned>(r)) < 0) orig_stderr = -1;
            std::string_view got(chunk.data(), static_cast<size_t>(r));
            pthread_mutex_lock(&lock);
            srv.log_text += got;
            size_t len = srv.log_text.size();
            if (len > LOG_KEEP) { /* keep the last lines */
                size_t cut = len - LOG_KEEP;
                while (cut < len && srv.log_text[cut - 1] != '\n') cut++;
                srv.log_text.erase(0, cut);
            }
            pthread_mutex_unlock(&lock);
            emit_event(log_event(got));
        }
    } catch (...) {
        xpp::out_of_memory_now("in the HTTP server");
    }
    return nullptr;
}

/* ---- HTTP ---------------------------------------------------------------------------- */

void reply(sock_t s, const char *status, const char *type, std::string_view body)
{
    std::string head = xpp::format("HTTP/1.1 {}\r\nContent-Type: {}\r\nContent-Length: {}\r\nCache-Control: no-store\r\n"
                                   "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
                                   status, type, body.size());
    if (send_all(s, head) && !body.empty()) send_all(s, body);
}

void reply_text(sock_t s, const char *status, const char *text) { reply(s, status, "text/plain", text); }

/* Limit refusals never drain the body. */
void refuse_connection(sock_t s, const char *reason)
{
    reply_text(s, "503 Service Unavailable", reason);
#ifdef _WIN32
    shutdown(s, SD_SEND);
#else
    shutdown(s, SHUT_WR);
#endif
    close_sock(s);
}

/* the query's t= is the token, compared in full and in constant time */
bool token_ok(std::string_view target)
{
    const std::string &token = srv.token;
    size_t n = token.size();
    for (size_t q = target.find('?'); q != std::string_view::npos; q = target.find('&', q + 1)) {
        std::string_view v = target.substr(q + 1);
        unsigned diff = 0;
        if (!v.starts_with("t=")) continue;
        v.remove_prefix(2);
        if (n == 0 || std::min(v.find('&'), v.size()) != n) continue;
        for (size_t i = 0; i < n; i++) diff |= static_cast<unsigned char>(v[i] ^ token[i]);
        if (!diff) return true;
    }
    return false;
}

/* the value of the query's first `key`=; nullopt when it has none */
std::optional<std::string_view> query_value(std::string_view target, std::string_view key)
{
    for (size_t q = target.find('?'); q != std::string_view::npos; q = target.find('&', q + 1)) {
        std::string_view v = target.substr(q + 1);
        if (!v.starts_with(key) || v.substr(key.size(), 1) != "=") continue;
        v.remove_prefix(key.size() + 1);
        return v.substr(0, std::min(v.find('&'), v.size()));
    }
    return std::nullopt;
}

void open_events(sock_t s)
{
    static constexpr std::string_view head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n\r\n";
    /* the events are many and mostly small: each goes out at once, not held
       back for the next (Nagle's algorithm) */
    const int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof one);
    pthread_mutex_lock(&lock);
    bool ok = send_all(s, head);
    if (ok && !srv.log_text.empty()) ok = send_event(s, log_event(srv.log_text));
    if (ok && !srv.sticky_hello.empty()) ok = send_event(s, srv.sticky_hello);
    for (int i = 0; ok && i < MAX_WINDOWS; i++)
        if (!srv.windows[i].line.empty()) ok = send_event(s, srv.windows[i].line);
    if (ok && !srv.sticky_state.empty()) ok = send_event(s, srv.sticky_state);
    if (ok && !srv.sticky_computing.empty()) ok = send_event(s, srv.sticky_computing);
    if (ok && !srv.sticky_ask.empty()) ok = send_event(s, srv.sticky_ask);
    if (ok && !srv.load_error.empty()) ok = send_event(s, srv.load_error);
    if (ok && !srv.exit_event.empty()) ok = send_event(s, srv.exit_event);
    if (ok && srv.nclients < MAX_CLIENTS) {
        had_client = 1;
        alone_since = 0;
        leave_at = 0; /* a page is back: a reload, not a close */
        srv.clients[srv.nclients++] = s;
        /* the page draws from scratch; a redraw would wait behind an open prompt */
        if (!srv.sticky_hello.empty() && srv.sticky_ask.empty() && srv.exit_event.empty())
            push_command("{\"cmd\":\"redraw\"}");
    } else close_sock(s);
    pthread_mutex_unlock(&lock);
}

/* ---- a request: its head, then a body read as it comes -------------------------------- */

constexpr size_t HEAD_MAX = 8192;                   /* request line and headers */
constexpr unsigned long long CMD_MAX = 1ULL << 20;  /* a POST /cmd body */
constexpr int RECV_SECONDS = 30;                    /* a client that stops sending mid-request is dropped */
constexpr int HEAD_SECONDS = 5;  /* a whole head from accept: a browser sends its small head at once, a preconnected spare socket is closed and reopened */
constexpr int SEND_SECONDS = 10; /* a client that stops reading is dropped rather than holding its thread, or the core at an event, for ever; a page busy drawing a large plot may not read for a few seconds, and a dropped event stream loses the run's data */
/* a connection over MAX_CONNECTION_THREADS is read on the accept thread,
   for its head only (closing on an unread head loses the 503 on Windows):
   a silent one may hold up the next accept this long, never RECV_SECONDS */
constexpr int REFUSED_HEAD_SECONDS = 1; /* bound the accept thread wait when connection slots are exhausted */
constexpr size_t METHOD_MAX = 7, TARGET_MAX = 1023; /* longer is cut, as sscanf's %7s %1023s did */

struct Request {
    sock_t s = INVALID_SOCKET;
    long long head_deadline = 0;  /* measured from accept, including thread scheduling */
    std::array<char, HEAD_MAX + 1> head{};
    std::string method, target;
    size_t head_len = 0;           /* the head, up to and with its blank line */
    const char *body0 = nullptr;   /* body bytes that arrived with the head */
    size_t have = 0;               /* how many */
    bool has_length = false;       /* a Content-Length was sent */
    unsigned long long length = 0;
    unsigned long long consumed = 0; /* body bytes read so far */
};

int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* header `name` (lower case) of the head: its value, blanks trimmed */
std::optional<std::string> header(const Request &q, std::string_view name)
{
    size_t n = name.size(), i;
    const char *p = q.head.data(), *end = q.head.data() + q.head_len;
    while ((p = static_cast<const char *>(std::memchr(p, '\n', static_cast<size_t>(end - p)))) != nullptr) {
        p++;
        if (static_cast<size_t>(end - p) <= n || p[n] != ':') continue;
        for (i = 0; i < n && lower(static_cast<unsigned char>(p[i])) == name[i]; i++) {}
        if (i < n) continue;
        p += n + 1;
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        const char *v = p;
        while (p < end && *p != '\r' && *p != '\n') p++;
        std::string_view value(v, static_cast<size_t>(p - v));
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
        return std::string(value);
    }
    return std::nullopt;
}

/* the next blank-separated word of `rest`, at most `max` characters (what
   sscanf's %Ns reads); false when there is none */
bool scan_word(std::string_view &rest, size_t max, std::string &out)
{
    size_t i = 0;
    while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) i++;
    size_t k = i;
    while (k < rest.size() && k - i < max && !std::isspace(static_cast<unsigned char>(rest[k]))) k++;
    if (k == i) return false;
    out.assign(rest.substr(i, k - i));
    rest.remove_prefix(k);
    return true;
}

bool set_recv_timeout(sock_t s, int milliseconds);

/* reads the head; false when the connection is not a request worth an answer */
bool read_head(Request &q)
{
    size_t got = 0, i = 0;
    while (got < HEAD_MAX) {
        const long long left = q.head_deadline - now_ms();
        if (left <= 0 || !set_recv_timeout(q.s, static_cast<int>(left))) return false;
        int r = recv(q.s, q.head.data() + got, static_cast<int>(HEAD_MAX - got), 0);
        if (r <= 0) return false;
        got += static_cast<size_t>(r);
        for (i = got >= static_cast<size_t>(r) + 3 ? got - static_cast<size_t>(r) - 3 : 0; i + 4 <= got; i++)
            if (std::memcmp(q.head.data() + i, "\r\n\r\n", 4) == 0) break;
        if (i + 4 <= got) {
            q.head_len = i + 4;
            break;
        }
    }
    if (!q.head_len) return false;
    q.body0 = q.head.data() + q.head_len;
    q.have = got - q.head_len;
    q.head[q.head_len - 2] = 0; /* the head as a string (the body starts after it) */
    std::string_view line(q.head.data());
    if (!scan_word(line, METHOD_MAX, q.method) || !scan_word(line, TARGET_MAX, q.target)) return false;
    std::optional<std::string> v = header(q, "content-length");
    q.has_length = v.has_value();
    if (q.has_length) {
        if (v->empty() || v->find_first_not_of("0123456789") != std::string::npos || v->size() > 18) q.length = ~0ULL;
        else q.length = std::strtoull(v->c_str(), nullptr, 10);
        q.consumed = q.have < q.length ? q.have : q.length;
    }
    return true;
}

/* up to n more bytes of the body */
int take(Request &q, char *buf, size_t n)
{
    int r = recv(q.s, buf, static_cast<int>(n), 0);
    if (r > 0) q.consumed += static_cast<unsigned long long>(r);
    return r;
}

void continue_body(Request &q)
{
    if (std::optional<std::string> v = header(q, "expect"); v && *v == "100-continue")
        send_all(q.s, "HTTP/1.1 100 Continue\r\n\r\n");
}

/* the query-less path of the target */
size_t path_len(std::string_view target) { return std::min(target.find('?'), target.size()); }

/* a %-encoded name; nullopt for a bad escape or a NUL */
std::optional<std::string> url_decode(std::string_view s)
{
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        int c = static_cast<unsigned char>(s[i]);
        if (c == '%') {
            int h = 0;
            for (size_t j = 1; j <= 2; j++) {
                if (i + j >= s.size()) return std::nullopt;
                int d = lower(static_cast<unsigned char>(s[i + j]));
                d = d >= '0' && d <= '9' ? d - '0' : d >= 'a' && d <= 'f' ? d - 'a' + 10 : -1;
                if (d < 0) return std::nullopt;
                h = h * 16 + d;
            }
            if (h == 0) return std::nullopt;
            c = h;
            i += 2;
        }
        out += static_cast<char>(c);
    }
    return out;
}

/* A page numbers its commands (web2's HttpTransport, W124): p= its id and
   n= 1, 2, ... in the order it sends them, one at a time. A POST that
   failed without an answer (a connection the browser or the network lost)
   may have arrived all the same, and the page sends it again: a number not
   above the last one of this page that reached the inbox is answered as
   before but not pushed twice. A command with neither (a script, curl) is
   pushed as it comes. */
enum class Numbered { no, yes, bad };
constexpr size_t COMMAND_NUMBER_DIGITS = 18; /* fits an unsigned long long */

Numbered command_number(std::string_view target, std::string_view &page, unsigned long long &n)
{
    std::optional<std::string_view> p = query_value(target, "p"), num = query_value(target, "n");
    if (!p && !num) return Numbered::no;
    if (!p || !num || p->empty() || p->size() > PAGE_ID_MAX || num->empty() || num->size() > COMMAND_NUMBER_DIGITS)
        return Numbered::bad;
    for (char c : *p)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return Numbered::bad;
    if (num->find_first_not_of("0123456789") != std::string_view::npos) return Numbered::bad;
    page = *p;
    n = std::strtoull(std::string(*num).c_str(), nullptr, 10);
    return n > 0 ? Numbered::yes : Numbered::bad;
}

/* whether command n of `page` is new, remembering it if so (the lock is held) */
bool first_arrival(std::string_view page, unsigned long long n)
{
    PageCommands *slot = nullptr;
    for (PageCommands &p : srv.pages)
        if (p.page == page) slot = &p;
    if (!slot) { /* a free slot (used 0), else the page that sent least recently */
        slot = &srv.pages[0];
        for (PageCommands &p : srv.pages)
            if (p.used < slot->used) slot = &p;
        *slot = PageCommands{std::string(page), 0, 0};
    }
    slot->used = ++srv.page_clock;
    if (n <= slot->last) return false;
    slot->last = n;
    return true;
}

/* POST /cmd: the whole body, then into the inbox */
void serve_cmd(Request &q)
{
    unsigned long long n = q.has_length ? q.length : q.have;
    if (n > CMD_MAX) {
        reply_text(q.s, "413 Payload Too Large", "command too long");
        return;
    }
    std::string_view page;
    unsigned long long number = 0;
    const Numbered numbered = command_number(q.target, page, number);
    if (numbered == Numbered::bad) {
        reply_text(q.s, "400 Bad Request", "bad command number");
        return;
    }
    continue_body(q);
    std::string body(static_cast<size_t>(n), '\0');
    size_t got = q.have < n ? q.have : static_cast<size_t>(n);
    std::memcpy(body.data(), q.body0, got);
    while (got < n) {
        int r = take(q, body.data() + got, static_cast<size_t>(n) - got);
        if (r <= 0) break;
        got += static_cast<size_t>(r);
    }
    if (got < n) {
        reply_text(q.s, "400 Bad Request", "incomplete body");
        return;
    }
    std::string_view cmd(body.c_str()); /* up to a NUL, as the C string it was */
    pthread_mutex_lock(&lock);
    if (numbered == Numbered::no || first_arrival(page, number)) {
        if (cmd.find("\"cmd\":\"answer\"") != std::string_view::npos) srv.sticky_ask.clear();
        if (srv.exit_event.empty()) push_command(cmd);
    }
    pthread_mutex_unlock(&lock);
    reply(q.s, "204 No Content", "text/plain", {});
}

/* POST /leave?t=TOKEN: the page is going away (a beacon from pagehide).
   The watchdog ends the program unless a page has connected again by
   LEAVE_MS later. */
void serve_leave(Request &q)
{
    pthread_mutex_lock(&lock);
    leave_at = now_ms();
    pthread_mutex_unlock(&lock);
    reply(q.s, "204 No Content", "text/plain", {});
}

const char *files_status(int st)
{
    switch (st) {
    case XPP_FILES_BAD_NAME: return "400 Bad Request";
    case XPP_FILES_NOT_FOUND: return "404 Not Found";
    case XPP_FILES_REFUSED: return "403 Forbidden";
    case XPP_FILES_TOO_LARGE: return "413 Payload Too Large";
    case XPP_FILES_BUSY: return "409 Conflict";
    default: return "500 Internal Server Error";
    }
}

void get_file(sock_t s, const std::string &name)
{
    FILE *raw = nullptr;
    unsigned long long size;
    int st = xpp::files::open(name, raw, size);
    if (st != XPP_FILES_OK) {
        reply_text(s, files_status(st), xpp::files::status_text(st));
        return;
    }
    xpp::UniqueFile fp(raw);
    std::string head = xpp::format("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: {}\r\n"
                                   "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
                                   size);
    bool ok = send_all(s, head);
    std::vector<char> buf(CHUNK);
    size_t n;
    while (ok && (n = std::fread(buf.data(), 1, buf.size(), fp.get())) > 0) ok = send_all(s, {buf.data(), n});
}

/* PUT /files/NAME: the body streams into a temporary file that becomes
   NAME only once all of it arrived (xpp_files.h); xpp_files decides
   whether it may land now, as for the protocol's `file` put (refused
   while a computation runs, 409) */
void put_file(Request &q, const std::string &name)
{
    xpp::files::Put *put;
    unsigned long long left, size;
    std::string sha;
    if (!q.has_length) {
        reply_text(q.s, "411 Length Required", "a Content-Length is required");
        return;
    }
    if (q.length > XPP_FILES_CAP) { /* refused before a byte of the body is read */
        reply_text(q.s, "413 Payload Too Large", xpp::files::status_text(XPP_FILES_TOO_LARGE));
        return;
    }
    pthread_mutex_lock(&serve_lock);
    int st = xpp::files::put_begin(name, XPP_FILES_CAP, put);
    pthread_mutex_unlock(&serve_lock);
    if (st != XPP_FILES_OK) {
        reply_text(q.s, files_status(st), xpp::files::status_text(st));
        return;
    }
    continue_body(q);
    left = q.length;
    {
        size_t first = q.have < left ? q.have : static_cast<size_t>(left);
        st = xpp::files::put_write(*put, {q.body0, first});
        left -= first;
    }
    std::vector<char> buf(CHUNK);
    while (st == XPP_FILES_OK && left > 0) {
        int r = take(q, buf.data(), static_cast<size_t>(left < CHUNK ? left : CHUNK));
        if (r <= 0) break; /* cut short, or the client stopped sending */
        st = xpp::files::put_write(*put, {buf.data(), static_cast<size_t>(r)});
        left -= static_cast<unsigned long long>(r);
    }
    if (st != XPP_FILES_OK || left > 0) {
        xpp::files::put_abort(put);
        if (st != XPP_FILES_OK) reply_text(q.s, files_status(st), xpp::files::status_text(st));
        else reply_text(q.s, "400 Bad Request", "incomplete body");
        return;
    }
    pthread_mutex_lock(&serve_lock);
    st = xpp::files::put_commit(put, size, sha);
    pthread_mutex_unlock(&serve_lock);
    if (st != XPP_FILES_OK) {
        reply_text(q.s, files_status(st), xpp::files::status_text(st));
        return;
    }
    /* the name passed xpp::files::name_ok: no quote, backslash or control character */
    reply(q.s, "200 OK", "application/json",
          xpp::format("{{\"name\":\"{}\",\"size\":{},\"sha256\":\"{}\"}}", name, size, sha));
}

/* /files (the listing), /files/NAME (GET, PUT): docs/protocol.md "Files" */
void serve_files(Request &q)
{
    size_t n = path_len(q.target);
    if (n == 6 || (n == 7 && q.target[6] == '/')) {
        if (q.method == "GET") reply(q.s, "200 OK", "application/json", xpp::files::list_json());
        else reply_text(q.s, "405 Method Not Allowed", "GET only");
        return;
    }
    std::optional<std::string> name = url_decode(std::string_view(q.target).substr(7, n - 7));
    if (!name || !xpp::files::name_ok(name->c_str())) {
        reply_text(q.s, files_status(XPP_FILES_BAD_NAME), xpp::files::status_text(XPP_FILES_BAD_NAME));
        return;
    }
    if (q.method == "GET") get_file(q.s, *name);
    else if (q.method == "PUT") put_file(q, *name);
    else reply_text(q.s, "405 Method Not Allowed", "GET or PUT");
}

/* a bookmark of an old path, /v2/ (web2 moved to / at T17) or /v1/ (the
   classic page, removed at T18): redirect it to the same path under /,
   keeping the query string (the token). */
void redirect(sock_t s, std::string_view location)
{
    send_all(s, xpp::format("HTTP/1.1 302 Found\r\nLocation: {}\r\nContent-Length: 0\r\nCache-Control: no-store\r\n"
                            "Connection: close\r\n\r\n",
                            location));
}

constexpr size_t ASSET_PATH_MAX = 511; /* a longer path is cut, and found nowhere */
constexpr size_t LOCATION_MAX = 559; /* preserve the legacy redirect location truncation */

void serve_asset(Request &q)
{
    std::string_view target = q.target;
    size_t query = target.find('?');
    std::string path(target.substr(0, std::min(query, ASSET_PATH_MAX)));
    if (path == "/v1" || path.starts_with("/v1/") || path == "/v2" || path.starts_with("/v2/")) {
        std::string_view rest = path.size() > 3 && path[3] == '/' ? std::string_view(path).substr(4) : "";
        std::string location =
            xpp::format("/{}{}", rest, query == std::string_view::npos ? std::string_view() : target.substr(query));
        if (location.size() > LOCATION_MAX) location.resize(LOCATION_MAX);
        redirect(q.s, location);
        return;
    }
    if (path == "/index.html") path = "/";
    const XppWebAsset *a;
    for (a = xpp_web_assets; a->path; a++)
        if (path == a->path) break;
    if (a->path) reply(q.s, "200 OK", a->type, {reinterpret_cast<const char *>(a->data), a->len});
    else reply_text(q.s, "404 Not Found", "not found");
}

/* Both socket timeouts use milliseconds; zero would disable the timeout. */
bool set_socket_timeout(sock_t s, int option, int milliseconds)
{
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(milliseconds);
    return setsockopt(s, SOL_SOCKET, option, reinterpret_cast<const char *>(&ms), sizeof ms) == 0;
#else
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return setsockopt(s, SOL_SOCKET, option, reinterpret_cast<const char *>(&tv), sizeof tv) == 0;
#endif
}

bool set_recv_timeout(sock_t s, int milliseconds)
{
    return set_socket_timeout(s, SO_RCVTIMEO, milliseconds);
}

/* A request refused before its body was read (a bad token, a bad name)
   still has the body coming: closing on unread data resets the connection,
   and the client may lose the answer. A small rest is read and dropped
   first; a large one (an upload over the cap) is not waited for. */
constexpr unsigned long long DRAIN_MAX = 1ULL << 20; /* drain only small refused bodies so their error reply survives */
constexpr int DRAIN_SECONDS = 2; /* a refused small body gets only a short wait to preserve its error reply */
void drain(Request &q)
{
    std::array<char, 4096> buf;
    if (!q.has_length || q.length == ~0ULL || q.consumed >= q.length) return;
    unsigned long long left = q.length - q.consumed;
    if (left > DRAIN_MAX) return;
    if (!set_recv_timeout(q.s, DRAIN_SECONDS * 1000)) return;
    while (left > 0) {
        int r = take(q, buf.data(), left < buf.size() ? static_cast<size_t>(left) : buf.size());
        if (r <= 0) break;
        left -= static_cast<unsigned long long>(r);
    }
}

/* One connection, on its own thread (http_main): reading its head may
   wait up to HEAD_SECONDS, since a browser opens connections it sends
   nothing on yet (a preconnect, a spare socket), and that wait must not
   hold up the next request: with one thread doing it all, an Abort POSTed
   meanwhile waited the old 30 s while the run went on (T25). A command is
   pushed as soon as it has arrived; other routes and upload admission and
   commit are serialized, but admitted upload bodies stream independently.
   The page keeps its commands in order by sending
   the next one once the last was answered (web2's HttpTransport), and
   numbers them so that one sent again is not taken twice (serve_cmd). */
void handle(sock_t s, bool connection_limited, long long head_deadline)
{
    std::unique_ptr<Request> q = std::make_unique<Request>();
    q->s = s;
    q->head_deadline = head_deadline;
    if (!read_head(*q) || !set_recv_timeout(s, RECV_SECONDS * 1000)) {
        close_sock(s);
        return;
    }
    const bool command = q->method == "POST" && q->target.starts_with("/cmd");
    size_t n = path_len(q->target);
    const bool events = q->method == "GET" && q->target.starts_with("/events");
    const bool leave = q->method == "POST" && q->target.starts_with("/leave");
    const bool files = n >= 6 && q->target.starts_with("/files") && (n == 6 || q->target[6] == '/');
    /* Static page assets are public; every protected route authenticates
       before validation, admission, serialization, or any body read. */
    if ((command || events || leave || files) && !token_ok(q->target)) {
        reply_text(s, "403 Forbidden", BAD_TOKEN);
        if (!connection_limited) drain(*q);
        close_sock(s);
        return;
    }
    if (connection_limited) {
        refuse_connection(s, "connection limit reached");
        return;
    }
    const bool upload = q->method == "PUT" && n > 7 && q->target.starts_with("/files/");
    RequestSlot upload_slot;
    if (upload) {
        if (!upload_slot.acquire(srv.nuploads, MAX_UPLOADS)) {
            refuse_connection(s, "upload limit reached");
            return; /* no drain: a refused upload's body is never read */
        }
    }
    const bool serial = !command && !upload;
    if (serial) pthread_mutex_lock(&serve_lock);
    if (q->target.size() >= TARGET_MAX) {
        reply_text(s, "414 URI Too Long", "address too long");
    } else if (q->has_length && q->length == ~0ULL) {
        reply_text(s, "400 Bad Request", "bad Content-Length");
    } else if (events) {
        open_events(s);
        pthread_mutex_unlock(&serve_lock);
        return;
    } else if (leave) {
        serve_leave(*q);
    } else if (command) {
        serve_cmd(*q);
    } else if (files) {
        serve_files(*q);
    } else if (q->method == "GET") {
        serve_asset(*q);
    } else reply(s, "405 Method Not Allowed", "text/plain", {});
    drain(*q);
    if (serial) pthread_mutex_unlock(&serve_lock);
    close_sock(s);
}

void answer_connection(sock_t s, bool connection_limited, long long head_deadline)
{
    try {
        handle(s, connection_limited, head_deadline);
    } catch (...) {
        xpp::out_of_memory_now("in the HTTP server");
    }
}

struct Connection {
    sock_t s;
    long long head_deadline;
};

void *connection_main(void *arg)
{
    RequestSlot connection_slot(&srv.nconnections);
    std::unique_ptr<Connection> connection(static_cast<Connection *>(arg));
    answer_connection(connection->s, false, connection->head_deadline);
    return nullptr;
}

/* A process xppautX starts (the web view's own processes, a browser
   opener, a second xppautX) must not inherit the listening socket, a
   page's connection or the log pipe: it would keep the port, or the pipe,
   after xppautX has gone. */
void no_inherit_sock(sock_t s)
{
#ifdef _WIN32
    SetHandleInformation(reinterpret_cast<HANDLE>(s), HANDLE_FLAG_INHERIT, 0);
#else
    fcntl(s, F_SETFD, FD_CLOEXEC);
#endif
}

#ifndef _WIN32
void no_inherit_fd(int fd) { fcntl(fd, F_SETFD, FD_CLOEXEC); }
#else
void no_inherit_fd(int) {} /* the pipe is made _O_NOINHERIT; a second xppautX is started inheriting nothing */
#endif

sock_t listener = INVALID_SOCKET;

void *http_main(void *)
{
    pthread_attr_t detached;
    pthread_t t;
    pthread_attr_init(&detached);
    pthread_attr_setdetachstate(&detached, PTHREAD_CREATE_DETACHED);
    for (;;) {
        sock_t s = accept(listener, nullptr, nullptr);
        if (s == INVALID_SOCKET) continue;
        const long long accepted_at = now_ms();
        no_inherit_sock(s);
        if (!set_recv_timeout(s, RECV_SECONDS * 1000) || !set_socket_timeout(s, SO_SNDTIMEO, SEND_SECONDS * 1000)) {
            close_sock(s); /* never serve with an unbounded socket if setting a timeout failed */
            continue;
        }
        RequestSlot slot;
        if (!slot.acquire(srv.nconnections, MAX_CONNECTION_THREADS)) {
            answer_connection(s, true, accepted_at + REFUSED_HEAD_SECONDS * 1000); /* head only: no thread, route or body */
            continue;
        }
        std::unique_ptr<Connection> connection;
        try {
            connection = std::make_unique<Connection>(Connection{s, accepted_at + HEAD_SECONDS * 1000});
        } catch (...) {
            xpp::out_of_memory_now("in the HTTP server");
        }
        if (pthread_create(&t, &detached, connection_main, connection.get()) != 0) {
            refuse_connection(s, "cannot start request thread");
        } else {
            connection.release(); /* connection_main owns the socket and deadline */
            slot.counter = nullptr; /* connection_main now owns the reservation */
        }
    }
    return nullptr;
}

/* the model stopped: say so in the page. After an error (no bye) keep
   serving so the page can show what xppaut printed. */
void at_exit()
{
    constexpr int EXIT_LOG_GRACE_MS = 200; /* let the log pipe's last output reach the page before exit */
    std::fflush(stdout);
    std::fflush(stderr);
#ifdef _WIN32
    Sleep(EXIT_LOG_GRACE_MS);
#else
    usleep(EXIT_LOG_GRACE_MS * 1000);
#endif
    try {
        std::string line = xpp::format("{{\"ev\":\"exit\",\"code\":{}}}", saw_bye ? 0 : 1);
        pthread_mutex_lock(&lock);
        srv.exit_event = line;
        pthread_mutex_unlock(&lock);
        emit_event(line);
    } catch (...) {
        xpp::out_of_memory_now("in the HTTP server");
    }
    pthread_mutex_lock(&lock);
    bool done = saw_bye || released; /* released: the window showing the page is closed */
    pthread_mutex_unlock(&lock);
    if (done) return;
    if (orig_stderr >= 0) {
        static constexpr std::string_view msg =
            "xppautX: the model stopped; the page shows what it printed. Ctrl+C (or closing its window) quits.\n";
        if (write(orig_stderr, msg.data(), static_cast<unsigned>(msg.size())) < 0) orig_stderr = -1;
    }
    /* until Ctrl+C, or until the window showing the page is closed */
    pthread_mutex_lock(&lock);
    while (!released) pthread_cond_wait(&released_cond, &lock);
    pthread_mutex_unlock(&lock);
}

void make_token()
{
    static constexpr std::string_view hex = "0123456789abcdef";
    constexpr size_t TOKEN_BYTES = 16; /* 128 random bits authenticate local browser requests */
    std::array<unsigned char, TOKEN_BYTES> bytes{};
#ifdef _WIN32
    for (unsigned char &b : bytes) {
        unsigned int v = 0;
        rand_s(&v);
        b = static_cast<unsigned char>(v);
    }
#else
    std::ifstream f("/dev/urandom", std::ios::binary);
    if (!f.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));
        for (unsigned char &b : bytes) b = static_cast<unsigned char>(rand());
    }
#endif
    std::string token;
    for (unsigned char b : bytes) {
        token += hex[b >> 4];
        token += hex[b & 15];
    }
    srv.token = token;
}

void open_in_browser(const std::string &url)
{
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    const char *opener = "xdg-open";
#ifdef __APPLE__
    opener = "open";
#endif
    if (getenv("WSL_DISTRO_NAME")) opener = "cmd.exe /c start";
    std::string cmd = xpp::format("{} '{}' >/dev/null 2>&1 &", opener, url);
    if (system(cmd.c_str()) != 0) xpp::log(XPP_LOG_WARN, "open {} in a browser\n", url);
#endif
}

int listen_on(int port)
{
    struct sockaddr_in addr{};
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return -1;
    no_inherit_sock(listener);
#ifndef _WIN32
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof one);
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* this machine only */
    addr.sin_port = htons(static_cast<unsigned short>(port));
    constexpr int LISTEN_BACKLOG = 16; /* queue a burst of browser connections while accept dispatches them */
    if (bind(listener, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr) != 0 || listen(listener, LISTEN_BACKLOG) != 0) {
        close_sock(listener);
        listener = INVALID_SOCKET;
        return -1;
    }
    socklen_t len = sizeof addr;
    getsockname(listener, reinterpret_cast<struct sockaddr *>(&addr), &len);
    return ntohs(addr.sin_port);
}

/* the "XPP: http://..." line tools and the VS Code extension read */
void print_address(const char *page_url)
{
    printf("XPP: %s\n", page_url);
    std::fflush(stdout);
}

std::array<int, 2> log_pipe;

void start_serving(int got, bool show, bool open)
{
    make_token();
    srv.page_url = xpp::format("http://127.0.0.1:{}/?t={}", got, srv.token);
    if (show) print_address(srv.page_url.c_str());

    /* what xppaut prints: to the terminal and the page */
    orig_stderr = fileno(stderr) >= 0 ? dup(fileno(stderr)) : -1;
    if (orig_stderr >= 0) no_inherit_fd(orig_stderr);
#ifdef _WIN32
    if (_pipe(log_pipe.data(), CHUNK, _O_BINARY | _O_NOINHERIT) == 0) {
#else
    if (pipe(log_pipe.data()) == 0) {
#endif
        no_inherit_fd(log_pipe[0]);
        no_inherit_fd(log_pipe[1]);
        dup2(log_pipe[1], xpp::files::stream_fd(stdout));
        dup2(log_pipe[1], xpp::files::stream_fd(stderr));
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        pthread_create(&log_thread, nullptr, log_main, &log_pipe[0]);
    }
    serving = true;
    pthread_create(&http_thread, nullptr, http_main, nullptr);
    pthread_create(&watchdog_thread, nullptr, watchdog_main, nullptr);
    atexit(at_exit);
    if (open) open_in_browser(srv.page_url);
}

} // namespace

/* ---- the API ---------------------------------------------------------------------- */

namespace xpp::http {

bool active() { return serving; }

void emit(std::string_view line)
{
    try {
        emit_event(line);
    } catch (...) {
        xpp::out_of_memory_now("in the HTTP server");
    }
}

void release()
{
    pthread_mutex_lock(&lock);
    released = true;
    pthread_cond_broadcast(&released_cond);
    pthread_mutex_unlock(&lock);
}

bool said_bye()
{
    pthread_mutex_lock(&lock);
    const bool bye = saw_bye;
    pthread_mutex_unlock(&lock);
    return bye;
}

const char *url() { return srv.page_url.c_str(); }

void show(bool open)
{
    print_address(srv.page_url.c_str());
    if (open) {
        try {
            open_in_browser(srv.page_url);
        } catch (...) {
            xpp::out_of_memory_now("in the HTTP server");
        }
    }
}

bool start(int port, bool show, bool open)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    int got = listen_on(port);
    if (got < 0 && port != 0) got = listen_on(0); /* taken: any free port */
    if (got < 0) {
        xpp::log_printf(XPP_LOG_ERROR, "xppautX: cannot open a port on 127.0.0.1\n");
        return false;
    }
    try {
        start_serving(got, show, open);
    } catch (...) {
        xpp::out_of_memory_now("in the HTTP server");
    }
    return true;
}

} // namespace xpp::http
