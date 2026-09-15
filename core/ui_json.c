/* The protocol front end: an XppUi table that talks line-delimited JSON.

   One JSON object per line in each direction. Everything the core would
   draw or show goes out as an event ("ev"); keys, sizes, parameter edits and
   the answers to prompts come in as commands ("cmd"). A prompt (menu,
   string box, file name, mouse pick, ...) is an "ask" event with an id; the
   core blocks until the matching {"cmd":"answer","id":N,...} arrives, the
   same way the X11 front end runs a nested event loop. See docs/protocol.md.

   Output goes to the file descriptor that was stdout when json_ui_install()
   ran; stdout itself is pointed at stderr so the core's own printing never
   corrupts the stream. */
#include "ui_json.h"
#include "xpp_win32.h"
#include "xpp_ui.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "graf_par.h"
#include "grobs.h"
#include "integrate.h"
#include "nullcline.h"
#include "browse.h"
#include "colormap.h"
#include "aniparse.h"
#include "auto_nox.h"
#include "mykeydef.h"
#include "derived.h"
#include "parserslow.h"
#include "axes2.h"
#include "volterra2.h"
#include "tabular.h"
#include "diagram.h"
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef _WIN32
#include <sys/select.h>
#endif

extern int NUPAR, NODE, NMarkov, NEQ;
extern char upar_names[MAXPAR][11], uvar_names[MAXODE][12];
extern double last_ic[MAXODE];
extern char this_file[];
extern char cur_dir[];
extern BROWSER my_browser;
extern BIFUR Auto;
extern int PointRadius, TextJustify, COLOR, colorline[];
extern unsigned int DONT_XORCross;
void commander(int ch); /* commands.c */

#define MAX_LEN_EBOX 86 /* edit_rhs.h */

/* window ids the client draws into; plot windows are graph index + 1 */
#define WIN_AUTO 101
#define WIN_AUTO_STAB 102
#define WIN_AUTO_INFO 103
#define WIN_ANI 104

static FILE *proto;
static int win_w[MAXPOP], win_h[MAXPOP];

/* ---- output ------------------------------------------------------------ */

typedef struct {
    char *s;
    size_t len, cap;
} Buf;

#define BUF_LIT(b, lit) buf_add(b, lit, sizeof(lit) - 1)

static Buf ops;          /* pending drawing ops for ops_win */
static unsigned long ops_win;
static int state_dirty;

static void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2 + 4096;
        b->s = realloc(b->s, b->cap);
    }
    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;
}

static void buf_printf(Buf *b, const char *fmt, ...)
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

static void buf_str(Buf *b, const char *s)
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

static void buf_str_array(Buf *b, char **v, int n)
{
    int i;
    BUF_LIT(b, "[");
    for (i = 0; i < n; i++) {
        if (i) BUF_LIT(b, ",");
        buf_str(b, v ? v[i] : "");
    }
    BUF_LIT(b, "]");
}

static void flush_ops(void)
{
    if (ops.len == 0) return;
    fprintf(proto, "{\"ev\":\"draw\",\"win\":%lu,\"ops\":[%s]}\n", ops_win, ops.s);
    ops.len = 0;
    ops.s[0] = 0;
}

static void send_state(void);

/* one complete event line */
static void send_buf(Buf *b)
{
    flush_ops();
    fputs(b->s, proto);
    fputc('\n', proto);
    fflush(proto);
    b->len = 0;
}

static void send_simple(const char *ev, const char *key, const char *text)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"%s\"", ev);
    if (key) {
        buf_printf(&b, ",\"%s\":", key);
        buf_str(&b, text);
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    free(b.s);
}

static void op(unsigned long win, const char *fmt, ...)
{
    char tmp[2048];
    va_list ap;
    int n;
    if (win != ops_win) {
        flush_ops();
        ops_win = win;
    }
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof tmp) n = sizeof tmp - 1;
    if (ops.len) BUF_LIT(&ops, ",");
    buf_add(&ops, tmp, n);
    if (ops.len > 60000) flush_ops();
}

static void op_text(unsigned long win, const char *name, int x, int y, const char *s, int size)
{
    Buf b = {0};
    buf_printf(&b, "[\"%s\",%d,%d,", name, x, y);
    buf_str(&b, s);
    if (size >= 0) buf_printf(&b, ",%d", size);
    BUF_LIT(&b, "]");
    op(win, "%s", b.s);
    free(b.s);
}

static void json_flush(void)
{
    flush_ops();
    if (state_dirty) send_state();
    fflush(proto);
}

/* ---- input ------------------------------------------------------------- */

static char inbuf[1 << 16];
static int inlen;

/* up to n bytes of input into buf, waiting at most wait_ms (< 0: block).
   Returns the byte count, 0 on timeout; end of input quits the program. */
#ifdef _WIN32
static int read_input(char *buf, int n, int wait_ms)
{
    int r = xpp_read_stdin(buf, n, wait_ms);
    if (r < 0) exit(0);
    return r;
}
#else
static int read_input(char *buf, int n, int wait_ms)
{
    for (;;) {
        fd_set fds;
        struct timeval tv, *tvp = NULL;
        int r;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        if (wait_ms >= 0) {
            tv.tv_sec = wait_ms / 1000;
            tv.tv_usec = (wait_ms % 1000) * 1000;
            tvp = &tv;
        }
        r = select(1, &fds, NULL, NULL, tvp);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return 0;
        r = read(0, buf, n);
        if (r <= 0) exit(0);
        return r;
    }
}
#endif

/* next complete line into line (without newline). wait_ms < 0 blocks.
   Returns 1 for a line, 0 on timeout. End of input quits the program. */
