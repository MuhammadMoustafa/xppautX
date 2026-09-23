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
#include "xpp_mem.h"
#include "xpp_log.h"
#include "xpp_http.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
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
#include "userbut.h"
#include "menudrive.h"
#include "txtread.h"
#include "shoot.h"
#include "load_eqn.h"
#include "scrngif.h"
#include "my_rhs.h"
#include "arrayplot.h"
#include "read_dir.h"
#include "xpp_session.h"
#include "plot_data.h"
#include "phase_data.h"
#include "marks_data.h"
#include "series_enc.h"
#include "ani_data.h"
#include "auto_data.h"
#include "xpp_files.h"
#include <strings.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>

/* a name a client sends: one byte more than any name, so a longer one is
   cut to something no name equals */
#define NAME_IN (XPP_NAME_MAX + 2)

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern int NUPAR, NODE, NMarkov, NEQ;
extern char upar_names[MAXPAR][XPP_NAME_MAX+1], uvar_names[MAXODE][XPP_NAME_MAX+1];
extern double last_ic[MAXODE];
extern char this_file[];
extern char cur_dir[];
extern BROWSER my_browser;
extern BIFUR Auto;
extern int PointRadius, TextJustify, COLOR, colorline[];
extern unsigned int DONT_XORCross;
extern BC_STRUCT my_bc[MAXODE];
extern char delay_string[MAXODE][80];
extern int DelayFlag, METHOD, EqType[];
extern char *ode_names[];
extern OptionsSet notAlreadySet;
typedef struct {
    char *text, *action;
    int aflag;
} ACTION; /* form_ode.c */
extern ACTION comments[];
extern int n_comments;
extern MPEG_SAVE mpeg;
extern int n_anicom, ani_speed, ani_speed_inc, ani_grab_flag, animation_on_the_fly;
extern int ks_ncycle, ks_speed;
extern int aplot_range_count, aplot_still, aplot_tag, plot3d_auto_redraw;
extern char aplot_range_stem[256];
extern FILE *ap_fp;
extern int DLeft, DRight, DTop, DBottom;
extern char *color_names[], *auto_hint[];
extern int DoTutorial, RunImmediately;
void commander(int ch); /* commands.c */
}

#define MAX_LEN_EBOX 86 /* edit_rhs.h */

/* window ids the client draws into; plot windows are graph index + 1 */
#define WIN_AUTO 101
#define WIN_AUTO_STAB 102
#define WIN_AUTO_INFO 103
#define WIN_ANI 104
#define WIN_APLOT 105

static FILE *proto;
static int win_w[MAXPOP], win_h[MAXPOP];

/* --script FILE (docs/protocol.md "Scripts"): script_mode is set by
   json_ui_set_script(), script_error by an error message, which makes the
   process exit 1 at the end of the file. A line that does not fit the
   dialogue (an answer with no question open, or a command where an answer
   was due) stops the script at once: nothing after it can line up. */
static int script_mode, script_error;
static char script_ask[400]; /* the open question, for script_fail() */

static void script_fail(const char *what, const char *line, const char *ask)
{
    xpp_log(XPP_LOG_ERROR, "xppautX: script line %d %s\n  line: %s\n", xpp_inbox_script_line(), what, line);
    if (ask && ask[0]) xpp_log(XPP_LOG_ERROR, "  open question: %s}\n", ask);
    exit(1);
}

static void script_next(void); /* the next line, and an interruption after it */

/* ---- output ------------------------------------------------------------ */

typedef struct {
    char *s;
    size_t len, cap;
} Buf;

#define BUF_LIT(b, lit) buf_add(b, lit, sizeof(lit) - 1)

/* pending drawing ops, one buffer per window: an op for window A no longer
   has to flush window B's picture, so AUTO's diagram (101), the stability
   circle (102) and the info strip (103) each keep accumulating between
   flushes instead of interrupting each other every couple of ops. Windows
   actually in use at once (the plot windows 1..MAXPOP, WIN_AUTO/_STAB/_INFO,
   WIN_ANI) comfortably fit; if something unexpected exceeds it, flush_ops()
   is called to make room rather than grow unboundedly. */
#define MAX_OP_BUFS 40
typedef struct {
    unsigned long win;
    Buf b;
} OpBuf;
static OpBuf op_bufs[MAX_OP_BUFS];
static int n_op_bufs; /* buffers with content since the last flush, in first-use order */
static int state_dirty;

static void flush_ops(void); /* forward: get_op_buf() may need to make room */

/* the buffer for win if it already has pending content, else NULL */
static OpBuf *find_op_buf(unsigned long win)
{
    int i;
    for (i = 0; i < n_op_bufs; i++)
        if (op_bufs[i].win == win) return &op_bufs[i];
    return NULL;
}

/* the buffer for win, taking a new one (in first-use order) if this is the
   first op for it since the last flush */
static OpBuf *get_op_buf(unsigned long win)
{
    OpBuf *ob = find_op_buf(win);
    if (ob) return ob;
    if (n_op_bufs >= MAX_OP_BUFS) flush_ops();
    ob = &op_bufs[n_op_bufs++];
    ob->win = win;
    ob->b.len = 0;
    return ob;
}

/* windows 102/103 (the stability circle, the info strip) only ever show
   their latest picture: a clear should drop whatever of theirs is still
   unsent rather than ship a picture the client will immediately overwrite */
static void op_buf_discard(unsigned long win)
{
    OpBuf *ob = find_op_buf(win);
    if (ob) ob->b.len = 0;
}

static void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        b->cap = (b->len + n + 1) * 2 + 4096;
        b->s = static_cast<char *>(xpp_realloc(b->s, b->cap));
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

/* one event line to the client: stdout, or the page xppautX serves */
static void out_line(const char *s, size_t n)
{
    if (xpp_http_active()) {
        xpp_http_emit(s, n);
        return;
    }
    fwrite(s, 1, n, proto);
    fputc('\n', proto);
}

static void out_flush(void)
{
    if (proto) fflush(proto);
}

/* AUTO draws a branch as a chain of one-segment lines, and one JSON op per
   segment is what makes a redraw of a large diagram slow: the wire carries
   tens of thousands of ops and the client strokes each one. Segments that
   join up are gathered into a single ["poly",x0,y0,x1,y1,...] that the client
   strokes as one path. The op stays open while it grows, so anything else
   that writes an op closes it first. */
static int poly_open, poly_x, poly_y;
static void auto_reset_state(void); /* colour/width cache, below */
static void auto_sync_state(void);

/* the open poly, when there is one, is always the tail of WIN_AUTO's buffer */
static void close_poly(void)
{
    if (!poly_open) return;
    poly_open = 0;
    BUF_LIT(&get_op_buf(WIN_AUTO)->b, "]");
}

/* one draw event per window with pending ops, in first-use order (windows
   are separate canvases, so only the order within a window matters) */
static void diag_flush(int final); /* the AUTO diagram's data, below */
static void diag_forget(void);
static XppDiagPoint *dg;
static int dg_n, dg_cap, dg_client, dg_dirty;

static void flush_ops(void)
{
    int i;
    close_poly();
    diag_flush(0);
    for (i = 0; i < n_op_bufs; i++) {
        OpBuf *ob = &op_bufs[i];
        char head[64];
        size_t k;
        if (ob->b.len == 0) continue;
        k = (size_t)snprintf(head, sizeof head, "{\"ev\":\"draw\",\"win\":%lu,\"ops\":[", ob->win);
        /* wrap the ops in place: {"ev":"draw",...,"ops":[ ... ]} */
        if (ob->b.len + k + 3 > ob->b.cap) {
            ob->b.cap = ob->b.len + k + 3 + 4096;
            ob->b.s = static_cast<char *>(xpp_realloc(ob->b.s, ob->b.cap));
        }
        memmove(ob->b.s + k, ob->b.s, ob->b.len);
        memcpy(ob->b.s, head, k);
        memcpy(ob->b.s + k + ob->b.len, "]}", 3);
        out_line(ob->b.s, ob->b.len + k + 2);
        ob->b.len = 0;
        ob->b.s[0] = 0;
    }
    n_op_bufs = 0;
}

static void send_state(void);

/* one complete event line */
static void send_buf(Buf *b)
{
    flush_ops();
    out_line(b->s, b->len);
    out_flush();
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
    xpp_free(b.s);
}

static void op(unsigned long win, const char *fmt, ...)
{
    OpBuf *ob;
    char tmp[2048];
    va_list ap;
    int n;
    /* the open poly is WIN_AUTO's: ops for 102/103 between two diagram
       segments leave it open, which is what joins a branch into one poly */
    if (win == WIN_AUTO) close_poly();
    ob = get_op_buf(win);
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof tmp) n = sizeof tmp - 1;
    if (ob->b.len) BUF_LIT(&ob->b, ",");
    buf_add(&ob->b, tmp, n);
    if (ob->b.len > 60000) flush_ops(); /* guard is per buffer, action flushes all */
}

static void op_text(unsigned long win, const char *name, int x, int y, const char *s, int size)
{
    Buf b = {0};
    buf_printf(&b, "[\"%s\",%d,%d,", name, x, y);
    buf_str(&b, s);
    if (size >= 0) buf_printf(&b, ",%d", size);
    BUF_LIT(&b, "]");
    op(win, "%s", b.s);
    xpp_free(b.s);
}

static void json_flush(void)
{
    flush_ops();
    if (state_dirty) send_state();
    out_flush();
}

/* ---- input ------------------------------------------------------------- */

/* Input comes from the inbox (xpp_inbox.h): reader threads fill it, the
   HTTP server in browser mode and a stdin reader with --server, so the core
   never reads a descriptor itself.

   Which queue a reader takes from (see classify() below): the command loop
   takes lines in the order they came (XPP_INBOX_ARRIVAL), a prompt the
   control lines first (XPP_INBOX_ANY), a long computation's checkpoint only
   the control queue, so it never takes (and never drops) a command meant to
   run after it.

   The next line (without newline) from `which`, in a buffer that stays
   valid until the next call, with its sequence number in line_seq; NULL
   when nothing came within wait_ms (< 0 blocks, 0 polls). Lines can be long
   (the pixels of a window in an answer). End of input quits. */
static unsigned long line_seq;

static char *read_line(int which, int wait_ms)
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
        exit(script_mode && script_error ? 1 : 0);
    default:
        return NULL;
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
static void apply_ani_size(void);
static void apply_set(const char *line);
static void browser_rows(const char *line);

/* commands that make sense at any moment, even while a prompt is open */
static int handle_async(const char *line)
{
    if (is_cmd(line, "size")) {
        apply_size(line);
        return 1;
    }
    if (is_cmd(line, "quit")) exit(script_mode && script_error ? 1 : 0);
    if (is_cmd(line, "state")) {
        send_state();
        return 1;
    }
    if (is_cmd(line, "browser") && js_find(line, "from")) {
        browser_rows(line);
        return 1;
    }
    return 0;
}

/* The input classifier (xpp_inbox.h), on the reader thread: it only parses
   the line and touches xpp_job's atomics.

   abort and quit go to the control queue always, and cancel the running
   job (and any not yet begun that came before them) at once: the
   computation sees xpp_job_cancelled() at its next check without reading
   input. Quit then exits when the engine takes the line.

   key, set, size, state, browser with from, and ani pause/fast/slow are
   what a computation's checkpoint acts on (Escape stops it, a set changes
   a parameter under it, the animation loop changes speed): they go to the
   control queue only while a job runs, and are meant for that job; a set
   sent then applies at once, ahead of commands queued behind the job, as
   it always has. Sent while no job runs they stay normal: prompts take
   control lines first and checkpoints nothing else, so a control line
   could be taken by the next command's prompt or computation ahead of
   commands sent before it; a normal line runs in its turn. The command
   loop reads both queues in arrival order, so a control line no job
   consumed also runs in its turn, as an ordinary command.

   Everything else is normal: a command sent while a job runs waits for
   it, and runs after the job's idle. */
static int classify(const char *line, unsigned long seq)
{
    char c[16], o[16];
    if (!get_str(line, "cmd", c, sizeof c)) return XPP_INBOX_NORMAL;
    if (strcmp(c, "abort") == 0 || strcmp(c, "quit") == 0) {
        xpp_job_cancel(seq);
        return XPP_INBOX_CONTROL;
    }
    if (!xpp_job_running()) return XPP_INBOX_NORMAL;
    if (strcmp(c, "key") == 0 || strcmp(c, "set") == 0 || strcmp(c, "size") == 0 || strcmp(c, "state") == 0)
        return XPP_INBOX_CONTROL;
    if (strcmp(c, "browser") == 0 && js_find(line, "from")) return XPP_INBOX_CONTROL;
    if (strcmp(c, "ani") == 0 && get_str(line, "op", o, sizeof o) &&
        (strcmp(o, "pause") == 0 || strcmp(o, "fast") == 0 || strcmp(o, "slow") == 0 || strcmp(o, "speed") == 0))
        return XPP_INBOX_CONTROL;
    return XPP_INBOX_NORMAL;
}

