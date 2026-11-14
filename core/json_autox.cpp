/* The autoX window: its prompts and mouse, the diagram's points as data
   ("diagram" events), and autoX's settings as data ({"cmd":"autox","op":"set"},
   autox_settings.h). */
#include <map>
#include "ui_json_internal.h"
#include "session.h"
#include "autox_state.h"
#include "display_state.h"
#include "many_pops.h"
#include "xpp_job.h"
#include "mykeydef.h"
#include "diagram.h"
#include "jacobian.h"
#include "autox_data.h"
#include "autox_settings.h"
#include "autox_script.h"
#include "json_number.h"
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include "model.h"
#include "json_reader.h"
#include "autox_axis.h"
#include "autox_codes.h"
#include "autox_nox.h"
#include "colormap.h"
#include "data_event.h"
#include "event_registry.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "xpp_types.h"
#include "xpp_ui.h"

namespace xpp::json {

/* ---- autoX diagram data --------------------------------------------------------
   The points of the diagram go out as data ("diagram"
   events, docs/protocol.md), so that the client can zoom, pan and show a
   point under the mouse without a round trip. Each view of the diagram
   (W50, AutoxState::views) has its own list, every event naming its view:
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
/* the first size of a client's copy of the diagram, doubled as it fills */
constexpr std::size_t diagram_copy_initial = 1024;
/* an "add" event's size, in bytes, past which the next points go in a new
   event: lines the page parses in one piece */
constexpr std::size_t diagram_event_bytes = 60000;
/* the most points in one run of an "add" event, so a long branch splits */
constexpr int diagram_run_points = 2000;

/* view v's list the client holds, made when the view is new */
ClientDiagramView &view_data(int v)
{
    std::vector<ClientDiagramView> &views = session.client_diagram;
    try {
        if (static_cast<std::size_t>(v) >= views.size()) views.resize(static_cast<std::size_t>(v) + 1);
    } catch (...) {
        xpp::out_of_memory("keeping the autoX diagram");
    }
    return views[static_cast<std::size_t>(v)];
}

/* the list the grab and the info strip's point index go by: the active
   view's (every view holds the same points in the same order) */
ClientDiagramView &active_data(const xpp::Session &s) { return view_data(s.autox_state->active_view); }

} // namespace

/* the client has nothing: a new window, or one it no longer holds */
void diag_forget(void)
{
    session.client_view_names.clear();
    session.client_diagram.assign(1, ClientDiagramView());
    session.client_views = 1;
    session.grab_key_after = 0;
    session.autoxinfo.want(session.autoxinfo.subscribed());
    session.autoxsettings.want(session.autoxsettings.subscribed());
    session.autoxview.want(session.autoxview.subscribed());
    session.autoxscript.want(session.autoxscript.subscribed());
}

namespace {

int diag_same(ClientDiagramView &view, const XppDiagPoint *a, const XppDiagPoint *b,
              const std::shared_ptr<const DiagramBranch> &old_branch,
              const std::shared_ptr<const DiagramBranch> &branch)
{
    if(old_branch!=branch){
        if(!old_branch || !branch || old_branch->parameters.size()!=branch->parameters.size())return 0;
        // A replay compares complete constants once per branch pair, never once per point.
        auto [match,added]=view.branch_matches[old_branch].try_emplace(branch,false);
        if(added)match->second=branch->parameters.empty() || std::memcmp(old_branch->parameters.data(),branch->parameters.data(),branch->parameters.size()*sizeof(double))==0;
        if(!match->second)return 0;
    }
    return a->ibr == b->ibr && a->pt == b->pt && a->itp == b->itp && a->lab == b->lab && a->type == b->type &&
           a->flag2 == b->flag2 && a->draw == b->draw && a->newseg == b->newseg && a->color == b->color &&
           a->lw == b->lw && a->from == b->from && a->job == b->job && a->live == b->live && a->jacobian == b->jacobian && std::memcmp(&a->x, &b->x, sizeof a->x) == 0 && std::memcmp(&a->y1, &b->y1, sizeof a->y1) == 0 &&
           std::memcmp(&a->y2, &b->y2, sizeof a->y2) == 0;
}

/* the list is its first k points: the client must drop its points from k on (a flush says so) */
void diag_drop_from(ClientDiagramView &d, int k)
{
    d.n = k;
    if (k < d.dirty) d.dirty = k;
}

/* the replay is over: the list is its first k points */
void diag_end_replay(ClientDiagramView &d, int k)
{
    d.branch_matches.clear();
    d.replay = false;
    diag_drop_from(d, k);
}

} // namespace