static int read_line(char *line, int max, int wait_ms)
{
    for (;;) {
        char *nl = memchr(inbuf, '\n', inlen);
        int r;
        if (nl) {
            int n = nl - inbuf;
            if (n >= max) n = max - 1;
            memcpy(line, inbuf, n);
            line[n] = 0;
            if (n > 0 && line[n - 1] == '\r') line[n - 1] = 0;
            memmove(inbuf, nl + 1, inlen - (nl + 1 - inbuf));
            inlen -= nl + 1 - inbuf;
            return 1;
        }
        if (inlen >= (int)sizeof inbuf - 1) inlen = 0; /* overlong line: drop */
        r = read_input(inbuf + inlen, sizeof inbuf - 1 - inlen, wait_ms);
        if (r == 0) return 0;
        inlen += r;
    }
}

/* ---- a small JSON reader for flat command objects ------------------------ */

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static const char *skip_value(const char *p)
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
static const char *js_find(const char *obj, const char *key)
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

static int js_string(const char *v, char *out, int max)
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

static double js_num(const char *v, double def)
{
    if (!v) return def;
    if (*v == '"') return def;
    if (strncmp(v, "true", 4) == 0) return 1;
    if (strncmp(v, "false", 5) == 0 || strncmp(v, "null", 4) == 0) return 0;
    return atof(v);
}

static const char *js_elem(const char *arr, int i)
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

static int get_str(const char *obj, const char *key, char *out, int max)
{
    return js_string(js_find(obj, key), out, max);
}

static double get_num(const char *obj, const char *key, double def)
{
    return js_num(js_find(obj, key), def);
}

static int is_cmd(const char *line, const char *name)
{
    char c[32];
    return get_str(line, "cmd", c, sizeof c) && strcmp(c, name) == 0;
}

/* key names as the client sends them (DOM KeyboardEvent.key or X keysym
   names) to the codes get_key_press() gives the core */
static int key_code(const char *k)
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

static void apply_size(const char *line);
static void apply_set(const char *line);

/* commands that make sense at any moment, even while a prompt is open */
static int handle_async(const char *line)
{
    if (is_cmd(line, "size")) {
        apply_size(line);
        return 1;
    }
    if (is_cmd(line, "quit")) exit(0);
    if (is_cmd(line, "state")) {
        send_state();
        return 1;
    }
    return 0;
}

/* ---- prompts ------------------------------------------------------------- */

static int ask_id;
static char answer[1 << 16];

/* b holds {"ev":"ask","id":N,"kind":... without the closing brace; send
   it and wait for the answer, which is left in answer[]. Returns 1 when the
   answer says ok (or has no ok member), 0 when cancelled. */
static int ask_wait(Buf *b, int id)
{
    BUF_LIT(b, "}");
    json_flush();
    send_buf(b);
    free(b->s);
    for (;;) {
        read_line(answer, sizeof answer, -1);
        if (handle_async(answer)) {
            flush_ops();
            fflush(proto);
            continue;
        }
        if (is_cmd(answer, "answer") && (int)get_num(answer, "id", -1) == id) {
            const char *ok = js_find(answer, "ok");
            return ok == NULL || js_num(ok, 0) != 0;
        }
        /* anything else (keys typed at the plot while a dialog is up) is
           dropped, as the X11 dialogs do */
    }
}

static int ask_begin(Buf *b, const char *kind)
{
    b->s = NULL;
    b->len = b->cap = 0;
    ask_id++;
    buf_printf(b, "{\"ev\":\"ask\",\"id\":%d,\"kind\":\"%s\"", ask_id, kind);
    return ask_id;
}

static void j_err_msg(char *msg)
{
    send_simple("message", "error", msg);
}

static void j_ping(void) { send_simple("ping", NULL, NULL); }

static void j_bottom_msg(int line, char *msg)
{
    (void)line;
    send_simple("message", "bottom", msg);
}

static void j_message_box(char *msg) { send_simple("message", "box", msg); }
static void j_kill_message_box(void) { send_simple("message", "box", ""); }
static void j_title_text(char *s) { send_simple("title", "text", s); }
static void j_canvas_xy(char *s) { send_simple("message", "xy", s); }

static int j_dialog(char *title, char *name, char *value, char *ok, char *cancel, int max)
{
    Buf b;
    int id = ask_begin(&b, "string");
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"name\":");
    buf_str(&b, name);
    BUF_LIT(&b, ",\"value\":");
    buf_str(&b, value);
    BUF_LIT(&b, ",\"ok\":");
    buf_str(&b, ok);
    BUF_LIT(&b, ",\"cancel\":");
    buf_str(&b, cancel);
    buf_printf(&b, ",\"max\":%d", max);
    if (!ask_wait(&b, id)) return 0;
    get_str(answer, "value", value, max + 1);
    return 1;
}

static int j_new_string(char *name, char *value)
{
    /* the X11 prompt edits a 256-byte line in place */
    return j_dialog("", name, value, "Ok", "Cancel", 255);
}

static int j_yes_no_box(void)
{
    Buf b;
    int id = ask_begin(&b, "choice");
    BUF_LIT(&b, ",\"question\":\"Are you sure?\",\"choices\":[\"Yes\",\"No\"],\"keys\":\"yn\"");
    if (!ask_wait(&b, id)) return 0;
    {
        char k[8];
        get_str(answer, "key", k, sizeof k);
        return k[0] == 'y';
    }
}

