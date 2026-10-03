/* Save session and Open session, and the one reader and writer of the
   files that carry a model (.snapx): see xpp_session.h. Their
   pure part (the members' names, the manifest, the model's members) is
   snapx.cpp; this file writes and reads the members, each through the
   module that owns its format: the set file (lunch-new.cpp), AUTO's
   members (xpp_session_auto_io.cpp), NPZ (data_formats.cpp), the zip (xpp_zip.cpp).
   Opening one is all or nothing (W125): every member is read whole
   through xpp_io.h's Lines and checked (read_session), its errors at the
   member's line ("name.snapx/windows.set:12"), before anything is applied
   (apply_session). The writers of a set file's lines take a FILE *, so
   those members are written through a scratch folder (xpp::TempDir). */
#include "xpp_session.h"
#include "snapx.h"
#include "session.h"
#include "model.h"
#include "model_switch.h"
#include "xpp_ui.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_files.h"
#include "xpp_zip.h"
#include "xpp_util.h"
#include "lunch-new.h"
#include "diagram.h" /* redraw_diagram; pulls in auto_nox.h */
#include "xpp_session_auto.h"
#include "data_formats.h"
#include "browse.h"
#include "graf_par.h"
#include "xpp_math.h"
#include "colormap.h"
#include "grobs.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

/* the data table's size above which Save session asks to leave it out */
constexpr std::uint64_t large_data = 50ull * 1024 * 1024;

/* what Save session offers */
std::string default_name(const xpp::Model &m) { return xpp::files::output_name(m.this_file, xpp::snapx::extension); }

/* name, or when it is NULL/empty the one the user picks with title
   (wild the files listed); false on a cancel */
bool name_or_ask(const xpp::Model &m, const char *title, std::string_view wild, const char *name, std::string &out)
{
    if (name != nullptr && name[0] != 0) {
        out = name;
        return true;
    }
    std::string file = default_name(m);
    xpp::ping();
    if (!xpp::file_selector(title, file, wild)) return false;
    out = file;
    return true;
}

/* what fn(FILE *) writes, through the file name in tmp */
template <class F>
std::optional<std::string> written(const xpp::TempDir &tmp, const char *name, F fn)
{
    const std::string path = tmp.file(name);
    {
        xpp::Writer w(path.c_str());
        if (!w || !fn(w.file()) || !w.commit()) return std::nullopt;
    }
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) return std::nullopt;
    return bytes;
}

/* ---- the set format's yes/no and ranges ---- */

void write_range(FILE *fp, const xpp::AxisRange &r, const char *name)
{
    xpp::write_whole(fp, r.set, name);
    xpp::write_real(fp, r.lo, " low");
    xpp::write_real(fp, r.hi, " high");
}

void write_zoom(FILE *fp, const xpp::Zoom &z)
{
    write_range(fp, z.x, "zoom x");
    write_range(fp, z.y, "zoom y");
}

bool read_bool(xpp::Lines &l, const char *name)
{
    const int i = l.whole(name);
    if (i != 0 && i != 1) l.fail(xpp::format("{} {}: 0 or 1", name, i));
    return i != 0;
}

xpp::AxisRange read_range(xpp::Lines &l, const char *name)
{
    xpp::AxisRange r;
    r.set = read_bool(l, name);
    r.lo = l.real("low");
    r.hi = l.real("high");
    return r;
}

xpp::Zoom read_zoom(xpp::Lines &l)
{
    xpp::Zoom z;
    z.x = read_range(l, "zoom x");
    z.y = read_range(l, "zoom y");
    return z;
}

/* ---- windows.set: the plot windows, AUTO's view, the added columns ---- */

bool write_windows(xpp::Session &s, FILE *fp)
{
    int count = 0;
    for (int i = 0; i < MAXPOP; i++) count += s.plot_windows.graph[i].Use != 0;
    xpp::write_whole(fp, count, "windows");
    xpp::write_whole(fp, s.plot_windows.active, "active");
    for (int i = 0; i < MAXPOP; i++) {
        const GRAPH &g = s.plot_windows.graph[i];
        if (!g.Use) continue;
        xpp::write_whole(fp, i, "window");
        xpp::write_whole(fp, g.nvars, "curves");
        for (int j = 0; j < g.nvars; j++)
            for (int v : {g.xv[j], g.yv[j], g.zv[j]}) xpp::write_text(fp, xpp::ind_to_sym(s, v));
        xpp::write_text(fp, g.xlabel);
        xpp::write_text(fp, g.ylabel);
        xpp::write_text(fp, g.zlabel);
        xpp::write_graph(fp, g);
        const xpp::PlotDisplay &d = s.plot_display[i];
        write_zoom(fp, d.zoom);
        xpp::write_whole(fp, d.show_runs, "previous runs");
    }
    const xpp::AutoView &v = s.auto_view;
    xpp::write_whole(fp, v.earlier, "AUTO: points before Clear");
    xpp::write_whole(fp, v.show_earlier, "AUTO: show them");
    xpp::write_whole(fp, static_cast<int>(s.browser.added_columns.size()), "added columns");
    for (const xpp::AddedColumn &c : s.browser.added_columns) {
        xpp::write_text(fp, c.name);
        xpp::write_text(fp, c.formula);
    }
    return true;
}

