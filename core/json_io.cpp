/* The JSON protocol's lines (docs/protocol.md): the event lines that go
   out, to the protocol's stdout or the page xppautX serves; the command
   lines that come in, from the inbox; and a small reader for the flat
   JSON objects they are. */
#include "ui_json_internal.h"
#include "session.h"
#include "json_number.h"
#include "xpp_mem.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_win32.h"
#include "mykeydef.h"
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <string_view>
#include <unistd.h>

namespace xpp::json {

namespace {

FILE *proto;

} // namespace

/* ---- output ------------------------------------------------------------ */

/* one step of building an event: a failed allocation ends the program */
template <class F> static void build_event(F &&step)
{
    try {
        step();
    } catch (...) {
        xpp::out_of_memory("building an event");
    }
}

void buf_add(std::string &b, const char *s, size_t n)
{
    build_event([&] { b.append(s, n); });
}

void buf_str(std::string &b, const char *s)
{
    buf_str(b, s ? std::string_view(s) : std::string_view());
}

void buf_str(std::string &b, std::string_view s)
{
    BUF_LIT(b, "\"");
    xpp::json_encode_string(b, s);
    BUF_LIT(b, "\"");
}

void buf_str_array(std::string &b, const char *const *v, int n)
{
    BUF_LIT(b, "[");
    for (int i = 0; i < n; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, v ? v[i] : "");
    }
    BUF_LIT(b, "]");
}

void buf_str_array(std::string &b, std::span<const std::string> v)
{
    BUF_LIT(b, "[");
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, v[i]);
    }
    BUF_LIT(b, "]");
}

void buf_num(std::string &b, double v, int sig)
{
    build_event([&] { json_append_number(b, v, sig); });
}

bool out_wanted() { return xpp::http::active() || proto; }

/* one event line to the client: stdout, or the page xppautX serves. Whole
   under a lock: the inbox's busy reply comes from a reader thread while
   the core writes (W389). */
void out_line(const char *s, size_t n)
{
    static std::mutex out_lock;
    std::lock_guard<std::mutex> whole(out_lock);
    if (xpp::http::active()) {
        xpp::http::emit({s, n});
        return;
    }
    if (!proto) return; /* --silent: nowhere */
    fwrite(s, 1, n, proto);
    fputc('\n', proto);
}

void out_flush(void)
{
    if (proto) fflush(proto);
}

/* stdout becomes the protocol's (json_ui_install): output goes to the file
   descriptor that was stdout, and stdout itself is pointed at stderr so the
   core's own printing never corrupts the stream */
void open_protocol_stdout(void)
{
    int fd = dup(1);
#ifdef _WIN32
    xpp::win32::binary_mode(fd); /* "\n" line ends, not "\r\n" */
    xpp::win32::binary_mode(0);
#endif
    proto = fdopen(fd, "w");
    dup2(2, 1);
}

/* what is pending goes out before any other event: the AUTO diagram's
   points (json_auto.cpp); before every event, from anywhere: the
   client's session's */
void flush_pending(void) { diag_flush(client(), 0); }

/* one complete event line */
void send_buf(std::string &b)
{
    flush_pending();
    out_line(b.data(), b.size());
    out_flush();
    b.clear();
}

void send_simple(std::string_view ev, const char *key, std::string_view text)
{
    std::string b = xpp::events::open(ev);
    if (key) {
        buf_format(b, ",\"{}\":", key);
        buf_str(b, text);
    }
    BUF_LIT(b, "}");
    send_buf(b);
}

void json_flush(void)
{
    flush_pending();
    send_state_if_dirty();
    out_flush();
}

/* a plots or series event: after the pending drawing, like any event */
void data_emit(std::string_view line)
{
    flush_pending();
    out_line(line.data(), line.size());
    out_flush();
}

void data_emit(const char *line, size_t n) { data_emit(std::string_view(line, n)); }

/* ---- input ------------------------------------------------------------- */

/* Input comes from the inbox (xpp_inbox.h): reader threads fill it, the
   HTTP server in browser mode and a stdin reader with --server, so the core
   never reads a descriptor itself.

   Which queue a reader takes from (see classify() in ui_json.cpp): the
   command loop takes lines in the order they came (inbox::From::arrival),
   a prompt the control lines first (From::any), a long computation's
   checkpoint only the control queue, so it never takes (and never drops) a
   command meant to run after it.

   The next line (without newline) from `which`, in a buffer that stays
   valid until the next call, with its sequence number in line_seq
   (read_line_seq()); NULL when nothing came within wait_ms (< 0 blocks, 0
   polls). Lines can be long (the pixels of a window in an answer). End of
   input quits. */

namespace {

unsigned long line_seq;
bool line_refused;
std::string last_line; /* the line read_line gave last, freed at the next call */

} // namespace

char *read_line(xpp::inbox::From which, int wait_ms)
{
    std::string().swap(last_line); /* a pixels answer is megabytes: not kept while waiting */
    switch (xpp::inbox::next(which, wait_ms, last_line, line_seq, line_refused)) {
    case xpp::inbox::Took::line:
        return last_line.data();
    case xpp::inbox::Took::end:
        /* End of input: --silent's error accounting, otherwise 0. */
        quit_session();
    default:
        return nullptr;
    }
}

