/* Save session and Open session, and the one reader and writer of the
   files that carry a model (.snapx, .autox): see xpp_session.h. Their
   pure part (the members' names, the manifest, the model's members) is
   snapx.cpp; this file writes and reads the members, each through the
   module that owns its format: the set file (lunch-new.cpp), AUTO's
   members (autox_io.cpp), NPZ (data_formats.cpp), the zip (xpp_zip.cpp).
   The writers and readers of a set file's lines take a FILE *, so those
   members pass through a scratch folder (xpp::TempDir). */
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
#include "autox.h"
#include "data_formats.h"
#include "browse.h"
#include "graf_par.h"
#include "xpp_math.h"
#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

/* a set file's io_int/io_double/io_string: reading or writing
   (lunch-new.cpp's READEM and WRITEM) */
constexpr int reading = 1, writing = 0;

/* the data table's size above which Save session asks to leave it out */
constexpr std::uint64_t large_data = 50ull * 1024 * 1024;

/* what Save session offers */
std::string default_name(const xpp::Model &m) { return xpp_session_file_name(m, xpp::snapx::extension); }

/* name, or when it is NULL/empty the one the user picks with title
   (wild the files listed); false on a cancel */
bool name_or_ask(const xpp::Model &m, const char *title, const char *wild, const char *name, std::string &out)
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

void io_bool(int f, FILE *fp, bool &b, const char *name)
{
    int i = b;
    xpp::io_int(&i, fp, f, name);
    b = i != 0;
}

void io_range(int f, FILE *fp, xpp::AxisRange &r, const char *name)
{
    io_bool(f, fp, r.set, name);
    xpp::io_double(&r.lo, fp, f, " low");
    xpp::io_double(&r.hi, fp, f, " high");
}

void io_zoom(int f, FILE *fp, xpp::Zoom &z)
{
    io_range(f, fp, z.x, "zoom x");
    io_range(f, fp, z.y, "zoom y");
}

/* ---- windows.set: the plot windows, AUTO's view, the added columns ---- */

bool write_windows(xpp::Session &s, FILE *fp)
{
    int count = 0;
    for (int i = 0; i < MAXPOP; i++) count += s.plot_windows.graph[i].Use != 0;
    xpp::io_int(&count, fp, writing, "windows");
    xpp::io_int(&s.plot_windows.active, fp, writing, "active");
    for (int i = 0; i < MAXPOP; i++) {
        GRAPH &g = s.plot_windows.graph[i];
        if (!g.Use) continue;
        xpp::io_int(&i, fp, writing, "window");
        xpp::io_int(&g.nvars, fp, writing, "curves");
        for (int j = 0; j < g.nvars; j++) {
            std::string x = xpp::ind_to_sym(s,g.xv[j]), y = xpp::ind_to_sym(s,g.yv[j]), z = xpp::ind_to_sym(s,g.zv[j]);
            xpp::io_string(x, fp, writing);
            xpp::io_string(y, fp, writing);
            xpp::io_string(z, fp, writing);
        }
        xpp::io_string(g.xlabel, fp, writing);
        xpp::io_string(g.ylabel, fp, writing);
        xpp::io_string(g.zlabel, fp, writing);
        xpp::write_graph(fp, g);
        xpp::PlotDisplay &d = s.plot_display[i];
        io_zoom(writing, fp, d.zoom);
        io_bool(writing, fp, d.show_runs, "previous runs");
    }
    xpp::AutoView &v = s.auto_view;
    xpp::io_int(&v.earlier, fp, writing, "AUTO: points before Clear");
    io_bool(writing, fp, v.show_earlier, "AUTO: show them");
    int added = static_cast<int>(s.browser.added_columns.size());
    xpp::io_int(&added, fp, writing, "added columns");
    for (xpp::AddedColumn &c : s.browser.added_columns) {
        xpp::io_string(c.name, fp, writing);
        xpp::io_string(c.formula, fp, writing);
    }
    return true;
}

/* the column name names: the time, a variable, or one of the columns
   added (windows.set's own, which the browser has only once it is
   restored); nothing when the model has none of that name */
