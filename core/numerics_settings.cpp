/* The main numerics as data: the "numerics" event and `set` kind `num`'s
   checks and writes (numerics_settings.h, docs/protocol.md "The numerics
   as data"). The same fields the Numerics menu's items write
   (numerics.cpp get_num_par), each value checked first, then applied as
   leaving the menu does (do_meth). */
#include <algorithm>
#include <cmath>
#include <climits>
#include <string>
#include <string_view>

#include "numerics_settings.h"
#include "data_event.h"
#include "model.h"
#include "numerics.h"
#include "pp_shoot.h"
#include "session.h"
#include "solver.h"
#include "xpp_io.h"
#include "xpp_mem.h"

namespace {

enum class Rule { any, nonzero, positive, nonnegative, whole_positive, method };

/* which methods use a field (xpp::SolverTraits), so a front end can say
   the others do not */
enum class Use { always, step_or_rel, step, rel, newton, delays };

struct Field {
    const char *key;
    const char *label;
    Rule rule;
    Use use;
    double NumericsSettings::*real; /* the member, one of the two */
    int NumericsSettings::*whole;
};

using N = NumericsSettings;

/* in the Numerics menu's order; total's sign says "forever", as the menu's */
constexpr Field fields[] = {
    {"total", "Total", Rule::any, Use::always, &N::tend, nullptr},
    {"t0", "Start time", Rule::any, Use::always, &N::t0, nullptr},
    {"trans", "Transient", Rule::any, Use::always, &N::trans, nullptr},
    {"dt", "Dt", Rule::nonzero, Use::always, &N::delta_t, nullptr},
    {"nmesh", "Ncline mesh", Rule::whole_positive, Use::always, nullptr, &N::nmesh},
    {"newt_iter", "Sing pt: maximum iterates", Rule::whole_positive, Use::always, nullptr, &N::evec_iter},
    {"newt_tol", "Sing pt: Newton tolerance", Rule::positive, Use::always, &N::evec_err, nullptr},
    {"jac_eps", "Sing pt: Jacobian epsilon", Rule::positive, Use::always, &N::newt_err, nullptr},
    {"nout", "nOutput", Rule::whole_positive, Use::always, nullptr, &N::njmp},
    {"bound", "Bounds", Rule::positive, Use::always, &N::bound, nullptr},
    {"method", "Method", Rule::method, Use::always, nullptr, &N::method},
    {"tol", "Tolerance", Rule::positive, Use::step_or_rel, &N::toler, nullptr},
    {"dtmin", "Minimum step", Rule::positive, Use::step, &N::hmin, nullptr},
    {"dtmax", "Maximum step", Rule::positive, Use::step, &N::hmax, nullptr},
    {"atol", "Abs. tolerance", Rule::positive, Use::rel, &N::atoler, nullptr},
    {"eul_tol", "Newton tolerance", Rule::positive, Use::newton, &N::eul_tol, nullptr},
    {"eul_iter", "Newton iterations", Rule::whole_positive, Use::newton, nullptr, &N::max_eul_iter},
    {"delay", "Maximal delay", Rule::nonnegative, Use::delays, &N::delay, nullptr},
    {"bvp_maxit", "BVP maximum iterates", Rule::whole_positive, Use::always, nullptr, &N::bvp_maxit},
    {"bvp_tol", "BVP tolerance", Rule::positive, Use::always, &N::bvp_tol, nullptr},
    {"bvp_eps", "BVP epsilon", Rule::positive, Use::always, &N::bvp_eps, nullptr},
};

/* whether the current method (and model) uses field f */
bool used(const Field &f)
{
    const xpp::SolverTraits &t = xpp::solver_info(xpp::session().numerics.method).traits;
    switch (f.use) {
    case Use::always: return true;
    case Use::step_or_rel: return t.step_tolerance || t.rel_abs_tolerance;
    case Use::step: return t.step_tolerance;
    case Use::rel: return t.rel_abs_tolerance;
    case Use::newton: return t.newton;
    case Use::delays: return xpp::model().ndelays > 0;
    }
    return true;
}

const Field *field_of(std::string_view key)
{
    const auto it = std::ranges::find_if(fields, [key](const Field &f) { return key == f.key; });
    return it == std::ranges::end(fields) ? nullptr : &*it;
}

double value_of(const Field &f)
{
    const NumericsSettings &n = xpp::session().numerics;
    if (f.real == &N::tend && n.forever) return -n.tend;
    return f.real ? n.*f.real : n.*f.whole;
}

std::string event_text()
{
    std::string o = "{\"ev\":\"numerics\",\"fields\":[";
    bool first = true;
    for (const Field &f : fields) {
        if (f.use == Use::delays && !used(f)) continue; /* no delays: no field */
        const double v = value_of(f);
        o += xpp::format("{}{{\"key\":\"{}\",\"label\":", first ? "" : ",", f.key);
        first = false;
        xpp::json_append_string(o, f.label);
        o += ",\"value\":";
        o += std::isfinite(v) ? xpp::number(v) : std::string("null");
        if (f.whole) o += ",\"integer\":true";
        if (!used(f)) o += ",\"unused\":true";
        if (f.rule == Rule::method) {
            o += ",\"choices\":[";
            bool c1 = true;
            for (const xpp::SolverInfo &m : xpp::solvers()) {
                if (!c1) o += ',';
                c1 = false;
                xpp::json_append_string(o, m.name);
            }
            o += ']';
        }
        o += '}';
    }
    o += "]}";
    return o;
}

xpp::ChangedEvent event{event_text, "sending the numerics"};

/* the method `text` names (a name of xpp::solvers(), any case, or its
   number); -1 for none */
int method_of(std::string_view text)
{
    int i;
    if (xpp::parse_int(text, i)) return i >= 0 && i < xpp::method::COUNT ? i : -1;
    for (const xpp::SolverInfo &m : xpp::solvers())
        if (xpp::equal_ignoring_case(text, m.name)) return m.id;
    return -1;
}

/* the value of text for field f, checked; false with why */
bool check(const Field &f, std::string_view text, double &v, std::string &why)
{
    if (f.rule == Rule::method) {
        const int m = method_of(text);
        if (m < 0) {
            why = xpp::format("{}: no method {}", f.label, text);
            return false;
        }
        if (const char *no = method_refusal(m)) {
            why = xpp::format("{}: {}", f.label, no);
            return false;
        }
        if (xpp::model().nkernel > 0 && m != xpp::method::VOLTERRA) {
            why = xpp::format("{}: a model with integral equations is integrated by Volterra", f.label);
            return false;
        }
        v = m;
        return true;
    }
    if (!xpp::parse_number(text, v) || !std::isfinite(v)) {
        why = xpp::format("{} must be a number", f.label);
        return false;
    }
    switch (f.rule) {
    case Rule::nonzero:
        if (v != 0) return true;
        why = xpp::format("{} must be a number other than 0", f.label);
        return false;
    case Rule::positive:
        if (v > 0) return true;
        why = xpp::format("{} must be a number above 0", f.label);
        return false;
    case Rule::nonnegative:
        if (v >= 0) return true;
        why = xpp::format("{} must be a number of at least 0", f.label);
        return false;
    case Rule::whole_positive:
        if (v == std::floor(v) && v >= 1 && v <= INT_MAX) return true;
        why = xpp::format("{} must be a whole number of at least 1", f.label);
        return false;
    case Rule::any:
    case Rule::method:
        break;
    }
    return true;
}

} // namespace

int numerics_settings_set(std::string_view key, std::string_view text, std::string &why)
{
    try {
        const Field *f = field_of(key);
        double v = 0;
        if (!f || (f->use == Use::delays && !used(*f))) {
            why = xpp::format("no numerics setting {}", key);
            return -1;
        }
        if (!check(*f, text, v, why)) return -1;
        NumericsSettings &n = xpp::session().numerics;
        if (f->real == &N::tend) { /* the menu's Total: below 0 for ever */
            n.forever = v < 0;
            n.tend = std::fabs(v);
        } else if (f->real) {
            n.*f->real = v;
        } else {
            n.*f->whole = static_cast<int>(v);
        }
        /* what the menu does after each of these */
        if (f->real == &N::delta_t) dt_changed();
        else if (f->real == &N::delay) chk_delay();
        else if (f->real == &N::bvp_tol || f->real == &N::bvp_eps || f->whole == &N::bvp_maxit) reset_bvp();
        do_meth();
        return 0;
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory("setting the numerics");
    }
}

void numerics_settings_init(NumericsSettingsEmit emit) { event.init(emit); }

void numerics_settings_subscribe(int on) { event.subscribe(on != 0); }

void numerics_settings_update(void) { event.update(); }
