/* Save session and Open session, and the one reader and writer of the
   files that carry a model (.snapx): see xpp_session.h. Their
   pure part (the members' names, the manifest, the model's members) is
   snapx.cpp; this file writes and reads the members, each through the
   module that owns its format: the settings (setx_file.cpp), AUTO's
   members (xpp_session_auto_io.cpp), NPZ (data_formats.cpp), the zip (xpp_zip.cpp).
   Opening one is all or nothing (W125): every member is read whole
   (the settings syntax, setx.h, or xpp_io.h's Lines) and checked
   (read_session), its errors at the member's line ("name.snapx/windows.setx:12"),
   before anything is applied (apply_session). */
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
#include "setx_file.h"
#include "setx.h"
#include "nullcline.h"
#include "diagram.h" /* redraw_diagram; pulls in auto_nox.h */
#include "xpp_session_auto.h"
#include "data_formats.h"
#include "browse.h"
#include "graf_par.h"
#include "xpp_math.h"
#include "colormap.h"
#include "grobs.h"
#include <algorithm>
#include <cmath>
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

/* ---- windows.setx: the plot windows, AUTO's view, the added columns ---- */

/* the most columns a session adds (the room the model has is checked as well) */
constexpr std::size_t max_added_columns = MAXODE;

void write_range(xpp::setx::Out &out, const xpp::AxisRange &r, std::string_view key)
{
    out.begin(key);
    out.flag("set", r.set);
    out.real("low", r.lo);
    out.real("high", r.hi);
    out.end();
}

xpp::AxisRange read_range(xpp::setx::Block &zoom, std::string_view key)
{
    xpp::AxisRange r;
    std::optional<xpp::setx::Block> g = zoom.group(key);
    if (!g) return r;
    g->flag("set", r.set);
    g->real("low", r.lo);
    g->real("high", r.hi);
    g->done();
    return r;
}

xpp::Result<std::string> windows_text(const xpp::Session &s)
{
    xpp::setx::Out out;
    out.begin_list("windows");
    for (int i = 0; i < MAXPOP; i++) {
        const GRAPH &g = s.plot_windows.graph[i];
        if (!g.Use) continue;
        out.begin_item();
        out.whole("number", i);
        xpp::write_window(out, s, g);
        out.text("xlabel", g.xlabel);
        out.text("ylabel", g.ylabel);
        out.text("zlabel", g.zlabel);
        const xpp::PlotDisplay &d = s.plot_display[i];
        out.begin("zoom");
        write_range(out, d.zoom.x, "x");
        write_range(out, d.zoom.y, "y");
        out.end();
        out.flag("previous_runs", d.show_runs);
        out.end_item();
    }
    out.end_list();
    out.whole("active", s.plot_windows.active);
    const xpp::AutoView &v = s.auto_view;
    out.begin("auto_view");
    out.whole("earlier", v.earlier);
    out.flag("show_earlier", v.show_earlier);
    out.end();
    out.begin_list("added_columns");
    for (const xpp::AddedColumn &c : s.browser.added_columns) {
        out.begin_item();
        out.text("name", c.name);
        out.text("formula", c.formula);
        out.end_item();
    }
    out.end_list();
    return out.result("session");
}

/* one window as windows.setx holds it */
struct SavedWindow {
    int saved = 0; /* its number when it was saved */
    GRAPH graph{}; /* its settings and its curves */
    std::string xlabel, ylabel, zlabel;
    xpp::Zoom zoom;
    bool show_runs = true;
};

/* windows.setx read */
struct WindowsRead {
    std::vector<SavedWindow> windows;
    int active = 0;
    xpp::AutoView auto_view;
    std::vector<xpp::AddedColumn> added;
};

/* windows.setx read for s (its model's names; an added column's formula
   compiled in it, nothing of it changed); ReadFailed at a key or value
   that does not read or names what the model does not have */
