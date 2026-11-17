#include "model_autox_numerics.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <string>

#include "model_options.h"
#include "xpp_text.h"
#include "xpp_ui.h"
#include "xpp_util.h"

namespace xpp {

namespace {

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

} // namespace

/* a value of the set that is wrong: what, and its key ("ntst", "var",
   "mark2", ...; "" for the whole set) as the Error's field; at the
   command's place (a file of options gives its own line instead) */
Error autox_refused(std::string what, std::string key)
{
    return Error{"autox set", std::move(what), command_place(), std::move(key)};
}

/* Numerics field i: its row's key and shape, and what autoX accepts for it */
AutoxNumField autox_num_field(int i)
{
    const OptionRow &row = numeric_row(i);
    const AutoxNumRule &rule = autox_num_rules[static_cast<std::size_t>(i)];
    return AutoxNumField{i, row.odex.data(), shape_of(row), rule.rule, rule.lo, rule.hi};
}

std::span<const char *const> autox_num_choices(int i)
{
    return i == AUTOX_NUM_OUTPUT_DETAIL ? std::span<const char *const>(autox_output_detail_names) : std::span<const char *const>();
}

/* what a field takes, as the refusal of a value it does not take says it
   (the event sends it, so the page says the same before it sends) */
std::string autox_num_rule_message(const AutoxNumField &f)
{
    const std::string label = name_label(f.key);
    if (f.shape == AutoxFieldShape::boolean) return xpp::format("{} must be true or false", label);
    if (f.shape == AutoxFieldShape::choice) {
        std::string words;
        for (const char *name : autox_num_choices(f.index)) words += xpp::format("{}{}", words.empty() ? "" : ", ", name);
        return xpp::format("{} must be one of {}", label, words);
    }
    const std::string whole = f.shape == AutoxFieldShape::whole ? "a whole number" : "a number";
    switch (f.rule) {
    case AutoxRule::positive:
        return xpp::format("{} must be a number above 0", label);
    case AutoxRule::nonzero:
        return xpp::format("{} must be a number other than 0", label);
    case AutoxRule::range:
        if (f.lo == INT_MIN && f.hi == INT_MAX) break;
        if (f.hi == INT_MAX) return xpp::format("{} must be {} of at least {}", label, whole, static_cast<long>(f.lo));
        return xpp::format("{} must be {} from {} to {}", label, whole, static_cast<long>(f.lo), static_cast<long>(f.hi));
    case AutoxRule::any:
        break;
    }
    return xpp::format("{} must be {}", label, whole);
}

bool autox_num_ok(int i, double v, std::string &why)
{
    try {
        if (i < 0 || i >= AUTOX_NUM_N) {
            why = "no such autoX option";
            return false;
        }
        const AutoxNumField f = autox_num_field(i);
        bool ok = std::isfinite(v) && !(autox_num_is_whole(f) && (v != std::floor(v) || v < INT_MIN || v > INT_MAX));
        if (ok && f.rule == AutoxRule::positive) ok = v > 0;
        if (ok && f.rule == AutoxRule::nonzero) ok = v != 0;
        if (ok && f.rule == AutoxRule::range) ok = v >= f.lo && v <= f.hi;
        if (!ok) why = xpp::format("{}={}: {}", f.key, xpp::number(v), autox_num_rule_message(f));
        return ok;
    } catch (...) {
        why.clear();
        try {
            why = "out of memory";
        } catch (...) {
        }
    }
    return false;
}


const char *autox_settings_num_key(int i) { return i >= 0 && i < AUTOX_NUM_N ? autox_num_field(i).key : nullptr; }

/* the first step within the step sizes autoX may take (T23), whichever its sign */
std::string step_rule()
{
    return xpp::format("The size of {} must be from {} to {} (its sign is the direction)", name_label(autox_num_field(AUTOX_NUM_STEP_SIZE).key),
                       name_label(autox_num_field(AUTOX_NUM_MIN_STEP_SIZE).key), name_label(autox_num_field(AUTOX_NUM_MAX_STEP_SIZE).key));
}

std::string pair_message(const NumPair &p)
{
    return xpp::format("{} must be {} {}", name_label(autox_num_field(p.lo).key), p.strict ? "below" : "at most", name_label(autox_num_field(p.hi).key));
}

Result<> autox_settings_numbers_check(std::span<const double, AUTOX_NUM_N> num,
                                    std::span<const int, AUTOX_NUM_N> supplied)
{
    for (int i = 0; i < AUTOX_NUM_N; ++i) {
        std::string why;
        if (supplied[i] && !autox_num_ok(i, num[i], why)) return std::unexpected(autox_refused(why, autox_num_field(i).key));
    }
    for (const NumPair &p : num_pairs) {
        if (!supplied[p.lo] && !supplied[p.hi]) continue;
        if (p.strict ? num[p.lo] < num[p.hi] : num[p.lo] <= num[p.hi]) continue;
        return std::unexpected(autox_refused(xpp::format("{} ({}={}, {}={})", pair_message(p), autox_num_field(p.lo).key, num[p.lo], autox_num_field(p.hi).key, num[p.hi]), autox_num_field(supplied[p.hi] ? p.hi : p.lo).key));
    }
    if ((supplied[AUTOX_NUM_STEP_SIZE] || supplied[AUTOX_NUM_MIN_STEP_SIZE] || supplied[AUTOX_NUM_MAX_STEP_SIZE])
        && !(std::fabs(num[AUTOX_NUM_STEP_SIZE]) >= num[AUTOX_NUM_MIN_STEP_SIZE] && std::fabs(num[AUTOX_NUM_STEP_SIZE]) <= num[AUTOX_NUM_MAX_STEP_SIZE])) {
        return std::unexpected(autox_refused(xpp::format("{} ({}={}, {}={}, {}={})", step_rule(), autox_num_field(AUTOX_NUM_STEP_SIZE).key, num[AUTOX_NUM_STEP_SIZE], autox_num_field(AUTOX_NUM_MIN_STEP_SIZE).key, num[AUTOX_NUM_MIN_STEP_SIZE], autox_num_field(AUTOX_NUM_MAX_STEP_SIZE).key, num[AUTOX_NUM_MAX_STEP_SIZE]), autox_num_field(supplied[AUTOX_NUM_STEP_SIZE] ? AUTOX_NUM_STEP_SIZE : supplied[AUTOX_NUM_MIN_STEP_SIZE] ? AUTOX_NUM_MIN_STEP_SIZE : AUTOX_NUM_MAX_STEP_SIZE).key));
    }
    return {};
}

} // namespace xpp
