/* The settings panels as data (options_panels.h, W616). */
#include "options_panels.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "json_number.h"
#include "model.h"
#include "model_features.h"
#include "model_options.h"
#include "numerics_settings.h"
#include "session.h"
#include "xpp_io.h"
#include "xpp_util.h"

namespace xpp {

namespace {

std::string_view kind_word(OptionKind kind)
{
    switch (kind) {
    case OptionKind::number: return "number";
    case OptionKind::whole: return "whole";
    case OptionKind::name: return "name";
    case OptionKind::choice: return "choice";
    case OptionKind::path: return "path";
    case OptionKind::text: return "text";
    case OptionKind::sliders: return "sliders";
    case OptionKind::curves: return "curves";
    case OptionKind::buttons: return "buttons";
    }
    return {};
}

/* the key's part before its last dot, "" when it has none */
std::string_view section_of(const OptionRow &row)
{
    const std::size_t dot = row.odex.rfind('.');
    return dot == std::string_view::npos ? std::string_view() : row.odex.substr(0, dot);
}

/* a key's words as the panel shows them: name_label, in the page's one spelling of color (W642); the keys keep `color` */
std::string words_of(std::string_view key)
{
    std::string label = name_label(key);
    for (std::size_t at = label.find("olor"); at != std::string::npos; at = label.find("olor", at + 4)) label.insert(at + 3, "u");
    return label;
}

/* the label inside the panel: the key's last part as words, derived like every choice's label (the section says the rest) */
std::string panel_label(const OptionRow &row)
{
    const std::string_view section = section_of(row);
    return words_of(section.empty() ? row.odex : row.odex.substr(section.size() + 1));
}

void add_string(std::string &out, std::string_view key, std::string_view text)
{
    out += xpp::format(",\"{}\":", key);
    json_append_string(out, text);
}

/* the member of a row's value as this module reads it (the Session's `reset` sets what no member's initial value is) */
std::string row_json(const Session &fresh, bool model_loaded, const OptionRow &row)
{
    std::string out = "{\"key\":";
    json_append_string(out, row.odex);
    add_string(out, "section", section_of(row));
    add_string(out, "section_label", words_of(section_of(row)));
    add_string(out, "label", panel_label(row));
    add_string(out, "kind", row.names_sets ? "sets" : kind_word(row.kind));
    add_string(out, "help", option_help(row));
    if (row.needs != Feature::none) add_string(out, "needs", feature_info(row.needs).name);
    add_string(out, "default", model_loaded ? setting_text(fresh, row) : std::string());
    if (row.boolean) out += ",\"boolean\":true";
    else if (row.kind == OptionKind::choice) {
        out += ",\"choices\":[";
        const std::vector<std::string> words = option_choices(row);
        for (std::size_t i = 0; i < words.size(); ++i) {
            if (i) out += ',';
            json_append_string(out, words[i]);
        }
        out += ']';
    }
    if (row.rule == OptionRule::nonzero) out += ",\"nonzero\":true";
    out += option_limit_members(row);
    return out + '}';
}

} // namespace

bool option_in_panel(const OptionRow &row)
{
    const bool has_member = row.real || row.whole || row.boolean || row.index || row.text || row.show;
    /* a list of names of the model's sets is a check list; the other lists (the torus's folds, the H function's coupling) have no field in the page yet */
    const bool list = row.repeatable && !row.names_sets;
    /* a path with no member of its own is chosen in another window (the animation's File), not typed in a panel */
    const bool chosen_elsewhere = row.kind == OptionKind::path && !row.text;
    return !row.odex.empty() && has_member && !list && !chosen_elsewhere && row.kind != OptionKind::sliders && row.kind != OptionKind::curves && row.kind != OptionKind::buttons;
}

std::string options_table_json(Session &s)
{
    Session fresh(s.model());
    if (s.model().loaded())
        for (const OptionRow &row : option_rows())
            if (row.reset) row.reset(fresh);
    std::string out = "[";
    for (const std::string_view group : option_groups) {
        std::string rows;
        for (const OptionRow &row : option_rows()) {
            if (row.group != group || !option_in_panel(row)) continue;
            rows += (rows.empty() ? "" : ",") + row_json(fresh, s.model().loaded(), row);
        }
        if (rows.empty()) continue; /* a group of lists only (interface) has no panel */
        out += out.size() > 1 ? "," : "";
        out += "{\"group\":";
        json_append_string(out, group);
        add_string(out, "label", name_label(group));
        out += ",\"rows\":[" + rows + "]}";
    }
    return out + ']';
}

} // namespace xpp