std::optional<int> column_named(const xpp::Session &s, const std::vector<xpp::AddedColumn> &added, const std::string &name)
{
    int col;
    find_variable(s, name, &col);
    if (col >= 0) return col;
    for (std::size_t k = 0; k < added.size(); k++)
        if (xpp::equal_ignoring_case(added[k].name, name)) return s.model().neq + 1 + static_cast<int>(k);
    return std::nullopt;
}

/* a curve's variable as windows.set names it, and the line it is on */
struct CurveName {
    std::string name;
    int line = 0;
};

/* what read_windows reads beside the windows */
struct WindowsRead {
    xpp::AutoView auto_view;
    std::vector<xpp::AddedColumn> added;
};

/* what is wrong at the line of fp just read */
[[noreturn]] void wrong_here(FILE *fp, std::string cause)
{
    throw xpp::SetLineError{xpp::lines_read(fp), std::move(cause)};
}

/* windows.set read: when make (restoring) the windows made and set as
   saved, the saved active one active, slot saying which window each saved
   one became; otherwise (xpp_saved_check) read into scratch, slot mapping
   each saved one to itself. SetLineError at a line that does not read or
   names what the model does not have. */
void read_windows(xpp::Session &s, FILE *fp, bool make, std::map<int, int> &slot, WindowsRead &rest)
{
    int count = 0, active = 0;
    xpp::io_int(&count, fp, reading, "windows");
    if (count < 1 || count > MAXPOP) wrong_here(fp, xpp::format("{} windows: there are 1 to {}", count, MAXPOP));
    xpp::io_int(&active, fp, reading, "active");
    const int active_line = xpp::lines_read(fp);
    std::vector<GRAPH> scratch(make ? 0 : static_cast<std::size_t>(count), s.plot_windows.graph[0]);
    std::vector<xpp::PlotDisplay> scratch_display(scratch.size());
    /* each window's curves, set by their names once the added columns are read */
    std::vector<std::pair<GRAPH *, std::vector<CurveName>>> curves;
    for (int k = 0; k < count; k++) {
        int saved = 0, nvars = 0;
        xpp::io_int(&saved, fp, reading, "window");
        if (saved < 0 || saved >= MAXPOP || slot.contains(saved))
            wrong_here(fp, xpp::format("window {} is not one of 0 to {} listed once", saved, MAXPOP - 1));
        xpp::io_int(&nvars, fp, reading, "curves");
        if (nvars < 1 || nvars > MAXPERPLOT) wrong_here(fp, xpp::format("{} curves: a window has 1 to {}", nvars, MAXPERPLOT));
        std::vector<CurveName> names(3 * static_cast<std::size_t>(nvars));
        for (CurveName &n : names) {
            xpp::io_string(n.name, fp, reading);
            n.line = xpp::lines_read(fp);
        }
        std::string xlabel, ylabel, zlabel;
        xpp::io_string(xlabel, fp, reading);
        xpp::io_string(ylabel, fp, reading);
        xpp::io_string(zlabel, fp, reading);
        /* the main window is there; another one is made as Makewindow/
           Create makes it */
        int i = make ? 0 : saved;
        if (make && saved != 0) {
            xpp::make_active(s, 0, 1);
            create_a_pop(s);
            i = s.plot_windows.active;
            if (i == 0) wrong_here(fp, xpp::format("window {} cannot be made", saved));
        }
        GRAPH &g = make ? s.plot_windows.graph[i] : scratch[static_cast<std::size_t>(k)];
        xpp::PlotDisplay &d = make ? s.plot_display[i] : scratch_display[static_cast<std::size_t>(k)];
        xpp::read_graph(fp, g);
        g.nvars = nvars;
        g.xlabel = std::move(xlabel);
        g.ylabel = std::move(ylabel);
        g.zlabel = std::move(zlabel);
        io_zoom(reading, fp, d.zoom);
        io_bool(reading, fp, d.show_runs, "previous runs");
        d.axes_seen = false; /* the zoom is for the axes just read */
        slot[saved] = i;
        curves.emplace_back(&g, std::move(names));
    }
    if (!slot.contains(active))
        throw xpp::SetLineError{active_line, xpp::format("the active window {} is not one of those listed", active)};
    if (make) xpp::make_active(s, slot[active], 1);
    xpp::io_int(&rest.auto_view.earlier, fp, reading, "AUTO: points before Clear");
    io_bool(reading, fp, rest.auto_view.show_earlier, "AUTO: show them");
    int added = 0;
    xpp::io_int(&added, fp, reading, "added columns");
    if (added < 0) wrong_here(fp, xpp::format("{} added columns", added));
    for (int k = 0; k < added; k++) {
        xpp::AddedColumn c;
        xpp::io_string(c.name, fp, reading);
        if (c.name.empty()) wrong_here(fp, "an added column without a name");
        xpp::io_string(c.formula, fp, reading);
        rest.added.push_back(std::move(c));
    }
    for (auto &[g, names] : curves)
        for (std::size_t j = 0; j < names.size() / 3; j++) {
            std::array<int, 3> col{};
            for (std::size_t a = 0; a < 3; a++) {
                const CurveName &n = names[3 * j + a];
                const std::optional<int> c = column_named(s, rest.added, n.name);
                if (!c) throw xpp::SetLineError{n.line, xpp::format("the model has no variable \"{}\"", n.name)};
                col[a] = *c;
            }
            g->xv[j] = col[0];
            g->yv[j] = col[1];
            g->zv[j] = col[2];
        }
}

