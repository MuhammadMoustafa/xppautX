/* The plot windows as data: the "plots" and "series" events (plot_data.h,
   docs/protocol.md "The plot as data").

   "series" carries a window's curves as numbers: T and every storage column
   a curve plots, once each, the stored single-precision values printed with
   9 digits so they read back exactly, or base64 float32 (series_enc.h). One
   event per plot window whose data, curves or style changed since the last
   one it got. While an integration runs, the rows it stores go out as they
   come for the active window: {"op":"append","from":n,...}, at most ten a
   second, with the columns of that window's last full series; the other
   windows get their full series at the end. A full series still ends the
   command.

   "plots" lists the windows themselves; it is compared as text with the
   last one sent. */
#include <new>
#include "xpp_mem.h"
#include <cstdio>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "plot_data.h"
#include "session.h"
#include "xpp_util.h"
#include "series_enc.h"
#include "json_number.h"
#include "xpp_job.h"
#include "browse.h"

extern "C" {
}

namespace {

PlotDataEmit emit_line;
bool series_on, plots_on, series_f32;
unsigned long data_version; /* counts data changes */

/* what a window's series shows: equal signatures, the same event */
struct SeriesSig : xpp::PlotCurves {
    unsigned long win, version;
    int rows;
    bool operator==(const SeriesSig &) const = default;
};

/* what the client holds of each window (graph index): its last full
   series, the columns of it, and how many of its first rows still equal
   storage (appends continue from there) */
struct Sent {
    bool valid = false;
    SeriesSig sig;
    std::vector<int> cols;
    int held = 0;
};
Sent sent[MAXPOP];

int rows_seen;     /* storage rows when last seen: fewer next time means it started again */
int appended = -1; /* the graph index that got appends in this command */

/* the least time between two appends of a live run: about one a display
   frame (60 Hz), so the page extends the curve at every frame it draws
   (W82; it was 100 ms, ten a second, which the eye sees as steps). Each
   append carries only the rows stored since the last one: more appends
   cost the core a little more framing, not more data. */
constexpr double append_every = 1.0 / 60;

std::string plots_sent;
bool plots_valid;

/* ---- JSON text ---- */

void add_int(std::string &o, long v) { o += std::to_string(v); }

void emit(const std::string &s)
{
    if (emit_line) emit_line(s.data(), s.size());
}

/* ---- series ---- */

/* the curves window g plots (the rest zero) */
xpp::PlotCurves curves_of(const GRAPH &g)
{
    xpp::PlotCurves c{};
    c.nvars = g.nvars;
    c.three = g.ThreeDFlag;
    for (int i = 0; i < g.nvars && i < MAXPERPLOT; i++) {
        c.xv[i] = g.xv[i];
        c.yv[i] = g.yv[i];
        c.zv[i] = g.zv[i];
        c.line[i] = g.line[i];
        c.color[i] = g.color[i];
    }
    c.shift[0] = g.xshft;
    c.shift[1] = g.yshft;
    c.shift[2] = g.zshft;
    return c;
}

SeriesSig series_sig(const xpp::Session &s, int pop)
{
    SeriesSig sig{};
    const GRAPH &g = s.plot_windows.graph[pop];
    sig.win = static_cast<unsigned long>(g.w);
    sig.version = data_version;
    sig.rows = s.browser.view.maxrow;
    sig.PlotCurves::operator=(curves_of(g));
    return sig;
}

bool same(const SeriesSig &a, const SeriesSig &b) { return a == b; }

/* the same window and curves, whatever the data */
bool same_plot(const SeriesSig &a, const SeriesSig &b)
{
    SeriesSig x = a;
    x.version = b.version;
    x.rows = b.rows;
    return same(x, b);
}

/* rows [from, to) of storage column col as a JSON value */
void add_values(std::string &o, const BROWSER &view, int col, int from, int to)
{
    xpp_series_append(o, view.data[col] + from, to - from, series_f32);
}

void add_curves(std::string &o, const xpp::PlotCurves &s)
{
    o += "\"curves\":[";
    for (int i = 0; i < s.nvars && i < MAXPERPLOT; i++) {
        if (i) o += ',';
        o += "{\"x\":";
        add_int(o, s.xv[i]);
        o += ",\"y\":";
        add_int(o, s.yv[i]);
        o += ",\"z\":";
        add_int(o, s.zv[i]);
        o += ",\"color\":";
        add_int(o, s.color[i]);
        o += ",\"line\":";
        add_int(o, s.line[i]);
        o += '}';
    }
    o += "],\"shift\":[";
    add_int(o, s.shift[0]);
    o += ',';
    add_int(o, s.shift[1]);
    o += ',';
    add_int(o, s.shift[2]);
    o += ']';
}

/* T and each column a curve uses, once each */
std::vector<int> used_columns(const xpp::PlotCurves &s, int maxcol)
{
    std::vector<int> cols{0}; /* T always: a readout names the time of any point */
    std::vector<bool> used(MAXODE + 1, false);
    used[0] = true;
    for (int i = 0; i < s.nvars && i < MAXPERPLOT; i++) {
        const int c[3] = {s.xv[i], s.yv[i], s.zv[i]};
        for (int k = 0; k < (s.three ? 3 : 2); k++)
            if (c[k] >= 0 && c[k] < maxcol && c[k] <= MAXODE && !used[c[k]]) {
                used[c[k]] = true;
                cols.push_back(c[k]);
            }
    }
    return cols;
}

/* ---- earlier runs (display_state.h, docs/protocol.md "The plot as data") ----

   A plot window shows every run over the ones before until Erase, so a new
   run keeps the one it replaces, drawn lighter under it. The core holds
   them (Session::plot_display) and tells the client with `runs` events:

   - a full series with other curves (Xi vs t, Viewaxes, ...) is another
     picture: the runs go;
   - a full series of other data (its version differs) keeps the current
     run as an earlier one, unless the current one is being extended in
     this command (appends: the full series ends that run) or was erased;
   - an append that starts again before the rows the client holds (a new
     run under way, or the next run of a range) keeps the current run first;
   - Erase forgets the runs and hides the current data until the next run
     or Redraw; Redraw shows the current data again, without the runs. */

/* at most this many earlier runs per window, oldest dropped, and at most
   this many rows in all of them, so memory stays bounded */
constexpr std::size_t runs_keep = 50;
constexpr long runs_max_rows = 4000000;

xpp::PlotDisplay &disp(xpp::Session &s, int pop) { return s.plot_display[pop]; }

bool same_curves(const xpp::PlotCurves &a, const xpp::PlotCurves &b) { return a == b; }

/* the runs event: the client drops its `drop` oldest runs (all of them when
   `clear`), then adds the last `added` of ours; `erased` as now */
void emit_runs(xpp::Session &s, int pop, bool clear, std::size_t drop, std::size_t added)
{
    const xpp::PlotDisplay &d = disp(s, pop);
    std::string o = "{\"ev\":\"runs\",\"win\":";
    add_int(o, static_cast<long>(s.plot_windows.graph[pop].w));
    o += ",\"erased\":";
    add_int(o, d.erased);
    o += ",\"clear\":";
    add_int(o, clear);
    o += ",\"drop\":";
    add_int(o, static_cast<long>(drop));
    if (series_f32) o += ",\"enc\":\"f32\"";
    o += ",\"add\":[";
    for (std::size_t i = d.runs.size() - added; i < d.runs.size(); i++) {
        const xpp::PlotRun &r = d.runs[i];
        if (i + added != d.runs.size()) o += ',';
        o += "{\"rows\":";
        add_int(o, r.rows);
        o += ",\"three\":";
        add_int(o, r.curves.three);
        o += ',';
        add_curves(o, r.curves);
        o += ",\"columns\":[";
        for (std::size_t k = 0; k < r.cols.size(); k++) {
            if (k) o += ',';
            o += "{\"col\":";
            add_int(o, r.cols[k]);
            o += ",\"name\":";
            xpp::json_append_string(o, xpp::ind_to_sym(s,r.cols[k]));
            o += ",\"data\":";
            xpp_series_append(o, r.data[k].data(), r.rows, series_f32);
            o += '}';
        }
        o += "]}";
    }
    o += "]}";
    emit(o);
}

/* `r` becomes the newest earlier run (within the limits); how many oldest
   ones went, and whether it was kept */
struct Kept {
    std::size_t drop = 0;
    std::size_t added = 0;
};

Kept keep_run(xpp::PlotDisplay &d, xpp::PlotRun &&r)
{
    Kept k;
    if (r.rows <= 0) return k;
    d.runs.push_back(std::move(r));
    long rows = 0;
    for (const xpp::PlotRun &x : d.runs) rows += x.rows;
    std::size_t drop = d.runs.size() > runs_keep ? d.runs.size() - runs_keep : 0;
    for (std::size_t i = 0; i < drop; i++) rows -= d.runs[i].rows;
    while (drop + 1 < d.runs.size() && rows > runs_max_rows) rows -= d.runs[drop++].rows;
    d.runs.erase(d.runs.begin(), d.runs.begin() + static_cast<std::ptrdiff_t>(drop));
    k.drop = drop;
    k.added = 1;
    return k;
}

/* window pop's current run replaced by the full series `s` of `cols`; `refresh`: the
   client asked again for what it had (nothing about the runs changed) */
void runs_on_full(xpp::Session &s, int pop, const SeriesSig &sig, const std::vector<int> &cols, int rows, bool refresh)
{
    xpp::PlotDisplay &d = disp(s, pop);
    if (refresh && d.has_cur && same_curves(d.cur.curves, sig) && d.cur_version == sig.version) {
        if (!d.runs.empty() || d.erased) emit_runs(s, pop, true, 0, d.runs.size());
    } else if (!d.has_cur || !same_curves(d.cur.curves, sig)) {
        if (!d.runs.empty() || d.erased || d.live) {
            d.runs.clear();
            d.erased = d.live = false;
            emit_runs(s, pop, true, 0, 0);
        }
    } else if (d.erased || d.live) {
        const bool was = d.erased;
        d.erased = d.live = false;
        if (was) emit_runs(s, pop, false, 0, 0);
    } else if (sig.version != d.cur_version && d.cur.rows > 0) {
        const Kept k = keep_run(d, std::move(d.cur));
        emit_runs(s, pop, false, k.drop, k.added);
    }
    d.has_cur = true;
    d.cur_version = sig.version;
    d.cur.curves = sig;
    d.cur.rows = rows;
    d.cur.cols = cols;
    d.cur.data.assign(cols.size(), std::vector<float>());
    for (std::size_t k = 0; k < cols.size(); k++) {
        const float *src = s.browser.view.data[cols[k]];
        d.cur.data[k].assign(src, src + rows);
    }
}

/* an append from row `from` continues window pop's current run; then the
   rows [from, rows) of its columns join the copy */
void runs_on_append(xpp::Session &s, int pop, int from, int rows)
{
    xpp::PlotDisplay &d = disp(s, pop);
    if (from < d.cur.rows) { /* a new run (or the next of a range): the rows it replaces become an earlier run */
        Kept k;
        if (!d.erased) {
            if (from == 0) {
                const std::vector<int> cols = d.cur.cols;
                const xpp::PlotCurves curves = d.cur.curves;
                k = keep_run(d, std::move(d.cur));
                d.cur = xpp::PlotRun();
                d.cur.curves = curves;
                d.cur.cols = cols;
                d.cur.data.assign(cols.size(), std::vector<float>());
            } else {
                xpp::PlotRun r = d.cur;
                k = keep_run(d, std::move(r));
            }
        }
        d.erased = false;
        d.live = true;
        emit_runs(s, pop, false, k.drop, k.added);
    } else if (!(d.live && !d.erased)) {
        const bool was = d.erased;
        d.erased = false;
        d.live = true;
        if (was) emit_runs(s, pop, false, 0, 0);
    }
    d.cur.rows = rows;
    for (std::size_t k = 0; k < d.cur.cols.size(); k++) {
        const float *src = s.browser.view.data[d.cur.cols[k]];
        d.cur.data[k].resize(static_cast<std::size_t>(from));
        d.cur.data[k].insert(d.cur.data[k].end(), src + from, src + rows);
    }
}

/* the window is gone: what it showed goes with it */
void runs_forget(xpp::Session &s, int pop) { disp(s, pop) = xpp::PlotDisplay(); }

/* window pop's whole series: rows 0..rows of the columns its curves use */
void send_series(xpp::Session &s, int pop, const SeriesSig &sig, int rows)
{
    const GRAPH &g = s.plot_windows.graph[pop];
    const std::vector<int> cols = used_columns(sig, s.browser.view.maxcol);
    runs_on_full(s, pop, sig, cols, rows, !sent[pop].valid);
    std::string o = "{\"ev\":\"series\",\"win\":";
    add_int(o, static_cast<long>(sig.win));
    o += ",\"rows\":";
    add_int(o, rows);
    o += ",\"version\":";
    add_int(o, static_cast<long>(sig.version));
    o += ",\"three\":";
    add_int(o, sig.three);
    if (series_f32) o += ",\"enc\":\"f32\"";
    o += ",\"xlabel\":";
    xpp::json_append_string(o, g.xlabel);
    o += ",\"ylabel\":";
    xpp::json_append_string(o, g.ylabel);
    o += ",\"zlabel\":";
    xpp::json_append_string(o, g.zlabel);
    o += ',';
    add_curves(o, sig);
    o += ",\"columns\":[";
    for (std::size_t k = 0; k < cols.size(); k++) {
        if (k) o += ',';
        o += "{\"col\":";
        add_int(o, cols[k]);
        o += ",\"name\":";
        xpp::json_append_string(o, xpp::ind_to_sym(s,cols[k]));
        o += ",\"data\":";
        add_values(o, s.browser.view, cols[k], 0, rows);
        o += '}';
    }
    o += "]}";
    emit(o);
    Sent &w = sent[pop];
    w.valid = true;
    w.sig = sig;
    w.cols = cols;
    w.held = rows;
    rows_seen = rows;
}

/* during a run: the active window's rows stored since what the client holds */
void series_append(xpp::Session &s, int rows)
{
    const int pop = s.plot_windows.active;
    const SeriesSig sig = series_sig(s, pop);
    Sent &w = sent[pop];
    appended = pop;
    if (!w.valid || !same_plot(sig, w.sig) || !disp(s, pop).has_cur || disp(s, pop).cur.cols != w.cols) {
        send_series(s, pop, sig, rows); /* other columns: the whole series, as far as it goes */
        return;
    }
    const int from = w.held;
    if (rows <= from) return;
    runs_on_append(s, pop, from, rows);
    std::string o = "{\"ev\":\"series\",\"op\":\"append\",\"win\":";
    add_int(o, static_cast<long>(sig.win));
    o += ",\"from\":";
    add_int(o, from);
    o += ",\"rows\":";
    add_int(o, rows);
    if (series_f32) o += ",\"enc\":\"f32\"";
    o += ",\"columns\":[";
    for (std::size_t k = 0; k < w.cols.size(); k++) {
        if (k) o += ',';
        o += "{\"col\":";
        add_int(o, w.cols[k]);
        o += ",\"data\":";
        add_values(o, s.browser.view, w.cols[k], from, rows);
        o += '}';
    }
    o += "]}";
    emit(o);
    w.held = rows;
}

/* the series of every window that changed, the active one first, and
   always the one that got appends */
void series_update(xpp::Session &s)
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? s.plot_windows.active : (k == s.plot_windows.active ? 0 : k);
        if (!s.plot_windows.graph[pop].Use) {
            sent[pop].valid = false; /* a window made again later starts afresh */
            runs_forget(s, pop);
            continue;
        }
        const SeriesSig sig = series_sig(s, pop);
        if (appended != pop && sent[pop].valid && same(sig, sent[pop].sig)) continue;
        send_series(s, pop, sig, s.browser.view.dataflag ? sig.rows : 0);
    }
    appended = -1;
}