#define ANI_PAUSE (-1)

/* ani fast, slow and speed: the delay between two frames of Go, ms; 0 when
   op is none of them */
static int ani_speed_op(const char *o, const char *line)
{
    if (strcmp(o, "fast") == 0) {
        if ((ani_speed -= ani_speed_inc) < 0) ani_speed = 0;
    } else if (strcmp(o, "slow") == 0) {
        if ((ani_speed += ani_speed_inc) > 100) ani_speed = 100;
    } else if (strcmp(o, "speed") == 0) {
        double ms = get_num(line, "ms", ani_speed);
        ani_speed = ms < 0 ? 0 : ms > 1000 ? 1000 : (int)ms; /* the .ani `speed` command's range */
    } else
        return 0;
    return 1;
}

/* a control line taken by a checkpoint: every kind classify() puts there
   is acted on, none dropped. Returns ESC for abort, the code of a key,
   ANI_PAUSE for the animation's Pause, 64 otherwise. */
static int control_line(const char *line)
{
    char k[32];
    if (handle_async(line)) return 64;
    if (is_cmd(line, "abort")) return ESC;
    if (is_cmd(line, "key")) {
        get_str(line, "key", k, sizeof k);
        return key_code(k);
    }
    if (is_cmd(line, "set")) {
        apply_set(line);
        return 64;
    }
    if (is_cmd(line, "ani")) {
        get_str(line, "op", k, sizeof k);
        if (strcmp(k, "pause") == 0) return ANI_PAUSE;
        ani_speed_op(k, line);
    }
    return 64;
}

/* ---- prompts ------------------------------------------------------------- */

static int ask_id;
static int ask_user; /* the open ask is the user's to answer, not the client's (pixels) */
static char *answer;
static size_t answer_cap;

/* b holds {"ev":"ask","id":N,"kind":... without the closing brace; send
   it and wait for the answer, which is left in answer[]. Returns 1 when the
   answer says ok (or has no ok member), 0 when cancelled. */
static int ask_wait(Buf *b, int id)
{
    BUF_LIT(b, "}");
    diag_flush(1);
    auto_data_update(1);
    json_flush();
    if (script_mode) snprintf(script_ask, sizeof script_ask, "%s", b->s);
    send_buf(b);
    xpp_free(b->s);
    /* a script's next line is its answer to this ask (ui_json.c "Which
       queue" comment above, and docs/protocol.md "Scripts") */
    if (script_mode) script_next();
    for (;;) {
        char *line = read_line(XPP_INBOX_ANY, -1);
        int lid;
        if (handle_async(line)) {
            flush_ops();
            out_flush();
            continue;
        }
        /* an id-less answer answers whichever ask is pending: a script
           cannot know the id handed out at run time (docs/protocol.md) */
        lid = (int)get_num(line, "id", -1);
        if (is_cmd(line, "answer") && (lid == -1 || lid == id)) {
            size_t n = strlen(line) + 1;
            const char *ok;
            if (n > answer_cap) {
                answer_cap = n;
                answer = static_cast<char *>(xpp_realloc(answer, n));
            }
            memcpy(answer, line, n);
            /* an Abort sent before this answer no longer stops the command */
            if (ask_user) xpp_job_resume(line_seq);
            ok = js_find(answer, "ok");
            return ok == NULL || js_num(ok, 0) != 0;
        }
        /* anything else (keys typed at the plot while a dialog is up) is
           dropped, as the X11 dialogs do; for a script this line was
           supposed to answer this ask, and nothing after it can line up */
        if (script_mode) script_fail("does not answer the open question", line, script_ask);
    }
}

static int ask_begin(Buf *b, const char *kind)
{
    b->s = NULL;
    b->len = b->cap = 0;
    ask_id++;
    ask_user = strcmp(kind, "pixels") != 0;
    buf_printf(b, "{\"ev\":\"ask\",\"id\":%d,\"kind\":\"%s\"", ask_id, kind);
    return ask_id;
}

static void j_err_msg(const char *msg)
{
    /* a script that provokes an error fails the run (docs/protocol.md) */
    if (script_mode) script_error = 1;
    send_simple("message", "error", msg);
}

static void j_ping(void) { send_simple("ping", NULL, NULL); }

static void j_bottom_msg(int line, char *msg)
{
    (void)line;
    send_simple("message", "bottom", msg);
}

static void j_message_box(const char *msg) { send_simple("message", "box", msg); }
static void j_kill_message_box(void) { send_simple("message", "box", ""); }
static void j_title_text(char *s) { send_simple("title", "text", s); }
static void j_canvas_xy(char *s) { send_simple("message", "xy", s); }

static int j_dialog(const char *title, const char *name, char *value, const char *ok, const char *cancel, int max)
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

static void j_respond_box(const char *button, const char *message)
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
                        char values[][MAX_LEN_SBOX], int maxchar)
{
    char *v[64];
    int i;
    (void)row; (void)col; (void)maxchar;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) v[i] = values[i];
    return form(title, names, n, v, MAX_LEN_SBOX);
}

static int j_edit_box(int n, char *title, char **names, char **values)
{
    return form(title, names, n, values, MAX_LEN_EBOX);
}

/* the file selector lists the directory like the X11 one; an answer with
   "cd" changes directory (as X11 does, for good) and asks again. "mode"
   says whether the command reads the file or writes it, so a client can
   show an open or a save dialog (docs/ui-v2.md section 4). */
static int j_file_selector(char *title, char *file, char *wild)
{
    char pattern[256], cd[1024];
    snprintf(pattern, sizeof pattern, "%s", wild);
    if (!cur_dir[0]) get_directory(cur_dir);
    for (;;) {
        Buf b;
        FILEINFO ff;
        int id = ask_begin(&b, "file");
        BUF_LIT(&b, ",\"title\":");
        buf_str(&b, title);
        BUF_LIT(&b, ",\"mode\":");
        buf_str(&b, xpp_files_ask_mode(title));
        BUF_LIT(&b, ",\"file\":");
        buf_str(&b, file);
        BUF_LIT(&b, ",\"wild\":");
        buf_str(&b, pattern);
        BUF_LIT(&b, ",\"dir\":");
        buf_str(&b, cur_dir);
        if (get_fileinfo(pattern, cur_dir, &ff)) {
            BUF_LIT(&b, ",\"dirs\":");
            buf_str_array(&b, ff.dirnames, ff.ndirs);
            BUF_LIT(&b, ",\"files\":");
            buf_str_array(&b, ff.filenames, ff.nfiles);
            free_finfo(&ff);
        }
        if (!ask_wait(&b, id)) return 0;
        if (get_str(answer, "wild", cd, sizeof cd) && cd[0]) snprintf(pattern, sizeof pattern, "%.255s", cd);
        if (get_str(answer, "cd", cd, sizeof cd) && cd[0]) {
            change_directory(cd);
            continue;
        }
        if (!js_find(answer, "file")) continue; /* a new pattern alone lists again */
        get_str(answer, "file", file, 256);
        return file[0] != 0;
    }
}

/* a data coordinate as the nearest pixel of an axis that maps pixel p0 to
   v0 and p1 to v1 (the inverse of scale_to_real, auto_motion_xy) */
static int data_to_pixel(double v, double v0, double v1, int p0, int p1)
{
    double p;
    if (!(v1 != v0) || !isfinite(v)) return p0;
    p = p0 + (v - v0) * (p1 - p0) / (v1 - v0);
    if (p > 1e6) p = 1e6;
    if (p < -1e6) p = -1e6;
    return (int)lround(p);
}

/* point k (0: x,y; 1: x2,y2) of the answer to a mouse, rubber, drag or grab
   ask in window win: pixels, or data coordinates xd,yd (xd2,yd2) converted
   with the window's current axes to the pixels that map back to them, so
   the command goes on exactly as for a click there (docs/protocol.md) */
static void answer_point(unsigned long win, int k, int *x, int *y)
{
    static const char *px[] = {"x", "x2"}, *py[] = {"y", "y2"}, *dx[] = {"xd", "xd2"}, *dy[] = {"yd", "yd2"};
    const char *jx = js_find(answer, dx[k]), *jy = js_find(answer, dy[k]);
    if (!jx || !jy) {
        *x = (int)get_num(answer, px[k], 0);
        *y = (int)get_num(answer, py[k], 0);
    } else if (win == WIN_AUTO) {
        *x = data_to_pixel(js_num(jx, 0), Auto.xmin, Auto.xmax, Auto.x0, Auto.x0 + Auto.wid);
        *y = data_to_pixel(js_num(jy, 0), Auto.ymin, Auto.ymax, Auto.y0 + Auto.hgt, Auto.y0);
    } else {
        get_draw_area();
        *x = data_to_pixel(js_num(jx, 0), MyGraph->xlo, MyGraph->xhi, DLeft, DRight);
        *y = data_to_pixel(js_num(jy, 0), MyGraph->ylo, MyGraph->yhi, DBottom, DTop);
    }
}

static int mouse_ask(unsigned long win, const char *kind, int flag, int *v, int nv)
{
    Buf b;
    int i, id = ask_begin(&b, kind);
    buf_printf(&b, ",\"win\":%lu,\"flag\":%d", win, flag);
    if (!ask_wait(&b, id)) return 0;
    for (i = 0; i < nv / 2; i++) answer_point(win, i, &v[2 * i], &v[2 * i + 1]);
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
    xpp_free(b.s);
}

/* ---- long loops ------------------------------------------------------------ */

static int j_check_abort(void)
{
    char *line;
    static double last;
    /* let the client see the picture grow, a few frames a second */
    if (xpp_every(&last, 0.05)) {
        flush_ops();
        out_flush();
    }
    /* only the control queue: a command sent during the computation waits
       for it (classify()) */
    while ((line = read_line(XPP_INBOX_CONTROL, 0)) != NULL) {
        int r = control_line(line);
        if (r != 64 && r != ANI_PAUSE) return r;
    }
    return 64;
}

static int j_progress_begin(void) { return 100; }