/* ---- marks.set and frozen.npz: labels, arrows and markers, frozen curves ---- */

bool write_marks(xpp::Session &s, FILE *fp)
{
    int n = 0;
    for (const LABEL &l : s.labels) n += l.use != 0;
    xpp::io_int(&n, fp, writing, "labels");
    for (LABEL &l : s.labels) {
        if (!l.use) continue;
        int win = xpp::graph_of(s, l.w);
        double x = l.x, y = l.y;
        xpp::io_int(&win, fp, writing, "window");
        xpp::io_double(&x, fp, writing, "x");
        xpp::io_double(&y, fp, writing, "y");
        xpp::io_int(&l.size, fp, writing, "size");
        xpp::io_int(&l.font, fp, writing, "font");
        xpp::io_string(l.s, fp, writing);
    }
    n = 0;
    for (const xpp::GROB &g : s.grobs) n += g.use != 0;
    xpp::io_int(&n, fp, writing, "arrows and markers");
    for (xpp::GROB &g : s.grobs) {
        if (!g.use) continue;
        int win = xpp::graph_of(s, g.w);
        double xs = g.xs, ys = g.ys, xe = g.xe, ye = g.ye;
        xpp::io_int(&win, fp, writing, "window");
        xpp::io_int(&g.type, fp, writing, "type");
        xpp::io_int(&g.color, fp, writing, "color");
        xpp::io_double(&g.size, fp, writing, "size");
        xpp::io_double(&xs, fp, writing, "x start");
        xpp::io_double(&ys, fp, writing, "y start");
        xpp::io_double(&xe, fp, writing, "x end");
        xpp::io_double(&ye, fp, writing, "y end");
    }
    n = 0;
    for (const CURVE &c : s.frozen_curves.curve) n += c.use != 0;
    xpp::io_int(&n, fp, writing, "frozen curves");
    xpp::io_int(&s.frozen_curves.auto_freeze, fp, writing, "freeze each run");
    for (int i = 0; i < MAXFRZ; i++) {
        CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        int win = xpp::graph_of(s, c.w), type = c.type;
        xpp::io_int(&i, fp, writing, "slot");
        xpp::io_int(&win, fp, writing, "window");
        xpp::io_int(&type, fp, writing, "type");
        xpp::io_int(&c.color, fp, writing, "color");
        xpp::io_int(&c.len, fp, writing, "points");
        xpp::io_string(c.key, fp, writing);
        xpp::io_string(c.name, fp, writing);
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

/* the window the saved window number just read from fp became (slot,
   read_windows'); SetLineError when windows.set has no such window */
XppWinId window_of(const xpp::Session &s, const std::map<int, int> &slot, FILE *fp, int saved)
{
    const auto it = slot.find(saved);
    if (it == slot.end()) wrong_here(fp, xpp::format("windows.set has no window {}", saved));
    return s.plot_windows.graph[it->second].w;
}

/* a count of marks.set just read from fp: 0 to most */
void count_in_range(FILE *fp, int n, int most, const char *what)
{
    if (n < 0 || n > most) wrong_here(fp, xpp::format("{} {}: there are 0 to {}", n, what, most));
}

/* marks.set read, its frozen curves' points from frozen (frozen.npz's):
   put in place when apply, otherwise (xpp_saved_check) only read.
   SetLineError at a line that does not read, names a window windows.set
   does not have (slot, read_windows') or a frozen curve frozen.npz holds
   no points of. */
void read_marks(xpp::Session &s, FILE *fp, const std::map<int, int> &slot, const xpp::DataTable &frozen, bool apply)
{
    int n = 0;
    xpp::io_int(&n, fp, reading, "labels");
    count_in_range(fp, n, MAXLAB, "labels");
    for (int k = 0; k < n; k++) {
        int win = 0, size = 0, font = 0;
        double x = 0, y = 0;
        std::string text;
        xpp::io_int(&win, fp, reading, "window");
        const XppWinId w = window_of(s, slot, fp, win);
        xpp::io_double(&x, fp, reading, "x");
        xpp::io_double(&y, fp, reading, "y");
        xpp::io_int(&size, fp, reading, "size");
        xpp::io_int(&font, fp, reading, "font");
        xpp::io_string(text, fp, reading);
        if (!apply) continue;
        LABEL &l = s.labels[k]; /* a restored session has none before */
        l.use = 1;
        l.w = w;
        l.x = static_cast<float>(x);
        l.y = static_cast<float>(y);
        l.size = size;
        l.font = font;
        l.s = std::move(text);
    }
    xpp::io_int(&n, fp, reading, "arrows and markers");
    count_in_range(fp, n, MAXGROB, "arrows and markers");
    for (int k = 0; k < n; k++) {
        int win = 0, type = 0, color = 0;
        double size = 0, xs = 0, ys = 0, xe = 0, ye = 0;
        xpp::io_int(&win, fp, reading, "window");
        const XppWinId w = window_of(s, slot, fp, win);
        xpp::io_int(&type, fp, reading, "type");
        xpp::io_int(&color, fp, reading, "color");
        xpp::io_double(&size, fp, reading, "size");
        xpp::io_double(&xs, fp, reading, "x start");
        xpp::io_double(&ys, fp, reading, "y start");
        xpp::io_double(&xe, fp, reading, "x end");
        xpp::io_double(&ye, fp, reading, "y end");
        if (apply)
            s.grobs[k] = xpp::GROB{static_cast<float>(xs), static_cast<float>(ys), static_cast<float>(xe), static_cast<float>(ye),
                              size, 1, w, type, color};
    }
    xpp::io_int(&n, fp, reading, "frozen curves");
    count_in_range(fp, n, MAXFRZ, "frozen curves");
    xpp::io_int(&s.frozen_curves.auto_freeze, fp, reading, "freeze each run");
    std::array<bool, MAXFRZ> listed{};
    for (int k = 0; k < n; k++) {
        int i = 0, win = 0, type = 0, color = 0, len = 0;
        std::string key, name;
        xpp::io_int(&i, fp, reading, "slot");
        if (i < 0 || i >= MAXFRZ || listed[static_cast<std::size_t>(i)])
            wrong_here(fp, xpp::format("frozen curve {} is not one of 0 to {} listed once", i, MAXFRZ - 1));
        listed[static_cast<std::size_t>(i)] = true;
        xpp::io_int(&win, fp, reading, "window");
        const XppWinId w = window_of(s, slot, fp, win);
        xpp::io_int(&type, fp, reading, "type");
        xpp::io_int(&color, fp, reading, "color");
        xpp::io_int(&len, fp, reading, "points");
        const int len_line = xpp::lines_read(fp);
        xpp::io_string(key, fp, reading);
        xpp::io_string(name, fp, reading);
        const std::string array = xpp::format("curve{}_", i);
        std::vector<float> x = points_of(frozen, array + "0", len), y = points_of(frozen, array + "1", len),
                           z = type > 0 ? points_of(frozen, array + "2", len) : std::vector<float>();
        if (len <= 0 || static_cast<int>(x.size()) != len || static_cast<int>(y.size()) != len ||
            (type > 0 && static_cast<int>(z.size()) != len))
            throw xpp::SetLineError{len_line, xpp::format("frozen.npz holds no {} points of frozen curve {}", len, i)};
        if (apply && !restore_frozen_curve(s, i, w, type, color, std::move(key), std::move(name), std::move(x), std::move(y),
                                           std::move(z)))
            throw xpp::SetLineError{len_line, xpp::format("frozen curve {} cannot be restored", i)};
    }
}

/* ---- the whole file ---- */

/* the member name of f read through read(fp) from a scratch copy in tmp
   (read throws SetLineError at a line that does not read, or returns what
   is wrong): what is wrong, the member and the line named ("its
   windows.set, line 12: ...") */
template <class F>
std::optional<std::string> member_read(const xpp::TempDir &tmp, const SavedFile &f, const char *name, F read)
{
    const auto it = f.members.find(name);
    if (it == f.members.end()) return xpp::format("its {} is missing", name);
    const std::string path = tmp.file(name);
    {
        xpp::Writer w = xpp::Writer::binary(path.c_str());
        if (!w || !w.write(it->second) || !w.commit()) return xpp::format("its {} cannot be put in a scratch folder", name);
    }
    /* binary: the lines' numbers are exact (xpp::lines_read) */
    xpp::UniqueFile fp = xpp::open_read_binary(path.c_str());
    if (!fp) return xpp::format("its {} cannot be read back from a scratch folder", name);
    std::optional<std::string> why;
    try {
        why = read(fp.get());
    } catch (const xpp::SetLineError &e) {
        why = e.text();
    }
    if (why) return xpp::format("its {}, {}", name, *why);
    return std::nullopt;
}

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

/* session file f read into s: when apply its model's session after the
   front end's set-up, restored (the values, AUTO's diagram, the windows
   and what they show); otherwise (xpp_saved_check) the session its
   model's load made, before the load keeps it, only its set file read
   into it. What is wrong when a member is missing or does not read. */
std::optional<std::string> read_session(xpp::Session &s, const SavedFile &f, bool apply)
{
    const std::map<std::string, std::string> &mem = f.members;
    xpp::TempDir tmp;
    if (tmp.path().empty()) return std::string("there is no scratch folder to read it in");

    /* the values and numerics (and the active window's graphics: in the
       load's session, which has no active window yet, the main one's) */
    if (!apply) s.plot_windows.current = &s.plot_windows.graph[0];
    if (std::optional<std::string> why = member_read(tmp, f, xpp::snapx::set_member, [&](FILE *fp) -> std::optional<std::string> {
            const xpp::Result<> r = xpp::read_lunch(s, fp, apply);
            if (!r) return r.error().what;
            return std::nullopt;
        }))
        return why;

    /* AUTO's diagram and settings: all of AUTO's members, or none (no diagram) */
    std::optional<xpp::autox::Members> diagram;
    if (std::any_of(mem.begin(), mem.end(), [](const auto &m) { return m.first.starts_with(xpp::snapx::auto_folder); })) {
        std::expected<xpp::autox::Members, std::string> read = xpp::autox::members_read(s, mem, xpp::snapx::auto_folder);
        if (!read) return read.error();
        diagram = std::move(*read);
    }
    if (apply && diagram)
        if (std::optional<std::string> why = xpp::autox::restore_members(s, std::move(*diagram), f.name)) return why;

    /* the windows, then what they show */
    std::map<int, int> slot;
    WindowsRead rest;
    if (std::optional<std::string> why = member_read(tmp, f, xpp::snapx::windows_member, [&](FILE *fp) {
            read_windows(s, fp, apply, slot, rest);
            return std::optional<std::string>();
        }))
        return why;
    if (apply) {
        if (diagram) {
            s.auto_view.earlier = std::min(rest.auto_view.earlier, diagram_count(s.diagram));
            s.auto_view.show_earlier = rest.auto_view.show_earlier && s.auto_view.earlier > 0;
        }
        s.browser.added_columns = std::move(rest.added);
    }

    /* the data table: there when the manifest says so */
    const auto data = mem.find(xpp::snapx::data_member);
    if (f.manifest.data != (data != mem.end()))
        return f.manifest.data ? xpp::format("its {} is missing", xpp::snapx::data_member)
                               : xpp::format("it has a {} its {} does not list", xpp::snapx::data_member, xpp::snapx::manifest_member);
    if (data != mem.end()) {
        xpp::DataTable t;
        if (!xpp::npz_table(data->second, t)) return xpp::format("its {} is not an NPZ file", xpp::snapx::data_member);
        if (std::optional<std::string> why = data_wrong(s, t)) return why;
        if (apply) {
            if (t.rows() > 0 && put_stored_data(s, t) == 0)
                return xpp::format("its {} cannot be put in the data table", xpp::snapx::data_member);
            s.numerics.last_seed = t.seed;
        }
    }

    /* the random numbers' state, one member in three parts: "seed N" (the
       next Go's), "wiener v..." (the Wiener parameters' current values, one
       per wiener of the model) and the generator's text; all or an error.
       A check only proves it reads, leaving the generator as it was */
    const auto rnd = mem.find(xpp::snapx::random_member);
    if (rnd == mem.end()) return xpp::format("its {} is missing", xpp::snapx::random_member);
    {
        const std::string &text = rnd->second;
        const std::size_t nl1 = text.find('\n');
        const std::size_t nl2 = nl1 == std::string::npos ? nl1 : text.find('\n', nl1 + 1);
        int seed = 0;
        if (nl2 == std::string::npos || !text.starts_with("seed ")
            || !xpp::parse_int(std::string_view(text).substr(5, nl1 - 5), seed))
            return xpp::format("its {} does not begin \"seed <number>\", then a wiener line", xpp::snapx::random_member);
        std::vector<double> wieners;
        std::string_view wl = std::string_view(text).substr(nl1 + 1, nl2 - nl1 - 1);
        if (!wl.starts_with("wiener")) return xpp::format("its {} has no wiener line", xpp::snapx::random_member);
        wl.remove_prefix(6);
        while (!wl.empty()) {
            wl.remove_prefix(1); /* the space before each value */
            const std::size_t sp = wl.find(' ');
            double v = 0;
            if (!xpp::parse_number(wl.substr(0, sp), v)) return xpp::format("its {}'s wiener line has a value that is not a number", xpp::snapx::random_member);
            wieners.push_back(v);
            wl = sp == std::string_view::npos ? std::string_view() : wl.substr(sp);
        }
        xpp::Model &m = s.model();
        if (static_cast<int>(wieners.size()) != m.nwiener)
            return xpp::format("its {} has {} wiener values, the model {}", xpp::snapx::random_member, wieners.size(), m.nwiener);
        const std::string before = xpp::rand_state_save();
        if (!xpp::rand_state_load(text.substr(nl2 + 1)))
            return xpp::format("its {} is not a random generator's state", xpp::snapx::random_member);
        if (apply) {
            s.numerics.rand_seed = seed;
            for (int i = 0; i < m.nwiener; i++) s.parser.constants[m.wiener[i]] = wieners[i];
        } else
            xpp::rand_state_load(before);
    }

    /* the marks, the frozen curves' points from frozen.npz */
    xpp::DataTable frozen;
    if (const auto fz = mem.find(xpp::snapx::frozen_member); fz != mem.end() && !xpp::npz_table(fz->second, frozen))
        return xpp::format("its {} is not an NPZ file", xpp::snapx::frozen_member);
    if (std::optional<std::string> why = member_read(tmp, f, xpp::snapx::marks_member, [&](FILE *fp) {
            read_marks(s, fp, slot, frozen, apply);
            return std::optional<std::string>();
        }))
        return why;
    if (!apply) return std::nullopt;

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
    return std::nullopt;
}

/* random.txt's text: the next Go's seed, the Wiener parameters' values, the generator */
std::string random_text(xpp::Session &s)
{
    std::string w = "wiener";
    const xpp::Model &m = s.model();
    for (int i = 0; i < m.nwiener; i++) w += ' ' + xpp::number(s.parser.constants[m.wiener[i]]);
    return xpp::format("seed {}\n{}\n{}", s.numerics.rand_seed, w, xpp::rand_state_save());
}

/* the session file of s, as its bytes: the data table in when data;
   nothing, with an error message, when it cannot be made */
std::optional<std::string> session_bytes(xpp::Session &s, bool data)
{
    xpp::snapx::Manifest man;
    man.data = data;
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp_saved_entries(s, man, xpp::snapx::session_kind);
    if (!entries) return std::nullopt;
    xpp::TempDir tmp;
    if (tmp.path().empty()) {
        xpp::err_msg("Save session: no scratch folder");
        return std::nullopt;
    }
    xpp::redraw_params(); /* as File/Write set does, before write_lunch */
    std::optional<std::string> set = written(tmp, xpp::snapx::set_member, [&s](FILE *fp) {
        xpp::write_lunch(s, fp);
        return true;
    });
    if (!set) {
        xpp::err_msg("Save session: cannot write the set file");
        return std::nullopt;
    }
    entries->push_back({xpp::snapx::set_member, std::move(*set)});
    /* a diagram exists: without its orbits, the save is refused (the error said why) */
    if (diagram_count(s.diagram) > 1 && !xpp::autox::add_members(s, *entries, xpp::snapx::auto_folder)) return std::nullopt;
    std::optional<std::string> windows = written(tmp, xpp::snapx::windows_member, [&s](FILE *fp) { return write_windows(s, fp); });
    std::optional<std::string> marks = written(tmp, xpp::snapx::marks_member, [&s](FILE *fp) { return write_marks(s, fp); });
    if (!windows || !marks) {
        xpp::err_msg("Save session: cannot write the windows");
        return std::nullopt;
    }
    entries->push_back({xpp::snapx::windows_member, std::move(*windows)});
    entries->push_back({xpp::snapx::marks_member, std::move(*marks)});
    if (std::optional<std::string> frozen = frozen_npz(s)) entries->push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries->push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table(s))});
    entries->push_back({xpp::snapx::random_member, random_text(s)});
    return xpp::zip::make_zip(*entries);
}

} // namespace

