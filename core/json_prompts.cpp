/* Prompts: every question the core asks the client (a menu, a string box,
   a form, a file name, a mouse pick, a drag, ...) is an "ask" event with an
   id; the core blocks until the matching {"cmd":"answer","id":N,...}
   arrives (docs/protocol.md). Also the messages, and what a long
   computation does between steps (Esc, progress). */
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "xpp_globals.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "auto_nox.h"
#include "read_dir.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "xpp_files.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "many_pops.h"

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern char cur_dir[];
extern BIFUR Auto;
extern int DLeft, DRight, DTop, DBottom;
}

namespace xpp::json {

namespace {

#define MAX_LEN_EBOX 86 /* edit_rhs.h */

int ask_id;
int ask_user; /* the open ask is the user's to answer, not the client's (pixels) */
char *answer;
size_t answer_cap;

char script_ask[400]; /* the open question, for script_fail() */

} // namespace

/* b holds {"ev":"ask","id":N,"kind":... without the closing brace; send
   it and wait for the answer, which is left in answer[]. Returns 1 when the
   answer says ok (or has no ok member), 0 when cancelled. */
int ask_wait(Buf *b, int id)
{
    BUF_LIT(b, "}");
    diag_flush(1);
    auto_data_update(1);
    auto_settings_update();
    json_flush();
    if (session.script_mode) snprintf(script_ask, sizeof script_ask, "%s", b->s);
    send_buf(b);
    xpp_free(b->s);
    /* a script's next line is its answer to this ask (json_io.cpp read_line()'s
       "Which queue" comment, and docs/protocol.md "Scripts") */
    if (session.script_mode) script_next();
    for (;;) {
        char *line = read_line(XPP_INBOX_ANY, -1);
        int lid;
        if (handle_async(line)) {
            flush_pending();
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
            if (ask_user) xpp_job_resume(read_line_seq());
            ok = js_find(answer, "ok");
            return ok == NULL || js_num(ok, 0) != 0;
        }
        /* AUTO's settings sent while a question is open are the page's
           own forms, not an answer: they apply when the command is done
           (docs/protocol.md "AUTO's settings as data") */
        if (!session.script_mode && is_auto_set(line)) {
            defer_auto_set(line);
            continue;
        }
        /* anything else (keys typed at the plot while a dialog is up) is
           dropped, as the X11 dialogs do; for a script this line was
           supposed to answer this ask, and nothing after it can line up */
        if (session.script_mode) script_fail("does not answer the open question", line, script_ask);
    }
}

int ask_begin(Buf *b, const char *kind)
{
    b->s = NULL;
    b->len = b->cap = 0;
    ask_id++;
    ask_user = strcmp(kind, "pixels") != 0;
    buf_printf(b, "{\"ev\":\"ask\",\"id\":%d,\"kind\":\"%s\"", ask_id, kind);
    return ask_id;
}

void j_err_msg(const char *msg)
{
    /* a script that provokes an error fails the run (docs/protocol.md) */
    if (session.script_mode) session.script_error = 1;
    send_simple("message", "error", msg);
}

void j_ping(void) { send_simple("ping", NULL, NULL); }

void j_bottom_msg(int line, char *msg)
{
    (void)line;
    send_simple("message", "bottom", msg);
}

void j_message_box(const char *msg) { send_simple("message", "box", msg); }
void j_kill_message_box(void) { send_simple("message", "box", ""); }
void j_title_text(char *s) { send_simple("title", "text", s); }
void j_canvas_xy(char *s) { send_simple("message", "xy", s); }

namespace {

/* a field's kind as the protocol names it (docs/protocol.md "Asks", `kinds`) */
void buf_kind(Buf *b, int kind)
{
    static const char *const names[] = {"text", "integer", "number", "formula", "expression", "file"};
    if (kind >= XPP_FIELD_NAME) buf_printf(b, "\"name:%d\"", kind - XPP_FIELD_NAME);
    else buf_str(b, kind > 0 && kind < (int)(sizeof names / sizeof *names) ? names[kind] : "text");
}

/* `kinds`, one per field: kinds[i], or `all` for each when kinds is NULL */
void buf_kinds(Buf *b, const int *kinds, int all, int n)
{
    BUF_LIT(b, ",\"kinds\":[");
    for (int i = 0; i < n; i++) {
        if (i) BUF_LIT(b, ",");
        buf_kind(b, kinds ? kinds[i] : all);
    }
    BUF_LIT(b, "]");
}

} // namespace

int j_dialog(const char *title, const char *name, char *value, const char *ok, const char *cancel, int max, int kind)
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
    buf_kinds(&b, &kind, kind, 1);
    if (!ask_wait(&b, id)) return 0;
    get_str(answer, "value", value, max + 1);
    return 1;
}

int j_new_string(char *name, char *value, int kind)
{
    /* the X11 prompt edits a 256-byte line in place */
    return j_dialog("", name, value, "Ok", "Cancel", 255, kind);
}

int j_yes_no_box(void)
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

int j_two_choice(char *c1, char *c2, char *q, char *key, char *title)
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

void j_respond_box(const char *button, const char *message)
{
    Buf b;
    int id = ask_begin(&b, "alert");
    BUF_LIT(&b, ",\"button\":");
    buf_str(&b, button);
    BUF_LIT(&b, ",\"message\":");
    buf_str(&b, message);
    ask_wait(&b, id);
}

int j_checklist(char *title, char **names, int *flags, int n)
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

const char *ask_answer(void) { return answer; }

namespace {

/* string_box and edit_box: a form of named fields, each of kinds[i]
   (every one `all` when kinds is NULL) */
int form(char *title, char **names, int n, char **values, int size, const int *kinds, int all)
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
    buf_kinds(&b, kinds, all, n);
    if (!ask_wait(&b, id)) return 0;
    arr = js_find(answer, "values");
    for (i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) js_string(e, values[i], size);
    }
    return 1;
}

} // namespace

