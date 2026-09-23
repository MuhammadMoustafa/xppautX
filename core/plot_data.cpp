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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "plot_data.h"
#include "series_enc.h"
#include "xpp_globals.h"
#include "xpp_job.h"
#include "xpp_mem.h"
#include "browse.h"

extern "C" {
extern BROWSER my_browser;
extern char uvar_names[MAXODE][XPP_NAME_MAX + 1];
}

namespace {

PlotDataEmit emit_line;
bool series_on, plots_on, series_f32;
unsigned long data_version; /* counts data changes */

/* what a window's series shows: equal signatures, the same event */
struct SeriesSig {
    unsigned long win, version;
    int rows, nvars, three;
    int xv[MAXPERPLOT], yv[MAXPERPLOT], zv[MAXPERPLOT], line[MAXPERPLOT], color[MAXPERPLOT];
    int shift[3];
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

std::string plots_sent;
bool plots_valid;

/* ---- JSON text ---- */

void add_str(std::string &o, const char *s)
{
    char esc[8];
    o += '"';
    for (; s && *s; s++) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20 || c >= 0x80) {
            /* the core's strings are ASCII or Latin-1; keep the byte value */
            std::snprintf(esc, sizeof esc, "\\u%04x", c);
            o += esc;
        } else o += static_cast<char>(c);
    }
    o += '"';
}

void add_int(std::string &o, long v) { o += std::to_string(v); }

/* the shortest of 15 or 17 digits that reads back as v; null when not finite */
void add_num(std::string &o, double v)
{
    char t[32];
    if (v != v || v > 1e308 || v < -1e308) {
        o += "null";
        return;
    }
    std::snprintf(t, sizeof t, "%.15g", v);
    if (std::strtod(t, nullptr) != v) std::snprintf(t, sizeof t, "%.17g", v);
    o += t;
}

void emit(const std::string &s)
{
    if (emit_line) emit_line(s.data(), s.size());
}

/* ---- series ---- */

SeriesSig series_sig(int pop)
{
    SeriesSig s;
    std::memset(&s, 0, sizeof s); /* padding too: signatures are compared with memcmp */
    const GRAPH &g = graph[pop];
    s.win = static_cast<unsigned long>(g.w);
    s.version = data_version;
    s.rows = my_browser.maxrow;
    s.nvars = g.nvars;
    s.three = g.ThreeDFlag;
    for (int i = 0; i < g.nvars && i < MAXPERPLOT; i++) {
        s.xv[i] = g.xv[i];
        s.yv[i] = g.yv[i];
        s.zv[i] = g.zv[i];
        s.line[i] = g.line[i];
        s.color[i] = g.color[i];
    }
    s.shift[0] = g.xshft;
    s.shift[1] = g.yshft;
    s.shift[2] = g.zshft;
    return s;
}

bool same(const SeriesSig &a, const SeriesSig &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

/* the same window and curves, whatever the data */
bool same_plot(const SeriesSig &a, const SeriesSig &b)
{
    SeriesSig x = a;
    x.version = b.version;
    x.rows = b.rows;
    return same(x, b);
}

const char *column_name(int col) { return col == 0 ? "T" : uvar_names[col - 1]; }

/* rows [from, to) of storage column col as a JSON value */
void add_values(std::string &o, int col, int from, int to)
{
    std::size_t n;
    char *t = xpp_series_values(my_browser.data[col] + from, to - from, series_f32, &n);
    if (t) {
        o.append(t, n);
        xpp_free(t);
    } else o += "[]";
}

void add_curves(std::string &o, const SeriesSig &s)
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
std::vector<int> used_columns(const SeriesSig &s)
{
    std::vector<int> cols{0}; /* T always: a readout names the time of any point */
    std::vector<bool> used(MAXODE + 1, false);
    used[0] = true;
    for (int i = 0; i < s.nvars && i < MAXPERPLOT; i++) {
        const int c[3] = {s.xv[i], s.yv[i], s.zv[i]};
        for (int k = 0; k < (s.three ? 3 : 2); k++)
            if (c[k] >= 0 && c[k] < my_browser.maxcol && c[k] <= MAXODE && !used[c[k]]) {
                used[c[k]] = true;
                cols.push_back(c[k]);
            }
    }
    return cols;
}

/* window pop's whole series: rows 0..rows of the columns its curves use */
void send_series(int pop, const SeriesSig &s, int rows)
{
    const GRAPH &g = graph[pop];
    const std::vector<int> cols = used_columns(s);
    std::string o = "{\"ev\":\"series\",\"win\":";
    add_int(o, static_cast<long>(s.win));
    o += ",\"rows\":";
    add_int(o, rows);
    o += ",\"version\":";
    add_int(o, static_cast<long>(s.version));
    o += ",\"three\":";
    add_int(o, s.three);
    if (series_f32) o += ",\"enc\":\"f32\"";
    o += ",\"xlabel\":";
    add_str(o, g.xlabel);
    o += ",\"ylabel\":";
    add_str(o, g.ylabel);
    o += ",\"zlabel\":";
    add_str(o, g.zlabel);
    o += ',';
    add_curves(o, s);
    o += ",\"columns\":[";
    for (std::size_t k = 0; k < cols.size(); k++) {
        if (k) o += ',';
        o += "{\"col\":";
        add_int(o, cols[k]);
        o += ",\"name\":";
        add_str(o, column_name(cols[k]));
        o += ",\"data\":";
        add_values(o, cols[k], 0, rows);
        o += '}';
    }
    o += "]}";
    emit(o);
    Sent &w = sent[pop];
    w.valid = true;
    w.sig = s;
    w.cols = cols;
    w.held = rows;
    rows_seen = rows;
}

/* during a run: the active window's rows stored since what the client holds */
void series_append(int rows)
{
    const int pop = current_pop;
    const SeriesSig s = series_sig(pop);
    Sent &w = sent[pop];
    appended = pop;
    if (!w.valid || !same_plot(s, w.sig)) {
        send_series(pop, s, rows); /* other columns: the whole series, as far as it goes */
        return;
    }
    const int from = w.held;
    if (rows <= from) return;
    std::string o = "{\"ev\":\"series\",\"op\":\"append\",\"win\":";
    add_int(o, static_cast<long>(s.win));
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
        add_values(o, w.cols[k], from, rows);
        o += '}';
    }
    o += "]}";
    emit(o);
    w.held = rows;
}

