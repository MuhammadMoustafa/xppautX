/* Nullclines, direction fields and flows as data: the "nullclines" and
   "dfield" events (phase_data.h, docs/protocol.md "The plot as data").

   Each plot window keeps a record of what the core drew in it since it was
   last blanked, reported by the code that draws (nullcline.c, and
   integrate.c for Flow's trajectories): the Session's (phase_data.h
   PhaseShown). The client's, kept here, is the fingerprint of what it got
   last. At the end of a command a window whose record was
   touched since then (its generation, which every change bumps) and whose
   fingerprint differs gets its event: drawing the same thing again sends
   nothing, and a command that draws nothing costs nothing. */
#include <new>
#include "xpp_mem.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "model.h"
#include "session.h"
#include "phase_data.h"
#include "json_number.h"
#include "series_enc.h"
#include "many_pops.h"
#include "form_ode.h"
#include "xpp_math.h"

namespace xpp {

namespace {

/* a flow's points are kept when they move more than this fraction of the
   window's axes from the last point kept: well under a pixel, and a
   trajectory that circles a limit cycle for a long time costs a few
   thousand points a turn instead of one per step */
const double FLOW_STEP = 2e-4;
/* at most this many values of flows per window (x and y together) */
const std::size_t FLOW_MAX = 8000000;

/* a record's fingerprint, to tell whether it is what the client got
   without keeping a copy of that: 64-bit FNV-1a over 8-byte words of its
   bits (a flow's breaks are NaN), so a change goes unseen with odds of
   one in 2^64 */
class Fingerprint {
public:
    void bytes(const void *p, std::size_t n)
    {
        const unsigned char *b = static_cast<const unsigned char *>(p);
        for (; n >= sizeof(std::uint64_t); n -= sizeof(std::uint64_t), b += sizeof(std::uint64_t)) {
            std::uint64_t w;
            std::memcpy(&w, b, sizeof w);
            mix(w);
        }
        for (; n > 0; n--, b++) mix(*b);
    }
    template <class T> void value(const T &v) { bytes(&v, sizeof v); }
    void floats(const std::vector<float> &v)
    {
        value(v.size());
        bytes(v.data(), v.size() * sizeof(float));
    }
    std::uint64_t result() const { return h_; }
private:
    void mix(std::uint64_t w) { h_ = (h_ ^ w) * 1099511628211ULL; /* the FNV 64-bit prime */ }
    std::uint64_t h_ = 14695981039346656037ULL; /* the FNV 64-bit offset basis */
};

void add_to(Fingerprint &f, const PhaseShown::Clines &c)
{
    f.floats(c.x);
    f.floats(c.y);
}

std::uint64_t fingerprint(const PhaseShown::Nullclines &nc)
{
    Fingerprint f;
    for (int v : {nc.ix, nc.iy, nc.xcolor, nc.ycolor}) f.value(v);
    add_to(f, nc.now);
    f.value(nc.frozen.size());
    for (const PhaseShown::Clines &c : nc.frozen) add_to(f, c);
    return f.result();
}

std::uint64_t fingerprint(const PhaseShown::Field &d)
{
    Fingerprint f;
    for (int v : {d.n, d.scaled, d.color}) f.value(v);
    f.value(d.du);
    f.value(d.dv);
    f.floats(d.grid);
    f.floats(d.speed);
    f.value(d.flows.size());
    for (const PhaseShown::FlowCurve &c : d.flows) {
        f.value(c.color);
        f.floats(c.x);
        f.floats(c.y);
    }
    return f.result();
}

/* what the client got of one record of a window: the generation of the
   record it looked at last (looked: one of this Session's), and the
   fingerprint of the record it got (valid: it got one) */
struct Got {
    unsigned long seen = 0;
    bool looked = false;
    bool valid = false;
    std::uint64_t sent = 0;
};

/* whether the client needs the record now (of that generation): none got
   yet, or another one */
template <class Record>
bool due(Got &g, const Record &now, unsigned long generation)
{
    if (g.valid && g.looked && generation == g.seen) return false;
    g.seen = generation;
    g.looked = true;
    const std::uint64_t f = fingerprint(now);
    if (g.valid && f == g.sent) return false;
    g.valid = true;
    g.sent = f;
    return true;
}

/* The protocol client's: the events it subscribed to and what it got of
   each window's records. It outlives a load, as the client does. */
struct ClientWindow {
    Got nc, df;
};
struct PhaseClient {
    PhaseDataEmit emit_line = nullptr;
    bool nullclines_on = false, dfield_on = false, values_f32 = false;
    std::array<ClientWindow, MAXPOP> windows;
};
PhaseClient client;

/* a change of a window's record (its nullclines or direction field): its
   generation moves */
template <class Record>
Record &change(Record &record, unsigned long &generation)
{
    generation++;
    return record;
}

/* the record of the active window of s, the window drawn in */
PhaseShown::Window *current(Session &s)
{
    const int pop = s.plot_windows.active;
    if (!client.emit_line || pop < 0 || pop >= MAXPOP) return nullptr;
    return &s.phase_shown.windows[pop];
}

/* ---- flows ---- */

void flow_flush(PhaseShown::FlowCurve &c)
{
    if (!c.pending) return;
    c.x.push_back(c.px);
    c.y.push_back(c.py);
    c.kx = c.px;
    c.ky = c.py;
    c.pending = false;
}

void flow_point(PhaseShown::FlowCurve &c, float x, float y, double ex, double ey)
{
    if (std::fabs(x - c.kx) > ex || std::fabs(y - c.ky) > ey || !std::isfinite(x) || !std::isfinite(y)) {
        c.x.push_back(x);
        c.y.push_back(y);
        c.kx = x;
        c.ky = y;
        c.pending = false;
    } else {
        c.px = x;
        c.py = y;
        c.pending = true;
    }
}

void flow_start_point(PhaseShown::FlowCurve &c, float x, float y)
{
    flow_flush(c);
    if (!c.x.empty()) {
        c.x.push_back(NAN);
        c.y.push_back(NAN);
    }
    c.x.push_back(x);
    c.y.push_back(y);
    c.kx = x;
    c.ky = y;
}

std::size_t flow_values(const PhaseShown::Field &f)
{
    std::size_t n = 0;
    for (const PhaseShown::FlowCurve &c : f.flows) n += c.x.size() + c.y.size();
    return n;
}

/* ---- JSON text ---- */

void add_int(std::string &o, long v) { o += std::to_string(v); }

/* a variable's name ("" for none): the model's own, letters, digits, underscores */
void add_name(std::string &o, const xpp::Model &m, int col)
{
    o += '"';
    if (col > 0 && col <= MAXODE)
        for (char c : m.uvar_names[col - 1])
            if (c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 0x20) o += c;
    o += '"';
}

void add_values(std::string &o, const std::vector<float> &v)
{
    xpp_series_append(o, v.data(), static_cast<int>(v.size()), client.values_f32);
}

void begin_event(std::string &o, const char *ev, const GRAPH &g)
{
    o = "{\"ev\":\"";
    o += ev;
    o += "\",\"win\":";
    add_int(o, static_cast<long>(g.w));
    if (client.values_f32) o += ",\"enc\":\"f32\"";
}

void send_nullclines(const xpp::Session &s, int pop, const PhaseShown::Nullclines &nc)
{
    std::string o;
    begin_event(o, "nullclines", s.plot_windows.graph[pop]);
    o += ",\"xname\":";
    add_name(o, s.model(), nc.ix);
    o += ",\"yname\":";
    add_name(o, s.model(), nc.iy);
    o += ",\"xcolor\":";
    add_int(o, nc.xcolor);
    o += ",\"ycolor\":";
    add_int(o, nc.ycolor);
    o += ",\"x\":";
    add_values(o, nc.now.x);
    o += ",\"y\":";
    add_values(o, nc.now.y);
    o += ",\"frozen\":[";
    for (std::size_t k = 0; k < nc.frozen.size(); k++) {
        o += k ? ",{\"x\":" : "{\"x\":";
        add_values(o, nc.frozen[k].x);
        o += ",\"y\":";
        add_values(o, nc.frozen[k].y);
        o += '}';
    }
    o += "]}";
    client.emit_line(o);
}

void send_dfield(const GRAPH &g, const PhaseShown::Field &f)
{
    std::string o;
    begin_event(o, "dfield", g);
    o += ",\"scaled\":";
    add_int(o, f.scaled);
    o += ",\"color\":";
    add_int(o, f.color);
    o += ",\"n\":";
    add_int(o, f.n);
    o += ",\"du\":";
    xpp::json::json_append_number_shortest(o, f.du);
    o += ",\"dv\":";
    xpp::json::json_append_number_shortest(o, f.dv);
    o += ",\"grid\":";
    add_values(o, f.grid);
    o += ",\"speed\":";
    add_values(o, f.speed);
    o += ",\"flows\":[";
    for (std::size_t k = 0; k < f.flows.size(); k++) {
        o += k ? ",{\"color\":" : "{\"color\":";
        add_int(o, f.flows[k].color);
        o += ",\"x\":";
        add_values(o, f.flows[k].x);
        o += ",\"y\":";
        add_values(o, f.flows[k].y);
        o += '}';
    }
    o += "]}";
    client.emit_line(o);
}

void update(xpp::Session &s)
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? s.plot_windows.active : (k == s.plot_windows.active ? 0 : k); /* the active window first */
        PhaseShown::Window &w = s.phase_shown.windows[pop];
        ClientWindow &got = client.windows[pop];
        if (!s.plot_windows.graph[pop].Use) {
            w = PhaseShown::Window(); /* a window made again later starts afresh */
            got = ClientWindow();
            continue;
        }
        if (client.nullclines_on && due(got.nc, w.nc, w.nc_generation)) send_nullclines(s, pop, w.nc);
        if (client.dfield_on && due(got.df, w.df, w.df_generation)) send_dfield(s.plot_windows.graph[pop], w.df);
    }
}

} // namespace

