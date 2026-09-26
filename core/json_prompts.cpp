/* Prompts: every question the core asks the client (a menu, a string box,
   a form, a file name, a mouse pick, a drag, ...) is an "ask" event with an
   id; the core blocks until the matching {"cmd":"answer","id":N,...}
   arrives (docs/protocol.md). Also the messages, and what a long
   computation does between steps (Esc, progress). */
#include "ui_json_internal.h"
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
#include <array>
#include <iterator>
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
std::string answer;

constexpr size_t SCRIPT_ASK_MAX = 399;
std::string script_ask; /* the open question (cut to SCRIPT_ASK_MAX), for script_fail() */

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
    if (session.script_mode) {
        try {
            script_ask.assign(b->s, 0, SCRIPT_ASK_MAX);
        } catch (...) {
            out_of_memory("asking");
        }
    }
    send_buf(b);
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
        lid = get_int(line, "id", -1);
        if (is_cmd(line, "answer") && (lid == -1 || lid == id)) {
            try {
                answer = line;
            } catch (...) {
                out_of_memory("taking an answer");
            }
            /* an Abort sent before this answer no longer stops the command */
            if (ask_user) xpp_job_resume(read_line_seq());
            const char *ok = js_find(answer.c_str(), "ok");
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
        if (session.script_mode) script_fail("does not answer the open question", line, script_ask.c_str());
    }
}

int ask_begin(Buf *b, const char *kind)
{
    b->s.clear();
    ask_id++;
    ask_user = strcmp(kind, "pixels") != 0;
    buf_format(b, "{{\"ev\":\"ask\",\"id\":{:d},\"kind\":\"{}\"", ask_id, kind);
    return ask_id;
}

void j_err_msg(const char *msg)
{
    /* a script that provokes an error fails the run (docs/protocol.md) */
    if (session.script_mode) session.script_error = 1;
    send_simple("message", "error", msg);
}

void j_ping(void) { send_simple("ping", NULL, NULL); }

void j_bottom_msg(int, const char *msg)
{
    send_simple("message", "bottom", msg);
}

void j_message_box(const char *msg) { send_simple("message", "box", msg); }
void j_kill_message_box(void) { send_simple("message", "box", ""); }
void j_title_text(const char *s) { send_simple("title", "text", s); }
void j_canvas_xy(const char *s) { send_simple("message", "xy", s); }

namespace {

/* a field's kind as the protocol names it (docs/protocol.md "Asks", `kinds`) */
void buf_kind(Buf *b, int kind)
{
    static constexpr std::array<const char *, 6> names = {"text", "integer", "number", "formula", "expression", "file"};
    if (kind >= XPP_FIELD_NAME) buf_format(b, "\"name:{:d}\"", kind - XPP_FIELD_NAME);
    else buf_str(b, kind > 0 && kind < static_cast<int>(names.size()) ? names[kind] : "text");
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
    buf_format(&b, ",\"max\":{:d}", max);
    buf_kinds(&b, &kind, kind, 1);
    if (!ask_wait(&b, id)) return 0;
    get_str(answer.c_str(), "value", value, max + 1);
    return 1;
}

int j_new_string(const char *name, char *value, int kind)
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
    std::string k;
    get_string(answer.c_str(), "key", k, 8);
    return k[0] == 'y';
}

int j_two_choice(const char *c1, const char *c2, const char *q, const char *key, const char *title)
{
    Buf b;
    std::string k;
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
    get_string(answer.c_str(), "key", k, 8);
    return static_cast<unsigned char>(k[0]);
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

int j_checklist(const char *title, const char *const *names, int *flags, int n)
{
    Buf b;
    int i, id = ask_begin(&b, "checklist");
    const char *arr;
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"names\":");
    buf_str_array(&b, names, n);
    BUF_LIT(&b, ",\"flags\":[");
    for (i = 0; i < n; i++) buf_format(&b, "{}{:d}", i ? "," : "", flags[i]);
    BUF_LIT(&b, "]");
    if (!ask_wait(&b, id)) return 0;
    arr = js_find(answer.c_str(), "flags");
    for (i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) flags[i] = js_num(e, flags[i]) != 0;
    }
    return 1;
}

const char *ask_answer(void) { return answer.c_str(); }

namespace {

/* string_box and edit_box: a form of named fields, each of kinds[i]
   (every one `all` when kinds is NULL) */
int form(const char *title, const char *const *names, int n, char **values, int size, const int *kinds, int all)
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
    buf_format(&b, ",\"max\":{:d}", size - 1);
    buf_kinds(&b, kinds, all, n);
    if (!ask_wait(&b, id)) return 0;
    arr = js_find(answer.c_str(), "values");
    for (i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) js_string(e, values[i], size);
    }
    return 1;
}

} // namespace

int j_string_box(int n, int, int, const char *title, const char *const *names, char values[][MAX_LEN_SBOX], int,
                 const int *kinds)
{
    std::array<char *, 64> v;
    if (n > static_cast<int>(v.size())) n = static_cast<int>(v.size());
    for (int i = 0; i < n; i++) v[i] = values[i];
    return form(title, names, n, v.data(), MAX_LEN_SBOX, kinds, XPP_FIELD_TEXT);
}