int xpp_session_save(xpp::Session &s, const char *name_arg, int data)
{
    std::string name;
    if (!name_or_ask(s.model(), "Save session", "*.snapx", name_arg, name)) return 0;
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
    const std::optional<std::string> bytes = session_bytes(s, with_data);
    if (!bytes) return 0;
    xpp::Writer w = xpp::Writer::binary(file.c_str());
    if (!w || !w.write(*bytes) || !w.commit()) {
        xpp::err_msg(xpp::format("Cannot write {}", file));
        return 0;
    }
    s.saved_session = SavedSession{file};
    return 1;
}

std::optional<std::string> xpp_session_snapshot(xpp::Session &s) { return session_bytes(s, false); }

int xpp_session_load(xpp::Session &s, const char *name_arg)
{
    std::string name;
    if (!name_or_ask(s.model(), "Open session", "*.snapx", name_arg, name)) return 0;
    xpp_model_open(s, xpp::snapx::session_file_name(name).c_str());
    return 1;
}

bool xpp_saved_file_name(std::string_view path)
{
    return xpp::snapx::is_session_file(path) || xpp::snapx::has_extension(path, xpp::autox::extension);
}

std::optional<SavedFile> xpp_saved_read(const std::string &path)
{
    const std::string abs = xpp::files::absolute(path);
    std::string bytes;
    if (!xpp::read_bytes(abs.c_str(), bytes)) {
        xpp::err_msg(xpp::format("Cannot open {}", path));
        return std::nullopt;
    }
    return xpp_saved_parse(abs, xpp::files::split_path(path).second, bytes, xpp::snapx::is_session_file(path) ? SavedKind::session : SavedKind::autox);
}