/* ---- the API: no exception leaves it (out of memory drops the record) ---- */

void phase_data_init(PhaseDataEmit emit) { client.emit_line = emit; }

void phase_data_subscribe(int nullclines, int dfield, int f32)
{
    client.nullclines_on = nullclines != 0;
    client.dfield_on = dfield != 0;
    client.values_f32 = f32 != 0;
    for (ClientWindow &w : client.windows) w.nc.valid = w.df.valid = false; /* the next update sends */
}

void phase_data_update(xpp::Session &s)
{
    if (!client.emit_line || (!client.nullclines_on && !client.dfield_on)) return;
    try {
        update(s);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("sending the nullclines and direction fields");
    }
}

void phase_data_cleared(xpp::Session &s, int pop)
{
    if (!client.emit_line || pop < 0 || pop >= MAXPOP) return;
    PhaseShown::Window &w = s.phase_shown.windows[pop];
    PhaseShown::Nullclines &nc = change(w.nc, w.nc_generation);
    nc.now = PhaseShown::Clines();
    nc.frozen.clear();
    change(w.df, w.df_generation) = PhaseShown::Field();
    /* the record may be a new Session's (a load's): its generations say
       nothing of what the client looked at */
    client.windows[pop].nc.looked = client.windows[pop].df.looked = false;
}