/* the series of every window that changed, the active one first, and
   always the one that got appends */
void series_update()
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? current_pop : (k == current_pop ? 0 : k);
        if (!graph[pop].Use) {
            sent[pop].valid = false; /* a window made again later starts afresh */
            continue;
        }
        const SeriesSig s = series_sig(pop);
        if (appended != pop && sent[pop].valid && same(s, sent[pop].sig)) continue;
        send_series(pop, s, my_browser.dataflag ? s.rows : 0);
    }
    appended = -1;
}

/* ---- plots ---- */

/* "W vs V" (or "z vs y vs x" in 3D): what the window plots, as axes2.c's
   make_title() names the active one */
std::string title(const GRAPH &g)
{
    const std::string x = column_name(g.xv[0]), y = column_name(g.yv[0]), z = column_name(g.zv[0]);
    return g.grtype >= 5 ? z + " vs " + y + " vs " + x : y + " vs " + x;
}

void add_field(std::string &o, const char *name, double v)
{
    o += ",\"";
    o += name;
    o += "\":";
    add_num(o, v);
}

std::string plots_event()
{
    std::string o = "{\"ev\":\"plots\",\"active\":";
    add_int(o, static_cast<long>(graph[current_pop].w));
    o += ",\"windows\":[";
    bool first = true;
    for (int pop = 0; pop < MAXPOP; pop++) {
        const GRAPH &g = graph[pop];
        if (!g.Use) continue;
        if (!first) o += ',';
        first = false;
        o += "{\"win\":";
        add_int(o, static_cast<long>(g.w));
        o += ",\"title\":";
        add_str(o, title(g).c_str());
        o += ",\"three\":";
        add_int(o, g.ThreeDFlag);
        add_field(o, "xlo", g.xlo);
        add_field(o, "xhi", g.xhi);
        add_field(o, "ylo", g.ylo);
        add_field(o, "yhi", g.yhi);
        o += ",\"xlabel\":";
        add_str(o, g.xlabel);
        o += ",\"ylabel\":";
        add_str(o, g.ylabel);
        o += ",\"zlabel\":";
        add_str(o, g.zlabel);
        o += ",\"box\":{\"xmin\":";
        add_num(o, g.xmin);
        add_field(o, "xmax", g.xmax);
        add_field(o, "ymin", g.ymin);
        add_field(o, "ymax", g.ymax);
        add_field(o, "zmin", g.zmin);
        add_field(o, "zmax", g.zmax);
        o += '}';
        add_field(o, "theta", g.Theta);
        add_field(o, "phi", g.Phi);
        o += ",\"persp\":";
        add_int(o, g.PerspFlag);
        add_field(o, "zplane", g.ZPlane);
        add_field(o, "zview", g.ZView);
        o += ',';
        SeriesSig s = series_sig(pop);
        add_curves(o, s);
        o += '}';
    }
    o += "]}";
    return o;
}

void plots_update()
{
    std::string o = plots_event();
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
extern "C" void plot_data_picture(int redraw)
{
    if (!series_on) return;
    const int n = SimulPlotFlag ? num_pops : 1;
    try {
        for (int k = 0; k < n; k++) {
            const int pop = SimulPlotFlag ? ActiveWinList[k] : current_pop;
            if (pop < 0 || pop >= MAXPOP || !graph[pop].Use) continue;
            std::string o = redraw ? "{\"ev\":\"redraw\",\"win\":" : "{\"ev\":\"erase\",\"win\":";
            add_int(o, static_cast<long>(graph[pop].w));
            o += '}';
            emit(o);
        }
    } catch (...) {
    }
}

/* the encoding the client asked for in its last "data" command (docs/ui-v2.md
   T12, "reuse series_enc"): other events that carry value arrays outside the
   subscription list (aplot) still honour it. */
extern "C" int plot_data_want_f32(void) { return series_f32; }

extern "C" void plot_data_rows_stored(int nrows)
{
    static double last;
    if (!series_on) return;
    if (nrows <= rows_seen) /* storage started again from its first row */
        for (Sent &w : sent) w.held = 0;
    rows_seen = nrows;
    if (!xpp_every(&last, 0.1)) return;
    try {
        series_append(nrows);
    } catch (...) {
    }
}

extern "C" void plot_data_update(void)
{
    try {
        if (plots_on) plots_update();
        if (series_on) series_update();
    } catch (...) {
    }
}