std::optional<SavedFile> xpp_saved_parse(const std::string &path, const std::string &name, std::string_view bytes, SavedKind kind)
{
    SavedFile f;
    f.path = path;
    f.name = name;
    f.session = kind != SavedKind::autox;
    f.snapshot = kind == SavedKind::snapshot;
    const char *what = f.snapshot ? "a session (.snapx)" : f.session ? "a session file (.snapx)" : "an AUTO file (.autox)";
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(bytes);
    if (!entries) {
        xpp::err_msg(xpp::format("{} is not {}: it is not a zip", name, what));
        return std::nullopt;
    }
    const char *manifest = f.session ? xpp::snapx::manifest_member : xpp::autox::manifest_member;
    for (const xpp::zip::Entry &e : *entries) f.members[e.name] = e.bytes;
    if (!f.members.contains(manifest)) {
        xpp::err_msg(xpp::format("{} is not {}: its {} is missing", name, what, manifest));
        return std::nullopt;
    }
    std::expected<xpp::snapx::Manifest, std::string> man =
        xpp::snapx::parse_manifest(f.members[manifest], f.session ? xpp::snapx::session_kind : xpp::autox::kind);
    if (!man) {
        xpp::err_msg(xpp::format("{} is not {} of this version: its {}: {}", name, what, manifest, man.error()));
        return std::nullopt;
    }
    f.manifest = std::move(*man);
    std::optional<std::vector<xpp::ModelFile>> files = xpp::snapx::model_members(*entries, f.manifest.model_name);
    if (!files) {
        xpp::err_msg(xpp::format("{} cannot be opened: its model is missing ({}{})", name, xpp::snapx::model_folder,
                            f.manifest.model_name)
                    .c_str());
        return std::nullopt;
    }
    f.model = xpp::SavedModel{f.path, std::move(*files)};
    return f;
}