static void j_progress(int nit, int icount, int cwidth)
{
    static double last;
    Buf b = {0};
    (void)cwidth;
    if (!xpp_every(&last, 0.1)) return;
    buf_printf(&b, "{\"ev\":\"progress\",\"n\":%d,\"of\":%d}", icount, nit);
    send_buf(&b);
    xpp_free(b.s);
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
    BUF_LIT(&b, "],\"bcs\":[");
    for (i = 0; i < NODE; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, my_bc[i].name);
        BUF_LIT(&b, ",");
        buf_str(&b, my_bc[i].string);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]");
    if (DelayFlag) {
        BUF_LIT(&b, ",\"delays\":[");
        for (i = 0; i < NODE; i++) {
            if (i) BUF_LIT(&b, ",");
            BUF_LIT(&b, "[");
            buf_str(&b, uvar_names[i]);
            BUF_LIT(&b, ",");
            buf_str(&b, delay_string[i]);
            BUF_LIT(&b, "]");
        }
        BUF_LIT(&b, "]");
    }
    /* pixel to plot coordinates of the active window and the AUTO diagram,
       for the x,y readout under the mouse (scale_to_real, auto_motion_xy) */
    get_draw_area();
    buf_printf(&b, ",\"view\":{\"win\":%lu,\"left\":%d,\"right\":%d,\"top\":%d,\"bottom\":%d,"
               "\"xlo\":%g,\"xhi\":%g,\"ylo\":%g,\"yhi\":%g,\"three\":%d",
               (unsigned long)draw_win, DLeft, DRight, DTop, DBottom, MyGraph->xlo, MyGraph->xhi,
               MyGraph->ylo, MyGraph->yhi, MyGraph->ThreeDFlag);
    /* a 3D window's angles (view3d); only then are they set at all */
    if (MyGraph->ThreeDFlag && isfinite(MyGraph->Theta) && isfinite(MyGraph->Phi))
        buf_printf(&b, ",\"theta\":%g,\"phi\":%g", MyGraph->Theta, MyGraph->Phi);
    BUF_LIT(&b, "}");
    if (Auto.exist)
        buf_printf(&b, ",\"auto\":{\"x0\":%d,\"y0\":%d,\"wid\":%d,\"hgt\":%d,\"xmin\":%g,\"xmax\":%g,"
                   "\"ymin\":%g,\"ymax\":%g}", Auto.x0, Auto.y0, Auto.wid, Auto.hgt, Auto.xmin, Auto.xmax,
                   Auto.ymin, Auto.ymax);
    buf_printf(&b, ",\"rows\":%d,\"menu\":%d,\"win\":%lu", my_browser.maxrow, help_menu,
               (unsigned long)draw_win);
    if (xpp_session_set_file()[0]) {
        BUF_LIT(&b, ",\"session\":{\"set\":");
        buf_str(&b, xpp_session_set_file());
        if (xpp_session_auto_file()[0]) {
            BUF_LIT(&b, ",\"auto\":");
            buf_str(&b, xpp_session_auto_file());
        }
        BUF_LIT(&b, "}");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

static void j_state_dirty(void) { state_dirty = 1; }

/* ---- data browser ---------------------------------------------------------------
   The client shows a scrolling table and asks for the block of rows and
   columns it can see; my_browser.row0 is the selected row the core's
   browser commands (Get, First, Last, Find) use. */
static int br_from, br_count, br_col = 1, br_ncol = 1;
static int browser_dirty;
static int aplot_dirty; /* the data behind an array plot changed */

static void buf_float(Buf *b, double z, int digits)
{
    if (z != z || z > 1e300 || z < -1e300) BUF_LIT(b, "null"); /* not JSON numbers */
    else buf_printf(b, "%.*g", digits, z);
}

static void send_browser(void)
{
    Buf b = {0};
    int i, j, last, maxcol = my_browser.maxcol;
    browser_dirty = 0;
    if (br_from > my_browser.maxrow - 1) br_from = my_browser.maxrow > 0 ? my_browser.maxrow - 1 : 0;
    if (br_from < 0) br_from = 0;
    if (br_col > maxcol - 1) br_col = maxcol - 1;
    if (br_col < 1) br_col = 1;
    buf_printf(&b, "{\"ev\":\"browser\",\"rows\":%d,\"row0\":%d,\"start\":%d,\"end\":%d,\"cols\":[\"T\"",
               my_browser.dataflag ? my_browser.maxrow : 0, my_browser.row0, my_browser.istart, my_browser.iend);
    for (j = 1; j < maxcol; j++) {
        BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[j - 1]);
    }
    buf_printf(&b, "],\"from\":%d,\"col\":%d,\"data\":[", br_from, br_col);
    last = my_browser.dataflag ? br_from + br_count : br_from;
    if (last > my_browser.maxrow) last = my_browser.maxrow;
    for (i = br_from; i < last; i++) {
        if (i > br_from) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        /* 9 significant digits read back as exactly the stored floats (as series) */
        buf_float(&b, my_browser.data[0][i], 9);
        for (j = br_col; j < br_col + br_ncol && j < maxcol; j++) {
            BUF_LIT(&b, ",");
            buf_float(&b, my_browser.data[j][i], 9);
        }
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
}

/* {"cmd":"browser","from":row,"count":n,"col":first column,"ncol":n}: the
   block the client can see; answered at once, even during a prompt */
static void browser_rows(const char *line)
{
    br_from = (int)get_num(line, "from", 0);
    br_count = (int)get_num(line, "count", 100);
    br_col = (int)get_num(line, "col", 1);
    br_ncol = (int)get_num(line, "ncol", 20);
    if (br_count > 2000) br_count = 2000;
    if (br_ncol > 500) br_ncol = 500;
    send_browser();
}

static void j_browser_changed(int i)
{
    (void)i;
    plot_data_changed();
    state_dirty = 1;
    browser_dirty = 1;
    aplot_dirty = 1; /* X11 redraws an auto-redrawn array plot on expose */
}

/* {"cmd":"browser","op":...,"row":selected row} */
static void browser_command(const char *line)
{
    char o[16];
    int row = (int)get_num(line, "row", -1);
    get_str(line, "op", o, sizeof o);
    if (row >= 0 && row < my_browser.maxrow) my_browser.row0 = row;
    if (strcmp(o, "find") == 0) data_find(&my_browser);
    else if (strcmp(o, "get") == 0) data_get(&my_browser);
    else if (strcmp(o, "replace") == 0) data_replace(&my_browser);
    else if (strcmp(o, "unreplace") == 0) data_unreplace(&my_browser);
    else if (strcmp(o, "table") == 0) data_table(&my_browser);
    else if (strcmp(o, "load") == 0) data_read(&my_browser);
    else if (strcmp(o, "write") == 0) data_write(&my_browser);
    else if (strcmp(o, "first") == 0) data_first(&my_browser);
    else if (strcmp(o, "last") == 0) data_last(&my_browser);
    else if (strcmp(o, "restore") == 0) data_restore(&my_browser);
    else if (strcmp(o, "addcol") == 0) data_add_col(&my_browser);
    else if (strcmp(o, "delcol") == 0) data_del_col(&my_browser);
    else if (strcmp(o, "close") == 0) br_count = 0;
    browser_dirty = 1;
}

/* the ICs box's xvst (0) and pp (1) buttons: {"cmd":"plotvars","how":0,"names":[...]} */
static void plotvars_command(const char *line)
{
    int isck[MAXODE], i, n = NODE + NMarkov;
    const char *arr = js_find(line, "names");
    char name[NAME_IN];
    memset(isck, 0, sizeof isck);
    for (i = 0; arr && js_elem(arr, i); i++) {
        int k;
        if (!js_string(js_elem(arr, i), name, sizeof name)) continue;
        for (k = 0; k < n; k++)
            if (strcasecmp(uvar_names[k], name) == 0) isck[k] = 1;
    }
    if ((int)get_num(line, "how", 0) == 2) {
        /* arry: the array of variables from the first to the second checked */
        int list[2], k = 0;
        for (i = 0; i < n && k < 2; i++)
            if (isck[i]) list[k++] = i + 1;
        if (k == 2) optimize_aplot(list);
        return;
    }
    plot_checked_vars((int)get_num(line, "how", 0), isck, n);
}

/* ---- the plot as data (plot_data.cpp) ---------------------------------------- */

/* a plots or series event: after the pending drawing, like any event */
static void data_emit(const char *line, size_t n)
{
    flush_ops();
    out_line(line, n);
    out_flush();
}

/* {"cmd":"data","events":["series","plots","nullclines","dfield","marks","ani"],"enc":"f32"}:
   the data events the client wants from now on (an empty list stops them);
   each is sent at the end of this command, which is what a client that
   (re)connects needs. hello.features lists the names known here. "enc":"f32"
   sends the events' value arrays as base64 of little-endian float32,
   anything else as JSON numbers. */
static void data_command(const char *line)
{
    const char *arr = js_find(line, "events");
    char name[32], enc[8];
    int i, series = 0, plots = 0, nullclines = 0, dfield = 0, marks = 0, ani = 0, autoinfo = 0, f32;
    for (i = 0; arr && js_elem(arr, i); i++) {
        if (!js_string(js_elem(arr, i), name, sizeof name)) continue;
        if (strcmp(name, "series") == 0) series = 1;
        else if (strcmp(name, "plots") == 0) plots = 1;
        else if (strcmp(name, "nullclines") == 0) nullclines = 1;
        else if (strcmp(name, "dfield") == 0) dfield = 1;
        else if (strcmp(name, "marks") == 0) marks = 1;
        else if (strcmp(name, "ani") == 0) ani = 1;
        else if (strcmp(name, "autoinfo") == 0) autoinfo = 1;
    }
    f32 = get_str(line, "enc", enc, sizeof enc) && strcmp(enc, "f32") == 0;
    plot_data_subscribe(series, plots, f32);
    phase_data_subscribe(nullclines, dfield, f32);
    marks_data_subscribe(marks, f32);
    ani_data_subscribe(ani);
    auto_data_subscribe(autoinfo);
}

/* the equations window: one "dX/dT=..." line per equation (eig_list.c) */
static void send_equations(void)
{
    Buf b = {0}, line = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"equations\",\"lines\":[");
    for (i = 0; i < NEQ; i++) {
        const char *name = uvar_names[i], *rhs = ode_names[i] ? ode_names[i] : "";
        line.len = 0;
        if (i < NODE && EqType[i] != 1 && METHOD > 0) BUF_LIT(&line, "d");
        buf_add(&line, name, strlen(name));
        if (i < NODE && EqType[i] == 1) BUF_LIT(&line, "(t)");
        else if (i < NODE && METHOD == 0) BUF_LIT(&line, "(n+1)");
        else if (i < NODE) BUF_LIT(&line, "/dT");
        BUF_LIT(&line, "=");
        buf_add(&line, rhs, strlen(rhs));
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, line.s);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
    xpp_free(line.s);
}
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

/* a blanked plot window no longer shows its nullclines, direction field
   and flows (phase_data.h), nor its marks (marks_data.h), until they are
   drawn again */
static void j_blank_draw_window(void)
{
    int i;
    op(draw_win, "[\"clear\"]");
    for (i = 0; i < MAXPOP; i++)
        if (graph[i].Use && graph[i].w == draw_win) {
            phase_data_cleared(i);
            marks_data_cleared(i);
        }
}

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
    xpp_free(b.s);
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
    char string[256], text[256];
    int x, y, size = 2;
    strcpy(string, "");
    if (new_string(const_cast<char *>("Text: "), string) == 0) return;
    if (string[0] == '%') {
        fillintext(&string[1], text);
        strcpy(string, text);
    }
    new_int(const_cast<char *>("Size 0-4 :"), &size);
    if (size > 4) size = 4;
    if (size < 0) size = 0;
    j_message_box("Place text with mouse");
    if (j_get_mouse_xy(&x, &y)) {
        fillintext(string, text);
        op_text(draw_win, "stext", x, y, text, size);
        marks_data_label(draw_win, add_label(string, x, y, size, 0), text);
    }
    j_kill_message_box();
}

static void j_draw_freeze(void) { draw_freeze(draw_win); }
static void j_draw_text(int x, int y, char *s);
static void j_put_text(int x, int y, char *s) { j_draw_text(x, y, s); }

/* ---- pixels -------------------------------------------------------------------
   Only the client has the rendered picture. Frame and GIF writers ask for
   it: {"kind":"pixels","win":W} or {"film":i} (a kinescope frame), answered
   with w, h and base64 RGB. */
static int b64_value(int c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* malloc'd w*h*3 RGB bytes, or NULL when cancelled */
static unsigned char *ask_pixels(int win, int film, int *w, int *h)
{
    Buf b;
    const char *v;
    unsigned char *rgb;
    size_t n, k = 0;
    int q[4], nq = 0, id = ask_begin(&b, "pixels");
    if (film >= 0) buf_printf(&b, ",\"film\":%d", film);
    else buf_printf(&b, ",\"win\":%d", win);
    if (!ask_wait(&b, id)) return NULL;
    *w = (int)get_num(answer, "w", 0);
    *h = (int)get_num(answer, "h", 0);
    v = js_find(answer, "rgb");
    if (!v || *v != '"' || *w <= 0 || *h <= 0 || *w > 8192 || *h > 8192) return NULL;
    n = (size_t)*w * (size_t)*h * 3;
    rgb = static_cast<unsigned char *>(xpp_calloc(n, 1));
    for (v++; *v && *v != '"' && k < n; v++) {
        int d = b64_value((unsigned char)*v);
        if (d < 0) continue;
        q[nq++] = d;
        if (nq == 4) {
            rgb[k++] = (unsigned char)(q[0] << 2 | q[1] >> 4);
            if (k < n) rgb[k++] = (unsigned char)(q[1] << 4 | q[2] >> 2);
            if (k < n) rgb[k++] = (unsigned char)(q[2] << 6 | q[3]);
            nq = 0;
        }
    }
    if (nq >= 2 && k < n) rgb[k++] = (unsigned char)(q[0] << 2 | q[1] >> 4);
    if (nq >= 3 && k < n) rgb[k++] = (unsigned char)(q[1] << 4 | q[2] >> 2);
    return rgb;
}

static int write_ppm(const char *file, unsigned char *rgb, int w, int h)
{
    FILE *fp = fopen(file, "wb");
    if (!fp) return 0;
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 3, (size_t)w * h, fp);
    fclose(fp);
    return 1;
}

/* the GIF writer takes at most 256 colours; a canvas smooths its lines */
static void web_safe_colors(unsigned char *rgb, int w, int h)
{
    size_t i, n = (size_t)w * h * 3;
    for (i = 0; i < n; i++) rgb[i] = (unsigned char)(((rgb[i] + 25) / 51) * 51);
}

