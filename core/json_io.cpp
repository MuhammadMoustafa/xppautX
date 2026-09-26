/* The JSON protocol's lines (docs/protocol.md): the event lines that go
   out, to the protocol's stdout or the page xppautX serves; the command
   lines that come in, from the inbox; and a small reader for the flat
   JSON objects they are. */
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_log.h"
#include "xpp_win32.h"
#include "mykeydef.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unistd.h>

namespace xpp::json {

namespace {

FILE *proto;

} // namespace

[[noreturn]] void out_of_memory(const char *what)
{
    xpp_log(XPP_LOG_ERROR, "out of memory %s\n", what);
    std::exit(1);
}

/* ---- output ------------------------------------------------------------ */

void buf_add(Buf *b, const char *s, size_t n)
{
    try {
        b->s.append(s, n);
    } catch (...) {
        out_of_memory("building an event");
    }
}

void buf_str(Buf *b, const char *s)
{
    static constexpr std::string_view hex = "0123456789abcdef";
    BUF_LIT(b, "\"");
    for (; s && *s; s++) {
        unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            const std::array<char, 2> esc{'\\', *s};
            buf_add(b, esc.data(), esc.size());
        } else if (c == '\n') BUF_LIT(b, "\\n");
        else if (c == '\t') BUF_LIT(b, "\\t");
        else if (c < 0x20 || c >= 0x80) {
            /* the core's strings are ASCII or Latin-1; keep the byte value */
            const std::array<char, 6> esc{'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            buf_add(b, esc.data(), esc.size());
        } else buf_add(b, s, 1);
    }
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

/* one event line to the client: stdout, or the page xppautX serves */
void out_line(const char *s, size_t n)
{
    if (xpp_http_active()) {
        xpp_http_emit(s, n);
        return;
    }
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
    xpp_binary_mode(fd); /* "\n" line ends, not "\r\n" */
    xpp_binary_mode(0);
#endif
    proto = fdopen(fd, "w");
    dup2(2, 1);
}

/* what is pending goes out before any other event: the AUTO diagram's
   points (json_auto.cpp) */
void flush_pending(void) { diag_flush(0); }

/* one complete event line */
void send_buf(Buf *b)
{
    flush_pending();
    out_line(b->s.data(), b->s.size());
    out_flush();
    b->s.clear();
}

void send_simple(const char *ev, const char *key, const char *text)
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
void data_emit(const char *line, size_t n)
{
    flush_pending();
    out_line(line, n);
    out_flush();
}

/* ---- input ------------------------------------------------------------- */

/* Input comes from the inbox (xpp_inbox.h): reader threads fill it, the
   HTTP server in browser mode and a stdin reader with --server, so the core
   never reads a descriptor itself.

   Which queue a reader takes from (see classify() in ui_json.cpp): the
   command loop takes lines in the order they came (XPP_INBOX_ARRIVAL), a
   prompt the control lines first (XPP_INBOX_ANY), a long computation's
   checkpoint only the control queue, so it never takes (and never drops) a
   command meant to run after it.

   The next line (without newline) from `which`, in a buffer that stays
   valid until the next call, with its sequence number in line_seq
   (read_line_seq()); NULL when nothing came within wait_ms (< 0 blocks, 0
   polls). Lines can be long (the pixels of a window in an answer). End of
   input quits. */

namespace {

unsigned long line_seq;
MemPtr<char> last_line; /* the inbox's block, freed at the next call */

} // namespace

char *read_line(int which, int wait_ms)
{
    char *line = nullptr;
    last_line.reset();
    int r = xpp_inbox_next(which, wait_ms, &line, &line_seq);
    last_line.reset(line);
    switch (r) {
    case 1:
        return line;
    case -1:
        /* end of input: exit 1 for a script that hit an error or an
           unmatched ask (docs/protocol.md "Scripts"), else as always, 0 */
        quit_session();
    default:
        return nullptr;
    }
}

unsigned long read_line_seq(void) { return line_seq; }

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

bool js_string(const char *v, std::string &out, size_t max)
{
    out.clear();
    if (!v || *v != '"') return false;
    v++;
    try {
        while (*v && *v != '"') {
            char c = *v++;
            if (c == '\\' && *v) {
                c = *v++;
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c == 'u') {
                    unsigned u = 0;
                    for (int i = 0; i < 4 && *v; i++, v++)
                        u = u * 16 + static_cast<unsigned>(*v <= '9' ? *v - '0' : (*v | 32) - 'a' + 10);
                    c = u < 256 ? static_cast<char>(u) : '?';
                }
            }
            if (out.size() + 1 < max) out += c;
        }
    } catch (...) {
        out_of_memory("reading a command");
    }
    return true;
}

int js_string(const char *v, char *out, int max)
{
    std::string s;
    bool ok = js_string(v, s, max > 0 ? static_cast<size_t>(max) : 1);
    if (max > 0) std::memcpy(out, s.c_str(), s.size() + 1);
    return ok;
}

double js_num(const char *v, double def)
{
    if (!v) return def;
    if (*v == '"') return def;
    if (strncmp(v, "true", 4) == 0) return 1;
    if (strncmp(v, "false", 5) == 0 || strncmp(v, "null", 4) == 0) return 0;
    return atof(v);
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

int get_str(const char *obj, const char *key, char *out, int max)
{
    return js_string(js_find(obj, key), out, max);
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
