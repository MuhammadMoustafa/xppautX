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

#include "auto_settings.h"
#include "auto_nox.h"
#include "browse.h"
#include "diagram.h"
#include "parserslow.h"
#include "pop_list.h"
#include "xpp_io.h"
#include "xpp_util.h"

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

/* the reason into the C API's why, a buffer of n */
void say(char *why, size_t n, const char *s)
{
    if (n) xpp_strlcpy(why, s, n);
}

/* ---- the settings now ---- */

void read_num(double v[AUTO_NUM_N])
{
    const double now[AUTO_NUM_N] = {
        static_cast<double>(Auto.ntst), static_cast<double>(Auto.nmx), static_cast<double>(Auto.npr),
        static_cast<double>(Auto.ncol), Auto.ds, Auto.dsmin, Auto.dsmax, Auto.rl0, Auto.rl1, Auto.a0, Auto.a1,
        Auto.epsl, Auto.epsu, Auto.epss, static_cast<double>(aauto.iad), static_cast<double>(aauto.mxbf),
        static_cast<double>(aauto.iid), static_cast<double>(aauto.itmx), static_cast<double>(aauto.itnw),
        static_cast<double>(aauto.nwtn), static_cast<double>(aauto.iads), static_cast<double>(SuppressBP),
    };
    std::memcpy(v, now, sizeof now);
}

void write_num(const double v[AUTO_NUM_N])
{
    auto i = [&](int k) { return static_cast<int>(v[k]); };
    Auto.ntst = i(AUTO_NUM_NTST);
    Auto.nmx = i(AUTO_NUM_NMX);
    Auto.npr = i(AUTO_NUM_NPR);
    Auto.ncol = i(AUTO_NUM_NCOL);
    Auto.ds = v[AUTO_NUM_DS];
    Auto.dsmin = v[AUTO_NUM_DSMIN];
    Auto.dsmax = v[AUTO_NUM_DSMAX];
    Auto.rl0 = v[AUTO_NUM_RL0];
    Auto.rl1 = v[AUTO_NUM_RL1];
    Auto.a0 = v[AUTO_NUM_A0];
    Auto.a1 = v[AUTO_NUM_A1];
    Auto.epsl = v[AUTO_NUM_EPSL];
    Auto.epsu = v[AUTO_NUM_EPSU];
    Auto.epss = v[AUTO_NUM_EPSS];
    aauto.iad = i(AUTO_NUM_IAD);
    aauto.mxbf = i(AUTO_NUM_MXBF);
    aauto.iid = i(AUTO_NUM_IID);
    aauto.itmx = i(AUTO_NUM_ITMX);
    aauto.itnw = i(AUTO_NUM_ITNW);
    aauto.nwtn = i(AUTO_NUM_NWTN);
    aauto.iads = i(AUTO_NUM_IADS);
    SuppressBP = i(AUTO_NUM_SUPPBP);
}

/* AUTO's parameter index of a model parameter's name in pars, or -1 */
int auto_index_of(const int pars[8], const char *name)
{
    int p = find_user_name(PARAM_BOX, name);
    if (p < 0) return -1;
    for (int k = 0; k < NAutoPar; k++)
        if (pars[k] == p) return k;
    return -1;
}

/* ---- the event ---- */

/* a JSON string, or null for no name */
void add_str_or_null(std::string &o, const char *s)
{
    if (s) xpp::json_append_string(o, s);
    else o += "null";
}

void add_num(std::string &o, double v) { o += std::isfinite(v) ? xpp::number(v) : std::string("null"); }

std::string event_text()
{
    std::string o = "{\"ev\":\"autosettings\",\"numerics\":{";
    double v[AUTO_NUM_N];
    read_num(v);
    for (int i = 0; i < AUTO_NUM_N; i++) {
        o += xpp::format("{}\"{}\":", i ? "," : "", num_fields[i].key);
        add_num(o, v[i]);
    }
    o += "},\"pars\":[";
    for (int k = 0; k < NAutoPar; k++) {
        if (k) o += ',';
        add_str_or_null(o, auto_par_name(k));
    }
    o += xpp::format("],\"axes\":{{\"plot\":{},\"var\":", Auto.plot);
    add_str_or_null(o, Auto.var >= 0 && Auto.var < NODE ? uvar_names[Auto.var] : nullptr);
    o += ",\"par1\":";
    add_str_or_null(o, auto_par_name(Auto.icp1));
    o += ",\"par2\":";
    add_str_or_null(o, auto_par_name(Auto.icp2));
    const std::pair<const char *, double> range[] = {
        {"xmin", Auto.xmin}, {"xmax", Auto.xmax}, {"ymin", Auto.ymin}, {"ymax", Auto.ymax}};
    for (const auto &[name, value] : range) {
        o += xpp::format(",\"{}\":", name);
        add_num(o, value);
    }
    o += "},\"marks\":[";
    for (int i = 0; i < Auto.nper && i < AUTO_SETTINGS_MARKS; i++) {
        o += i ? ",[" : "[";
        add_str_or_null(o, Auto.uzrpar[i] == AUTO_PERIOD_INDEX ? "T" : auto_par_name(Auto.uzrpar[i]));
        o += ',';
        add_num(o, Auto.period[i]);
        o += ']';
    }
    o += "]}";
    return o;
}

