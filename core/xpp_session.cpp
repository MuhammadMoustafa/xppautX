/* Save session and Open session: see xpp_session.h. The session file's
   pure part (the members' names, the manifest, the fingerprint) is
   snapx.cpp; this file writes and reads the members, each through the
   module that owns its format: the set file (lunch-new.cpp), AUTO's file
   (autox_io.cpp), NPZ (data_formats.cpp), the zip (xpp_zip.cpp). The
   writers and readers of a set file's lines take a FILE *, so those
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

std::string model_path()
{
    const xpp::Model &m = xpp::model();
    return xpp_files_absolute(m.this_file, m.load_dir);
}

/* what Save session offers */
std::string default_name() { return xpp_session_file_name(xpp::snapx::extension); }

/* name, or when it is NULL/empty the one the user picks with title
   (wild the files listed); false on a cancel */
bool name_or_ask(const char *title, const char *wild, const char *name, std::string &out)
{
    if (name != nullptr && name[0] != 0) {
        out = name;
        return true;
    }
    std::string file = default_name();
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

bool write_windows(FILE *fp)
{
    xpp::Session &s = xpp::session();
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
            std::string x = ind_to_sym(g.xv[j]), y = ind_to_sym(g.yv[j]), z = ind_to_sym(g.zv[j]);
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
    io_zoom(writing, fp, v.zoom);
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
void curves_by_name(GRAPH &g, const std::vector<std::string> &names)
{
    const int nvars = std::min(g.nvars, static_cast<int>(names.size() / 3));
    int n = 0;
    for (int j = 0; j < nvars; j++) {
        int x, y, z;
        find_variable(names[3 * j], &x);
        find_variable(names[3 * j + 1], &y);
        find_variable(names[3 * j + 2], &z);
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
        g.yv[0] = xpp::model().neq > 0 ? 1 : 0;
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
bool read_windows(FILE *fp, std::map<int, int> &slot, WindowsRead &rest)
{
    xpp::Session &s = xpp::session();
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
            make_active(0, 1);
            create_a_pop();
            i = s.plot_windows.active;
        }
        GRAPH scratch = s.plot_windows.graph[0];
        xpp::PlotDisplay scratch_display;
        const bool made = saved == 0 || i != 0;
        GRAPH &g = made ? s.plot_windows.graph[i] : scratch;
        xpp::PlotDisplay &d = made ? s.plot_display[i] : scratch_display;
        read_graph(fp, g);
        g.nvars = nvars;
        curves_by_name(g, names);
        g.xlabel = std::move(xlabel);
        g.ylabel = std::move(ylabel);
        g.zlabel = std::move(zlabel);
        io_zoom(reading, fp, d.zoom);
        io_bool(reading, fp, d.show_runs, "");
        d.axes_seen = false; /* the zoom is for the axes just read */
        if (made) slot[saved] = i;
    }
    make_active(slot.contains(active) ? slot[active] : 0, 1);
    io_int(&rest.auto_view.earlier, fp, reading, "");
    io_bool(reading, fp, rest.auto_view.show_earlier, "");
    io_zoom(reading, fp, rest.auto_view.zoom);
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

bool write_marks(FILE *fp)
{
    xpp::Session &s = xpp::session();
    int n = 0;
    for (const LABEL &l : s.labels) n += l.use != 0;
    io_int(&n, fp, writing, "labels");
    for (LABEL &l : s.labels) {
        if (!l.use) continue;
        int win = graph_of(l.w);
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
        int win = graph_of(g.w);
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
        int win = graph_of(c.w), type = c.type;
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
std::optional<std::string> frozen_npz()
{
    const xpp::Session &s = xpp::session();
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

XppWinId window_of(const std::map<int, int> &slot, int saved)
{
    const auto it = slot.find(saved);
    return xpp::session().plot_windows.graph[it == slot.end() ? 0 : it->second].w;
}

bool read_marks(FILE *fp, const std::map<int, int> &slot, const xpp::DataTable &frozen)
{
    xpp::Session &s = xpp::session();
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
            l.w = window_of(slot, win);
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
                     size, 1, window_of(slot, win), type, color};
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
            !restore_frozen_curve(i, window_of(slot, win), type, color, std::move(key), std::move(name), std::move(x),
                                  std::move(y), std::move(z)))
            xpp::log(XPP_LOG_WARN, "Open session: frozen curve {} left out\n", i + 1);
    }
    return true;
}

/* ---- the whole file ---- */

/* the members of the zip at path by name; nothing (an error message) when
   it cannot be read or is not a zip */
std::optional<std::map<std::string, std::string>> members(const std::string &path)
{
    std::string bytes;
    if (!xpp::read_bytes(path.c_str(), bytes)) {
        err_msg(xpp::format("Cannot open {}", path).c_str());
        return std::nullopt;
    }
    std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(bytes);
    if (!entries) {
        err_msg(xpp::format("{} is not a session file (not a zip)", path).c_str());
        return std::nullopt;
    }
    std::map<std::string, std::string> m;
    for (xpp::zip::Entry &e : *entries) m[e.name] = std::move(e.bytes);
    return m;
}

std::optional<xpp::snapx::Manifest> manifest_of(const std::map<std::string, std::string> &m, const std::string &path)
{
    const auto it = m.find(xpp::snapx::manifest_member);
    std::optional<xpp::snapx::Manifest> man;
    if (it != m.end()) man = xpp::snapx::parse_manifest(it->second);
    if (!man) err_msg(xpp::format("{} is not a session file of this version", path).c_str());
    return man;
}

/* the bytes of member name, or nullptr when the session file has none */
const std::string *member(const std::map<std::string, std::string> &m, const char *name)
{
    if (!m.contains(name)) return nullptr;
    return &m.at(name);
}

/* the older session: <base>.set and, when there is one, <base>.auto, into
   this model */
int load_set_and_auto(const std::string &base)
{
    std::string set_name = base + ".set";
    {
        xpp::UniqueFile fp = xpp::open_read(set_name.c_str());
        if (!fp) {
            err_msg("Cannot open file");
            return 0;
        }
        if (!read_lunch(fp.get())) return 0;
    }
    SavedSession &saved = xpp::session().saved_session;
    saved = SavedSession{};
    saved.set = set_name;

    std::string auto_name = base + ".auto";
    xpp::UniqueFile fp = xpp::open_read(auto_name.c_str());
    if (fp) {
        if (diagram_count() > 1) yes_reset_auto(); /* as load_auto does, without its confirmation ask */
        if (!xpp::session().auto_state.bifur.exist) do_auto_win(); /* the diagram needs a window to draw into */
        if (import_auto_file(fp.get()) != 1) {
            err_msg("Bad AUTO file");
            return 0;
        }
        fp.reset();
        if (xpp::session().auto_state.bifur.exist) redraw_diagram(); /* load_auto leaves this to the caller */
        saved.auto_file = auto_name;
    }
    return 1;
}

} // namespace

int xpp_session_save(const char *name_arg, int data)
{
    std::string name;
    if (!name_or_ask("Save session", "*.snapx", name_arg, name)) return 0;
    const std::string file = xpp::snapx::session_file_name(name);
    const xpp::Model &m = xpp::model();
    xpp::Session &s = xpp::session();

    xpp::snapx::Manifest man = xpp_session_manifest();
    man.data = s.data_store.rows > 0 && data != 0;
    const std::uint64_t data_bytes = static_cast<std::uint64_t>(s.data_store.rows) * static_cast<std::uint64_t>(m.neq + 1) * 8;
    if (man.data && data < 0 && data_bytes > large_data) {
        const std::string q = xpp::format("The data table is {} MB. Save the session without it? (Go computes it again.)",
                                          data_bytes / (1024 * 1024));
        switch (TwoChoice("Leave it out", "Save it", q.c_str(), "ls")) {
        case 'l':
            man.data = false;
            break;
        case 's':
            break;
        default:
            return 0;
        }
    }

    xpp::TempDir tmp;
    if (tmp.path().empty()) {
        err_msg("Save session: no scratch folder");
        return 0;
    }
    std::vector<xpp::zip::Entry> entries;
    entries.push_back({xpp::snapx::manifest_member, xpp::snapx::manifest_text(man)});
    redraw_params(); /* as File/Write set does, before write_lunch */
    std::optional<std::string> set = written(tmp, xpp::snapx::set_member, [](FILE *fp) {
        write_lunch(fp);
        return true;
    });
    if (!set) {
        err_msg("Save session: cannot write the set file");
        return 0;
    }
    entries.push_back({xpp::snapx::set_member, std::move(*set)});
    if (diagram_count() > 1) { /* a diagram exists */
        std::optional<std::string> a = xpp::autox::file_bytes();
        if (!a) {
            err_msg("Save session: cannot write AUTO's diagram");
            return 0;
        }
        entries.push_back({xpp::snapx::autox_member, std::move(*a)});
    }
    std::optional<std::string> windows = written(tmp, xpp::snapx::windows_member, write_windows);
    std::optional<std::string> marks = written(tmp, xpp::snapx::marks_member, write_marks);
    if (!windows || !marks) {
        err_msg("Save session: cannot write the windows");
        return 0;
    }
    entries.push_back({xpp::snapx::windows_member, std::move(*windows)});
    entries.push_back({xpp::snapx::marks_member, std::move(*marks)});
    if (std::optional<std::string> frozen = frozen_npz()) entries.push_back({xpp::snapx::frozen_member, std::move(*frozen)});
    if (man.data) entries.push_back({xpp::snapx::data_member, xpp::npz_bytes(stored_data_table())});

    xpp::Writer w = xpp::Writer::binary(file.c_str());
    if (!w || !w.write(xpp::zip::make_zip(entries)) || !w.commit()) {
        err_msg(xpp::format("Cannot write {}", file).c_str());
        return 0;
    }
    s.saved_session = SavedSession{file, "", ""};
    return 1;
}

int xpp_session_load(const char *name_arg)
{
    std::string name;
    if (!name_or_ask("Open session", "*.snapx", name_arg, name)) return 0;
    std::string file;
    if (xpp::snapx::is_session_file(name)) file = name;
    else if (xpp_files_exists(xpp::snapx::session_file_name(name).c_str())) file = xpp::snapx::session_file_name(name);
    else return load_set_and_auto(name);
    xpp_model_open(file.c_str());
    return 1;
}

std::string xpp_session_fingerprint()
{
    const xpp::Model &m = xpp::model();
    std::vector<std::string> contents;
    for (const std::string &f : m.source_files.empty() ? std::vector<std::string>{m.this_file} : m.source_files) {
        std::string bytes;
        const std::string path = xpp_files_absolute(f, m.load_dir);
        if (!xpp::read_bytes(path.c_str(), bytes))
            xpp::log(XPP_LOG_WARN, "Session: cannot read {} for the model's fingerprint\n", path);
        contents.push_back(std::move(bytes));
    }
    return xpp::snapx::fingerprint(contents);
}

std::string xpp_session_file_name(std::string_view ext)
{
    std::string base = xpp_files_split_path(xpp::model().this_file).second;
    const std::size_t dot = base.rfind('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    return base + std::string(ext);
}

xpp::snapx::Manifest xpp_session_manifest()
{
    const xpp::Model &m = xpp::model();
    xpp::snapx::Manifest man;
    man.model = model_path();
    man.model_name = xpp_files_split_path(m.this_file).second;
    man.sha256 = xpp_session_fingerprint();
    man.node = m.node;
    man.nmarkov = m.nmarkov;
    man.vars.assign(m.uvar_names.begin(), m.uvar_names.begin() + m.neq);
    man.pars.assign(m.upar_names.begin(), m.upar_names.begin() + m.nupar);
    return man;
}

bool xpp_session_same_names(const xpp::snapx::Manifest &man)
{
    const xpp::Model &m = xpp::model();
    return man.node == m.node && man.nmarkov == m.nmarkov && std::ssize(man.vars) == m.neq && std::ssize(man.pars) == m.nupar &&
           std::equal(man.vars.begin(), man.vars.end(), m.uvar_names.begin()) &&
           std::equal(man.pars.begin(), man.pars.end(), m.upar_names.begin());
}

void xpp_session_warn(const std::string &text)
{
    xpp::log(XPP_LOG_WARN, "{}\n", text);
    bottom_msg(0, text.c_str());
}

std::string xpp_session_model(const std::string &snapx)
{
    std::optional<std::map<std::string, std::string>> m = members(snapx);
    if (!m) return {};
    std::optional<xpp::snapx::Manifest> man = manifest_of(*m, snapx);
    if (!man) return {};
    const std::string beside = xpp_files_absolute(
        xpp_files_split_path(snapx).first.empty() ? man->model_name : xpp_files_split_path(snapx).first + "/" + man->model_name);
    for (const std::string &p : {beside, man->model})
        if (!p.empty() && xpp_files_exists(p.c_str()) && !xpp_files_is_dir(p.c_str())) return p;
    err_msg(xpp::format("Cannot find {}'s model {}, beside it or at {}", snapx, man->model_name, man->model).c_str());
    return {};
}

bool xpp_session_restore(const std::string &snapx)
{
    std::optional<std::map<std::string, std::string>> mem = members(snapx);
    if (!mem) return false;
    std::optional<xpp::snapx::Manifest> man = manifest_of(*mem, snapx);
    if (!man) return false;
    const xpp::Model &m = xpp::model();
    xpp::Session &s = xpp::session();
    xpp::TempDir tmp;
    if (tmp.path().empty()) {
        err_msg("Open session: no scratch folder");
        return false;
    }

    const bool same_names = xpp_session_same_names(*man);
    if (xpp_session_fingerprint() != man->sha256)
        xpp_session_warn(xpp::format("{} has changed since {} was saved: {}", m.this_file, xpp_files_split_path(snapx).second,
                         same_names ? "its names are the same, and everything is restored"
                                    : "what still fits is restored by name"));

    /* the values and numerics (and the active window's graphics) */
    if (const std::string *set = member(*mem, xpp::snapx::set_member)) {
        bool ok;
        if (same_names) ok = read_as_file(tmp, xpp::snapx::set_member, *set, [](FILE *fp) { return read_lunch(fp) != 0; });
        else {
            xpp::KeptValues kept;
            ok = read_as_file(tmp, xpp::snapx::set_member, *set, [&](FILE *fp) {
                return read_set_by_name(fp, man->vars, man->node, man->nmarkov, man->pars, kept);
            });
            if (ok) xpp::restore_values(kept);
        }
        if (!ok) xpp_session_warn("Open session: its parameters and numerics could not be read");
    }

    /* AUTO's diagram and settings, which name everything by its index
       (an .autox, or the .auto a session file before W92 held) */
    bool diagram = false;
    const std::string *autox = member(*mem, xpp::snapx::autox_member);
    const std::string *old_auto = member(*mem, xpp::snapx::old_auto_member);
    if (autox || old_auto) {
        if (!same_names) xpp_session_warn("Open session: AUTO's diagram is left out, the model's variables or parameters have changed");
        else if (autox) {
            /* the session's own manifest has said whether the model changed */
            diagram = xpp::autox::load_bytes(*autox, snapx, false);
            if (!diagram) xpp_session_warn("Open session: AUTO's diagram could not be read");
        } else {
            if (!s.auto_state.bifur.exist) do_auto_win(); /* the diagram needs a window to draw into */
            diagram = read_as_file(tmp, xpp::snapx::old_auto_member, *old_auto, [](FILE *fp) { return import_auto_file(fp) == 1; });
            if (!diagram) xpp_session_warn("Open session: AUTO's diagram could not be read");
            if (s.auto_state.bifur.exist) redraw_diagram();
        }
    }

    /* the windows, then what they show */
    std::map<int, int> slot = {{0, 0}};
    WindowsRead rest;
    if (const std::string *w = member(*mem, xpp::snapx::windows_member)) {
        if (!read_as_file(tmp, xpp::snapx::windows_member, *w, [&](FILE *fp) { return read_windows(fp, slot, rest); }))
            xpp_session_warn("Open session: its windows could not be read");
    }
    if (diagram) {
        s.auto_view.earlier = std::min(rest.auto_view.earlier, diagram_count());
        s.auto_view.show_earlier = rest.auto_view.show_earlier && s.auto_view.earlier > 0;
        s.auto_view.zoom = rest.auto_view.zoom;
        s.auto_view.axes_seen = false;
    }
    if (same_names) s.browser.added_columns = std::move(rest.added);
    if (const std::string *d = member(*mem, xpp::snapx::data_member)) {
        xpp::DataTable t;
        if (!xpp::npz_table(*d, t) || (t.rows() > 0 && put_stored_data(t) == 0)) xpp_session_warn("Open session: its data could not be read");
        else s.numerics.last_seed = t.seed;
    }
    if (const std::string *mk = member(*mem, xpp::snapx::marks_member)) {
        xpp::DataTable frozen;
        if (const std::string *f = member(*mem, xpp::snapx::frozen_member)) xpp::npz_table(*f, frozen);
        read_as_file(tmp, xpp::snapx::marks_member, *mk, [&](FILE *fp) { return read_marks(fp, slot, frozen); });
    }

    /* every window drawn as it now is, the active one last */
    const int active = s.plot_windows.active;
    for (int i = 0; i < MAXPOP; i++)
        if (s.plot_windows.graph[i].Use && i != active) {
            make_active(i, 1);
            redraw_the_graph();
        }
    make_active(active, 1);
    redraw_the_graph();
    s.saved_session = SavedSession{snapx, "", ""};
    return true;
}