/* the column name names: the time, a variable, or one of the columns
   added (windows.set's own, which the browser has only once it is
   restored); nothing when the model has none of that name */
std::optional<int> column_named(const xpp::Session &s, const std::vector<xpp::AddedColumn> &added, std::string_view name)
{
    int col;
    find_variable(s, name, &col);
    if (col >= 0) return col;
    for (std::size_t k = 0; k < added.size(); k++)
        if (xpp::equal_ignoring_case(added[k].name, name)) return s.model().neq + 1 + static_cast<int>(k);
    return std::nullopt;
}

/* one window as windows.set holds it */
struct SavedWindow {
    int saved = 0; /* its number when it was saved */
    int nvars = 0;
    std::vector<std::array<int, 3>> curves; /* each curve's x, y, z columns */
    std::string xlabel, ylabel, zlabel;
    GRAPH graph{}; /* its settings (write_graph's) */
    xpp::Zoom zoom;
    bool show_runs = true;
};

/* windows.set read */
struct WindowsRead {
    std::vector<SavedWindow> windows;
    int active = 0;
    xpp::AutoView auto_view;
    std::vector<xpp::AddedColumn> added;
};

/* windows.set's lines l read for s (its model's names; an added
   column's formula compiled in it, nothing of it changed); ReadFailed at
   a line that does not read or names what the model does not have */
WindowsRead read_windows(xpp::Session &s, xpp::Lines &l)
{
    WindowsRead r;
    const int count = l.whole("windows");
    if (count < 1 || count > MAXPOP) l.fail(xpp::format("{} windows: there are 1 to {}", count, MAXPOP));
    r.active = l.whole("active");
    const int active_line = l.line();
    /* each window's curves' names and their lines, looked up once the
       added columns are read */
    std::vector<std::vector<std::pair<std::string, int>>> names;
    std::set<int> listed;
    for (int k = 0; k < count; k++) {
        SavedWindow &w = r.windows.emplace_back();
        w.saved = l.whole("window");
        if (w.saved < 0 || w.saved >= MAXPOP || !listed.insert(w.saved).second)
            l.fail(xpp::format("window {} is not one of 0 to {} listed once", w.saved, MAXPOP - 1));
        w.nvars = l.whole("curves");
        if (w.nvars < 1 || w.nvars > MAXPERPLOT) l.fail(xpp::format("{} curves: a window has 1 to {}", w.nvars, MAXPERPLOT));
        std::vector<std::pair<std::string, int>> &n = names.emplace_back();
        for (int j = 0; j < 3 * w.nvars; j++) {
            std::string name(l.next("a curve's variable"));
            n.emplace_back(std::move(name), l.line());
        }
        w.xlabel = l.next("the x label");
        w.ylabel = l.next("the y label");
        w.zlabel = l.next("the z label");
        w.graph = s.plot_windows.graph[0];
        xpp::read_graph(l, w.graph);
        w.zoom = read_zoom(l);
        w.show_runs = read_bool(l, "previous runs");
    }
    if (!listed.contains(r.active))
        l.fail(active_line, xpp::format("the active window {} is not one of those listed", r.active));
    r.auto_view.earlier = l.whole("AUTO: points before Clear");
    r.auto_view.show_earlier = read_bool(l, "AUTO: show them");
    const int added = l.whole("added columns");
    const int room = xpp::added_columns_room(s.model());
    if (added < 0 || added > room) l.fail(xpp::format("{} added columns: the model has room for 0 to {}", added, room));
    for (int k = 0; k < added; k++) {
        xpp::AddedColumn c;
        c.name = l.next("an added column's name");
        if (c.name.empty()) l.fail("an added column without a name");
        c.formula = l.next("an added column's formula");
        if (!xpp::added_column_compiles(s, c.formula))
            l.fail(xpp::format("the formula of the added column {} does not compile", c.name));
        r.added.push_back(std::move(c));
    }
    l.end();
    for (std::size_t k = 0; k < r.windows.size(); k++)
        for (std::size_t j = 0; j < names[k].size(); j += 3) {
            std::array<int, 3> col{};
            for (std::size_t a = 0; a < 3; a++) {
                const auto &[name, line] = names[k][j + a];
                const std::optional<int> c = column_named(s, r.added, name);
                if (!c) l.fail(line, xpp::format("the model has no variable \"{}\"", name));
                col[a] = *c;
            }
            r.windows[k].curves.push_back(col);
        }
    return r;
}

/* the windows of w made in s (the main one there; another made as
   Makewindow/Create makes it) and set as saved, the saved active one
   active; slot says which window each saved one became */