/* ---- plots ---- */

/* "W vs V" (or "z vs y vs x" in 3D): what the window plots, as axes2.c's
   make_title() names the active one */
std::string title(const xpp::Session &s, const GRAPH &g)
{
    const std::string x = xpp::ind_to_sym(s,g.xv[0]), y = xpp::ind_to_sym(s,g.yv[0]), z = xpp::ind_to_sym(s,g.zv[0]);
    return g.grtype >= 5 ? z + " vs " + y + " vs " + x : y + " vs " + x;
}

/* the zoom is for the axes and curves it was made at: other ones drop it */
void zoom_upkeep(xpp::Session &s, int pop)
{
    const GRAPH &g = s.plot_windows.graph[pop];
    xpp::PlotDisplay &d = disp(s, pop);
    const std::array<double, 4> axes = {g.xlo, g.xhi, g.ylo, g.yhi};
    const xpp::PlotCurves c = curves_of(g);
    if (d.axes_seen && (axes != d.axes || !same_curves(c, d.axes_curves))) d.zoom = xpp::Zoom();
    d.axes_seen = true;
    d.axes = axes;
    d.axes_curves = c;
}

void add_zoom(std::string &o, const xpp::AxisRange &r)
{
    if (!r.set) {
        o += "null";
        return;
    }
    o += '[';
    xpp::json::json_append_number_shortest(o, r.lo);
    o += ',';
    xpp::json::json_append_number_shortest(o, r.hi);
    o += ']';
}

