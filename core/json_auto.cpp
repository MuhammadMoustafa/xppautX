/* The AUTO window: its prompts and mouse, the diagram's points as data
   ("diagram" events), and AUTO's settings as data ({"cmd":"auto","op":"set"},
   auto_settings.h). */
#include "ui_json_internal.h"
#include "xpp_mem.h"
#include "xpp_job.h"
#include "xpp_globals.h"
#include "menus.h"
#include "mykeydef.h"
#include "diagram.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "xpp_io.h"
#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* the core's own globals and functions that have no header of their own */
extern "C" {
extern BIFUR Auto;
}

namespace xpp::json {

/* ---- AUTO diagram data --------------------------------------------------------
   The points of the diagram go out as data ("diagram"
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

namespace {

XppDiagPoint *dg;
int dg_n, dg_cap, dg_client, dg_dirty;

int dg_replay, dg_match, dg_axes;
struct {
    double xmin, xmax, ymin, ymax;
    int x0, y0, wid, hgt, plot;
    char xlabel[AUTO_LABEL_LEN], ylabel[AUTO_LABEL_LEN];
} dg_ax;

/* the client has nothing: a new window, or one it no longer holds */
void diag_forget(void)
{
    dg_n = dg_client = dg_dirty = 0;
    dg_replay = dg_match = dg_axes = 0;
}

int diag_same(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return a->ibr == b->ibr && a->pt == b->pt && a->itp == b->itp && a->lab == b->lab && a->type == b->type &&
           a->flag2 == b->flag2 && a->draw == b->draw && a->newseg == b->newseg && a->color == b->color &&
           a->lw == b->lw && a->from == b->from && memcmp(&a->x, &b->x, sizeof a->x) == 0 && memcmp(&a->y1, &b->y1, sizeof a->y1) == 0 &&
           memcmp(&a->y2, &b->y2, sizeof a->y2) == 0;
}

/* the replay is over: the list is its first k points */
void diag_end_replay(int k)
{
    dg_replay = 0;
    dg_n = k;
    if (k < dg_dirty) dg_dirty = k;
}

} // namespace

void j_auto_diagram(const XppDiagPoint *p)
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

namespace {

/* a number, null when it is not finite (JSON has no nan) */
void buf_num(Buf *b, double v)
{
    if (v != v || v > 1e308 || v < -1e308) BUF_LIT(b, "null");
    else buf_printf(b, "%.7g", v);
}

void diag_axes(Buf *b)
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
void diag_run(Buf *b, int i, int j)
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

} // namespace

/* the index in the data the client holds of AUTO's diagram entry `node`
   (auto_data.h); the latest when a redraw of other axes left two */
int diag_point_of_node(int node)
{
    int i;
    for (i = dg_client - 1; i >= 0; i--)
        if (dg[i].node == node) return i;
    return -1;
}

namespace {

/* b continues a's run */
int diag_joins(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return !b->newseg && !b->from && abs(a->ibr) == abs(b->ibr) && abs(b->pt) == abs(a->pt) + 1 && a->type == b->type &&
           a->draw == b->draw && a->color == b->color && a->lw == b->lw && a->flag2 == b->flag2;
}

} // namespace

/* send what the client does not have yet. final: the command ends or asks
   something, so a replay that has not been completed never will be */
void diag_flush(int final)
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
    /* the points in events of some 60 kB */
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
void j_auto_refresh(void)
{
    static double last;
    if (xpp_every(&last, 0.05)) {
        json_flush();
        auto_data_update(0);
    }
}

/* a reconnected client has a blank diagram, and no data: all of it again,
   starting with a reset */
void auto_redraw_for_client(void)
{
    if (!Auto.exist) return;
    dg_dirty = 0;
    dg_client = dg_n > 0 ? dg_n : 1;
    redraw_diagram();
}

/* ---- AUTO window --------------------------------------------------------------- */

void j_auto_make_window(const char *wname, const char *iname)
{
    (void)iname;
    Auto.hgt = 20 * text_metrics.big_height;
    Auto.wid = 67 * text_metrics.big_width;
    Auto.x0 = 10 * text_metrics.small_width;
    Auto.y0 = 2 * text_metrics.small_height;
    XPP_STRCPY(Auto.hinttxt, "hint");
    diag_forget();      /* a new window has no data */
    auto_data_forget(); /* nor an info strip or a stability circle */
    send_window("create", WIN_AUTO, Auto.wid + 12 * text_metrics.small_width, Auto.hgt + 4 * text_metrics.small_height, wname);
    draw_bif_axes();
}

int j_auto_check_abort(int *iflag)
{
    *iflag = 0;
    if (j_check_abort() == ESC) {
        *iflag = 1;
        return 0;
    }
    return 0;
}
int j_auto_rubber(int *i1, int *j1, int *i2, int *j2, int flag)
{
    int v[4];
    if (!mouse_ask(WIN_AUTO, "rubber", flag, v, 4)) return 0;
    *i1 = v[0]; *j1 = v[1]; *i2 = v[2]; *j2 = v[3];
    return 1;
}
int j_auto_choose_key(const char *title, const char *const *list, const char *key, int n, int max, int def,
                             int x, int y, const char *const *hints, const char *httxt)
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

namespace {

/* a grab answer's key after its point ({"point":i,"key":"Return"}): the
   point first, then the key without asking again */
int grab_key_after;

} // namespace

