/* Save session and Open session, and the one reader and writer of the
   files that carry a model (.snapx): see xpp_session.h. Their
   pure part (the members' names, the manifest, the model's members) is
   snapx.cpp; this file writes and reads the members, each through the
   module that owns its format: the settings (session_settings.cpp), autoX's
   state (xpp_session_autox.cpp), the nullclines (nullcline.cpp), NPZ (data_formats.cpp), the zip (xpp_zip.cpp).
   The state is one file in .odex syntax, the member session.odex: `version`, the par and init lines, the options
   block and one `session { ... }` statement (read by odex_block.h over odex.h's read_state_file).
   Opening one is all or nothing (W125): the state file is read whole and checked
   (read_session), its errors at the line of the file ("name.snapx/session.odex:12"),
   before anything is applied (apply_session). */
#include "numerics_settings.h"
#include "model_options.h"
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
#include "many_pops.h"
#include "session_settings.h"
#include "odex_block.h"
#include "nullcline.h"
#include "diagram.h" /* redraw_diagram; pulls in autox_nox.h */
#include "xpp_session_autox.h"
#include "data_formats.h"
#include "aniparse.h"
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

/* ---- the session's windows: the plot windows, autoX's view, the added columns ---- */

void write_windows(xpp::odex::Out &out, const xpp::Session &s)
{
    out.begin_list("windows");
    for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++) {
        const GRAPH &g = s.plot_windows.graph[i];
        if (!g.Use) continue;
        out.begin_item();
        out.text("name",g.name);
        out.flag("hidden", g.hidden);
        xpp::write_window(out, s, g);
        out.text("xlabel", g.xlabel);
        out.text("ylabel", g.ylabel);
        out.text("zlabel", g.zlabel);
        const xpp::PlotDisplay &d = s.plot_display[i];
        xpp::write_zoom(out, d.zoom);
        out.flag("previous_runs", d.show_runs);
        out.end_item();
    }
    out.end_list();
    out.text("active",s.plot_windows.graph[s.plot_windows.active].name);
    out.flag("data_hidden", s.tools_hidden.data);
    const xpp::AutoxView &v = s.autox_view;
    out.begin("autox_view");
    out.whole("earlier", v.earlier);
    out.flag("show_earlier", v.show_earlier);
    out.end();
    out.begin_list("added_columns");
    for (const xpp::AddedColumn &c : s.data.added_columns) {
        out.begin_item();
        out.text("name", c.name);
        out.text("formula", c.formula);
        out.end_item();
    }
    out.end_list();
}

/* one window as the session holds it */
struct SavedWindow {
    GRAPH graph{}; /* its settings and its curves */
    std::string xlabel, ylabel, zlabel;
    xpp::Zoom zoom;
    bool show_runs = true;
    bool hidden = false;
};

/* the session's windows read */
struct WindowsRead {
    std::vector<SavedWindow> windows;
    int active = 0;
    bool data_hidden = false;
    xpp::AutoxView autox_view;
    std::vector<xpp::AddedColumn> added;
};

/* the session's windows read for s (its model's names; an added column's formula
   compiled in it, nothing of it changed); ReadFailed at a key or value
   that does not read or names what the model does not have */
