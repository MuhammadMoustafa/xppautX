/* The main numerics as data: the "numerics" event, `set` kind `num`'s
   checks and writes and the Numerics menu's questions
   (numerics_settings.h, docs/protocol.md "The numerics as data"). The
   fields are the option table's numerics rows (model_options.h); each
   value is checked first, then applied as leaving the menu does
   (do_meth). */
#include <algorithm>
#include <cmath>
#include <climits>
#include <ranges>
#include <string>
#include <string_view>

#include "numerics_settings.h"
#include "data_event.h"
#include "model.h"
#include "model_options.h"
#include "numerics.h"
#include "pp_shoot.h"
#include "session.h"
#include "solver.h"
#include "xpp_io.h"
#include "xpp_mem.h"
#include "xpp_ui.h"

namespace xpp {

namespace {

/* the numerics settings: the option table's rows with a key, in the
   Numerics menu's order */
auto fields()
{
    return option_rows() | std::views::filter([](const OptionRow &r) { return !r.key.empty(); });
}

/* whether the session's method (and model) uses field f */
bool used(const xpp::Session &s, const OptionRow &f)
{
    const xpp::SolverTraits &t = xpp::solver_info(s.numerics.method).traits;
    switch (f.use) {
    case OptionUse::always: return true;
    case OptionUse::step_or_rel: return t.step_tolerance || t.rel_abs_tolerance;
    case OptionUse::step: return t.step_tolerance;
    case OptionUse::rel: return t.rel_abs_tolerance;
    case OptionUse::newton: return t.newton;
    case OptionUse::delays: return s.model().ndelays > 0;
    }
    return true;
}

/* total's sign says "forever", as the menu's */
bool is_total(const OptionRow &f) { return f.key == "total"; }

double value_of(const xpp::Session &s, const OptionRow &f)
{
    if (is_total(f) && s.numerics.forever) return -s.numerics.tend;
    /* the accessors name the member; reading it changes nothing */
    xpp::Session &member_of = const_cast<xpp::Session &>(s);
    return f.real ? f.real(member_of) : f.whole(member_of);
}

std::string event_text(const xpp::Session &s)
{
    std::string o = "{\"ev\":\"numerics\",\"fields\":[";
    bool first = true;
    for (const OptionRow &f : fields()) {
        if (f.use == OptionUse::delays && !used(s, f)) continue; /* no delays: no field */
        const double v = value_of(s, f);
        o += xpp::format("{}{{\"key\":\"{}\",\"label\":", first ? "" : ",", f.key);
        first = false;
        xpp::json_append_string(o, f.label);
        o += ",\"value\":";
        o += std::isfinite(v) ? xpp::number(v) : std::string("null");
        if (f.whole) o += ",\"integer\":true";
        if (!used(s, f)) o += ",\"unused\":true";
        if (f.rule == OptionRule::method) {
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

xpp::ChangedEvent<xpp::Session> event{event_text, "sending the numerics"};

/* the value of text for field f of a model m's numerics, checked; false
   with why */
bool check(const xpp::Model &m_of, const OptionRow &f, std::string_view text, double &v, std::string &why, const xpp::Place &place)
{
    if (f.rule == OptionRule::method) {
        int id = 0;
        const auto picked = xpp::parse_int(text, id) ? xpp::check_method(m_of, id, place)
                                                     : xpp::pick_method(m_of, text, place);
        if (!picked) {
            why = xpp::format("{}: {}", f.label, picked.error().what);
            return false;
        }
        v = *picked;
        return true;
    }
    if (!xpp::parse_number(text, v) || !std::isfinite(v)) {
        why = xpp::format("{} must be a number", f.label);
        return false;
    }
    if (const char *no = rule_problem(f.rule, v)) {
        why = xpp::format("{} {}", f.label, no);
        return false;
    }
    return true;
}

} // namespace

int numerics_settings_check(const xpp::Session &s, std::string_view key, std::string_view text, std::string &why)
{
    const OptionRow *f = numerics_option(key);
    double v = 0;
    if (!f || (f->use == OptionUse::delays && !used(s, *f))) {
        why = xpp::format("no numerics setting {}", key);
        return -1;
    }
    return check(s.model(), *f, text, v, why, command_place()) ? 0 : -1;
}

int numerics_settings_set(xpp::Session &s, std::string_view key, std::string_view text, std::string &why, const Place *place)
{
    try {
        const OptionRow *f = numerics_option(key);
        double v = 0;
        if (!f || (f->use == OptionUse::delays && !used(s, *f))) {
            why = xpp::format("no numerics setting {}", key);
            return -1;
        }
        if (!check(s.model(), *f, text, v, why, place ? *place : command_place())) return -1;
        NumericsSettings &n = s.numerics;
        if (is_total(*f)) { /* the menu's Total: below 0 for ever */
            n.forever = v < 0;
            n.tend = std::fabs(v);
        } else if (f->real) {
            f->real(s) = v;
        } else {
            f->whole(s) = static_cast<int>(v);
        }
        /* what changing these needs */
        if (key == "dt") dt_changed(s);
        else if (key == "delay") chk_delay(s);
        else if (key.starts_with("bvp_")) reset_bvp(s);
        do_meth(s);
        return 0;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("setting the numerics");
    }
}

bool numerics_settings_ask(xpp::Session &s, std::string_view key)
{
    const OptionRow *f = numerics_option(key);
    if (!f) return false;
    const std::string prompt = xpp::format("{} :", f->label);
    std::string text;
    if (f->whole) {
        text = xpp::format("{}", f->whole(s));
        if (new_string_of(prompt.c_str(), text, XPP_FIELD_INTEGER) == 0 || text.empty()) return false;
    } else {
        double v = value_of(s, *f);
        if (new_float(s, prompt.c_str(), &v) != 0) return false;
        text = xpp::number(v);
    }
    std::string why;
    if (numerics_settings_set(s, key, text, why) == 0) return true;
    command_error("numerics", why);
    return false;
}

void numerics_settings_init(NumericsSettingsEmit emit) { event.init(emit); }

void numerics_settings_subscribe(const xpp::Session &s, int on) { event.subscribe(on != 0, s); }

void numerics_settings_update(const xpp::Session &s) { event.update(s); }

} // namespace xpp
