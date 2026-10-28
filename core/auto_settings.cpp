#include "auto_unknowns.h"
/* AUTO's settings as data: the "autosettings" event and the `auto` `set`
   command's checks and writes (auto_settings.h, docs/protocol.md "AUTO's
   settings as data").

   The same fields the AUTO window's forms write (auto_nox.cpp auto_num_par,
   auto_plot_par, auto_per_par), with every value checked
   first: AUTO takes some values badly (Ncol above 7 exits the program,
   autlib1.cpp cpnts), and a form typed into by hand could always send them. */
#include <cmath>
#include <climits>
#include <cstring>
#include <string>
#include <strings.h>

#include "model.h"
#include "session.h"
#include "auto_settings.h"
#include "xpp_session_auto.h"
#include "xpp_log.h"
#include "xpp_ui.h"
#include <algorithm>
#include "browse.h"
#include "diagram.h"
#include "expr.h"
#include "xpp_util.h"
#include "typed_values.h"
#include "form_ode.h"
#include "xpp_mem.h"
#include "auto_parallel.h"
#include "xpp_cpu.h"
#include "xpp_files.h"
#include "xpp_prefs.h"
#include "json_number.h"

namespace xpp {

namespace {

constexpr int PARAM_BOX = 1; /* find_user_name's parameters */

enum class Rule { any, positive, nonzero, range };

struct NumField {
    const char *key;
    AutoFieldShape shape;
    Rule rule;
    double lo, hi; /* Rule::range, inclusive; a boolean's and a choice's index range too */
};

constexpr int output_detail_levels = static_cast<int>(auto_output_detail_names.size());

/* the Numerics form's fields and what AUTO accepts for each */
constexpr NumField num_fields[AUTO_NUM_N] = {
    {"mesh_intervals", AutoFieldShape::whole, Rule::range, 1, auto_max_mesh_intervals},
    {"max_steps", AutoFieldShape::whole, Rule::range, 1, INT_MAX},
    {"output_every", AutoFieldShape::whole, Rule::range, 1, INT_MAX},
    {"collocation_points", AutoFieldShape::whole, Rule::range, auto_min_collocation_points, auto_max_collocation_points},
    {"step_size", AutoFieldShape::real, Rule::nonzero, 0, 0},
    {"min_step_size", AutoFieldShape::real, Rule::positive, 0, 0},
    {"max_step_size", AutoFieldShape::real, Rule::positive, 0, 0},
    {"parameter_min", AutoFieldShape::real, Rule::any, 0, 0},
    {"parameter_max", AutoFieldShape::real, Rule::any, 0, 0},
    {"norm_min", AutoFieldShape::real, Rule::any, 0, 0},
    {"norm_max", AutoFieldShape::real, Rule::any, 0, 0},
    {"parameter_tolerance", AutoFieldShape::real, Rule::positive, 0, 0},
    {"state_tolerance", AutoFieldShape::real, Rule::positive, 0, 0},
    {"special_point_tolerance", AutoFieldShape::real, Rule::positive, 0, 0},
    {"mesh_adapt_every", AutoFieldShape::whole, Rule::range, 0, INT_MAX},
    {"max_branch_switches", AutoFieldShape::whole, Rule::range, 0, INT_MAX},
    {"switch_one_direction", AutoFieldShape::boolean, Rule::range, 0, 1},
    {"output_detail", AutoFieldShape::choice, Rule::range, 0, output_detail_levels - 1},
    {"max_location_iterations", AutoFieldShape::whole, Rule::range, 1, INT_MAX},
    {"max_newton_iterations", AutoFieldShape::whole, Rule::range, 1, INT_MAX},
    {"full_newton_after", AutoFieldShape::whole, Rule::range, 1, INT_MAX},
    {"step_adapt_every", AutoFieldShape::whole, Rule::range, 0, INT_MAX},
    {"suppress_branch_points", AutoFieldShape::boolean, Rule::range, 0, 1},
};

/* the names a choice field takes */
std::span<const std::string_view> choices_of(int i)
{
    return i == AUTO_NUM_OUTPUT_DETAIL ? std::span<const std::string_view>(auto_output_detail_names) : std::span<const std::string_view>();
}

bool is_whole(const NumField &f) { return f.shape != AutoFieldShape::real; }

/* the values that must be in order: lo below hi (strict) or at most hi */
struct NumPair {
    int lo, hi;
    bool strict;
};
constexpr NumPair num_pairs[] = {
    {AUTO_NUM_MIN_STEP_SIZE, AUTO_NUM_MAX_STEP_SIZE, false}, {AUTO_NUM_PARAMETER_MIN, AUTO_NUM_PARAMETER_MAX, true},
    {AUTO_NUM_NORM_MIN, AUTO_NUM_NORM_MAX, true}};
/* the first step within the step sizes AUTO may take (T23), whichever its sign */
std::string step_rule()
{
    return xpp::format("The size of {} must be from {} to {} (its sign is the direction)", name_label(num_fields[AUTO_NUM_STEP_SIZE].key),
                       name_label(num_fields[AUTO_NUM_MIN_STEP_SIZE].key), name_label(num_fields[AUTO_NUM_MAX_STEP_SIZE].key));
}

/* what a field takes, as the refusal of a value it does not take says it
   (the event sends it, so the page says the same before it sends) */
std::string rule_message(const NumField &f)
{
    const std::string label = name_label(f.key);
    if (f.shape == AutoFieldShape::boolean) return xpp::format("{} must be true or false", label);
    if (f.shape == AutoFieldShape::choice) {
        std::string words;
        for (const std::string_view name : choices_of(static_cast<int>(&f - num_fields))) words += xpp::format("{}{}", words.empty() ? "" : ", ", name);
        return xpp::format("{} must be one of {}", label, words);
    }
    const std::string whole = f.shape == AutoFieldShape::whole ? "a whole number" : "a number";
    switch (f.rule) {
    case Rule::positive:
        return xpp::format("{} must be a number above 0", label);
    case Rule::nonzero:
        return xpp::format("{} must be a number other than 0", label);
    case Rule::range:
        if (f.lo == INT_MIN && f.hi == INT_MAX) break;
        if (f.hi == INT_MAX) return xpp::format("{} must be {} of at least {}", label, whole, static_cast<long>(f.lo));
        return xpp::format("{} must be {} from {} to {}", label, whole, static_cast<long>(f.lo), static_cast<long>(f.hi));
    case Rule::any:
        break;
    }
    return xpp::format("{} must be {}", label, whole);
}

std::string pair_message(const NumPair &p)
{
    return xpp::format("{} must be {} {}", name_label(num_fields[p.lo].key), p.strict ? "below" : "at most", name_label(num_fields[p.hi].key));
}

/* ---- the settings now ---- */

void read_num(const xpp::Session &s, double v[AUTO_NUM_N])
{
    const xpp::BIFUR &b = s.auto_state.bifur;
    const xpp::ADVAUTO &a = s.auto_state.advanced;
    const double now[AUTO_NUM_N] = {
        static_cast<double>(b.ntst), static_cast<double>(b.nmx), static_cast<double>(b.npr), static_cast<double>(b.ncol),
        b.ds, b.dsmin, b.dsmax, b.rl0, b.rl1, b.a0, b.a1, b.epsl, b.epsu, b.epss,
        static_cast<double>(a.iad), static_cast<double>(a.max_branch_switches), a.switch_one_direction ? 1.0 : 0.0,
        static_cast<double>(a.iid), static_cast<double>(a.itmx), static_cast<double>(a.itnw), static_cast<double>(a.nwtn),
        static_cast<double>(a.iads), s.auto_state.suppress_bp ? 1.0 : 0.0,
    };
    std::memcpy(v, now, sizeof now);
}

void write_num(xpp::Session &s, const double v[AUTO_NUM_N])
{
    auto i = [&](int k) { return static_cast<int>(v[k]); };
    xpp::BIFUR &b = s.auto_state.bifur;
    xpp::ADVAUTO &a = s.auto_state.advanced;
    b.ntst = i(AUTO_NUM_MESH_INTERVALS);
    b.nmx = i(AUTO_NUM_MAX_STEPS);
    b.npr = i(AUTO_NUM_OUTPUT_EVERY);
    b.ncol = i(AUTO_NUM_COLLOCATION_POINTS);
    b.ds = v[AUTO_NUM_STEP_SIZE];
    b.dsmin = v[AUTO_NUM_MIN_STEP_SIZE];
    b.dsmax = v[AUTO_NUM_MAX_STEP_SIZE];
    b.rl0 = v[AUTO_NUM_PARAMETER_MIN];
    b.rl1 = v[AUTO_NUM_PARAMETER_MAX];
    b.a0 = v[AUTO_NUM_NORM_MIN];
    b.a1 = v[AUTO_NUM_NORM_MAX];
    b.epsl = v[AUTO_NUM_PARAMETER_TOLERANCE];
    b.epsu = v[AUTO_NUM_STATE_TOLERANCE];
    b.epss = v[AUTO_NUM_SPECIAL_POINT_TOLERANCE];
    a.iad = i(AUTO_NUM_MESH_ADAPT_EVERY);
    a.max_branch_switches = i(AUTO_NUM_MAX_BRANCH_SWITCHES);
    a.switch_one_direction = i(AUTO_NUM_SWITCH_ONE_DIRECTION) != 0;
    a.iid = i(AUTO_NUM_OUTPUT_DETAIL);
    a.itmx = i(AUTO_NUM_MAX_LOCATION_ITERATIONS);
    a.itnw = i(AUTO_NUM_MAX_NEWTON_ITERATIONS);
    a.nwtn = i(AUTO_NUM_FULL_NEWTON_AFTER);
    a.iads = i(AUTO_NUM_STEP_ADAPT_EVERY);
    s.auto_state.suppress_bp = i(AUTO_NUM_SUPPRESS_BRANCH_POINTS) != 0;
}

/* ---- the event ---- */

/* a JSON string, or null for no name */
void add_str_or_null(std::string &o, const std::string &s)
{
    if (!s.empty()) xpp::json_append_string(o, s.c_str());
    else o += "null";
}

void add_num(std::string &o, double v) { o += xpp::json::json_finite(v) ? xpp::number(v) : std::string("null"); }

/* a name, or "" for none */
std::string name_or_empty(const char *s) { return s ? std::string(s) : std::string(); }

/* view axes a as the `set` command's axes keys */
void axes_of(const xpp::Session &s, const AUTOAX &a, AutoSettingsSet &set)
{
    set.plot = a.plot;
    if (a.var >= 0 && a.var < s.model().node) set.var = s.model().uvar_names[a.var];
    set.par1 = name_or_empty(auto_par_name(s, a.icp1));
    set.par2 = name_or_empty(auto_par_name(s, a.icp2));
    set.has_range.fill(1);
    set.range = {a.xmin, a.xmax, a.ymin, a.ymax};
}

AutoSettingsSet settings_now(const xpp::Session &s)
{
    AutoSettingsSet set;
    for (std::size_t i=0;i<auto_codim_kinds.size();++i) set.special_points[i]=s.auto_state.special_points[i];
    read_num(s, set.num.data());
    set.has_num.fill(1);
    axes_of(s, s.auto_state.axes(), set);
    set.nmarks = static_cast<int>(s.auto_state.bifur.marks.size());
    for (const auto &mark : s.auto_state.bifur.marks) {
        set.mark_name.push_back(mark.period ? std::string("T") : name_or_empty(auto_par_name(s, mark.parameter)));
        set.mark_value.push_back(mark.value);
    }
    return set;
}

std::string event_text(const xpp::Session &s)
{
    const AutoSettingsSet set = settings_now(s);
    std::string o = "{\"ev\":\"autosettings\",\"numerics\":{";
    for (int i = 0; i < AUTO_NUM_N; i++) {
        o += xpp::format("{}\"{}\":", i ? "," : "", num_fields[i].key);
        if (num_fields[i].shape == AutoFieldShape::boolean) o += set.num[i] != 0 ? "true" : "false";
        else if (num_fields[i].shape == AutoFieldShape::choice) xpp::json_append_string(o, auto_settings_num_text(i, set.num[i]).c_str());
        else add_num(o, set.num[i]);
    }
    o += xpp::format("}},\"axes\":{{\"plot\":\"{}\",\"var\":", auto_projection_name(*set.plot));
    add_str_or_null(o, set.var);
    o += ",\"par1\":";
    add_str_or_null(o, set.par1);
    o += ",\"par2\":";
    add_str_or_null(o, set.par2);
    static const char *const range_names[4] = {"xmin", "xmax", "ymin", "ymax"};
    for (int i = 0; i < 4; i++) {
        o += xpp::format(",\"{}\":", range_names[i]);
        add_num(o, set.range[i]);
    }
    o += "},\"marks\":[";
    for (int i = 0; i < set.nmarks.value_or(0); i++) {
        o += i ? ",[" : "[";
        add_str_or_null(o, set.mark_name[i]);
        o += ',';
        add_num(o, set.mark_value[i]);
        o += ']';
    }
    /* what each Numerics value takes and which must be in order, as `auto`
       `set` checks them (num_ok, apply), with the messages it refuses with */
    /* the thread count (a preference, not a setting): "auto" or the number,
       what automatic gives now, and the most the preference takes */
    o += "],\"special_points\":{";
    for (std::size_t i=0;i<auto_codim_kinds.size();++i)
        o += xpp::format("{}\"{}\":{}",i ? "," : "",auto_codim_keys[i],*set.special_points[i] ? "true" : "false");
    o += "},\"threads\":";
    if (s.auto_state.threads == prefs::AUTO_THREADS_AUTO) xpp::json_append_string(o, std::string(prefs::AUTO_THREADS_WORD).c_str());
    else o += xpp::number(s.auto_state.threads);
    o += xpp::format(",\"threads_auto\":{},\"threads_max\":{}", AutoParallel::automatic_threads(), cpu::logical_processors());
    o += ",\"rules\":{";
    for (int i = 0; i < AUTO_NUM_N; i++) {
        const NumField &f = num_fields[i];
        o += xpp::format("{}\"{}\":{{\"label\":", i ? "," : "", f.key);
        xpp::json_append_string(o, name_label(f.key));
        if (f.shape == AutoFieldShape::boolean) o += ",\"boolean\":true";
        if (f.shape == AutoFieldShape::choice) {
            o += ",\"choices\":[";
            bool first = true;
            for (const std::string_view name : choices_of(i)) {
                if (!first) o += ',';
                first = false;
                xpp::json_append_string(o, name);
            }
            o += ']';
        }
        if (f.shape == AutoFieldShape::whole) o += ",\"integer\":true";
        if (f.rule == Rule::positive) o += ",\"positive\":true";
        if (f.rule == Rule::nonzero) o += ",\"nonzero\":true";
        if (f.shape == AutoFieldShape::whole && f.rule == Rule::range && f.lo != INT_MIN) o += xpp::format(",\"min\":{}", static_cast<long>(f.lo));
        if (f.shape == AutoFieldShape::whole && f.rule == Rule::range && f.hi != INT_MAX) o += xpp::format(",\"max\":{}", static_cast<long>(f.hi));
        o += ",\"message\":";
        xpp::json_append_string(o, rule_message(f).c_str());
        o += '}';
    }
    o += "},\"pairs\":[";
    for (const NumPair &p : num_pairs) {
        o += xpp::format("{}{{\"lo\":\"{}\",\"hi\":\"{}\",\"strict\":{},\"message\":", &p == num_pairs ? "" : ",",
                         num_fields[p.lo].key, num_fields[p.hi].key, p.strict);
        xpp::json_append_string(o, pair_message(p).c_str());
        o += '}';
    }
    o += xpp::format("],\"step\":{{\"key\":\"{}\",\"lo\":\"{}\",\"hi\":\"{}\",\"message\":", num_fields[AUTO_NUM_STEP_SIZE].key,
                     num_fields[AUTO_NUM_MIN_STEP_SIZE].key, num_fields[AUTO_NUM_MAX_STEP_SIZE].key);
    xpp::json_append_string(o, step_rule());
    o += "}}";
    return o;
}

/* A loaded model with variables has AUTO settings. */
bool have_settings(const xpp::Session &s) { return s.model().node > 0; }

bool num_ok(int i, double v, std::string &why)
{
    if (i < 0 || i >= AUTO_NUM_N) {
        why = "no such AUTO setting";
        return false;
    }
    const NumField &f = num_fields[i];
    bool ok = std::isfinite(v) && !(is_whole(f) && (v != std::floor(v) || v < INT_MIN || v > INT_MAX));
    if (ok && f.rule == Rule::positive) ok = v > 0;
    if (ok && f.rule == Rule::nonzero) ok = v != 0;
    if (ok && f.rule == Rule::range) ok = v >= f.lo && v <= f.hi;
    if (!ok) why = xpp::format("{}={}: {}", f.key, xpp::number(v), rule_message(f));
    return ok;
}

/* what a set changes, checked (check), for commit to write */
struct Checked {
    double num[AUTO_NUM_N];
    int view = 0, var = 0, icp1 = 0, icp2 = 0;
    AutoProjection plot = AutoProjection::maximum;
    double range[4];
    std::vector<UserMark> marks;
};

/* a value of the set that is wrong: what, and its key ("ntst", "var",
   "mark2", ...; "" for the whole set) as the Error's field; at the
   command's place (a file of settings gives its own line instead) */
Error refused(std::string what, std::string key = {})
{
    return Error{"auto set", std::move(what), command_place(), std::move(key)};
}

/* set checked against s, as it will be when views views of the diagram
   are there (-1: the views s has) */
Result<Checked> check(const xpp::Session &s, const AutoSettingsSet *set, int views)
{
    Checked c;
    if (!have_settings(s)) return std::unexpected(refused("AUTO needs a model with variables"));
    /* Numerics: each value given, then the pairs that must be in order */
    double *num = c.num;
    read_num(s, num);
    for (int i = 0; i < AUTO_NUM_N; i++) {
        if (!set->has_num[i]) continue;
        num[i] = set->num[i];
    }
    if (auto valid = auto_settings_numbers_check(std::span<const double, AUTO_NUM_N>(num, AUTO_NUM_N), set->has_num); !valid)
        return std::unexpected(valid.error());

    /* the Axes: plot type, the variable, the two parameters among AUTO's, the ranges */
    /* of the view named (W50), else the active one: setting a view's axes makes it the active one */
    const int view = set->view < 0 ? s.auto_state.active_view : set->view;
    c.view = view;
    if (view >= (views < 0 ? static_cast<int>(s.auto_state.views.size()) : views)) {
        return std::unexpected(refused(xpp::format("there is no view {}", view), "view"));
    }
    /* a view not there yet (views) starts from the active one's axes */
    const AUTOAX &now = static_cast<std::size_t>(view) < s.auto_state.views.size() ? s.auto_state.views[static_cast<std::size_t>(view)].axes
                                                                                  : s.auto_state.axes();
    AutoProjection &plot = c.plot;
    int &var = c.var, &icp1 = c.icp1, &icp2 = c.icp2;
    plot = now.plot;
    var = now.var;
    icp1 = now.icp1;
    icp2 = now.icp2;
    double (&range)[4] = c.range;
    range[0] = now.xmin;
    range[1] = now.xmax;
    range[2] = now.ymin;
    range[3] = now.ymax;
    if (set->plot) plot = *set->plot;
    if (!set->var.empty()) {
        int col;
        find_variable(s,set->var.c_str(), &col);
        if (col < 1 || col > s.model().node) {
            return std::unexpected(refused(xpp::format("{} is not a variable AUTO computes", set->var), "var"));
        }
        var = col - 1;
    }
    const struct { const char *name; int *icp; const char *key; } axis_pars[] = {{set->par1.c_str(), &icp1, "par1"}, {set->par2.c_str(), &icp2, "par2"}};
    for (const auto &a : axis_pars) {
        if (!a.name[0]) continue;
        int k = xpp::find_user_name(s.model(), PARAM_BOX, a.name);
        if (k < 0) {
            return std::unexpected(refused(xpp::format("{} is not a model parameter", a.name), a.key));
        }
        *a.icp = k;
    }
    static const char *const range_names[4] = {"Xmin", "Xmax", "Ymin", "Ymax"};
    static const char *const range_keys[4] = {"xmin", "xmax", "ymin", "ymax"};
    for (int i = 0; i < 4; i++) {
        if (!set->has_range[i]) continue;
        if (!std::isfinite(set->range[i])) {
            return std::unexpected(refused(xpp::format("{} must be a number", range_names[i]), range_keys[i]));
        }
        range[i] = set->range[i];
    }
    for (int i = 0; i < 4; i += 2) {
        if ((set->has_range[i] || set->has_range[i + 1]) && !(range[i] < range[i + 1])) {
            return std::unexpected(refused(xpp::format("{} must be below {}", range_names[i], range_names[i + 1]), range_keys[i + 1]));
        }
    }

    /* Mark values: parameter (or T, the period) = value */
    if (set->nmarks && (*set->nmarks < 0 || *set->nmarks > auto_file_marks_limit ||
        (set->mark_name.size() != static_cast<std::size_t>(*set->nmarks) ||
         set->mark_value.size() != static_cast<std::size_t>(*set->nmarks)))) {
        return std::unexpected(refused("Mark values: count does not match the list", "mark"));
    }
    for (int i = 0; i < set->nmarks.value_or(0); i++) {
        const bool period = strcasecmp(set->mark_name[i].c_str(), "T") == 0;
        const int parameter = period ? xpp::auto_unknown::period : xpp::find_user_name(s.model(), PARAM_BOX, set->mark_name[i]);
        if (parameter < 0) {
            return std::unexpected(refused(xpp::format("Mark values: {} is not a model parameter or T", set->mark_name[i]), xpp::format("mark{}", i)));
        }
        if (!std::isfinite(set->mark_value[i])) {
            return std::unexpected(refused(xpp::format("Mark values: the value of {} must be a number", set->mark_name[i]), xpp::format("mark{}", i)));
        }
        c.marks.push_back({parameter, period, set->mark_value[i]});
    }

    return c;
}

/* set, checked into c, written as the forms do */
void commit(xpp::Session &s, const AutoSettingsSet *set, Checked &c)
{
    const double *num = c.num;
    const int view = c.view, var = c.var, icp1 = c.icp1, icp2 = c.icp2;
    const AutoProjection plot = c.plot;
    const double(&range)[4] = c.range;
    write_num(s, num);
    for (std::size_t i=0;i<auto_codim_kinds.size();++i)
        if (set->special_points[i]) s.auto_state.special_points[i]=*set->special_points[i];
    bool axes = set->plot.has_value() || !set->var.empty() || !set->par1.empty() || !set->par2.empty() || set->fit;
    for (int i = 0; i < 4; i++) axes = axes || set->has_range[i];
    if (axes) {
        s.auto_state.active_view = view;
        s.auto_state.axes().plot = plot;
        s.auto_state.axes().var = var;
        s.auto_state.axes().icp1 = icp1;
        s.auto_state.axes().icp2 = icp2;
        s.auto_state.axes().xmin = range[0];
        s.auto_state.axes().xmax = range[1];
        s.auto_state.axes().ymin = range[2];
        s.auto_state.axes().ymax = range[3];
        keep_axes_as_last(s);
        if (set->fit) auto_fit(s);
    }
    if (set->nmarks.has_value()) s.auto_state.bifur.marks.swap(c.marks);
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if (axes && s.auto_state.bifur.exist) redraw_diagram(s);
}

bool apply(xpp::Session &s, const AutoSettingsSet *set, std::string &why)
{
    Result<Checked> checked = check(s, set, -1);
    if (!checked) {
        why = checked.error().what;
        return false;
    }
    Checked &c = *checked;
    if (set->threads) {
        if (*set->threads != prefs::AUTO_THREADS_AUTO)
            if (const auto problem = prefs::auto_threads_problem(*set->threads)) {
                why = *problem;
                return false;
            }
        /* a recording's replay changes the session's count, never the user's preferences */
        if (!files::serving_reads()) {
            const auto saved = prefs::update_prefs([&](prefs::Preferences &p) { p.auto_threads = *set->threads; });
            if (!saved) {
                why = saved.error().text();
                return false;
            }
        }
        s.auto_state.threads = *set->threads;
    }
    commit(s, set, c);
    return true;
}

} // namespace

/* the event, none for a model with no settings */
std::string auto_settings_event_line(const xpp::Session &s) { return have_settings(s) ? event_text(s) : std::string(); }

const char *auto_settings_num_key(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].key : nullptr; }