void apply_windows(xpp::Session &s, const WindowsRead &r, std::map<int, int> &slot)
{
    for (const SavedWindow &w : r.windows) {
        int i = 0;
        if (w.saved != 0) {
            xpp::make_active(s, 0, 1);
            create_a_pop(s);
            i = s.plot_windows.active;
            /* only a front end with no windows cannot make one */
            if (i == 0) {
                xpp::log(XPP_LOG_WARN, "window {} of the session cannot be made here\n", w.saved);
                continue;
            }
        }
        GRAPH &g = s.plot_windows.graph[i];
        xpp::copy_graph_settings(w.graph, g);
        g.nvars = w.nvars;
        for (std::size_t j = 0; j < w.curves.size(); j++) {
            g.xv[j] = w.curves[j][0];
            g.yv[j] = w.curves[j][1];
            g.zv[j] = w.curves[j][2];
        }
        g.xlabel = w.xlabel;
        g.ylabel = w.ylabel;
        g.zlabel = w.zlabel;
        xpp::PlotDisplay &d = s.plot_display[i];
        d.zoom = w.zoom;
        d.show_runs = w.show_runs;
        d.axes_seen = false; /* the zoom is for the axes just read */
        slot[w.saved] = i;
    }
    if (const auto a = slot.find(r.active); a != slot.end()) xpp::make_active(s, a->second, 1);
}

/* ---- marks.set and frozen.npz: labels, arrows and markers, frozen curves ---- */

bool write_marks(xpp::Session &s, FILE *fp)
{
    int n = 0;
    for (const LABEL &l : s.labels) n += l.use != 0;
    xpp::write_whole(fp, n, "labels");
    for (const LABEL &l : s.labels) {
        if (!l.use) continue;
        xpp::write_whole(fp, xpp::graph_of(s, l.w), "window");
        xpp::write_real(fp, l.x, "x");
        xpp::write_real(fp, l.y, "y");
        xpp::write_whole(fp, l.size, "size");
        xpp::write_whole(fp, l.font, "font");
        xpp::write_text(fp, l.s);
    }
    n = 0;
    for (const xpp::GROB &g : s.grobs) n += g.use != 0;
    xpp::write_whole(fp, n, "arrows and markers");
    for (const xpp::GROB &g : s.grobs) {
        if (!g.use) continue;
        xpp::write_whole(fp, xpp::graph_of(s, g.w), "window");
        xpp::write_whole(fp, g.type, "type");
        xpp::write_whole(fp, g.color, "color");
        xpp::write_real(fp, g.size, "size");
        xpp::write_real(fp, g.xs, "x start");
        xpp::write_real(fp, g.ys, "y start");
        xpp::write_real(fp, g.xe, "x end");
        xpp::write_real(fp, g.ye, "y end");
    }
    n = 0;
    for (const CURVE &c : s.frozen_curves.curve) n += c.use != 0;
    xpp::write_whole(fp, n, "frozen curves");
    xpp::write_whole(fp, s.frozen_curves.auto_freeze, "freeze each run");
    for (int i = 0; i < MAXFRZ; i++) {
        const CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        xpp::write_whole(fp, i, "slot");
        xpp::write_whole(fp, xpp::graph_of(s, c.w), "window");
        xpp::write_whole(fp, c.type, "type");
        xpp::write_whole(fp, c.color, "color");
        xpp::write_whole(fp, c.len, "points");
        xpp::write_text(fp, c.key);
        xpp::write_text(fp, c.name);
    }
    return true;
}

/* the frozen curves' points, one array per curve ("curve<slot>", rows of
   x, y, z) */
std::optional<std::string> frozen_npz(const xpp::Session &s)
{
    xpp::DataTable t;
    t.names = {"curve", "x", "y", "z"};
    t.curves = true;
    t.columns.resize(4);
    for (int i = 0; i < MAXFRZ; i++) {
        const CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        for (int j = 0; j < c.len; j++) {
            t.columns[0].push_back(static_cast<float>(i));
            t.columns[1].push_back(c.xv[j]);
            t.columns[2].push_back(c.yv[j]);
            t.columns[3].push_back(c.zv ? c.zv[j] : 0.0f);
        }
    }
    if (t.columns[0].empty()) return std::nullopt;
    return xpp::npz_bytes(t);
}

/* column name's first n values of t, or none */
std::vector<float> points_of(const xpp::DataTable &t, const std::string &name, int n)
{
    for (std::size_t k = 0; k < t.columns.size(); k++)
        if (xpp::data_column_name(t, k) == name && t.columns[k].size() >= static_cast<std::size_t>(n))
            return std::vector<float>(t.columns[k].begin(), t.columns[k].begin() + n);
    return {};
}

/* marks.set read: each mark with the saved window it is in */
struct SavedLabel {
    int window = 0, size = 0, font = 0;
    double x = 0, y = 0;
    std::string text;
};
struct SavedGrob {
    int window = 0, type = 0, color = 0;
    double size = 0, xs = 0, ys = 0, xe = 0, ye = 0;
};
struct SavedCurve {
    int slot = 0, window = 0, type = 0, color = 0;
    std::string key, name;
    std::vector<float> x, y, z;
};
struct MarksRead {
    std::vector<SavedLabel> labels;
    std::vector<SavedGrob> grobs;
    int auto_freeze = 0;
    std::vector<SavedCurve> curves;
};

/* a count of marks.set just read: 0 to most */
int count_of(xpp::Lines &l, const char *what, int most)
{
    const int n = l.whole(what);
    if (n < 0 || n > most) l.fail(xpp::format("{} {}: there are 0 to {}", n, what, most));
    return n;
}

