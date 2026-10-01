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
    ping();
    if (!file_selector(title, file, wild)) return false;
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

/* fn(FILE *) reading bytes, through the file name in tmp */
template <class F>
bool read_as_file(const xpp::TempDir &tmp, const char *name, const std::string &bytes, F fn)
{
    const std::string path = tmp.file(name);
    xpp::Writer w = xpp::Writer::binary(path.c_str());
    if (!w || !w.write(bytes) || !w.commit()) return false;
    xpp::UniqueFile fp = xpp::open_read(path.c_str());
    return fp && fn(fp.get());
}

void io_bool(int f, FILE *fp, bool &b, const char *name)
{
    int i = b;
    io_int(&i, fp, f, name);
    b = i != 0;
}

void io_range(int f, FILE *fp, xpp::AxisRange &r, const char *name)
{
    io_bool(f, fp, r.set, name);
    io_double(&r.lo, fp, f, " low");
    io_double(&r.hi, fp, f, " high");
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
    io_int(&count, fp, writing, "windows");
    io_int(&s.plot_windows.active, fp, writing, "active");
    for (int i = 0; i < MAXPOP; i++) {
        GRAPH &g = s.plot_windows.graph[i];
        if (!g.Use) continue;
        io_int(&i, fp, writing, "window");
        io_int(&g.nvars, fp, writing, "curves");
        for (int j = 0; j < g.nvars; j++) {
            std::string x = ind_to_sym(s,g.xv[j]), y = ind_to_sym(s,g.yv[j]), z = ind_to_sym(s,g.zv[j]);
            io_string(x, fp, writing);
            io_string(y, fp, writing);
            io_string(z, fp, writing);
        }
        io_string(g.xlabel, fp, writing);
        io_string(g.ylabel, fp, writing);
        io_string(g.zlabel, fp, writing);
        write_graph(fp, g);
        xpp::PlotDisplay &d = s.plot_display[i];
        io_zoom(writing, fp, d.zoom);
        io_bool(writing, fp, d.show_runs, "previous runs");
    }
    xpp::AutoView &v = s.auto_view;
    io_int(&v.earlier, fp, writing, "AUTO: points before Clear");
    io_bool(writing, fp, v.show_earlier, "AUTO: show them");
    int added = static_cast<int>(s.browser.added_columns.size());
    io_int(&added, fp, writing, "added columns");
    for (AddedColumn &c : s.browser.added_columns) {
        io_string(c.name, fp, writing);
        io_string(c.formula, fp, writing);
    }
    return true;
}

/* g's curves by their variables' names: a name this model no longer has
   drops its curve */
void curves_by_name(const xpp::Session &s, GRAPH &g, const std::vector<std::string> &names)
{
    const int nvars = std::min(g.nvars, static_cast<int>(names.size() / 3));
    int n = 0;
    for (int j = 0; j < nvars; j++) {
        int x, y, z;
        find_variable(s,names[3 * j], &x);
        find_variable(s,names[3 * j + 1], &y);
        find_variable(s,names[3 * j + 2], &z);
        if (x < 0 || y < 0) continue;
        g.xv[n] = x;
        g.yv[n] = y;
        g.zv[n] = z < 0 ? 0 : z;
        g.line[n] = g.line[j];
        g.color[n] = g.color[j];
        n++;
    }
    if (n == 0) { /* none left: the first variable against time */
        g.xv[0] = 0;
        g.yv[0] = s.model().neq > 0 ? 1 : 0;
        n = 1;
    }
    g.nvars = n;
}

/* what read_windows restores beside the windows */
struct WindowsRead {
    xpp::AutoView auto_view;
    std::vector<AddedColumn> added;
};

/* the windows of windows.set made and set as saved, the saved active one
   active; slot says which window each saved one became */