void phase_data_nullclines(xpp::Session &s, const float *xn, int nx, const float *yn, int ny, int ix, int iy,
                           int xcolor, int ycolor)
{
    PhaseShown::Window *w = current(s);
    if (!w) return;
    try {
        PhaseShown::Nullclines &nc = change(w->nc, w->nc_generation);
        nc.ix = ix;
        nc.iy = iy;
        nc.xcolor = xcolor;
        nc.ycolor = ycolor;
        nc.now.x.assign(xn, xn + 4 * (nx > 0 ? nx : 0));
        nc.now.y.assign(yn, yn + 4 * (ny > 0 ? ny : 0));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording the nullclines");
    }
}

void phase_data_frozen_begin(xpp::Session &s)
{
    if (PhaseShown::Window *w = current(s)) change(w->nc, w->nc_generation).frozen.clear();
}

void phase_data_frozen(xpp::Session &s, const float *xn, int nx, const float *yn, int ny)
{
    PhaseShown::Window *w = current(s);
    if (!w) return;
    try {
        PhaseShown::Clines c;
        c.x.assign(xn, xn + 4 * (nx > 0 ? nx : 0));
        c.y.assign(yn, yn + 4 * (ny > 0 ? ny : 0));
        change(w->nc, w->nc_generation).frozen.push_back(std::move(c));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording the frozen nullclines");
    }
}

