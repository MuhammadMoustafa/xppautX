/* The browser front end's HTTP server, inside xppautX (xpp_http.h).
   Serve the compiled-in page (web2/dist),
   stream protocol events to it (Server-Sent Events), take its commands by
   POST, and replay what a page that (re)connects needs to draw.

   Threads: the core runs on the main thread and calls xpp_http_emit; one
   thread accepts connections, each answered on a thread of its own (see
   handle()), which pushes the page's commands into the inbox (xpp_inbox.h),
   where the core takes them; one thread copies what xppaut prints to the
   terminal and into the page's log. The
   model's folder is served as /files (xpp_files.h: listing, reading, and
   uploads streamed to a temporary file). This file includes no core header
   but those small C APIs (xpp_inbox.h, xpp_files.h, xpp_log.h, xpp_io.h,
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

namespace {

constexpr int MAX_CLIENTS = 16;
constexpr int MAX_WINDOWS = 32;
constexpr size_t LOG_KEEP = 100000;

/* a window's create event, replayed to a page that (re)connects */
struct WindowLine {
    int win = 0;
    std::string line; /* empty: a free slot */
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
    std::string sticky_hello, sticky_state, sticky_ask, exit_event;
    std::array<WindowLine, MAX_WINDOWS> windows;
    std::string log_text; /* the last LOG_KEEP bytes printed */
};
Server &srv = *new Server;

int active;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* requests other than POST /cmd are answered one at a time (handle()) */
pthread_mutex_t serve_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t watchdog_thread;
pthread_t http_thread, log_thread;
int saw_bye, orig_stderr = -1;
/* at_exit's wait after an error ends when this is set (xpp_http_release) */
int released;
pthread_cond_t released_cond = PTHREAD_COND_INITIALIZER;

/* No exception may leave a thread or reach the C code that calls in: a
   failed allocation ends the program, as xpp_mem's do. _exit, not exit:
   at_exit would wait on the lock this thread may hold. */
[[noreturn]] void out_of_memory()
{
    xpp_log(XPP_LOG_ERROR, "xppautX: out of memory in the HTTP server\n");
    std::fflush(nullptr);
    _exit(1);
}

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
        int r = send(s, p, static_cast<int>(n > 65536 ? 65536 : n), 0);
#else
        ssize_t r = send(s, p, n, MSG_NOSIGNAL);
#endif
        if (r <= 0) return false;
        p += r;
        n -= static_cast<size_t>(r);
    }
    return true;
}

