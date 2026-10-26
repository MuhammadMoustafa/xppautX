/* The JSON Schema of a .odex's options block (model_options.h): built from
   the option table, the one source of every key, its group, its kind of
   value and what it accepts. docs/odex-options.schema.json holds the
   output; tools/optionschema.sh checks it is current. */
#include "model_options.h"
#include "auto_settings.h"

#include "xpp_io.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

namespace {

/* a bare value: the characters a number, a name or a font name is made of */
constexpr std::string_view bare_pattern = "^[A-Za-z0-9_.+-]+$";

std::string quoted(std::string_view text)
{
  std::string out;
  json_append_string(out, text);
  return out;
}

bool all_digits(const std::string &w)
{
  return !w.empty() && std::ranges::all_of(w, [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });
}

} // namespace

std::string option_value_schema(const OptionRow &row)
{
  std::string out = "{";
  switch (row.kind) {
  case OptionKind::number: out += "\"type\": \"number\""; break;
  case OptionKind::whole: out += "\"type\": \"integer\""; break;
  case OptionKind::name: out += "\"type\": \"string\", \"pattern\": " + quoted(bare_pattern); break;
  case OptionKind::path:
    out += "\"type\": \"string\", \"x-kind\": \"path\"";
    break;
  case OptionKind::sliders:
    out += "\"type\": \"array\", \"items\": {\"type\": \"object\", \"additionalProperties\": false, "
           "\"required\": [\"name\", \"low\", \"high\"], \"properties\": {"
           "\"name\": {\"type\": \"string\"}, \"low\": {\"type\": \"number\"}, \"high\": {\"type\": \"number\"}, "
           "\"step\": {\"type\": \"number\", \"minimum\": 0}}}";
    break;
  case OptionKind::text: out += "\"type\": \"string\""; break;
  case OptionKind::color: out += "\"type\": \"string\", \"x-kind\": \"color\""; break;
  case OptionKind::choice: {
    if (row.boolean) {
      out += "\"type\": \"boolean\"";
      break;
    }
    const std::vector<std::string> words = option_choices(row);
    const bool numbers = std::ranges::all_of(words, all_digits);
    out += numbers ? "\"type\": \"integer\", \"enum\": [" : "\"type\": \"string\", \"enum\": [";
    for (std::size_t i = 0; i < words.size(); ++i) out += (i ? ", " : "") + (numbers ? words[i] : quoted(words[i]));
    out += "]";
    break;
  }
  }
  if (row.zero_or_one) out += ", \"enum\": [0, 1]";
  switch (row.rule) {
  case OptionRule::positive: out += ", \"exclusiveMinimum\": 0"; break;
  case OptionRule::nonnegative: out += ", \"minimum\": 0"; break;
  case OptionRule::whole_positive: out += ", \"minimum\": 1"; break;
  case OptionRule::nonzero:
  case OptionRule::any:
  case OptionRule::method:
  case OptionRule::choice: break;
  }
  if (row.maximum) out += ", \"maximum\": " + std::to_string(row.maximum);
  if (row.auto_numeric >= 0) out += auto_settings_num_schema(row.auto_numeric);
  if (!row.label.empty()) out += ", \"description\": " + quoted(row.label);
  if (row.repeatable) out += ", \"x-repeatable\": true";
  return out + "}";
}

std::string option_schema()
{
  std::string out =
      "{\n"
      "  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
      "  \"title\": \"xppautX .odex options\",\n"
      "  \"description\": \"The options block of a .odex model: options { group: { key: value, ... }, ... }. "
      "Generated from core/model_options.cpp by xppautX --options-schema; the JSON form of the block's "
      "dictionary. A key marked x-repeatable may be written more than once; a path or a text is a "
      "double-quoted string, sliders a list of presets, every other value one bare word.\",\n"
      "  \"type\": \"object\",\n"
      "  \"additionalProperties\": false,\n"
      "  \"properties\": {\n";
  for (std::size_t g = 0; g < option_groups.size(); ++g) {
    out += "    " + quoted(option_groups[g]) + ": {\n      \"type\": \"object\",\n      \"additionalProperties\": false,\n      \"properties\": {\n";
    bool first = true;
    for (const OptionRow &row : option_rows()) {
      if (row.group != option_groups[g]) continue;
      if (row.first_digit) {
        for (char d = row.first_digit; d <= row.last_digit; ++d) {
          out += std::string(first ? "" : ",\n") + "        " + quoted(odex_key(row, d - '0')) + ": " + option_value_schema(row);
          first = false;
        }
        continue;
      }
      out += std::string(first ? "" : ",\n") + "        " + quoted(odex_key(row, 0)) + ": " + option_value_schema(row);
      first = false;
    }
    out += "\n      }\n    }";
    out += g + 1 < option_groups.size() ? ",\n" : "\n";
  }
  return out + "  }\n}\n";
}

} // namespace xpp