static void write_gif(const char *file, unsigned char *rgb, int w, int h)
{
    FILE *fp = fopen(file, "wb");
    if (!fp) return;
    web_safe_colors(rgb, w, h);
    gif_stuff_ppm(rgb, w, h, fp, MAKE_ONE_GIF);
    fclose(fp);
}

/* ---- kinescope: the client keeps the frames ------------------------------------ */
#define MAXFILM 250 /* kinescope.c */
static int film_count;

static void send_film(const char *what)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"film\",\"op\":\"%s\",\"count\":%d,\"win\":%lu,\"cycles\":%d,\"delay\":%d}",
               what, film_count, (unsigned long)draw_win, ks_ncycle, ks_speed);
    send_buf(&b);
    xpp_free(b.s);
}

static int j_film_clip(void)
{
    if (film_count >= MAXFILM) return 0;
    film_count++;
    send_film("capture");
    return 1;
}

static void j_reset_film(void)
{
    film_count = 0;
    send_film("reset");
}

static void j_movie_play_back(void)
{
    if (film_count) send_film("play");
}

static void j_movie_auto_play(void)
{
    if (film_count) send_film("autoplay");
}

static void j_movie_save(char *basename, int fmat)
{
    char file[XPP_MAX_NAME + 32];
    int i, w, h;
    for (i = 0; i < film_count; i++) {
        unsigned char *rgb = ask_pixels(0, i, &w, &h);
        if (!rgb) return;
        snprintf(file, sizeof file, "%s_%d.%s", basename, i, fmat == 1 ? "ppm" : "gif");
        if (fmat == 1) write_ppm(file, rgb, w, h);
        else write_gif(file, rgb, w, h);
        xpp_free(rgb);
    }
}

