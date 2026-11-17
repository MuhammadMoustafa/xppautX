/* The main numerics as data: the "numerics" event, `set` kind `num`'s
   checks and writes and the Numerics menu's questions
   (numerics_options.h, docs/protocol.md "The numerics as data"). The
   fields are the option table's numerics rows (model_options.h); each
   value is checked first, then applied as leaving the menu does
   (do_meth). */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <vector>
#include <ranges>
#include <string>
#include <string_view>
#include <functional>
#include <memory>
#include <new>

#include "event_registry.h"
#include "model_method.h"
#include "model_failed.h"
#include "numerics_options.h"
#include "model_numerics.h"
#include "xpp_util.h"
#include "session_values.h"
#include "data_event.h"
#include "model_place.h"
#include "model_features.h"
#include "model_options.h"
#include "json_number.h"
#include "numerics.h"
#include "solver.h"
#include "xpp_text.h"
#include "xpp_mem.h"
#include "xpp_ui.h"
#include "session.h"
#include "xpp_error.h"

namespace xpp {

namespace {

/* the numerics options: the option table's rows with a key, in the
   Numerics menu's order */
auto fields()
{
    return option_rows() | std::views::filter([](const OptionRow &r) { return !r.key.empty(); });
}

/* whether method (and s's model) uses option f */
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
    case OptionUse::integral_history: return t.integral_history;
    }
    return true;
}

/* whether the session's method (and model) uses field f */
bool used(const xpp::Session &s, const OptionRow &f) { return uses(s, f, s.numerics->method); }

/* whether field f is one of s's model's: the delay of a model with no delays is not */
bool of_the_model(const xpp::Session &s, const OptionRow &f) { return xpp::has_feature(s.model(), s.tables, f.needs); }



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

/* field f's value as JSON: a number, a boolean or a choice's quoted word */
void append_value(std::string &o, const xpp::Session &s, const OptionRow &f)
{
    if (f.boolean) {
        o += f.boolean(s) ? "true" : "false";
    } else if (f.rule == OptionRule::method || f.rule == OptionRule::choice) {
        const std::vector<std::string> words = option_choices(f);
        xpp::json_append_string(o, words[static_cast<std::size_t>(row_index(s, f))]);
    } else {
        const double v = row_number(s, f);
        o += xpp::json::json_finite(v) ? xpp::number(v) : std::string("null");
    }
}

} // namespace

std::string unavailable_choices_json(const xpp::Session &s, const OptionRow &f)
{
    std::string unavailable;
    const auto add = [&](std::string_view word, std::string_view why) {
        unavailable += unavailable.empty() ? "{" : ",";
        xpp::json_append_string(unavailable, word);
        unavailable += ':';
        xpp::json_append_string(unavailable, why);
    };
    if (f.rule == OptionRule::method) {
        for (const xpp::SolverInfo &m : xpp::solvers())
            if (const auto res = xpp::check_method(s.model(), m.id, {}); !res) add(m.key, res.error().what);
    } else {
        for (std::size_t i = 0; i < f.choices.size(); ++i)
            if (const std::string_view why = f.unavailable ? f.unavailable(s.model(), static_cast<int>(i)) : std::string_view(); !why.empty())
                add(f.choices[i], why);
    }
    return unavailable.empty() ? unavailable : unavailable + '}';
}