std::string auto_settings_num_label(int i) { return i >= 0 && i < AUTO_NUM_N ? name_label(num_fields[i].key) : std::string(); }

AutoFieldShape auto_settings_num_shape(int i) { return num_fields[i].shape; }

std::span<const std::string_view> auto_settings_num_choices(int i) { return choices_of(i); }

std::string auto_settings_num_text(int i, double v)
{
    switch (num_fields[i].shape) {
    case AutoFieldShape::boolean: return v != 0 ? "true" : "false";
    case AutoFieldShape::choice: return std::string(choices_of(i)[static_cast<std::size_t>(v)]);
    case AutoFieldShape::whole:
    case AutoFieldShape::real: break;
    }
    return xpp::number(v);
}

Result<double> auto_settings_num_parse(int i, std::string_view text)
{
    const NumField &f = num_fields[i];
    double v = 0;
    switch (f.shape) {
    case AutoFieldShape::boolean: {
        bool enabled;
        if (!xpp::parse_bool(text, enabled)) return std::unexpected(refused(rule_message(f), f.key));
        return enabled ? 1.0 : 0.0;
    }
    case AutoFieldShape::choice: {
        const auto names = choices_of(i);
        const auto at = std::find(names.begin(), names.end(), text);
        if (at == names.end()) return std::unexpected(refused(xpp::format("{}, not \"{}\"", rule_message(f), text), f.key));
        return static_cast<double>(at - names.begin());
    }
    case AutoFieldShape::whole:
    case AutoFieldShape::real:
        if (!xpp::parse_number(text, v) || !std::isfinite(v)) return std::unexpected(refused(xpp::format("{} must be {}", name_label(f.key), is_whole(f) ? "a whole number" : "a number"), f.key));
        return v;
    }
    return v;
}

