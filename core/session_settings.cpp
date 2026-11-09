/* A session's settings read, written and applied: see session_settings.h.
   Every group of the session statement's settings is described once, by `describe`, which is given a
   writer (Put) or a reader (Take) and says "this key, this member"; the
   same description writes and reads, so a key is never named twice. The values and
   the numerics are the file's par and init lines and options block (values_text,
   read_values, options_items, read_options). */
#include "session_settings.h"
#include "odex_write.h"
#include "snapx.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "delay_handle.h"
#include "expr.h"
#include "form_ode.h"
#include "model.h"
#include "model_options.h"
#include "numerics.h"
#include "numerics_settings.h"
#include "session.h"
#include "solver.h"
#include "volterra2.h"
#include "xpp_globals.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include "xpp_util.h"

namespace xpp {

namespace {

/* the groups of a settings file, as it names them */
namespace group {
constexpr std::string_view session = "session", numerics = "numerics", run = "run", ending = "ending", delays = "delays", boundary = "boundary",
                           graphics = "graphics", transpose = "transpose", coupling = "coupling",
                           array_plot = "array_plot", torus = "torus", sweep = "sweep",
                           equilibrium_range = "equilibrium_range", bvp_range = "bvp_range";
} // namespace group


/* the names of the model's variables: its differential equations first,
   then the Markov chains' (count of them from the first) */
std::vector<std::string> variable_names(const Model &m, int count)
{
    return std::vector<std::string>(m.uvar_names.begin(), m.uvar_names.begin() + count);
}

/* the room a boundary condition of s has, its NUL left out */
std::size_t bc_room(const Session &s, int i)
{
    const auto &bc = s.bcs[static_cast<std::size_t>(i)].string;
    return bc.empty() ? 0 : bc.size() - 1;
}

/* ---- the writer and the reader of a description ---- */

/* the values of the model's variables (equations) in one group: the ones the file names
   (complete: every one, and none the model does not have) */
template <class V, class Read>
void read_named(const Model &model, odex::Block &g, std::span<const std::string> names, std::vector<V> &values, Read read)
{
    std::size_t matched = 0;
    for (const odex::Member *m : g.members()) {
        const Model::Name *entry = model.name(m->key);
        const int index = entry ? entry->column : -1;
        if (index < 0 || static_cast<std::size_t>(index) >= names.size() || names[static_cast<std::size_t>(index)] != m->key)
            g.fail(*m, xpp::format("`{}` is not one of the model's variables", m->key));
        read(static_cast<std::size_t>(index), values[static_cast<std::size_t>(index)], *m);
        matched++;
    }
    if (g.complete() && matched != names.size())
        for (const std::string &n : names)
            if (!g.has(n)) g.fail(xpp::format("{} has no `{}`", g.name(), n));
}

/* writes what a description names */
class Put {
public:
    /* s: the session whose model names the keys; none for a window's settings alone */
    Put(odex::Out &out, const Session *s) : out_(out), s_(s) {}
    void operator()(std::string_view key, double v) { out_.real(key, v); }
    void operator()(std::string_view key, int v) { out_.whole(key, v); }
    void dimension(std::string_view key, int v) { (*this)(key, v); }
    void flag(std::string_view key, int v) { out_.flag(key, v != 0); }
    /* a name of the model (bare, "" for none: quoted) / a formula or any other text (quoted) */
    void word(std::string_view key, const std::string &v) { out_.name(key, v); }
    void text(std::string_view key, const std::string &v) { out_.text(key, v); }
    void numbers(std::string_view key, std::span<const double> v) { out_.reals(key, v); }
    template <class F> void group(std::string_view key, F fn)
    {
        out_.begin(key);
        fn(*this);
        out_.end();
    }
    void named(std::string_view key, std::span<const std::string> names, const std::vector<double> &values)
    {
        group(key, [&](Put &p) {
            for (std::size_t i = 0; i < names.size(); i++) p(names[i], values[i]);
        });
    }
    void named(std::string_view key, std::span<const std::string> names, const std::vector<std::string> &values,
               std::span<const std::size_t> = {})
    {
        group(key, [&](Put &p) {
            for (std::size_t i = 0; i < names.size(); i++) p.text(names[i], values[i]);
        });
    }
    /* the names whose flag is set */
    void flagged(std::string_view key, std::span<const std::string> names, const std::vector<int> &flags)
    {
        std::vector<std::string> on;
        for (std::size_t i = 0; i < names.size(); i++)
            if (flags[i]) on.push_back(names[i]);
        out_.words(key, on);
    }
    void curves(const GRAPH &g) { write_curves(out_, *s_, g); }

private:
    odex::Out &out_;
    const Session *s_;
};

/* reads what a description names */
class Take {
public:
    /* s: the session whose model names the keys; none for a window's settings alone */
    Take(odex::Block &block, Session *s) : block_(block), s_(s) {}
    void operator()(std::string_view key, double &v) { block_.number(key, v); }
    void operator()(std::string_view key, int &v) { block_.whole(key, v); }
    void dimension(std::string_view key, int &v)
    {
        if (!block_.whole(key, v)) return;
        if (const char *why = rule_problem(OptionRule::whole_positive, v, settings_dimension_limit))
            block_.fail(*block_.member(key), xpp::format("`{}={}`: {} (maximum {})", key, v, why, settings_dimension_limit));
    }
    void flag(std::string_view key, int &v)
    {
        bool on = v != 0;
        block_.flag(key, on);
        v = on ? 1 : 0;
    }
    void word(std::string_view key, std::string &v)
    {
        if (const odex::Member *m = block_.member(key)) v = block_.name_of(*m);
    }
    void text(std::string_view key, std::string &v) { block_.text(key, v); }
    void numbers(std::string_view key, std::span<double> v) { block_.reals(key, v); }
    template <class F> void group(std::string_view key, F fn)
    {
        std::optional<odex::Block> g = block_.group(key);
        if (!g) return;
        Take nested(*g, s_);
        fn(nested);
        g->done();
    }
    void named(std::string_view key, std::span<const std::string> names, std::vector<double> &values)
    {
        std::optional<odex::Block> g = block_.group(key);
        if (!g) return;
        read_named(s_->model(), *g, names, values, [&](std::size_t, double &v, const odex::Member &m) { v = g->number_of(m); });
    }
    void named(std::string_view key, std::span<const std::string> names, std::vector<std::string> &values,
               std::span<const std::size_t> longest = {})
    {
        std::optional<odex::Block> g = block_.group(key);
        if (!g) return;
        read_named(s_->model(), *g, names, values, [&](std::size_t i, std::string &v, const odex::Member &m) {
            v = g->text_of(m);
            if (!longest.empty() && v.size() > longest[i])
                g->fail(m, xpp::format("`{}` is {} characters: at most {}", m.key, v.size(), longest[i]));
        });
    }
    void flagged(std::string_view key, std::span<const std::string> names, std::vector<int> &flags)
    {
        std::vector<std::string> on;
        const odex::Member *m = block_.member(key);
        if (!block_.words(key, on, names.size())) return;
        std::ranges::fill(flags, 0);
        for (const std::string &n : on) {
            const Model::Name *entry = s_->model().name(n);
            const int index = entry ? entry->column : -1;
            if (index < 0 || static_cast<std::size_t>(index) >= names.size() || names[static_cast<std::size_t>(index)] != n)
                block_.fail(*m, xpp::format("`{}` is not one of the model's variables", n));
            flags[static_cast<std::size_t>(index)] = 1;
        }
    }
    void curves(GRAPH &g) {
        std::size_t expanded_bytes = 0;
        read_curves(block_, *s_, g, {}, expanded_bytes);
    }

private:
    odex::Block &block_;
    Session *s_;
};

/* ---- a window's other settings ---- */

/* the window's settings as a settings file holds them, each given to io
   with its key (its curves are write_curves' and read_curves') */
template <class IO, class G> void graph_fields(IO &io, G &g)
{
    io.numbers("rm", std::span(&g.rm[0][0], 9));
    io("z_plane", g.ZPlane);
    io("z_view", g.ZView);
    io("perspective", g.PerspFlag);
    io("three_d", g.ThreeDFlag);
    io("time", g.TimeFlag);
    io("color_flag", g.ColorFlag);
    io("type", g.grtype);
    io("color_scale", g.color_scale);
    io("min_scale", g.min_scale);
    io("xmax", g.xmax);
    io("xmin", g.xmin);
    io("ymax", g.ymax);
    io("ymin", g.ymin);
    io("zmax", g.zmax);
    io("zmin", g.zmin);
    io("xbar", g.xbar);
    io("dx", g.dx);
    io("ybar", g.ybar);
    io("dy", g.dy);
    io("zbar", g.zbar);
    io("dz", g.dz);
    io("theta", g.Theta);
    io("phi", g.Phi);
    io("xshift", g.xshft);
    io("yshift", g.yshft);
    io("zshift", g.zshft);
    io("xlo", g.xlo);
    io("ylo", g.ylo);
    io("old_xlo", g.oldxlo);
    io("old_ylo", g.oldylo);
    io("xhi", g.xhi);
    io("yhi", g.yhi);
    io("old_xhi", g.oldxhi);
    io("old_yhi", g.oldyhi);
}

/* gathers the settings' values into a list / puts them back from it */
class Gather {
public:
    template <class T> void operator()(std::string_view, T &v) { values.push_back(static_cast<double>(v)); }
    void numbers(std::string_view, std::span<const double> v) { values.insert(values.end(), v.begin(), v.end()); }
    std::vector<double> values;
};

class Scatter {
public:
    explicit Scatter(const std::vector<double> &values) : values_(values) {}
    template <class T> void operator()(std::string_view, T &v) { v = static_cast<T>(values_[next_++]); }
    void numbers(std::string_view, std::span<double> v)
    {
        for (double &x : v) x = values_[next_++];
    }

private:
    const std::vector<double> &values_;
    std::size_t next_ = 0;
};

/* the column name names: the time, a variable, or one of the columns
   added (a session's windows add them before the browser has them);
   nothing when the model has none of that name */
std::optional<int> column_named(const Session &s, std::span<const AddedColumn> added, std::string_view name)
{
    int col;
    find_variable(s, name, &col);
    if (col >= 0) return col;
    for (std::size_t k = 0; k < added.size(); k++)
        if (equal_ignoring_case(added[k].name, name)) return s.model().neq + 1 + static_cast<int>(k);
    return std::nullopt;
}

/* ---- the whole description ---- */

/* the names a session's groups are keyed by (the model's), and the room
   each boundary condition has */
struct Names {
    std::vector<std::string> variables, equations, all, parameters;
    std::vector<std::size_t> bc_room;
};

Names names_of(const Session &s)
{
    const Model &m = s.model();
    Names n{variable_names(m, m.node + m.nmarkov), variable_names(m, m.node), variable_names(m, m.neq),
            std::vector<std::string>(m.upar_names.begin(), m.upar_names.begin() + m.nupar), {}};
    for (int i = 0; i < m.node; i++) n.bc_room.push_back(bc_room(s, i));
    return n;
}

/* what the session statement holds of the settings, given to io: f is the Settings written from
   or read into. The parameters, the initial values and the numerics are the file's par and init
   lines and options block. */
template <class IO, class S> void describe(IO &io, S &f, const Names &n)
{
    io.group(group::run, [&](IO &r) {
        r("current_time", f.current_time);
        r("last_time", f.last_time);
        r("my_start", f.my_start);
        r.flag("inflag", f.inflag);
        r.flag("delay_flag", f.delay_flag);
        r.named(group::ending, n.variables, f.current);
    });
    io.named(group::delays, n.equations, f.delays);
    io.named(group::boundary, n.equations, f.bcs, n.bc_room);
    if (f.graphics)
        io.group(group::graphics, [&](IO &g) {
            g.curves(f.graph);
            graph_fields(g, f.graph);
        });
    io.group(group::transpose, [&](IO &t) {
        t.text("first_column", f.transpose.firstcol);
        t.dimension("columns", f.transpose.ncol);
        t.dimension("rows", f.transpose.nrow);
        t("row_skip", f.transpose.rowskip);
        t("col_skip", f.transpose.colskip);
        t("row0", f.transpose.row0);
    });
    io.named(group::coupling, n.equations, f.coupling);
    io.group(group::array_plot, [&](IO &a) {
        a.text("first_column", f.aplot.name);
        a.dimension("columns", f.aplot.nacross);
        a("first_row", f.aplot.nstart);
        a.dimension("rows", f.aplot.ndown);
        a("row_skip", f.aplot.nskip);
        a("zmin", f.aplot.zmin);
        a("zmax", f.aplot.zmax);
    });
    io.group(group::torus, [&](IO &t) {
        t.flag("on", f.torus);
        t("period", f.torus_period);
        t.flagged("fold", n.all, f.itor);
    });
    io.group(group::sweep, [&](IO &r) {
        r.word("rangeover2", f.range_item2);
        r("rangelow2", f.range_low2);
        r("rangehigh2", f.range_high2);
        r("rangecycle", f.range_cycle);
    });
    io.group(group::equilibrium_range, [&](IO &e) {
        e.word("item", f.eq_range.item);
        e("stab_col", f.eq_range.col);
        e("shoot", f.eq_range.shoot);
        e("steps", f.eq_range.steps);
        e("low", f.eq_range.plow);
        e("high", f.eq_range.phigh);
    });
    io.group(group::bvp_range, [&](IO &b) {
        b.word("item", f.shoot_range.item);
        b("side", f.shoot_range.side);
        b("cycle", f.shoot_range.cycle);
        b("steps", f.shoot_range.steps);
        b("low", f.shoot_range.plow);
        b("high", f.shoot_range.phigh);
    });
}

} // namespace

void Settings::set_setting(const OptionRow *row, std::string text)
{
    std::vector<Setting> &rows = row->group == option_group::sweep ? range : numerics;
    const auto at = std::ranges::find_if(rows, [row](const Setting &r) { return r.first == row; });
    if (at != rows.end()) at->second = std::move(text);
    else rows.emplace_back(row, std::move(text));
}

double parameter_value(const Session &s, int i)
{
    double z = 0;
    get_val(s, s.model().upar_names[static_cast<std::size_t>(i)], &z);
    return z;
}

void copy_graph_settings(const GRAPH &from, GRAPH &to)
{
    Gather values;
    graph_fields(values, from);
    Scatter put(values.values);
    graph_fields(put, to);
}

void copy_curves(const GRAPH &from, GRAPH &to)
{
    to.resize_curves(from.nvars);
    to.xv = from.xv;
    to.yv = from.yv;
    to.zv = from.zv;
    to.style = from.style;
    to.color = from.color;
}

void write_window(odex::Out &out, const Session &s, const GRAPH &g)
{
    write_curves(out, s, g);
    Put io(out, &s);
    graph_fields(io, g);
}

void read_window(odex::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes)
{
    read_curves(window, s, g, added, expanded_bytes);
    Take io(window, nullptr);
    graph_fields(io, g);
}

void write_zoom(odex::Out &out, const Zoom &z)
{
    out.begin("zoom");
    for (const auto &[key, r] : {std::pair<std::string_view, const AxisRange &>{"x", z.x}, {"y", z.y}}) {
        out.begin(key);
        out.flag("set", r.set);
        out.real("low", r.lo);
        out.real("high", r.hi);
        out.end();
    }
    out.end();
}

Zoom read_zoom(odex::Block &owner)
{
    Zoom z;
    std::optional<odex::Block> g = owner.group("zoom");
    if (!g) return z;
    for (const auto &[key, r] : {std::pair<std::string_view, AxisRange &>{"x", z.x}, {"y", z.y}}) {
        std::optional<odex::Block> a = g->group(key);
        if (!a) continue;
        a->flag("set", r.set);
        a->real("low", r.lo);
        a->real("high", r.hi);
        if (r.set && !(r.lo < r.hi)) a->fail(*a->member("low"), xpp::format("a zoom's low {} is not below its high {}", r.lo, r.hi));
        a->done();
    }
    g->done();
    return z;
}

void write_curves(odex::Out &out, const Session &s, const GRAPH &g)
{
    out.begin_list("curves");
    for (int j = 0; j < g.nvars; j++) {
        const auto k = static_cast<std::size_t>(j);
        out.begin_item();
        out.name("x", ind_to_sym(s, g.xv[k]));
        out.name("y", ind_to_sym(s, g.yv[k]));
        out.name("z", ind_to_sym(s, g.zv[k]));
        out.color("color", g.color[k]);
        out.line_style(g.style[k]);
        out.end_item();
    }
    out.end_list();
}

void read_curves(odex::Block &window, const Session &s, GRAPH &g, std::span<const AddedColumn> added, std::size_t &expanded_bytes)
{
    std::vector<odex::Block> curves;
    /* Five integer columns in the staged/runtime graphs and two display
       identities; six float scratch columns in both graphs, plus phase flow. */
    constexpr std::size_t curve_bytes = sizeof(odex::Block) + 4 * 5 * sizeof(int)
        + 2 * 2 * plot_axes * sizeof(float) + sizeof(PhaseShown::FlowCurve);
    if (!snapx::memory_groups(window, "curves", curves, curve_bytes, expanded_bytes)) return;
    if (curves.empty()) window.fail(*window.member("curves"), "a window has at least one curve");
    g.resize_curves(static_cast<int>(curves.size()));
    for (std::size_t j = 0; j < curves.size(); j++) {
        odex::Block &c = curves[j];
        const auto column = [&](std::string_view key) {
            const odex::Member *m = c.member(key);
            if (!m) return 0;
            const std::string name = c.name_of(*m);
            const std::optional<int> col = column_named(s, added, name);
            if (!col) c.fail(*m, xpp::format("the model has no variable \"{}\"", name));
            return *col;
        };
        g.xv[j] = column("x");
        g.yv[j] = column("y");
        g.zv[j] = column("z");
        c.color("color", g.color[j]);
        c.line_style(g.style[j]);
        c.done();
    }
}

Settings capture_settings(const Session &s, SettingsUse use)
{
    const Model &m = s.model();
    Settings f;
    f.graphics = use == SettingsUse::file;
    for (const OptionRow *row : saved_settings(group::numerics))
        if (setting_applies(s, *row, s.numerics.method)) f.numerics.emplace_back(row, setting_text(s, *row));
    for (const OptionRow *row : saved_settings(option_group::sweep)) f.range.emplace_back(row, setting_text(s, *row));
    f.current_time = s.data_store.current_time;
    f.last_time = s.integrator.last_time;
    f.my_start = s.integrator.my_start;
    f.inflag = s.numerics.inflag;
    f.delay_flag = s.delay.flag;
    for (int i = 0; i < m.node; i++) {
        f.delays.push_back(s.delay_string[static_cast<std::size_t>(i)]);
        f.bcs.emplace_back(s.bcs[static_cast<std::size_t>(i)].string.data());
        f.coupling.push_back(s.adjoint.coup_string[static_cast<std::size_t>(i)]);
    }
    for (int i = 0; i < m.node + m.nmarkov; i++) {
        f.last_ic.push_back(s.last_ic[static_cast<std::size_t>(i)]);
        f.current.push_back(s.data_store.current[static_cast<std::size_t>(i)]);
    }
    for (int i = 0; i < m.nupar; i++) f.params.push_back(parameter_value(s, i));
    f.graph = s.plot_windows.current ? *s.plot_windows.current : s.plot_windows.graph[0];
    f.transpose = s.adjoint.transpose;
    f.aplot = s.array_plot.plot;
    f.torus = s.numerics.torus;
    f.torus_period = s.numerics.torus_period;
    for (int i = 0; i < m.neq; i++) f.itor.push_back(s.itor[static_cast<std::size_t>(i)]);
    f.range_item2 = s.integrator.range.item2;
    f.range_low2 = s.integrator.range.plow2;
    f.range_high2 = s.integrator.range.phigh2;
    f.range_cycle = s.integrator.range.cycle;
    f.eq_range = s.integrator.eq_range;
    f.shoot_range = s.shoot_range;
    return f;
}

namespace {

/* the par and init lines of f: one statement for each value, the name as the model has it */
Result<std::string> values_text(const Settings &f, const Names &n)
{
    std::string out;
    const auto put = [&](std::string_view keyword, std::span<const std::string> names, const std::vector<double> &values) -> Result<> {
        for (std::size_t i = 0; i < names.size(); i++) {
            if (!odex::is_name(names[i]))
                return fail("session", xpp::format("`{}` cannot be written as a name of a {} line", names[i], keyword), Place{snapx::state_member});
            if (std::isnan(values[i]))
                return fail("session", xpp::format("the value of `{}` is not a number: it cannot be written", names[i]), Place{snapx::state_member});
            out += xpp::format("{} {} = {}\n", keyword, names[i], odex::print_number(values[i]));
        }
        return {};
    };
    if (Result<> done = put("par", n.parameters, f.params); !done) return std::unexpected(done.error());
    if (Result<> done = put("init", n.variables, f.last_ic); !done) return std::unexpected(done.error());
    return out;
}

/* the options block of f: each row under its group and key, a path or a text in quotes */
std::string options_text(const Settings &f)
{
    std::vector<odex::GroupItem> items;
    for (const auto *rows : {&f.numerics, &f.range})
        for (const auto &[row, text] : *rows)
            items.push_back({row->group, odex_key(*row, 0) + ": " + (option_written_quoted(*row) ? odex::quote_string(text) : text)});
    return odex::options_block(items);
}

/* the file's par or init lines (kind) read into values: each name of the model's, once, a number each */
void read_values(const odex::StateFile &file, const Model &m, odex::Statement::Kind kind, std::span<const std::string> names, std::vector<double> &values)
{
    const bool parameters = kind == odex::Statement::Kind::Par;
    std::vector<bool> seen(names.size(), false);
    for (const odex::Statement &st : file.parsed.statements) {
        if (st.kind != kind) continue;
        for (const odex::Binding &b : st.bindings) {
            const Model::Name *entry = m.name(b.name);
            const int index = entry ? (parameters ? entry->parameter : entry->column) : -1;
            if (index < 0 || static_cast<std::size_t>(index) >= names.size() || names[static_cast<std::size_t>(index)] != b.name)
                file.doc.fail(b.pos.line, b.pos.col, xpp::format("`{}` is not one of the model's {}", b.name, parameters ? "parameters" : "variables"));
            std::string number;
            double v = 0;
            switch (odex::whole_number(st, b, number)) {
            case odex::WholeNumber::element:
                file.doc.fail(b.pos.line, b.pos.col, xpp::format("`{}` is an array's element: a session holds each name whole", b.name));
            case odex::WholeNumber::formula:
                file.doc.fail(b.value.pos.line, b.value.pos.col, xpp::format("`{}` is not given a number", b.name));
            case odex::WholeNumber::number: break;
            }
            if (!parse_number(number, v)) file.doc.fail(b.value.pos.line, b.value.pos.col, xpp::format("`{}` is not given a number", b.name));
            if (seen[static_cast<std::size_t>(index)])
                file.doc.fail(b.pos.line, b.pos.col, xpp::format("`{}` is given twice", b.name));
            seen[static_cast<std::size_t>(index)] = true;
            values[static_cast<std::size_t>(index)] = v;
        }
    }
    for (std::size_t i = 0; i < names.size(); i++)
        if (!seen[i]) file.doc.fail(file.group.line, file.group.col, xpp::format("the file has no `{} {}` line", parameters ? "par" : "init", names[i]));
}

/* the options block read into f's numerics and sweep rows, each checked as the Numerics menu and the @ lines check
   them (every one the model has, once; the file's method decides whether the Volterra points belong) */
void read_options(Session &s, const odex::StateFile &file, Settings &f)
{
    std::map<const OptionRow *, const odex::Option *> given;
    for (const odex::Statement &st : file.parsed.statements) {
        if (st.kind != odex::Statement::Kind::Options) continue;
        for (const odex::Option &o : st.more().options) {
            int index = 0;
            const OptionRow *row = find_odex_option(o.group, o.name, index);
            if (!row || !row->saved)
                file.doc.fail(o.pos.line, o.pos.col, xpp::format("`{}` is not a setting a session holds", o.name));
            given[row] = &o;
        }
    }
    for (const std::string_view which : {group::numerics, option_group::sweep}) {
        int method = s.numerics.method;
        for (const OptionRow *row : saved_settings(which)) {
            const std::string key = odex_key(*row, 0);
            const auto at = given.find(row);
            const odex::Option *o = at == given.end() ? nullptr : at->second;
            if (!setting_applies(s, *row, method)) {
                if (o) file.doc.fail(o->pos.line, o->pos.col, xpp::format("`{}` is not a setting of this model", key));
                continue;
            }
            if (!o) file.doc.fail(file.group.line, file.group.col, xpp::format("the options have no `{}`", key));
            if (const std::optional<std::string> why = setting_problem(s, *row, o->value))
                file.doc.fail(o->value_pos.line, o->value_pos.col, xpp::format("`{}`: {} (found `{}`)", key, *why, o->value));
            if (row->rule == OptionRule::method) method = method_by_key(s.model(), o->value, command_place()).value();
            f.set_setting(row, o->value);
        }
    }
    /* the settings the file holds keep the orders the options keep */
    const auto held = [&](const OptionRow &r) -> std::optional<double> {
        for (const auto *rows : {&f.numerics, &f.range})
            for (const auto &[row, text] : *rows) {
                double v = 0;
                if (row == &r && parse_number(text, v)) return v;
            }
        return r.real ? r.real(s) : r.whole(s);
    };
    if (const auto bad = option_pairs_problem(held)) {
        const auto at = [&](const OptionRow &r) { const auto g = given.find(&r); return g == given.end() ? nullptr : g->second; };
        const odex::Option *o = at(once_option(bad->hi));
        if (!o) o = at(once_option(bad->lo));
        if (o) file.doc.fail(o->pos.line, o->pos.col, bad->text);
    }
}

} // namespace

Result<std::string> settings_statements(const Session &s, const Settings &f)
{
    Result<std::string> values = values_text(f, names_of(s));
    if (!values) return std::unexpected(values.error());
    return odex::version_statement() + "\n" + *values + options_text(f);
}

Result<std::string> settings_text(const Session &s, const Settings &f)
{
    Result<std::string> head = settings_statements(s, f);
    if (!head) return std::unexpected(head.error());
    odex::Out out;
    out.begin_statement(group::session);
    write_settings(out, s, f);
    out.end_statement();
    Result<std::string> body = out.result("session");
    if (!body) return std::unexpected(body.error());
    return *head + *body;
}

void write_settings(odex::Out &out, const Session &s, const Settings &f)
{
    Put io(out, &s);
    describe(io, f, names_of(s));
}

Settings read_settings(Session &s, const odex::StateFile &file, odex::Block &session)
{
    Settings f = capture_settings(s, SettingsUse::session);
    const Names n = names_of(s);
    read_values(file, s.model(), odex::Statement::Kind::Par, n.parameters, f.params);
    read_values(file, s.model(), odex::Statement::Kind::Init, n.variables, f.last_ic);
    read_options(s, file, f);
    Take io(session, &s);
    describe(io, f, n);
    return f;
}

void apply_storage_precision(Session &s, const Settings &f)
{
    for (const auto &[row, text] : f.numerics)
        if (row->applied == storage_precision_changed) setting_write(s, *row, text);
    storage_precision_changed(s);
}

void apply_settings(Session &s, const Settings &f, bool redraw)
{
    const Model &m = s.model();
    NumericsSettings &n = s.numerics;
    for (const auto &[row, text] : f.numerics) {
        setting_write(s, *row, text);
        if (row->rule == OptionRule::method) do_meth(s);
    }
    storage_precision_changed(s); /* the data held converted when the settings changed what it is kept as */
    s.delay.flag = f.delay_flag;
    s.data_store.current_time = f.current_time;
    s.integrator.last_time = f.last_time;
    s.integrator.my_start = f.my_start;
    n.inflag = f.inflag;
    if (solver_info(n.method).traits.integral_history) {
        allocate_volterra(s, n.max_points, 1);
        s.integrator.my_start = 1;
    }
    chk_delay(s);
    for (int i = 0; i < m.node; i++) {
        const auto k = static_cast<std::size_t>(i);
        s.delay_string[k] = f.delays[k];
        set_bc_formula(s, i, f.bcs[k]);
        s.adjoint.coup_string[k] = f.coupling[k];
    }
    for (int i = 0; i < m.node + m.nmarkov; i++) {
        const auto k = static_cast<std::size_t>(i);
        s.last_ic[k] = f.last_ic[k];
        s.data_store.current[k] = f.current[k];
    }
    for (int i = 0; i < m.nupar; i++) set_val(s, m.upar_names[static_cast<std::size_t>(i)], f.params[static_cast<std::size_t>(i)]);
    if (f.graphics) {
        copy_graph_settings(f.graph, *s.plot_windows.current);
        copy_curves(f.graph, *s.plot_windows.current);
    }
    s.adjoint.transpose = f.transpose;
    s.array_plot.plot = f.aplot;
    n.torus = f.torus;
    n.torus_period = f.torus_period;
    for (std::size_t i = 0; i < f.itor.size(); i++) s.itor[i] = f.itor[i];
    for (const auto &[row, text] : f.range) setting_write(s, *row, text);
    RangeVars &r = s.integrator.range;
    r.item2 = f.range_item2;
    r.plow2 = f.range_low2;
    r.phigh2 = f.range_high2;
    r.cycle = f.range_cycle;
    r.steps2 = r.steps;
    s.integrator.eq_range = f.eq_range;
    s.shoot_range = f.shoot_range;
    if (redraw && program.interactive) {
        ui.redraw_bcs();
        redraw_ics();
        ui.redraw_delays();
        redraw_params();
        ui.redraw_graph(s);
    }
}

} // namespace xpp
