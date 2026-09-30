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

/* Auto.plot's values: hi, norm, hi and lo, period, two parameters, frequency, average */
bool plot_ok(int p) { return (p >= 0 && p <= 4) || p == 10 || p == 11; }

/* ---- the settings now ---- */

void read_num(double v[AUTO_NUM_N])
{
    xpp::Session &s=xpp::session();
    const double now[AUTO_NUM_N] = {
        static_cast<double>(s.auto_state.bifur.ntst), static_cast<double>(s.auto_state.bifur.nmx), static_cast<double>(s.auto_state.bifur.npr),
        static_cast<double>(s.auto_state.bifur.ncol), s.auto_state.bifur.ds, s.auto_state.bifur.dsmin, s.auto_state.bifur.dsmax, s.auto_state.bifur.rl0, s.auto_state.bifur.rl1, s.auto_state.bifur.a0, s.auto_state.bifur.a1,
        s.auto_state.bifur.epsl, s.auto_state.bifur.epsu, s.auto_state.bifur.epss, static_cast<double>(s.auto_state.advanced.iad), static_cast<double>(s.auto_state.advanced.mxbf),
        static_cast<double>(s.auto_state.advanced.iid), static_cast<double>(s.auto_state.advanced.itmx), static_cast<double>(s.auto_state.advanced.itnw),
        static_cast<double>(s.auto_state.advanced.nwtn), static_cast<double>(s.auto_state.advanced.iads), static_cast<double>(s.auto_state.suppress_bp),
    };
    std::memcpy(v, now, sizeof now);
}