bool send_event(sock_t s, std::string_view line)
{
    return send_all(s, "data: ") && send_all(s, line) && send_all(s, "\n\n");
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

int had_client;
time_t alone_since;

void *watchdog_main(void *)
{
    for (;;) {
#ifdef _WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
        pthread_mutex_lock(&lock);
        for (int i = 0; i < srv.nclients; i++) {
            if (!send_all(srv.clients[i], ":\n\n")) drop_client(i--); /* a comment: the page ignores it */
        }
        if (srv.nclients > 0) alone_since = 0;
        else if (had_client && !alone_since) alone_since = time(nullptr);
        if (had_client && srv.nclients == 0 && alone_since
            && time(nullptr) - alone_since >= ALONE_SECONDS) {
            pthread_mutex_unlock(&lock);
            /* exit() would run at_exit(), which tells the page the program is
               going and waits on the same lock from this thread: it hangs, and
               there is no page left to tell anyway. Flush what the core wrote,
               then go. */
            std::fflush(nullptr);
            _exit(0);
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

void emit(std::string_view line)
{
    pthread_mutex_lock(&lock);
    if (std::optional<std::string_view> ev = field(line.substr(0, 40), "ev")) {
        if (*ev == "hello") srv.sticky_hello = line;
        else if (*ev == "state") srv.sticky_state = line;
        else if (*ev == "ask") srv.sticky_ask = line;
        else if (*ev == "idle") srv.sticky_ask.clear();
        else if (*ev == "bye") saw_bye = 1;
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
        xpp_inbox_push(one.data(), one.size());
        if (nl == std::string_view::npos) break;
        s.remove_prefix(nl + 1);
    }
}

/* ---- what xppaut prints ----------------------------------------------------------- */

/* {"ev":"log","text":"..."} for printed text */
std::string log_event(std::string_view text)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    std::string line;
    line.reserve(6 * text.size() + 32);
    line = "{\"ev\":\"log\",\"text\":\"";
    for (char ch : text) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c == '"' || c == '\\') {
            line += '\\';
            line += ch;
        } else if (c == '\n') {
            line += "\\n";
        } else if (c < 0x20 || c >= 0x80) {
            line += "\\u00";
            line += hex[c >> 4];
            line += hex[c & 15];
        } else line += ch;
    }
    line += "\"}";
    return line;
}

void *log_main(void *arg)
{
    int fd = *static_cast<int *>(arg);
    std::array<char, 4096> chunk;
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
            emit(log_event(got));
        }
    } catch (...) {
        out_of_memory();
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

void open_events(sock_t s)
{
    static constexpr std::string_view head = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\n\r\n";
    pthread_mutex_lock(&lock);
    bool ok = send_all(s, head);
    if (ok && !srv.log_text.empty()) ok = send_event(s, log_event(srv.log_text));
    if (ok && !srv.sticky_hello.empty()) ok = send_event(s, srv.sticky_hello);
    for (int i = 0; ok && i < MAX_WINDOWS; i++)
        if (!srv.windows[i].line.empty()) ok = send_event(s, srv.windows[i].line);
    if (ok && !srv.sticky_state.empty()) ok = send_event(s, srv.sticky_state);
    if (ok && !srv.sticky_ask.empty()) ok = send_event(s, srv.sticky_ask);
    if (ok && !srv.exit_event.empty()) ok = send_event(s, srv.exit_event);
    if (ok && srv.nclients < MAX_CLIENTS) {
        had_client = 1;
        alone_since = 0;
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
constexpr size_t CHUNK = 65536;
constexpr size_t METHOD_MAX = 7, TARGET_MAX = 1023; /* longer is cut, as sscanf's %7s %1023s did */

struct Request {
    sock_t s = INVALID_SOCKET;
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

/* reads the head; false when the connection is not a request worth an answer */
bool read_head(Request &q)
{
    size_t got = 0, i = 0;
    while (got < HEAD_MAX) {
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

/* POST /cmd: the whole body, then into the inbox */
void serve_cmd(Request &q)
{
    unsigned long long n = q.has_length ? q.length : q.have;
    if (!token_ok(q.target)) {
        reply_text(q.s, "403 Forbidden", "bad token");
        return;
    }
    if (n > CMD_MAX) {
        reply_text(q.s, "413 Payload Too Large", "command too long");
        return;
    }
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
    if (cmd.find("\"cmd\":\"answer\"") != std::string_view::npos) srv.sticky_ask.clear();
    if (srv.exit_event.empty()) push_command(cmd);
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
    default: return "500 Internal Server Error";
    }
}

void get_file(sock_t s, const std::string &name)
{
    FILE *raw = nullptr;
    unsigned long long size;
    int st = xpp_files_open(name.c_str(), &raw, &size);
    if (st != XPP_FILES_OK) {
        reply_text(s, files_status(st), xpp_files_status_text(st));
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
   NAME only once all of it arrived (xpp_files.h) */
void put_file(Request &q, const std::string &name)
{
    XppFilePut *put;
    unsigned long long left, size;
    std::array<char, 65> sha;
    if (!q.has_length) {
        reply_text(q.s, "411 Length Required", "a Content-Length is required");
        return;
    }
    if (q.length > XPP_FILES_CAP) { /* refused before a byte of the body is read */
        reply_text(q.s, "413 Payload Too Large", xpp_files_status_text(XPP_FILES_TOO_LARGE));
        return;
    }
    int st = xpp_files_put_begin(name.c_str(), XPP_FILES_CAP, &put);
    if (st != XPP_FILES_OK) {
        reply_text(q.s, files_status(st), xpp_files_status_text(st));
        return;
    }
    if (std::optional<std::string> v = header(q, "expect"); v && *v == "100-continue")
        send_all(q.s, "HTTP/1.1 100 Continue\r\n\r\n");
    left = q.length;
    {
        size_t first = q.have < left ? q.have : static_cast<size_t>(left);
        st = xpp_files_put_write(put, q.body0, first);
        left -= first;
    }
    std::vector<char> buf(CHUNK);
    while (st == XPP_FILES_OK && left > 0) {
        int r = take(q, buf.data(), static_cast<size_t>(left < CHUNK ? left : CHUNK));
        if (r <= 0) break; /* cut short, or the client stopped sending */
        st = xpp_files_put_write(put, buf.data(), static_cast<size_t>(r));
        left -= static_cast<unsigned long long>(r);
    }
    if (st != XPP_FILES_OK || left > 0) {
        xpp_files_put_abort(put);
        if (st != XPP_FILES_OK) reply_text(q.s, files_status(st), xpp_files_status_text(st));
        else reply_text(q.s, "400 Bad Request", "incomplete body");
        return;
    }
    st = xpp_files_put_commit(put, &size, sha.data());
    if (st != XPP_FILES_OK) {
        reply_text(q.s, files_status(st), xpp_files_status_text(st));
        return;
    }
    /* the name passed xpp_files_name_ok: no quote, backslash or control character */
    reply(q.s, "200 OK", "application/json",
          xpp::format("{{\"name\":\"{}\",\"size\":{},\"sha256\":\"{}\"}}", name, size, sha.data()));
}

/* /files (the listing), /files/NAME (GET, PUT): docs/protocol.md "Files" */
void serve_files(Request &q)
{
    size_t n = path_len(q.target);
    if (!token_ok(q.target)) {
        reply_text(q.s, "403 Forbidden", "bad token");
        return;
    }
    if (n == 6 || (n == 7 && q.target[6] == '/')) {
        if (q.method == "GET") {
            size_t len;
            xpp::MemPtr<char> json(xpp_files_list_json(&len));
            reply(q.s, "200 OK", "application/json", {json.get(), len});
        }
        else reply_text(q.s, "405 Method Not Allowed", "GET only");
        return;
    }
    std::optional<std::string> name = url_decode(std::string_view(q.target).substr(7, n - 7));
    if (!name || !xpp_files_name_ok(name->c_str())) {
        reply_text(q.s, files_status(XPP_FILES_BAD_NAME), xpp_files_status_text(XPP_FILES_BAD_NAME));
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
constexpr size_t LOCATION_MAX = 559;

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

/* a client that stops sending (a stalled upload) is dropped after a while */
void set_recv_timeout(sock_t s, int seconds)
{
#ifdef _WIN32
    DWORD ms = static_cast<DWORD>(seconds) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&ms), sizeof ms);
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof tv);
#endif
}

/* A request refused before its body was read (a bad token, a bad name)
   still has the body coming: closing on unread data resets the connection,
   and the client may lose the answer. A small rest is read and dropped
   first; a large one (an upload over the cap) is not waited for. */
constexpr unsigned long long DRAIN_MAX = 1ULL << 20;
void drain(Request &q)
{
    std::array<char, 4096> buf;
    if (!q.has_length || q.length == ~0ULL || q.consumed >= q.length) return;
    unsigned long long left = q.length - q.consumed;
    if (left > DRAIN_MAX) return;
    set_recv_timeout(q.s, 2);
    while (left > 0) {
        int r = take(q, buf.data(), left < buf.size() ? static_cast<size_t>(left) : buf.size());
        if (r <= 0) break;
        left -= static_cast<unsigned long long>(r);
    }
}

/* One connection, on its own thread (http_main): reading its head may
   wait up to RECV_SECONDS, since a browser opens connections it sends
   nothing on yet (a preconnect, a spare socket), and that wait must not
   hold up the next request: with one thread doing it all, an Abort POSTed
   meanwhile waited those 30 s while the run went on (T25). A command is
   pushed as soon as it has arrived; everything else is answered one at a
   time, as when a single thread answered them all (an upload, the event
   streams' registration). The page keeps its commands in order by sending
   the next one once the last was answered (web2's HttpTransport). */
void handle(sock_t s)
{
    std::unique_ptr<Request> q = std::make_unique<Request>();
    q->s = s;
    if (!read_head(*q)) {
        close_sock(s);
        return;
    }
    const bool command = q->method == "POST" && q->target.starts_with("/cmd");
    const bool serial = !command;
    if (serial) pthread_mutex_lock(&serve_lock);
    size_t n = path_len(q->target);
    if (q->target.size() >= TARGET_MAX) {
        reply_text(s, "414 URI Too Long", "address too long");
    } else if (q->has_length && q->length == ~0ULL) {
        reply_text(s, "400 Bad Request", "bad Content-Length");
    } else if (q->method == "GET" && q->target.starts_with("/events")) {
        if (token_ok(q->target)) {
            open_events(s);
            pthread_mutex_unlock(&serve_lock);
            return;
        }
        reply_text(s, "403 Forbidden", "bad token");
    } else if (command) {
        serve_cmd(*q);
    } else if (n >= 6 && q->target.starts_with("/files") && (n == 6 || q->target[6] == '/')) {
        serve_files(*q);
    } else if (q->method == "GET") {
        serve_asset(*q);
    } else reply(s, "405 Method Not Allowed", "text/plain", {});
    drain(*q);
    if (serial) pthread_mutex_unlock(&serve_lock);
    close_sock(s);
}

void *connection_main(void *arg)
{
    try {
        handle(static_cast<sock_t>(reinterpret_cast<uintptr_t>(arg)));
    } catch (...) {
        out_of_memory();
    }
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
        no_inherit_sock(s);
        set_recv_timeout(s, RECV_SECONDS);
        if (pthread_create(&t, &detached, connection_main, reinterpret_cast<void *>(static_cast<uintptr_t>(s))) != 0)
            connection_main(reinterpret_cast<void *>(static_cast<uintptr_t>(s))); /* no thread to spare: answered here, as it always was */
    }
    return nullptr;
}

/* the model stopped: say so in the page. After an error (no bye) keep
   serving so the page can show what xppaut printed. */
void at_exit()
{
    std::fflush(stdout);
    std::fflush(stderr);
#ifdef _WIN32
    Sleep(200);
#else
    usleep(200000);
#endif
    try {
        std::string line = xpp::format("{{\"ev\":\"exit\",\"code\":{}}}", saw_bye ? 0 : 1);
        pthread_mutex_lock(&lock);
        srv.exit_event = line;
        pthread_mutex_unlock(&lock);
        emit(line);
    } catch (...) {
        out_of_memory();
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
    std::array<unsigned char, 16> bytes{};
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
    if (bind(listener, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr) != 0 || listen(listener, 16) != 0) {
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

/* the descriptor a standard stream writes through, given one if it has
   none: the Windows exe is a GUI-subsystem program, and started with no
   console (Explorer, a shortcut, Start-Process) the C library leaves stdout
   and stderr without a descriptor (_fileno -2), where a dup2 onto 1 and 2
   never reaches them and all the core prints, AUTO's table included, was
   lost to the page (T27). The pipe goes onto the stream's own descriptor,
   whichever it is (also a stream xpp_win32_attach_console reopened). */
int stream_fd(FILE *f)
{
#ifdef _WIN32
    if (_fileno(f) < 0 && !freopen("NUL", "w", f)) return -1;
#endif
    return fileno(f);
}

std::array<int, 2> log_pipe;

void start(int got, int flags)
{
    make_token();
    srv.page_url = xpp::format("http://127.0.0.1:{}/?t={}", got, srv.token);
    if (flags & XPP_HTTP_SHOW) print_address(srv.page_url.c_str());

    /* what xppaut prints: to the terminal and the page */
    orig_stderr = fileno(stderr) >= 0 ? dup(fileno(stderr)) : -1;
    if (orig_stderr >= 0) no_inherit_fd(orig_stderr);
#ifdef _WIN32
    if (_pipe(log_pipe.data(), 65536, _O_BINARY | _O_NOINHERIT) == 0) {
#else
    if (pipe(log_pipe.data()) == 0) {
#endif
        no_inherit_fd(log_pipe[0]);
        no_inherit_fd(log_pipe[1]);
        dup2(log_pipe[1], stream_fd(stdout));
        dup2(log_pipe[1], stream_fd(stderr));
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        pthread_create(&log_thread, nullptr, log_main, &log_pipe[0]);
    }
    active = 1;
    pthread_create(&http_thread, nullptr, http_main, nullptr);
    pthread_create(&watchdog_thread, nullptr, watchdog_main, nullptr);
    atexit(at_exit);
    if (flags & XPP_HTTP_OPEN) open_in_browser(srv.page_url);
}

} // namespace

/* ---- the C API ------------------------------------------------------------------- */

int xpp_http_active(void) { return active; }

void xpp_http_emit(const char *line, size_t n)
{
    try {
        emit(std::string_view(line, n));
    } catch (...) {
        out_of_memory();
    }
}

void xpp_http_release(void)
{
    pthread_mutex_lock(&lock);
    released = 1;
    pthread_cond_broadcast(&released_cond);
    pthread_mutex_unlock(&lock);
}

int xpp_http_said_bye(void)
{
    pthread_mutex_lock(&lock);
    int bye = saw_bye;
    pthread_mutex_unlock(&lock);
    return bye;
}

const char *xpp_http_url(void) { return srv.page_url.c_str(); }

void xpp_http_show(int open)
{
    print_address(srv.page_url.c_str());
    if (open) {
        try {
            open_in_browser(srv.page_url);
        } catch (...) {
            out_of_memory();
        }
    }
}

int xpp_http_start(int port, int flags)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    int got = listen_on(port);
    if (got < 0 && port != 0) got = listen_on(0); /* taken: any free port */
    if (got < 0) {
        xpp_log(XPP_LOG_ERROR, "xppautX: cannot open a port on 127.0.0.1\n");
        return 0;
    }
    try {
        start(got, flags);
    } catch (...) {
        out_of_memory();
    }
    return 1;
}