/* a window number of marks.set: one windows.set lists */
int window_in(xpp::Lines &l, const WindowsRead &w)
{
    const int saved = l.whole("window");
    if (std::none_of(w.windows.begin(), w.windows.end(), [saved](const SavedWindow &x) { return x.saved == saved; }))
        l.fail(xpp::format("windows.set has no window {}", saved));
    return saved;
}

/* a colour of marks.set: one a curve or a mark takes (colormap.h) */
int color_in(xpp::Lines &l)
{
    const int color = l.whole("color");
    if (color < 0 || color > xpp::LAST_PLOT_COLOR) l.fail(xpp::format("{} is not a colour (0 to {})", color, xpp::LAST_PLOT_COLOR));
    return color;
}

/* marks.set's lines l read, its windows those of windows (windows.set's),
   its frozen curves' points from frozen (frozen.npz's); ReadFailed at a
   line that does not read, names a window windows.set does not have or a
   frozen curve frozen.npz holds no points of */
MarksRead read_marks(xpp::Lines &l, const WindowsRead &windows, const xpp::DataTable &frozen)
{
    MarksRead r;
    for (int k = count_of(l, "labels", MAXLAB); k > 0; k--) {
        SavedLabel &m = r.labels.emplace_back();
        m.window = window_in(l, windows);
        m.x = l.real("x");
        m.y = l.real("y");
        m.size = l.whole("size");
        m.font = l.whole("font");
        m.text = l.next("the label's text");
    }
    for (int k = count_of(l, "arrows and markers", MAXGROB); k > 0; k--) {
        SavedGrob &g = r.grobs.emplace_back();
        g.window = window_in(l, windows);
        g.type = l.whole("type");
        if (g.type < xpp::POINTER || g.type >= xpp::MARKER + xpp::MARKER_SHAPE_COUNT)
            l.fail(xpp::format("{} is not an object's type (0 to {})", g.type, xpp::MARKER + xpp::MARKER_SHAPE_COUNT - 1));
        g.color = color_in(l);
        g.size = l.real("size");
        g.xs = l.real("x start");
        g.ys = l.real("y start");
        g.xe = l.real("x end");
        g.ye = l.real("y end");
    }
    std::array<bool, MAXFRZ> listed{};
    const int n = count_of(l, "frozen curves", MAXFRZ);
    r.auto_freeze = l.whole("freeze each run");
    for (int k = 0; k < n; k++) {
        SavedCurve &c = r.curves.emplace_back();
        c.slot = l.whole("slot");
        if (c.slot < 0 || c.slot >= MAXFRZ || listed[static_cast<std::size_t>(c.slot)])
            l.fail(xpp::format("frozen curve {} is not one of 0 to {} listed once", c.slot, MAXFRZ - 1));
        listed[static_cast<std::size_t>(c.slot)] = true;
        c.window = window_in(l, windows);
        c.type = l.whole("type");
        c.color = color_in(l);
        const int len = l.whole("points");
        const int len_line = l.line();
        c.key = l.next("the curve's key");
        c.name = l.next("the curve's name");
        const std::string array = xpp::format("curve{}_", c.slot);
        c.x = points_of(frozen, array + "0", len);
        c.y = points_of(frozen, array + "1", len);
        if (c.type > 0) c.z = points_of(frozen, array + "2", len);
        if (len <= 0 || static_cast<int>(c.x.size()) != len || static_cast<int>(c.y.size()) != len ||
            (c.type > 0 && static_cast<int>(c.z.size()) != len))
            l.fail(len_line, xpp::format("frozen.npz holds no {} points of frozen curve {}", len, c.slot));
    }
    l.end();
    return r;
}

/* r put in place in s (a restored session has none before), the windows
   the saved ones became (slot, apply_windows') */
void apply_marks(xpp::Session &s, MarksRead r, const std::map<int, int> &slot)
{
    const auto w = [&](int saved) { return s.plot_windows.graph[slot.at(saved)].w; };
    for (std::size_t k = 0; k < r.labels.size(); k++) {
        SavedLabel &m = r.labels[k];
        LABEL &l = s.labels[k];
        l.use = 1;
        l.w = w(m.window);
        l.x = static_cast<float>(m.x);
        l.y = static_cast<float>(m.y);
        l.size = m.size;
        l.font = m.font;
        l.s = std::move(m.text);
    }
    for (std::size_t k = 0; k < r.grobs.size(); k++) {
        const SavedGrob &g = r.grobs[k];
        s.grobs[k] = xpp::GROB{static_cast<float>(g.xs), static_cast<float>(g.ys), static_cast<float>(g.xe), static_cast<float>(g.ye),
                               g.size, 1, w(g.window), g.type, g.color};
    }
    s.frozen_curves.auto_freeze = r.auto_freeze;
    for (SavedCurve &c : r.curves)
        restore_frozen_curve(s, c.slot, w(c.window), c.type, c.color, std::move(c.key), std::move(c.name), std::move(c.x),
                             std::move(c.y), std::move(c.z));
}

/* ---- random.txt: the next Go's seed, the Wiener parameters, the generator ---- */

struct RandomRead {
    int seed = 0;
    std::vector<double> wieners;
    std::string generator;
};

/* random.txt's lines l read for the model m: "seed N", then "wiener" and
   one value per wiener of the model, then the generator's state on a line
   of its own, the last (checked by loading it into a generator of its
   own) */