static int j_two_choice(char *c1, char *c2, char *q, char *key, char *title)
{
    Buf b;
    char k[8];
    int id = ask_begin(&b, "choice");
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title ? title : "");
    BUF_LIT(&b, ",\"question\":");
    buf_str(&b, q);
    BUF_LIT(&b, ",\"choices\":[");
    buf_str(&b, c1);
    BUF_LIT(&b, ",");
    buf_str(&b, c2);
    BUF_LIT(&b, "],\"keys\":");
    buf_str(&b, key);
    if (!ask_wait(&b, id)) return 0;
    get_str(answer, "key", k, sizeof k);
    return (unsigned char)k[0];
}

static void j_respond_box(char *button, char *message)
{
    Buf b;
    int id = ask_begin(&b, "alert");
    BUF_LIT(&b, ",\"button\":");
    buf_str(&b, button);
    BUF_LIT(&b, ",\"message\":");
    buf_str(&b, message);
    ask_wait(&b, id);
}

static int j_checklist(char *title, char **names, int *flags, int n)
{
    Buf b;
    int i, id = ask_begin(&b, "checklist");
    const char *arr;
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"names\":");
    buf_str_array(&b, names, n);
    BUF_LIT(&b, ",\"flags\":[");
    for (i = 0; i < n; i++) buf_printf(&b, i ? ",%d" : "%d", flags[i]);
    BUF_LIT(&b, "]");
    if (!ask_wait(&b, id)) return 0;
    arr = js_find(answer, "flags");
    for (i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) flags[i] = js_num(e, flags[i]) != 0;
    }
    return 1;
}

/* string_box and edit_box: a form of named fields */
static int form(char *title, char **names, int n, char **values, int size)
{
    Buf b;
    int i, id = ask_begin(&b, "form");
    const char *arr;
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"names\":");
    buf_str_array(&b, names, n);
    BUF_LIT(&b, ",\"values\":");
    buf_str_array(&b, values, n);
    buf_printf(&b, ",\"max\":%d", size - 1);
    if (!ask_wait(&b, id)) return 0;
    arr = js_find(answer, "values");
    for (i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) js_string(e, values[i], size);
    }
    return 1;
}

static int j_string_box(int n, int row, int col, char *title, char **names,
                        char values[][25], int maxchar)
{
    char *v[64];
    int i;
    (void)row; (void)col; (void)maxchar;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) v[i] = values[i];
    return form(title, names, n, v, 25);
}

static int j_edit_box(int n, char *title, char **names, char **values)
{
    return form(title, names, n, values, MAX_LEN_EBOX);
}

static int j_file_selector(char *title, char *file, char *wild)
{
    Buf b;
    int id = ask_begin(&b, "file");
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"file\":");
    buf_str(&b, file);
    BUF_LIT(&b, ",\"wild\":");
    buf_str(&b, wild);
    BUF_LIT(&b, ",\"dir\":");
    buf_str(&b, cur_dir);
    if (!ask_wait(&b, id)) return 0;
    get_str(answer, "file", file, 256);
    return file[0] != 0;
}

static int mouse_ask(unsigned long win, const char *kind, int flag, int *v, int nv)
{
    Buf b;
    int i, id = ask_begin(&b, kind);
    static const char *names[] = {"x", "y", "x2", "y2"};
    buf_printf(&b, ",\"win\":%lu,\"flag\":%d", win, flag);
    if (!ask_wait(&b, id)) return 0;
    for (i = 0; i < nv; i++) v[i] = (int)get_num(answer, names[i], 0);
    return 1;
}

static int j_get_mouse_xy(int *x, int *y)
{
    int v[2];
    if (!mouse_ask(draw_win, "mouse", 0, v, 2)) return 0;
    *x = v[0];
    *y = v[1];
    return 1;
}

static int j_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    int v[4];
    if (!mouse_ask(draw_win, "rubber", flag, v, 4)) return 0;
    *i1 = v[0]; *j1 = v[1]; *i2 = v[2]; *j2 = v[3];
    return 1;
}

static int j_menu_choose(const struct XppMenu *m, int def)
{
    Buf b;
    char k[8];
    int id = ask_begin(&b, "menu");
    BUF_LIT(&b, ",\"name\":");
    buf_str(&b, m->name);
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, m->title);
    BUF_LIT(&b, ",\"items\":");
    buf_str_array(&b, m->items, m->n);
    BUF_LIT(&b, ",\"keys\":");
    buf_str(&b, m->keys);
    if (m->hints) {
        BUF_LIT(&b, ",\"hints\":");
        buf_str_array(&b, m->hints, m->n);
    }
    buf_printf(&b, ",\"def\":%d", def);
    if (!ask_wait(&b, id)) return 27;
    get_str(answer, "key", k, sizeof k);
    return k[0] ? (unsigned char)k[0] : 27;
}

static void j_show_menu(int which)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"menu\",\"which\":%d}", which);
    send_buf(&b);
    free(b.s);
}

/* ---- long loops ------------------------------------------------------------ */

static int j_check_abort(void)
{
    char line[4096];
    static struct timeval last;
    struct timeval now;
    /* let the client see the picture grow, a few frames a second */
    gettimeofday(&now, NULL);
    if ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec) > 50000) {
        last = now;
        flush_ops();
        fflush(proto);
    }
    while (read_line(line, sizeof line, 0)) {
        if (handle_async(line)) continue;
        if (is_cmd(line, "key")) {
            char k[32];
            get_str(line, "key", k, sizeof k);
            return key_code(k);
        }
        if (is_cmd(line, "abort")) return ESC;
        if (is_cmd(line, "set")) apply_set(line);
    }
    return 64;
}

static int j_progress_begin(void) { return 100; }