void j_autox_diagram(xpp::Session &s, int view, const XppDiagPoint *p)
{
    if (!session.diagram_subscribed) return;
    ClientDiagramView &d = view_data(view);
    if (!p) {
        const AUTOAX &a = s.autox_state->views[static_cast<std::size_t>(view)].axes;
        d.ax.xmin = a.xmin;
        d.ax.xmax = a.xmax;
        d.ax.ymin = a.ymin;
        d.ax.ymax = a.ymax;
        d.ax.x0 = s.autox_state->bifur.x0;
        d.ax.y0 = s.autox_state->bifur.y0;
        d.ax.wid = s.autox_state->bifur.wid;
        d.ax.hgt = s.autox_state->bifur.hgt;
        d.ax.plot = a.plot;
        get_autox_str(s, a, d.ax.xlabel, d.ax.ylabel);
        d.axes = true;
        d.replay = true;
        d.match = 0;
        return;
    }
    const auto branch=p->node>=0 && static_cast<std::size_t>(p->node)<s.diagram->points.size()
        ? s.diagram->points[p->node].branch : std::shared_ptr<const DiagramBranch>();
    if (d.replay) {
        if (d.match < d.n && diag_same(d,&d.dg[static_cast<std::size_t>(d.match)], p,d.branches[d.match],branch)) {
            d.branches[d.match]=branch;
            d.dg[static_cast<std::size_t>(d.match++)].node = p->node; /* not in the data: the entry a grab by point goes to */
            return;
        }
        diag_end_replay(d, d.match);
    }
    if (p->live && d.live_from < 0) d.live_from = d.n;
    if (static_cast<size_t>(d.n) == d.dg.size()) {
        try {
            d.dg.resize(d.dg.empty() ? diagram_copy_initial : 2 * d.dg.size());
            d.branches.resize(d.dg.size());
        } catch (...) {
            xpp::out_of_memory("keeping the autoX diagram");
        }
    }
    d.branches[d.n]=branch;
    d.dg[static_cast<std::size_t>(d.n++)] = *p;
}

void j_autox_live_drop(xpp::Session &)
{
    for (ClientDiagramView &d : session.client_diagram) {
        if (d.live_from < 0) continue;
        diag_drop_from(d, d.live_from);
        d.live_from = -1;
    }
}

/* the index in the data the client holds of autoX's diagram entry `node`
   (autox_data.h); the latest when a redraw of other axes left two */
int diag_point_of_node(const xpp::Session &s, int node)
{
    const ClientDiagramView &d = active_data(s);
    for (int i = d.client - 1; i >= 0; i--)
        if (d.dg[static_cast<std::size_t>(i)].node == node) return i;
    return -1;
}

