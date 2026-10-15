#include "auto_unknowns.h"
/* AUTO's settings as data: the "autosettings" event and the `auto` `set`
   command's checks and writes (auto_settings.h, docs/protocol.md "AUTO's
   settings as data").

   The same fields the AUTO window's forms write (auto_nox.cpp auto_num_par,
   auto_plot_par, auto_per_par), with every value checked
   first: AUTO takes some values badly (Ncol above 7 exits the program,
   autlib1.c cpnts), and a form typed into by hand could always send them. */
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
#include "form_ode.h"
#include "xpp_mem.h"
#include "auto_parallel.h"
#include "xpp_cpu.h"
#include "xpp_files.h"
#include "xpp_prefs.h"

namespace xpp {

namespace {

constexpr int PARAM_BOX = 1; /* find_user_name's parameters */

enum class Rule { any, positive, nonzero, range };

struct NumField {
    const char *key, *label;
    bool integer;
    Rule rule;
    double lo, hi; /* Rule::range, inclusive */
};

/* the Numerics form's fields and what AUTO accepts for each */
constexpr NumField num_fields[AUTO_NUM_N] = {
    {"ntst", "Ntst", true, Rule::range, auto_min_mesh_intervals, auto_max_mesh_intervals},
    {"nmx", "Nmax", true, Rule::range, 1, static_cast<int>(snapx::auto_members::saved_points_limit)},
    {"npr", "NPr", true, Rule::range, 1, INT_MAX},
    {"ncol", "Ncol", true, Rule::range, auto_min_collocation_points, auto_max_collocation_points},
    {"ds", "Ds", false, Rule::nonzero, 0, 0},
    {"dsmin", "Dsmin", false, Rule::positive, 0, 0},
    {"dsmax", "Dsmax", false, Rule::positive, 0, 0},
    {"rl0", "Par Min", false, Rule::any, 0, 0},
    {"rl1", "Par Max", false, Rule::any, 0, 0},
    {"a0", "Norm Min", false, Rule::any, 0, 0},
    {"a1", "Norm Max", false, Rule::any, 0, 0},
    {"epsl", "EPSL", false, Rule::positive, 0, 0},
    {"epsu", "EPSU", false, Rule::positive, 0, 0},
    {"epss", "EPSS", false, Rule::positive, 0, 0},
    {"iad", "IAD", true, Rule::range, 0, INT_MAX},
    {"mxbf", "MXBF", true, Rule::range, INT_MIN, INT_MAX},
    {"iid", "IID", true, Rule::range, auto_min_diagnostic_level, auto_max_diagnostic_level},
    {"itmx", "ITMX", true, Rule::range, 1, INT_MAX},
    {"itnw", "ITNW", true, Rule::range, 1, INT_MAX},
    {"nwtn", "NWTN", true, Rule::range, 1, INT_MAX},
    {"iads", "IADS", true, Rule::range, 0, INT_MAX},
    {"suppbp", "SuppBP", true, Rule::range, 0, 1},
};

/* the values that must be in order: lo below hi (strict) or at most hi */
struct NumPair {
    int lo, hi;
    bool strict;
};
constexpr NumPair num_pairs[] = {
    {AUTO_NUM_DSMIN, AUTO_NUM_DSMAX, false}, {AUTO_NUM_RL0, AUTO_NUM_RL1, true}, {AUTO_NUM_A0, AUTO_NUM_A1, true}};
/* the first step within the step sizes AUTO may take (T23), whichever its sign */
constexpr const char *step_rule = "Ds must be from Dsmin to Dsmax in size (its sign is the direction)";

/* what a field takes, as the refusal of a value it does not take says it
   (the event sends it, so the page says the same before it sends) */
std::string rule_message(const NumField &f)
{
    const std::string whole = f.integer ? "a whole number" : "a number";
    switch (f.rule) {
    case Rule::positive:
        return xpp::format("{} must be a number above 0", f.label);
    case Rule::nonzero:
        return xpp::format("{} must be a number other than 0", f.label);
    case Rule::range:
        if (f.lo == INT_MIN && f.hi == INT_MAX) break;
        if (f.hi == INT_MAX) return xpp::format("{} must be {} of at least {}", f.label, whole, static_cast<long>(f.lo));
        return xpp::format("{} must be {} from {} to {}", f.label, whole, static_cast<long>(f.lo), static_cast<long>(f.hi));
    case Rule::any:
        break;
    }
    return xpp::format("{} must be {}", f.label, whole);
}

std::string pair_message(const NumPair &p)
{
    return xpp::format("{} must be {} {}", num_fields[p.lo].label, p.strict ? "below" : "at most", num_fields[p.hi].label);
}

/* Auto.plot's values: hi, norm, hi and lo, period, two parameters, frequency, average */
bool plot_ok(int p) { return (p >= 0 && p <= 4) || p == 10 || p == 11; }

/* ---- the settings now ---- */

void read_num(const xpp::Session &s, double v[AUTO_NUM_N])
{
    const double now[AUTO_NUM_N] = {
        static_cast<double>(s.auto_state.bifur.ntst), static_cast<double>(s.auto_state.bifur.nmx), static_cast<double>(s.auto_state.bifur.npr),
        static_cast<double>(s.auto_state.bifur.ncol), s.auto_state.bifur.ds, s.auto_state.bifur.dsmin, s.auto_state.bifur.dsmax, s.auto_state.bifur.rl0, s.auto_state.bifur.rl1, s.auto_state.bifur.a0, s.auto_state.bifur.a1,
        s.auto_state.bifur.epsl, s.auto_state.bifur.epsu, s.auto_state.bifur.epss, static_cast<double>(s.auto_state.advanced.iad), static_cast<double>(s.auto_state.advanced.mxbf),
        static_cast<double>(s.auto_state.advanced.iid), static_cast<double>(s.auto_state.advanced.itmx), static_cast<double>(s.auto_state.advanced.itnw),
        static_cast<double>(s.auto_state.advanced.nwtn), static_cast<double>(s.auto_state.advanced.iads), static_cast<double>(s.auto_state.suppress_bp),
    };
    std::memcpy(v, now, sizeof now);
}

void write_num(xpp::Session &s, const double v[AUTO_NUM_N])
{
    auto i = [&](int k) { return static_cast<int>(v[k]); };
    s.auto_state.bifur.ntst = i(AUTO_NUM_NTST);
    s.auto_state.bifur.nmx = i(AUTO_NUM_NMX);
    s.auto_state.bifur.npr = i(AUTO_NUM_NPR);
    s.auto_state.bifur.ncol = i(AUTO_NUM_NCOL);
    s.auto_state.bifur.ds = v[AUTO_NUM_DS];
    s.auto_state.bifur.dsmin = v[AUTO_NUM_DSMIN];
    s.auto_state.bifur.dsmax = v[AUTO_NUM_DSMAX];
    s.auto_state.bifur.rl0 = v[AUTO_NUM_RL0];
    s.auto_state.bifur.rl1 = v[AUTO_NUM_RL1];
    s.auto_state.bifur.a0 = v[AUTO_NUM_A0];
    s.auto_state.bifur.a1 = v[AUTO_NUM_A1];
    s.auto_state.bifur.epsl = v[AUTO_NUM_EPSL];
    s.auto_state.bifur.epsu = v[AUTO_NUM_EPSU];
    s.auto_state.bifur.epss = v[AUTO_NUM_EPSS];
    s.auto_state.advanced.iad = i(AUTO_NUM_IAD);
    s.auto_state.advanced.mxbf = i(AUTO_NUM_MXBF);
    s.auto_state.advanced.iid = i(AUTO_NUM_IID);
    s.auto_state.advanced.itmx = i(AUTO_NUM_ITMX);
    s.auto_state.advanced.itnw = i(AUTO_NUM_ITNW);
    s.auto_state.advanced.nwtn = i(AUTO_NUM_NWTN);
    s.auto_state.advanced.iads = i(AUTO_NUM_IADS);
    s.auto_state.suppress_bp = i(AUTO_NUM_SUPPBP);
}

/* ---- the event ---- */

/* a JSON string, or null for no name */
void add_str_or_null(std::string &o, const std::string &s)
{
    if (!s.empty()) xpp::json_append_string(o, s.c_str());
    else o += "null";
}

void add_num(std::string &o, double v) { o += std::isfinite(v) ? xpp::number(v) : std::string("null"); }

/* a name, or "" for none */
std::string name_or_empty(const char *s) { return s ? std::string(s) : std::string(); }

/* view axes a as the `set` command's axes keys */
void axes_of(const xpp::Session &s, const AUTOAX &a, AutoSettingsSet &set)
{
    set.has_plot = 1;
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
        add_num(o, set.num[i]);
    }
    o += xpp::format("}},\"axes\":{{\"plot\":{},\"var\":", set.plot);
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
    for (int i = 0; i < set.nmarks; i++) {
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
    o += "],\"threads\":";
    if (s.auto_state.threads == prefs::AUTO_THREADS_AUTO) xpp::json_append_string(o, std::string(prefs::AUTO_THREADS_WORD).c_str());
    else o += xpp::number(s.auto_state.threads);
    o += xpp::format(",\"threads_auto\":{},\"threads_max\":{}", AutoParallel::automatic_threads(), cpu::logical_processors());
    o += ",\"rules\":{";
    for (int i = 0; i < AUTO_NUM_N; i++) {
        const NumField &f = num_fields[i];
        o += xpp::format("{}\"{}\":{{\"label\":", i ? "," : "", f.key);
        xpp::json_append_string(o, f.label);
        if (f.integer) o += ",\"integer\":true";
        if (f.rule == Rule::positive) o += ",\"positive\":true";
        if (f.rule == Rule::nonzero) o += ",\"nonzero\":true";
        if (f.rule == Rule::range && f.lo != INT_MIN) o += xpp::format(",\"min\":{}", static_cast<long>(f.lo));
        if (f.rule == Rule::range && f.hi != INT_MAX) o += xpp::format(",\"max\":{}", static_cast<long>(f.hi));
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
    o += xpp::format("],\"step\":{{\"key\":\"{}\",\"lo\":\"{}\",\"hi\":\"{}\",\"message\":", num_fields[AUTO_NUM_DS].key,
                     num_fields[AUTO_NUM_DSMIN].key, num_fields[AUTO_NUM_DSMAX].key);
    xpp::json_append_string(o, step_rule);
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
    bool ok = std::isfinite(v) && !(f.integer && (v != std::floor(v) || v < INT_MIN || v > INT_MAX));
    if (ok && f.rule == Rule::positive) ok = v > 0;
    if (ok && f.rule == Rule::nonzero) ok = v != 0;
    if (ok && f.rule == Rule::range) ok = v >= f.lo && v <= f.hi;
    if (!ok) why = rule_message(f);
    return ok;
}

/* what a set changes, checked (check), for commit to write */
struct Checked {
    double num[AUTO_NUM_N];
    int view = 0, plot = 0, var = 0, icp1 = 0, icp2 = 0;
    double range[4];
    std::vector<UserMark> marks;
};

/* set checked against s, as it will be when views views of the diagram
   are there (-1: the views s has), into c: false, with why and the key
   of the value that is wrong ("ntst", "var", "mark2", ...; "" for
   the whole set) when one is */
bool check(const xpp::Session &s, const AutoSettingsSet *set, int views, Checked &c, std::string &why, std::string &key)
{
    key.clear();
    if (!have_settings(s)) {
        why = "AUTO needs a model with variables";
        return false;
    }
    /* Numerics: each value given, then the pairs that must be in order */
    double *num = c.num;
    read_num(s, num);
    for (int i = 0; i < AUTO_NUM_N; i++) {
        if (!set->has_num[i]) continue;
        if (!num_ok(i, set->num[i], why)) {
            key = num_fields[i].key;
            return false;
        }
        num[i] = set->num[i];
    }
    for (const NumPair &p : num_pairs) {
        if (!set->has_num[p.lo] && !set->has_num[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        why = pair_message(p);
        key = num_fields[p.hi].key;
        return false;
    }
    if ((set->has_num[AUTO_NUM_DS] || set->has_num[AUTO_NUM_DSMIN] || set->has_num[AUTO_NUM_DSMAX])
        && !(std::fabs(num[AUTO_NUM_DS]) >= num[AUTO_NUM_DSMIN] && std::fabs(num[AUTO_NUM_DS]) <= num[AUTO_NUM_DSMAX])) {
        why = step_rule;
        key = num_fields[AUTO_NUM_DS].key;
        return false;
    }

    /* the Axes: plot type, the variable, the two parameters among AUTO's, the ranges */
    /* of the view named (W50), else the active one: setting a view's axes makes it the active one */
    const int view = set->view < 0 ? s.auto_state.active_view : set->view;
    c.view = view;
    if (view >= (views < 0 ? static_cast<int>(s.auto_state.views.size()) : views)) {
        why = xpp::format("there is no view {}", view);
        key = "view";
        return false;
    }
    /* a view not there yet (views) starts from the active one's axes */
    const AUTOAX &now = static_cast<std::size_t>(view) < s.auto_state.views.size() ? s.auto_state.views[static_cast<std::size_t>(view)].axes
                                                                                  : s.auto_state.axes();
    int &plot = c.plot, &var = c.var, &icp1 = c.icp1, &icp2 = c.icp2;
    plot = now.plot;
    var = now.var;
    icp1 = now.icp1;
    icp2 = now.icp2;
    double (&range)[4] = c.range;
    range[0] = now.xmin;
    range[1] = now.xmax;
    range[2] = now.ymin;
    range[3] = now.ymax;
    if (set->has_plot) {
        if (!plot_ok(set->plot)) {
            why = xpp::format("{} is not one of AUTO's plot types (0-4, 10, 11)", set->plot);
            key = "plot";
            return false;
        }
        plot = set->plot;
    }
    if (!set->var.empty()) {
        int col;
        find_variable(s,set->var.c_str(), &col);
        if (col < 1 || col > s.model().node) {
            why = xpp::format("{} is not a variable AUTO computes", set->var);
            key = "var";
            return false;
        }
        var = col - 1;
    }
    const struct { const char *name; int *icp; const char *key; } axis_pars[] = {{set->par1.c_str(), &icp1, "par1"}, {set->par2.c_str(), &icp2, "par2"}};
    for (const auto &a : axis_pars) {
        if (!a.name[0]) continue;
        int k = xpp::find_user_name(s.model(), PARAM_BOX, a.name);
        if (k < 0) {
            why = xpp::format("{} is not a model parameter", a.name);
            key = a.key;
            return false;
        }
        *a.icp = k;
    }
    static const char *const range_names[4] = {"Xmin", "Xmax", "Ymin", "Ymax"};
    static const char *const range_keys[4] = {"xmin", "xmax", "ymin", "ymax"};
    for (int i = 0; i < 4; i++) {
        if (!set->has_range[i]) continue;
        if (!std::isfinite(set->range[i])) {
            why = xpp::format("{} must be a number", range_names[i]);
            key = range_keys[i];
            return false;
        }
        range[i] = set->range[i];
    }
    for (int i = 0; i < 4; i += 2) {
        if ((set->has_range[i] || set->has_range[i + 1]) && !(range[i] < range[i + 1])) {
            why = xpp::format("{} must be below {}", range_names[i], range_names[i + 1]);
            key = range_keys[i + 1];
            return false;
        }
    }

    /* Mark values: parameter (or T, the period) = value */
    if (set->nmarks < -1 || (set->nmarks >= 0 &&
        (set->mark_name.size() != static_cast<std::size_t>(set->nmarks) ||
         set->mark_value.size() != static_cast<std::size_t>(set->nmarks)))) {
        why = "Mark values: count does not match the list";
        key = "mark";
        return false;
    }
    for (int i = 0; i < set->nmarks; i++) {
        const bool period = strcasecmp(set->mark_name[i].c_str(), "T") == 0;
        const int parameter = period ? xpp::auto_unknown::period : xpp::find_user_name(s.model(), PARAM_BOX, set->mark_name[i]);
        if (parameter < 0) {
            why = xpp::format("Mark values: {} is not a model parameter or T", set->mark_name[i]);
            key = xpp::format("mark{}", i);
            return false;
        }
        if (!std::isfinite(set->mark_value[i])) {
            why = xpp::format("Mark values: the value of {} must be a number", set->mark_name[i]);
            key = xpp::format("mark{}", i);
            return false;
        }
        c.marks.push_back({parameter, period, set->mark_value[i]});
    }

    return true;
}

/* set, checked into c, written as the forms do */
void commit(xpp::Session &s, const AutoSettingsSet *set, Checked &c)
{
    const double *num = c.num;
    const int view = c.view, plot = c.plot, var = c.var, icp1 = c.icp1, icp2 = c.icp2;
    const double(&range)[4] = c.range;
    write_num(s, num);
    bool axes = set->has_plot || !set->var.empty() || !set->par1.empty() || !set->par2.empty() || set->fit;
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
        if (s.auto_state.axes().plot < 4) keep_last_plot(s, 1);
        if (s.auto_state.axes().plot == 4) keep_last_plot(s, 2);
        if (set->fit) auto_fit(s);
    }
    if (set->nmarks >= 0) s.auto_state.bifur.marks.swap(c.marks);
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if (axes && s.auto_state.bifur.exist) redraw_diagram(s);
}

bool apply(xpp::Session &s, const AutoSettingsSet *set, std::string &why)
{
    Checked c;
    std::string key;
    if (!check(s, set, -1, c, why, key)) return false;
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

const char *auto_settings_num_label(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].label : nullptr; }

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
    for (int i = 0; i < kept.nmarks; i++) {
        if (kept.mark_name[i] != "T" && xpp::find_user_name(s.model(), PARAM_BOX, kept.mark_name[i]) < 0) continue;
        set.mark_name[set.nmarks] = kept.mark_name[i];
        set.mark_value[set.nmarks++] = kept.mark_value[i];
    }
    set.mark_name.resize(set.nmarks);
    set.mark_value.resize(set.nmarks);
    /* The form can hold values that a run refuses (and a Fit can leave a
       flat range). Check the names through the normal settings owner,
       then commit the numbers and ranges already held in memory. */
    AutoSettingsSet names = set;
    names.has_num.fill(0);
    names.has_range.fill(0);
    Checked c;
    std::string why, key;
    if (!check(s, &names, -1, c, why, key)) {
        command_error("reload AUTO settings", why);
        return;
    }
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

bool auto_settings_check(const xpp::Session &s, const AutoSettingsSet &set, int views, std::string &why, std::string &key)
{
    Checked c;
    return check(s, &set, views, c, why, key);
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