static void j_progress(int nit, int icount, int cwidth)
{
    static struct timeval last;
    struct timeval now;
    Buf b = {0};
    (void)cwidth;
    gettimeofday(&now, NULL);
    if ((now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec) < 100000) return;
    last = now;
    buf_printf(&b, "{\"ev\":\"progress\",\"n\":%d,\"of\":%d}", icount, nit);
    send_buf(&b);
    free(b.s);
}

/* ---- state ------------------------------------------------------------------ */

static void send_state(void)
{
    Buf b = {0};
    int i;
    double z;
    state_dirty = 0;
    evaluate_derived();
    BUF_LIT(&b, "{\"ev\":\"state\",\"pars\":[");
    for (i = 0; i < NUPAR; i++) {
        get_val(upar_names[i], &z);
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, upar_names[i]);
        buf_printf(&b, ",%.16g]", z);
    }
    BUF_LIT(&b, "],\"ics\":[");
    for (i = 0; i < NODE + NMarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, uvar_names[i]);
        buf_printf(&b, ",%.16g]", last_ic[i]);
    }
    buf_printf(&b, "],\"rows\":%d,\"menu\":%d,\"win\":%lu}", my_browser.maxrow, help_menu,
               (unsigned long)draw_win);
    send_buf(&b);
    free(b.s);
}

static void j_state_dirty(void) { state_dirty = 1; }
static void j_state_dirty_i(int i) { (void)i; state_dirty = 1; }
static void j_state_dirty_is(int i, char *s) { (void)i; (void)s; state_dirty = 1; }

/* ---- plot windows ------------------------------------------------------------ */

static int graph_of(unsigned long w)
{
    int i;
    for (i = 0; i < MAXPOP; i++)
        if (graph[i].Use && graph[i].w == w) return i;
    return 0;
}

static void j_get_draw_size(unsigned int *w, unsigned int *h)
{
    int i = graph_of(draw_win);
    *w = win_w[i];
    *h = win_h[i];
}

static void j_blank_draw_window(void) { op(draw_win, "[\"clear\"]"); }

static void j_redraw_all(void)
{
    redraw_dfield();
    restore(0, my_browser.maxrow);
    draw_label(draw_win);
    draw_freeze(draw_win);
    restore_on();
}

static void j_redraw_graph(void)
{
    j_blank_draw_window();
    set_normal_scale();
    do_axes();
    restore(0, my_browser.maxrow);
    draw_label(draw_win);
    draw_freeze(draw_win);
    redraw_dfield();
    if (MyGraph->Nullrestore) restore_nullclines();
}

static void j_redraw_screens(void)
{
    int i, ic = current_pop;
    if (SimulPlotFlag == 0) {
        j_redraw_all();
        return;
    }
    for (i = 0; i < num_pops; i++) {
        make_active(ActiveWinList[i], 1);
        j_redraw_all();
    }
    make_active(ic, 1);
}

static void j_clear_screens(void)
{
    int i, ic = current_pop;
    if (SimulPlotFlag == 0) {
        clr_scrn();
        return;
    }
    for (i = 0; i < num_pops; i++) {
        make_active(ActiveWinList[i], 1);
        clr_scrn();
    }
    make_active(ic, 1);
}

static void j_reset_graphics(void)
{
    j_blank_draw_window();
    do_axes();
}

static void send_window(const char *what, unsigned long id, int w, int h, const char *title)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"window\",\"op\":\"%s\",\"win\":%lu,\"w\":%d,\"h\":%d,\"title\":",
               what, id, w, h);
    buf_str(&b, title ? title : "");
    BUF_LIT(&b, "}");
    send_buf(&b);
    free(b.s);
}

static void select_graph(int i)
{
    current_pop = i;
    MyGraph = &graph[i];
    draw_win = graph[i].w;
    get_draw_area();
    send_window("select", draw_win, win_w[i], win_h[i], NULL);
}

static void j_activate_graph(int i, int flag)
{
    draw_win = graph[i].w;
    get_draw_area_flag(flag);
}

static void j_create_plot_window(void)
{
    int i;
    for (i = 1; i < MAXPOP; i++)
        if (graph[i].Use == 0) break;
    if (i >= MAXPOP) {
        j_respond_box("Okay", "Too many windows!");
        return;
    }
    copy_graph(i, current_pop);
    graph[i].w = i + 1;
    win_w[i] = graph[i].Width = 450;
    win_h[i] = graph[i].Height = 350;
    graph[i].x0 = 0;
    graph[i].y0 = 0;
    num_pops++;
    send_window("create", graph[i].w, win_w[i], win_h[i], "");
    select_graph(i);
}

static void destroy_graph(int i)
{
    graph[i].Use = 0;
    destroy_label(graph[i].w);
    destroy_grob(graph[i].w);
    send_window("destroy", graph[i].w, 0, 0, NULL);
    num_pops--;
}

static void j_destroy_plot_window(void)
{
    int i;
    if (draw_win == graph[0].w) {
        j_respond_box("Okay", "Can't destroy big window!");
        return;
    }
    i = graph_of(draw_win);
    if (i == 0) return;
    select_graph(0);
    destroy_graph(i);
}

static void j_kill_plot_windows(void)
{
    int i;
    select_graph(0);
    for (i = 1; i < MAXPOP; i++)
        if (graph[i].Use) destroy_graph(i);
    num_pops = 1;
}

static void j_cput_text(void)
{
    char string[256], new[256];
    int x, y, size = 2;
    strcpy(string, "");
    if (new_string("Text: ", string) == 0) return;
    if (string[0] == '%') {
        fillintext(&string[1], new);
        strcpy(string, new);
    }
    new_int("Size 0-4 :", &size);
    if (size > 4) size = 4;
    if (size < 0) size = 0;
    j_message_box("Place text with mouse");
    if (j_get_mouse_xy(&x, &y)) {
        fillintext(string, new);
        op_text(draw_win, "stext", x, y, new, size);
        add_label(string, x, y, size, 0);
    }
    j_kill_message_box();
}