std::string plots_event(xpp::Session &s)
{
    std::string o = "{\"ev\":\"plots\",\"active\":";
    add_int(o, static_cast<long>(s.plot_windows.graph[s.plot_windows.active].w));
    o += ",\"windows\":[";
    bool first = true;
    for (int pop = 0; pop < MAXPOP; pop++) {
        const GRAPH &g = s.plot_windows.graph[pop];
        if (!g.Use) continue;
        if (!first) o += ',';
        first = false;
        o += "{\"win\":";
        add_int(o, static_cast<long>(g.w));
        o += ",\"title\":";
        xpp::json_append_string(o, title(s, g).c_str());
        o += ",\"three\":";
        add_int(o, g.ThreeDFlag);
        xpp::json::json_append_field(o, "xlo", g.xlo);
        xpp::json::json_append_field(o, "xhi", g.xhi);
        xpp::json::json_append_field(o, "ylo", g.ylo);
        xpp::json::json_append_field(o, "yhi", g.yhi);
        o += ",\"xlabel\":";
        xpp::json_append_string(o, g.xlabel);
        o += ",\"ylabel\":";
        xpp::json_append_string(o, g.ylabel);
        o += ",\"zlabel\":";
        xpp::json_append_string(o, g.zlabel);
        o += ",\"box\":{\"xmin\":";
        xpp::json::json_append_number_shortest(o, g.xmin);
        xpp::json::json_append_field(o, "xmax", g.xmax);
        xpp::json::json_append_field(o, "ymin", g.ymin);
        xpp::json::json_append_field(o, "ymax", g.ymax);
        xpp::json::json_append_field(o, "zmin", g.zmin);
        xpp::json::json_append_field(o, "zmax", g.zmax);
        o += '}';
        xpp::json::json_append_field(o, "theta", g.Theta);
        xpp::json::json_append_field(o, "phi", g.Phi);
        o += ",\"persp\":";
        add_int(o, g.PerspFlag);
        xpp::json::json_append_field(o, "zplane", g.ZPlane);
        xpp::json::json_append_field(o, "zview", g.ZView);
        zoom_upkeep(s, pop);
        o += ",\"zoom\":{\"x\":";
        add_zoom(o, disp(s, pop).zoom.x);
        o += ",\"y\":";
        add_zoom(o, disp(s, pop).zoom.y);
        o += "},\"runs\":";
        add_int(o, disp(s, pop).show_runs);
        o += ',';
        add_curves(o, series_sig(s, pop));
        o += '}';
    }
    o += "]}";
    return o;
}