void phase_data_dfield_begin(xpp::Session &s, int n, double du, double dv, int scaled, int color)
{
    PhaseShown::Window *w = current(s);
    if (!w) return;
    PhaseShown::Field &f = change(w->df, w->df_generation);
    f.n = n;
    f.du = du;
    f.dv = dv;
    f.scaled = scaled;
    f.color = color;
    f.grid.clear();
    f.speed.clear();
}

void phase_data_arrow(xpp::Session &s, double x, double y, double fx, double fy)
{
    PhaseShown::Window *w = current(s);
    if (!w) return;
    const double speed = xpp::math::hypot(fx, fy);
    const bool unit = speed > 0 && std::isfinite(speed);
    try {
        PhaseShown::Field &f = change(w->df, w->df_generation);
        const float v[4] = {static_cast<float>(x), static_cast<float>(y), unit ? static_cast<float>(fx / speed) : 0.0f,
                            unit ? static_cast<float>(fy / speed) : 0.0f};
        f.grid.insert(f.grid.end(), v, v + 4);
        f.speed.push_back(static_cast<float>(speed));
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording the direction field");
    }
}

void phase_data_flow_start(xpp::Session &s) { s.phase_shown.flowing = client.emit_line != nullptr; }

void phase_data_flow_next(xpp::Session &s) { s.phase_shown.trajectory++; }

void phase_data_flow_step(xpp::Session &s, int ncurves, const float *ox, const float *oy, const float *x,
                          const float *y, const int *color)
{
    if (!s.phase_shown.flowing) return;
    PhaseShown::Window *w = current(s);
    if (!w || s.plot_windows.graph[s.plot_windows.active].ThreeDFlag || ncurves <= 0) return;
    try {
        if (flow_values(w->df) > FLOW_MAX) return;
        PhaseShown::Field &f = change(w->df, w->df_generation);
        if (f.flows.size() != static_cast<std::size_t>(ncurves)) f.flows.resize(ncurves);
        const GRAPH &g = s.plot_windows.graph[s.plot_windows.active];
        const double ex = std::fabs(g.xhi - g.xlo) * FLOW_STEP, ey = std::fabs(g.yhi - g.ylo) * FLOW_STEP;
        const bool first = w->trajectory != s.phase_shown.trajectory;
        w->trajectory = s.phase_shown.trajectory;
        for (int i = 0; i < ncurves; i++) {
            PhaseShown::FlowCurve &c = f.flows[i];
            c.color = color[i];
            if (first) flow_start_point(c, ox[i], oy[i]);
            flow_point(c, x[i], y[i], ex, ey);
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("recording the flow");
    }
}

void phase_data_flow_stop(xpp::Session &s)
{
    s.phase_shown.flowing = false;
    for (PhaseShown::Window &w : s.phase_shown.windows)
        for (PhaseShown::FlowCurve &c : w.df.flows) try {
                if (c.pending) change(w.df, w.df_generation);
                flow_flush(c);
            } catch (const std::bad_alloc &) {
                xpp::out_of_memory("recording the flow");
            }
}

} // namespace xpp
