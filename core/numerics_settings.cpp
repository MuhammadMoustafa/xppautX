/* The main numerics as data: the "numerics" event, `set` kind `num`'s
   checks and writes and the Numerics menu's questions
   (numerics_settings.h, docs/protocol.md "The numerics as data"). The
   fields are the option table's numerics rows (model_options.h); each
   value is checked first, then applied as leaving the menu does
   (do_meth). */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <climits>
#include <optional>
#include <vector>
#include <ranges>
#include <string>
#include <string_view>

#include "numerics_settings.h"
#include "xpp_util.h"
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

/* whether method (and s's model) uses setting f */
bool uses(const xpp::Session &s, const OptionRow &f, int method)
{
    const xpp::SolverTraits &t = xpp::solver_info(method).traits;
    switch (f.use) {
    case OptionUse::always: return true;
    case OptionUse::relative_tolerance: return t.relative_tolerance;
    case OptionUse::absolute_tolerance: return t.absolute_tolerance;
    case OptionUse::min_step: return t.min_step;
    case OptionUse::max_step: return t.max_step;
    case OptionUse::newton: return t.newton;
    case OptionUse::delays: return s.model().ndelays > 0;
    case OptionUse::integral_history: return t.integral_history;
    }
    return true;
}

/* whether the session's method (and model) uses field f */
bool used(const xpp::Session &s, const OptionRow &f) { return uses(s, f, s.numerics.method); }

/* whether field f is one of s's model's: the delay of a model with no delays is not */
bool used_or_hidden(const xpp::Session &s, const OptionRow &f) { return f.use != OptionUse::delays || used(s, f); }

double value_of(const xpp::Session &s, const OptionRow &f)
{
    return f.real ? f.real(s) : f.whole(s);
}

/* the number of choice row f's word now (a boolean's false is 0, true 1) */
int index_of(const xpp::Session &s, const OptionRow &f)
{
    if (f.boolean) return f.boolean(s) ? 1 : 0;
    return f.index ? f.index.read(s) : f.whole(s);
}

/* the checked word number i put in choice row f's member */
void write_index(xpp::Session &s, const OptionRow &f, int i)
{
    if (f.boolean) f.boolean(s) = i != 0;
    else if (f.index) f.index.write(s, i);
    else f.whole(s) = i;
}

/* the words of choice row f: "false or true" in a message */
std::string words_in_text(const OptionRow &f)
{
    const std::vector<std::string> words = option_choices(f);
    std::string text;
    for (std::size_t k = 0; k < words.size(); ++k) text += xpp::format("{}{}", k ? (k + 1 == words.size() ? " or " : ", ") : "", words[k]);
    return text;
}

std::string event_text(const xpp::Session &s)
{
    std::string o = "{\"ev\":\"numerics\",\"fields\":[";
    bool first = true;
    for (const OptionRow &f : fields()) {
        if (f.use == OptionUse::delays && !used(s, f)) continue; /* no delays: no field */
        o += xpp::format("{}{{\"key\":\"{}\",\"label\":", first ? "" : ",", f.key);
        first = false;
        xpp::json_append_string(o, f.label);
        o += ",\"value\":";
        if (f.boolean) {
            o += f.boolean(s) ? "true,\"boolean\":true" : "false,\"boolean\":true";
        } else if (f.rule == OptionRule::method || f.rule == OptionRule::choice) {
            const std::vector<std::string> words = option_choices(f);
            xpp::json_append_string(o, words[static_cast<std::size_t>(index_of(s, f))]);
        } else {
            const double v = value_of(s, f);
            o += std::isfinite(v) ? xpp::number(v) : std::string("null");
            if (f.whole) o += ",\"integer\":true";
        }
        if (!used(s, f)) o += ",\"unused\":true";
        if (f.rule == OptionRule::method) {
            o += ",\"choices\":[";
            bool c1 = true;
            std::string unavailable;
            for (const xpp::SolverInfo &m : xpp::solvers()) {
                if (!c1) o += ',';
                c1 = false;
                xpp::json_append_string(o, m.key);
                const auto res = xpp::check_method(s.model(), m.id, {});
                if (!res) {
                    unavailable += unavailable.empty() ? "{" : ",";
                    xpp::json_append_string(unavailable, m.key);
                    unavailable += ':';
                    xpp::json_append_string(unavailable, res.error().what);
                }
            }
            o += ']';
            if (!unavailable.empty()) o += ",\"unavailable\":" + unavailable + "}";
        }
        if (f.rule == OptionRule::choice && !f.boolean) {
            o += ",\"choices\":[";
            std::string unavailable;
            for (std::size_t i = 0; i < f.choices.size(); ++i) {
                o += i ? "," : "";
                xpp::json_append_string(o, f.choices[i]);
                const std::string_view why = f.unavailable ? f.unavailable(s.model(), static_cast<int>(i)) : std::string_view();
                if (why.empty()) continue;
                unavailable += unavailable.empty() ? "{" : ",";
                xpp::json_append_string(unavailable, f.choices[i]);
                unavailable += ':';
                xpp::json_append_string(unavailable, why);
            }
            o += ']';
            if (!unavailable.empty()) o += ",\"unavailable\":" + unavailable + "}";
        }
        o += '}';
    }
    o += "]}";
    return o;
}

xpp::ChangedEvent<xpp::Session> event{event_text, "sending the numerics"};