namespace {

/* the autoX diagram's points, to 7 significant digits (buf_num, 3-arg, is
   the shared JSON number writer: json_number.h) */
void diag_num(std::string &b, double v) { buf_num(b, v, 7); }

/* an event's start: its op and view */
void diag_head(std::string &b,const char *op,const xpp::Session &s,int v){buf_format(b,"{},\"op\":\"{}\",\"view\":",xpp::events::open(xpp::events::AUTOX_DIAGRAM),op);buf_str(b,s.autox_state->views[v].name);}

void diag_axes(std::string &b, const ClientDiagramView &d)
{
    BUF_LIT(b, ",\"xmin\":");
    buf_num(b, d.ax.xmin, 17);
    BUF_LIT(b, ",\"xmax\":");
    buf_num(b, d.ax.xmax, 17);
    BUF_LIT(b, ",\"ymin\":");
    buf_num(b, d.ax.ymin, 17);
    BUF_LIT(b, ",\"ymax\":");
    buf_num(b, d.ax.ymax, 17);
    buf_format(b, ",\"x0\":{:d},\"y0\":{:d},\"wid\":{:d},\"hgt\":{:d},\"plot\":\"{}\",\"xlabel\":", d.ax.x0, d.ax.y0,
               d.ax.wid, d.ax.hgt, autox_projection_name(d.ax.plot));
    buf_str(b, d.ax.xlabel.c_str());
    BUF_LIT(b, ",\"ylabel\":");
    buf_str(b, d.ax.ylabel.c_str());
}

/* points i..j of dg as one run: they share branch, kind and style, and
   their point numbers count up by one */
void diag_run(const xpp::Session &s, std::string &b, const std::vector<XppDiagPoint> &dg, int i, int j)
{
    const auto at = [&dg](int k) -> const XppDiagPoint & { return dg[static_cast<std::size_t>(k)]; };
    const XppDiagPoint *p = &at(i);
    int k, two = 0, nlab = 0;
    buf_format(b, "{{\"br\":{:d},\"pt\":{:d},\"ty\":{:d},\"d\":{:d},\"c\":{},\"lw\":{:d}", std::abs(p->ibr), std::abs(p->pt), xpp::diagram_point_type_code(p->type),
               p->draw, xpp::quoted_color(p->color), p->lw);
    buf_format(b, ",\"stable\":{},\"periodic\":{}", xpp::point_is_stable(p->type), xpp::point_is_periodic(p->type));
    if (p->flag2!=no_calculation) buf_format(b, ",\"f2\":{:d}", static_cast<int>(p->flag2));
    if (p->newseg) BUF_LIT(b, ",\"new\":1");
    if (p->from) buf_format(b, ",\"from\":{:d}", p->from);
    if (p->job) buf_format(b, ",\"job\":{:d}", p->job);
    if (p->live) buf_format(b, ",\"live\":{:d}", p->live);
    buf_format(b, ",\"jac\":\"{}\"", xpp::jacobian_name(p->jacobian));
    /* A branch's complete constants, once at its first point, in model order.
       Continued point coordinates are resolved from this same stored branch. */
    if (p->newseg && p->node >= 0 && static_cast<std::size_t>(p->node) < s.diagram->points.size()) {
        const DIAGRAM &point=s.diagram->points[p->node];
        BUF_LIT(b, ",\"parameters\":[");
        for (std::size_t index=0;index<point.branch->parameters.size();index++) {
            if(index)BUF_LIT(b,",");
            BUF_LIT(b,"[");
            buf_str(b,s.model().upar_names[index].c_str());
            BUF_LIT(b,",");
            buf_num(b,point.branch->parameters[index],17);
            BUF_LIT(b,"]");
        }
        BUF_LIT(b,"]");
    }
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
            const std::string_view t = autox_point_kind_name(at(k).itp.kind);
            buf_format(b, "{}[{:d},{:d},", nlab++ ? "," : "", k - i, p->live ? 0 : at(k).lab); /* a live point's number is its job's own: typed, not numbered */
            buf_str(b, t);
            BUF_LIT(b, "]");
        }
        BUF_LIT(b, "]");
    }
    BUF_LIT(b, "}");
}

/* the client's point i of the active view, NULL when it has none */
const XppDiagPoint *diag_client_point(const xpp::Session &s, int i)
{
    const ClientDiagramView &d = active_data(s);
    return i >= 0 && i < d.client ? &d.dg[static_cast<std::size_t>(i)] : nullptr;
}

} // namespace

/* the number of points each view holds (Clear: the branches so far are the earlier ones) */
int j_autox_points_held(xpp::Session &s) { return active_data(s).n; }