WindowsRead read_windows(xpp::Session &s, xpp::odex::Block &file, std::size_t &expanded_bytes)
{
    WindowsRead r;
    /* the added columns first: a window's curves may name them */
    std::vector<xpp::odex::Block> added;
    file.groups("added_columns", added, xpp::DataStore::ADDED_COLUMNS);
    for (xpp::odex::Block &g : added) {
        xpp::AddedColumn c;
        g.text("name", c.name);
        if (c.name.empty()) g.fail(*g.member("name"), "an added column without a name");
        g.text("formula", c.formula);
        if (!xpp::added_column_compiles(s, c.formula))
            g.fail(*g.member("formula"), xpp::format("the formula of the added column {} does not compile", c.name));
        g.done();
        r.added.push_back(std::move(c));
    }
    std::vector<xpp::odex::Block> windows;
    xpp::snapx::memory_groups(file, "windows", windows, sizeof(SavedWindow) + sizeof(xpp::odex::Block), expanded_bytes);
    if (windows.empty()) file.fail(*file.member("windows"), "a session has at least one window");
    r.windows.reserve(windows.size());
    std::size_t slots = 0;
    for (xpp::odex::Block &w : windows) {
        SavedWindow &sw = r.windows.emplace_back();
        w.text("name",sw.graph.name);
        if(const std::string problem=xpp::window_name_problem(sw.graph.name);!problem.empty())w.fail(*w.member("name"),problem);
        for(std::size_t j=0;j+1<r.windows.size();++j)
            if(r.windows[j].graph.name==sw.graph.name)w.fail(*w.member("name"),"a window name is listed twice");
        /* Creating a graph copies the main graph's curves as well. */
        const int main_curves = s.plot_windows.graph[0].nvars;
        const std::size_t slot_bytes = sizeof(GRAPH) + sizeof(xpp::PlotDisplay) + sizeof(xpp::MarksShown::Record)
            + sizeof(xpp::PhaseShown::Window) + sizeof(int)
            + static_cast<std::size_t>(main_curves) * (5 * sizeof(int) + 2 * plot_axes * sizeof(float));
        const std::size_t next = r.windows.size();
        if (!xpp::snapx::account_memory(expanded_bytes, next - slots, slot_bytes))
            file.fail(*file.member("windows"), "too many windows for the session memory limit");
        slots = next;
        xpp::copy_graph_settings(s.plot_windows.graph[0], sw.graph);
        xpp::read_window(w, s, sw.graph, r.added, expanded_bytes);
        w.text("xlabel", sw.xlabel);
        w.text("ylabel", sw.ylabel);
        w.text("zlabel", sw.zlabel);
        sw.zoom = xpp::read_zoom(w);
        w.flag("previous_runs", sw.show_runs);
        w.flag("hidden", sw.hidden);
        w.done();
    }
    std::string active;file.text("active",active);
    const auto current=std::ranges::find(r.windows,active,[](const SavedWindow &w){return w.graph.name;});
    if(current==r.windows.end())file.fail(*file.member("active"),xpp::format("Unknown window \"{}\"",active));
    r.active=static_cast<int>(current-r.windows.begin());
    file.flag("data_hidden", r.data_hidden);
    if (std::optional<xpp::odex::Block> v = file.group("autox_view")) {
        v->whole("earlier", r.autox_view.earlier);
        v->flag("show_earlier", r.autox_view.show_earlier);
        v->done();
    }
    return r;
}

/* Restore the named windows in file order, then select the saved one. */
void apply_windows(xpp::Session &s, const WindowsRead &r)
{
    /* Allocate the checked count before creating the graphs. */
    const std::size_t slots = r.windows.size();
    s.plot_windows.graph.reserve(slots);
    s.plot_windows.current = &s.plot_windows.graph[s.plot_windows.active];
    s.plot_display.reserve(slots);
    s.marks_shown.windows.reserve(slots);
    s.phase_shown.windows.reserve(slots);
    s.plot_windows.open.reserve(slots);
    for (int i = 1; i < static_cast<int>(slots); ++i) {
        xpp::make_active(s, 0, 1);
        create_a_pop(s,r.windows[i].graph.name);
    }
    for (int i=0;i<static_cast<int>(slots);++i) {
        const SavedWindow &w=r.windows[i];
        GRAPH &g = s.plot_windows.graph[i];
        xpp::copy_graph_settings(w.graph, g);
        g.name=w.graph.name;
        xpp::copy_curves(w.graph, g);
        g.xlabel = w.xlabel;
        g.ylabel = w.ylabel;
        g.zlabel = w.zlabel;
        xpp::PlotDisplay &d = s.plot_display[i];
        d.zoom = w.zoom;
        d.show_runs = w.show_runs;
        g.hidden = w.hidden;
        d.axes_seen = false; /* the zoom is for the axes just read */
    }
    xpp::set_active_windows(s);
    xpp::make_active(s,r.active,1);
    s.tools_hidden.data = r.data_hidden;
}

/* ---- the session's marks and frozen.npz: labels, arrows and markers, frozen curves ---- */