std::vector<std::string> xpp_saved_args(const SavedFile &f)
{
    std::vector<std::string> args{f.manifest.model_name};
    if (!f.manifest.anifile.empty()) args.insert(args.end(), {"-anifile", f.manifest.anifile});
    return args;
}

std::optional<std::vector<xpp::zip::Entry>> xpp_saved_entries(const xpp::Session &s, xpp::snapx::Manifest man, std::string_view kind)
{
    const xpp::Model &m = s.model();
    auto has = [&m](const std::string &name) {
        return std::any_of(m.files.begin(), m.files.end(), [&name](const xpp::ModelFile &f) { return f.name == name; });
    };
    if (!has(m.this_file)) {
        xpp::err_msg(xpp::format("{} was not read from a file: it cannot be saved with the model", m.this_file));
        return std::nullopt;
    }
    man.model_name = m.this_file;
    man.anifile = s.animation.options.use_file && has(s.animation.options.file) ? s.animation.options.file : std::string();
    std::vector<xpp::zip::Entry> entries;
    const char *manifest = kind == xpp::snapx::session_kind ? xpp::snapx::manifest_member : xpp::autox::manifest_member;
    entries.push_back({manifest, xpp::snapx::manifest_text(man, kind)});
    xpp::snapx::add_model_members(entries, m.files);
    return entries;
}