namespace {

/* b continues a's run */
int diag_joins(const XppDiagPoint *a, const XppDiagPoint *b)
{
    return !b->newseg && !b->from && std::abs(a->ibr) == std::abs(b->ibr) && std::abs(b->pt) == std::abs(a->pt) + 1 && a->type == b->type &&
           a->draw == b->draw && a->color == b->color && a->lw == b->lw && a->flag2 == b->flag2 && a->job == b->job && a->live == b->live && a->jacobian == b->jacobian;
}

/* what view v's client list lacks */
void diag_flush_view(const xpp::Session &s, int v, ClientDiagramView &d, int final)
{
    std::string b;
    if (d.replay) {
        if (d.match == d.n) diag_end_replay(d, d.n); /* all agreed; more points are new ones */
        else if (final) diag_end_replay(d, d.match);
        else return; /* still replaying: nothing is known yet */
    }
    if (d.dirty < d.client) {
        diag_head(b, "reset", s,v);
        buf_format(b, ",\"keep\":{:d}", d.dirty);
        diag_axes(b, d);
        BUF_LIT(b, "}");
        out_line(b.data(), b.size());
        b.clear();
        d.client = d.dirty;
        d.axes = false;
    } else if (d.axes) {
        diag_head(b, "axes", s,v);
        diag_axes(b, d);
        BUF_LIT(b, "}");
        out_line(b.data(), b.size());
        b.clear();
        d.axes = false;
    }
    /* the points, in events of some diagram_event_bytes */
    while (d.client < d.n) {
        int i = d.client, j;
        diag_head(b, "add", s,v);
        buf_format(b, ",\"from\":{:d},\"runs\":[", d.client);
        while (i < d.n && b.size() < diagram_event_bytes) {
            for (j = i; j + 1 < d.n && j - i < diagram_run_points && diag_joins(&d.dg[static_cast<std::size_t>(j)], &d.dg[static_cast<std::size_t>(j + 1)]); j++) {
            }
            if (i > d.client) BUF_LIT(b, ",");
            diag_run(s, b, d.dg, i, j);
            i = j + 1;
        }
        BUF_LIT(b, "]}");
        out_line(b.data(), b.size());
        b.clear();
        d.client = i;
    }
    d.dirty = d.n;
}

} // namespace

/* send what the client does not have yet. final: the command ends or asks
   something, so a replay that has not been completed never will be */
void diag_flush(const xpp::Session &s, int final)
{
    if (!session.diagram_subscribed) return;
    const int n = static_cast<int>(s.autox_state->views.size());
    std::vector<std::string> names;for(const auto &view:s.autox_state->views)names.push_back(view.name);
    if (names != session.client_view_names && s.autox_state->bifur.exist) {
        /* the lists of views no longer there go on both sides; a new one
           starts empty on both */
        std::string e=xpp::format("{},\"op\":\"views\",\"n\":{},\"names\":[",xpp::events::open(xpp::events::AUTOX_DIAGRAM),n);
        for(std::size_t k=0;k<names.size();++k){if(k)e+=',';xpp::json_append_string(e,names[k]);}e+="]}";
        session.client_view_names=names;
        out_line(e.data(), e.size());
        try {
            session.client_diagram.resize(static_cast<std::size_t>(n));
        } catch (...) {
            xpp::out_of_memory("keeping the autoX diagram");
        }
        session.client_views = n;
    }
    for (int v = 0; v < static_cast<int>(session.client_diagram.size()) && v < session.client_views; v++)
        diag_flush_view(s, v, session.client_diagram[static_cast<std::size_t>(v)], final);
}

/* autoX's refreshdisplay() after every point: a few frames a second, not a
   flush per point. The end of a command and every ask flush in full, so
   the last point of a run and a grab's circle are never held back. */
void j_autox_refresh(xpp::Session &s)
{
    if (xpp::every(session.autox_refresh_last, xpp::INPUT_POLL_SECONDS)) {
        json_flush();
        autox_data_update(s, 0);
    }
}

/* ---- what the page displays of the diagram: hidden branches, the views' zoom (display_state.h) ----
   The `autoxview` event, sent with the autoxinfo subscription when it changed:
   the points before `earlier` are the branches computed before Clear
   (hidden unless `show`), the active view and each view's zoom. A zoom
   belongs to the axes it was made at: other ones drop it. */