namespace {

std::string event_text(const xpp::Session &s)
{
    std::string o = xpp::events::open(xpp::events::RUN_NUMERICS) + ",\"fields\":[";
    bool first = true;
    for (const OptionRow &f : fields()) {
        if (!of_the_model(s, f)) continue; /* a feature the model lacks: no field */
        o += xpp::format("{}{{\"key\":\"{}\",\"label\":", first ? "" : ",", f.key);
        first = false;
        xpp::json_append_string(o, f.label);
        o += ",\"value\":";
        append_value(o, s, f);
        if (f.boolean) o += ",\"boolean\":true";
        else if (f.rule != OptionRule::method && f.rule != OptionRule::choice && f.whole) o += ",\"integer\":true";
        if (!used(s, f)) o += ",\"unused\":true";
        if (f.rule == OptionRule::method || (f.rule == OptionRule::choice && !f.boolean)) {
            o += ",\"choices\":[";
            bool first_word = true;
            for (const std::string &word : option_choices(f)) {
                if (!first_word) o += ',';
                first_word = false;
                xpp::json_append_string(o, word);
            }
            o += ']';
            if (const std::string unavailable = unavailable_choices_json(s, f); !unavailable.empty()) o += ",\"unavailable\":" + unavailable;
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
    if (!f || !of_the_model(s, *f)) {
        why = xpp::format("no numerics option {}", key);
        return -1;
    }
    return check(s, *f, text, v, why, command_place()) ? 0 : -1;
}

namespace {
struct CheckedNumerics {
    const OptionRow *field;
    double value;
};

int check_values(Session &s, std::span<const NumericsValue> values, std::vector<CheckedNumerics> &checked, std::string &why, const Place *place)
{
    for (const NumericsValue &given : values) {
        const OptionRow *field = numerics_option(given.key);
        double value = 0;
        if (!field || !of_the_model(s, *field)) {
            why = xpp::format("no numerics option {}", given.key);
            return -1;
        }
        if (!check(s, *field, given.text, value, why, place ? *place : command_place())) return -1;
        checked.push_back({field, value});
    }
    const bool numbers = std::ranges::any_of(checked, [](const auto &c) { return c.field->rule != OptionRule::method && c.field->rule != OptionRule::choice; });
    if (const auto bad = numbers ? option_pairs_problem([&](const OptionRow &row) -> std::optional<double> {
            for (auto at = checked.rbegin(); at != checked.rend(); ++at)
                if (at->field == &row) return at->value;
            return row.real ? row.real(s) : row.whole(s);
        }) : std::nullopt) {
        why = bad->text;
        return -1;
    }
    return 0;
}
}

int numerics_settings_check_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place)
{
    try {
        std::vector<CheckedNumerics> checked;
        return check_values(s, values, checked, why, place);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("checking the numerics");
    }
}

int numerics_settings_set_all(Session &s, std::span<const NumericsValue> values, std::string &why, const Place *place)
{
    try {
        std::vector<CheckedNumerics> checked;
        if (check_values(s, values, checked, why, place) != 0) return -1;
        for (const auto &c : checked) write_field(s, *c.field, c.value);
        for (const auto &c : checked)
            apply_option_effect(s, c.field->effect);
        do_meth(s);
        return 0;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("setting the numerics");
    }
}

int numerics_settings_set(xpp::Session &s, std::string_view key, std::string_view text, std::string &why, const Place *place)
{
    const NumericsValue value{key, std::string(text)};
    return numerics_settings_set_all(s, std::span(&value, 1), why, place);
}

std::string numerics_settings_json_value(const xpp::Session &s, std::string_view key)
{
    std::string text;
    if (const OptionRow *f = numerics_option(key)) append_value(text, s, *f);
    return text;
}



bool setting_applies(const xpp::Session &s, const OptionRow &row, int method)
{
    /* the files hold every option of a model but those of what it has none of: the delays of one without delays, the parameter a range
       varies of one without parameters (the format's own rule: a session of a model with no boundary conditions still holds its bvp_ keys) */
    const bool held = (row.needs != xpp::Feature::delays && row.needs != xpp::Feature::parameters) || of_the_model(s, row);
    if (!row.key.empty()) return held;
    return held && uses(s, row, method);
}




/* whether row is checked and written as an @ line is: it has no numerics key, or a model is loading (the exact
   Jacobian is checked once the model is compiled: the option's parser defers it, as the @ line does) */
static bool by_option(const OptionRow &row) { return row.key.empty() || LoadSite::running(); }

namespace {

/* why text, one value of row, is not one */
std::optional<std::string> value_problem(xpp::Session &s, const OptionRow &row, std::string_view text)
{
    if (by_option(row)) {
        /* the rule's words, with the option's label before them, as the Numerics menu says them */
        double v = 0;
        if (!row.label.empty() && xpp::parse_number(text, v))
            if (const char *no = rule_problem(row.rule, v, row.maximum)) return xpp::format("{} {}", row.label, no);
        return option_problem(s, option_name(row), text, false);
    }
    double v = 0;
    std::string why;
    if (check(s, row, text, v, why, command_place())) return std::nullopt;
    return why;
}

} // namespace

std::optional<std::string> setting_problem(xpp::Session &s, const OptionRow &row, std::string_view text)
{
    if (!row.repeatable) return value_problem(s, row, text);
    for (const std::string &item : option_item_lines(row, text))
        if (const auto why = value_problem(s, row, item)) return why;
    return std::nullopt;
}

void setting_write(xpp::Session &s, const OptionRow &row, std::string_view text)
{
    if (row.repeatable) {
        assert(row.reset != nullptr);
        row.reset(s);
        for (const std::string &item : option_item_lines(row, text)) set_option(s, option_name(row), item, true, nullptr);
        return;
    }
    if (by_option(row)) return set_option(s, option_name(row), text, true, nullptr);
    double v = 0;
    std::string why;
    if (check(s, row, text, v, why, command_place())) write_field(s, row, v);
}

void numerics_settings_init(NumericsSettingsEmit emit) { event.init(emit); }

void numerics_settings_subscribe(const xpp::Session &s, int on) { event.subscribe(on != 0, s); }

void numerics_settings_update(const xpp::Session &s) { event.update(s); }

} // namespace xpp