bool read_windows(xpp::Session &s, FILE *fp, std::map<int, int> &slot, WindowsRead &rest)
{
    int count = 0, active = 0;
    io_int(&count, fp, reading, "");
    io_int(&active, fp, reading, "");
    if (count < 1 || count > MAXPOP) return false;
    for (int k = 0; k < count; k++) {
        int saved = 0, nvars = 0;
        io_int(&saved, fp, reading, "");
        io_int(&nvars, fp, reading, "");
        if (saved < 0 || saved >= MAXPOP || nvars < 0 || nvars > MAXPERPLOT) return false;
        std::vector<std::string> names(3 * static_cast<std::size_t>(nvars));
        for (std::string &n : names) io_string(n, fp, reading);
        std::string xlabel, ylabel, zlabel;
        io_string(xlabel, fp, reading);
        io_string(ylabel, fp, reading);
        io_string(zlabel, fp, reading);
        /* the main window is there; another one is made as Makewindow/
           Create makes it (a front end without windows makes none, and the
           saved one is read and left) */
        int i = 0;
        if (saved != 0) {
            make_active(s, 0, 1);
            create_a_pop(s);
            i = s.plot_windows.active;
        }
        GRAPH scratch = s.plot_windows.graph[0];
        xpp::PlotDisplay scratch_display;
        const bool made = saved == 0 || i != 0;
        GRAPH &g = made ? s.plot_windows.graph[i] : scratch;
        xpp::PlotDisplay &d = made ? s.plot_display[i] : scratch_display;
        read_graph(fp, g);
        g.nvars = nvars;
        curves_by_name(s, g, names);
        g.xlabel = std::move(xlabel);
        g.ylabel = std::move(ylabel);
        g.zlabel = std::move(zlabel);
        io_zoom(reading, fp, d.zoom);
        io_bool(reading, fp, d.show_runs, "");
        d.axes_seen = false; /* the zoom is for the axes just read */
        if (made) slot[saved] = i;
    }
    make_active(s, slot.contains(active) ? slot[active] : 0, 1);
    io_int(&rest.auto_view.earlier, fp, reading, "");
    io_bool(reading, fp, rest.auto_view.show_earlier, "");
    int added = 0;
    io_int(&added, fp, reading, "");
    for (int k = 0; k < added; k++) {
        AddedColumn c;
        io_string(c.name, fp, reading);
        io_string(c.formula, fp, reading);
        if (!c.name.empty()) rest.added.push_back(std::move(c));
    }
    return true;
}

/* ---- marks.set and frozen.npz: labels, arrows and markers, frozen curves ---- */