namespace {

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

void autox_view_subscribe(int on)
{
    session.autoxview.want(on != 0);
}

std::string autox_view_event_line(const xpp::Session &s)
{
    const xpp::AutoxView &v = *s.autox_view;
    int earlier = v.earlier > diagram_count(*s.diagram) ? diagram_count(*s.diagram) : v.earlier;
    bool show_earlier = earlier == 0 ? false : v.show_earlier;
    std::string o = xpp::events::open(xpp::events::AUTOX_VIEW) + ",\"earlier\":" + std::to_string(earlier) + ",\"show\":" +
                    (show_earlier ? "1" : "0") + ",\"active\":";
    xpp::json_append_string(o,s.autox_state->views[s.autox_state->active_view].name);o+=",\"views\":[";
    bool first = true;
    for (const AutoxDiagramView &w : s.autox_state->views) {
        if(!first)o+=',';
        o+="{\"name\":";xpp::json_append_string(o,w.name);o+=",\"zoom\":{\"x\":";
        first = false;
        av_range(o, w.zoom.x);
        o += ",\"y\":";
        av_range(o, w.zoom.y);
        o += "}}";
    }
    o += "]}";
    return o;
}

void autox_view_update(xpp::Session &s)
{
    if (!session.autoxview.ready() || !session.autoxview.subscribed()) return;
    xpp::AutoxView &v = *s.autox_view;
    if (v.earlier > diagram_count(*s.diagram)) v.earlier = diagram_count(*s.diagram); /* a diagram that holds fewer points than Clear hid (File/Reset diagram) */
    if (v.earlier == 0) v.show_earlier = false;
    for (AutoxDiagramView &w : s.autox_state->views) {
        const std::array<double, 4> axes = {w.axes.xmin, w.axes.xmax, w.axes.ymin, w.axes.ymax};
        if (w.zoom_seen && axes != w.zoom_axes) w.zoom = xpp::Zoom();
        w.zoom_seen = true;
        w.zoom_axes = axes;
    }
    session.autoxview.update(s);
}

void autox_script_subscribe(int on)
{
    session.autoxscript.want(on != 0);
}

std::string autox_script_event_line(const xpp::Session &s)
{
    const xpp::AutoxScriptState &state = s.autox_state->script;
    std::string o = xpp::events::open(xpp::events::AUTOX_SCRIPT_RUNS);
    if (!state.script) return o + ",\"open\":false}";
    const xpp::AutoxScript &script = *state.script;
    o += ",\"open\":true,\"file\":";
    xpp::json_append_string(o, script.file);
    o += ",\"model\":";
    xpp::json_append_string(o, script.model);
    o += ",\"runs\":[";
    for (std::size_t i = 0; i < script.runs.size(); ++i) {
        const xpp::AutoxScriptRun &run = script.runs[i];
        if (i) o += ',';
        o += "{\"name\":";
        xpp::json_append_string(o, run.name);
        o += ",\"from\":";
        if (run.from) {
            o += "{\"run\":";
            xpp::json_append_string(o, run.from->run);
            o += ",\"type\":";
            xpp::json_append_string(o, run.from->type);
            o += ",\"index\":" + std::to_string(run.from->index) + "}";
        } else o += "null";
        o += xpp::autox_script_run_done(state, i) ? ",\"done\":true}" : ",\"done\":false}";
    }
    o += "]}";
    return o;
}

void autox_script_update(xpp::Session &s)
{
    session.autoxscript.update(s);
}

/* a reconnected client has a blank diagram, and no data: all of it again,
   starting with a reset */
void auto_redraw_for_client(xpp::Session &s)
{
    if (!s.autox_state->bifur.exist) return;
    session.client_views = 1; /* a new client holds one empty view */
    session.client_diagram.resize(1);
    ClientDiagramView &d = session.client_diagram.front();
    d.dirty = 0;
    d.client = d.n > 0 ? d.n : 1;
    redraw_diagram(s);
}

} // namespace xpp::json

namespace xpp {

void autox_data_subscribe(int on)
{
    json::session.autoxinfo.want(on != 0);
}

void autox_data_update(const Session &s, int final)
{
    if (!json::session.autoxinfo.ready() || !json::session.autoxinfo.subscribed()) return;
    if (!final && !xpp::every(json::session.autoxinfo_last, xpp::PROGRESS_SECONDS)) return;
    json::session.autoxinfo.update(s, json::diag_point_of_node(s, s.autox_state->shown.v.node));
}

void autox_settings_subscribe(const Session &s, int on)
{
    json::session.autoxsettings.subscribe(on != 0, s);
}

void autox_settings_update(const Session &s)
{
    json::session.autoxsettings.update(s);
}

} // namespace xpp