static void j_draw_freeze(void) { draw_freeze(draw_win); }
static void j_draw_text(int x, int y, char *s);
static void j_put_text(int x, int y, char *s) { j_draw_text(x, y, s); }
static int j_film_clip(void) { return 1; }

static void j_unsupported(const char *what)
{
    char msg[128];
    snprintf(msg, sizeof msg, "%s is not available in this front end yet", what);
    j_err_msg(msg);
}

static void j_movie_play_back(void) { j_unsupported("Kinescope playback"); }
static void j_movie_auto_play(void) { j_unsupported("Kinescope playback"); }
static void j_movie_save(char *basename, int fmat) { (void)basename; (void)fmat; j_unsupported("Kinescope save"); }
static void j_movie_make_anigif(void) { j_unsupported("Animated GIF"); }
static void j_scroll_window(void) { j_unsupported("Scroll"); }
static void j_auto_scroll_window(void) { j_unsupported("Scroll"); }

static void send_palette(void)
{
    Buf b = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"palette\",\"colors\":[");
    for (i = 0; i < XPP_MAX_COLORS; i++)
        buf_printf(&b, i ? ",\"#%02x%02x%02x\"" : "\"#%02x%02x%02x\"", xpp_cmap_rgb[i][0] >> 8,
                   xpp_cmap_rgb[i][1] >> 8, xpp_cmap_rgb[i][2] >> 8);
    BUF_LIT(&b, "]}");
    send_buf(&b);
    free(b.s);
}

static void j_new_colormap(int type)
{
    custom_color = type;
    xpp_build_colormap();
    send_palette();
}

static void j_draw_point(int x, int y) { op(draw_win, "[\"point\",%d,%d,%d]", x, y, PointRadius); }
static void j_draw_line(int x1, int y1, int x2, int y2) { op(draw_win, "[\"line\",%d,%d,%d,%d]", x1, y1, x2, y2); }
static void j_draw_bead(int x, int y) { op(draw_win, "[\"bead\",%d,%d]", x, y); }
static void j_draw_frect(int x, int y, int w, int h) { op(draw_win, "[\"frect\",%d,%d,%d,%d]", x, y, w, h); }
/* put_text_x11: justified, baseline a third of a cell below y, foreground */
static void j_draw_text(int x, int y, char *s)
{
    int sw = strlen(s) * DCURXs;
    switch (TextJustify) {
    case 0: sw = 0; break;
    case 1: sw = -sw / 2; break;
    case 2: sw = -sw; break;
    }
    op_text(draw_win, "text", x + sw, y + DCURYs / 3, s, -1);
}
static void j_draw_special_text(int x, int y, char *s, int size) { op_text(draw_win, "stext", x, y, s, size); }
/* set_line_style_x11: -2 border, -1 dashed axis, else a curve colour */
static void j_draw_linestyle(int ls)
{
    if (ls == -2) {
        op(draw_win, "[\"color\",0],[\"lw\",2],[\"dash\",0]");
        return;
    }
    if (ls == -1) {
        op(draw_win, "[\"color\",0],[\"lw\",1],[\"dash\",1]");
        return;
    }
    if (!COLOR) {
        ls = (ls % 8) + 2;
        op(draw_win, "[\"color\",0],[\"lw\",1],[\"dash\",%d]", ls == 2 ? 0 : ls);
        return;
    }
    op(draw_win, "[\"lw\",1],[\"dash\",0],[\"color\",%d]", colorline[ls % 11]);
}
static void j_set_color(int col) { op(draw_win, "[\"color\",%d]", col); }

static void j_aplot_make(char *name) { (void)name; j_unsupported("Array plot"); }
static void j_aplot_draw_one(char *tag) { (void)tag; }

/* ---- AUTO window --------------------------------------------------------------- */

/* a size the client asked for while a command ran, applied when it ends */
static int auto_size_w, auto_size_h;

static void j_auto_make_window(char *wname, char *iname)
{
    (void)iname;
    Auto.hgt = auto_size_h ? auto_size_h - 4 * DCURYs : 20 * DCURY;
    Auto.wid = auto_size_w ? auto_size_w - 12 * DCURXs : 67 * DCURX;
    auto_size_w = auto_size_h = 0;
    Auto.x0 = 10 * DCURXs;
    Auto.y0 = 2 * DCURYs;
    Auto.st_wid = 12 * DCURX;
    strcpy(Auto.hinttxt, "hint");
    send_window("create", WIN_AUTO, Auto.wid + 12 * DCURXs, Auto.hgt + 4 * DCURYs, wname);
    draw_bif_axes();
}