void write_marks(xpp::odex::Out &out, const xpp::Session &s)
{
    out.begin("marks");
    out.whole("next_annotation_id", s.next_annotation_id);
    out.begin_list("labels");
    for (const LABEL &l : s.labels) {
        out.begin_item();
        out.whole("id", l.id);
        out.text("window",xpp::window_name(s,l.w));
        out.real("x", l.x);
        out.real("y", l.y);
        out.whole("size", l.size);
        out.whole("font", l.font);
        out.word("style", xpp::text_style_word(l.style));
        out.color("color", l.color);
        out.text("text", l.s);
        out.end_item();
    }
    out.end_list();
    out.begin_list("arrows_and_markers");
    for (const xpp::GROB &g : s.grobs) {
        out.begin_item();
        out.whole("id", g.id);
        out.text("window",xpp::window_name(s,g.w));
        out.whole("type", g.type);
        out.color("color", g.color);
        out.real("size", g.size);
        out.real("x_start", g.xs);
        out.real("y_start", g.ys);
        out.real("x_end", g.xe);
        out.real("y_end", g.ye);
        out.end_item();
    }
    out.end_list();
    out.whole("freeze_each_run", s.frozen_curves.autox_freeze);
    out.begin_list("frozen_curves");
    for (int i = 0; i < static_cast<int>(s.frozen_curves.curve.size()); i++) {
        const CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        out.begin_item();
        out.whole("slot", i);
        out.flag("used",c.use!=0);
        out.text("window",xpp::window_name(s,c.w));
        out.whole("type", c.type);
        out.color("color", c.color);
        out.line_style(c.style);
        out.whole("points", c.len);
        out.text("key", c.key);
        out.text("name", c.name);
        out.end_item();
    }
    out.end_list();
    out.end();
}

/* the frozen curves' points, one array per curve ("curve<slot>", rows of
   x, y, z) */
std::optional<std::string> frozen_npz(const xpp::Session &s)
{
    xpp::DataTable t;
    t.names = {"curve", "x", "y", "z"};
    t.curves = true;
    t.columns.resize(4);
    for (int i = 0; i < static_cast<int>(s.frozen_curves.curve.size()); i++) {
        const CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        for (int j = 0; j < c.len; j++) {
            t.columns[0].push_back(static_cast<double>(i));
            t.columns[1].push_back(c.xv[j]);
            t.columns[2].push_back(c.yv[j]);
            t.columns[3].push_back(c.zv ? c.zv[j] : 0.0f);
        }
    }
    if (t.columns[0].empty()) return std::nullopt;
    return xpp::npz_bytes(t);
}

/* Column name's exactly n stored values, or none; a declared count never sizes allocation. */
std::vector<float> points_of(const xpp::DataTable &t, const std::string &name, int n)
{
    for (std::size_t k = 0; k < t.columns.size(); k++)
        if (n>0 && xpp::data_column_name(t, k) == name && t.columns[k].size() == static_cast<std::size_t>(n))
            return std::vector<float>(t.columns[k].begin(), t.columns[k].begin() + n);
    return {};
}

/* the session's marks read: each mark with the saved window it is in */
struct SavedLabel {
    int id = 0, window = 0, font = 0;
    double x = 0, y = 0;
    xpp::LabelLook look{"", 0, xpp::TextStyle::normal, xpp::foreground_color};
};
struct SavedGrob {
    int id = 0, window = 0, type = 0;
    xpp::Color color;
    double size = 0, xs = 0, ys = 0, xe = 0, ye = 0;
};
struct SavedCurve {
    int slot = 0, window = 0, type = 0;
    xpp::Color color;
    xpp::LineStyle style;
    bool used = false;
    std::string key, name;
    std::vector<float> x, y, z;
};
struct MarksRead {
    std::vector<SavedLabel> labels;
    std::vector<SavedGrob> grobs;
    int next_annotation_id = 0;
    int autox_freeze = 0;
    std::vector<SavedCurve> curves;
};

/* a window number of the marks: one the session's windows list */
int window_in(xpp::odex::Block &g, const WindowsRead &w)
{
    std::string name;g.text("window",name);
    const auto window=std::ranges::find(w.windows,name,[](const SavedWindow &v){return v.graph.name;});
    if(window==w.windows.end())g.fail(*g.member("window"),xpp::format("Unknown window \"{}\"",name));
    return static_cast<int>(window-w.windows.begin());
}