WindowsRead read_windows(xpp::Session &s, xpp::setx::Block &file)
{
    WindowsRead r;
    /* the added columns first: a window's curves may name them */
    std::vector<xpp::setx::Block> added;
    file.groups("added_columns", added, std::min<std::size_t>(max_added_columns, static_cast<std::size_t>(xpp::added_columns_room(s.model()))));
    for (xpp::setx::Block &g : added) {
        xpp::AddedColumn c;
        g.text("name", c.name);
        if (c.name.empty()) g.fail(*g.member("name"), "an added column without a name");
        g.text("formula", c.formula);
        if (!xpp::added_column_compiles(s, c.formula))
            g.fail(*g.member("formula"), xpp::format("the formula of the added column {} does not compile", c.name));
        g.done();
        r.added.push_back(std::move(c));
    }
    std::vector<xpp::setx::Block> windows;
    file.groups("windows", windows, MAXPOP);
    if (windows.empty()) file.fail(*file.member("windows"), "a session has at least one window");
    std::set<int> listed;
    for (xpp::setx::Block &w : windows) {
        SavedWindow &sw = r.windows.emplace_back();
        w.whole("number", sw.saved);
        if (sw.saved < 0 || sw.saved >= MAXPOP || !listed.insert(sw.saved).second)
            w.fail(*w.member("number"), xpp::format("window {} is not one of 0 to {} listed once", sw.saved, MAXPOP - 1));
        sw.graph = s.plot_windows.graph[0];
        xpp::read_window(w, s, sw.graph, r.added);
        w.text("xlabel", sw.xlabel);
        w.text("ylabel", sw.ylabel);
        w.text("zlabel", sw.zlabel);
        if (std::optional<xpp::setx::Block> z = w.group("zoom")) {
            sw.zoom.x = read_range(*z, "x");
            sw.zoom.y = read_range(*z, "y");
            z->done();
        }
        w.flag("previous_runs", sw.show_runs);
        w.done();
    }
    file.whole("active", r.active);
    if (!listed.contains(r.active))
        file.fail(*file.member("active"), xpp::format("the active window {} is not one of those listed", r.active));
    if (std::optional<xpp::setx::Block> v = file.group("auto_view")) {
        v->whole("earlier", r.auto_view.earlier);
        v->flag("show_earlier", r.auto_view.show_earlier);
        v->done();
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
        xpp::copy_curves(w.graph, g);
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

/* ---- marks.setx and frozen.npz: labels, arrows and markers, frozen curves ---- */

xpp::Result<std::string> marks_text(const xpp::Session &s)
{
    xpp::setx::Out out;
    out.begin_list("labels");
    for (const LABEL &l : s.labels) {
        if (!l.use) continue;
        out.begin_item();
        out.whole("window", xpp::graph_of(s, l.w));
        out.real("x", l.x);
        out.real("y", l.y);
        out.whole("size", l.size);
        out.whole("font", l.font);
        out.whole("style", l.style);
        out.whole("color", l.color);
        out.text("text", l.s);
        out.end_item();
    }
    out.end_list();
    out.begin_list("arrows_and_markers");
    for (const xpp::GROB &g : s.grobs) {
        if (!g.use) continue;
        out.begin_item();
        out.whole("window", xpp::graph_of(s, g.w));
        out.whole("type", g.type);
        out.whole("color", g.color);
        out.real("size", g.size);
        out.real("x_start", g.xs);
        out.real("y_start", g.ys);
        out.real("x_end", g.xe);
        out.real("y_end", g.ye);
        out.end_item();
    }
    out.end_list();
    out.whole("freeze_each_run", s.frozen_curves.auto_freeze);
    out.begin_list("frozen_curves");
    for (int i = 0; i < MAXFRZ; i++) {
        const CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        out.begin_item();
        out.whole("slot", i);
        out.whole("window", xpp::graph_of(s, c.w));
        out.whole("type", c.type);
        out.whole("color", c.color);
        out.whole("points", c.len);
        out.text("key", c.key);
        out.text("name", c.name);
        out.end_item();
    }
    out.end_list();
    return out.result("session");
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

/* marks.setx read: each mark with the saved window it is in */
struct SavedLabel {
    int window = 0, font = 0;
    double x = 0, y = 0;
    xpp::LabelLook look{"", 0, 0, 0};
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

/* a window number of marks.setx: one windows.setx lists */
int window_in(xpp::setx::Block &g, const WindowsRead &w)
{
    int saved = 0;
    g.whole("window", saved);
    if (std::none_of(w.windows.begin(), w.windows.end(), [saved](const SavedWindow &x) { return x.saved == saved; }))
        g.fail(*g.member("window"), xpp::format("windows.setx has no window {}", saved));
    return saved;
}

/* a colour of marks.setx: one a curve or a mark takes (colormap.h) */
int color_in(xpp::setx::Block &g)
{
    int color = 0;
    g.whole("color", color);
    if (color < 0 || color > xpp::LAST_PLOT_COLOR)
        g.fail(*g.member("color"), xpp::format("{} is not a colour (0 to {})", color, xpp::LAST_PLOT_COLOR));
    return color;
}

/* a position of a label or a mark: a finite number */
double position_in(xpp::setx::Block &g, std::string_view key)
{
    double v = 0;
    g.real(key, v);
    return v;
}

/* marks.setx read, its windows those of windows (windows.setx's), its
   frozen curves' points from frozen (frozen.npz's); ReadFailed at a key or
   value that does not read, names a window windows.setx does not have or a
   frozen curve frozen.npz holds no points of */
MarksRead read_marks(xpp::setx::Block &file, const WindowsRead &windows, const xpp::DataTable &frozen)
{
    MarksRead r;
    std::vector<xpp::setx::Block> labels;
    file.groups("labels", labels, MAXLAB);
    for (xpp::setx::Block &g : labels) {
        SavedLabel &m = r.labels.emplace_back();
        m.window = window_in(g, windows);
        m.x = position_in(g, "x");
        m.y = position_in(g, "y");
        g.whole("size", m.look.size);
        if (const std::string p = xpp::label_size_problem(m.look.size); !p.empty()) g.fail(*g.member("size"), p);
        g.whole("font", m.font);
        if (m.font < 0 || m.font >= xpp::LABEL_FONT_COUNT)
            g.fail(*g.member("font"), xpp::format("{} is not a text font (0 to {})", m.font, xpp::LABEL_FONT_COUNT - 1));
        g.whole("style", m.look.style);
        if (const std::string p = xpp::label_style_problem(m.look.style); !p.empty()) g.fail(*g.member("style"), p);
        m.look.color = color_in(g);
        g.text("text", m.look.text);
        if (const std::string p = xpp::label_text_problem(m.look.text); !p.empty()) g.fail(*g.member("text"), p);
        g.done();
    }
    std::vector<xpp::setx::Block> grobs;
    file.groups("arrows_and_markers", grobs, MAXGROB);
    for (xpp::setx::Block &g : grobs) {
        SavedGrob &o = r.grobs.emplace_back();
        o.window = window_in(g, windows);
        g.whole("type", o.type);
        if (o.type < xpp::POINTER || o.type >= xpp::MARKER + xpp::MARKER_SHAPE_COUNT)
            g.fail(*g.member("type"), xpp::format("{} is not an object's type (0 to {})", o.type, xpp::MARKER + xpp::MARKER_SHAPE_COUNT - 1));
        o.color = color_in(g);
        g.number("size", o.size);
        g.number("x_start", o.xs);
        g.number("y_start", o.ys);
        g.number("x_end", o.xe);
        g.number("y_end", o.ye);
        g.done();
    }
    file.whole("freeze_each_run", r.auto_freeze);
    std::array<bool, MAXFRZ> listed{};
    std::vector<xpp::setx::Block> curves;
    file.groups("frozen_curves", curves, MAXFRZ);
    for (xpp::setx::Block &g : curves) {
        SavedCurve &c = r.curves.emplace_back();
        g.whole("slot", c.slot);
        if (c.slot < 0 || c.slot >= MAXFRZ || listed[static_cast<std::size_t>(c.slot)])
            g.fail(*g.member("slot"), xpp::format("frozen curve {} is not one of 0 to {} listed once", c.slot, MAXFRZ - 1));
        listed[static_cast<std::size_t>(c.slot)] = true;
        c.window = window_in(g, windows);
        g.whole("type", c.type);
        c.color = color_in(g);
        int len = 0;
        g.whole("points", len);
        g.text("key", c.key);
        g.text("name", c.name);
        const std::string array = xpp::format("curve{}_", c.slot);
        c.x = points_of(frozen, array + "0", len);
        c.y = points_of(frozen, array + "1", len);
        if (c.type > 0) c.z = points_of(frozen, array + "2", len);
        if (len <= 0 || static_cast<int>(c.x.size()) != len || static_cast<int>(c.y.size()) != len ||
            (c.type > 0 && static_cast<int>(c.z.size()) != len))
            g.fail(*g.member("points"), xpp::format("frozen.npz holds no {} points of frozen curve {}", len, c.slot));
        g.done();
    }
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
        l.size = m.look.size;
        l.font = m.font;
        l.style = m.look.style;
        l.color = m.look.color;
        l.s = std::move(m.look.text);
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

/* the most sliders a session holds (the model's three and those added: a
   page offers a handful) */
constexpr std::size_t max_sliders = 256;

xpp::Result<std::string> sliders_text(const xpp::Session &s)
{
    xpp::setx::Out out;
    out.begin_list("sliders");
    for (const xpp::XppSlider &slider : s.sliders) {
        out.begin_item();
        out.word("name", slider.var);
        out.real("low", slider.lo);
        out.real("high", slider.hi);
        out.real("step", slider.step);
        out.end_item();
    }
    out.end_list();
    return out.result("session");
}

std::vector<xpp::XppSlider> read_sliders(const xpp::Model &m, xpp::setx::Block &file)
{
    std::vector<xpp::setx::Block> items;
    file.groups("sliders", items, max_sliders);
    if (items.size() < XPP_NSLIDERS)
        file.fail(*file.member("sliders"), xpp::format("sliders {}: at least {} model slots required", items.size(), XPP_NSLIDERS));
    std::vector<xpp::XppSlider> sliders;
    for (xpp::setx::Block &g : items) {
        xpp::XppSlider slider;
        g.text("name", slider.var);
        if (const auto why = xpp::slider_wrong(m, slider)) g.fail(*g.member("name"), *why);
        g.number("low", slider.lo);
        g.number("high", slider.hi);
        if (const auto why = xpp::slider_wrong(m, slider)) g.fail(*g.member("high"), *why);
        g.number("step", slider.step);
        if (const auto why = xpp::slider_wrong(m, slider)) g.fail(*g.member("step"), *why);
        g.done();
        sliders.push_back(std::move(slider));
    }
    return sliders;
}

/* a session file, read whole: every member, before any is applied */
struct SessionRead {
    xpp::Settings set;
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
    /* the same for a member in the settings syntax: every key it needs there, none it does not know */
    const auto settings_member = [&](const char *name, auto parse) -> xpp::Result<std::invoke_result_t<decltype(parse) &, xpp::setx::Block &>> {
        const auto it = mem.find(name);
        if (it == mem.end()) return missing(name);
        return xpp::setx::read_block("session", xpp::format("{}/{}", f.name, name), it->second, true, parse);
    };
    SessionRead r;
    auto sliders = settings_member(xpp::snapx::sliders_member, [&](xpp::setx::Block &b) { return read_sliders(s.model(), b); });
    if (!sliders) return std::unexpected(sliders.error());
    r.sliders = std::move(*sliders);
    auto nullclines = settings_member(xpp::snapx::nullclines_member, [&](xpp::setx::Block &b) { return xpp::read_nullclines(s.model(), b); });
    if (!nullclines) return std::unexpected(nullclines.error());
    r.nullclines = std::move(*nullclines);

    /* the values and numerics */
    xpp::Result<xpp::Settings> set = [&]() -> xpp::Result<xpp::Settings> {
        const auto it = mem.find(xpp::snapx::set_member);
        if (it == mem.end()) return missing(xpp::snapx::set_member);
        return xpp::read_settings(s, xpp::format("{}/{}", f.name, xpp::snapx::set_member), it->second, xpp::SettingsUse::session);
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
    xpp::Result<WindowsRead> windows = settings_member(xpp::snapx::windows_member, [&s](xpp::setx::Block &b) { return read_windows(s, b); });
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
        settings_member(xpp::snapx::marks_member, [&](xpp::setx::Block &b) { return read_marks(b, r.windows, frozen); });
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
    xpp::apply_settings(s, r.set, true);

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
    if (r.data && r.data->rows() > 0) {
        refresh_browser(s, s.data_store.rows);
        s.numerics.storflag = 1; /* a run stored these rows: Continue stores on (storing is not in the .set) */
    }

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
    if (!f.snapshot) s.saved_session.file = f.path;
    xpp_session_mark_clean(s);
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
    xpp::redraw_params(); /* the values panel up to date, before the settings are taken */
    /* a member's text, or the error naming the member it could not be written for */
    const auto member = [&entries](const char *name, xpp::Result<std::string> text) -> xpp::Result<> {
        if (!text) {
            text.error().place.file = name;
            return std::unexpected(text.error());
        }
        entries->push_back({name, std::move(*text)});
        return {};
    };
    if (const xpp::Result<> done = member(xpp::snapx::set_member, xpp::setx_text(s, xpp::capture_settings(s, xpp::SettingsUse::session))); !done)
        return std::unexpected(done.error());
    /* a diagram exists: without its orbits, the save is refused */
    if (s.model().node <= NAUTO) {
        if (const xpp::Result<> added = xpp::snapx::auto_members::add_members(s, *entries, xpp::snapx::auto_folder); !added)
            return std::unexpected(added.error());
    }
    if (const xpp::Result<> done = member(xpp::snapx::windows_member, windows_text(s)); !done) return std::unexpected(done.error());
    if (const xpp::Result<> done = member(xpp::snapx::marks_member, marks_text(s)); !done) return std::unexpected(done.error());
    if (std::optional<std::string> frozen = frozen_npz(s)) entries->push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries->push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table(s))});
    entries->push_back({xpp::snapx::random_member, random_text(s)});
    if (const xpp::Result<> done = member(xpp::snapx::sliders_member, sliders_text(s)); !done) return std::unexpected(done.error());
    if (const xpp::Result<> done = member(xpp::snapx::nullclines_member, xpp::nullclines_text(s)); !done) return std::unexpected(done.error());
    return xpp::zip::make_zip(*entries);
}

} // namespace

namespace {

/* the values the changed flag compares (xpp_session.h) */
std::uint64_t values_fingerprint(const xpp::Session &s)
{
    const xpp::Model &m = s.model();
    xpp::Fingerprint f;
    for (int i = 0; i < m.nupar; i++) f.value(s.parser.constants[m.upar_con[i]]);
    for (int i = 0; i < m.node + m.nmarkov; i++) f.value(s.last_ic[i]);
    const xpp::NumericsSettings &n = s.numerics;
    for (const double d : {n.start_time, n.total_time, n.delta_t, n.transient_time, n.min_step, n.max_step, n.tolerance, n.abs_tolerance, n.bound, n.delay}) f.value(d);
    f.value(n.method);
    f.value(n.store_every);
    f.value(s.data_store.rows);
    f.value(s.data_store.current_time);
    return f.result();
}

/* the whole save to `file`; replace: the path is the remembered one, so
   no question about replacing it; copy: a copy, not the session's file */
int save_to(xpp::Session &s, const std::string &file, int data, bool replace, bool copy)
{
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
    const xpp::Result<bool> saved = xpp_session_save_file(s, file, with_data, replace, copy);
    if (!saved) {
        xpp::show_error(saved.error());
        return 0;
    }
    return *saved ? 1 : 0;
}

} // namespace

int xpp_session_save(xpp::Session &s, const char *name_arg, int data, bool copy)
{
    if(!xpp::save_ready(s.model().nlines()>0))return 0;
    std::string name;
    if (!name_or_ask(s.model(), copy ? "Save a copy of the session" : "Save session", "*" + std::string(xpp::snapx::extension), name_arg, name)) return 0;
    return save_to(s, xpp::snapx::session_file_name(name), data, false, copy);
}

int xpp_session_save_here(xpp::Session &s)
{
    if (s.saved_session.file.empty()) return xpp_session_save(s, nullptr, -1);
    if (!xpp::save_ready(s.model().nlines() > 0)) return 0;
    return save_to(s, s.saved_session.file, -1, true, false);
}

xpp::Result<bool> xpp_session_save_file(xpp::Session &s, const std::string &file, bool data, bool replace, bool copy)
{
    const xpp::Result<std::string> bytes = session_bytes(s, data && s.data_store.rows > 0);
    if (!bytes) {
        xpp::ui.save_result(file,false);
        return std::unexpected(bytes.error());
    }
    xpp::Result<> opened;
    xpp::Writer w = replace ? xpp::open_writer_replacing(file, true, &opened) : xpp::open_writer_asking(file, true, &opened);
    if (!opened) return std::unexpected(opened.error());
    if (!w) return false;
    w.write(*bytes);
    if (const xpp::Result<> saved=xpp::commit_save(w); !saved) return std::unexpected(saved.error());
    if (!copy) {
        s.saved_session.file = file;
        xpp_session_mark_clean(s);
    }
    return true;
}

bool xpp_session_changed(const xpp::Session &s)
{
    return values_fingerprint(s) != s.saved_session.clean;
}

void xpp_session_mark_clean(xpp::Session &s)
{
    s.saved_session.clean = values_fingerprint(s);
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
    xpp::Result<std::string> manifest_text = xpp::snapx::manifest_text(man);
    if (!manifest_text) {
        manifest_text.error().place.file = manifest;
        return std::unexpected(manifest_text.error());
    }
    entries.push_back({manifest, std::move(*manifest_text)});
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