static void j_auto_line(int a, int b, int c, int d) { op(WIN_AUTO, "[\"line\",%d,%d,%d,%d]", a, b, c, d); }
static void j_auto_text(int a, int b, char *c) { op_text(WIN_AUTO, "rtext", a, b, c, -1); }
static void j_auto_circle(int x, int y, int r) { op(WIN_AUTO, "[\"circle\",%d,%d,%d]", x, y, r); }
static void j_auto_fill_circle(int x, int y, int r) { op(WIN_AUTO, "[\"fcircle\",%d,%d,%d]", x, y, r); }
static void j_auto_xor_cross(int x, int y)
{
    if (DONT_XORCross) return;
    op(WIN_AUTO, "[\"cross\",%d,%d]", x, y);
}
static void j_auto_line_width(int wid) { op(WIN_AUTO, "[\"lw\",%d]", wid); }
static void j_auto_col(int col) { op(WIN_AUTO, "[\"color\",%d]", col); }
static void j_auto_bw(void) { op(WIN_AUTO, "[\"color\",0]"); }
static void j_auto_clr_stab(void)
{
    int r = Auto.st_wid / 4;
    op(WIN_AUTO_STAB, "[\"clear\"]");
    op(WIN_AUTO_STAB, "[\"circle\",%d,%d,%d]", 2 * r, 2 * r, r);
}
static void j_auto_stab_line(int x, int y, int xp, int yp) { op(WIN_AUTO_STAB, "[\"line\",%d,%d,%d,%d]", x, y, xp, yp); }
static void j_auto_clear_plot(void) { op(WIN_AUTO, "[\"clear\"]"); }
static void j_auto_clear_info(void) { op(WIN_AUTO_INFO, "[\"clear\"]"); }
static void j_auto_draw_info(char *s, int x, int y) { op_text(WIN_AUTO_INFO, "rtext", x, y, s, -1); }
static int j_auto_check_abort(int *iflag)
{
    *iflag = 0;
    if (j_check_abort() == ESC) {
        *iflag = 1;
        return 0;
    }
    return 0;
}
static int j_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag)
{
    int v[4];
    if (!mouse_ask(WIN_AUTO, "rubber", flag, v, 4)) return 0;
    *i1 = v[0]; *j1 = v[1]; *i2 = v[2]; *j2 = v[3];
    return 1;
}
static int j_auto_choose_key(char *title, char **list, char *key, int n, int max, int def,
                             int x, int y, char **hints, char *httxt)
{
    XppMenu m;
    (void)max; (void)x; (void)y; (void)httxt;
    m.name = "auto";
    m.title = title;
    m.n = n;
    m.items = list;
    m.keys = key;
    m.hints = hints;
    m.first_cmd = -1;
    m.width = 0;
    m.row = 0;
    return j_menu_choose(&m, def);
}
static int j_auto_grab_event(int *x, int *y)
{
    Buf b;
    char k[32];
    int id = ask_begin(&b, "grab");
    buf_printf(&b, ",\"win\":%d", WIN_AUTO);
    if (!ask_wait(&b, id)) return ESC;
    if (get_str(answer, "key", k, sizeof k)) return key_code(k);
    *x = (int)get_num(answer, "x", 0);
    *y = (int)get_num(answer, "y", 0);
    return XPP_AUTO_CLICK;
}
static void j_auto_show_hint(void) { send_simple("message", "auto", Auto.hinttxt); }

/* ---- animation window ------------------------------------------------------------ */

static void j_new_vcr(void)
{
    if (vcr.iexist == 1) return;
    vcr.wid = 280;
    vcr.hgt = 350;
    vcr.iexist = 1;
    send_window("create", WIN_ANI, vcr.wid, vcr.hgt, "Animation");
    ani_view_created();
}
static void j_ani_clear(void) { op(WIN_ANI, "[\"clear\"]"); }
static void j_ani_show(void) { flush_ops(); fflush(proto); }
static void j_ani_color(int icol) { op(WIN_ANI, "[\"color\",%d]", icol); }
static void j_ani_thick(int t) { op(WIN_ANI, "[\"lw\",%d]", t); }
static void j_ani_font(int size, int font, int color) { op(WIN_ANI, "[\"font\",%d,%d,%d]", size, font, color); }
static void j_ani_line(int x1, int y1, int x2, int y2) { op(WIN_ANI, "[\"line\",%d,%d,%d,%d]", x1, y1, x2, y2); }
static void j_ani_rect(int x, int y, int w, int h, int fill) { op(WIN_ANI, "[\"%s\",%d,%d,%d,%d]", fill ? "frect" : "rect", x, y, w, h); }
static void j_ani_arc(int x, int y, int w, int h, int fill) { op(WIN_ANI, "[\"%s\",%d,%d,%d,%d]", fill ? "fellipse" : "ellipse", x, y, w, h); }
static void j_ani_text(int x, int y, char *s) { op_text(WIN_ANI, "rtext", x, y, s, -1); }

/* ---- misc ------------------------------------------------------------------------ */

static void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n)
{
    Buf b = {0};
    int i;
    (void)ev;
    redraw_ics();
    buf_printf(&b, "{\"ev\":\"equilibrium\",\"type\":\"%s\",\"cplus\":%d,\"cminus\":%d,"
               "\"im\":%d,\"rplus\":%d,\"rminus\":%d,\"values\":[",
               eq_stability(cp, rp, im), cp, cm, im, rp, rm);
    for (i = 0; i < n; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, uvar_names[i]);
        buf_printf(&b, ",%.16g]", y[i]);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    free(b.s);
}

static void j_make_txtview(void)
{
    extern char *save_eqn[];
    extern int NLINES;
    Buf b = {0};
    BUF_LIT(&b, "{\"ev\":\"source\",\"lines\":");
    buf_str_array(&b, save_eqn, NLINES);
    BUF_LIT(&b, "}");
    send_buf(&b);
    free(b.s);
}

static void j_q_calc(void)
{
    char expr[256] = "";
    double z;
    char result[300];
    while (new_string("Formula:", expr)) {
        if (do_calc(expr, &z) != -1) {
            snprintf(result, sizeof result, "%s = %.16g", expr, z);
            send_simple("message", "calc", result);
        }
    }
}

static void j_exit_program(void)
{
    send_simple("bye", NULL, NULL);
    exit(0);
}

static void j_void(void) {}
static void j_int(int i) { (void)i; }

