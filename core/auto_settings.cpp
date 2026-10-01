/* AUTO's settings as data: the "autosettings" event and the `auto` `set`
   command's checks and writes (auto_settings.h, docs/protocol.md "AUTO's
   settings as data").

   The same fields the AUTO window's forms write (auto_nox.cpp auto_num_par,
   auto_params, auto_plot_par, auto_per_par), with every value checked
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
#include "data_event.h"
#include "browse.h"
#include "diagram.h"
#include "expr.h"
#include "xpp_util.h"
#include "form_ode.h"
#include "xpp_mem.h"

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
    {"ntst", "Ntst", true, Rule::range, 1, INT_MAX},
    {"nmx", "Nmax", true, Rule::range, 1, INT_MAX},
    {"npr", "NPr", true, Rule::range, 1, INT_MAX},
    {"ncol", "Ncol", true, Rule::range, 2, 7},
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
    {"iid", "IID", true, Rule::range, 0, 5},
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

/* AUTO's parameter index of a model parameter's name in pars, or -1 */
int auto_index_of(const xpp::Session &s, const int pars[8], const char *name)
{
    int p = xpp::find_user_name(s.model(),PARAM_BOX, name);
    if (p < 0) return -1;
    for (int k = 0; k < s.auto_state.npar; k++)
        if (pars[k] == p) return k;
    return -1;
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
    set.npars = s.auto_state.npar < AUTO_SETTINGS_PARS ? s.auto_state.npar : AUTO_SETTINGS_PARS;
    for (int k = 0; k < set.npars; k++) set.pars[k] = name_or_empty(auto_par_name(s, k));
    axes_of(s, s.auto_state.axes(), set);
    set.nmarks = s.auto_state.bifur.nper < 0 ? 0 : s.auto_state.bifur.nper < AUTO_SETTINGS_MARKS ? s.auto_state.bifur.nper : AUTO_SETTINGS_MARKS;
    for (int i = 0; i < set.nmarks; i++) {
        set.mark_name[i] = s.auto_state.bifur.uzrpar[i] == AUTO_PERIOD_INDEX ? std::string("T") : name_or_empty(auto_par_name(s, s.auto_state.bifur.uzrpar[i]));
        set.mark_value[i] = s.auto_state.bifur.period[i];
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
    o += "},\"pars\":[";
    for (int k = 0; k < set.npars; k++) {
        if (k) o += ',';
        add_str_or_null(o, set.pars[k]);
    }
    o += xpp::format("],\"axes\":{{\"plot\":{},\"var\":", set.plot);
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
    o += "],\"rules\":{";
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

/* the model has settings: init_auto_win() skipped a model too big for AUTO */
bool have_settings(const xpp::Session &s) { return s.model().node <= NAUTO; }

/* the event, none for a model with no settings */
std::string event_if_any(const xpp::Session &s) { return have_settings(s) ? event_text(s) : std::string(); }

xpp::ChangedEvent<xpp::Session> event{event_if_any, "sending AUTO's settings"};

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

bool apply(xpp::Session &s, const AutoSettingsSet *set, std::string &why)
{
    if (!have_settings(s)) {
        why = xpp::format("AUTO is restricted to less than {} variables", NAUTO);
        return false;
    }
    /* Numerics: each value given, then the pairs that must be in order */
    double num[AUTO_NUM_N];
    read_num(s, num);
    for (int i = 0; i < AUTO_NUM_N; i++) {
        if (!set->has_num[i]) continue;
        if (!num_ok(i, set->num[i], why)) return false;
        num[i] = set->num[i];
    }
    for (const NumPair &p : num_pairs) {
        if (!set->has_num[p.lo] && !set->has_num[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        why = pair_message(p);
        return false;
    }
    if ((set->has_num[AUTO_NUM_DS] || set->has_num[AUTO_NUM_DSMIN] || set->has_num[AUTO_NUM_DSMAX])
        && !(std::fabs(num[AUTO_NUM_DS]) >= num[AUTO_NUM_DSMIN] && std::fabs(num[AUTO_NUM_DS]) <= num[AUTO_NUM_DSMAX])) {
        why = step_rule;
        return false;
    }

    /* the Parameter form: AUTO's parameters by name */
    int pars[8];
    std::memcpy(pars, s.auto_state.par.data(), sizeof pars);
    if (set->npars > s.auto_state.npar) {
        why = xpp::format("AUTO has {} parameters for this model, not {}", s.auto_state.npar, set->npars);
        return false;
    }
    for (int k = 0; k < set->npars; k++) {
        if (set->pars[k].empty()) continue;
        int p = xpp::find_user_name(s.model(),PARAM_BOX, set->pars[k].c_str());
        if (p < 0) {
            why = xpp::format("{} is not a parameter", set->pars[k]);
            return false;
        }
        pars[k] = p;
    }

    /* the Axes: plot type, the variable, the two parameters among AUTO's, the ranges */
    /* of the view named (W50), else the active one: setting a view's axes makes it the active one */
    const int view = set->view < 0 ? s.auto_state.active_view : set->view;
    if (view >= static_cast<int>(s.auto_state.views.size())) {
        why = xpp::format("there is no view {}", view);
        return false;
    }
    const AUTOAX &now = s.auto_state.views[static_cast<std::size_t>(view)].axes;
    int plot = now.plot, var = now.var, icp1 = now.icp1, icp2 = now.icp2;
    double range[4] = {now.xmin, now.xmax, now.ymin, now.ymax};
    if (set->has_plot) {
        if (!plot_ok(set->plot)) {
            why = xpp::format("{} is not one of AUTO's plot types (0-4, 10, 11)", set->plot);
            return false;
        }
        plot = set->plot;
    }
    if (!set->var.empty()) {
        int col;
        find_variable(s,set->var.c_str(), &col);
        if (col < 1 || col > s.model().node) {
            why = xpp::format("{} is not a variable AUTO computes", set->var);
            return false;
        }
        var = col - 1;
    }
    const struct { const char *name; int *icp; } axis_pars[] = {{set->par1.c_str(), &icp1}, {set->par2.c_str(), &icp2}};
    for (const auto &a : axis_pars) {
        if (!a.name[0]) continue;
        int k = auto_index_of(s, pars, a.name);
        if (k < 0) {
            why = xpp::format("{} is not one of AUTO's parameters (see Parameter)", a.name);
            return false;
        }
        *a.icp = k;
    }
    static const char *const range_names[4] = {"Xmin", "Xmax", "Ymin", "Ymax"};
    for (int i = 0; i < 4; i++) {
        if (!set->has_range[i]) continue;
        if (!std::isfinite(set->range[i])) {
            why = xpp::format("{} must be a number", range_names[i]);
            return false;
        }
        range[i] = set->range[i];
    }
    for (int i = 0; i < 4; i += 2) {
        if ((set->has_range[i] || set->has_range[i + 1]) && !(range[i] < range[i + 1])) {
            why = xpp::format("{} must be below {}", range_names[i], range_names[i + 1]);
            return false;
        }
    }

    /* Mark values: parameter (or T, the period) = value */
    int uzr[AUTO_SETTINGS_MARKS];
    if (set->nmarks > AUTO_SETTINGS_MARKS) {
        why = xpp::format("at most {} Mark values", AUTO_SETTINGS_MARKS);
        return false;
    }
    for (int i = 0; i < set->nmarks; i++) {
        uzr[i] = strcasecmp(set->mark_name[i].c_str(), "T") == 0 ? AUTO_PERIOD_INDEX : auto_index_of(s, pars, set->mark_name[i].c_str());
        if (uzr[i] < 0) {
            why = xpp::format("Mark values: {} is not one of AUTO's parameters (see Parameter) or T",
                                    set->mark_name[i]);
            return false;
        }
        if (!std::isfinite(set->mark_value[i])) {
            why = xpp::format("Mark values: the value of {} must be a number", set->mark_name[i]);
            return false;
        }
    }

    /* all good: write them, as the forms do */
    write_num(s, num);
    const bool new_pars = set->npars > 0 && std::memcmp(pars, s.auto_state.par.data(), sizeof pars) != 0;
    for (int k = 0; k < set->npars; k++) {
        if (set->pars[k].empty()) continue;
        s.auto_state.par[k] = pars[k];
        s.auto_state.par_index[k] = xpp::get_param_index(s,s.model().upar_names[pars[k]]);
    }
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
    if (set->nmarks >= 0) {
        s.auto_state.bifur.nper = set->nmarks;
        for (int i = 0; i < set->nmarks; i++) {
            s.auto_state.bifur.uzrpar[i] = uzr[i];
            s.auto_state.bifur.period[i] = set->mark_value[i];
        }
        s.auto_state.nuzr = s.auto_state.bifur.nper;
        for (int i = 0; i < AUTO_SETTINGS_MARKS; i++) {
            s.auto_state.uzr_period[i] = s.auto_state.bifur.period[i];
            s.auto_state.uzr_par[i] = s.auto_state.bifur.uzrpar[i];
        }
    }
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if ((axes || new_pars) && s.auto_state.bifur.exist) redraw_diagram(s);
    return true;
}

} // namespace

const char *auto_settings_num_key(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].key : nullptr; }

const char *auto_settings_num_label(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].label : nullptr; }

void auto_settings_init(AutoSettingsEmit emit) { event.init(emit); }

void auto_settings_subscribe(const xpp::Session &s, int on) { event.subscribe(on != 0, s); }

void auto_settings_update(const xpp::Session &s) { event.update(s); }

AutoSettingsSet auto_settings_now(const xpp::Session &s)
{
    try {
        return settings_now(s);
    } catch (...) {
        xpp::out_of_memory("reading AUTO's settings");
    }
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

int auto_settings_apply(xpp::Session &s, const AutoSettingsSet &set, std::string &why)
{
    try {
        if (apply(s, &set, why)) return 0;
    } catch (...) {
        /* only the messages allocate, all before anything is written */
        why.clear();
        try {
            why = "out of memory";
        } catch (...) {
        }
    }
    return -1;
}

} // namespace xpp