RandomRead read_random(const xpp::Model &m, xpp::Lines &l)
{
    RandomRead r;
    const std::string_view seed = l.next("seed");
    if (!seed.starts_with("seed ") || !xpp::parse_int(seed.substr(5), r.seed)) l.fail("not \"seed <number>\"");
    std::string_view wl = l.next("wiener");
    if (!wl.starts_with("wiener")) l.fail("not \"wiener\" and the Wiener parameters' values");
    wl.remove_prefix(6);
    while (!wl.empty()) {
        wl.remove_prefix(1); /* the space before each value */
        const std::size_t sp = wl.find(' ');
        double v = 0;
        if (!xpp::parse_number(wl.substr(0, sp), v)) l.fail(xpp::format("\"{}\" is not a number", wl.substr(0, sp)));
        r.wieners.push_back(v);
        wl = sp == std::string_view::npos ? std::string_view() : wl.substr(sp);
    }
    if (static_cast<int>(r.wieners.size()) != m.nwiener)
        l.fail(xpp::format("{} wiener values, the model has {}", r.wieners.size(), m.nwiener));
    r.generator = l.next("the random generator's state");
    /* only proves it reads: a generator of its own, not the Session's */
    xpp::Random probe;
    if (!probe.load(r.generator)) l.fail("not a random generator's state");
    l.end();
    return r;
}

/* ---- the whole file ---- */

/* what is wrong with t as the data table of s's model: its columns are
   the stored ones (the time, then each variable), all as long */
std::optional<std::string> data_wrong(const xpp::Session &s, const xpp::DataTable &t)
{
    const std::size_t ncol = static_cast<std::size_t>(s.model().neq) + 1;
    if (t.columns.size() != ncol)
        return xpp::format("its {} has {} columns, the model's data table {}", xpp::snapx::data_member, t.columns.size(), ncol);
    for (std::size_t k = 0; k < ncol; k++) {
        const std::string name = xpp::data_column_name(t, k), want = browse_column_name(s, static_cast<int>(k));
        if (!xpp::equal_ignoring_case(name, want))
            return xpp::format("its {}'s column {} is {}, not {}", xpp::snapx::data_member, k + 1, name, want);
        if (t.columns[k].size() != t.rows())
            return xpp::format("its {}'s column {} is not as long as its first", xpp::snapx::data_member, name);
    }
    return std::nullopt;
}

std::string sliders_text(const xpp::Session &s)
{
    std::string out=xpp::format("{} sliders\n",s.sliders.size());
    for(const xpp::XppSlider &slider:s.sliders)
        out+=xpp::format("{}\n{} low\n{} high\n{} step\n",slider.var,xpp::number(slider.lo),xpp::number(slider.hi),xpp::number(slider.step));
    return out;
}

std::vector<xpp::XppSlider> read_sliders(const xpp::Model &m,xpp::Lines &l)
{
    const int count=l.whole("sliders",true);
    if(count<XPP_NSLIDERS)l.fail(xpp::format("sliders {}: at least {} model slots required",count,XPP_NSLIDERS));
    std::vector<xpp::XppSlider> sliders;
    // Consume input before allocating a slot; archive limits bound hostile counts.
    for(int k=0;k<count;++k){
        xpp::XppSlider slider;
        slider.var=l.next("slider name");
        if(const auto why=xpp::slider_wrong(m,slider))l.fail(*why);
        slider.lo=l.real("low",true);
        slider.hi=l.real("high",true);
        if(const auto why=xpp::slider_wrong(m,slider))l.fail(*why);
        slider.step=l.real("step",true);
        if(const auto why=xpp::slider_wrong(m,slider))l.fail(*why);
        sliders.push_back(std::move(slider));
    }
    l.end();
    return sliders;
}

/* a session file, read whole: every member, before any is applied */
struct SessionRead {
    xpp::SetFile set;
    std::optional<xpp::snapx::auto_members::Members> diagram;
    WindowsRead windows;
    std::optional<xpp::DataTable> data;
    RandomRead random;
    MarksRead marks;
    std::vector<xpp::XppSlider> sliders;
    xpp::NullclineState nullclines;
};

/* session file f read for s (its model's session): each member read and
   checked whole, or the error at the member's line (the member named as
   f.name/member) or about the file (f.name) when a member is missing */