static const XppUi json_ui = {
    .err_msg = j_err_msg,
    .ping = j_ping,
    .bottom_msg = j_bottom_msg,
    .message_box = j_message_box,
    .kill_message_box = j_kill_message_box,
    .title_text = j_title_text,
    .canvas_xy = j_canvas_xy,
    .new_string = j_new_string,
    .yes_no_box = j_yes_no_box,
    .two_choice = j_two_choice,
    .respond_box = j_respond_box,
    .checklist = j_checklist,
    .string_box = j_string_box,
    .file_selector = j_file_selector,
    .dialog = j_dialog,
    .edit_box = j_edit_box,
    .get_mouse_xy = j_get_mouse_xy,
    .menu_flash = j_int,
    .show_menu = j_show_menu,
    .redraw_menu = j_void,
    .menu_choose = j_menu_choose,
    .check_abort = j_check_abort,
    .progress_begin = j_progress_begin,
    .progress = j_progress,
    .flush = json_flush,
    .redraw_params = j_state_dirty,
    .param_box_set = j_state_dirty_is,
    .param_box_redraw = j_state_dirty_i,
    .ic_box_set = j_state_dirty_is,
    .ic_box_redraw = j_state_dirty_i,
    .redraw_ics = j_state_dirty,
    .redraw_all = j_redraw_all,
    .redraw_bcs = j_state_dirty,
    .redraw_delays = j_state_dirty,
    .redraw_graph = j_redraw_graph,
    .redraw_screens = j_redraw_screens,
    .clear_screens = j_clear_screens,
    .clear_draw_window = clr_scrn,
    .reset_graphics = j_reset_graphics,
    .data_changed = j_state_dirty_i,
    .browser_redraw = j_state_dirty_i,
    .activate_graph = j_activate_graph,
    .create_plot_window = j_create_plot_window,
    .destroy_plot_window = j_destroy_plot_window,
    .kill_plot_windows = j_kill_plot_windows,
    .lower_plot_window = j_void,
    .gr_col = j_void,
    .base_col = j_void,
    .cput_text = j_cput_text,
    .get_draw_size = j_get_draw_size,
    .draw_freeze = j_draw_freeze,
    .blank_draw_window = j_blank_draw_window,
    .put_text = j_put_text,
    .small_base = j_void,
    .small_gr = j_void,
    .film_clip = j_film_clip,
    .reset_film = j_void,
    .movie_play_back = j_movie_play_back,
    .movie_auto_play = j_movie_auto_play,
    .movie_save = j_movie_save,
    .movie_make_anigif = j_movie_make_anigif,
    .rubber_band = j_rubber_band,
    .scroll_window = j_scroll_window,
    .new_colormap = j_new_colormap,
    .draw_point = j_draw_point,
    .draw_line = j_draw_line,
    .draw_bead = j_draw_bead,
    .draw_frect = j_draw_frect,
    .draw_text = j_draw_text,
    .draw_special_text = j_draw_special_text,
    .draw_linestyle = j_draw_linestyle,
    .set_color = j_set_color,
    .aplot_make = j_aplot_make,
    .aplot_redraw = j_void,
    .aplot_reset_axes = j_void,
    .aplot_draw_one = j_aplot_draw_one,
    .auto_make_window = j_auto_make_window,
    .auto_line = j_auto_line,
    .auto_text = j_auto_text,
    .auto_circle = j_auto_circle,
    .auto_fill_circle = j_auto_fill_circle,
    .auto_xor_cross = j_auto_xor_cross,
    .auto_line_width = j_auto_line_width,
    .auto_col = j_auto_col,
    .auto_bw = j_auto_bw,
    .auto_clr_stab = j_auto_clr_stab,
    .auto_stab_line = j_auto_stab_line,
    .auto_clear_plot = j_auto_clear_plot,
    .auto_redraw_menus = j_void,
    .auto_clear_info = j_auto_clear_info,
    .auto_draw_info = j_auto_draw_info,
    .auto_refresh = json_flush,
    .auto_check_abort = j_auto_check_abort,
    .auto_rubber = j_auto_rubber,
    .auto_choose_key = j_auto_choose_key,
    .auto_scroll_window = j_auto_scroll_window,
    .auto_grab_event = j_auto_grab_event,
    .auto_show_hint = j_auto_show_hint,
    .new_vcr = j_new_vcr,
    .ani_clear = j_ani_clear,
    .ani_show = j_ani_show,
    .ani_color = j_ani_color,
    .ani_thick = j_ani_thick,
    .ani_font = j_ani_font,
    .ani_line = j_ani_line,
    .ani_rect = j_ani_rect,
    .ani_arc = j_ani_arc,
    .ani_text = j_ani_text,
    .ani_slider = j_void,
    .init_txtview = j_void,
    .show_eq_box = j_show_eq_box,
    .make_txtview = j_make_txtview,
    .q_calc = j_q_calc,
    .exit_program = j_exit_program,
};

/* ---- commands from the client ------------------------------------------------------ */

/* The AUTO diagram follows the size of the client's window. Resizing
   redraws the diagram, so it waits until no command (a run, a grab) is
   using it. */
static void apply_auto_size(void)
{
    int w = auto_size_w, h = auto_size_h;
    if (!w) return;
    if (!Auto.exist) return; /* j_auto_make_window takes it */
    auto_size_w = auto_size_h = 0;
    if (w - 12 * DCURXs == Auto.wid && h - 4 * DCURYs == Auto.hgt) return;
    Auto.wid = w - 12 * DCURXs;
    Auto.hgt = h - 4 * DCURYs;
    send_window("create", WIN_AUTO, w, h, "It's AUTO man!");
    redraw_diagram();
}