bool write_marks(xpp::Session &s, FILE *fp)
{
    int n = 0;
    for (const LABEL &l : s.labels) n += l.use != 0;
    io_int(&n, fp, writing, "labels");
    for (LABEL &l : s.labels) {
        if (!l.use) continue;
        int win = graph_of(s, l.w);
        double x = l.x, y = l.y;
        io_int(&win, fp, writing, "window");
        io_double(&x, fp, writing, "x");
        io_double(&y, fp, writing, "y");
        io_int(&l.size, fp, writing, "size");
        io_int(&l.font, fp, writing, "font");
        io_string(l.s, fp, writing);
    }
    n = 0;
    for (const GROB &g : s.grobs) n += g.use != 0;
    io_int(&n, fp, writing, "arrows and markers");
    for (GROB &g : s.grobs) {
        if (!g.use) continue;
        int win = graph_of(s, g.w);
        double xs = g.xs, ys = g.ys, xe = g.xe, ye = g.ye;
        io_int(&win, fp, writing, "window");
        io_int(&g.type, fp, writing, "type");
        io_int(&g.color, fp, writing, "color");
        io_double(&g.size, fp, writing, "size");
        io_double(&xs, fp, writing, "x start");
        io_double(&ys, fp, writing, "y start");
        io_double(&xe, fp, writing, "x end");
        io_double(&ye, fp, writing, "y end");
    }
    n = 0;
    for (const CURVE &c : s.frozen_curves.curve) n += c.use != 0;
    io_int(&n, fp, writing, "frozen curves");
    io_int(&s.frozen_curves.auto_freeze, fp, writing, "freeze each run");
    for (int i = 0; i < MAXFRZ; i++) {
        CURVE &c = s.frozen_curves.curve[i];
        if (!c.use) continue;
        int win = graph_of(s, c.w), type = c.type;
        io_int(&i, fp, writing, "slot");
        io_int(&win, fp, writing, "window");
        io_int(&type, fp, writing, "type");
        io_int(&c.color, fp, writing, "color");
        io_int(&c.len, fp, writing, "points");
        io_string(c.key, fp, writing);
        io_string(c.name, fp, writing);
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

XppWinId window_of(const xpp::Session &s, const std::map<int, int> &slot, int saved)
{
    const auto it = slot.find(saved);
    return s.plot_windows.graph[it == slot.end() ? 0 : it->second].w;
}

bool read_marks(xpp::Session &s, FILE *fp, const std::map<int, int> &slot, const xpp::DataTable &frozen)
{
    int n = 0;
    io_int(&n, fp, reading, "");
    for (int k = 0; k < n; k++) {
        int win = 0, size = 0, font = 0;
        double x = 0, y = 0;
        std::string text;
        io_int(&win, fp, reading, "");
        io_double(&x, fp, reading, "");
        io_double(&y, fp, reading, "");
        io_int(&size, fp, reading, "");
        io_int(&font, fp, reading, "");
        io_string(text, fp, reading);
        for (LABEL &l : s.labels) {
            if (l.use) continue;
            l.use = 1;
            l.w = window_of(s, slot, win);
            l.x = static_cast<float>(x);
            l.y = static_cast<float>(y);
            l.size = size;
            l.font = font;
            l.s = std::move(text);
            break;
        }
    }
    io_int(&n, fp, reading, "");
    for (int k = 0; k < n; k++) {
        int win = 0, type = 0, color = 0;
        double size = 0, xs = 0, ys = 0, xe = 0, ye = 0;
        io_int(&win, fp, reading, "");
        io_int(&type, fp, reading, "");
        io_int(&color, fp, reading, "");
        io_double(&size, fp, reading, "");
        io_double(&xs, fp, reading, "");
        io_double(&ys, fp, reading, "");
        io_double(&xe, fp, reading, "");
        io_double(&ye, fp, reading, "");
        for (GROB &g : s.grobs) {
            if (g.use) continue;
            g = GROB{static_cast<float>(xs), static_cast<float>(ys), static_cast<float>(xe), static_cast<float>(ye),
                     size, 1, window_of(s, slot, win), type, color};
            break;
        }
    }
    io_int(&n, fp, reading, "");
    io_int(&s.frozen_curves.auto_freeze, fp, reading, "");
    for (int k = 0; k < n; k++) {
        int i = 0, win = 0, type = 0, color = 0, len = 0;
        std::string key, name;
        io_int(&i, fp, reading, "");
        io_int(&win, fp, reading, "");
        io_int(&type, fp, reading, "");
        io_int(&color, fp, reading, "");
        io_int(&len, fp, reading, "");
        io_string(key, fp, reading);
        io_string(name, fp, reading);
        const std::string array = xpp::format("curve{}_", i);
        std::vector<float> x = points_of(frozen, array + "0", len), y = points_of(frozen, array + "1", len),
                           z = type > 0 ? points_of(frozen, array + "2", len) : std::vector<float>();
        if (len <= 0 || static_cast<int>(x.size()) != len ||
            !restore_frozen_curve(s,i, window_of(s, slot, win), type, color, std::move(key), std::move(name), std::move(x),
                                  std::move(y), std::move(z)))
            xpp::log(XPP_LOG_WARN, "Open session: frozen curve {} left out\n", i + 1);
    }
    return true;
}

/* ---- the whole file ---- */

/* the bytes of member name, or nullptr when the file has none */
const std::string *member(const std::map<std::string, std::string> &m, const char *name)
{
    if (!m.contains(name)) return nullptr;
    return &m.at(name);
}

/* session file f (its model s's) restored: the values,
   AUTO's diagram, the windows and what they show */
bool restore_session(xpp::Session &s, const SavedFile &f)
{
    const std::map<std::string, std::string> &mem = f.members;
    xpp::TempDir tmp;
    if (tmp.path().empty()) {
        err_msg("Open session: no scratch folder");
        return false;
    }

    /* the values and numerics (and the active window's graphics) */
    if (const std::string *set = member(mem, xpp::snapx::set_member)) {
        if (!read_as_file(tmp, xpp::snapx::set_member, *set, [&s](FILE *fp) { return read_lunch(s, fp) != 0; }))
            xpp_session_warn("Open session: its parameters and numerics could not be read");
    }

    /* AUTO's diagram and settings */
    bool diagram = false;
    if (mem.contains(std::string(xpp::snapx::auto_folder) + xpp::autox::diagram_member)) {
        diagram = xpp::autox::restore_members(s, mem, xpp::snapx::auto_folder, f.path);
        if (!diagram) xpp_session_warn("Open session: AUTO's diagram could not be read");
    }

    /* the windows, then what they show */
    std::map<int, int> slot = {{0, 0}};
    WindowsRead rest;
    if (const std::string *w = member(mem, xpp::snapx::windows_member)) {
        if (!read_as_file(tmp, xpp::snapx::windows_member, *w, [&](FILE *fp) { return read_windows(s, fp, slot, rest); }))
            xpp_session_warn("Open session: its windows could not be read");
    }
    if (diagram) {
        s.auto_view.earlier = std::min(rest.auto_view.earlier, diagram_count(s.diagram));
        s.auto_view.show_earlier = rest.auto_view.show_earlier && s.auto_view.earlier > 0;
    }
    s.browser.added_columns = std::move(rest.added);
    if (const std::string *d = member(mem, xpp::snapx::data_member)) {
        xpp::DataTable t;
        if (!xpp::npz_table(*d, t) || (t.rows() > 0 && put_stored_data(s, t) == 0)) xpp_session_warn("Open session: its data could not be read");
        else s.numerics.last_seed = t.seed;
    }
    if (const std::string *mk = member(mem, xpp::snapx::marks_member)) {
        xpp::DataTable frozen;
        if (const std::string *fz = member(mem, xpp::snapx::frozen_member)) xpp::npz_table(*fz, frozen);
        read_as_file(tmp, xpp::snapx::marks_member, *mk, [&](FILE *fp) { return read_marks(s, fp, slot, frozen); });
    }

    /* every window drawn as it now is, the active one last */
    const int active = s.plot_windows.active;
    for (int i = 0; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use && i != active) {
            make_active(s, i, 1);
            redraw_the_graph(s);
        }
    make_active(s, active, 1);
    redraw_the_graph(s);
    if (!f.snapshot) s.saved_session = SavedSession{f.path};
    return true;
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
        err_msg("Save session: no scratch folder");
        return std::nullopt;
    }
    redraw_params(); /* as File/Write set does, before write_lunch */
    std::optional<std::string> set = written(tmp, xpp::snapx::set_member, [&s](FILE *fp) {
        write_lunch(s, fp);
        return true;
    });
    if (!set) {
        err_msg("Save session: cannot write the set file");
        return std::nullopt;
    }
    entries->push_back({xpp::snapx::set_member, std::move(*set)});
    if (diagram_count(s.diagram) > 1) xpp::autox::add_members(s, *entries, xpp::snapx::auto_folder); /* a diagram exists */
    std::optional<std::string> windows = written(tmp, xpp::snapx::windows_member, [&s](FILE *fp) { return write_windows(s, fp); });
    std::optional<std::string> marks = written(tmp, xpp::snapx::marks_member, [&s](FILE *fp) { return write_marks(s, fp); });
    if (!windows || !marks) {
        err_msg("Save session: cannot write the windows");
        return std::nullopt;
    }
    entries->push_back({xpp::snapx::windows_member, std::move(*windows)});
    entries->push_back({xpp::snapx::marks_member, std::move(*marks)});
    if (std::optional<std::string> frozen = frozen_npz(s)) entries->push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries->push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table(s))});
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
        switch (TwoChoice("Leave it out", "Save it", q.c_str(), "ls")) {
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
        err_msg(xpp::format("Cannot write {}", file).c_str());
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
    const std::string abs = xpp_files_absolute(path);
    std::string bytes;
    if (!xpp::read_bytes(abs.c_str(), bytes)) {
        err_msg(xpp::format("Cannot open {}", path).c_str());
        return std::nullopt;
    }
    return xpp_saved_parse(abs, xpp_files_split_path(path).second, bytes, xpp::snapx::is_session_file(path) ? SavedKind::session : SavedKind::autox);
}

