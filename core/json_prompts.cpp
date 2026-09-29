/* Prompts: every question the core asks the client (a menu, a string box,
   a form, a file name, a mouse pick, a drag, ...) is an "ask" event with an
   id; the core blocks until the matching {"cmd":"answer","id":N,...}
   arrives (docs/protocol.md). Also the messages, and what a long
   computation does between steps (Esc, progress). */
#include "ui_json_internal.h"
#include "session.h"
#include "xpp_inbox.h"
#include "xpp_job.h"
#include "xpp_util.h"
#include "menus.h"
#include "graphics.h"
#include "auto_nox.h"
#include "xpp_files.h"
#include "auto_data.h"
#include "auto_settings.h"
#include <array>
#include <iterator>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* the core's own globals and functions that have no header of their own */
extern "C" {
}

namespace xpp::json {

namespace {


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
            xpp_out_of_memory("asking");
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
                xpp_out_of_memory("taking an answer");
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
        /* for a script this line was supposed to answer this ask, and
           nothing after it can line up */
        if (session.script_mode) script_fail("does not answer the open question", line, script_ask.c_str());
        /* anything else was sent before the client saw the question (a
           click right behind the command that asks): kept for after the
           command, never lost (W95) */
        defer_line(line, read_line_refused());
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

int j_dialog(const char *title, const char *name, std::string &value, const char *ok, const char *cancel, int kind)
{
    Buf b;
    int id = ask_begin(&b, "string");
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"name\":");
    buf_str(&b, name);
    BUF_LIT(&b, ",\"value\":");
    buf_str(&b, value.c_str());
    BUF_LIT(&b, ",\"ok\":");
    buf_str(&b, ok);
    BUF_LIT(&b, ",\"cancel\":");
    buf_str(&b, cancel);
    buf_kinds(&b, &kind, kind, 1);
    if (!ask_wait(&b, id)) return 0;
    get_string(answer.c_str(), "value", value); /* whole: no dialog cuts (W76) */
    return 1;
}

int j_new_string(const char *name, std::string &value, int kind)
{
    return j_dialog("", name, value, "Ok", "Cancel", kind);
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

/* string_box: a form of named fields, each of kinds[i]
   (every one `all` when kinds is NULL) */
int form(const char *title, const char *const *names, std::span<std::string> values, const int *kinds, int all)
{
    Buf b;
    const int n = static_cast<int>(values.size());
    int id = ask_begin(&b, "form");
    /* every value whole, however long (W76) */
    std::vector<const char *> shown(values.size());
    for (size_t i = 0; i < values.size(); i++) shown[i] = values[i].c_str();
    BUF_LIT(&b, ",\"title\":");
    buf_str(&b, title);
    BUF_LIT(&b, ",\"names\":");
    buf_str_array(&b, names, n);
    BUF_LIT(&b, ",\"values\":");
    buf_str_array(&b, shown.data(), n);
    buf_kinds(&b, kinds, all, n);
    if (!ask_wait(&b, id)) return 0;
    const char *arr = js_find(answer.c_str(), "values");
    for (int i = 0; i < n; i++) {
        const char *e = js_elem(arr, i);
        if (e) js_string(e, values[i]);
    }
    return 1;
}

} // namespace

int j_string_box(int, int, const char *title, const char *const *names, std::span<std::string> values,
                 const int *kinds)
{
    return form(title, names, values, kinds, XPP_FIELD_TEXT);
}

/* the file selector lists the directory like the X11 one; an answer with
   "cd" changes directory (as X11 does, for good) and asks again. "mode"
   says whether the command reads the file or writes it, so a client can
   show an open or a save dialog (docs/ui-v2.md section 4). */
int j_file_selector(const char *title, std::string &file, const char *wild)
{
    constexpr size_t PATTERN_MAX = 255, CD_MAX = 1024;
    std::string pattern(wild ? wild : ""), cd;
    if (pattern.size() > PATTERN_MAX) pattern.resize(PATTERN_MAX);
    if (!xpp_files_cur_dir()[0]) xpp_files_refresh_cur_dir();
    for (;;) {
        Buf b;
        std::vector<std::string> dirs, files;
        int id = ask_begin(&b, "file");
        BUF_LIT(&b, ",\"title\":");
        buf_str(&b, title);
        BUF_LIT(&b, ",\"mode\":");
        buf_str(&b, xpp_files_ask_mode(title));
        BUF_LIT(&b, ",\"file\":");
        buf_str(&b, file.c_str());
        BUF_LIT(&b, ",\"wild\":");
        buf_str(&b, pattern.c_str());
        BUF_LIT(&b, ",\"dir\":");
        buf_str(&b, xpp_files_cur_dir());
        if (xpp_files_list_matching(pattern.c_str(), xpp_files_cur_dir(), dirs, files)) {
            std::vector<const char *> dirv, filev;
            for (const std::string &s : dirs) dirv.push_back(s.c_str());
            for (const std::string &s : files) filev.push_back(s.c_str());
            BUF_LIT(&b, ",\"dirs\":");
            buf_str_array(&b, dirv.data(), static_cast<int>(dirv.size()));
            BUF_LIT(&b, ",\"files\":");
            buf_str_array(&b, filev.data(), static_cast<int>(filev.size()));
        }
        if (!ask_wait(&b, id)) return 0;
        if (get_string(answer.c_str(), "wild", cd, CD_MAX) && !cd.empty()) pattern = cd.substr(0, PATTERN_MAX);
        if (get_string(answer.c_str(), "cd", cd, CD_MAX) && !cd.empty()) {
            xpp_files_change_dir(cd.c_str());
            continue;
        }
        if (!js_find(answer.c_str(), "file")) continue; /* a new pattern alone lists again */
        get_string(answer.c_str(), "file", file); /* a name in the folder or a full path, whole (W88) */
        return !file.empty();
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
    xpp::Session &s=xpp::session();
    static constexpr std::array<const char *, 2> px = {"x", "x2"}, py = {"y", "y2"}, dx = {"xd", "xd2"}, dy = {"yd", "yd2"};
    const char *jx = js_find(answer.c_str(), dx[k]), *jy = js_find(answer.c_str(), dy[k]);
    if (!jx || !jy) {
        *x = get_int(answer.c_str(), px[k], 0);
        *y = get_int(answer.c_str(), py[k], 0);
    } else if (win == WIN_AUTO) {
        *x = data_to_pixel(js_num(jx, 0), s.auto_state.bifur.xmin, s.auto_state.bifur.xmax, s.auto_state.bifur.x0, s.auto_state.bifur.x0 + s.auto_state.bifur.wid);
        *y = data_to_pixel(js_num(jy, 0), s.auto_state.bifur.ymin, s.auto_state.bifur.ymax, s.auto_state.bifur.y0 + s.auto_state.bifur.hgt, s.auto_state.bifur.y0);
    } else {
        get_draw_area();
        *x = data_to_pixel(js_num(jx, 0), s.plot_windows.current->xlo, s.plot_windows.current->xhi, s.drawing.d_left, s.drawing.d_right);
        *y = data_to_pixel(js_num(jy, 0), s.plot_windows.current->ylo, s.plot_windows.current->yhi, s.drawing.d_bottom, s.drawing.d_top);
    }
}

int mouse_ask(unsigned long win, const char *kind, int flag, std::span<int> v)
{
    Buf b;
    int id = ask_begin(&b, kind);
    buf_format(&b, ",\"win\":{:d},\"flag\":{:d}", win, flag);
    if (!ask_wait(&b, id)) return 0;
    for (size_t i = 0; i < v.size() / 2; i++) answer_point(win, static_cast<int>(i), &v[2 * i], &v[2 * i + 1]);
    return 1;
}

int j_get_mouse_xy(int *x, int *y)
{
    std::array<int, 2> v;
    if (!mouse_ask(xpp::session().plot_windows.draw_win, "mouse", 0, v)) return 0;
    *x = v[0];
    *y = v[1];
    return 1;
}

int j_rubber_band(int *i1, int *j1, int *i2, int *j2, int flag)
{
    std::array<int, 4> v;
    if (!mouse_ask(xpp::session().plot_windows.draw_win, "rubber", flag, v)) return 0;
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
    session.menu.store(which, std::memory_order_relaxed);
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

/* text for the page's clipboard */
void j_copy_text(const char *what, const char *text)
{
    Buf b;
    BUF_LIT(&b, "{\"ev\":\"copy\",\"what\":");
    buf_str(&b, what);
    BUF_LIT(&b, ",\"text\":");
    buf_str(&b, text);
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
    std::string expr;
    double z;
    std::string result = "Formula:";
    /* the X11 calculator shows the answer in its window: here in the prompt */
    while (new_string_of(result.c_str(), expr, XPP_FIELD_EXPRESSION)) {
        if (do_calc(expr.c_str(), &z) != -1) {
            result = xpp::format("{:.200} = {:.16g}   Formula:", expr, z);
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
    /* only the control queue, judged by the list of what a computation
       takes (during_run()): a line queued just before it began, a key or a
       set, is kept for after the command like one sent during it, refused
       when its kind is data or computation */
    while ((line = read_line(XPP_INBOX_CONTROL, 0)) != NULL) {
        const int take = during_run(line);
        if (take != XPP_INBOX_CONTROL) {
            defer_line(line, take == XPP_INBOX_REFUSE);
            continue;
        }
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
