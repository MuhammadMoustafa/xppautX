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
    if (!proto) return; /* -silent: nowhere */
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
        /* end of input: exit 1 for a script that hit an error or an
           unmatched ask (docs/protocol.md "Scripts"), else as always, 0 */
        quit_session();
    default:
        return nullptr;
    }
}

unsigned long read_line_seq(void) { return line_seq; }

bool read_line_refused(void) { return line_refused; }

/* ---- a small JSON reader for flat command objects ------------------------ */

const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

const char *skip_value(const char *p)
{
    int depth = 0;
    p = skip_ws(p);
    do {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (*p) p++;
        } else if (*p == '[' || *p == '{') {
            depth++;
            p++;
        } else if (*p == ']' || *p == '}') {
            if (depth == 0) return p;
            depth--;
            p++;
        } else if (*p == ',' && depth == 0) {
            return p;
        } else if (*p) {
            p++;
        }
        if (depth == 0 && (*p == ',' || *p == '}' || *p == ']')) return p;
    } while (*p);
    return p;
}

namespace {

/* one JSON value at p, strictly (RFC 8259, nesting at most 64 deep): past
   it, or NULL when it is not one */
const char *valid_value(const char *p, int depth)
{
    p = skip_ws(p);
    if (depth > 64) return nullptr;
    if (*p == '{' || *p == '[') {
        const char close = *p == '{' ? '}' : ']';
        p = skip_ws(p + 1);
        if (*p == close) return p + 1;
        for (;;) {
            if (close == '}') {
                if (*p != '"' || !(p = valid_value(p, depth + 1))) return nullptr;
                p = skip_ws(p);
                if (*p++ != ':') return nullptr;
            }
            if (!(p = valid_value(p, depth + 1))) return nullptr;
            p = skip_ws(p);
            if (*p == close) return p + 1;
            if (*p++ != ',') return nullptr;
            p = skip_ws(p);
        }
    }
    if (*p == '"') {
        for (p++; *p != '"'; p++) {
            if (static_cast<unsigned char>(*p) < 0x20) return nullptr; /* the end, or a raw control character */
            if (*p == '\\') {
                p++;
                if (*p == 'u') {
                    for (int i = 1; i <= 4; i++)
                        if (!std::isxdigit(static_cast<unsigned char>(p[i]))) return nullptr;
                    p += 4;
                } else if (!*p || !std::strchr("\"\\/bfnrt", *p)) {
                    return nullptr;
                }
            }
        }
        return p + 1;
    }
    for (const char *word : {"true", "false", "null"})
        if (std::strncmp(p, word, std::strlen(word)) == 0) return p + std::strlen(word);
    const char *start = p;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else if (*p >= '1' && *p <= '9')
        while (*p >= '0' && *p <= '9') p++;
    else return nullptr;
    if (*p == '.') {
        if (!(*++p >= '0' && *p <= '9')) return nullptr;
        while (*p >= '0' && *p <= '9') p++;
    }
    if (*p == 'e' || *p == 'E') {
        if (*++p == '+' || *p == '-') p++;
        if (!(*p >= '0' && *p <= '9')) return nullptr;
        while (*p >= '0' && *p <= '9') p++;
    }
    return p > start ? p : nullptr;
}

} // namespace

bool js_valid(const char *text)
{
    const char *end = valid_value(text, 0);
    return end && *skip_ws(end) == '\0';
}

/* value of member key in the object at obj, or NULL */
const char *js_find(const char *obj, const char *key)
{
    const char *p = skip_ws(obj);
    std::string_view want(key);
    if (*p != '{') return nullptr;
    p++;
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') return nullptr;
        const char *k = ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (!*p) return nullptr;
        bool match = std::string_view(k, static_cast<size_t>(p - k)) == want;
        p = skip_ws(p + 1);
        if (*p != ':') return nullptr;
        p = skip_ws(p + 1);
        if (match) return p;
        p = skip_ws(skip_value(p));
        if (*p != ',') return nullptr;
        p++;
    }
}

std::string_view js_raw(const char *v)
{
    if (!v) return {};
    const char *end = skip_value(v);
    while (end > v && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
    return std::string_view(v, static_cast<size_t>(end - v));
}

std::string js_object_without(const char *obj, std::initializer_list<std::string_view> drop)
{
    std::string out = "{";
    const char *p = skip_ws(obj);
    if (*p == '{') p++;
    for (;;) {
        p = skip_ws(p);
        if (*p != '"') break;
        const char *k = p++;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (!*p) break;
        const std::string_view key(k + 1, static_cast<size_t>(p - k - 1));
        const char *colon = skip_ws(p + 1);
        if (*colon != ':') break;
        const char *v = skip_ws(colon + 1);
        if (std::find(drop.begin(), drop.end(), key) == drop.end()) {
            if (out.size() > 1) out += ',';
            out.append(k, static_cast<size_t>(p + 1 - k));
            out += ':';
            out += js_raw(v);
        }
        p = skip_ws(skip_value(v));
        if (*p != ',') break;
        p++;
    }
    return out + "}";
}

bool js_string(const char *v, std::string &out, size_t max)
{
    try {
        return xpp::json_decode_string(v, out, max, /*strict=*/false);
    } catch (...) {
        xpp::out_of_memory("reading a command");
    }
}

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

const char *js_elem(const char *arr, int i)
{
    if (!arr || *arr != '[') return nullptr;
    const char *p = skip_ws(arr + 1);
    if (*p == ']') return nullptr;
    while (i-- > 0) {
        p = skip_ws(skip_value(p));
        if (*p != ',') return nullptr;
        p = skip_ws(p + 1);
    }
    return p;
}

bool get_string(const char *obj, const char *key, std::string &out, size_t max)
{
    return js_string(js_find(obj, key), out, max);
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
    return get_string(line, "cmd", c, 32) && c == name;
}

/* a JSON number at v into *out; 0 for anything else (a string, null, true) */
int js_number(const char *v, double *out)
{
    char *end;
    if (!v || !(*v == '-' || (*v >= '0' && *v <= '9'))) return 0;
    *out = strtod(v, &end);
    return end != v;
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
