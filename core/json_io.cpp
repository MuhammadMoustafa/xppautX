/* The JSON protocol's lines (docs/protocol.md): the event lines that go
   out, to the protocol's stdout or the page xppautX serves; the command
   lines that come in, from the inbox; and a small reader for the flat
   JSON objects they are. */
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_win32.h"
#include "mykeydef.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

namespace xpp::json {

namespace {

FILE *proto;

} // namespace

/* ---- output ------------------------------------------------------------ */

void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2 + 4096;
        b->s = static_cast<char *>(xpp_realloc(b->s, b->cap));
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}

void buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof tmp) n = sizeof tmp - 1;
    buf_add(b, tmp, n);
}

void buf_str(Buf *b, const char *s)
{
    char esc[8];
    BUF_LIT(b, "\"");
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            esc[0] = '\\'; esc[1] = c;
            buf_add(b, esc, 2);
        } else if (c == '\n') BUF_LIT(b, "\\n");
        else if (c == '\t') BUF_LIT(b, "\\t");
        else if (c < 0x20 || c >= 0x80) {
            /* the core's strings are ASCII or Latin-1; keep the byte value */
            snprintf(esc, sizeof esc, "\\u%04x", c);
            buf_add(b, esc, 6);
        } else buf_add(b, (char *)&c, 1);
    }
    BUF_LIT(b, "\"");
}

void buf_str_array(Buf *b, char **v, int n)
{
    int i;
    BUF_LIT(b, "[");
    for (i = 0; i < n; i++) {
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
    out_line(b->s, b->len);
    out_flush();
    b->len = 0;
}

void send_simple(const char *ev, const char *key, const char *text)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"%s\"", ev);
    if (key) {
        buf_printf(&b, ",\"%s\":", key);
        buf_str(&b, text);
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
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

} // namespace

char *read_line(int which, int wait_ms)
{
    static char *line;
    xpp_free(line);
    line = NULL;
    switch (xpp_inbox_next(which, wait_ms, &line, &line_seq)) {
    case 1:
        return line;
    case -1:
        /* end of input: exit 1 for a script that hit an error or an
           unmatched ask (docs/protocol.md "Scripts"), else as always, 0 */
        quit_session();
    default:
        return NULL;
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
    size_t klen = strlen(key);
    if (*p != '{') return NULL;
    p++;
    for (;;) {
        const char *k;
        p = skip_ws(p);
        if (*p != '"') return NULL;
        k = ++p;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (!*p) return NULL;
        {
            int match = (size_t)(p - k) == klen && strncmp(k, key, klen) == 0;
            p = skip_ws(p + 1);
            if (*p != ':') return NULL;
            p = skip_ws(p + 1);
            if (match) return p;
        }
        p = skip_ws(skip_value(p));
        if (*p != ',') return NULL;
        p++;
    }
}

int js_string(const char *v, char *out, int max)
{
    int n = 0;
    if (!v || *v != '"') {
        if (max > 0) out[0] = 0;
        return 0;
    }
    v++;
    while (*v && *v != '"') {
        char c = *v++;
        if (c == '\\' && *v) {
            c = *v++;
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'u') {
                unsigned u = 0;
                int i;
                for (i = 0; i < 4 && *v; i++, v++)
                    u = u * 16 + (*v <= '9' ? *v - '0' : (*v | 32) - 'a' + 10);
                c = u < 256 ? (char)u : '?';
            }
        }
        if (n < max - 1) out[n++] = c;
    }
    out[n] = 0;
    return 1;
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
    const char *p;
    if (!arr || *arr != '[') return NULL;
    p = skip_ws(arr + 1);
    if (*p == ']') return NULL;
    while (i-- > 0) {
        p = skip_ws(skip_value(p));
        if (*p != ',') return NULL;
        p = skip_ws(p + 1);
    }
    return p;
}

int get_str(const char *obj, const char *key, char *out, int max)
{
    return js_string(js_find(obj, key), out, max);
}

double get_num(const char *obj, const char *key, double def)
{
    return js_num(js_find(obj, key), def);
}

int is_cmd(const char *line, const char *name)
{
    char c[32];
    return get_str(line, "cmd", c, sizeof c) && strcmp(c, name) == 0;
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
    static const struct { const char *name; int code; } named[] = {
        {"Escape", ESC}, {"Enter", FINE}, {"Return", FINE}, {"Tab", TAB},
        {"Backspace", BKSP}, {"BackSpace", BKSP}, {"Delete", DEL},
        {"Home", HOME}, {"End", END}, {"ArrowLeft", LEFT}, {"Left", LEFT},
        {"ArrowRight", RIGHT}, {"Right", RIGHT}, {"ArrowUp", UP}, {"Up", UP},
        {"ArrowDown", DOWN}, {"Down", DOWN}, {"PageUp", PGUP}, {"Prior", PGUP},
        {"PageDown", PGDN}, {"Next", PGDN}, {" ", ' '}, {"space", ' '},
    };
    size_t i;
    if (k[0] && !k[1]) return (unsigned char)k[0];
    for (i = 0; i < sizeof named / sizeof named[0]; i++)
        if (strcmp(k, named[i].name) == 0) return named[i].code;
    return BADKEY;
}

} // namespace xpp::json