unsigned long read_line_seq(void) { return line_seq; }

bool read_line_refused(void) { return line_refused; }

namespace {

/* whether the command running on this thread (handle_line's) refuses an
   argument that is not what its reader asks for; the reader thread's own
   calls (classify) never do */
thread_local bool checking_arguments = false;

} // namespace

ArgumentCheck::ArgumentCheck() : before_(checking_arguments) { checking_arguments = true; }
ArgumentCheck::~ArgumentCheck() { checking_arguments = before_; }

/* The argument at v as a number: true, false and null are 1 and 0 (a flag
   sent as a boolean), a number is itself, absent is def; a string, an
   object, an array or a token that is not all one number is a BadArgument
   in a command, and the old reading (a string is def, the number's prefix)
   on the reader thread. */
double js_num(const char *v, double def)
{
    if (!v) return def;
    if (strncmp(v, "true", 4) == 0) return 1;
    if (strncmp(v, "false", 5) == 0 || strncmp(v, "null", 4) == 0) return 0;
    double d;
    if (js_number(v, &d)) return d;
    if (checking_arguments) throw BadArgument{"", std::string(js_raw(v)), "a number"};
    if (*v == '"') return def;
    return atof(v);
}

xpp::Result<> read_save_replace(const char *line, int &decision)
{
    const char *permission = js_find(line, "replace");
    if (!permission) return {};
    double replace;
    if (!js_number(permission, &replace) ||
        (replace != static_cast<double>(SAVE_ASK) && replace != static_cast<double>(SAVE_REPLACE) &&
         replace != static_cast<double>(SAVE_DECLINE)))
        return xpp::fail("save", "replace must be 1 (save), -1 (decline), or 0 (ask)", xpp::command_place());
    decision = static_cast<int>(replace);
    return {};
}

bool get_string(const char *obj, const char *key, std::string &out)
{
    return js_string(js_find(obj, key), out);
}

double get_num(const char *obj, const char *key, double def)
{
    try {
        return js_num(js_find(obj, key), def);
    } catch (BadArgument &bad) {
        bad.key = key;
        throw;
    }
}

/* a whole number int holds (a value outside int's range saturates, which
   every caller's own range check refuses; NaN is the default: never an
   undefined conversion); in a command a fraction is a BadArgument */
int get_int(const char *obj, const char *key, double def)
{
    const double v = get_num(obj, key, def);
    if (std::isnan(v)) return static_cast<int>(def);
    if (checking_arguments && v != std::floor(v)) throw BadArgument{key, std::string(js_raw(js_find(obj, key))), "a whole number"};
    if (v >= static_cast<double>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
    if (v <= static_cast<double>(std::numeric_limits<int>::min())) return std::numeric_limits<int>::min();
    return static_cast<int>(v);
}

int is_cmd(const char *line, const char *name)
{
    std::string c;
    return get_string(line, "cmd", c) && c == name;
}

/* a JSON number at v into *out; 0 for anything else (a string, null, true,
   or a token that is not all one number: xpp::parse_number's rule, W131) */
int js_number(const char *v, double *out)
{
    if (!v || !(*v == '-' || (*v >= '0' && *v <= '9'))) return 0;
    const char *end = v + strcspn(v, ",}] \t\r\n");
    return xpp::parse_number(std::string_view(v, static_cast<size_t>(end - v)), *out);
}

int js_whole(const char *v, int *out)
{
    double d;
    if (!js_number(v, &d) || d != std::floor(d) || d < INT_MIN || d > INT_MAX) return 0;
    *out = static_cast<int>(d);
    return 1;
}

bool get_whole(const char *obj, const char *key, int &out)
{
    const char *v = js_find(obj, key);
    return !v || js_whole(v, &out);
}

/* key names as the client sends them (DOM KeyboardEvent.key or X keysym
   names) to the codes get_key_press() gives the core */
int key_code(const char *k)
{
    static const struct {
        const char *name;
        int code;
    } named[] = {
        {"Escape", ESC}, {"Enter", FINE}, {"Return", FINE}, {"Tab", TAB},
        {"Backspace", BKSP}, {"BackSpace", BKSP}, {"Delete", DEL},
        {"Home", HOME}, {"End", END}, {"ArrowLeft", LEFT}, {"Left", LEFT},
        {"ArrowRight", RIGHT}, {"Right", RIGHT}, {"ArrowUp", UP}, {"Up", UP},
        {"ArrowDown", DOWN}, {"Down", DOWN}, {"PageUp", PGUP}, {"Prior", PGUP},
        {"PageDown", PGDN}, {"Next", PGDN}, {" ", ' '}, {"space", ' '},
    };
    if (k[0] && !k[1]) return static_cast<unsigned char>(k[0]);
    for (const auto &n : named)
        if (strcmp(k, n.name) == 0) return n.code;
    return BADKEY;
}

} // namespace xpp::json