void plots_update(xpp::Session &s)
{
    std::string o = plots_event(s);
    if (plots_valid && o == plots_sent) return;
    emit(o);
    plots_sent.swap(o);
    plots_valid = true;
}

} // namespace

/* ---- the C API: no exception leaves it (out of memory drops the event) ---- */

extern "C" void plot_data_init(PlotDataEmit emit) { emit_line = emit; }

extern "C" void plot_data_subscribe(int series, int plots, int f32)
{
    series_on = series != 0;
    plots_on = plots != 0;
    series_f32 = f32 != 0;
    for (Sent &w : sent) w.valid = false; /* the next update sends */
    plots_valid = false;
    appended = -1;
}

extern "C" void plot_data_changed(void) { data_version++; }

/* the windows a command draws on: all of ActiveWinList under Simulplot, else the active one */
void plot_data_picture(xpp::Session &s, int redraw)
{
    if (!series_on) return;
    const XppPlotWindows &w = s.plot_windows;
    const int n = w.simul ? w.count : 1;
    try {
        for (int k = 0; k < n; k++) {
            const int pop = w.simul ? w.open[k] : w.active;
            if (pop < 0 || pop >= MAXPOP || !w.graph[pop].Use) continue;
            std::string o = redraw ? "{\"ev\":\"redraw\",\"win\":" : "{\"ev\":\"erase\",\"win\":";
            add_int(o, static_cast<long>(w.graph[pop].w));
            o += '}';
            emit(o);
            xpp::PlotDisplay &d = disp(s, pop);
            if (redraw ? (!d.runs.empty() || d.erased) : true) {
                d.runs.clear();
                d.erased = !redraw;
                d.live = false;
                emit_runs(s, pop, true, 0, 0);
            }
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the runs of the plot windows");
    }
}

/* the encoding the client asked for in its last "data" command (docs/ui-v2.md
   T12, "reuse series_enc"): other events that carry value arrays outside the
   subscription list (aplot) still honour it. */
extern "C" int plot_data_want_f32(void) { return series_f32; }

void plot_data_rows_stored(xpp::Session &s, int nrows)
{
    static double last;
    if (!series_on) return;
    if (nrows <= rows_seen) /* storage started again from its first row */
        for (Sent &w : sent) w.held = 0;
    rows_seen = nrows;
    if (!xpp_every(&last, append_every)) return;
    try {
        series_append(s, nrows);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the rows a run stored");
    }
}

void plot_data_update(xpp::Session &s)
{
    try {
        if (plots_on) plots_update(s);
        if (series_on) series_update(s);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the plots");
    }
}