std::optional<xpp::Diagnostic> xpp_saved_check(xpp::Session &s, const SavedFile &f)
{
    std::optional<std::string> why;
    if (f.session) why = read_session(s, f, false);
    else if (std::expected<xpp::autox::Members, std::string> read = xpp::autox::members_read(s, f.members, ""); !read)
        why = read.error();
    if (!why) return std::nullopt;
    xpp::Diagnostic d;
    d.file = f.name;
    d.cause = std::move(*why);
    return d;
}

bool xpp_saved_restore(xpp::Session &s, const SavedFile &f)
{
    std::optional<std::string> why;
    if (f.session) why = read_session(s, f, true);
    else if (std::expected<xpp::autox::Members, std::string> read = xpp::autox::members_read(s, f.members, ""); !read)
        why = read.error();
    else {
        if (diagram_count(s.diagram) > 1) yes_reset_auto(s); /* the diagram before goes, with AUTO's files */
        why = xpp::autox::restore_members(s, std::move(*read), f.name);
    }
    if (why) xpp::err_msg(xpp::format("{}: {}", f.name, *why));
    return !why;
}

std::string xpp_session_file_name(const xpp::Model &m, std::string_view ext)
{
    std::string base = xpp::files::split_path(m.this_file).second;
    const std::size_t dot = base.rfind('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    return base + std::string(ext);
}

void xpp_session_warn(const std::string &text)
{
    xpp::log(XPP_LOG_WARN, "{}\n", text);
    xpp::bottom_msg(0, text);
}