/* a color of the marks (colormap.h) */
xpp::Color color_in(xpp::odex::Block &g)
{
    xpp::Color color;
    g.color("color", color);
    return color;
}

/* the id of the label or object in g: not used before (in `taken`, the ids of both lists) and above the
   list's last (the lists are kept in id order), and below next */
int annotation_id_in(xpp::odex::Block &g, int next, int last, std::set<int> &taken)
{
    int id = 0;
    g.whole("id", id);
    if (id < 0 || id >= next) g.fail(*g.member("id"), xpp::format("{} is not an id below next_annotation_id {}", id, next));
    if (id <= last) g.fail(*g.member("id"), xpp::format("id {} is not above the one before it ({})", id, last));
    if (!taken.insert(id).second) g.fail(*g.member("id"), xpp::format("id {} belongs to a text and an arrow, pointer or marker", id));
    return id;
}

/* a position of a label or a mark: a finite number */
double position_in(xpp::odex::Block &g, std::string_view key)
{
    double v = 0;
    g.real(key, v);
    return v;
}

/* the group `marks` of session read, its windows those of windows, its
   frozen curves' points from frozen (frozen.npz's); ReadFailed at a key or
   value that does not read, names a window the session does not have or a
   frozen curve frozen.npz holds no points of */
MarksRead read_marks(xpp::odex::Block &session, const WindowsRead &windows, const xpp::DataTable &frozen, std::size_t &expanded_bytes)
{
    MarksRead r;
    xpp::odex::Block file = *session.group("marks");
    file.whole("next_annotation_id", r.next_annotation_id);
    if (r.next_annotation_id < 0 || r.next_annotation_id > xpp::ANNOTATION_ID_MAX)
        file.fail(*file.member("next_annotation_id"),
                  xpp::format("next_annotation_id {} is not from 0 to {}", r.next_annotation_id, xpp::ANNOTATION_ID_MAX));
    std::set<int> taken;
    std::vector<xpp::odex::Block> labels;
    file.groups("labels", labels, xpp::ANNOTATION_LIST_MAX);
    for (xpp::odex::Block &g : labels) {
        SavedLabel &m = r.labels.emplace_back();
        m.id = annotation_id_in(g, r.next_annotation_id, r.labels.size() > 1 ? r.labels[r.labels.size() - 2].id : -1, taken);
        m.window = window_in(g, windows);
        m.x = position_in(g, "x");
        m.y = position_in(g, "y");
        g.whole("size", m.look.size);
        if (const std::string p = xpp::label_size_problem(m.look.size); !p.empty()) g.fail(*g.member("size"), p);
        g.whole("font", m.font);
        if (m.font < 0 || m.font >= xpp::LABEL_FONT_COUNT)
            g.fail(*g.member("font"), xpp::format("{} is not a text font (0 to {})", m.font, xpp::LABEL_FONT_COUNT - 1));
        g.text_style(m.look.style);
        m.look.color = color_in(g);
        g.text("text", m.look.text);
        if (const std::string p = xpp::label_text_problem(m.look.text); !p.empty()) g.fail(*g.member("text"), p);
        g.done();
    }
    std::vector<xpp::odex::Block> grobs;
    file.groups("arrows_and_markers", grobs, xpp::ANNOTATION_LIST_MAX);
    for (xpp::odex::Block &g : grobs) {
        SavedGrob &o = r.grobs.emplace_back();
        o.id = annotation_id_in(g, r.next_annotation_id, r.grobs.size() > 1 ? r.grobs[r.grobs.size() - 2].id : -1, taken);
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
    file.whole("freeze_each_run", r.autox_freeze);
    std::vector<xpp::odex::Block> curves;
    xpp::snapx::memory_groups(file, "frozen_curves", curves, sizeof(SavedCurve) + sizeof(xpp::odex::Block), expanded_bytes);
    r.curves.reserve(curves.size());
    std::size_t slots = 0;
    for (xpp::odex::Block &g : curves) {
        SavedCurve &c = r.curves.emplace_back();
        g.whole("slot", c.slot);
        if (c.slot < 0 || static_cast<std::size_t>(c.slot) < slots)
            g.fail(*g.member("slot"), "frozen slots must be listed once in index order");
        const std::size_t next = static_cast<std::size_t>(c.slot) + 1;
        constexpr std::size_t slot_bytes = sizeof(CURVE) + sizeof(std::array<std::vector<float>, plot_axes>) + sizeof(unsigned long);
        if (!xpp::snapx::account_memory(expanded_bytes, next - slots, slot_bytes))
            file.fail(*file.member("frozen_curves"), "too many frozen_curves for the session memory limit");
        slots = next;
        g.flag("used",c.used);
        if(!c.used){g.done();continue;}
        c.window = window_in(g, windows);
        g.whole("type", c.type);
        c.color = color_in(g);
        g.line_style(c.style);
        int len = 0;
        g.whole("points", len);
        g.text("key", c.key);
        g.text("name", c.name);
        if (len > 0 && !xpp::snapx::account_memory(expanded_bytes, static_cast<std::size_t>(len), plot_axes * sizeof(float)))
            g.fail(*g.member("points"), "too many frozen curve points for the session memory limit");
        const std::string array = xpp::format("curve{}_", c.slot);
        c.x = points_of(frozen, array + "0", len);
        c.y = points_of(frozen, array + "1", len);
        if (c.type > 0) c.z = points_of(frozen, array + "2", len);
        if (len <= 0 || static_cast<int>(c.x.size()) != len || static_cast<int>(c.y.size()) != len ||
            (c.type > 0 && static_cast<int>(c.z.size()) != len))
            g.fail(*g.member("points"), xpp::format("frozen.npz holds no {} points of frozen curve {}", len, c.slot));
        g.done();
    }
    file.done();
    return r;
}

/* r put in place in s (a restored session has none before), the windows
   apply_windows restored in file order */
void apply_marks(xpp::Session &s, MarksRead r)
{
    const auto w = [&](int saved) { return s.plot_windows.graph[saved].w; };
    s.next_annotation_id = r.next_annotation_id;
    s.labels.clear();
    s.grobs.clear();
    for (SavedLabel &m : r.labels) {
        LABEL &l = s.labels.emplace_back();
        l.id = m.id;
        l.w = w(m.window);
        l.x = static_cast<float>(m.x);
        l.y = static_cast<float>(m.y);
        l.size = m.look.size;
        l.font = m.font;
        l.style = m.look.style;
        l.color = m.look.color;
        l.s = std::move(m.look.text);
    }
    for (const SavedGrob &g : r.grobs)
        s.grobs.push_back(xpp::GROB{static_cast<float>(g.xs), static_cast<float>(g.ys), static_cast<float>(g.xe), static_cast<float>(g.ye),
                                    g.size, g.id, w(g.window), g.type, g.color});
    s.frozen_curves.autox_freeze = r.autox_freeze;
    const std::size_t slots = r.curves.empty() ? 0 : static_cast<std::size_t>(r.curves.back().slot) + 1;
    s.frozen_curves.curve.resize(slots);
    s.frozen_curves.points.resize(slots);
    s.marks_shown.generation.resize(slots);
    for (SavedCurve &c : r.curves)
        if(c.used)restore_frozen_curve(s, c.slot, w(c.window), c.type, c.color, c.style, std::move(c.key), std::move(c.name), std::move(c.x),
                             std::move(c.y), std::move(c.z));
}

/* ---- the session's random state: the next Go's seed, the Wiener parameters, the Ornstein-Uhlenbeck processes, the generator ---- */

struct RandomRead {
    int seed = 0;
    std::vector<double> wieners;
    std::vector<double> ous;
    std::string generator;
};

/* the group `random`: the next Go's seed, the values of the model's Wiener parameters and Ornstein-Uhlenbeck
   processes, and the generator's state */
void write_random(xpp::odex::Out &out, xpp::Session &s)
{
    const xpp::Model &m = s.model();
    std::vector<double> wieners, ous;
    for (int i = 0; i < m.nwiener; i++) wieners.push_back(s.parser.constants[m.wiener[i]]);
    for (const xpp::Model::OuProcess &p : m.ou) ous.push_back(s.parser.constants[p.slot]);
    out.begin("random");
    out.whole("seed", s.numerics.rand_seed);
    out.reals("wiener", wieners);
    out.reals("ou", ous);
    out.text("generator", s.random.save());
    out.end();
}

/* the group `random` of session read for the model m: one value per Wiener parameter and one per
   Ornstein-Uhlenbeck process, the generator's state checked by loading it into a generator of its own */
RandomRead read_random(const xpp::Model &m, xpp::odex::Block &session)
{
    RandomRead r;
    xpp::odex::Block g = *session.group("random");
    g.whole("seed", r.seed);
    const auto values = [&](std::string_view key, std::vector<double> &out, std::size_t count, std::string_view what) {
        g.reals(key, out, count);
        if (out.size() != count) g.fail(*g.member(key), xpp::format("{} {} values, the model has {}", out.size(), what, count));
    };
    values("wiener", r.wieners, static_cast<std::size_t>(m.nwiener), "Wiener");
    values("ou", r.ous, m.ou.size(), "Ornstein-Uhlenbeck");
    g.text("generator", r.generator);
    /* only proves it reads: a generator of its own, not the Session's */
    xpp::Random probe;
    if (!probe.load(r.generator)) g.fail(*g.member("generator"), "not a random generator's state");
    g.done();
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


void write_sliders(xpp::odex::Out &out, const xpp::Session &s)
{
    out.begin_list("sliders");
    for (const xpp::XppSlider &slider : s.sliders) {
        out.begin_item();
        out.name("name", slider.var);
        out.real("low", slider.lo);
        out.real("high", slider.hi);
        out.real("step", slider.step);
        out.end_item();
    }
    out.end_list();
}

/* a session file, read whole: every member, before any is applied */
struct SessionRead {
    xpp::Settings set;
    std::optional<xpp::snapx::autox_members::Members> diagram;
    WindowsRead windows;
    std::optional<xpp::DataTable> data;
    RandomRead random;
    MarksRead marks;
    std::vector<xpp::XppSlider> sliders;
    xpp::NullclineState nullclines;
};

/* session file f read for s (its model's session): the state file read and
   checked whole, or the error at its line (named as f.name/session.odex), or about the file (f.name) when a
   member is missing */
xpp::Result<SessionRead> read_session(xpp::Session &s, const SavedFile &f)
{
    const std::map<std::string, std::string> &mem = f.members;
    const auto missing = [&f](const std::string &member) {
        return xpp::fail("session", xpp::format("its {} is missing", member), xpp::Place{f.name});
    };
    const auto wrong = [&f](std::string what) { return xpp::fail("session", std::move(what), xpp::Place{f.name}); };
    const auto state = mem.find(xpp::snapx::state_member);
    if (state == mem.end()) return missing(xpp::snapx::state_member);
    xpp::Result<xpp::odex::StateFile> file = xpp::odex::read_state_file(
        xpp::odex::FileKind::session, "session", xpp::format("{}/{}", f.name, xpp::snapx::state_member), state->second);
    if (!file) return std::unexpected(file.error());
    SessionRead r;
    try {
        xpp::odex::Block session = file->block(true, "the session");
        xpp::snapx::read_manifest(session);
        r.set = xpp::read_settings(s, *file, session);
        r.sliders = xpp::read_sliders(s.model(), session);
        {
            xpp::odex::Block nullclines = *session.group("nullclines");
            r.nullclines = xpp::read_nullclines(s.model(), nullclines);
        }
        /* Every autoX-capable session holds its settings/views, with or without a diagram. */
        r.diagram = xpp::snapx::autox_members::members_read(s, mem, xpp::snapx::autox_folder, f.name, session);

        /* One expansion budget for all plot lists, including sparse runtime slots. */
        std::size_t expanded_bytes = 0;
        r.windows = read_windows(s, session, expanded_bytes);
        if(!r.diagram->points.empty())for(const auto &view:r.diagram->views.views)
            for(const auto &window:r.windows.windows)
                if(view.name==window.graph.name)return wrong(xpp::format("Window name \"{}\" occurs in both graph and diagram lists",view.name));

        /* the data table: there when the manifest says so */
        const auto data = mem.find(xpp::snapx::data_member);
        if (f.manifest.data != (data != mem.end()))
            return f.manifest.data ? missing(xpp::snapx::data_member)
                                   : wrong(xpp::format("it has a {} its {} does not list", xpp::snapx::data_member, xpp::snapx::state_member));
        if (data != mem.end()) {
            xpp::DataTable t;
            if (!xpp::npz_table(data->second, t)) return wrong(xpp::format("its {} is not an NPZ file", xpp::snapx::data_member));
            if (std::optional<std::string> why = data_wrong(s, t)) return wrong(std::move(*why));
            r.data = std::move(t);
        }

        /* the random numbers' state */
        r.random = read_random(s.model(), session);

        /* the marks, the frozen curves' points from frozen.npz */
        xpp::DataTable frozen;
        if (const auto fz = mem.find(xpp::snapx::frozen_member); fz != mem.end() && !xpp::npz_table(fz->second, frozen))
            return wrong(xpp::format("its {} is not an NPZ file", xpp::snapx::frozen_member));
        r.marks = read_marks(session, r.windows, frozen, expanded_bytes);
        session.done();
    } catch (xpp::ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
    return r;
}

/* r, read whole from f, applied to s (its model's session after the
   front end's set-up): the values, autoX's diagram, the windows and what
   they show, every window drawn. The error, nothing applied, when the data
   table or autoX's solutions cannot be put in place. */
xpp::Result<> apply_session(xpp::Session &s, SessionRead r, const SavedFile &f)
{
    /* what can fail first: the data table's room, autoX's solution file */
    xpp::apply_storage_precision(s, r.set);
    if (r.data && r.data->rows() > 0 && put_stored_data(s, *r.data) == 0)
        return xpp::fail("session", xpp::format("its {} cannot be put in the data table", xpp::snapx::data_member), xpp::Place{f.name});
    if (r.diagram)
        if (xpp::Result<> d = xpp::snapx::autox_members::restore_members(s, std::move(*r.diagram)); !d) return d;
    if (r.data) s.numerics.last_seed = r.data->seed;
    xpp::apply_settings(s, r.set, true);

    apply_windows(s, r.windows);
    if (r.diagram) {
        s.autox_view.earlier = std::min(r.windows.autox_view.earlier, diagram_count(s.diagram));
        s.autox_view.show_earlier = r.windows.autox_view.show_earlier && s.autox_view.earlier > 0;
    }
    s.data.added_columns = std::move(r.windows.added);

    s.numerics.rand_seed = r.random.seed;
    const xpp::Model &m = s.model();
    for (int i = 0; i < m.nwiener; i++) s.parser.constants[m.wiener[i]] = r.random.wieners[static_cast<std::size_t>(i)];
    for (std::size_t i = 0; i < m.ou.size(); i++) s.parser.constants[m.ou[i].slot] = r.random.ous[i];
    s.random.load(r.random.generator); /* read_random proved it loads */
    /* the added columns computed over the data put in place, with the
       values just restored */
    if (r.data && r.data->rows() > 0) {
        refresh_data(s, s.data_store.rows);
        s.numerics.storflag=true; /* a run stored these rows: Continue stores on (storing is not in the .set) */
    }

    apply_marks(s, std::move(r.marks));
    s.sliders=std::move(r.sliders);
    s.nullcline_state=std::move(r.nullclines);
    s.numerics.null_here=s.nullcline_state.num_x_n||s.nullcline_state.num_y_n||s.nullcline_state.frozen_started;

    /* every window drawn as it now is, the active one last */
    const int active = s.plot_windows.active;
    for (int i = 0; i < static_cast<int>(s.plot_windows.graph.size()); i++)
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

/* the state file of s: the settings' version, par and init lines and options block, then the session statement
   whose groups are the manifest, the settings, the sliders, the random state, the nullclines, autoX's state, the
   windows and the marks; the error when it cannot be written */
xpp::Result<std::string> state_text(xpp::Session &s, const xpp::snapx::Manifest &man)
{
    const xpp::Settings settings = xpp::capture_settings(s, xpp::SettingsUse::session);
    xpp::Result<std::string> head = xpp::settings_statements(s, settings);
    if (!head) return std::unexpected(head.error());
    xpp::odex::Out out;
    out.begin_statement("session");
    xpp::snapx::write_manifest(out, man);
    xpp::write_settings(out, s, settings);
    write_sliders(out, s);
    write_random(out, s);
    xpp::write_nullclines(out, s);
    xpp::snapx::autox_members::write_state(out, s);
    write_windows(out, s);
    write_marks(out, s);
    out.end_statement();
    xpp::Result<std::string> body = out.result("session");
    if (!body) return std::unexpected(body.error());
    return *head + *body;
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
    xpp::Result<std::string> state = state_text(s, man);
    if (!state) {
        state.error().place.file = xpp::snapx::state_member;
        return std::unexpected(state.error());
    }
    entries->front().bytes = std::move(*state);
    /* a diagram exists: without its orbits, the save is refused */
    if (const xpp::Result<> added = xpp::snapx::autox_members::add_members(s, *entries, xpp::snapx::autox_folder); !added)
        return std::unexpected(added.error());
    if (std::optional<std::string> frozen = frozen_npz(s)) entries->push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries->push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table(s))});
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
    /* the numerics the session file keeps, whatever the table holds */
    for (const xpp::OptionRow *row : xpp::saved_settings("numerics")) {
        if (row->real) f.value(row->real(s));
        else if (row->whole) f.value(row->whole(s));
        else if (row->boolean) f.value(row->boolean(s) ? 1 : 0);
        else if (row->index) f.value(row->index.read(s));
    }
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

int xpp_session_save(xpp::Session &s, const char *name_arg, int data, bool copy, bool replace)
{
    if(!xpp::save_ready(s.model().nlines()>0))return 0;
    std::string name;
    if (!name_or_ask(s.model(), copy ? "Save a copy of the session" : "Save session", "*" + std::string(xpp::snapx::extension), name_arg, name)) return 0;
    return save_to(s, xpp::snapx::session_file_name(name), data, replace, copy);
}

int xpp_session_save_here(xpp::Session &s, int data)
{
    if (s.saved_session.file.empty()) return xpp_session_save(s, nullptr, data);
    if (!xpp::save_ready(s.model().nlines() > 0)) return 0;
    return save_to(s, s.saved_session.file, data, true, false);
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
    const char *manifest = xpp::snapx::state_member;
    for (const xpp::zip::Entry &e : *entries) {
        if (e.name.starts_with(xpp::snapx::model_folder) && xpp::files::has_extension(e.name, ".ode")) {
            xpp::show_error(xpp::Error{"open", "a session cannot carry foreign .ode or .ani text", xpp::Place{xpp::format("{}/{}", name, e.name)}});
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

xpp::Result<std::vector<xpp::zip::Entry>> xpp_saved_entries(const xpp::Session &s, xpp::snapx::Manifest &man)
{
    const xpp::Model &m = s.model();
    auto has = [&m](const std::string &name) {
        return std::any_of(m.files.begin(), m.files.end(), [&name](const xpp::ModelFile &f) { return f.name == name; });
    };
    if (!xpp::odex::is_odex(m.this_file))
        return xpp::fail_reading("save session", "a session stores an .odex model only", m.this_file);
    for (const xpp::ModelFile &file : m.files)
        if (xpp::files::has_extension(file.name, ".ode") || xpp::files::has_extension(file.name, ".ani"))
            return xpp::fail_reading("save session", "a session cannot carry foreign .ode or .ani text", file.name);
    if (!has(m.this_file)) {
        return xpp::fail("save session", xpp::format("{} was not read from a file: it cannot be saved with the model", m.this_file),
                         xpp::command_place());
    }
    man.model_name = m.this_file;
    man.anifile = s.animation.options.use_file && has(s.animation.options.file) ? s.animation.options.file : std::string();
    std::vector<xpp::zip::Entry> entries;
    const char *manifest = xpp::snapx::state_member;
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
    /* the animation the session was saved with: a file of the model, loaded after it as a .anix's is */
    if (done && !f.manifest.anifile.empty()) {
        s.animation.options.file = f.manifest.anifile;
        s.animation.options.use_file = 1;
        if (const auto loaded = xpp::load_animation(s, s.animation.options.file); !loaded) {
            xpp::log(XPP_LOG_WARN, "{}\n", loaded.error().text());
            s.animation.options.use_file = false;
        }
    }
    return done.has_value();
}