void auto_threads_load(Session &s)
{
    if (files::serving_reads()) return;
    const Result<prefs::Preferences> p = prefs::load_prefs();
    if (p) s.auto_state.threads = p->auto_threads;
    /* the count stays automatic until the file is fixed (a `set` of it refuses until then); said in the
       log, as --silent shows no recent models' note to carry the file's error */
    else xpp::log(XPP_LOG_WARN, "{}: AUTO's thread count stays automatic\n", p.error().text());
}

AutoSettingsSet auto_settings_now(const xpp::Session &s)
{
    try {
        return settings_now(s);
    } catch (...) {
        xpp::out_of_memory("reading AUTO's settings");
    }
}

void auto_settings_reload(Session &s, const AutoSettingsSet &kept)
{
    if (!have_settings(s)) return;
    AutoSettingsSet set = kept;
    const AutoSettingsSet fresh = settings_now(s);
    if (xpp::find_user_name(s.model(), ICBOX, set.var) < 0) set.var = fresh.var;
    const auto has_par = [&](const std::string &name) {
        return xpp::find_user_name(s.model(), PARAM_BOX, name) >= 0;
    };
    if (!has_par(set.par1)) set.par1 = fresh.par1;
    if (!has_par(set.par2)) set.par2 = fresh.par2;
    set.nmarks = 0;
    for (int i = 0; i < kept.nmarks.value_or(0); i++) {
        if (kept.mark_name[i] != "T" && xpp::find_user_name(s.model(), PARAM_BOX, kept.mark_name[i]) < 0) continue;
        set.mark_name[*set.nmarks] = kept.mark_name[i];
        set.mark_value[(*set.nmarks)++] = kept.mark_value[i];
    }
    set.mark_name.resize(*set.nmarks);
    set.mark_value.resize(*set.nmarks);
    /* The form can hold values that a run refuses (and a Fit can leave a
       flat range). Check the names through the normal settings owner,
       then commit the numbers and ranges already held in memory. */
    AutoSettingsSet names = set;
    names.has_num.fill(0);
    names.has_range.fill(0);
    Result<Checked> checked = check(s, &names, -1);
    if (!checked) {
        command_error("reload AUTO settings", checked.error().what);
        return;
    }
    Checked &c = *checked;
    std::copy(set.num.begin(), set.num.end(), c.num);
    std::copy(set.range.begin(), set.range.end(), c.range);
    commit(s, &set, c);
}