AutoSettingsEmit emit_line;
bool subscribed;
std::string sent;
bool sent_valid;

/* the model has settings: init_auto_win() skipped a model too big for AUTO */
bool have_settings() { return NODE <= NAUTO; }


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
    std::memcpy(pars, AutoPar, sizeof pars);
    if (s->npars > NAutoPar) {
        why = xpp::format("AUTO has {} parameters for this model, not {}", NAutoPar, s->npars);
        return false;
    }
    for (int k = 0; k < s->npars; k++) {
        if (!s->pars[k][0]) continue;
        int p = find_user_name(PARAM_BOX, s->pars[k]);
        if (p < 0) {
            why = xpp::format("{} is not a parameter", s->pars[k]);
            return false;
        }
        pars[k] = p;
    }

    /* the Axes: plot type, the variable, the two parameters among AUTO's, the ranges */
    int plot = Auto.plot, var = Auto.var, icp1 = Auto.icp1, icp2 = Auto.icp2;
    double range[4] = {Auto.xmin, Auto.xmax, Auto.ymin, Auto.ymax};
    if (s->has_plot) {
        if (!plot_ok(s->plot)) {
            why = xpp::format("{} is not one of AUTO's plot types (0-4, 10, 11)", s->plot);
            return false;
        }
        plot = s->plot;
    }
    if (s->var[0]) {
        int col;
        find_variable(s->var, &col);
        if (col < 1 || col > NODE) {
            why = xpp::format("{} is not a variable AUTO computes", s->var);
            return false;
        }
        var = col - 1;
    }
    const struct { const char *name; int *icp; } axis_pars[] = {{s->par1, &icp1}, {s->par2, &icp2}};
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
        uzr[i] = strcasecmp(s->mark_name[i], "T") == 0 ? AUTO_PERIOD_INDEX : auto_index_of(pars, s->mark_name[i]);
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
    const bool new_pars = s->npars > 0 && std::memcmp(pars, AutoPar, sizeof pars) != 0;
    for (int k = 0; k < s->npars; k++) {
        if (!s->pars[k][0]) continue;
        AutoPar[k] = pars[k];
        Auto_index_to_array[k] = get_param_index(upar_names[pars[k]]);
    }
    bool axes = s->has_plot || s->var[0] || s->par1[0] || s->par2[0] || s->fit;
    for (int i = 0; i < 4; i++) axes = axes || s->has_range[i];
    if (axes) {
        Auto.plot = plot;
        Auto.var = var;
        Auto.icp1 = icp1;
        Auto.icp2 = icp2;
        Auto.xmin = range[0];
        Auto.xmax = range[1];
        Auto.ymin = range[2];
        Auto.ymax = range[3];
        if (Auto.plot < 4) keep_last_plot(1);
        if (Auto.plot == 4) keep_last_plot(2);
        if (s->fit) auto_fit();
    }
    if (s->nmarks >= 0) {
        Auto.nper = s->nmarks;
        for (int i = 0; i < s->nmarks; i++) {
            Auto.uzrpar[i] = uzr[i];
            Auto.period[i] = s->mark_value[i];
        }
        NAutoUzr = Auto.nper;
        for (int i = 0; i < AUTO_SETTINGS_MARKS; i++) {
            outperiod[i] = Auto.period[i];
            UzrPar[i] = Auto.uzrpar[i];
        }
    }
    /* the diagram in its new quantities (and a new parameter's name on its axis) */
    if ((axes || new_pars) && Auto.exist) redraw_diagram();
    return true;
}

} // namespace

extern "C" {

const char *auto_settings_num_key(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].key : nullptr; }

const char *auto_settings_num_label(int i) { return i >= 0 && i < AUTO_NUM_N ? num_fields[i].label : nullptr; }

int auto_settings_num_ok(int i, double v, char *why, size_t n)
{
    try {
        std::string w;
        if (num_ok(i, v, w)) return 1;
        say(why, n, w.c_str());
    } catch (...) {
        say(why, n, "out of memory");
    }
    return 0;
}

void auto_settings_set_init(AutoSettingsSet *s)
{
    std::memset(s, 0, sizeof *s);
    s->npars = -1;
    s->nmarks = -1;
}

int auto_settings_apply(const AutoSettingsSet *s, char *why, size_t n)
{
    try {
        std::string w;
        if (apply(s, w)) return 0;
        say(why, n, w.c_str());
    } catch (...) {
        /* only the messages allocate, all before anything is written */
        say(why, n, "out of memory");
    }
    return -1;
}

void auto_settings_init(AutoSettingsEmit emit) { emit_line = emit; }

void auto_settings_subscribe(int on)
{
    subscribed = on != 0;
    sent_valid = false;
    auto_settings_update();
}

void auto_settings_update(void)
{
    if (!emit_line || !subscribed || !have_settings()) return;
    try {
        std::string text = event_text();
        if (sent_valid && text == sent) return;
        emit_line(text.c_str(), text.size());
        sent = std::move(text);
        sent_valid = true;
    } catch (...) {
    }
}

} // extern "C"