static void apply_size(const char *line)
{
    int win = (int)get_num(line, "win", 1);
    int w = (int)get_num(line, "w", 640), h = (int)get_num(line, "h", 480);
    int i = win - 1;
    if (win == WIN_AUTO) {
        /* room for the axis labels around the diagram */
        auto_size_w = w < 20 * DCURXs + 12 * DCURXs ? 32 * DCURXs : w;
        auto_size_h = h < 8 * DCURYs + 4 * DCURYs ? 12 * DCURYs : h;
        return;
    }
    if (i < 0 || i >= MAXPOP || w < 50 || h < 50) return;
    win_w[i] = w;
    win_h[i] = h;
    graph[i].Width = w;
    graph[i].Height = h;
    if (graph[i].Use && (unsigned long)graph[i].w == (unsigned long)draw_win) {
        get_draw_area();
        j_redraw_graph();
    }
}

static void apply_set(const char *line)
{
    char kind[16], name[64];
    double v = get_num(line, "value", 0);
    int i;
    get_str(line, "kind", kind, sizeof kind);
    get_str(line, "name", name, sizeof name);
    if (strcmp(kind, "par") == 0) {
        set_val(name, v);
        re_evaluate_kernels();
        redo_all_fun_tables();
    } else if (strcmp(kind, "ic") == 0) {
        for (i = 0; i < NODE + NMarkov; i++)
            if (strcasecmp(uvar_names[i], name) == 0) last_ic[i] = v;
    }
    state_dirty = 1;
}

void json_ui_handle(const char *line)
{
    char k[32];
    if (handle_async(line)) {
    } else if (is_cmd(line, "key")) {
        get_str(line, "key", k, sizeof k);
        commander(key_code(k));
    } else if (is_cmd(line, "set")) {
        apply_set(line);
    } else if (is_cmd(line, "click")) {
        int win = (int)get_num(line, "win", 1) - 1;
        if (win >= 0 && win < MAXPOP && graph[win].Use && current_pop != win) select_graph(win);
    } else if (is_cmd(line, "redraw")) {
        j_redraw_graph();
        if (Auto.exist) redraw_diagram(); /* a reconnected client has a blank one */
    } else if (is_cmd(line, "ani")) {
        char o[16];
        get_str(line, "op", o, sizeof o);
        if (strcmp(o, "step") == 0) ani_flip1((int)get_num(line, "n", 1));
        else if (strcmp(o, "reset") == 0) ani_reset();
        else if (strcmp(o, "file") == 0) get_ani_file(NULL);
        else if (strcmp(o, "close") == 0) vcr.iexist = 0;
    } else if (is_cmd(line, "auto")) {
        char o[16];
        get_str(line, "op", o, sizeof o);
        if (strcmp(o, "param") == 0) auto_params();
        else if (strcmp(o, "axes") == 0) auto_plot_par();
        else if (strcmp(o, "numerics") == 0) auto_num_par();
        else if (strcmp(o, "run") == 0) auto_run();
        else if (strcmp(o, "grab") == 0) auto_grab();
        else if (strcmp(o, "usr") == 0) auto_per_par();
        else if (strcmp(o, "clear") == 0) draw_bif_axes();
        else if (strcmp(o, "redraw") == 0) redraw_diagram();
        else if (strcmp(o, "file") == 0) auto_file();
    }
    apply_auto_size();
    json_flush();
    /* the command is finished; the client may send the next one */
    send_state();
    send_simple("idle", NULL, NULL);
}

void json_ui_loop(void)
{
    char line[1 << 16];
    for (;;) {
        read_line(line, sizeof line, -1);
        json_ui_handle(line);
    }
}

void json_ui_install(void)
{
    int fd = dup(1);
    int i;
#ifdef _WIN32
    xpp_binary_mode(fd); /* "\n" line ends, not "\r\n" */
    xpp_binary_mode(0);
#endif
    proto = fdopen(fd, "w");
    dup2(2, 1);
    for (i = 0; i < MAXPOP; i++) {
        win_w[i] = 640;
        win_h[i] = 480;
    }
    xpp_set_ui(&json_ui);
}

/* the first events a client sees */
void json_ui_hello(char *title)
{
    Buf b = {0};
    BUF_LIT(&b, "{\"ev\":\"hello\",\"protocol\":1,\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"file\":");
    buf_str(&b, this_file);
    buf_printf(&b, ",\"char\":{\"w\":%d,\"h\":%d,\"bw\":%d,\"bh\":%d}", DCURXs, DCURYs, DCURXb, DCURYb);
    BUF_LIT(&b, ",\"menus\":{\"main\":");
    buf_str_array(&b, main_menu + 1, MAIN_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"main_keys\":");
    buf_str(&b, main_menu_keys);
    BUF_LIT(&b, ",\"main_hints\":");
    buf_str_array(&b, main_hint, MAIN_ENTRIES);
    BUF_LIT(&b, ",\"file\":");
    buf_str_array(&b, fileon_menu + 1, FILE_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"file_keys\":");
    buf_str(&b, file_menu_keys);
    BUF_LIT(&b, ",\"file_hints\":");
    buf_str_array(&b, file_hint, FILE_ENTRIES);
    BUF_LIT(&b, ",\"num\":");
    buf_str_array(&b, num_menu + 1, NUM_ENTRIES); /* [0] is the title */
    BUF_LIT(&b, ",\"num_keys\":");
    buf_str(&b, num_menu_keys);
    BUF_LIT(&b, ",\"num_hints\":");
    buf_str_array(&b, num_hint, NUM_ENTRIES);
    BUF_LIT(&b, "}}");
    send_buf(&b);
    free(b.s);
    send_palette();
    send_window("create", 1, win_w[0], win_h[0], title);
    send_state();
}