int j_string_box(int n, int row, int col, char *title, char **names,
                        char values[][MAX_LEN_SBOX], int maxchar, const int *kinds)
{
    char *v[64];
    int i;
    (void)row; (void)col; (void)maxchar;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) v[i] = values[i];
    return form(title, names, n, v, MAX_LEN_SBOX, kinds, XPP_FIELD_TEXT);
}

int j_edit_box(int n, char *title, char **names, char **values)
{
    /* edit_rhs.c's right-hand sides and functions: expressions */
    return form(title, names, n, values, MAX_LEN_EBOX, NULL, XPP_FIELD_EXPRESSION);
}

/* the file selector lists the directory like the X11 one; an answer with
   "cd" changes directory (as X11 does, for good) and asks again. "mode"
   says whether the command reads the file or writes it, so a client can
   show an open or a save dialog (docs/ui-v2.md section 4). */
int j_file_selector(char *title, char *file, char *wild)
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

namespace {

/* a data coordinate as the nearest pixel of an axis that maps pixel p0 to
   v0 and p1 to v1 (the inverse of scale_to_real, auto_motion_xy) */
int data_to_pixel(double v, double v0, double v1, int p0, int p1)
{
    double p;
    if (!(v1 != v0) || !isfinite(v)) return p0;
    p = p0 + (v - v0) * (p1 - p0) / (v1 - v0);
    if (p > 1e6) p = 1e6;
    if (p < -1e6) p = -1e6;
    return (int)lround(p);
}

} // namespace

/* point k (0: x,y; 1: x2,y2) of the answer to a mouse, rubber, drag or grab
   ask in window win: pixels, or data coordinates xd,yd (xd2,yd2) converted
   with the window's current axes to the pixels that map back to them, so
   the command goes on exactly as for a click there (docs/protocol.md) */
void answer_point(unsigned long win, int k, int *x, int *y)
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
        *x = data_to_pixel(js_num(jx, 0), plot_windows.current->xlo, plot_windows.current->xhi, DLeft, DRight);
        *y = data_to_pixel(js_num(jy, 0), plot_windows.current->ylo, plot_windows.current->yhi, DBottom, DTop);
    }
}

int mouse_ask(unsigned long win, const char *kind, int flag, int *v, int nv)
{
    Buf b;
    int i, id = ask_begin(&b, kind);
    buf_printf(&b, ",\"win\":%lu,\"flag\":%d", win, flag);
    if (!ask_wait(&b, id)) return 0;
    for (i = 0; i < nv / 2; i++) answer_point(win, i, &v[2 * i], &v[2 * i + 1]);
    return 1;
}

int j_get_mouse_xy(int *x, int *y)
{
    int v[2];
    if (!mouse_ask(plot_windows.draw_win, "mouse", 0, v, 2)) return 0;
    *x = v[0];
    *y = v[1];
    return 1;
}

int j_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    int v[4];
    if (!mouse_ask(plot_windows.draw_win, "rubber", flag, v, 4)) return 0;
    *i1 = v[0]; *j1 = v[1]; *i2 = v[2]; *j2 = v[3];
    return 1;
}

int j_menu_choose(const struct XppMenu *m, int def)
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

void j_show_menu(int which)
{
    Buf b = {0};
    buf_printf(&b, "{\"ev\":\"menu\",\"which\":%d}", which);
    send_buf(&b);
    xpp_free(b.s);
}

void j_open_help(const char *chapter, const char *anchor)
{
    Buf b = {0};
    BUF_LIT(&b, "{\"ev\":\"help\",\"chapter\":");
    buf_str(&b, chapter);
    if (anchor && *anchor) {
        BUF_LIT(&b, ",\"anchor\":");
        buf_str(&b, anchor);
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
    xpp_free(b.s);
}

/* one pointer event of a drag in window win: 1 down, 2 move, 3 up; 0 when
   a key or Cancel ends the drag */
int ask_drag(unsigned long win, int *x, int *y)
{
    Buf b;
    char what[8];
    int id = ask_begin(&b, "drag");
    buf_printf(&b, ",\"win\":%lu", win);
    if (!ask_wait(&b, id) || !get_str(answer, "what", what, sizeof what)) return 0;
    answer_point(win, 0, x, y);
    return strcmp(what, "down") == 0 ? 1 : strcmp(what, "move") == 0 ? 2 : strcmp(what, "up") == 0 ? 3 : 0;
}

void j_q_calc(void)
{
    char expr[256] = "";
    double z;
    char result[300] = "Formula:";
    /* the X11 calculator shows the answer in its window: here in the prompt */
    while (new_string_of(result, expr, XPP_FIELD_EXPRESSION)) {
        if (do_calc(expr, &z) != -1) {
            snprintf(result, sizeof result, "%.200s = %.16g   Formula:", expr, z);
            send_simple("message", "calc", result);
        }
    }
}

/* ---- long loops ------------------------------------------------------------ */

int j_check_abort(void)
{
    char *line;
    static double last;
    /* let the client see the picture grow, a few frames a second */
    if (xpp_every(&last, 0.05)) {
        flush_pending();
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

int j_progress_begin(void) { return 100; }

void j_progress(int nit, int icount, int cwidth)
{
    static double last;
    Buf b = {0};
    (void)cwidth;
    if (!xpp_every(&last, 0.1)) return;
    buf_printf(&b, "{\"ev\":\"progress\",\"n\":%d,\"of\":%d}", icount, nit);
    send_buf(&b);
    xpp_free(b.s);
}

} // namespace xpp::json