static void j_movie_make_anigif(void)
{
    FILE *fp;
    int i, w, h, w0 = 0, h0 = 0;
    if (film_count == 0) return;
    fp = fopen("anim.gif", "wb");
    if (!fp) return;
    set_global_map(1);
    for (i = 0; i < film_count; i++) {
        unsigned char *rgb = ask_pixels(0, i, &w, &h);
        if (!rgb) break;
        if (i == 0) {
            w0 = w;
            h0 = h;
        } else if (w != w0 || h != h0) {
            xpp_free(rgb);
            j_err_msg("All clips must be same size");
            break;
        }
        web_safe_colors(rgb, w, h);
        gif_stuff_ppm(rgb, w, h, fp, i == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
        xpp_free(rgb);
    }
    end_ani_gif(fp);
    fclose(fp);
    set_global_map(0);
}
/* one pointer event of a drag in window win: 1 down, 2 move, 3 up; 0 when
   a key or Cancel ends the drag */
static int ask_drag(unsigned long win, int *x, int *y)
{
    Buf b;
    char what[8];
    int id = ask_begin(&b, "drag");
    buf_printf(&b, ",\"win\":%lu", win);
    if (!ask_wait(&b, id) || !get_str(answer, "what", what, sizeof what)) return 0;
    answer_point(win, 0, x, y);
    return strcmp(what, "down") == 0 ? 1 : strcmp(what, "move") == 0 ? 2 : strcmp(what, "up") == 0 ? 3 : 0;
}

/* Window/zoom Scroll: drag the plot (rubber.c x11_scroll_window) */
static void j_scroll_window(void)
{
    int i, j, t, state = 0;
    float x, y, x0 = 0, y0 = 0, dx = 0, dy = 0;
    float xlo = MyGraph->xlo, ylo = MyGraph->ylo, xhi = MyGraph->xhi, yhi = MyGraph->yhi;
    send_simple("message", "box", "Drag the plot to scroll it; any key ends");
    while ((t = ask_drag((unsigned long)draw_win, &i, &j)) != 0) {
        if (t == 1 && state == 0) {
            scale_to_real(i, j, &x0, &y0);
            state = 1;
        } else if (t == 2 && state == 1) {
            scale_to_real(i, j, &x, &y);
            dx = -(x - x0) / 2;
            dy = -(y - y0) / 2;
            update_view(xlo + dx, xhi + dx, ylo + dy, yhi + dy);
        } else if (t == 3) {
            state = 0;
            xlo += dx;
            xhi += dx;
            ylo += dy;
            yhi += dy;
            dx = dy = 0;
        }
        json_flush();
    }
    j_kill_message_box();
}

/* AUTO Axes/Scroll: drag the diagram (auto_x11.c x11_auto_scroll_window) */
static void j_auto_scroll_window(void)
{
    int i, j, t, i0 = 0, j0 = 0, state = 0;
    float xlo = Auto.xmin, ylo = Auto.ymin, xhi = Auto.xmax, yhi = Auto.ymax, dx = 0, dy = 0;
    send_simple("message", "auto", "Drag the diagram to scroll it; any key ends");
    while ((t = ask_drag(WIN_AUTO, &i, &j)) != 0) {
        if (t == 1 && state == 0) {
            i0 = i;
            j0 = j;
            state = 1;
        } else if (t == 2 && state == 1) {
            dx = (float)(i0 - i) * (xhi - xlo) / (float)Auto.wid;
            dy = (float)(j - j0) * (yhi - ylo) / (float)Auto.hgt;
            auto_update_view(xlo + dx, xhi + dx, ylo + dy, yhi + dy);
        } else if (t == 3) {
            state = 0;
            xlo += dx;
            xhi += dx;
            ylo += dy;
            yhi += dy;
            dx = dy = 0;
        }
        json_flush();
    }
}

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
    xpp_free(b.s);
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

/* ---- array plot ------------------------------------------------------------------
   The picture is a grid of colour indices (aplotwin.c redraw_aplot): the
   classic client paints them at the size of its window. `values` (added for
   web2/, docs/ui-v2.md T12) carries the same cells' numbers before that
   mapping, so a client can pick its own colour scale from them and zmin/zmax;
   encoded like a series column (series_enc.h), base64 float32 when the
   client last asked for that (data_command's "enc":"f32", reused here via
   plot_data_want_f32 since aplot is not itself in the "data" subscription
   list -- it is sent whenever the window is alive and dirtied, as before). */
#define FIRSTCOLOR 30 /* aplotwin.c */

static void send_aplot(const char *tag)
{
    Buf b = {0};
    char sroot[100];
    int num, i, j, nx, ny, nrows = my_browser.maxrow;
    double tlo = 0.0, thi = 20.0;
    APLOT *ap = &aplot;
    float *vals;
    int f32 = plot_data_want_f32();
    aplot_dirty = 0;
    if (!ap->alive) return;
    get_root(ap->name, sroot, &num);
    buf_printf(&b, "{\"ev\":\"aplot\",\"title\":\"");
    buf_printf(&b, "%.60s%d..%d\"", sroot, num, num + ap->nacross - 1);
    nx = ap->ncskip > 0 ? ap->nacross / ap->ncskip : 0;
    ny = ap->ndown;
    if (nrows <= 2 || ap->plotdef == 0 || ap->nacross < 2 || ap->ndown < 2) nx = ny = 0;
    if (nx) {
        j = ap->nstart;
        if (j > 0 && j < nrows) tlo = my_browser.data[0][j];
        j = ap->nstart + ap->nskip * (ap->ndown - 1);
        if (j >= nrows) j = nrows - 1;
        if (j >= 0) thi = my_browser.data[0][j];
    }
    buf_printf(&b, ",\"tlo\":%g,\"thi\":%g,\"zmin\":%g,\"zmax\":%g,\"first\":%d,\"ncolors\":%d,\"nx\":%d,\"ny\":%d",
               tlo, thi, ap->zmin, ap->zmax, FIRSTCOLOR, color_total, nx, ny);
    if (tag) {
        BUF_LIT(&b, ",\"tag\":");
        buf_str(&b, tag);
    }
    vals = nx * ny > 0 ? (float *)xpp_malloc(sizeof(float) * (size_t)(nx * ny)) : NULL;
    /* -1 (cells) / NaN (values): past the stored rows or columns (left blank) */
    BUF_LIT(&b, ",\"cells\":[");
    for (j = 0; j < ny; j++) {
        int jb = ap->nstart + ap->nskip * j;
        for (i = 0; i < nx; i++) {
            int ib = ap->index0 + i * ap->ncskip, c = -1;
            float v = NAN;
            if (ib < my_browser.maxcol && jb < nrows && jb >= 0) {
                double z = my_browser.data[ib][jb];
                v = (float)z;
                if (ap->zmax > ap->zmin) {
                    c = (int)(color_total * (z - ap->zmin) / (ap->zmax - ap->zmin));
                    if (c < 0) c = 0;
                    if (c > color_total) c = color_total;
                }
            }
            if (vals) vals[j * nx + i] = v;
            if (i || j) BUF_LIT(&b, ",");
            buf_printf(&b, "%d", c);
        }
    }
    BUF_LIT(&b, "]");
    if (f32) BUF_LIT(&b, ",\"enc\":\"f32\"");
    BUF_LIT(&b, ",\"values\":");
    {
        size_t len;
        char *t = xpp_series_values(vals, nx * ny, f32, &len);
        if (t) {
            buf_add(&b, t, len);
            xpp_free(t);
        } else BUF_LIT(&b, "[]");
    }
    if (vals) xpp_free(vals);
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

static void j_aplot_make(char *name)
{
    if (aplot.alive) return;
    aplot.alive = 1;
    aplot.plotw = aplot.width - 30 - 10 * DCURXs;
    aplot.ploth = aplot.height - 55;
    send_window("create", WIN_APLOT, aplot.plotw, aplot.ploth, name);
}

static void j_aplot_redraw(void) { send_aplot(NULL); }

/* write the picture the client shows as a GIF: one file, or a frame of
   the range movie (aplotwin.c gif_aplot_all) */
static void aplot_gif(const char *file, int still)
{
    int w, h;
    unsigned char *rgb;
    if (still == 1 || aplot_range_count == 0) {
        if ((ap_fp = fopen(file, "wb")) == NULL) {
            j_err_msg("Cannot open file ");
            return;
        }
    }
    rgb = ask_pixels(WIN_APLOT, -1, &w, &h);
    if (rgb) {
        web_safe_colors(rgb, w, h);
        if (still == 1) gif_stuff_ppm(rgb, w, h, ap_fp, MAKE_ONE_GIF);
        else gif_stuff_ppm(rgb, w, h, ap_fp, aplot_range_count == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
        xpp_free(rgb);
    }
    if (still == 1) fclose(ap_fp);
}

static void j_aplot_draw_one(char *tag)
{
    char file[300];
    send_aplot(aplot_tag ? tag : NULL);
    snprintf(file, sizeof file, "%s.%d.gif", aplot_range_stem, aplot_range_count);
    aplot_gif(file, aplot_still);
    aplot_range_count++;
}

/* "Use this view" (docs/ui-v2.md T9): {"cmd":"view","win":w,"xlo":..,
   "xhi":..,"ylo":..,"yhi":..} sets window w's axes exactly as
   Window/Window (graf_par.c user_window, here update_view()) would: the
   client's zoom becomes the core's own, so a PostScript/SVG export,
   Restore and later redraws all agree with it. A range that is not
   finite or not increasing, or a window that does not exist, is refused
   (message error) and changes nothing. */
/* the plot window a command names ("win", 1-based): its index, or -1
   after telling the client it does not exist */
static int command_window(const char *line)
{
    int i = (int)get_num(line, "win", -1) - 1;
    if (i < 0 || i >= MAXPOP || !graph[i].Use) {
        j_err_msg("No such window");
        return -1;
    }
    return i;
}

static void view_command(const char *line)
{
    int i = command_window(line);
    double xlo = get_num(line, "xlo", 0), xhi = get_num(line, "xhi", 0);
    double ylo = get_num(line, "ylo", 0), yhi = get_num(line, "yhi", 0);
    if (i < 0) return;
    if (!isfinite(xlo) || !isfinite(xhi) || !isfinite(ylo) || !isfinite(yhi) || xlo >= xhi || ylo >= yhi) {
        j_err_msg("Bad view");
        return;
    }
    if (i != current_pop) select_graph(i);
    update_view((float)xlo, (float)xhi, (float)ylo, (float)yhi);
}

/* dragging a 3D plot turns it (many_pops.c rotate3dcheck):
   {"cmd":"rotate","what":"down|move|up","x","y"} */
static void rotate_command(const char *line)
{
    static int x0, y0;
    static double theta, phi;
    char what[8];
    int x = (int)get_num(line, "x", 0), y = (int)get_num(line, "y", 0);
    if (!MyGraph->ThreeDFlag) return;
    get_str(line, "what", what, sizeof what);
    if (strcmp(what, "down") == 0) {
        x0 = x;
        y0 = y;
        phi = MyGraph->Phi;
        theta = MyGraph->Theta;
    } else if (strcmp(what, "move") == 0) {
        MyGraph->Phi = phi - (double)(y - y0);
        MyGraph->Theta = theta - (double)(x - x0);
        redraw_cube_pt(MyGraph->Theta, MyGraph->Phi);
    } else if (strcmp(what, "up") == 0) {
        do_axes();
        j_redraw_all();
    }
}

/* a web2 client turns a 3D plot itself (projecting the box with its own
   angles, docs/ui-v2.md T14) and reports where it settled, so the core's
   own state agrees for a PostScript/SVG export, Restore, and any other
   client: {"cmd":"view3d","win":w,"theta":..,"phi":..} sets window w's
   angles exactly, redraws it, and sends state and idle as usual. Simpler
   than replaying `rotate`'s pixel deltas, which only make sense relative
   to a drag the core itself is tracking. A window that is not a 3D plot,
   does not exist, or an angle that is not finite, is refused (message
   error) and changes nothing. */
static void view3d_command(const char *line)
{
    int i = command_window(line);
    double theta = get_num(line, "theta", 0), phi = get_num(line, "phi", 0);
    if (i < 0) return;
    if (!graph[i].ThreeDFlag) {
        j_err_msg("Not a 3D window");
        return;
    }
    if (!isfinite(theta) || !isfinite(phi)) {
        j_err_msg("Bad view");
        return;
    }
    if (i != current_pop) select_graph(i);
    MyGraph->Theta = theta;
    MyGraph->Phi = phi;
    do_axes();
    j_redraw_all();
}

/* the array plot window's buttons */
static void aplot_command(const char *line)
{
    char o[16];
    get_str(line, "op", o, sizeof o);
    if (!aplot.alive) return;
    if (strcmp(o, "redraw") == 0) send_aplot(NULL);
    else if (strcmp(o, "edit") == 0) {
        editaplot(&aplot);
        send_aplot(NULL);
    } else if (strcmp(o, "fit") == 0) fit_aplot();
    else if (strcmp(o, "range") == 0) set_up_aplot_range();
    else if (strcmp(o, "print") == 0) print_aplot(&aplot);
    else if (strcmp(o, "gif") == 0) {
        char file[XPP_MAX_NAME];
        snprintf(file, sizeof file, "%s.gif", this_file);
        if (file_selector(const_cast<char *>("GIF plot"), file, const_cast<char *>("*.gif"))) aplot_gif(file, 1);
    } else if (strcmp(o, "scroll") == 0) {
        /* dragging the plot by dy pixels moves the first row, as in X11 */
        aplot.nstart -= (int)get_num(line, "dy", 0);
        if (aplot.nstart < 0) aplot.nstart = 0;
        send_aplot(NULL);
    } else if (strcmp(o, "close") == 0) {
        aplot.alive = 0;
        send_window("destroy", WIN_APLOT, 0, 0, NULL);
    }
}

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
    auto_reset_state(); /* a fresh canvas starts from the defaults */
    diag_forget();      /* and a new window has no data */
    auto_data_forget(); /* nor an info strip or a stability circle */
    send_window("create", WIN_AUTO, Auto.wid + 12 * DCURXs, Auto.hgt + 4 * DCURYs, wname);
    draw_bif_axes();
}

static void j_auto_line(int a, int b, int c, int d)
{
    char tmp[48];
    int n;
    auto_sync_state(); /* a change here closes the run, which is correct */
    /* the diagram draws each segment as (this point, the previous one), so a
       branch arrives end first: the run continues when the new segment's
       second point is where the last one started. The path is stored in the
       order it was walked, which strokes the same either way. */
    if (poly_open && c == poly_x && d == poly_y) {
        OpBuf *ob = get_op_buf(WIN_AUTO);
        n = snprintf(tmp, sizeof tmp, ",%d,%d", a, b);
        buf_add(&ob->b, tmp, (size_t)n);
        poly_x = a;
        poly_y = b;
        if (ob->b.len > 60000) flush_ops();
        return;
    }
    op(WIN_AUTO, "[\"poly\",%d,%d,%d,%d", c, d, a, b); /* left open to grow */
    poly_open = 1;
    poly_x = a;
    poly_y = b;
}
static void j_auto_text(int a, int b, char *c) { auto_sync_state(); op_text(WIN_AUTO, "rtext", a, b, c, -1); }
static void j_auto_circle(int x, int y, int r) { auto_sync_state(); op(WIN_AUTO, "[\"circle\",%d,%d,%d]", x, y, r); }
static void j_auto_fill_circle(int x, int y, int r) { auto_sync_state(); op(WIN_AUTO, "[\"fcircle\",%d,%d,%d]", x, y, r); }
/* The grab cursor lives on the client's overlay, not in the diagram.
   XORCross toggles: a call where the cursor is shown hides it, a call
   anywhere else shows it there. */
static int auto_cross_shown;
static int auto_cross_x, auto_cross_y;

static void j_auto_xor_cross(int x, int y)
{
    if (DONT_XORCross) return;
    if (auto_cross_shown && x == auto_cross_x && y == auto_cross_y) {
        op(WIN_AUTO, "[\"cursor\"]");
        auto_cross_shown = 0;
        return;
    }
    op(WIN_AUTO, "[\"cursor\",%d,%d]", x, y);
    auto_cross_shown = 1;
    auto_cross_x = x;
    auto_cross_y = y;
}

/* the grab is over: the diagram was never touched, so hiding the cursor
   and putting back the branch marks is all a taken point needs */
static void j_auto_grab_end(int done)
{
    if (auto_cross_shown) {
        op(WIN_AUTO, "[\"cursor\"]");
        auto_cross_shown = 0;
    }
    if (done == 1) RedrawMark();
}
/* For every point the diagram sets the width and the colour, draws one
   segment, then sets the colour back to black. Sending each of those puts
   two state ops between every pair of segments, which is both bandwidth and
   a broken polyline run.

   So the wanted state is only recorded, and goes out just before something
   is actually drawn with it. The colour set back after the last segment
   never reaches the client, and a run of same-coloured points sends the
   colour once. auto_reset_state() is called wherever the client's canvas
   goes back to its own defaults, so the two cannot drift apart. */
static int auto_col_want, auto_lw_want = 1;
static int auto_col_sent, auto_lw_sent = 1;

/* The canvas keeps its colour across a clear, so after one the client is not
   back at the default however much the core would like it to be: the axes
   would be drawn in whatever colour the last branch left behind. Ask for the
   default and mark what the client holds as unknown, so the next thing drawn
   sends the colour rather than assuming it. */
static void auto_reset_state(void)
{
    auto_col_want = 0;
    auto_lw_want = 1;
    auto_col_sent = auto_lw_sent = -1;
    auto_cross_shown = 0; /* a fresh canvas has no cursor on it either */
}

/* emit what a drawing op is about to depend on; a change closes any open
   polyline, which is right: it is no longer the same stroke */
static void auto_sync_state(void)
{
    if (auto_lw_want != auto_lw_sent) {
        auto_lw_sent = auto_lw_want;
        op(WIN_AUTO, "[\"lw\",%d]", auto_lw_sent);
    }
    if (auto_col_want != auto_col_sent) {
        auto_col_sent = auto_col_want;
        op(WIN_AUTO, "[\"color\",%d]", auto_col_sent);
    }
}

static void j_auto_line_width(int wid) { auto_lw_want = wid; }
static void j_auto_col(int col) { auto_col_want = col; }
static void j_auto_bw(void) { auto_col_want = 0; }
static void j_auto_clr_stab(void)
{
    int r = Auto.st_wid / 4;
    /* window 102 only ever shows its latest picture: drop whatever of it is
       still unsent instead of shipping a circle the client immediately
       overwrites */
    op_buf_discard(WIN_AUTO_STAB);
    op(WIN_AUTO_STAB, "[\"clear\"]");
    op(WIN_AUTO_STAB, "[\"circle\",%d,%d,%d]", 2 * r, 2 * r, r);
}
static void j_auto_stab_line(int x, int y, int xp, int yp) { op(WIN_AUTO_STAB, "[\"line\",%d,%d,%d,%d]", x, y, xp, yp); }
static void j_auto_clear_plot(void) { auto_reset_state(); op(WIN_AUTO, "[\"clear\"]"); }
static void j_auto_clear_info(void) { op_buf_discard(WIN_AUTO_INFO); op(WIN_AUTO_INFO, "[\"clear\"]"); }
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
/* a grab answer's key after its point ({"point":i,"key":"Return"}): the
   point first, then the key without asking again */
static int grab_key_after;

static int j_auto_grab_event(int *x, int *y)
{
    Buf b;
    char k[32];
    const char *jp;
    int id;
    if (grab_key_after) {
        id = grab_key_after;
        grab_key_after = 0;
        return id;
    }
    id = ask_begin(&b, "grab");
    buf_printf(&b, ",\"win\":%d", WIN_AUTO);
    if (!ask_wait(&b, id)) return ESC;
    /* a point of the diagram data by its index: the cursor goes to that
       point's entry (docs/protocol.md "Grab by point"); one the data do not
       have, or whose entry AUTO no longer has (the data are an old drawing
       until reDraw), is ignored, and so is its key */
    if ((jp = js_find(answer, "point")) != NULL) {
        double i = js_num(jp, -1);
        const XppDiagPoint *p = i >= 0 && i < dg_client ? &dg[(int)i] : NULL;
        *x = p && diagram_has(p->node, p->ibr, p->pt) ? p->node : -1;
        if (*x >= 0 && get_str(answer, "key", k, sizeof k)) grab_key_after = key_code(k);
        return XPP_AUTO_NODE;
    }
    if (get_str(answer, "key", k, sizeof k)) return key_code(k);
    answer_point(WIN_AUTO, 0, x, y);
    return XPP_AUTO_CLICK;
}
static void j_auto_show_hint(void) { send_simple("message", "auto", Auto.hinttxt); }

/* ---- AUTO diagram data --------------------------------------------------------
   Beside the primitives, the points of the diagram go out as data ("diagram"
   events, docs/protocol.md), so that the client can zoom, pan and show a
   point under the mouse without a round trip. dg[0..dg_n) is the list the
   client holds once the pending events are out: dg_client points of it
   were sent, and from dg_dirty on it changed since (dg_dirty < dg_client
   means the client must first drop its points from dg_dirty on).

   A redraw is a clear (draw_bif_axes) and then every point again, and
   mostly the same points at other axes: a zoom, Fit, a scroll, a resize.
   So after a clear the points are compared with the list (dg_replay,
   dg_match of them agreed so far) and nothing is sent while they agree; if
   all of them agree only the new axes go out. The first point that differs
   drops the rest of the old list and is sent with the ones after it. A
   clear that is not followed by the whole list (the Clear button) drops
   the rest at the end of the command. */
static int dg_replay, dg_match, dg_axes;
static struct {
    double xmin, xmax, ymin, ymax;
    int x0, y0, wid, hgt, plot;
    char xlabel[AUTO_LABEL_LEN], ylabel[AUTO_LABEL_LEN];
} dg_ax;

/* the client has nothing: a new window, or one it no longer holds */
static void diag_forget(void)
{
    dg_n = dg_client = dg_dirty = 0;
    dg_replay = dg_match = dg_axes = 0;
}

static int diag_same(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return a->ibr == b->ibr && a->pt == b->pt && a->itp == b->itp && a->lab == b->lab && a->type == b->type &&
           a->flag2 == b->flag2 && a->draw == b->draw && a->newseg == b->newseg && a->color == b->color &&
           a->lw == b->lw && a->from == b->from && memcmp(&a->x, &b->x, sizeof a->x) == 0 && memcmp(&a->y1, &b->y1, sizeof a->y1) == 0 &&
           memcmp(&a->y2, &b->y2, sizeof a->y2) == 0;
}

/* the replay is over: the list is its first k points */
static void diag_end_replay(int k)
{
    dg_replay = 0;
    dg_n = k;
    if (k < dg_dirty) dg_dirty = k;
}

static void j_auto_diagram(const XppDiagPoint *p)
{
    if (!p) {
        dg_ax.xmin = Auto.xmin;
        dg_ax.xmax = Auto.xmax;
        dg_ax.ymin = Auto.ymin;
        dg_ax.ymax = Auto.ymax;
        dg_ax.x0 = Auto.x0;
        dg_ax.y0 = Auto.y0;
        dg_ax.wid = Auto.wid;
        dg_ax.hgt = Auto.hgt;
        dg_ax.plot = Auto.plot;
        get_auto_str(dg_ax.xlabel, dg_ax.ylabel);
        dg_axes = 1;
        dg_replay = 1;
        dg_match = 0;
        return;
    }
    if (dg_replay) {
        if (dg_match < dg_n && diag_same(&dg[dg_match], p)) {
            dg[dg_match++].node = p->node; /* not in the data: the entry a grab by point goes to */
            return;
        }
        diag_end_replay(dg_match);
    }
    if (dg_n == dg_cap) {
        dg_cap = dg_cap ? 2 * dg_cap : 1024;
        dg = static_cast<XppDiagPoint *>(xpp_realloc(dg, (size_t)dg_cap * sizeof *dg));
    }
    dg[dg_n++] = *p;
}

/* a number, null when it is not finite (JSON has no nan) */
static void buf_num(Buf *b, double v)
{
    if (v != v || v > 1e308 || v < -1e308) BUF_LIT(b, "null");
    else buf_printf(b, "%.7g", v);
}

static void diag_axes(Buf *b)
{
    buf_printf(b, ",\"xmin\":%.17g,\"xmax\":%.17g,\"ymin\":%.17g,\"ymax\":%.17g", dg_ax.xmin, dg_ax.xmax,
               dg_ax.ymin, dg_ax.ymax);
    buf_printf(b, ",\"x0\":%d,\"y0\":%d,\"wid\":%d,\"hgt\":%d,\"plot\":%d,\"xlabel\":", dg_ax.x0, dg_ax.y0,
               dg_ax.wid, dg_ax.hgt, dg_ax.plot);
    buf_str(b, dg_ax.xlabel);
    BUF_LIT(b, ",\"ylabel\":");
    buf_str(b, dg_ax.ylabel);
}

/* points i..j of the list as one run: they share branch, kind and style,
   and their point numbers count up by one */
static void diag_run(Buf *b, int i, int j)
{
    const XppDiagPoint *p = &dg[i];
    int k, two = 0, nlab = 0;
    char sym[4];
    buf_printf(b, "{\"br\":%d,\"pt\":%d,\"ty\":%d,\"d\":%d,\"c\":%d,\"lw\":%d", abs(p->ibr), abs(p->pt), p->type,
               p->draw, p->color, p->lw);
    if (p->flag2) buf_printf(b, ",\"f2\":%d", p->flag2);
    if (p->newseg) BUF_LIT(b, ",\"new\":1");
    if (p->from) buf_printf(b, ",\"from\":%d", p->from);
    BUF_LIT(b, ",\"x\":[");
    for (k = i; k <= j; k++) {
        if (k > i) BUF_LIT(b, ",");
        buf_num(b, dg[k].x);
        if (dg[k].y2 != dg[k].y1) two = 1;
        if (dg[k].lab) nlab++;
    }
    BUF_LIT(b, "],\"y\":[");
    for (k = i; k <= j; k++) {
        if (k > i) BUF_LIT(b, ",");
        buf_num(b, dg[k].y1);
    }
    BUF_LIT(b, "]");
    if (two) {
        BUF_LIT(b, ",\"y2\":[");
        for (k = i; k <= j; k++) {
            if (k > i) BUF_LIT(b, ",");
            buf_num(b, dg[k].y2);
        }
        BUF_LIT(b, "]");
    }
    if (nlab) {
        BUF_LIT(b, ",\"lab\":[");
        for (k = i, nlab = 0; k <= j; k++) {
            const char *t = sym;
            if (!dg[k].lab) continue;
            get_bif_sym(sym, dg[k].itp);
            while (*t == ' ') t++;
            buf_printf(b, "%s[%d,%d,", nlab++ ? "," : "", k - i, dg[k].lab);
            buf_str(b, t);
            BUF_LIT(b, "]");
        }
        BUF_LIT(b, "]");
    }
    BUF_LIT(b, "}");
}

/* the index in the data the client holds of AUTO's diagram entry `node`
   (auto_data.h); the latest when a redraw of other axes left two */
static int diag_point_of_node(int node)
{
    int i;
    for (i = dg_client - 1; i >= 0; i--)
        if (dg[i].node == node) return i;
    return -1;
}

/* b continues a's run */
static int diag_joins(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return !b->newseg && !b->from && abs(a->ibr) == abs(b->ibr) && abs(b->pt) == abs(a->pt) + 1 && a->type == b->type &&
           a->draw == b->draw && a->color == b->color && a->lw == b->lw && a->flag2 == b->flag2;
}

/* send what the client does not have yet. final: the command ends or asks
   something, so a replay that has not been completed never will be */
static void diag_flush(int final)
{
    Buf b = {0};
    if (dg_replay) {
        if (dg_match == dg_n) diag_end_replay(dg_n); /* all agreed; more points are new ones */
        else if (final) diag_end_replay(dg_match);
        else return; /* still replaying: nothing is known yet */
    }
    if (dg_dirty < dg_client) {
        buf_printf(&b, "{\"ev\":\"diagram\",\"op\":\"reset\",\"keep\":%d", dg_dirty);
        diag_axes(&b);
        BUF_LIT(&b, "}");
        out_line(b.s, b.len);
        b.len = 0;
        dg_client = dg_dirty;
        dg_axes = 0;
    } else if (dg_axes) {
        BUF_LIT(&b, "{\"ev\":\"diagram\",\"op\":\"axes\"");
        diag_axes(&b);
        BUF_LIT(&b, "}");
        out_line(b.s, b.len);
        b.len = 0;
        dg_axes = 0;
    }
    /* the points in events of some 60 kB, like the drawing */
    while (dg_client < dg_n) {
        int i = dg_client, j;
        buf_printf(&b, "{\"ev\":\"diagram\",\"op\":\"add\",\"from\":%d,\"runs\":[", dg_client);
        while (i < dg_n && b.len < 60000) {
            for (j = i; j + 1 < dg_n && j - i < 2000 && diag_joins(&dg[j], &dg[j + 1]); j++) {
            }
            if (i > dg_client) BUF_LIT(&b, ",");
            diag_run(&b, i, j);
            i = j + 1;
        }
        BUF_LIT(&b, "]}");
        out_line(b.s, b.len);
        b.len = 0;
        dg_client = i;
    }
    dg_dirty = dg_n;
    xpp_free(b.s);
}

/* AUTO's refreshdisplay() after every point: a few frames a second, not a
   flush per point. The end of a command and every ask flush in full, so
   the last point of a run and a grab's circle are never held back. */
static void j_auto_refresh(void)
{
    static double last;
    if (xpp_every(&last, 0.05)) {
        json_flush();
        auto_data_update(0);
    }
}

/* ---- animation window ------------------------------------------------------------ */

/* the animation window's state for its slider and toggles */
static void j_ani_slider(void)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"ani\",\"pos\":%d,\"rows\":%d,\"fly\":%d,\"grab\":%d,\"skip\":%d,\"speed\":%d,"
               "\"loaded\":%d,\"open\":%d}",
               vcr.pos, my_browser.maxrow, animation_on_the_fly, ani_grab_flag, vcr.inc, ani_speed, n_anicom > 0,
               vcr.iexist);
    send_buf(&b);
    xpp_free(b.s);
}