int j_edit_box(int n, const char *title, const char *const *names, char **values)
{
    /* edit_rhs.c's right-hand sides and functions: expressions */
    return form(title, names, n, values, MAX_LEN_EBOX, NULL, XPP_FIELD_EXPRESSION);
}

/* the file selector lists the directory like the X11 one; an answer with
   "cd" changes directory (as X11 does, for good) and asks again. "mode"
   says whether the command reads the file or writes it, so a client can
   show an open or a save dialog (docs/ui-v2.md section 4). */
int j_file_selector(const char *title, char *file, const char *wild)
{
    constexpr size_t PATTERN_MAX = 255, CD_MAX = 1024;
    std::string pattern(wild ? wild : ""), cd;
    if (pattern.size() > PATTERN_MAX) pattern.resize(PATTERN_MAX);
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
        buf_str(&b, pattern.c_str());
        BUF_LIT(&b, ",\"dir\":");
        buf_str(&b, cur_dir);
        if (get_fileinfo(pattern.c_str(), cur_dir, &ff)) {
            BUF_LIT(&b, ",\"dirs\":");
            buf_str_array(&b, ff.dirnames, ff.ndirs);
            BUF_LIT(&b, ",\"files\":");
            buf_str_array(&b, ff.filenames, ff.nfiles);
            free_finfo(&ff);
        }
        if (!ask_wait(&b, id)) return 0;
        if (get_string(answer.c_str(), "wild", cd, CD_MAX) && !cd.empty()) pattern = cd.substr(0, PATTERN_MAX);
        if (get_string(answer.c_str(), "cd", cd, CD_MAX) && !cd.empty()) {
            change_directory(cd.c_str());
            continue;
        }
        if (!js_find(answer.c_str(), "file")) continue; /* a new pattern alone lists again */
        get_str(answer.c_str(), "file", file, 256);
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
    return static_cast<int>(lround(p));
}

} // namespace

/* point k (0: x,y; 1: x2,y2) of the answer to a mouse, rubber, drag or grab
   ask in window win: pixels, or data coordinates xd,yd (xd2,yd2) converted
   with the window's current axes to the pixels that map back to them, so
   the command goes on exactly as for a click there (docs/protocol.md) */
void answer_point(unsigned long win, int k, int *x, int *y)
{
    static constexpr std::array<const char *, 2> px = {"x", "x2"}, py = {"y", "y2"}, dx = {"xd", "xd2"}, dy = {"yd", "yd2"};
    const char *jx = js_find(answer.c_str(), dx[k]), *jy = js_find(answer.c_str(), dy[k]);
    if (!jx || !jy) {
        *x = get_int(answer.c_str(), px[k], 0);
        *y = get_int(answer.c_str(), py[k], 0);
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
    buf_format(&b, ",\"win\":{:d},\"flag\":{:d}", win, flag);
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
    std::string k;
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
    buf_format(&b, ",\"def\":{:d}", def);
    if (!ask_wait(&b, id)) return 27;
    get_string(answer.c_str(), "key", k, 8);
    return k[0] ? static_cast<unsigned char>(k[0]) : 27;
}

void j_show_menu(int which)
{
    Buf b;
    buf_format(&b, "{{\"ev\":\"menu\",\"which\":{:d}}}", which);
    send_buf(&b);
}

void j_open_help(const char *chapter, const char *anchor)
{
    Buf b;
    BUF_LIT(&b, "{\"ev\":\"help\",\"chapter\":");
    buf_str(&b, chapter);
    if (anchor && *anchor) {
        BUF_LIT(&b, ",\"anchor\":");
        buf_str(&b, anchor);
    }
    BUF_LIT(&b, "}");
    send_buf(&b);
}

/* one pointer event of a drag in window win: 1 down, 2 move, 3 up; 0 when
   a key or Cancel ends the drag */
int ask_drag(unsigned long win, int *x, int *y)
{
    Buf b;
    std::string what;
    int id = ask_begin(&b, "drag");
    buf_format(&b, ",\"win\":{:d}", win);
    if (!ask_wait(&b, id) || !get_string(answer.c_str(), "what", what, 8)) return 0;
    answer_point(win, 0, x, y);
    return what == "down" ? 1 : what == "move" ? 2 : what == "up" ? 3 : 0;
}

void j_q_calc(void)
{
    std::array<char, 256> expr{}; /* new_string_of edits it in place, as the X11 prompt's line */
    double z;
    std::string result = "Formula:";
    /* the X11 calculator shows the answer in its window: here in the prompt */
    while (new_string_of(result.c_str(), expr.data(), XPP_FIELD_EXPRESSION)) {
        if (do_calc(expr.data(), &z) != -1) {
            result = xpp::format("{:.200} = {:.16g}   Formula:", expr.data(), z);
            send_simple("message", "calc", result.c_str());
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

void j_progress(int nit, int icount, int)
{
    static double last;
    Buf b;
    if (!xpp_every(&last, 0.1)) return;
    buf_format(&b, "{{\"ev\":\"progress\",\"n\":{:d},\"of\":{:d}}}", icount, nit);
    send_buf(&b);
}

} // namespace xpp::json