void write_num(const double v[AUTO_NUM_N])
{
    xpp::Session &s=xpp::session();
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
int auto_index_of(const int pars[8], const char *name)
{
    int p = find_user_name(PARAM_BOX, name);
    if (p < 0) return -1;
    for (int k = 0; k < xpp::session().auto_state.npar; k++)
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
void axes_of(const AUTOAX &a, AutoSettingsSet &set)
{
    set.has_plot = 1;
    set.plot = a.plot;
    if (a.var >= 0 && a.var < xpp::model().node) set.var = xpp::model().uvar_names[a.var];
    set.par1 = name_or_empty(auto_par_name(a.icp1));
    set.par2 = name_or_empty(auto_par_name(a.icp2));
    set.has_range.fill(1);
    set.range = {a.xmin, a.xmax, a.ymin, a.ymax};
}

AutoSettingsSet settings_now()
{
    xpp::Session &s=xpp::session();
    AutoSettingsSet set;
    read_num(set.num.data());
    set.has_num.fill(1);
    set.npars = s.auto_state.npar < AUTO_SETTINGS_PARS ? s.auto_state.npar : AUTO_SETTINGS_PARS;
    for (int k = 0; k < set.npars; k++) set.pars[k] = name_or_empty(auto_par_name(k));
    axes_of(s.auto_state.axes(), set);
    set.nmarks = s.auto_state.bifur.nper < 0 ? 0 : s.auto_state.bifur.nper < AUTO_SETTINGS_MARKS ? s.auto_state.bifur.nper : AUTO_SETTINGS_MARKS;
    for (int i = 0; i < set.nmarks; i++) {
        set.mark_name[i] = s.auto_state.bifur.uzrpar[i] == AUTO_PERIOD_INDEX ? std::string("T") : name_or_empty(auto_par_name(s.auto_state.bifur.uzrpar[i]));
        set.mark_value[i] = s.auto_state.bifur.period[i];
    }
    return set;
}

std::string event_text()
{
    const AutoSettingsSet set = settings_now();
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
    o += "]}";
    return o;
}

/* the model has settings: init_auto_win() skipped a model too big for AUTO */
bool have_settings() { return xpp::model().node <= NAUTO; }

/* the event, none for a model with no settings */
std::string event_if_any() { return have_settings() ? event_text() : std::string(); }

xpp::ChangedEvent event{event_if_any, "sending AUTO's settings"};

bool num_ok(int i, double v, std::string &why)
{
    if (i < 0 || i >= AUTO_NUM_N) {
        why = "no such AUTO setting";
        return false;
    }
    const NumField &f = num_fields[i];
    const std::string whole = f.integer ? "a whole number" : "a number";
    if (!std::isfinite(v) || (f.integer && (v != std::floor(v) || v < INT_MIN || v > INT_MAX))) {
        why = xpp::format("{} must be {}", f.label, whole);
        return false;
    }
    switch (f.rule) {
    case Rule::positive:
        if (v > 0) return true;
        why = xpp::format("{} must be a number above 0", f.label);
        return false;
    case Rule::nonzero:
        if (v != 0) return true;
        why = xpp::format("{} must be a number other than 0", f.label);
        return false;
    case Rule::range:
        if (v >= f.lo && v <= f.hi) return true;
        if (f.hi == INT_MAX)
            why = xpp::format("{} must be {} of at least {}", f.label, whole, static_cast<long>(f.lo));
        else
            why = xpp::format("{} must be {} from {} to {}", f.label, whole, static_cast<long>(f.lo),
                                    static_cast<long>(f.hi));
        return false;
    case Rule::any:
        break;
    }
    return true;
}

bool apply(const AutoSettingsSet *s, std::string &why)
{
    if (!have_settings()) {
        why = xpp::format("AUTO is restricted to less than {} variables", NAUTO);
        return false;
    }
    /* Numerics: each value given, then the pairs that must be in order */
    double num[AUTO_NUM_N];
    read_num(num);
    for (int i = 0; i < AUTO_NUM_N; i++) {
        if (!s->has_num[i]) continue;
        if (!num_ok(i, s->num[i], why)) return false;
        num[i] = s->num[i];
    }
    const struct { int lo, hi; bool strict; } pairs[] = {
        {AUTO_NUM_DSMIN, AUTO_NUM_DSMAX, false}, {AUTO_NUM_RL0, AUTO_NUM_RL1, true}, {AUTO_NUM_A0, AUTO_NUM_A1, true}};
    for (const auto &p : pairs) {
        if (!s->has_num[p.lo] && !s->has_num[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        why = xpp::format("{} must be {} {}", num_fields[p.lo].label, p.strict ? "below" : "at most",
                                num_fields[p.hi].label);
        return false;
    }
    /* the first step within the step sizes AUTO may take (T23) */
    if ((s->has_num[AUTO_NUM_DS] || s->has_num[AUTO_NUM_DSMIN] || s->has_num[AUTO_NUM_DSMAX])
        && !(std::fabs(num[AUTO_NUM_DS]) >= num[AUTO_NUM_DSMIN] && std::fabs(num[AUTO_NUM_DS]) <= num[AUTO_NUM_DSMAX])) {
        why = "Ds must be from Dsmin to Dsmax in size (its sign is the direction)";
        return false;
    }

    /* the Parameter form: AUTO's parameters by name */
    int pars[8];
    std::memcpy(pars, xpp::session().auto_state.par.data(), sizeof pars);
    if (s->npars > xpp::session().auto_state.npar) {
        why = xpp::format("AUTO has {} parameters for this model, not {}", xpp::session().auto_state.npar, s->npars);
        return false;
    }
    for (int k = 0; k < s->npars; k++) {
        if (s->pars[k].empty()) continue;
        int p = find_user_name(PARAM_BOX, s->pars[k].c_str());
        if (p < 0) {
            why = xpp::format("{} is not a parameter", s->pars[k]);
            return false;
        }
        pars[k] = p;
    }

    /* the Axes: plot type, the variable, the two parameters among AUTO's, the ranges */
    /* of the view named (W50), else the active one: setting a view's axes makes it the active one */
    const int view = s->view < 0 ? xpp::session().auto_state.active_view : s->view;
    if (view >= static_cast<int>(xpp::session().auto_state.views.size())) {
        why = xpp::format("there is no view {}", view);
        return false;
    }
    const AUTOAX &now = xpp::session().auto_state.views[static_cast<std::size_t>(view)].axes;
    int plot = now.plot, var = now.var, icp1 = now.icp1, icp2 = now.icp2;
    double range[4] = {now.xmin, now.xmax, now.ymin, now.ymax};
    if (s->has_plot) {
        if (!plot_ok(s->plot)) {
            why = xpp::format("{} is not one of AUTO's plot types (0-4, 10, 11)", s->plot);
            return false;
        }
        plot = s->plot;
    }
    if (!s->var.empty()) {
        int col;
        find_variable(s->var.c_str(), &col);
        if (col < 1 || col > xpp::model().node) {
            why = xpp::format("{} is not a variable AUTO computes", s->var);
            return false;
        }
        var = col - 1;
    }
    const struct { const char *name; int *icp; } axis_pars[] = {{s->par1.c_str(), &icp1}, {s->par2.c_str(), &icp2}};
    for (const auto &a : axis_pars) {
        if (!a.name[0]) continue;
        int k = auto_index_of(pars, a.name);
        if (k < 0) {
            why = xpp::format("{} is not one of AUTO's parameters (see Parameter)", a.name);
            return false;
        }
        *a.icp = k;
    }
    static const char *const range_names[4] = {"Xmin", "Xmax", "Ymin", "Ymax"};
    for (int i = 0; i < 4; i++) {
        if (!s->has_range[i]) continue;
        if (!std::isfinite(s->range[i])) {
            why = xpp::format("{} must be a number", range_names[i]);
            return false;
        }
        range[i] = s->range[i];
    }
    for (int i = 0; i < 4; i += 2) {
        if ((s->has_range[i] || s->has_range[i + 1]) && !(range[i] < range[i + 1])) {
            why = xpp::format("{} must be below {}", range_names[i], range_names[i + 1]);
            return false;
        }
    }

    /* Mark values: parameter (or T, the period) = value */
    int uzr[AUTO_SETTINGS_MARKS];
    if (s->nmarks > AUTO_SETTINGS_MARKS) {
        why = xpp::format("at most {} Mark values", AUTO_SETTINGS_MARKS);
        return false;
    }
    for (int i = 0; i < s->nmarks; i++) {
        uzr[i] = strcasecmp(s->mark_name[i].c_str(), "T") == 0 ? AUTO_PERIOD_INDEX : auto_index_of(pars, s->mark_name[i].c_str());
        if (uzr[i] < 0) {
            why = xpp::format("Mark values: {} is not one of AUTO's parameters (see Parameter) or T",
                                    s->mark_name[i]);
            return false;
        }
        if (!std::isfinite(s->mark_value[i])) {
            why = xpp::format("Mark values: the value of {} must be a number", s->mark_name[i]);
            return false;
        }
    }

    /* all good: write them, as the forms do */
    write_num(num);
    const bool new_pars = s->npars > 0 && std::memcmp(pars, xpp::session().auto_state.par.data(), sizeof pars) != 0;
    for (int k = 0; k < s->npars; k++) {
        if (s->pars[k].empty()) continue;
        xpp::session().auto_state.par[k] = pars[k];
        xpp::session().auto_state.par_index[k] = get_param_index(xpp::model().upar_names[pars[k]]);
    }
    bool axes = s->has_plot || !s->var.empty() || !s->par1.empty() || !s->par2.empty() || s->fit;
    for (int i = 0; i < 4; i++) axes = axes || s->has_range[i];
    if (axes) {
        xpp::session().auto_state.active_view = view;
        xpp::session().auto_state.axes().plot = plot;
        xpp::session().auto_state.axes().var = var;
        xpp::session().auto_state.axes().icp1 = icp1;
        xpp::session().auto_state.axes().icp2 = icp2;
        xpp::session().auto_state.axes().xmin = range[0];
        xpp::session().auto_state.axes().xmax = range[1];
        xpp::session().auto_state.axes().ymin = range[2];
        xpp::session().auto_state.axes().ymax = range[3];
        if (xpp::session().auto_state.axes().plot < 4) keep_last_plot(1);
        if (xpp::session().auto_state.axes().plot == 4) keep_last_plot(2);
        if (s->fit) auto_fit();
    }
    if (s->nmarks >= 0) {
        xpp::session().auto_state.bifur.nper = s->nmarks;
        for (int i = 0; i < s->nmarks; i++) {
            xpp::session().auto_state.bifur.uzrpar[i] = uzr[i];
            xpp::session().auto_state.bifur.period[i] = s->mark_value[i];
        }
        xpp::session().auto_state.nuzr = xpp::session().auto_state.bifur.nper;
        for (int i = 0; i < AUTO_SETTINGS_MARKS; i++) {
            xpp::session().auto_state.uzr_period[i] = xpp::session().auto_state.bifur.period[i];
            xpp::session().auto_state.uzr_par[i] = xpp::session().auto_state.bifur.uzrpar[i];
        }
    }
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if ((axes || new_pars) && xpp::session().auto_state.bifur.exist) redraw_diagram();
    return true;
}

} // namespace

extern "C" {

const char *auto_settings_num_key(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].key : nullptr; }

const char *auto_settings_num_label(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].label : nullptr; }

void auto_settings_init(AutoSettingsEmit emit) { event.init(emit); }

void auto_settings_subscribe(int on) { event.subscribe(on != 0); }

void auto_settings_update(void) { event.update(); }

} // extern "C"

AutoSettingsSet auto_settings_now()
{
    try {
        return settings_now();
    } catch (...) {
        xpp_out_of_memory("reading AUTO's settings");
    }
}

AutoSettingsSet auto_settings_view(int view)
{
    try {
        AutoSettingsSet set;
        axes_of(xpp::session().auto_state.views[static_cast<std::size_t>(view)].axes, set);
        set.view = view;
        return set;
    } catch (...) {
        xpp_out_of_memory("reading AUTO's settings");
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

int auto_settings_apply(const AutoSettingsSet &s, std::string &why)
{
    try {
        if (apply(&s, why)) return 0;
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