xpp::Result<SessionRead> read_session(xpp::Session &s, const SavedFile &f)
{
    const std::map<std::string, std::string> &mem = f.members;
    const auto missing = [&f](const std::string &member) {
        return xpp::fail("session", xpp::format("its {} is missing", member), xpp::Place{f.name});
    };
    const auto wrong = [&f](std::string what) { return xpp::fail("session", std::move(what), xpp::Place{f.name}); };
    /* member's text read whole by parse(lines), its errors at its lines */
    const auto member = [&](const char *name, auto parse) -> xpp::Result<std::invoke_result_t<decltype(parse) &, xpp::Lines &>> {
        const auto it = mem.find(name);
        if (it == mem.end()) return missing(name);
        return xpp::read_lines("session", xpp::format("{}/{}", f.name, name), it->second, parse);
    };
    SessionRead r;
    auto sliders=member(xpp::snapx::sliders_member,[&](xpp::Lines &l){return read_sliders(s.model(),l);});
    if(!sliders)return std::unexpected(sliders.error());
    r.sliders=std::move(*sliders);
    auto nullclines=member(xpp::snapx::nullclines_member,[&](xpp::Lines &l){return xpp::read_nullclines(s.model(),l);});
    if(!nullclines)return std::unexpected(nullclines.error());
    r.nullclines=std::move(*nullclines);

    /* the values and numerics (and the active window's graphics) */
    xpp::Result<xpp::SetFile> set = [&]() -> xpp::Result<xpp::SetFile> {
        const auto it = mem.find(xpp::snapx::set_member);
        if (it == mem.end()) return missing(xpp::snapx::set_member);
        return xpp::read_session_set(s, xpp::format("{}/{}", f.name, xpp::snapx::set_member), it->second);
    }();
    if (!set) return std::unexpected(set.error());
    r.set = std::move(*set);

    /* Every AUTO-capable session holds its settings/views, with or without a diagram. */
    if (s.model().node <= NAUTO || std::any_of(mem.begin(), mem.end(), [](const auto &m) { return m.first.starts_with(xpp::snapx::auto_folder); })) {
        xpp::Result<xpp::snapx::auto_members::Members> read = xpp::snapx::auto_members::members_read(s, mem, xpp::snapx::auto_folder, f.name);
        if (!read) return std::unexpected(read.error());
        r.diagram = std::move(*read);
    }

    /* the windows, then what they show */
    xpp::Result<WindowsRead> windows = member(xpp::snapx::windows_member, [&s](xpp::Lines &l) { return read_windows(s, l); });
    if (!windows) return std::unexpected(windows.error());
    r.windows = std::move(*windows);

    /* the data table: there when the manifest says so */
    const auto data = mem.find(xpp::snapx::data_member);
    if (f.manifest.data != (data != mem.end()))
        return f.manifest.data ? missing(xpp::snapx::data_member)
                               : wrong(xpp::format("it has a {} its {} does not list", xpp::snapx::data_member, xpp::snapx::manifest_member));
    if (data != mem.end()) {
        xpp::DataTable t;
        if (!xpp::npz_table(data->second, t)) return wrong(xpp::format("its {} is not an NPZ file", xpp::snapx::data_member));
        if (std::optional<std::string> why = data_wrong(s, t)) return wrong(std::move(*why));
        r.data = std::move(t);
    }

    /* the random numbers' state */
    const xpp::Model &m = s.model();
    xpp::Result<RandomRead> random = member(xpp::snapx::random_member, [&m](xpp::Lines &l) { return read_random(m, l); });
    if (!random) return std::unexpected(random.error());
    r.random = std::move(*random);

    /* the marks, the frozen curves' points from frozen.npz */
    xpp::DataTable frozen;
    if (const auto fz = mem.find(xpp::snapx::frozen_member); fz != mem.end() && !xpp::npz_table(fz->second, frozen))
        return wrong(xpp::format("its {} is not an NPZ file", xpp::snapx::frozen_member));
    xpp::Result<MarksRead> marks =
        member(xpp::snapx::marks_member, [&](xpp::Lines &l) { return read_marks(l, r.windows, frozen); });
    if (!marks) return std::unexpected(marks.error());
    r.marks = std::move(*marks);
    return r;
}

/* r, read whole from f, applied to s (its model's session after the
   front end's set-up): the values, AUTO's diagram, the windows and what
   they show, every window drawn. The error, nothing applied, when the data
   table or AUTO's solutions cannot be put in place. */
xpp::Result<> apply_session(xpp::Session &s, SessionRead r, const SavedFile &f)
{
    /* what can fail first: the data table's room, AUTO's solution file */
    if (r.data && r.data->rows() > 0 && put_stored_data(s, *r.data) == 0)
        return xpp::fail("session", xpp::format("its {} cannot be put in the data table", xpp::snapx::data_member), xpp::Place{f.name});
    if (r.diagram)
        if (xpp::Result<> d = xpp::snapx::auto_members::restore_members(s, std::move(*r.diagram)); !d) return d;
    if (r.data) s.numerics.last_seed = r.data->seed;
    xpp::apply_set_file(s, r.set, true);

    std::map<int, int> slot;
    apply_windows(s, r.windows, slot);
    if (r.diagram) {
        s.auto_view.earlier = std::min(r.windows.auto_view.earlier, diagram_count(s.diagram));
        s.auto_view.show_earlier = r.windows.auto_view.show_earlier && s.auto_view.earlier > 0;
    }
    s.browser.added_columns = std::move(r.windows.added);

    s.numerics.rand_seed = r.random.seed;
    const xpp::Model &m = s.model();
    for (int i = 0; i < m.nwiener; i++) s.parser.constants[m.wiener[i]] = r.random.wieners[static_cast<std::size_t>(i)];
    s.random.load(r.random.generator); /* read_random proved it loads */
    /* the added columns computed over the data put in place, with the
       values just restored */
    if (r.data && r.data->rows() > 0) refresh_browser(s, s.data_store.rows);

    apply_marks(s, std::move(r.marks), slot);
    s.sliders=std::move(r.sliders);
    s.nullcline_state=std::move(r.nullclines);
    s.numerics.null_here=s.nullcline_state.num_x_n||s.nullcline_state.num_y_n||s.nullcline_state.frozen_started;

    /* every window drawn as it now is, the active one last */
    const int active = s.plot_windows.active;
    for (int i = 0; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use && i != active) {
            xpp::make_active(s, i, 1);
            redraw_the_graph(s);
        }
    xpp::make_active(s, active, 1);
    redraw_the_graph(s);
    if (!f.snapshot) s.saved_session = SavedSession{f.path};
    return {};
}

