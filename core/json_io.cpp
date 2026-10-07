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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unistd.h>

namespace xpp::json {

namespace {

FILE *proto;

} // namespace

/* ---- output ------------------------------------------------------------ */

void buf_add(Buf *b, const char *s, size_t n)
{
    try {
        b->s.append(s, n);
    } catch (...) {
        xpp::out_of_memory("building an event");
    }
}

void buf_str(Buf *b, const char *s)
{
    buf_str(b, s ? std::string_view(s) : std::string_view());
}

void buf_str(Buf *b, std::string_view s)
{
    BUF_LIT(b, "\"");
    xpp::json_encode_string(b->s, s);
    BUF_LIT(b, "\"");
}

void buf_str_array(Buf *b, const char *const *v, int n)
{
    BUF_LIT(b, "[");
    for (int i = 0; i < n; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, v ? v[i] : "");
    }
    BUF_LIT(b, "]");
}

void buf_str_array(Buf *b, std::span<const std::string> v)
{
    BUF_LIT(b, "[");
    for (std::size_t i = 0; i < v.size(); i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, v[i]);
    }
    BUF_LIT(b, "]");
}

void buf_num(Buf *b, double v, int sig)
{
    try {
        json_append_number(b->s, v, sig);
    } catch (...) {
        xpp::out_of_memory("building an event");
    }
}

/* one event line to the client: stdout, or the page xppautX serves */
void out_line(const char *s, size_t n)
{
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
void send_buf(Buf *b)
{
    flush_pending();
    out_line(b->s.data(), b->s.size());
    out_flush();
    b->s.clear();
}

void send_simple(const char *ev, const char *key, std::string_view text)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"{}\"", ev);
    if (key) {
        buf_format(&b, ",\"{}\":", key);
        buf_str(&b, text);
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
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

double js_num(const char *v, double def)
{
    if (!v) return def;
    if (*v == '"') return def;
    if (strncmp(v, "true", 4) == 0) return 1;
    if (strncmp(v, "false", 5) == 0 || strncmp(v, "null", 4) == 0) return 0;
    return atof(v);
}

int get_range(const char *obj, const char *key, xpp::AxisRange &r)
{
    const char *v = js_find(obj, key);
    if (!v) return 0;
    if (strncmp(v, "null", 4) == 0) {
        r = xpp::AxisRange();
        return 1;
    }
    double lo, hi;
    const char *a = js_elem(v, 0), *b = js_elem(v, 1);
    if (!a || !b || !js_number(a, &lo) || !js_number(b, &hi) || !(lo < hi)) return -1;
    r.set = true;
    r.lo = lo;
    r.hi = hi;
    return 1;
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
    return js_num(js_find(obj, key), def);
}

int get_int(const char *obj, const char *key, double def)
{
    return static_cast<int>(get_num(obj, key, def));
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
