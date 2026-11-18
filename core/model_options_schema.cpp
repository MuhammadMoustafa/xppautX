/* The JSON Schema of a .odex's options block (model_options.h): built from
   the option table, the one source of every key, its group, its kind of
   value and what it accepts. docs/odex-options.schema.json holds the
   output; tools/optionschema.sh checks it is current. */
#include "model_options.h"
#include "model_autox_numerics.h"
#include "model_numerics.h"

#include "xpp_text.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <climits>
#include <optional>
#include <cstddef>
#include <span>

namespace xpp {

namespace {

/* a bare value: the characters a number, a name or a font name is made of */
constexpr std::string_view bare_pattern = "^[A-Za-z0-9_.+-]+$";

bool all_digits(const std::string &w)
{
  return !w.empty() && std::ranges::all_of(w, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

/* a choice whose words are all numbers is written as a JSON integer (its enum), else as strings */
bool words_are_numbers(const std::vector<std::string> &words)
{
  return std::ranges::all_of(words, all_digits);
}

/* a row's default as a JSON value: the value its reset gives a session with no model (fresh); nothing when the row has none
   (a list, a name) or its reset reads the model (model_default) */
std::optional<std::string> default_json(const SessionParts &fresh, const OptionRow &row)
{
  if (row.model_default) return std::nullopt;
  if (row.boolean) return std::string(row.boolean(fresh) ? "true" : "false");
  switch (row.kind) {
  case OptionKind::number:
  case OptionKind::whole:
    if (!row.real && !row.whole) return std::nullopt;
    return setting_text(fresh, row);
  case OptionKind::choice:
    if (!row.index && !row.whole) return std::nullopt;
    return words_are_numbers(option_choices(row)) ? setting_text(fresh, row) : json_quoted(setting_text(fresh, row));
  case OptionKind::text:
  case OptionKind::path:
    if (!row.text) return std::nullopt;
    return json_quoted(setting_text(fresh, row));
  case OptionKind::name:
  case OptionKind::sliders:
  case OptionKind::curves:
  case OptionKind::buttons: break;
  }
  return std::nullopt;
}

} // namespace

/* the bounds autoX accepts for Numerics field i (autox_num_rules), as JSON Schema keywords */
static std::string autox_num_schema(int i)
{
  const AutoxNumRule &f = autox_num_rules[static_cast<std::size_t>(i)];
  switch (f.rule) {
  case AutoxRule::positive: return ", \"exclusiveMinimum\": 0";
  case AutoxRule::range: {
    std::string out;
    if (f.lo != INT_MIN) out += ", \"minimum\": " + std::to_string(static_cast<long>(f.lo));
    if (f.hi != INT_MAX) out += ", \"maximum\": " + std::to_string(static_cast<long>(f.hi));
    return out;
  }
  case AutoxRule::nonzero:
  case AutoxRule::any: break;
  }
  return {};
}

std::string option_limit_members(const OptionRow &row)
{
  std::string out;
  switch (row.rule) {
  case OptionRule::positive: out += ", \"exclusiveMinimum\": 0"; break;
  case OptionRule::nonnegative: out += ", \"minimum\": 0"; break;
  case OptionRule::whole_positive: out += ", \"minimum\": 1"; break;
  case OptionRule::whole_at_least_two: out += ", \"minimum\": 2"; break;
  case OptionRule::nonzero:
  case OptionRule::any:
  case OptionRule::method:
  case OptionRule::choice: break;
  }
  if (row.maximum) out += ", \"maximum\": " + std::to_string(row.maximum);
  if (row.autox_numeric >= 0) out += autox_num_schema(row.autox_numeric);
  return out;
}

/* the method table as the schema lists it, in methods order (the set file's numbers): each method's name, set-file label, key and
   XPPAUT's one-letter @ key, which numerics.py reads for the set files it writes */
static std::string method_table_schema()
{
  std::string out = "[";
  for (std::size_t i = 0; i < methods.size(); ++i) {
    const MethodInfo &m = methods[i];
    out += std::string(i ? ", " : "") + "{\"name\": " + json_quoted(m.name) + ", \"set_label\": " + json_quoted(m.set_label) +
           ", \"key\": " + json_quoted(m.key) + ", \"xppaut\": " + json_quoted(xppaut_method_keys.substr(i, 1)) + "}";
  }
  return out + "]";
}

std::string option_value_schema(const OptionRow &row, const SessionParts *fresh)
{
  std::string out = "{";
  switch (row.kind) {
  case OptionKind::number: out += "\"type\": \"number\""; break;
  case OptionKind::whole: out += "\"type\": \"integer\""; break;
  case OptionKind::name: out += "\"type\": \"string\", \"pattern\": " + json_quoted(bare_pattern); break;
  case OptionKind::path:
    out += "\"type\": \"string\", \"x-kind\": \"path\"";
    break;
  case OptionKind::sliders:
    out += "\"type\": \"array\", \"items\": {\"type\": \"object\", \"additionalProperties\": false, "
           "\"required\": [\"name\", \"low\", \"high\"], \"properties\": {"
           "\"name\": {\"type\": \"string\"}, \"low\": {\"type\": \"number\"}, \"high\": {\"type\": \"number\"}, "
           "\"step\": {\"type\": \"number\", \"minimum\": 0}}}";
    break;
  case OptionKind::buttons:
    out += "\"type\": \"array\", \"items\": {\"type\": \"object\", \"additionalProperties\": false, "
           "\"required\": [\"label\", \"command\"], \"properties\": {"
           "\"label\": {\"type\": \"string\", \"minLength\": 1}, \"command\": {\"type\": \"string\", \"pattern\": " + json_quoted(bare_pattern) + "}}}";
    break;
  case OptionKind::curves:
    out += "\"type\": \"array\", \"minItems\": 1, \"items\": {\"type\": \"object\", \"additionalProperties\": false, "
           "\"properties\": {\"x\": {\"type\": \"string\"}, \"y\": {\"type\": \"string\"}, \"z\": {\"type\": \"string\"}}}";
    break;
  case OptionKind::text: out += "\"type\": \"string\""; break;
  case OptionKind::choice: {
    if (row.boolean) {
      out += "\"type\": \"boolean\"";
      break;
    }
    const std::vector<std::string> words = option_choices(row);
    const bool numbers = words_are_numbers(words);
    out += numbers ? "\"type\": \"integer\", \"enum\": [" : "\"type\": \"string\", \"enum\": [";
    for (std::size_t i = 0; i < words.size(); ++i) out += (i ? ", " : "") + (numbers ? words[i] : json_quoted(words[i]));
    out += "]";
    if (row.rule == OptionRule::method) out += ", \"x-methods\": " + method_table_schema();
    break;
  }
  }
  if (row.zero_or_one) out += ", \"enum\": [0, 1]";
  out += option_limit_members(row);
  if (!row.odex.empty()) out += ", \"title\": " + json_quoted(option_label(row)) + ", \"description\": " + json_quoted(option_help(row));
  if (row.repeatable) out += ", \"x-repeatable\": true";
  if (!row.key.empty()) out += ", \"x-key\": " + json_quoted(row.key);
  if (row.advanced) out += ", \"x-advanced\": true";
  if (fresh)
    if (const std::optional<std::string> value = default_json(*fresh, row)) out += ", \"default\": " + *value;
  return out + "}";
}

std::string option_schema(const SessionParts &fresh)
{
  /* the defaults the schema gives: each reset that reads no model (model_default), in the table's order, as set_option_defaults runs them
     (a repeatable row's reset empties its list, so it is not run) */
  for (const OptionRow &row : option_rows())
    if (row.reset && !row.model_default && !row.repeatable) row.reset(fresh);
  std::string out =
      "{\n"
      "  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
      "  \"title\": \"xppautX .odex options\",\n"
      "  \"description\": \"The options block of a .odex model: options { group: { key: value, ... }, ... }. "
      "Generated from core/model_options.cpp by xppautX --options-schema; the JSON form of the block's "
      "dictionary. A key marked x-repeatable may be written more than once; a path or a text is a "
      "double-quoted string, sliders a list of presets, curves a list of curves, every other value one bare word. "
      "default is the value a session with no model gives the key (a value a model sets, a name or a node count, has none); "
      "x-key is the key of the numerics event and of set num; x-advanced marks an option that tunes a method's inside (a tolerance, an iteration cap, a step floor), whose default is right for almost every model.\",\n"
      "  \"type\": \"object\",\n"
      "  \"additionalProperties\": false,\n"
      "  \"properties\": {\n";
  for (const OptionRow &row : option_rows())
    if (row.top_level) out += "    " + json_quoted(odex_key(row, 0)) + ": " + option_value_schema(row, &fresh) + ",\n";
  for (std::size_t g = 0; g < option_groups.size(); ++g) {
    out += "    " + json_quoted(option_groups[g]) + ": {\n      \"type\": \"object\",\n      \"additionalProperties\": false,\n      \"properties\": {\n";
    bool first = true;
    for (const OptionRow &row : option_rows()) {
      if (row.top_level || row.group != option_groups[g]) continue;
      if (row.first_digit) {
        for (char d = row.first_digit; d <= row.last_digit; ++d) {
          out += std::string(first ? "" : ",\n") + "        " + json_quoted(odex_key(row, d - '0')) + ": " + option_value_schema(row, &fresh);
          first = false;
        }
        continue;
      }
      out += std::string(first ? "" : ",\n") + "        " + json_quoted(odex_key(row, 0)) + ": " + option_value_schema(row, &fresh);
      first = false;
    }
    out += "\n      }\n    }";
    out += g + 1 < option_groups.size() ? ",\n" : "\n";
  }
  return out + "  }\n}\n";
}

} // namespace xpp