AutoSettingsSet auto_settings_view(const xpp::Session &s, int view)
{
    try {
        AutoSettingsSet set;
        axes_of(s, s.auto_state.views[static_cast<std::size_t>(view)].axes, set);
        set.view = view;
        return set;
    } catch (...) {
        xpp::out_of_memory("reading AUTO's settings");
    }
}

Result<> auto_settings_numbers_check(std::span<const double, AUTO_NUM_N> num,
                                    std::span<const int, AUTO_NUM_N> supplied)
{
    for (int i = 0; i < AUTO_NUM_N; ++i) {
        std::string why;
        if (supplied[i] && !num_ok(i, num[i], why)) return std::unexpected(refused(why, num_fields[i].key));
    }
    for (const NumPair &p : num_pairs) {
        if (!supplied[p.lo] && !supplied[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        return std::unexpected(refused(xpp::format("{} ({}={}, {}={})", pair_message(p), num_fields[p.lo].key, num[p.lo], num_fields[p.hi].key, num[p.hi]), num_fields[supplied[p.hi] ? p.hi : p.lo].key));
    }
    if ((supplied[AUTO_NUM_STEP_SIZE] || supplied[AUTO_NUM_MIN_STEP_SIZE] || supplied[AUTO_NUM_MAX_STEP_SIZE])
        && !(std::fabs(num[AUTO_NUM_STEP_SIZE]) >= num[AUTO_NUM_MIN_STEP_SIZE] && std::fabs(num[AUTO_NUM_STEP_SIZE]) <= num[AUTO_NUM_MAX_STEP_SIZE])) {
        return std::unexpected(refused(xpp::format("{} ({}={}, {}={}, {}={})", step_rule(), num_fields[AUTO_NUM_STEP_SIZE].key, num[AUTO_NUM_STEP_SIZE], num_fields[AUTO_NUM_MIN_STEP_SIZE].key, num[AUTO_NUM_MIN_STEP_SIZE], num_fields[AUTO_NUM_MAX_STEP_SIZE].key, num[AUTO_NUM_MAX_STEP_SIZE]), num_fields[supplied[AUTO_NUM_STEP_SIZE] ? AUTO_NUM_STEP_SIZE : supplied[AUTO_NUM_MIN_STEP_SIZE] ? AUTO_NUM_MIN_STEP_SIZE : AUTO_NUM_MAX_STEP_SIZE].key));
    }
    return {};
}

std::string auto_settings_num_schema(int i)
{
    const NumField &f = num_fields[i];
    switch (f.rule) {
    case Rule::positive: return ", \"exclusiveMinimum\": 0";
    case Rule::range: {
        std::string out;
        if (f.lo != INT_MIN) out += xpp::format(", \"minimum\": {}", static_cast<long>(f.lo));
        if (f.hi != INT_MAX) out += xpp::format(", \"maximum\": {}", static_cast<long>(f.hi));
        return out;
    }
    case Rule::nonzero:
    case Rule::any: break;
    }
    return {};
}

int auto_settings_num_ok(int i, double v, std::string &why)
{
    try {
        return num_ok(i, v, why) ? 1 : 0;
    } catch (...) {
        why.clear();
        try {
            why = "out of memory";
        } catch (...) {
        }
    }
    return 0;
}

Result<> auto_settings_check(const xpp::Session &s, const AutoSettingsSet &set, int views)
{
    if (auto checked = check(s, &set, views); !checked) return std::unexpected(std::move(checked.error()));
    return {};
}

int auto_settings_apply(xpp::Session &s, const AutoSettingsSet &set, std::string &why)
{
    try {
        if (apply(s, &set, why)) return 0;
    } catch (...) {
        /* Validation builds held marks before any setting is written. */
        why.clear();
        try {
            why = "out of memory";
        } catch (...) {
        }
    }
    return -1;
}

} // namespace xpp
