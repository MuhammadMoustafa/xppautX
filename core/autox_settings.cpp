#include "event_registry.h"
#include "autox_unknowns.h"
/* autoX's settings as data: the "autoxsettings" event and the `autox` `set`
   command's checks and writes (autox_settings.h, docs/protocol.md "autoX's
   settings as data").

   The same fields the autoX window's forms write (autox_nox.cpp autox_num_par,
   autox_plot_par, autox_per_par), with every value checked
   first: autoX takes some values badly (Ncol above 7 exits the program,
   autox_lib1.cpp cpnts), and a form typed into by hand could always send them. */
#include <cmath>
#include <climits>
#include <cstring>
#include <string>
#include <strings.h>

#include "model.h"
#include "session.h"
#include "autox_settings.h"
#include "model_options.h"
#include "xpp_session_autox.h"
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
#include "autox_parallel.h"
#include "xpp_cpu.h"
#include "xpp_files.h"
#include "xpp_prefs.h"
#include "json_number.h"

namespace xpp {

namespace {

constexpr int PARAM_BOX = 1; /* find_user_name's parameters */

enum class Rule { any, positive, nonzero, range };

struct NumField {
    int index; /* AUTOX_NUM_* */
    const char *key;
    AutoxFieldShape shape;
    Rule rule;
    double lo, hi; /* Rule::range, inclusive; a boolean's and a choice's index range too */
};

constexpr int output_detail_levels = static_cast<int>(autox_output_detail_names.size());

/* what autoX accepts for each field, by AUTOX_NUM_*; the key and the shape are autox_settings.h's lists */
struct NumRule {
    Rule rule;
    double lo, hi; /* Rule::range, inclusive; a boolean's and a choice's index range too */
};
constexpr std::array<NumRule, AUTOX_NUM_N> num_rules = {{
    {Rule::range, 1, autox_max_mesh_intervals},       /* mesh_intervals */
    {Rule::range, 1, INT_MAX},                       /* max_steps */
    {Rule::range, 1, INT_MAX},                       /* output_every */
    {Rule::range, autox_min_collocation_points, autox_max_collocation_points}, /* collocation_points */
    {Rule::nonzero, 0, 0},                           /* step_size */
    {Rule::positive, 0, 0},                          /* min_step_size */
    {Rule::positive, 0, 0},                          /* max_step_size */
    {Rule::any, 0, 0},                               /* parameter_min */
    {Rule::any, 0, 0},                               /* parameter_max */
    {Rule::any, 0, 0},                               /* norm_min */
    {Rule::any, 0, 0},                               /* norm_max */
    {Rule::positive, 0, 0},                          /* parameter_tolerance */
    {Rule::positive, 0, 0},                          /* state_tolerance */
    {Rule::positive, 0, 0},                          /* special_point_tolerance */
    {Rule::range, 0, INT_MAX},                       /* mesh_adapt_every */
    {Rule::range, 0, INT_MAX},                       /* max_branch_switches */
    {Rule::range, 0, 1},                             /* switch_one_direction */
    {Rule::range, 0, output_detail_levels - 1},      /* output_detail */
    {Rule::range, 1, INT_MAX},                       /* max_location_iterations */
    {Rule::range, 1, INT_MAX},                       /* max_newton_iterations */
    {Rule::range, 1, INT_MAX},                       /* full_newton_after */
    {Rule::range, 0, INT_MAX},                       /* step_adapt_every */
    {Rule::range, 0, 1},                             /* suppress_branch_points */
    {Rule::range, 0, 1},                             /* follow_all_branches */
}};

/* the option table's row of Numerics field i */
const OptionRow &numeric_row(int i)
{
    const auto rows = option_rows();
    return *std::ranges::find_if(rows, [i](const OptionRow &r) { return r.autox_numeric == i; });
}

AutoxFieldShape shape_of(const OptionRow &row)
{
    if (row.boolean) return AutoxFieldShape::boolean;
    if (row.kind == OptionKind::choice) return AutoxFieldShape::choice;
    return row.kind == OptionKind::whole ? AutoxFieldShape::whole : AutoxFieldShape::real;
}

/* Numerics field i: its row's key and shape, and what autoX accepts for it */
NumField field(int i)
{
    const OptionRow &row = numeric_row(i);
    const NumRule &rule = num_rules[static_cast<std::size_t>(i)];
    return NumField{i, row.odex.data(), shape_of(row), rule.rule, rule.lo, rule.hi};
}

/* the names a choice field takes */
std::span<const char *const> choices_of(int i)
{
    return i == AUTOX_NUM_OUTPUT_DETAIL ? std::span<const char *const>(autox_output_detail_names) : std::span<const char *const>();
}

bool is_whole(const NumField &f) { return f.shape != AutoxFieldShape::real; }

/* the values that must be in order: lo below hi (strict) or at most hi */
struct NumPair {
    int lo, hi;
    bool strict;
};
constexpr NumPair num_pairs[] = {
    {AUTOX_NUM_MIN_STEP_SIZE, AUTOX_NUM_MAX_STEP_SIZE, false}, {AUTOX_NUM_PARAMETER_MIN, AUTOX_NUM_PARAMETER_MAX, true},
    {AUTOX_NUM_NORM_MIN, AUTOX_NUM_NORM_MAX, true}};
/* the first step within the step sizes autoX may take (T23), whichever its sign */
std::string step_rule()
{
    return xpp::format("The size of {} must be from {} to {} (its sign is the direction)", name_label(field(AUTOX_NUM_STEP_SIZE).key),
                       name_label(field(AUTOX_NUM_MIN_STEP_SIZE).key), name_label(field(AUTOX_NUM_MAX_STEP_SIZE).key));
}

/* what a field takes, as the refusal of a value it does not take says it
   (the event sends it, so the page says the same before it sends) */
std::string rule_message(const NumField &f)
{
    const std::string label = name_label(f.key);
    if (f.shape == AutoxFieldShape::boolean) return xpp::format("{} must be true or false", label);
    if (f.shape == AutoxFieldShape::choice) {
        std::string words;
        for (const char *name : choices_of(f.index)) words += xpp::format("{}{}", words.empty() ? "" : ", ", name);
        return xpp::format("{} must be one of {}", label, words);
    }
    const std::string whole = f.shape == AutoxFieldShape::whole ? "a whole number" : "a number";
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
    return xpp::format("{} must be {} {}", name_label(field(p.lo).key), p.strict ? "below" : "at most", name_label(field(p.hi).key));
}

/* ---- the settings now ---- */

void read_num(const xpp::Session &s, double v[AUTOX_NUM_N])
{
    const xpp::BIFUR &b = s.autox_state.bifur;
    const xpp::ADVAUTOX &a = s.autox_state.advanced;
    const double now[AUTOX_NUM_N] = {
        static_cast<double>(b.ntst), static_cast<double>(b.nmx), static_cast<double>(b.npr), static_cast<double>(b.ncol),
        b.ds, b.dsmin, b.dsmax, b.rl0, b.rl1, b.a0, b.a1, b.epsl, b.epsu, b.epss,
        static_cast<double>(a.iad), static_cast<double>(a.max_branch_switches), a.switch_one_direction ? 1.0 : 0.0,
        static_cast<double>(a.iid), static_cast<double>(a.itmx), static_cast<double>(a.itnw), static_cast<double>(a.nwtn),
        static_cast<double>(a.iads), s.autox_state.suppress_bp ? 1.0 : 0.0, s.autox_state.follow_all_branches ? 1.0 : 0.0,
    };
    std::memcpy(v, now, sizeof now);
}

void write_num(xpp::Session &s, const double v[AUTOX_NUM_N])
{
    auto i = [&](int k) { return static_cast<int>(v[k]); };
    xpp::BIFUR &b = s.autox_state.bifur;
    xpp::ADVAUTOX &a = s.autox_state.advanced;
    b.ntst = i(AUTOX_NUM_MESH_INTERVALS);
    b.nmx = i(AUTOX_NUM_MAX_STEPS);
    b.npr = i(AUTOX_NUM_OUTPUT_EVERY);
    b.ncol = i(AUTOX_NUM_COLLOCATION_POINTS);
    b.ds = v[AUTOX_NUM_STEP_SIZE];
    b.dsmin = v[AUTOX_NUM_MIN_STEP_SIZE];
    b.dsmax = v[AUTOX_NUM_MAX_STEP_SIZE];
    b.rl0 = v[AUTOX_NUM_PARAMETER_MIN];
    b.rl1 = v[AUTOX_NUM_PARAMETER_MAX];
    b.a0 = v[AUTOX_NUM_NORM_MIN];
    b.a1 = v[AUTOX_NUM_NORM_MAX];
    b.epsl = v[AUTOX_NUM_PARAMETER_TOLERANCE];
    b.epsu = v[AUTOX_NUM_STATE_TOLERANCE];
    b.epss = v[AUTOX_NUM_SPECIAL_POINT_TOLERANCE];
    a.iad = i(AUTOX_NUM_MESH_ADAPT_EVERY);
    a.max_branch_switches = i(AUTOX_NUM_MAX_BRANCH_SWITCHES);
    a.switch_one_direction = i(AUTOX_NUM_SWITCH_ONE_DIRECTION) != 0;
    a.iid = i(AUTOX_NUM_OUTPUT_DETAIL);
    a.itmx = i(AUTOX_NUM_MAX_LOCATION_ITERATIONS);
    a.itnw = i(AUTOX_NUM_MAX_NEWTON_ITERATIONS);
    a.nwtn = i(AUTOX_NUM_FULL_NEWTON_AFTER);
    a.iads = i(AUTOX_NUM_STEP_ADAPT_EVERY);
    s.autox_state.suppress_bp = i(AUTOX_NUM_SUPPRESS_BRANCH_POINTS) != 0;
    s.autox_state.follow_all_branches = i(AUTOX_NUM_FOLLOW_ALL_BRANCHES) != 0;
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
void axes_of(const xpp::Session &s, const AUTOAX &a, AutoxSettingsSet &set)
{
    set.plot = a.plot;
    if (a.var >= 0 && a.var < s.model().node) set.var = s.model().uvar_names[a.var];
    set.par1 = name_or_empty(autox_par_name(s, a.icp1));
    set.par2 = name_or_empty(autox_par_name(s, a.icp2));
    set.has_range.fill(1);
    set.range = {a.xmin, a.xmax, a.ymin, a.ymax};
}

AutoxSettingsSet settings_now(const xpp::Session &s)
{
    AutoxSettingsSet set;
    for (std::size_t i=0;i<autox_codim_kinds.size();++i) set.special_points[i]=s.autox_state.special_points[i];
    read_num(s, set.num.data());
    set.has_num.fill(1);
    axes_of(s, s.autox_state.axes(), set);
    set.nmarks = static_cast<int>(s.autox_state.bifur.marks.size());
    for (const auto &mark : s.autox_state.bifur.marks) {
        set.mark_name.push_back(mark.period ? std::string("T") : name_or_empty(autox_par_name(s, mark.parameter)));
        set.mark_value.push_back(mark.value);
    }
    return set;
}

std::string event_text(const xpp::Session &s)
{
    const AutoxSettingsSet set = settings_now(s);
    std::string o = xpp::events::open(xpp::events::AUTOX_SETTINGS) + ",\"numerics\":{";
    for (int i = 0; i < AUTOX_NUM_N; i++) {
        o += xpp::format("{}\"{}\":", i ? "," : "", field(i).key);
        if (field(i).shape == AutoxFieldShape::boolean) o += set.num[i] != 0 ? "true" : "false";
        else if (field(i).shape == AutoxFieldShape::choice) xpp::json_append_string(o, autox_settings_num_text(i, set.num[i]).c_str());
        else add_num(o, set.num[i]);
    }
    o += xpp::format("}},\"axes\":{{\"plot\":\"{}\",\"var\":", autox_projection_name(*set.plot));
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
    /* what each Numerics value takes and which must be in order, as `autox`
       `set` checks them (num_ok, apply), with the messages it refuses with */
    /* the thread count (a preference, not a setting): "auto" or the number,
       what automatic gives now, and the most the preference takes */
    o += "],\"special_points\":{";
    for (std::size_t i=0;i<autox_codim_kinds.size();++i)
        o += xpp::format("{}\"{}\":{}",i ? "," : "",autox_codim_keys[i],*set.special_points[i] ? "true" : "false");
    o += "},\"threads\":";
    if (s.threads == prefs::THREADS_AUTO) xpp::json_append_string(o, std::string(prefs::THREADS_AUTO_WORD).c_str());
    else o += xpp::number(s.threads);
    o += xpp::format(",\"threads_auto\":{},\"threads_max\":{}", AutoxParallel::automatic_threads(), cpu::logical_processors());
    o += ",\"rules\":{";
    for (int i = 0; i < AUTOX_NUM_N; i++) {
        const NumField f = field(i);
        o += xpp::format("{}\"{}\":{{\"label\":", i ? "," : "", f.key);
        xpp::json_append_string(o, name_label(f.key));
        if (f.shape == AutoxFieldShape::boolean) o += ",\"boolean\":true";
        if (f.shape == AutoxFieldShape::choice) {
            o += ",\"choices\":[";
            bool first = true;
            for (const char *name : choices_of(i)) {
                if (!first) o += ',';
                first = false;
                xpp::json_append_string(o, name);
            }
            o += ']';
        }
        if (f.shape == AutoxFieldShape::whole) o += ",\"integer\":true";
        if (f.rule == Rule::positive) o += ",\"positive\":true";
        if (f.rule == Rule::nonzero) o += ",\"nonzero\":true";
        if (f.shape == AutoxFieldShape::whole && f.rule == Rule::range && f.lo != INT_MIN) o += xpp::format(",\"min\":{}", static_cast<long>(f.lo));
        if (f.shape == AutoxFieldShape::whole && f.rule == Rule::range && f.hi != INT_MAX) o += xpp::format(",\"max\":{}", static_cast<long>(f.hi));
        o += ",\"message\":";
        xpp::json_append_string(o, rule_message(f).c_str());
        o += '}';
    }
    o += "},\"pairs\":[";
    for (const NumPair &p : num_pairs) {
        o += xpp::format("{}{{\"lo\":\"{}\",\"hi\":\"{}\",\"strict\":{},\"message\":", &p == num_pairs ? "" : ",",
                         field(p.lo).key, field(p.hi).key, p.strict);
        xpp::json_append_string(o, pair_message(p).c_str());
        o += '}';
    }
    o += xpp::format("],\"step\":{{\"key\":\"{}\",\"lo\":\"{}\",\"hi\":\"{}\",\"message\":", field(AUTOX_NUM_STEP_SIZE).key,
                     field(AUTOX_NUM_MIN_STEP_SIZE).key, field(AUTOX_NUM_MAX_STEP_SIZE).key);
    xpp::json_append_string(o, step_rule());
    o += "}}";
    return o;
}

/* A loaded model with variables has autoX settings. */
bool have_settings(const xpp::Session &s) { return s.model().node > 0; }

bool num_ok(int i, double v, std::string &why)
{
    if (i < 0 || i >= AUTOX_NUM_N) {
        why = "no such autoX setting";
        return false;
    }
    const NumField f = field(i);
    bool ok = std::isfinite(v) && !(is_whole(f) && (v != std::floor(v) || v < INT_MIN || v > INT_MAX));
    if (ok && f.rule == Rule::positive) ok = v > 0;
    if (ok && f.rule == Rule::nonzero) ok = v != 0;
    if (ok && f.rule == Rule::range) ok = v >= f.lo && v <= f.hi;
    if (!ok) why = xpp::format("{}={}: {}", f.key, xpp::number(v), rule_message(f));
    return ok;
}

/* what a set changes, checked (check), for commit to write */
struct Checked {
    double num[AUTOX_NUM_N];
    int view = 0, var = 0, icp1 = 0, icp2 = 0;
    AutoxProjection plot = AutoxProjection::maximum;
    double range[4];
    std::vector<UserMark> marks;
};

/* a value of the set that is wrong: what, and its key ("ntst", "var",
   "mark2", ...; "" for the whole set) as the Error's field; at the
   command's place (a file of settings gives its own line instead) */
Error refused(std::string what, std::string key = {})
{
    return Error{"autox set", std::move(what), command_place(), std::move(key)};
}

/* set checked against s, as it will be when views views of the diagram
   are there (-1: the views s has) */
Result<Checked> check(const xpp::Session &s, const AutoxSettingsSet *set, int views)
{
    Checked c;
    if (!have_settings(s)) return std::unexpected(refused("autoX needs a model with variables"));
    /* Numerics: each value given, then the pairs that must be in order */
    double *num = c.num;
    read_num(s, num);
    for (int i = 0; i < AUTOX_NUM_N; i++) {
        if (!set->has_num[i]) continue;
        num[i] = set->num[i];
    }
    if (auto valid = autox_settings_numbers_check(std::span<const double, AUTOX_NUM_N>(num, AUTOX_NUM_N), set->has_num); !valid)
        return std::unexpected(valid.error());

    /* the Axes: plot type, the variable, the two parameters among autoX's, the ranges */
    /* of the view named (W50), else the active one: setting a view's axes makes it the active one */
    const int view = set->view < 0 ? s.autox_state.active_view : set->view;
    c.view = view;
    if (view >= (views < 0 ? static_cast<int>(s.autox_state.views.size()) : views)) {
        return std::unexpected(refused(xpp::format("there is no view {}", view), "view"));
    }
    /* a view not there yet (views) starts from the active one's axes */
    const AUTOAX &now = static_cast<std::size_t>(view) < s.autox_state.views.size() ? s.autox_state.views[static_cast<std::size_t>(view)].axes
                                                                                  : s.autox_state.axes();
    AutoxProjection &plot = c.plot;
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
            return std::unexpected(refused(xpp::format("{} is not a variable autoX computes", set->var), "var"));
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
    if (set->nmarks && (*set->nmarks < 0 || *set->nmarks > autox_file_marks_limit ||
        (set->mark_name.size() != static_cast<std::size_t>(*set->nmarks) ||
         set->mark_value.size() != static_cast<std::size_t>(*set->nmarks)))) {
        return std::unexpected(refused("Mark values: count does not match the list", "mark"));
    }
    for (int i = 0; i < set->nmarks.value_or(0); i++) {
        const bool period = strcasecmp(set->mark_name[i].c_str(), "T") == 0;
        const int parameter = period ? xpp::autox_unknown::period : xpp::find_user_name(s.model(), PARAM_BOX, set->mark_name[i]);
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
void commit(xpp::Session &s, const AutoxSettingsSet *set, Checked &c)
{
    const double *num = c.num;
    const int view = c.view, var = c.var, icp1 = c.icp1, icp2 = c.icp2;
    const AutoxProjection plot = c.plot;
    const double(&range)[4] = c.range;
    write_num(s, num);
    for (std::size_t i=0;i<autox_codim_kinds.size();++i)
        if (set->special_points[i]) s.autox_state.special_points[i]=*set->special_points[i];
    bool axes = set->plot.has_value() || !set->var.empty() || !set->par1.empty() || !set->par2.empty() || set->fit;
    for (int i = 0; i < 4; i++) axes = axes || set->has_range[i];
    if (axes) {
        s.autox_state.active_view = view;
        s.autox_state.axes().plot = plot;
        s.autox_state.axes().var = var;
        s.autox_state.axes().icp1 = icp1;
        s.autox_state.axes().icp2 = icp2;
        s.autox_state.axes().xmin = range[0];
        s.autox_state.axes().xmax = range[1];
        s.autox_state.axes().ymin = range[2];
        s.autox_state.axes().ymax = range[3];
        keep_axes_as_last(s);
        if (set->fit) autox_fit(s);
    }
    if (set->nmarks.has_value()) s.autox_state.bifur.marks.swap(c.marks);
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if (axes && s.autox_state.bifur.exist) redraw_diagram(s);
}

bool apply(xpp::Session &s, const AutoxSettingsSet *set, std::string &why)
{
    Result<Checked> checked = check(s, set, -1);
    if (!checked) {
        why = checked.error().what;
        return false;
    }
    Checked &c = *checked;
    if (set->threads) {
        if (*set->threads != prefs::THREADS_AUTO)
            if (const auto problem = prefs::threads_problem(*set->threads)) {
                why = *problem;
                return false;
            }
        /* a recording's replay changes the session's count, never the user's preferences */
        if (!files::serving_reads()) {
            const auto saved = prefs::update_prefs([&](prefs::Preferences &p) { p.threads = *set->threads; });
            if (!saved) {
                why = saved.error().text();
                return false;
            }
        }
        s.threads = *set->threads;
    }
    commit(s, set, c);
    return true;
}

} // namespace

/* the event, none for a model with no settings */
std::string autox_settings_event_line(const xpp::Session &s) { return have_settings(s) ? event_text(s) : std::string(); }

const char *autox_settings_num_key(int i) { return i >= 0 && i < AUTOX_NUM_N ? field(i).key : nullptr; }

AutoxFieldShape autox_settings_num_shape(int i) { return field(i).shape; }



std::string autox_settings_num_text(int i, double v)
{
    switch (field(i).shape) {
    case AutoxFieldShape::boolean: return v != 0 ? "true" : "false";
    case AutoxFieldShape::choice: return std::string(choices_of(i)[static_cast<std::size_t>(v)]);
    case AutoxFieldShape::whole:
    case AutoxFieldShape::real: break;
    }
    return xpp::number(v);
}

Result<double> autox_settings_num_parse(int i, std::string_view text)
{
    const NumField f = field(i);
    double v = 0;
    switch (f.shape) {
    case AutoxFieldShape::boolean: {
        bool enabled;
        if (!xpp::parse_bool(text, enabled)) return std::unexpected(refused(rule_message(f), f.key));
        return enabled ? 1.0 : 0.0;
    }
    case AutoxFieldShape::choice: {
        const auto names = choices_of(i);
        const auto at = std::find_if(names.begin(), names.end(), [&](const char *name) { return text == name; });
        if (at == names.end()) return std::unexpected(refused(xpp::format("{}, not \"{}\"", rule_message(f), text), f.key));
        return static_cast<double>(at - names.begin());
    }
    case AutoxFieldShape::whole:
    case AutoxFieldShape::real:
        if (!xpp::parse_number(text, v) || !std::isfinite(v)) return std::unexpected(refused(xpp::format("{} must be {}", name_label(f.key), is_whole(f) ? "a whole number" : "a number"), f.key));
        return v;
    }
    return v;
}

void preferences_load(Session &s)
{
    if (files::serving_reads()) return;
    const Result<prefs::Preferences> p = prefs::load_prefs();
    if (p) {
        s.threads = p->threads;
        s.colors = p->colors;
    }
    /* the count stays automatic and the colours the defaults until the file is fixed (a `set` of the count refuses until then); said in the
       log, as --silent shows no recent models' note to carry the file's error */
    else xpp::log(XPP_LOG_WARN, "{}: the thread count stays automatic and the colours the defaults\n", p.error().text());
}

AutoxSettingsSet autox_settings_now(const xpp::Session &s)
{
    try {
        return settings_now(s);
    } catch (...) {
        xpp::out_of_memory("reading autoX's settings");
    }
}

void autox_settings_reload(Session &s, const AutoxSettingsSet &kept)
{
    if (!have_settings(s)) return;
    AutoxSettingsSet set = kept;
    const AutoxSettingsSet fresh = settings_now(s);
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
    AutoxSettingsSet names = set;
    names.has_num.fill(0);
    names.has_range.fill(0);
    Result<Checked> checked = check(s, &names, -1);
    if (!checked) {
        command_error("reload autoX settings", checked.error().what);
        return;
    }
    Checked &c = *checked;
    std::copy(set.num.begin(), set.num.end(), c.num);
    std::copy(set.range.begin(), set.range.end(), c.range);
    commit(s, &set, c);
}

AutoxSettingsSet autox_settings_view(const xpp::Session &s, int view)
{
    try {
        AutoxSettingsSet set;
        axes_of(s, s.autox_state.views[static_cast<std::size_t>(view)].axes, set);
        set.view = view;
        return set;
    } catch (...) {
        xpp::out_of_memory("reading autoX's settings");
    }
}

Result<> autox_settings_numbers_check(std::span<const double, AUTOX_NUM_N> num,
                                    std::span<const int, AUTOX_NUM_N> supplied)
{
    for (int i = 0; i < AUTOX_NUM_N; ++i) {
        std::string why;
        if (supplied[i] && !num_ok(i, num[i], why)) return std::unexpected(refused(why, field(i).key));
    }
    for (const NumPair &p : num_pairs) {
        if (!supplied[p.lo] && !supplied[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        return std::unexpected(refused(xpp::format("{} ({}={}, {}={})", pair_message(p), field(p.lo).key, num[p.lo], field(p.hi).key, num[p.hi]), field(supplied[p.hi] ? p.hi : p.lo).key));
    }
    if ((supplied[AUTOX_NUM_STEP_SIZE] || supplied[AUTOX_NUM_MIN_STEP_SIZE] || supplied[AUTOX_NUM_MAX_STEP_SIZE])
        && !(std::fabs(num[AUTOX_NUM_STEP_SIZE]) >= num[AUTOX_NUM_MIN_STEP_SIZE] && std::fabs(num[AUTOX_NUM_STEP_SIZE]) <= num[AUTOX_NUM_MAX_STEP_SIZE])) {
        return std::unexpected(refused(xpp::format("{} ({}={}, {}={}, {}={})", step_rule(), field(AUTOX_NUM_STEP_SIZE).key, num[AUTOX_NUM_STEP_SIZE], field(AUTOX_NUM_MIN_STEP_SIZE).key, num[AUTOX_NUM_MIN_STEP_SIZE], field(AUTOX_NUM_MAX_STEP_SIZE).key, num[AUTOX_NUM_MAX_STEP_SIZE]), field(supplied[AUTOX_NUM_STEP_SIZE] ? AUTOX_NUM_STEP_SIZE : supplied[AUTOX_NUM_MIN_STEP_SIZE] ? AUTOX_NUM_MIN_STEP_SIZE : AUTOX_NUM_MAX_STEP_SIZE).key));
    }
    return {};
}

std::string autox_settings_num_schema(int i)
{
    const NumField f = field(i);
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

int autox_settings_num_ok(int i, double v, std::string &why)
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

Result<> autox_settings_check(const xpp::Session &s, const AutoxSettingsSet &set, int views)
{
    if (auto checked = check(s, &set, views); !checked) return std::unexpected(std::move(checked.error()));
    return {};
}

int autox_settings_apply(xpp::Session &s, const AutoxSettingsSet &set, std::string &why)
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

namespace {
/* the view rows by option name: the variable, then the four edges in AUTOAX's order (xmin, xmax, ymin, ymax) */
constexpr std::string_view view_variable = "diagram.variable";
constexpr std::array<std::string_view, 4> view_edges = {"diagram.x_min", "diagram.x_max", "diagram.y_min", "diagram.y_max"};
} // namespace

bool autox_view_row(const OptionRow &row)
{
    const std::string name = option_name(row);
    return name == view_variable || std::ranges::find(view_edges, name) != view_edges.end();
}

std::string autox_view_text(const Session &s, const OptionRow &row)
{
    const AUTOAX &a = s.autox_state.axes();
    const std::string name = option_name(row);
    if (name == view_variable) return a.var >= 0 && a.var < s.model().node ? s.model().uvar_names[a.var] : std::string();
    const double edges[4] = {a.xmin, a.xmax, a.ymin, a.ymax};
    return xpp::number(edges[std::ranges::find(view_edges, name) - view_edges.begin()]);
}

Result<> autox_view_apply(Session &s, std::span<const std::pair<const OptionRow *, std::string>> items, bool check_only)
{
    AutoxSettingsSet set;
    for (const auto &[row, text] : items) {
        const std::string name = option_name(*row);
        if (name == view_variable) {
            set.var = text;
            continue;
        }
        const std::size_t i = static_cast<std::size_t>(std::ranges::find(view_edges, name) - view_edges.begin());
        double value = 0;
        if (!parse_number(text, value)) return fail("diagram", xpp::format("{}={}: not a number", row->odex, text), command_place());
        set.range[i] = value;
        set.has_range[i] = 1;
    }
    if (Result<> checked = autox_settings_check(s, set, -1); !checked) return checked;
    if (check_only) return {};
    std::string why;
    if (autox_settings_apply(s, set, why) != 0) return fail("diagram", why, command_place());
    return {};
}

} // namespace xpp