int j_auto_grab_event(int *x, int *y)
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
    if ((jp = js_find(ask_answer(), "point")) != NULL) {
        double i = js_num(jp, -1);
        const XppDiagPoint *p = i >= 0 && i < dg_client ? &dg[(int)i] : NULL;
        *x = p && diagram_has(p->node, p->ibr, p->pt) ? p->node : -1;
        if (*x >= 0 && get_str(ask_answer(), "key", k, sizeof k)) grab_key_after = key_code(k);
        return XPP_AUTO_NODE;
    }
    if (get_str(ask_answer(), "key", k, sizeof k)) return key_code(k);
    answer_point(WIN_AUTO, 0, x, y);
    return XPP_AUTO_CLICK;
}
void j_auto_show_hint(void) { send_simple("message", "auto", Auto.hinttxt); }

/* AUTO Axes/Scroll: drag the diagram (auto_x11.c x11_auto_scroll_window) */
void j_auto_scroll_window(void)
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

/* ---- AUTO's settings as data (auto_settings.h) ---- */

int is_auto_set(const char *line)
{
    char o[8];
    return is_cmd(line, "auto") && get_str(line, "op", o, sizeof o) && strcmp(o, "set") == 0;
}

namespace {

/* the "numerics", "pars", "axes" and "marks" of {"cmd":"auto","op":"set",...}
   into s; 0 with why when one is not what docs/protocol.md says */
int read_auto_set(const char *line, AutoSettingsSet *s, char *why, size_t n)
{
    const char *num = js_find(line, "numerics"), *pars = js_find(line, "pars"), *axes = js_find(line, "axes"),
               *marks = js_find(line, "marks"), *v;
    int i;
    for (i = 0; num && *num == '{' && i < AUTO_NUM_N; i++) {
        if (!(v = js_find(num, auto_settings_num_key(i)))) continue;
        if (!js_number(v, &s->num[i])) {
            xpp_snprintf(why, n, "%s must be a number", auto_settings_num_label(i));
            return 0;
        }
        s->has_num[i] = 1;
    }
    if (pars && *pars == '[') {
        for (i = 0; (v = js_elem(pars, i)) != NULL; i++) {
            if (i >= AUTO_SETTINGS_PARS || !js_string(v, s->pars[i], sizeof s->pars[i])) {
                xpp_snprintf(why, n, "AUTO's parameters must be a list of at most %d names", AUTO_SETTINGS_PARS);
                return 0;
            }
        }
        s->npars = i;
    }
    if (axes && *axes == '{') {
        static const char *const range[4] = {"xmin", "xmax", "ymin", "ymax"};
        double z;
        if ((v = js_find(axes, "plot")) != NULL) {
            if (!js_number(v, &z) || z < INT_MIN || z > INT_MAX || z != floor(z)) {
                xpp_snprintf(why, n, "the plot type must be a whole number");
                return 0;
            }
            s->has_plot = 1;
            s->plot = (int)z;
        }
        get_str(axes, "var", s->var, sizeof s->var);
        get_str(axes, "par1", s->par1, sizeof s->par1);
        get_str(axes, "par2", s->par2, sizeof s->par2);
        for (i = 0; i < 4; i++) {
            if (!(v = js_find(axes, range[i]))) continue;
            if (!js_number(v, &s->range[i])) {
                xpp_snprintf(why, n, "%c%s must be a number", range[i][0] - 32, range[i] + 1);
                return 0;
            }
            s->has_range[i] = 1;
        }
        s->fit = get_num(axes, "fit", 0) != 0;
    }
    if (marks && *marks == '[') {
        for (i = 0; (v = js_elem(marks, i)) != NULL; i++) {
            if (i >= AUTO_SETTINGS_MARKS || *v != '[' || !js_string(js_elem(v, 0), s->mark_name[i], sizeof s->mark_name[i])
                || !js_number(js_elem(v, 1), &s->mark_value[i])) {
                xpp_snprintf(why, n, "Mark values must be a list of at most %d pairs [name, number]", AUTO_SETTINGS_MARKS);
                return 0;
            }
        }
        s->nmarks = i;
    }
    return 1;
}

/* {"cmd":"auto","op":"set",...}: AUTO's settings, checked and set all
   together or not at all; a refusal is an error message */
void auto_set_command(const char *line)
{
    AutoSettingsSet s;
    char why[256];
    auto_settings_set_init(&s);
    if (!read_auto_set(line, &s, why, sizeof why) || auto_settings_apply(&s, why, sizeof why) != 0) {
        char msg[300];
        XPP_FORMAT_TO_BUF(msg, "AUTO settings: {}", why);
        j_err_msg(msg);
    }
}

/* the settings a question's wait put aside, applied at the command's end */
char **deferred_sets;
int n_deferred, cap_deferred;

} // namespace

void defer_auto_set(const char *line)
{
    if (n_deferred == cap_deferred) {
        cap_deferred = cap_deferred ? 2 * cap_deferred : 4;
        deferred_sets = static_cast<char **>(xpp_realloc(deferred_sets, cap_deferred * sizeof *deferred_sets));
    }
    deferred_sets[n_deferred++] = xpp_strdup(line);
}

void apply_deferred_sets(void)
{
    int i;
    for (i = 0; i < n_deferred; i++) {
        auto_set_command(deferred_sets[i]);
        xpp_free(deferred_sets[i]);
    }
    n_deferred = 0;
}

/* {"cmd":"auto","op":...}: the AUTO window's buttons */
void auto_command(const char *line)
{
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
    else if (strcmp(o, "set") == 0) auto_set_command(line);
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
}

} // namespace xpp::json
