/* The AUTO window: its prompts and mouse, the diagram's points as data
   ("diagram" events), and AUTO's settings as data ({"cmd":"auto","op":"set"},
   auto_settings.h). */
#include "ui_json_internal.h"
#include "session.h"
#include "xpp_job.h"
#include "menus.h"
#include "mykeydef.h"
#include "diagram.h"
#include "auto_data.h"
#include "auto_settings.h"
#include "json_number.h"
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "menus.h"

namespace xpp::json {

/* ---- AUTO diagram data --------------------------------------------------------
   The points of the diagram go out as data ("diagram"
   events, docs/protocol.md), so that the client can zoom, pan and show a
   point under the mouse without a round trip. Each view of the diagram
   (W50, AutoState::views) has its own list, every event naming its view:
   in view v, dg[0..n) is the list the client holds once the pending events
   are out: `client` points of it were sent, and from `dirty` on it changed
   since (dirty < client means the client must first drop its points from
   dirty on). The client holds `client_views` lists; when the number of
   views differs, a `views` event says the new number first (a closed view
   leaves the lists after it to be sent again, by the replay below).

   A redraw is a clear (draw_bif_axes) and then every point again, and
   mostly the same points at other axes: a zoom, Fit, a scroll, a resize.
   So after a clear the points are compared with the list (replay, `match`
   of them agreed so far) and nothing is sent while they agree; if all of
   them agree only the new axes go out. The first point that differs drops
   the rest of the old list and is sent with the ones after it. A clear
   that is not followed by the whole list (the Clear button) drops the rest
   at the end of the command. */

namespace {

struct ViewData {
    std::vector<XppDiagPoint> dg; /* n of them in use */
    int n = 0, client = 0, dirty = 0;
    int replay = 0, match = 0, axes = 0;
    struct {
        double xmin, xmax, ymin, ymax;
        int x0, y0, wid, hgt, plot;
        std::string xlabel, ylabel;
    } ax{};
};

std::vector<ViewData> views(1);
int client_views = 1; /* the lists the client holds */

/* view v's data, made when the view is new */
ViewData &view_data(int v)
{
    try {
        if (static_cast<std::size_t>(v) >= views.size()) views.resize(static_cast<std::size_t>(v) + 1);
    } catch (...) {
        xpp_out_of_memory("keeping the AUTO diagram");
    }
    return views[static_cast<std::size_t>(v)];
}

/* the list the grab and the info strip's point index go by: the active
   view's (every view holds the same points in the same order) */
ViewData &active_data() { return view_data(xpp::session().auto_state.active_view); }

} // namespace

/* the client has nothing: a new window, or one it no longer holds */
void diag_forget(void)
{
    views.assign(1, ViewData());
    client_views = 1;
}

namespace {

int diag_same(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return a->ibr == b->ibr && a->pt == b->pt && a->itp == b->itp && a->lab == b->lab && a->type == b->type &&
           a->flag2 == b->flag2 && a->draw == b->draw && a->newseg == b->newseg && a->color == b->color &&
           a->lw == b->lw && a->from == b->from && std::memcmp(&a->x, &b->x, sizeof a->x) == 0 && std::memcmp(&a->y1, &b->y1, sizeof a->y1) == 0 &&
           std::memcmp(&a->y2, &b->y2, sizeof a->y2) == 0;
}

/* the replay is over: the list is its first k points */
void diag_end_replay(ViewData &d, int k)
{
    d.replay = 0;
    d.n = k;
    if (k < d.dirty) d.dirty = k;
}

} // namespace

void j_auto_diagram(int view, const XppDiagPoint *p)
{
    ViewData &d = view_data(view);
    if (!p) {
        const xpp::Session &s = xpp::session();
        const AUTOAX &a = s.auto_state.views[static_cast<std::size_t>(view)].axes;
        d.ax.xmin = a.xmin;
        d.ax.xmax = a.xmax;
        d.ax.ymin = a.ymin;
        d.ax.ymax = a.ymax;
        d.ax.x0 = s.auto_state.bifur.x0;
        d.ax.y0 = s.auto_state.bifur.y0;
        d.ax.wid = s.auto_state.bifur.wid;
        d.ax.hgt = s.auto_state.bifur.hgt;
        d.ax.plot = a.plot;
        get_auto_str(s, a, d.ax.xlabel, d.ax.ylabel);
        d.axes = 1;
        d.replay = 1;
        d.match = 0;
        return;
    }
    if (d.replay) {
        if (d.match < d.n && diag_same(&d.dg[static_cast<std::size_t>(d.match)], p)) {
            d.dg[static_cast<std::size_t>(d.match++)].node = p->node; /* not in the data: the entry a grab by point goes to */
            return;
        }
        diag_end_replay(d, d.match);
    }
    if (static_cast<size_t>(d.n) == d.dg.size()) {
        try {
            d.dg.resize(d.dg.empty() ? 1024 : 2 * d.dg.size());
        } catch (...) {
            xpp_out_of_memory("keeping the AUTO diagram");
        }
    }
    d.dg[static_cast<std::size_t>(d.n++)] = *p;
}

namespace {

/* the AUTO diagram's points, to 7 significant digits (buf_num, 3-arg, is
   the shared JSON number writer: json_number.h) */
void diag_num(Buf *b, double v) { buf_num(b, v, 7); }

/* an event's start: its op and view */
void diag_head(Buf *b, const char *op, int v) { buf_format(b, "{{\"ev\":\"diagram\",\"op\":\"{}\",\"view\":{:d}", op, v); }

void diag_axes(Buf *b, const ViewData &d)
{
    BUF_LIT(b, ",\"xmin\":");
    buf_num(b, d.ax.xmin, 17);
    BUF_LIT(b, ",\"xmax\":");
    buf_num(b, d.ax.xmax, 17);
    BUF_LIT(b, ",\"ymin\":");
    buf_num(b, d.ax.ymin, 17);
    BUF_LIT(b, ",\"ymax\":");
    buf_num(b, d.ax.ymax, 17);
    buf_format(b, ",\"x0\":{:d},\"y0\":{:d},\"wid\":{:d},\"hgt\":{:d},\"plot\":{:d},\"xlabel\":", d.ax.x0, d.ax.y0,
               d.ax.wid, d.ax.hgt, d.ax.plot);
    buf_str(b, d.ax.xlabel.c_str());
    BUF_LIT(b, ",\"ylabel\":");
    buf_str(b, d.ax.ylabel.c_str());
}

/* points i..j of dg as one run: they share branch, kind and style, and
   their point numbers count up by one */
void diag_run(Buf *b, const std::vector<XppDiagPoint> &dg, int i, int j)
{
    const auto at = [&dg](int k) -> const XppDiagPoint & { return dg[static_cast<std::size_t>(k)]; };
    const XppDiagPoint *p = &at(i);
    int k, two = 0, nlab = 0;
    buf_format(b, "{{\"br\":{:d},\"pt\":{:d},\"ty\":{:d},\"d\":{:d},\"c\":{:d},\"lw\":{:d}", std::abs(p->ibr), std::abs(p->pt), p->type,
               p->draw, p->color, p->lw);
    if (p->flag2) buf_format(b, ",\"f2\":{:d}", p->flag2);
    if (p->newseg) BUF_LIT(b, ",\"new\":1");
    if (p->from) buf_format(b, ",\"from\":{:d}", p->from);
    BUF_LIT(b, ",\"x\":[");
    for (k = i; k <= j; k++) {
        if (k > i) BUF_LIT(b, ",");
        diag_num(b, at(k).x);
        if (at(k).y2 != at(k).y1 && !std::isnan(at(k).y1)) two = 1;
        if (at(k).lab) nlab++;
    }
    BUF_LIT(b, "],\"y\":[");
    for (k = i; k <= j; k++) {
        if (k > i) BUF_LIT(b, ",");
        diag_num(b, at(k).y1);
    }
    BUF_LIT(b, "]");
    if (two) {
        BUF_LIT(b, ",\"y2\":[");
        for (k = i; k <= j; k++) {
            if (k > i) BUF_LIT(b, ",");
            diag_num(b, at(k).y2);
        }
        BUF_LIT(b, "]");
    }
    if (nlab) {
        BUF_LIT(b, ",\"lab\":[");
        for (k = i, nlab = 0; k <= j; k++) {
            if (!at(k).lab) continue;
            const char *t = auto_bif_sym(at(k).itp);
            while (*t == ' ') t++;
            buf_format(b, "{}[{:d},{:d},", nlab++ ? "," : "", k - i, at(k).lab);
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
    const ViewData &d = active_data();
    for (int i = d.client - 1; i >= 0; i--)
        if (d.dg[static_cast<std::size_t>(i)].node == node) return i;
    return -1;
}

namespace {

/* the client's point i of the active view, NULL when it has none */
const XppDiagPoint *diag_client_point(int i)
{
    const ViewData &d = active_data();
    return i >= 0 && i < d.client ? &d.dg[static_cast<std::size_t>(i)] : nullptr;
}

/* the number of points each view holds (Clear: the branches so far are the earlier ones) */
int diag_points_held(void) { return active_data().n; }

/* b continues a's run */
int diag_joins(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return !b->newseg && !b->from && std::abs(a->ibr) == std::abs(b->ibr) && std::abs(b->pt) == std::abs(a->pt) + 1 && a->type == b->type &&
           a->draw == b->draw && a->color == b->color && a->lw == b->lw && a->flag2 == b->flag2;
}

/* what view v's client list lacks */
void diag_flush_view(int v, ViewData &d, int final)
{
    Buf b;
    if (d.replay) {
        if (d.match == d.n) diag_end_replay(d, d.n); /* all agreed; more points are new ones */
        else if (final) diag_end_replay(d, d.match);
        else return; /* still replaying: nothing is known yet */
    }
    if (d.dirty < d.client) {
        diag_head(&b, "reset", v);
        buf_format(&b, ",\"keep\":{:d}", d.dirty);
        diag_axes(&b, d);
        BUF_LIT(&b, "}");
        out_line(b.s.data(), b.s.size());
        b.s.clear();
        d.client = d.dirty;
        d.axes = 0;
    } else if (d.axes) {
        diag_head(&b, "axes", v);
        diag_axes(&b, d);
        BUF_LIT(&b, "}");
        out_line(b.s.data(), b.s.size());
        b.s.clear();
        d.axes = 0;
    }
    /* the points in events of some 60 kB */
    while (d.client < d.n) {
        int i = d.client, j;
        diag_head(&b, "add", v);
        buf_format(&b, ",\"from\":{:d},\"runs\":[", d.client);
        while (i < d.n && b.s.size() < 60000) {
            for (j = i; j + 1 < d.n && j - i < 2000 && diag_joins(&d.dg[static_cast<std::size_t>(j)], &d.dg[static_cast<std::size_t>(j + 1)]); j++) {
            }
            if (i > d.client) BUF_LIT(&b, ",");
            diag_run(&b, d.dg, i, j);
            i = j + 1;
        }
        BUF_LIT(&b, "]}");
        out_line(b.s.data(), b.s.size());
        b.s.clear();
        d.client = i;
    }
    d.dirty = d.n;
}

} // namespace

/* send what the client does not have yet. final: the command ends or asks
   something, so a replay that has not been completed never will be */
void diag_flush(const xpp::Session &s, int final)
{
    const int n = static_cast<int>(s.auto_state.views.size());
    if (n != client_views && s.auto_state.bifur.exist) {
        /* the lists of views no longer there go on both sides; a new one
           starts empty on both */
        const std::string e = xpp::format("{{\"ev\":\"diagram\",\"op\":\"views\",\"n\":{:d}}}", n);
        out_line(e.data(), e.size());
        try {
            views.resize(static_cast<std::size_t>(n));
        } catch (...) {
            xpp_out_of_memory("keeping the AUTO diagram");
        }
        client_views = n;
    }
    for (int v = 0; v < static_cast<int>(views.size()) && v < client_views; v++)
        diag_flush_view(v, views[static_cast<std::size_t>(v)], final);
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

/* ---- what the page displays of the diagram: hidden branches, the views' zoom (display_state.h) ----
   The `autoview` event, sent with the autoinfo subscription when it changed:
   the points before `earlier` are the branches computed before Clear
   (hidden unless `show`), the active view and each view's zoom. A zoom
   belongs to the axes it was made at: other ones drop it. */

namespace {

bool av_on, av_valid;
std::string av_sent;

void av_range(std::string &o, const xpp::AxisRange &r)
{
    if (!r.set) {
        o += "null";
        return;
    }
    o += '[';
    json_append_number_shortest(o, r.lo);
    o += ',';
    json_append_number_shortest(o, r.hi);
    o += ']';
}

} // namespace

void auto_view_subscribe(int on)
{
    av_on = on != 0;
    av_valid = false;
}

void auto_view_update(xpp::Session &s)
{
    if (!av_on) return;
    xpp::AutoView &v = s.auto_view;
    if (v.earlier > diagram_count(s.diagram)) v.earlier = diagram_count(s.diagram); /* a diagram that holds fewer points than Clear hid (File/Reset diagram) */
    if (v.earlier == 0) v.show_earlier = false;
    std::string o = "{\"ev\":\"autoview\",\"earlier\":" + std::to_string(v.earlier) + ",\"show\":" +
                    (v.show_earlier ? "1" : "0") + ",\"active\":" + std::to_string(s.auto_state.active_view) + ",\"views\":[";
    bool first = true;
    for (AutoDiagramView &w : s.auto_state.views) {
        const std::array<double, 4> axes = {w.axes.xmin, w.axes.xmax, w.axes.ymin, w.axes.ymax};
        if (w.zoom_seen && axes != w.zoom_axes) w.zoom = xpp::Zoom();
        w.zoom_seen = true;
        w.zoom_axes = axes;
        o += first ? "{\"zoom\":{\"x\":" : ",{\"zoom\":{\"x\":";
        first = false;
        av_range(o, w.zoom.x);
        o += ",\"y\":";
        av_range(o, w.zoom.y);
        o += "}}";
    }
    o += "]}";
    if (av_valid && o == av_sent) return;
    out_line(o.data(), o.size());
    av_sent.swap(o);
    av_valid = true;
}

/* a reconnected client has a blank diagram, and no data: all of it again,
   starting with a reset */
void auto_redraw_for_client(xpp::Session &s)
{
    if (!s.auto_state.bifur.exist) return;
    client_views = 1; /* a new client holds one empty view */
    views.resize(1);
    ViewData &d = views.front();
    d.dirty = 0;
    d.client = d.n > 0 ? d.n : 1;
    redraw_diagram(s);
}

/* ---- AUTO window --------------------------------------------------------------- */

void j_auto_make_window(const char *wname, const char *)
{
    xpp::Session &s = xpp::session();
    s.auto_state.bifur.hgt = 20 * text_metrics.big_height;
    s.auto_state.bifur.wid = 67 * text_metrics.big_width;
    s.auto_state.bifur.x0 = 10 * text_metrics.small_width;
    s.auto_state.bifur.y0 = 2 * text_metrics.small_height;
    s.auto_state.bifur.hinttxt = "hint";
    diag_forget();      /* a new window has no data */
    auto_data_forget(); /* nor an info strip or a stability circle */
    send_window("create", WIN_AUTO, s.auto_state.bifur.wid + 12 * text_metrics.small_width, s.auto_state.bifur.hgt + 4 * text_metrics.small_height, wname);
    draw_bif_axes(s);
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
    std::array<int, 4> v;
    if (!mouse_ask(WIN_AUTO, "rubber", flag, v)) return 0;
    *i1 = v[0]; *j1 = v[1]; *i2 = v[2]; *j2 = v[3];
    return 1;
}

namespace {

/* a grab answer's key after its point ({"point":i,"key":"Return"}): the
   point first, then the key without asking again */
int grab_key_after;

} // namespace

int j_auto_grab_event(int *x, int *y)
{
    Buf b;
    std::string k;
    const char *jp;
    int id;
    if (grab_key_after) {
        id = grab_key_after;
        grab_key_after = 0;
        return id;
    }
    id = ask_begin(&b, "grab");
    buf_format(&b, ",\"win\":{:d}", WIN_AUTO);
    if (!ask_wait(&b, id)) return ESC;
    /* a point of the diagram data by its index: the cursor goes to that
       point's entry (docs/protocol.md "Grab by point"); one the data do not
       have, or whose entry AUTO no longer has (the data are an old drawing
       until reDraw), is ignored, and so is its key */
    if ((jp = js_find(ask_answer(), "point")) != NULL) {
        double i = js_num(jp, -1);
        const XppDiagPoint *p = i >= 0 && i < INT_MAX ? diag_client_point(static_cast<int>(i)) : NULL;
        *x = p && diagram_has(xpp::session().diagram, p->node, p->ibr, p->pt) ? p->node : -1;
        if (*x >= 0 && get_string(ask_answer(), "key", k, 32)) grab_key_after = key_code(k.c_str());
        return XPP_AUTO_NODE;
    }
    if (get_string(ask_answer(), "key", k, 32)) return key_code(k.c_str());
    answer_point(WIN_AUTO, 0, x, y);
    return XPP_AUTO_CLICK;
}
void j_auto_show_hint(void) { send_simple("message", "auto", xpp::session().auto_state.bifur.hinttxt.c_str()); }

/* AUTO Axes/Scroll: drag the diagram (auto_x11.c x11_auto_scroll_window) */
void j_auto_scroll_window(void)
{
    xpp::Session &s = xpp::session();
    int i, j, t, i0 = 0, j0 = 0, state = 0;
    float xlo = s.auto_state.axes().xmin, ylo = s.auto_state.axes().ymin, xhi = s.auto_state.axes().xmax, yhi = s.auto_state.axes().ymax, dx = 0, dy = 0;
    send_simple("message", "auto", "Drag the diagram to scroll it; any key ends");
    while ((t = ask_drag(WIN_AUTO, &i, &j)) != 0) {
        if (t == 1 && state == 0) {
            i0 = i;
            j0 = j;
            state = 1;
        } else if (t == 2 && state == 1) {
            dx = static_cast<float>(i0 - i) * (xhi - xlo) / static_cast<float>(s.auto_state.bifur.wid);
            dy = static_cast<float>(j - j0) * (yhi - ylo) / static_cast<float>(s.auto_state.bifur.hgt);
            auto_update_view(s, xlo + dx, xhi + dx, ylo + dy, yhi + dy);
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

namespace {

/* the "numerics", "pars", "axes" and "marks" of {"cmd":"auto","op":"set",...}
   into s; 0 with why when one is not what docs/protocol.md says */
int read_auto_set(const char *line, AutoSettingsSet &s, std::string &why)
{
    const char *num = js_find(line, "numerics"), *pars = js_find(line, "pars"), *axes = js_find(line, "axes"),
               *marks = js_find(line, "marks"), *v;
    int i;
    for (i = 0; num && *num == '{' && i < AUTO_NUM_N; i++) {
        if (!(v = js_find(num, auto_settings_num_key(i)))) continue;
        if (!js_number(v, &s.num[i])) {
            why = xpp::format("{} must be a number", auto_settings_num_label(i));
            return 0;
        }
        s.has_num[i] = 1;
    }
    if (pars && *pars == '[') {
        for (i = 0; (v = js_elem(pars, i)) != NULL; i++) {
            if (i >= AUTO_SETTINGS_PARS || !js_string(v, s.pars[i])) {
                why = xpp::format("AUTO's parameters must be a list of at most {} names", AUTO_SETTINGS_PARS);
                return 0;
            }
        }
        s.npars = i;
    }
    if (axes && *axes == '{') {
        static const char *const range[4] = {"xmin", "xmax", "ymin", "ymax"};
        double z;
        if ((v = js_find(axes, "plot")) != NULL) {
            if (!js_number(v, &z) || z < INT_MIN || z > INT_MAX || z != std::floor(z)) {
                why = "the plot type must be a whole number";
                return 0;
            }
            s.has_plot = 1;
            s.plot = static_cast<int>(z);
        }
        get_string(axes, "var", s.var);
        get_string(axes, "par1", s.par1);
        get_string(axes, "par2", s.par2);
        for (i = 0; i < 4; i++) {
            if (!(v = js_find(axes, range[i]))) continue;
            if (!js_number(v, &s.range[i])) {
                why = xpp::format("{}{} must be a number", static_cast<char>(range[i][0] - 32), range[i] + 1);
                return 0;
            }
            s.has_range[i] = 1;
        }
        s.fit = get_num(axes, "fit", 0) != 0;
        if ((v = js_find(axes, "view")) != NULL) {
            if (!js_number(v, &z) || z < 0 || z > INT_MAX || z != std::floor(z)) {
                why = "the view must be a view's number";
                return 0;
            }
            s.view = static_cast<int>(z);
        }
    }
    if (marks && *marks == '[') {
        for (i = 0; (v = js_elem(marks, i)) != NULL; i++) {
            if (i >= AUTO_SETTINGS_MARKS || *v != '[' || !js_string(js_elem(v, 0), s.mark_name[i])
                || !js_number(js_elem(v, 1), &s.mark_value[i])) {
                why = xpp::format("Mark values must be a list of at most {} pairs [name, number]", AUTO_SETTINGS_MARKS);
                return 0;
            }
        }
        s.nmarks = i;
    }
    return 1;
}

/* {"cmd":"auto","op":"set",...}: AUTO's settings, checked and set all
   together or not at all; a refusal is an error message */
void auto_set_command(xpp::Session &s, const char *line)
{
    AutoSettingsSet set;
    std::string why;
    if (!read_auto_set(line, set, why) || auto_settings_apply(s, set, why) != 0)
        j_err_msg(xpp::format("AUTO settings: {}", why).c_str());
}

} // namespace

/* {"cmd":"auto","op":...}: what the AUTO window's keys (menu_auto_window)
   do not say: a grab by label or by type and index, the settings, a click
   on the diagram, the window closed */
void auto_command(xpp::Session &s, const char *line)
{
    std::string o;
    get_string(line, "op", o, 16);
    if (o == "grab") {
        /* a label, or a type and index ("the 2nd HB"), grabs that stored
           point directly, exactly as the interactive grab ending with
           Return on it would (docs/protocol.md "Grab by label"); the
           interactive grab is the window's key g */
        const char *jl = js_find(line, "label");
        std::string type;
        if (jl != NULL) {
            int lab = static_cast<int>(js_num(jl, 0));
            if (!auto_grab_label(s, lab))
                j_err_msg(xpp::format("Grab: no point labelled {}", lab).c_str());
        } else if (get_string(line, "type", type, 8) && js_find(line, "index") != NULL) {
            int idx = get_int(line, "index", 0);
            if (!auto_grab_type_index(s, type.c_str(), idx))
                j_err_msg(xpp::format("Grab: no {} point number {}", type, idx).c_str());
        } else j_err_msg("Grab: give a label, or a type and index");
    }
    else if (o == "set") auto_set_command(s, line);
    else if (o == "display") {
        /* the zoom shown in a view of the diagram (the active one unless
           `view` names another), and (with `show`) whether the branches
           before Clear are drawn */
        xpp::AutoView &v = s.auto_view;
        const int k = get_int(line, "view", s.auto_state.active_view);
        if (k < 0 || k >= static_cast<int>(s.auto_state.views.size())) {
            j_err_msg(xpp::format("auto display: no view {}", k).c_str());
            return;
        }
        xpp::Zoom z = s.auto_state.views[static_cast<std::size_t>(k)].zoom;
        if (get_range(line, "x", z.x) < 0 || get_range(line, "y", z.y) < 0) {
            j_err_msg("auto display: x and y are [low, high] with low below high, or null");
            return;
        }
        s.auto_state.views[static_cast<std::size_t>(k)].zoom = z;
        const char *js = js_find(line, "show");
        if (js) v.show_earlier = js_num(js, 0) != 0 && v.earlier > 0;
    }
    else if (o == "view") {
        /* the views of the diagram (W50): a new one, one closed, the active one */
        const int n = static_cast<int>(s.auto_state.views.size());
        if (js_find(line, "new")) auto_new_view(s);
        else if (js_find(line, "close")) {
            const int k = get_int(line, "close", -1);
            if (k < 0 || k >= n) j_err_msg(xpp::format("auto view: no view {}", k).c_str());
            else if (!auto_close_view(s, k)) j_err_msg("auto view: the last view stays open");
        } else if (js_find(line, "active")) {
            const int k = get_int(line, "active", -1);
            if (!auto_activate_view(s, k)) j_err_msg(xpp::format("auto view: no view {}", k).c_str());
        } else j_err_msg("auto view: give new, close or active");
    }
    else if (o == "point") {
        /* in the diagram's quantities, or a pixel of window 101 */
        const char *jx = js_find(line, "xd"), *jy = js_find(line, "yd");
        if (!s.auto_state.bifur.exist) return;
        if (jx && jy) auto_point_xy(s, js_num(jx, 0), js_num(jy, 0));
        else auto_motion_xy(s, get_int(line, "x", 0), get_int(line, "y", 0));
    }
    else if (o == "close") {
        if (!s.auto_state.bifur.exist) return;
        s.auto_state.bifur.exist = 0; /* auto_x11.c auto_kill; File/Auto opens it again */
        send_window("destroy", WIN_AUTO, 0, 0, NULL);
        diag_forget();
        auto_data_forget();
        s.auto_view = xpp::AutoView();
        for (AutoDiagramView &w : s.auto_state.views) w.zoom = xpp::Zoom(); /* the views stay, shown whole */
    }
    else j_err_msg(xpp::format("Unknown auto op {}", o).c_str());
}

/* a key of the AUTO window (menu_auto_window) */
void auto_key(xpp::Session &s, int ch)
{
    switch (xpp_menu_index(&menu_auto_window, ch)) {
    case AK_PARAM: auto_params(s); break;
    case AK_AXES: auto_plot_par(s); break;
    case AK_NUMERICS: auto_num_par(s); break;
    case AK_RUN: auto_run(s); break;
    case AK_GRAB: auto_grab(s); break;
    case AK_USR: auto_per_par(s); break;
    case AK_CLEAR:
        /* the branches so far are the earlier ones, hidden until shown again */
        s.auto_view.earlier = diag_points_held();
        s.auto_view.show_earlier = false;
        draw_bif_axes(s);
        break;
    case AK_REDRAW: redraw_diagram(s); break;
    case AK_FILE: auto_file(s); break;
    }
}

} // namespace xpp::json