/* random.txt's text: the next Go's seed, the Wiener parameters' values, the generator */
std::string random_text(xpp::Session &s)
{
    std::string w = "wiener";
    const xpp::Model &m = s.model();
    for (int i = 0; i < m.nwiener; i++) w += ' ' + xpp::number(s.parser.constants[m.wiener[i]]);
    return xpp::format("seed {}\n{}\n{}", s.numerics.rand_seed, w, s.random.save());
}

/* the session file of s, as its bytes: the data table in when data;
   the error when it cannot be made */
xpp::Result<std::string> session_bytes(xpp::Session &s, bool data)
{
    xpp::snapx::Manifest man;
    man.data = data;
    xpp::Result<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(s, man);
    if (!entries) return std::unexpected(entries.error());
    xpp::TempDir tmp;
    if (tmp.path().empty()) {
        return xpp::fail("save session", "Save session: no scratch folder", xpp::command_place());
    }
    xpp::redraw_params(); /* the values panel up to date, before write_lunch */
    std::optional<std::string> set = written(tmp, xpp::snapx::set_member, [&s](FILE *fp) {
        xpp::write_lunch(s, fp);
        return true;
    });
    if (!set) {
        return xpp::fail("save session", "Save session: cannot write the set file", xpp::command_place());
    }
    entries->push_back({xpp::snapx::set_member, std::move(*set)});
    /* a diagram exists: without its orbits, the save is refused */
    if (s.model().node <= NAUTO) {
        if (const xpp::Result<> added = xpp::snapx::auto_members::add_members(s, *entries, xpp::snapx::auto_folder); !added)
            return std::unexpected(added.error());
    }
    std::optional<std::string> windows = written(tmp, xpp::snapx::windows_member, [&s](FILE *fp) { return write_windows(s, fp); });
    std::optional<std::string> marks = written(tmp, xpp::snapx::marks_member, [&s](FILE *fp) { return write_marks(s, fp); });
    if (!windows || !marks) {
        return xpp::fail("save session", "Save session: cannot write the windows", xpp::command_place());
    }
    entries->push_back({xpp::snapx::windows_member, std::move(*windows)});
    entries->push_back({xpp::snapx::marks_member, std::move(*marks)});
    if (std::optional<std::string> frozen = frozen_npz(s)) entries->push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries->push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table(s))});
    entries->push_back({xpp::snapx::random_member, random_text(s)});
    entries->push_back({xpp::snapx::sliders_member, sliders_text(s)});
    entries->push_back({xpp::snapx::nullclines_member, xpp::nullclines_text(s)});
    return xpp::zip::make_zip(*entries);
}

} // namespace

int xpp_session_save(xpp::Session &s, const char *name_arg, int data)
{
    if(!xpp::save_ready(s.model().nlines()>0))return 0;
    std::string name;
    if (!name_or_ask(s.model(), "Save session", "*" + std::string(xpp::snapx::extension), name_arg, name)) return 0;
    const std::string file = xpp::snapx::session_file_name(name);
    const xpp::Model &m = s.model();

    bool with_data = s.data_store.rows > 0 && data != 0;
    const std::uint64_t data_bytes = static_cast<std::uint64_t>(s.data_store.rows) * static_cast<std::uint64_t>(m.neq + 1) * 8;
    if (with_data && data < 0 && data_bytes > large_data) {
        const std::string q = xpp::format("The data table is {} MB. Save the session without it? (Go computes it again.)",
                                          data_bytes / (1024 * 1024));
        switch (xpp::TwoChoice("Leave it out", "Save it", q, "ls")) {
        case 'l':
            with_data = false;
            break;
        case 's':
            break;
        default:
            return 0;
        }
    }
    const xpp::Result<bool> saved = xpp_session_save_file(s, file, with_data);
    if (!saved) {
        xpp::show_error(saved.error());
        return 0;
    }
    return *saved ? 1 : 0;
}

xpp::Result<bool> xpp_session_save_file(xpp::Session &s, const std::string &file, bool data)
{
    const xpp::Result<std::string> bytes = session_bytes(s, data && s.data_store.rows > 0);
    if (!bytes) {
        xpp::ui.save_result(file,false);
        return std::unexpected(bytes.error());
    }
    xpp::Result<> opened;
    xpp::Writer w=xpp::open_writer_asking(file,true,&opened);
    if (!opened) return std::unexpected(opened.error());
    if (!w) return false;
    w.write(*bytes);
    if (const xpp::Result<> saved=xpp::commit_save(w); !saved) return std::unexpected(saved.error());
    s.saved_session = SavedSession{file};
    return true;
}

