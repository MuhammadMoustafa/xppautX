/* Nullclines, direction fields and flows as data: the "nullclines" and
   "dfield" events (phase_data.h, docs/protocol.md "The plot as data").

   Each plot window keeps a record of what the core drew in it since it was
   last blanked, reported by the code that draws (nullcline.c, and
   integrate.c for Flow's trajectories), and what the client got last. At
   the end of a command a window whose record differs from what the client
   got gets its event. Records are compared bit for bit (a flow's breaks are
   NaN), so drawing the same thing again sends nothing. */
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "phase_data.h"
#include "series_enc.h"
#include "xpp_globals.h"
#include "xpp_mem.h"
#include "many_pops.h"

extern "C" {
extern char uvar_names[MAXODE][XPP_NAME_MAX + 1];
}

namespace {

PhaseDataEmit emit_line;
bool nullclines_on, dfield_on, values_f32;

/* a flow's points are kept when they move more than this fraction of the
   window's axes from the last point kept: well under a pixel, and a
   trajectory that circles a limit cycle for a long time costs a few
   thousand points a turn instead of one per step */
const double FLOW_STEP = 2e-4;
/* at most this many values of flows per window (x and y together) */
const std::size_t FLOW_MAX = 8000000;

using Floats = std::vector<float>;

bool same_floats(const Floats &a, const Floats &b)
{
    return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

struct Clines {
    Floats x, y; /* segments, 4 values each */
    bool operator==(const Clines &o) const { return same_floats(x, o.x) && same_floats(y, o.y); }
};

struct Nullclines {
    int ix = 0, iy = 0, xcolor = 0, ycolor = 0;
    Clines now;
    std::vector<Clines> frozen;
    bool operator==(const Nullclines &o) const
    {
        return ix == o.ix && iy == o.iy && xcolor == o.xcolor && ycolor == o.ycolor && now == o.now
               && frozen == o.frozen;
    }
};

struct FlowCurve {
    int color = 0;
    Floats x, y; /* the trajectories one after the other, NaN between two */
    /* thinning: the last point kept, and the last one seen when it was not kept */
    float kx = 0, ky = 0, px = 0, py = 0;
    bool pending = false;
    bool operator==(const FlowCurve &o) const { return color == o.color && same_floats(x, o.x) && same_floats(y, o.y); }
};

struct Field {
    int n = 0, scaled = 0, color = 0;
    double du = 0, dv = 0;
    Floats grid;  /* x, y, ux, uy per arrow */
    Floats speed; /* one per arrow */
    std::vector<FlowCurve> flows; /* one per curve of the window */
    bool operator==(const Field &o) const
    {
        return n == o.n && scaled == o.scaled && color == o.color && std::memcmp(&du, &o.du, sizeof du) == 0
               && std::memcmp(&dv, &o.dv, sizeof dv) == 0 && same_floats(grid, o.grid)
               && same_floats(speed, o.speed) && flows == o.flows;
    }
};

struct Window {
    Nullclines nc, nc_sent;
    Field df, df_sent;
    bool nc_valid = false, df_valid = false; /* the client got nc_sent, df_sent */
    unsigned long trajectory = 0;           /* the flow trajectory its last point was from */
};

Window windows[MAXPOP];

bool flowing;
unsigned long trajectory;

Window *current()
{
    if (!emit_line || plot_windows.active < 0 || plot_windows.active >= MAXPOP) return nullptr;
    return &windows[plot_windows.active];
}

/* ---- flows ---- */

void flow_flush(FlowCurve &c)
{
    if (!c.pending) return;
    c.x.push_back(c.px);
    c.y.push_back(c.py);
    c.kx = c.px;
    c.ky = c.py;
    c.pending = false;
}

void flow_point(FlowCurve &c, float x, float y, double ex, double ey)
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

void flow_start_point(FlowCurve &c, float x, float y)
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

std::size_t flow_values(const Field &f)
{
    std::size_t n = 0;
    for (const FlowCurve &c : f.flows) n += c.x.size() + c.y.size();
    return n;
}

/* ---- JSON text ---- */

void add_int(std::string &o, long v) { o += std::to_string(v); }

void add_num(std::string &o, double v)
{
    char t[32];
    if (!std::isfinite(v)) {
        o += "null";
        return;
    }
    std::snprintf(t, sizeof t, "%.15g", v);
    if (std::strtod(t, nullptr) != v) std::snprintf(t, sizeof t, "%.17g", v);
    o += t;
}

/* a variable's name ("" for none): the model's own, letters, digits, underscores */
void add_name(std::string &o, int col)
{
    o += '"';
    if (col > 0 && col <= MAXODE)
        for (const char *s = uvar_names[col - 1]; *s; s++)
            if (*s != '"' && *s != '\\' && static_cast<unsigned char>(*s) >= 0x20) o += *s;
    o += '"';
}

void add_values(std::string &o, const Floats &v)
{
    std::size_t n;
    char *t = xpp_series_values(v.data(), static_cast<int>(v.size()), values_f32, &n);
    if (t) {
        o.append(t, n);
        xpp_free(t);
    } else o += "[]";
}

void begin_event(std::string &o, const char *ev, int pop)
{
    o = "{\"ev\":\"";
    o += ev;
    o += "\",\"win\":";
    add_int(o, static_cast<long>(plot_windows.graph[pop].w));
    if (values_f32) o += ",\"enc\":\"f32\"";
}

void send_nullclines(int pop, const Nullclines &nc)
{
    std::string o;
    begin_event(o, "nullclines", pop);
    o += ",\"xname\":";
    add_name(o, nc.ix);
    o += ",\"yname\":";
    add_name(o, nc.iy);
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
    emit_line(o.data(), o.size());
}

void send_dfield(int pop, const Field &f)
{
    std::string o;
    begin_event(o, "dfield", pop);
    o += ",\"scaled\":";
    add_int(o, f.scaled);
    o += ",\"color\":";
    add_int(o, f.color);
    o += ",\"n\":";
    add_int(o, f.n);
    o += ",\"du\":";
    add_num(o, f.du);
    o += ",\"dv\":";
    add_num(o, f.dv);
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
    emit_line(o.data(), o.size());
}

void update()
{
    for (int k = 0; k < MAXPOP; k++) {
        const int pop = k == 0 ? plot_windows.active : (k == plot_windows.active ? 0 : k); /* the active window first */
        Window &w = windows[pop];
        if (!plot_windows.graph[pop].Use) {
            w = Window(); /* a window made again later starts afresh */
            continue;
        }
        if (nullclines_on && !(w.nc_valid && w.nc == w.nc_sent)) {
            send_nullclines(pop, w.nc);
            w.nc_sent = w.nc;
            w.nc_valid = true;
        }
        if (dfield_on && !(w.df_valid && w.df == w.df_sent)) {
            send_dfield(pop, w.df);
            w.df_sent = w.df;
            w.df_valid = true;
        }
    }
}

} // namespace

/* ---- the C API: no exception leaves it (out of memory drops the record) ---- */

extern "C" void phase_data_init(PhaseDataEmit emit) { emit_line = emit; }

extern "C" void phase_data_subscribe(int nullclines, int dfield, int f32)
{
    nullclines_on = nullclines != 0;
    dfield_on = dfield != 0;
    values_f32 = f32 != 0;
    for (Window &w : windows) w.nc_valid = w.df_valid = false; /* the next update sends */
}

extern "C" void phase_data_update(void)
{
    if (!emit_line || (!nullclines_on && !dfield_on)) return;
    try {
        update();
    } catch (...) {
    }
}

extern "C" void phase_data_cleared(int pop)
{
    if (!emit_line || pop < 0 || pop >= MAXPOP) return;
    Window &w = windows[pop];
    w.nc.now = Clines();
    w.nc.frozen.clear();
    w.df = Field();
}

extern "C" void phase_data_nullclines(const float *xn, int nx, const float *yn, int ny, int ix, int iy, int xcolor,
                                      int ycolor)
{
    Window *w = current();
    if (!w) return;
    try {
        Nullclines &nc = w->nc;
        nc.ix = ix;
        nc.iy = iy;
        nc.xcolor = xcolor;
        nc.ycolor = ycolor;
        nc.now.x.assign(xn, xn + 4 * (nx > 0 ? nx : 0));
        nc.now.y.assign(yn, yn + 4 * (ny > 0 ? ny : 0));
    } catch (...) {
        w->nc = Nullclines();
    }
}

extern "C" void phase_data_frozen_begin(void)
{
    if (Window *w = current()) w->nc.frozen.clear();
}

extern "C" void phase_data_frozen(const float *xn, int nx, const float *yn, int ny)
{
    Window *w = current();
    if (!w) return;
    try {
        Clines c;
        c.x.assign(xn, xn + 4 * (nx > 0 ? nx : 0));
        c.y.assign(yn, yn + 4 * (ny > 0 ? ny : 0));
        w->nc.frozen.push_back(std::move(c));
    } catch (...) {
    }
}

extern "C" void phase_data_dfield_begin(int n, double du, double dv, int scaled, int color)
{
    Window *w = current();
    if (!w) return;
    Field &f = w->df;
    f.n = n;
    f.du = du;
    f.dv = dv;
    f.scaled = scaled;
    f.color = color;
    f.grid.clear();
    f.speed.clear();
}

extern "C" void phase_data_arrow(double x, double y, double fx, double fy)
{
    Window *w = current();
    if (!w) return;
    const double s = std::hypot(fx, fy);
    const bool unit = s > 0 && std::isfinite(s);
    try {
        Field &f = w->df;
        const float v[4] = {static_cast<float>(x), static_cast<float>(y), unit ? static_cast<float>(fx / s) : 0.0f,
                            unit ? static_cast<float>(fy / s) : 0.0f};
        f.grid.insert(f.grid.end(), v, v + 4);
        f.speed.push_back(static_cast<float>(s));
    } catch (...) {
        w->df.grid.clear();
        w->df.speed.clear();
    }
}

extern "C" void phase_data_flow_start(void) { flowing = emit_line != nullptr; }

extern "C" void phase_data_flow_next(void) { trajectory++; }

extern "C" void phase_data_flow_step(int ncurves, const float *ox, const float *oy, const float *x, const float *y,
                                     const int *color)
{
    if (!flowing) return;
    Window *w = current();
    if (!w || plot_windows.graph[plot_windows.active].ThreeDFlag || ncurves <= 0) return;
    try {
        Field &f = w->df;
        if (flow_values(f) > FLOW_MAX) return;
        if (f.flows.size() != static_cast<std::size_t>(ncurves)) f.flows.resize(ncurves);
        const GRAPH &g = plot_windows.graph[plot_windows.active];
        const double ex = std::fabs(g.xhi - g.xlo) * FLOW_STEP, ey = std::fabs(g.yhi - g.ylo) * FLOW_STEP;
        const bool first = w->trajectory != trajectory;
        w->trajectory = trajectory;
        for (int i = 0; i < ncurves; i++) {
            FlowCurve &c = f.flows[i];
            c.color = color[i];
            if (first) flow_start_point(c, ox[i], oy[i]);
            flow_point(c, x[i], y[i], ex, ey);
        }
    } catch (...) {
        w->df.flows.clear();
    }
}

extern "C" void phase_data_flow_stop(void)
{
    flowing = false;
    for (Window &w : windows)
        for (FlowCurve &c : w.df.flows) try {
                flow_flush(c);
            } catch (...) {
            }
}