std::optional<SavedFile> xpp_saved_parse(const std::string &path, const std::string &name, std::string_view bytes, SavedKind kind)
{
    SavedFile f;
    f.path = path;
    f.session = kind != SavedKind::autox;
    f.snapshot = kind == SavedKind::snapshot;
    const char *what = f.snapshot ? "a session (.snapx)" : f.session ? "a session file (.snapx)" : "an AUTO file (.autox)";
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(bytes);
    if (!entries) {
        err_msg(xpp::format("{} is not {}: it is not a zip", name, what).c_str());
        return std::nullopt;
    }
    const char *manifest = f.session ? xpp::snapx::manifest_member : xpp::autox::manifest_member;
    for (const xpp::zip::Entry &e : *entries) f.members[e.name] = e.bytes;
    std::optional<xpp::snapx::Manifest> man;
    if (f.members.contains(manifest))
        man = xpp::snapx::parse_manifest(f.members[manifest], f.session ? xpp::snapx::session_kind : xpp::autox::kind);
    if (!man) {
        err_msg(xpp::format("{} is not {} of this version: its {} is missing or not one", name, what, manifest).c_str());
        return std::nullopt;
    }
    f.manifest = std::move(*man);
    std::optional<std::vector<xpp::ModelFile>> files = xpp::snapx::model_members(*entries, f.manifest.model_name);
    if (!files) {
        err_msg(xpp::format("{} cannot be opened: its model is missing ({}{})", name, xpp::snapx::model_folder,
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
        err_msg(xpp::format("{} was not read from a file: it cannot be saved with the model", m.this_file).c_str());
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

bool xpp_saved_restore(xpp::Session &s, const SavedFile &f)
{
    if (f.session) return restore_session(s, f);
    if (diagram_count(s.diagram) > 1) yes_reset_auto(s); /* the diagram before goes, with AUTO's files */
    return xpp::autox::restore_members(s, f.members, "", f.path);
}

std::string xpp_session_file_name(const xpp::Model &m, std::string_view ext)
{
    std::string base = xpp_files_split_path(m.this_file).second;
    const std::size_t dot = base.rfind('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    return base + std::string(ext);
}

void xpp_session_warn(const std::string &text)
{
    xpp::log(XPP_LOG_WARN, "{}\n", text);
    bottom_msg(0, text.c_str());
}