std::optional<std::string> xpp_session_snapshot(xpp::Session &s)
{
    xpp::Result<std::string> bytes = session_bytes(s, false);
    if (!bytes) {
        xpp::show_error(bytes.error());
        return std::nullopt;
    }
    return std::move(*bytes);
}

int xpp_session_load(xpp::Session &s, const char *name_arg)
{
    std::string name;
    if (!name_or_ask(s.model(), "Open session", "*" + std::string(xpp::snapx::extension), name_arg, name)) return 0;
    xpp_model_open(s, xpp::snapx::session_file_name(name).c_str());
    return 1;
}

bool xpp_saved_file_name(std::string_view path)
{
    return xpp::snapx::is_session_file(path);
}

std::optional<SavedFile> xpp_saved_read(const std::string &path)
{
    const std::string abs = xpp::files::absolute(path);
    std::string bytes;
    if (!xpp::read_bytes(abs, bytes, xpp::zip::archive_bytes_limit)) {
        xpp::err_reading(path, "cannot be read within the session archive size limit");
        return std::nullopt;
    }
    return xpp_saved_parse(abs, xpp::files::split_path(path).second, bytes, SavedKind::session);
}

std::optional<SavedFile> xpp_saved_parse(const std::string &path, const std::string &name, std::string_view bytes, SavedKind kind)
{
    SavedFile f;
    f.path = path;
    f.name = name;
    f.snapshot = kind == SavedKind::snapshot;
    const char *what = "a session file (.snapx)";
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(bytes);
    if (!entries) {
        xpp::command_error("open", xpp::format("{} is not {}: it is not a valid zip within the archive size and member limits", name, what));
        return std::nullopt;
    }
    const char *manifest = xpp::snapx::manifest_member;
    for (const xpp::zip::Entry &e : *entries) {
        if (e.name.starts_with(xpp::snapx::model_folder) && xpp::files::has_extension(e.name, ".ode")) {
            xpp::show_error(xpp::Error{"open", "a session cannot carry .ode model text", xpp::Place{xpp::format("{}/{}", name, e.name)}});
            return std::nullopt;
        }
        f.members[e.name] = e.bytes;
    }
    if (!f.members.contains(manifest)) {
        xpp::command_error("open", xpp::format("{} is not {}: its {} is missing", name, what, manifest));
        return std::nullopt;
    }
    xpp::Result<xpp::snapx::Manifest> man = xpp::snapx::parse_manifest(
        xpp::format("{}/{}", name, manifest), f.members[manifest]);
    if (!man) {
        xpp::Error e = std::move(man.error());
        e.what = xpp::format("not {} of this version: {}", what, e.what);
        xpp::show_error(e);
        return std::nullopt;
    }
    f.manifest = std::move(*man);
    std::optional<std::vector<xpp::ModelFile>> files = xpp::snapx::model_members(*entries, f.manifest.model_name);
    if (!files) {
        xpp::show_error(xpp::files::open_error("open", name,
            xpp::format("its model is missing ({}{})", xpp::snapx::model_folder, f.manifest.model_name)));
        return std::nullopt;
    }
    f.model = xpp::SavedModel{f.path, std::move(*files)};
    return f;
}

std::vector<std::string> xpp_saved_args(const SavedFile &f)
{
    std::vector<std::string> args{f.manifest.model_name};
    if (!f.manifest.anifile.empty()) args.insert(args.end(), {"--anifile", f.manifest.anifile});
    return args;
}

xpp::Result<std::vector<xpp::zip::Entry>> xpp_saved_entries(const xpp::Session &s, xpp::snapx::Manifest man)
{
    const xpp::Model &m = s.model();
    auto has = [&m](const std::string &name) {
        return std::any_of(m.files.begin(), m.files.end(), [&name](const xpp::ModelFile &f) { return f.name == name; });
    };
    if (!xpp::odex::is_odex(m.this_file))
        return xpp::fail_reading("save session", "a session stores an .odex model only", m.this_file);
    for (const xpp::ModelFile &file : m.files)
        if (xpp::files::has_extension(file.name, ".ode"))
            return xpp::fail_reading("save session", "a session cannot carry .ode model text", file.name);
    if (!has(m.this_file)) {
        return xpp::fail("save session", xpp::format("{} was not read from a file: it cannot be saved with the model", m.this_file),
                         xpp::command_place());
    }
    man.model_name = m.this_file;
    man.anifile = s.animation.options.use_file && has(s.animation.options.file) ? s.animation.options.file : std::string();
    std::vector<xpp::zip::Entry> entries;
    const char *manifest = xpp::snapx::manifest_member;
    entries.push_back({manifest, xpp::snapx::manifest_text(man)});
    xpp::snapx::add_model_members(entries, m.files);
    return entries;
}

std::optional<xpp::Error> xpp_saved_check(xpp::Session &s, const SavedFile &f)
{
    if (xpp::Result<SessionRead> r = read_session(s, f); !r) return r.error();
    return std::nullopt;
}

bool xpp_saved_restore(xpp::Session &s, const SavedFile &f)
{
    xpp::Result<SessionRead> r = read_session(s, f);
    xpp::Result<> done = r ? apply_session(s, std::move(*r), f) : xpp::Result<>(std::unexpected(r.error()));
    if (!done) xpp::show_error(done.error());
    return done.has_value();
}