namespace xpp::json {

/* ---- autoX window --------------------------------------------------------------- */

void j_autox_make_window(xpp::Session &s, std::string_view wname, std::string_view)
{
    s.autox_state->bifur.hgt = 20 * text_metrics.big_height;
    s.autox_state->bifur.wid = 67 * text_metrics.big_width;
    s.autox_state->bifur.x0 = 10 * text_metrics.small_width;
    s.autox_state->bifur.y0 = 2 * text_metrics.small_height;
    s.autox_state->bifur.hinttxt = "hint";
    diag_forget();      /* a new window has no data */
    autox_data_forget(s); /* nor an info strip or a stability circle */
    send_window(xpp::events::WINDOW_CREATED,xpp::window_name(s,WIN_AUTOX), s.autox_state->bifur.wid + 12 * text_metrics.small_width, s.autox_state->bifur.hgt + 4 * text_metrics.small_height, wname,"diagram");
    draw_bif_axes(s);
}

void j_autox_jobs(const XppAutoxJobs &jobs)
{
    std::string b = xpp::events::open(xpp::events::AUTOX_JOBS);
    buf_format(b, ",\"running\":{:d},\"queued\":{:d},\"done\":{:d},\"jobs\":[", jobs.running, jobs.queued, jobs.done);
    for (std::size_t i = 0; i < jobs.each.size(); ++i) {
        const XppAutoxJob &job = jobs.each[i];
        buf_format(b, "{}{{\"live\":{:d},\"key\":", i ? "," : "", job.live);
        buf_key(b, job.key);
        buf_format(b, ",\"points\":{:d}}}", job.points);
    }
    BUF_LIT(b, "]}");
    send_buf(b);
}

/* the autox.joined event: the jobs that kept their points in the join, each with the branch its joined points carry */
void j_autox_joined(const std::vector<XppJoinedJob> &jobs)
{
    std::string b = xpp::events::open(xpp::events::AUTOX_JOINED);
    BUF_LIT(b, ",\"jobs\":[");
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        buf_format(b, "{}{{\"key\":", i ? "," : "");
        buf_key(b, jobs[i].key);
        buf_format(b, ",\"branch\":{:d}}}", jobs[i].branch);
    }
    BUF_LIT(b, "]}");
    send_buf(b);
}

int j_autox_check_abort(int *iflag)
{
    *iflag = 0;
    if (j_check_abort() == ESC) {
        *iflag = 1;
        return 0;
    }
    return 0;
}
/* A question about the diagram (a grab, a zoom box, a scroll) is answered on it: the views are shown */
static void show_views_for_ask(xpp::Session &s)
{
    for (AutoxDiagramView &view : s.autox_state->views) view.hidden = false;
}

int j_autox_grab_event(xpp::Session &s, int *x, int *y)
{
    std::string b;
    std::string k;
    const char *jp;
    int id;
    if (session.grab_key_after) {
        id = session.grab_key_after;
        session.grab_key_after = 0;
        return id;
    }
    show_views_for_ask(s);
    id = ask_begin(b, "grab");
    BUF_LIT(b,",\"win\":");buf_str(b,xpp::window_name(s,WIN_AUTOX));
    if (!ask_wait(b, id)) return ESC;
    /* a point of the diagram data by its index: the cursor goes to that
       point's entry (docs/protocol.md "Grab by point"); one the data do not
       have, or whose entry autoX no longer has (the data are an old drawing
       until reDraw), is ignored, and so is its key */
    if ((jp = js_find(ask_answer(), "point")) != NULL) {
        double i = get_num(ask_answer(), "point", -1);
        const XppDiagPoint *p = i >= 0 && i < INT_MAX ? diag_client_point(s, static_cast<int>(i)) : NULL;
        *x = p && diagram_has(*s.diagram, p->node, p->ibr, p->pt) ? p->node : -1;
        if (*x >= 0 && get_string(ask_answer(), "key", k)) session.grab_key_after = key_code(k.c_str());
        return XPP_AUTOX_NODE;
    }
    if (get_string(ask_answer(), "key", k)) return key_code(k.c_str());
    answer_point(s, WIN_AUTOX, 0, x, y);
    return XPP_AUTOX_CLICK;
}
void j_autox_show_hint(xpp::Session &s) { send_simple(xpp::events::SESSION_MESSAGE, "autox", s.autox_state->bifur.hinttxt.c_str()); }

} // namespace xpp::json