/* the value of text for field f of a model m's numerics, checked; false
   with why */
bool check(xpp::Session &s, const OptionRow &f, std::string_view text, double &v, std::string &why, const xpp::Place &place)
{
    const xpp::Model &m_of = s.model();
    if (f.rule == OptionRule::method) {
        const auto picked = xpp::method_by_key(m_of, text, place);
        if (!picked) {
            why = xpp::format("{}: {}", f.label, picked.error().what);
            return false;
        }
        v = *picked;
        return true;
    }
    if (f.rule == OptionRule::choice) {
        const int i = choice_index(f.choices, text);
        if (i < 0) {
            why = xpp::format("{} must be {}", f.label, words_in_text(f));
            return false;
        }
        if (f.prepare) f.prepare(s, i);
        if (const std::string_view no = f.unavailable ? f.unavailable(s.model(), i) : std::string_view(); !no.empty()) {
            why = xpp::format("{}: {} is not available: {}", f.label, f.choices[static_cast<std::size_t>(i)], no);
            return false;
        }
        v = i;
        return true;
    }
    if (!xpp::parse_number(text, v) || !std::isfinite(v)) {
        why = xpp::format("{} must be a number", f.label);
        return false;
    }
    if (const char *no = rule_problem(f.rule, v, f.maximum)) {
        why = xpp::format("{} {}", f.label, no);
        return false;
    }
    return true;
}

/* the checked value v put in field f's member */
void write_field(xpp::Session &s, const OptionRow &f, double v)
{
    if (f.real) f.real(s) = v;
    else if (f.rule == OptionRule::method || f.rule == OptionRule::choice) write_index(s, f, static_cast<int>(v));
    else f.whole(s) = static_cast<int>(v);
}

} // namespace

int numerics_settings_check(xpp::Session &s, std::string_view key, std::string_view text, std::string &why)
{
    const OptionRow *f = numerics_option(key);
    double v = 0;
    if (!f || (f->use == OptionUse::delays && !used(s, *f))) {
        why = xpp::format("no numerics setting {}", key);
        return -1;
    }
    return check(s, *f, text, v, why, command_place()) ? 0 : -1;
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
        if (!check(s, *f, text, v, why, place ? *place : command_place())) return -1;
        write_field(s, *f, v);
        /* what changing these needs */
        if (f->applied) f->applied(s);
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

std::vector<const OptionRow *> saved_settings(std::string_view group)
{
    std::vector<const OptionRow *> rows;
    for (const OptionRow &r : option_rows())
        if (r.saved && (group == "numerics" ? r.group != option_group::sweep : r.group == option_group::sweep)) rows.push_back(&r);
    return rows;
}

const OptionRow *saved_setting(std::string_view key)
{
    for (const OptionRow &r : option_rows())
        if (r.saved && setting_key(r) == key) return &r;
    return nullptr;
}

std::string setting_key(const OptionRow &row)
{
    return row.key.empty() ? lower_case(std::string(row.name)) : std::string(row.key);
}

bool setting_applies(const xpp::Session &s, const OptionRow &row, int method)
{
    if (!row.key.empty()) return used_or_hidden(s, row);
    return uses(s, row, method);
}

std::string setting_text(const xpp::Session &s, const OptionRow &row)
{
    if (row.kind == OptionKind::choice) return option_choices(row)[static_cast<std::size_t>(index_of(s, row))];
    if (row.kind == OptionKind::name && row.whole) return xpp::ind_to_sym(s, row.whole(s));
    if (row.text) return row.text(s);
    if (row.color) return print_color(row.color(s));
    return xpp::number(value_of(s, row));
}

std::string named_settings(const xpp::Session &s, std::initializer_list<std::string_view> keys)
{
    std::string text;
    for (const std::string_view key : keys) {
        const OptionRow *row = saved_setting(key);
        assert(row != nullptr);
        text += xpp::format("{}{} = {}", text.empty() ? "" : ", ", key, setting_text(s, *row));
    }
    return text;
}

/* whether row is checked and written as an @ line is: it has no numerics key, or a model is loading (the exact
   Jacobian is checked once the model is compiled: the option's parser defers it, as the @ line does) */
static bool by_option(const OptionRow &row) { return row.key.empty() || (!row.name.empty() && Load::running()); }

std::optional<std::string> setting_problem(xpp::Session &s, const OptionRow &row, std::string_view text)
{
    if (by_option(row)) {
        /* the rule's words, with the setting's label before them, as the Numerics menu says them */
        double v = 0;
        if (!row.label.empty() && xpp::parse_number(text, v))
            if (const char *no = rule_problem(row.rule, v, row.maximum)) return xpp::format("{} {}", row.label, no);
        return option_problem(s, row.name, text, false);
    }
    double v = 0;
    std::string why;
    if (check(s, row, text, v, why, command_place())) return std::nullopt;
    return why;
}

void setting_write(xpp::Session &s, const OptionRow &row, std::string_view text)
{
    if (by_option(row)) return set_option(s, row.name, text, true, nullptr);
    double v = 0;
    std::string why;
    if (check(s, row, text, v, why, command_place())) write_field(s, row, v);
}

void numerics_settings_init(NumericsSettingsEmit emit) { event.init(emit); }

void numerics_settings_subscribe(const xpp::Session &s, int on) { event.subscribe(on != 0, s); }

void numerics_settings_update(const xpp::Session &s) { event.update(s); }

} // namespace xpp