/* between frames of Go: 1 when Pause, ABORT or Esc stops the playback */
static int ani_wait(int ms)
{
    struct timeval start, now;
    gettimeofday(&start, NULL);
    flush_ops();
    out_flush();
    for (;;) {
        char *line;
        long left;
        int r;
        if (xpp_job_cancelled()) return 1;
        gettimeofday(&now, NULL);
        left = ms - ((now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000);
        line = read_line(XPP_INBOX_CONTROL, left > 0 ? (int)left : 0);
        if (!line) return 0;
        r = control_line(line);
        if (r == ESC || r == ANI_PAUSE) return 1;
    }
}

/* the Go button (aniwin.c ani_flip without the X pixmap) */
static void ani_go(void)
{
    double y[MAXODE];
    float **ss = my_browser.data;
    char file[160];
    FILE *gif = NULL;
    int i, stop = 0, frame = 0, written = 0, w, h;
    if (n_anicom == 0 || my_browser.maxrow < 2) return;
    set_ani_perm();
    if (mpeg.aviflag == 1) {
        gif = fopen("anim.gif", "wb");
        set_global_map(1);
    }
    while (!stop) {
        int row = vcr.pos, ppm = mpeg.flag > 0 && frame % (mpeg.skip > 0 ? mpeg.skip : 1) == 0;
        for (i = 0; i < NODE + NMarkov; i++) y[i] = ss[i + 1][row];
        set_fix_rhs((double)ss[0][row], y);
        xpp_ui.ani_clear();
        render_ani();
        xpp_ui.ani_show();
        if (ppm || gif) {
            unsigned char *rgb = ask_pixels(WIN_ANI, -1, &w, &h);
            if (!rgb) break;
            if (ppm) {
                snprintf(file, sizeof file, "%s_%d.ppm", mpeg.root, written++);
                write_ppm(file, rgb, w, h);
            }
            if (gif) {
                web_safe_colors(rgb, w, h);
                gif_stuff_ppm(rgb, w, h, gif, frame == 0 ? FIRST_ANI_GIF : NEXT_ANI_GIF);
            }
            xpp_free(rgb);
        }
        frame++;
        stop = ani_wait(ani_speed * (mpeg.aviflag == 1 || mpeg.flag > 0 ? 6 : 1));
        vcr.pos += vcr.inc;
        if (vcr.pos >= my_browser.maxrow) {
            stop = 1;
            vcr.pos = 0;
            reset_comets();
        }
    }
    mpeg.flag = 0;
    if (gif) {
        end_ani_gif(gif);
        fclose(gif);
        set_global_map(0);
    }
    j_ani_slider();
}

/* a size the client asked for, applied when the running command ends */
static int ani_size_w, ani_size_h;

static void apply_ani_size(void)
{
    int w = 4 * (ani_size_w / 4), h = 5 * (ani_size_h / 5);
    if (!ani_size_w || !vcr.iexist) return;
    ani_size_w = ani_size_h = 0;
    if (w < 40 || h < 40 || (w == vcr.wid && h == vcr.hgt)) return;
    vcr.wid = w;
    vcr.hgt = h;
    send_window("create", WIN_ANI, w, h, "Animation");
    if (n_anicom) ani_flip1(0);
}

static void ani_command(const char *line)
{
    char o[16], what[8];
    int x = (int)get_num(line, "x", 0), yy = (int)get_num(line, "y", 0);
    /* a point in the animation's unit coordinates (u, v: y up, as the ani
       frame event's) instead of pixels: the nearest pixel of the window */
    if (js_find(line, "u") && js_find(line, "v")) {
        x = (int)floor(get_num(line, "u", 0) * vcr.wid + 0.5);
        yy = (int)floor((1 - get_num(line, "v", 0)) * vcr.hgt + 0.5);
    }
    get_str(line, "op", o, sizeof o);
    if (ani_speed_op(o, line)) {
        j_ani_slider();
        return;
    }
    if (strcmp(o, "step") == 0) ani_flip1((int)get_num(line, "n", 1));
    else if (strcmp(o, "reset") == 0) ani_reset();
    else if (strcmp(o, "file") == 0) {
        /* a new animation shows its first frame at once when there is data */
        if (get_ani_file(NULL) && my_browser.maxrow >= 2) ani_reset();
    } else if (strcmp(o, "go") == 0) ani_go();
    else if (strcmp(o, "skip") == 0) ani_newskip();
    else if (strcmp(o, "mpeg") == 0) ani_create_mpeg();
    else if (strcmp(o, "fly") == 0) animation_on_the_fly = 1 - animation_on_the_fly;
    else if (strcmp(o, "grab") == 0) ani_grab_start();
    else if (strcmp(o, "seek") == 0 && my_browser.maxrow >= 2) {
        vcr.pos = 0;
        ani_flip1(0);
        ani_flip1((int)get_num(line, "pos", 0));
    } else if (strcmp(o, "mouse") == 0 && ani_grab_flag) {
        /* dragging a grab point: down, move..., up (which may integrate) */
        get_str(line, "what", what, sizeof what);
        if (strcmp(what, "down") == 0) ani_grab_mouse(1, x, yy);
        else if (strcmp(what, "move") == 0) update_ani_motion_stuff(x, yy);
        else if (strcmp(what, "up") == 0) ani_grab_mouse(0, x, yy);
    } else if (strcmp(o, "close") == 0 && vcr.iexist) {
        vcr.iexist = 0;
        ani_grab_flag = 0;
        send_window("destroy", WIN_ANI, 0, 0, NULL);
    }
    j_ani_slider();
}

static void j_new_vcr(void)
{
    /* already open: say so (a client that reconnected has not seen it made) */
    if (vcr.iexist == 1) {
        j_ani_slider();
        return;
    }
    vcr.wid = 280;
    vcr.hgt = 350;
    vcr.iexist = 1;
    send_window("create", WIN_ANI, vcr.wid, vcr.hgt, "Animation");
    ani_view_created();
}
static void j_ani_clear(void) { op(WIN_ANI, "[\"clear\"]"); }
static void j_ani_show(void) { flush_ops(); out_flush(); }
static void j_ani_color(int icol) { op(WIN_ANI, "[\"color\",%d]", icol); }
static void j_ani_thick(int t) { op(WIN_ANI, "[\"lw\",%d]", t); }
static void j_ani_font(int size, int font, int color) { op(WIN_ANI, "[\"font\",%d,%d,%d]", size, font, color); }
static void j_ani_line(int x1, int y1, int x2, int y2) { op(WIN_ANI, "[\"line\",%d,%d,%d,%d]", x1, y1, x2, y2); }
static void j_ani_rect(int x, int y, int w, int h, int fill) { op(WIN_ANI, "[\"%s\",%d,%d,%d,%d]", fill ? "frect" : "rect", x, y, w, h); }
static void j_ani_arc(int x, int y, int w, int h, int fill) { op(WIN_ANI, "[\"%s\",%d,%d,%d,%d]", fill ? "fellipse" : "ellipse", x, y, w, h); }
static void j_ani_text(int x, int y, char *s) { op_text(WIN_ANI, "rtext", x, y, s, -1); }

/* ---- misc ------------------------------------------------------------------------ */

/* the last equilibrium shown, for its Import button */
static double last_eq[MAXODE];
static int last_eq_n;

static void j_show_eq_box(int cp, int cm, int rp, int rm, int im, double *y, double *ev, int n)
{
    Buf b = {0};
    int i;
    redraw_ics();
    for (i = 0; i < n && i < MAXODE; i++) last_eq[i] = y[i];
    last_eq_n = n < MAXODE ? n : MAXODE;
    buf_printf(&b, "{\"ev\":\"equilibrium\",\"type\":\"%s\",\"cplus\":%d,\"cminus\":%d,"
               "\"im\":%d,\"rplus\":%d,\"rminus\":%d,\"values\":[",
               eq_stability(cp, rp, im), cp, cm, im, rp, rm);
    for (i = 0; i < n; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, uvar_names[i]);
        buf_printf(&b, ",%.16g]", y[i]);
    }
    BUF_LIT(&b, "]");
    if (ev) { /* the Jacobian's eigenvalues, (re, im) pairs (gear.c eigen) */
        BUF_LIT(&b, ",\"eigenvalues\":[");
        for (i = 0; i < n; i++) buf_printf(&b, "%s[%.16g,%.16g]", i ? "," : "", ev[2 * i], ev[2 * i + 1]);
        BUF_LIT(&b, "]");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

static void j_make_txtview(void)
{
    extern char *save_eqn[];
    extern int NLINES;
    Buf b = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"source\",\"lines\":");
    buf_str_array(&b, save_eqn, NLINES);
    /* comments; one with an action runs it when picked ({"cmd":"action"}) */
    BUF_LIT(&b, ",\"comments\":[");
    for (i = 0; i < n_comments; i++) {
        if (i) BUF_LIT(&b, ",");
        BUF_LIT(&b, "[");
        buf_str(&b, comments[i].text);
        buf_printf(&b, ",%d]", comments[i].aflag > 0);
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
}

static void j_q_calc(void)
{
    char expr[256] = "";
    double z;
    char result[300] = "Formula:";
    /* the X11 calculator shows the answer in its window: here in the prompt */
    while (new_string(result, expr)) {
        if (do_calc(expr, &z) != -1) {
            snprintf(result, sizeof result, "%.200s = %.16g   Formula:", expr, z);
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

/* the JSON front end's table: assignments, so C++17 needs no designated
   initializers; fields not set stay null, as in the C initializer */
static XppUi make_json_ui(void)
{
    XppUi u{};
    u.err_msg = [](char *m) { j_err_msg(m); };
    u.ping = j_ping;
    u.bottom_msg = j_bottom_msg;
    u.message_box = [](char *m) { j_message_box(m); };
    u.kill_message_box = j_kill_message_box;
    u.title_text = j_title_text;
    u.canvas_xy = j_canvas_xy;
    u.new_string = j_new_string;
    u.yes_no_box = j_yes_no_box;
    u.two_choice = j_two_choice;
    u.respond_box = [](char *b, char *m) { j_respond_box(b, m); };
    u.checklist = j_checklist;
    u.string_box = j_string_box;
    u.file_selector = j_file_selector;
    u.dialog = [](char *t, char *n, char *v, char *o, char *c, int m) { return j_dialog(t, n, v, o, c, m); };
    u.edit_box = j_edit_box;
    u.get_mouse_xy = j_get_mouse_xy;
    u.menu_flash = j_int;
    u.show_menu = j_show_menu;
    u.redraw_menu = j_void;
    u.menu_choose = j_menu_choose;
    u.check_abort = j_check_abort;
    u.progress_begin = j_progress_begin;
    u.progress = j_progress;
    u.flush = json_flush;
    u.redraw_params = j_state_dirty;
    u.param_box_set = j_state_dirty_is;
    u.param_box_redraw = j_state_dirty_i;
    u.ic_box_set = j_state_dirty_is;
    u.ic_box_redraw = j_state_dirty_i;
    u.redraw_ics = j_state_dirty;
    u.redraw_all = j_redraw_all;
    u.redraw_bcs = j_state_dirty;
    u.redraw_delays = j_state_dirty;
    u.redraw_graph = j_redraw_graph;
    u.redraw_screens = j_redraw_screens;
    u.clear_screens = j_clear_screens;
    u.clear_draw_window = clr_scrn;
    u.reset_graphics = j_reset_graphics;
    u.data_changed = j_browser_changed;
    u.rows_stored = plot_data_rows_stored;
    u.browser_redraw = j_browser_changed;
    u.activate_graph = j_activate_graph;
    u.create_plot_window = j_create_plot_window;
    u.destroy_plot_window = j_destroy_plot_window;
    u.kill_plot_windows = j_kill_plot_windows;
    u.lower_plot_window = j_void;
    u.gr_col = j_void;
    u.base_col = j_void;
    u.cput_text = j_cput_text;
    u.get_draw_size = j_get_draw_size;
    u.draw_freeze = j_draw_freeze;
    u.blank_draw_window = j_blank_draw_window;
    u.put_text = j_put_text;
    u.small_base = j_void;
    u.small_gr = j_void;
    u.film_clip = j_film_clip;
    u.reset_film = j_reset_film;
    u.movie_play_back = j_movie_play_back;
    u.movie_auto_play = j_movie_auto_play;
    u.movie_save = j_movie_save;
    u.movie_make_anigif = j_movie_make_anigif;
    u.rubber_band = j_rubber_band;
    u.scroll_window = j_scroll_window;
    u.new_colormap = j_new_colormap;
    u.draw_point = j_draw_point;
    u.draw_line = j_draw_line;
    u.draw_bead = j_draw_bead;
    u.draw_frect = j_draw_frect;
    u.draw_text = j_draw_text;
    u.draw_special_text = j_draw_special_text;
    u.draw_linestyle = j_draw_linestyle;
    u.set_color = j_set_color;
    u.aplot_make = j_aplot_make;
    u.aplot_redraw = j_aplot_redraw;
    u.aplot_reset_axes = j_aplot_redraw;
    u.aplot_draw_one = j_aplot_draw_one;
    u.auto_make_window = j_auto_make_window;
    u.auto_line = j_auto_line;
    u.auto_text = j_auto_text;
    u.auto_circle = j_auto_circle;
    u.auto_fill_circle = j_auto_fill_circle;
    u.auto_xor_cross = j_auto_xor_cross;
    u.auto_line_width = j_auto_line_width;
    u.auto_col = j_auto_col;
    u.auto_bw = j_auto_bw;
    u.auto_clr_stab = j_auto_clr_stab;
    u.auto_stab_line = j_auto_stab_line;
    u.auto_clear_plot = j_auto_clear_plot;
    u.auto_redraw_menus = j_void;
    u.auto_clear_info = j_auto_clear_info;
    u.auto_draw_info = j_auto_draw_info;
    u.auto_refresh = j_auto_refresh;
    u.auto_check_abort = j_auto_check_abort;
    u.auto_rubber = j_auto_rubber;
    u.auto_choose_key = j_auto_choose_key;
    u.auto_scroll_window = j_auto_scroll_window;
    u.auto_grab_event = j_auto_grab_event;
    u.auto_show_hint = j_auto_show_hint;
    u.auto_grab_end = j_auto_grab_end;
    u.auto_diagram = j_auto_diagram;
    u.new_vcr = j_new_vcr;
    u.ani_clear = j_ani_clear;
    u.ani_show = j_ani_show;
    u.ani_color = j_ani_color;
    u.ani_thick = j_ani_thick;
    u.ani_font = j_ani_font;
    u.ani_line = j_ani_line;
    u.ani_rect = j_ani_rect;
    u.ani_arc = j_ani_arc;
    u.ani_text = j_ani_text;
    u.ani_slider = j_ani_slider;
    u.init_txtview = j_void;
    u.show_eq_box = j_show_eq_box;
    u.make_txtview = j_make_txtview;
    u.q_calc = j_q_calc;
    u.exit_program = j_exit_program;
    return u;
}

static const XppUi json_ui = make_json_ui();

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
    auto_reset_state();
    send_window("create", WIN_AUTO, w, h, "It's AUTO man!");
    redraw_diagram();
}

static void apply_size(const char *line)
{
    int win = (int)get_num(line, "win", 1);
    int w = (int)get_num(line, "w", 640), h = (int)get_num(line, "h", 480);
    int i = win - 1;
    if (win == WIN_ANI) {
        ani_size_w = w;
        ani_size_h = h;
        return;
    }
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

/* {"cmd":"set","kind":"par|ic|bc|delay","name":...,"value":number or "text":...}
   Text is what the user would type in the X11 box: a number or %formula for
   parameters and ICs, an expression for BCs and delays. */
static void apply_set(const char *line)
{
    char kind[16], name[NAME_IN], text[256];
    double z;
    int type, i, n, index = -1;
    get_str(line, "kind", kind, sizeof kind);
    get_str(line, "name", name, sizeof name);
    if (!get_str(line, "text", text, sizeof text))
        snprintf(text, sizeof text, "%.16g", get_num(line, "value", 0));
    if (strcmp(kind, "par") == 0) type = 1;       /* PARAMBOX */
    else if (strcmp(kind, "ic") == 0) type = 2;   /* ICBOX */
    else if (strcmp(kind, "delay") == 0) type = 3; /* DELAYBOX */
    else if (strcmp(kind, "bc") == 0) type = 4;   /* BCBOX */
    else return;
    n = type == 1 ? NUPAR : type == 2 ? NODE + NMarkov : NODE;
    /* BC names are not unique ("0="): those come by index */
    index = (int)get_num(line, "index", -1);
    if (index >= n) index = -1;
    for (i = 0; index < 0 && i < n; i++) {
        const char *s = type == 1 ? upar_names[i] : type == 4 ? my_bc[i].name : uvar_names[i];
        if (s && strcasecmp(s, name) == 0) index = i;
    }
    state_dirty = 1;
    if (index < 0) return;
    if (box_set_value(type, index, text, &z) == -1) {
        j_err_msg("Bad formula");
        return;
    }
    box_values_loaded(type);
}

/* {"ev":"stopped","at":AT}: where the running job was when it was
   cancelled (docs/protocol.md "stopped"), from what the computation
   reported last (xpp_job.h). A script replays the interruption from AT. */
static void send_stopped(void)
{
    XppJobProgress p = xpp_job_progress();
    Buf b = {0};
    BUF_LIT(&b, "{\"ev\":\"stopped\",\"at\":");
    if (p.what == XPP_JOB_INTEGRATE) {
        /* t is a stored single-precision number: 9 digits read back exactly */
        buf_printf(&b, "{\"what\":\"integrate\",\"rows\":%ld,\"t\":", p.rows);
        if (isfinite(p.t)) buf_printf(&b, "%.9g}", p.t);
        else BUF_LIT(&b, "null}");
    } else if (p.what == XPP_JOB_AUTO) {
        buf_printf(&b, "{\"what\":\"auto\",\"branch\":%d,\"point\":%d}", p.branch, p.point);
    } else {
        BUF_LIT(&b, "{\"what\":\"other\"}");
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

/* A recorded interruption (docs/protocol.md "Scripts"): the line after the
   one a script is about to run is {"cmd":"abort","at":AT}. That line is
   dropped, and the job of the line about to run (the running job, for an
   answer) cancels itself at AT (xpp_job_stop_at_rows/point). stop_line and
   stop_at say what was armed, for script_stop_missed(). */
static int stop_line;
static char stop_at[400];

static void script_arm_stop(void)
{
    int no = 0;
    const char *next = xpp_inbox_script_peek(&no), *at, *end;
    char what[16];
    if (!next || !is_cmd(next, "abort") || !(at = js_find(next, "at")) || *at != '{') return;
    end = skip_value(at);
    snprintf(stop_at, sizeof stop_at, "%.*s", (int)(end - at), at);
    stop_line = no;
    get_str(at, "what", what, sizeof what);
    if (strcmp(what, "integrate") == 0)
        xpp_job_stop_at_rows((long)get_num(at, "rows", -1));
    else if (strcmp(what, "auto") == 0)
        xpp_job_stop_at_point((int)get_num(at, "branch", -1), (int)get_num(at, "point", -1));
    /* an interruption of anything else cannot be placed: the job runs on */
    xpp_inbox_script_skip();
}

/* the script's next line to the core (docs/protocol.md "Scripts"), and
   the interruption recorded after it */
static void script_next(void)
{
    xpp_inbox_script_advance();
    script_arm_stop();
}

/* the job ends with its recorded interruption still armed */
static void script_stop_missed(void)
{
    xpp_log(XPP_LOG_ERROR, "xppautX: script line %d: the recorded interruption at %s was never reached\n", stop_line,
            stop_at);
    exit(1);
}

/* one command, run as a job (xpp_job.h) numbered by its line's sequence
   number: an abort cancels it from the reader thread */
static void handle_line(const char *line, unsigned long seq)
{
    char k[32];
    xpp_job_begin(seq);
    if (handle_async(line)) {
    } else if (is_cmd(line, "key")) {
        get_str(line, "key", k, sizeof k);
        commander(key_code(k));
    } else if (is_cmd(line, "set")) {
        apply_set(line);
    } else if (is_cmd(line, "default")) {
        char kind[16];
        get_str(line, "kind", kind, sizeof kind);
        if (strcmp(kind, "par") == 0) set_default_params();
        else set_default_ics();
    } else if (is_cmd(line, "slide")) {
        /* a parameter slider moved: {"cmd":"slide","name":...,"value":v,"rerun":1} */
        char name[NAME_IN];
        int type, index;
        get_str(line, "name", name, sizeof name);
        if (find_par_or_var(name, &type, &index)) {
            set_par_or_var(name, type, index, get_num(line, "value", 0));
            state_dirty = 1;
            if (get_num(line, "rerun", 1)) slider_rerun();
        }
    } else if (is_cmd(line, "userbut")) {
        int i = (int)get_num(line, "index", -1);
        if (i >= 0 && i < nuserbut) run_the_commands(userbut[i].com);
    } else if (is_cmd(line, "browser")) {
        browser_command(line);
    } else if (is_cmd(line, "aplot")) {
        aplot_command(line);
    } else if (is_cmd(line, "view")) {
        view_command(line);
    } else if (is_cmd(line, "rotate")) {
        rotate_command(line);
    } else if (is_cmd(line, "view3d")) {
        view3d_command(line);
    } else if (is_cmd(line, "plotvars")) {
        plotvars_command(line);
    } else if (is_cmd(line, "eqimport")) {
        if (last_eq_n) eq_import(last_eq, last_eq_n);
    } else if (is_cmd(line, "answer")) {
        /* reaching the main dispatch (rather than ask_wait) means no ask
           was pending for it (docs/protocol.md "Scripts") */
        if (script_mode) script_fail("answers a question that was never asked", line, NULL);
    } else if (is_cmd(line, "data")) {
        data_command(line);
    } else if (is_cmd(line, "equations")) {
        send_equations();
    } else if (is_cmd(line, "action")) {
        int i = (int)get_num(line, "index", -1);
        if (i >= 0 && i < n_comments && comments[i].aflag > 0) do_txt_action(comments[i].action);
    } else if (is_cmd(line, "click")) {
        int win = (int)get_num(line, "win", 1) - 1;
        if (win >= 0 && win < MAXPOP && graph[win].Use && current_pop != win) select_graph(win);
    } else if (is_cmd(line, "redraw")) {
        j_redraw_graph();
        if (Auto.exist) { /* a reconnected client has a blank one, and no data */
            dg_dirty = 0;
            dg_client = dg_n > 0 ? dg_n : 1; /* so the data starts with a reset */
            redraw_diagram();
        }
    } else if (is_cmd(line, "ani")) {
        ani_command(line);
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
        else if (strcmp(o, "point") == 0 && Auto.exist) {
            /* in the diagram's quantities, or a pixel of window 101 */
            const char *jx = js_find(line, "xd"), *jy = js_find(line, "yd");
            if (jx && jy) auto_point_xy(js_num(jx, 0), js_num(jy, 0));
            else auto_motion_xy((int)get_num(line, "x", 0), (int)get_num(line, "y", 0));
        }
        else if (strcmp(o, "close") == 0 && Auto.exist) {
            Auto.exist = 0; /* auto_x11.c auto_kill; File/Auto opens it again */
            send_window("destroy", WIN_AUTO, 0, 0, NULL);
            diag_forget();
            auto_data_forget();
        }
    } else if (is_cmd(line, "session")) {
        char o[8], name[XPP_MAX_NAME];
        get_str(line, "op", o, sizeof o);
        get_str(line, "name", name, sizeof name);
        if (strcmp(o, "save") == 0) xpp_session_save(name[0] ? name : NULL);
        else if (strcmp(o, "load") == 0) xpp_session_load(name[0] ? name : NULL);
    } else if (is_cmd(line, "file")) {
        /* the model's folder for a client that cannot reach it (xpp_files.h) */
        char o[8];
        get_str(line, "op", o, sizeof o);
        xpp_files_command(o, js_find(line, "name"), js_find(line, "data"), data_emit);
    }
    apply_auto_size();
    apply_ani_size();
    if (aplot_dirty && aplot.alive && plot3d_auto_redraw == 1) send_aplot(NULL);
    aplot_dirty = 0;
    if (browser_dirty && br_count) send_browser();
    plot_data_update();
    phase_data_update();
    marks_data_update();
    ani_data_update();
    diag_flush(1);
    auto_data_update(1);
    json_flush();
    /* a cancelled job says where it stopped; a replayed one must have
       stopped where the recorded session did */
    if (xpp_job_cancelled()) send_stopped();
    if (script_mode && xpp_job_stop_armed()) script_stop_missed();
    /* the command is finished; the client may send the next one */
    xpp_job_end();
    send_state();
    send_simple("idle", NULL, NULL);
    /* a script's next line is the next command (docs/protocol.md
       "Scripts"); this also releases the very first script line, since
       xppautx_main.c's startup "redraw" ends here too */
    if (script_mode) script_next();
}

void json_ui_handle(const char *line) { handle_line(line, 0); }

void json_ui_loop(void)
{
    char *copy = NULL;
    size_t cap = 0;
    for (;;) {
        /* a copy: the command's own prompts read further lines */
        char *line = read_line(XPP_INBOX_ARRIVAL, -1);
        unsigned long seq = line_seq;
        size_t n = strlen(line) + 1;
        if (n > cap) {
            cap = n;
            copy = static_cast<char *>(xpp_realloc(copy, cap));
        }
        memcpy(copy, line, n);
        /* an abort did its work when it arrived (classify()): it is no
           command of its own, and gets no state or idle; in a script,
           where nothing ran for it to stop, the next line follows */
        if (!is_cmd(copy, "abort")) handle_line(copy, seq);
        else if (script_mode) script_next();
    }
}

int json_ui_set_script(const char *path)
{
    if (!xpp_inbox_start_file(path)) return 0;
    script_mode = 1;
    return 1;
}

void json_ui_install(void)
{
    int i;
    if (!xpp_http_active()) { /* browser mode has taken stdout and stderr */
        int fd = dup(1);
#ifdef _WIN32
        xpp_binary_mode(fd); /* "\n" line ends, not "\r\n" */
        xpp_binary_mode(0);
#endif
        proto = fdopen(fd, "w");
        dup2(2, 1);
        /* commands from stdin, read on a thread of their own; a script's
           file (json_ui_set_script(), called before this) is read by the
           core thread itself instead, so no reader thread for it here */
        if (!script_mode && !xpp_inbox_start_stdin()) {
            xpp_log(XPP_LOG_ERROR, "xppautX: cannot start the input thread\n");
            exit(1);
        }
    }
    for (i = 0; i < MAXPOP; i++) {
        win_w[i] = 640;
        win_h[i] = 480;
    }
    plot_data_init(data_emit);
    phase_data_init(data_emit);
    marks_data_init(data_emit);
    ani_data_init(data_emit);
    auto_data_init(data_emit, diag_point_of_node);
    xpp_inbox_set_classifier(classify);
    xpp_set_ui(&json_ui);
}

/* the first events a client sees */
void json_ui_hello(char *title)
{
    Buf b = {0};
    int i;
    BUF_LIT(&b, "{\"ev\":\"hello\",\"protocol\":1,\"features\":[\"series\",\"plots\",\"nullclines\",\"dfield\",\"marks\",\"ani\",\"autoinfo\"],\"title\":");
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
    BUF_LIT(&b, "}");
    /* the lists a form field *n picks from (pop_list.c make_scrbox_lists) */
    BUF_LIT(&b, ",\"lists\":[[\"T\"");
    for (i = 0; i < NEQ; i++) {
        BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NODE + NMarkov; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, uvar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NUPAR; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, upar_names[i]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < NODE + NMarkov + NUPAR; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, i < NODE + NMarkov ? uvar_names[i] : upar_names[i - NODE - NMarkov]);
    }
    BUF_LIT(&b, "],[");
    for (i = 0; i < 11; i++) {
        char item[40];
        snprintf(item, sizeof item, "%d %s", i, color_names[i]);
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, item);
    }
    BUF_LIT(&b, "],[\"2 Box\",\"3 Diamond\",\"4 Triangle\",\"5 Plus\",\"6 X\",\"7 Circle\"],"
                "[\"0 Discrete\",\"1 Euler\",\"2 Mod. Euler\",\"3 Runge-Kutta\",\"4 Adams\",\"5 Gear\","
                "\"6 Volterra\",\"7 BackEul\",\"8 QualRK\",\"9 Stiff\",\"10 CVode\",\"11 DoPri5\","
                "\"12 DoPri8(3)\",\"13 Rosenbrock\",\"14 Symplectic\"]]");
    BUF_LIT(&b, ",\"auto_hints\":");
    buf_str_array(&b, auto_hint, 9);
    /* @ button name:keys lines of the ODE file ({"cmd":"userbut","index":i}) */
    BUF_LIT(&b, ",\"userbuttons\":[");
    for (i = 0; i < nuserbut; i++) {
        if (i) BUF_LIT(&b, ",");
        buf_str(&b, userbut[i].bname);
    }
    /* @ slider1=name,slider1lo=...: the parameter sliders set in the file */
    BUF_LIT(&b, "],\"sliders\":[");
    {
        int set[3] = {!notAlreadySet.SLIDER1, !notAlreadySet.SLIDER2, !notAlreadySet.SLIDER3};
        char *var[3] = {SLIDER1VAR, SLIDER2VAR, SLIDER3VAR};
        double lo[3] = {SLIDER1LO, SLIDER2LO, SLIDER3LO}, hi[3] = {SLIDER1HI, SLIDER2HI, SLIDER3HI};
        int k = 0;
        for (i = 0; i < 3; i++) {
            if (!set[i]) continue;
            if (k++) BUF_LIT(&b, ",");
            BUF_LIT(&b, "{\"name\":");
            buf_str(&b, var[i]);
            buf_printf(&b, ",\"lo\":%.16g,\"hi\":%.16g}", lo[i], hi[i]);
        }
    }
    BUF_LIT(&b, "]}");
    send_buf(&b);
    xpp_free(b.s);
    send_palette();
    send_window("create", 1, win_w[0], win_h[0], title);
    send_state();
}
