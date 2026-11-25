/* An autoX script's file, name.autox: see autox_script.h. */
#include "autox_script.h"
#include "autox_codes.h"
#include "format_version.h"
#include "json_number.h"
#include "model_autox_numerics.h"
#include "odex_block.h"
#include "xpp_path.h"
#include "xpp_util.h"
#include "xpp_lines.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <expected>
#include <system_error>
#include <utility>

namespace xpp {

namespace {

constexpr std::string_view command_name = "autox.run";

/* what a run's own keys write: an option may not name them (it would be the same argument given twice, or a start a script cannot know) */
constexpr std::array<std::string_view, 13> run_arguments = {"kind", "switch", "label", "type", "index", "decimals", "hold",
                                                            "maximum", "replace_diagram", "unstable_dimension", "stable_dimension",
                                                            "left_equilibrium", "right_equilibrium"};

/* the value as JSON text: a number as the shortest text that reads back, true and false, a bare word (a choice) and a string
   as a string, a list as a list; a group is no value of an option */
void append_value(std::string &out, const odex::Value &v, const odex::Block &where)
{
    switch (v.kind) {
    case odex::Value::Kind::string: json_append_string(out, v.text); return;
    case odex::Value::Kind::word: {
        if (v.text == "true" || v.text == "false") {
            out += v.text;
            return;
        }
        double number = 0;
        if (!parse_number(v.text, number)) {
            json_append_string(out, v.text);
            return;
        }
        if (!std::isfinite(number)) where.fail(v, xpp::format("{} is not a finite number", v.text));
        json::json_append_number_shortest(out, number);
        return;
    }
    case odex::Value::Kind::list: {
        out += '[';
        for (std::size_t i = 0; i < v.items.size(); ++i) {
            if (i > 0) out += ',';
            append_value(out, v.items[i], where);
        }
        out += ']';
        return;
    }
    case odex::Value::Kind::group: where.fail(v, "a group is not a value here");
    }
}

void append_member(std::string &out, std::string_view key, const odex::Value &value, const odex::Block &where)
{
    out += ',';
    json_append_string(out, key);
    out += ':';
    append_value(out, value, where);
}

/* how many nearest option names a "no option named" error suggests (its hint) */
constexpr std::size_t option_hints = 3;

/* the Numerics' keys, the names a run's options may give */
std::vector<std::string> numerics_names()
{
    std::vector<std::string> names;
    for (int i = 0; i < AUTOX_NUM_N; ++i) names.emplace_back(autox_settings_num_key(i));
    return names;
}

/* an option of run `run` that names no Numerics field: the error at its own line and column, with the nearest names */
void check_option_name(const odex::Block &group, const odex::Member &m, std::string_view run, const std::vector<std::string> &names)
{
    if (std::ranges::find(names, m.key) != names.end()) return;
    std::string hint;
    for (const std::string &near : nearest_names(m.key, names, option_hints))
        hint += (hint.empty() ? " (the nearest: \"" : ", \"") + near + "\"";
    if (!hint.empty()) hint += ")";
    group.fail(m, xpp::format("in run \"{}\": no option named \"{}\"{}", run, m.key, hint));
}

/* the members of a group that has nothing but numbers or choices of autox.run's, each as an argument */
void append_group(std::string &out, std::optional<odex::Block> &group, std::set<std::string> &given, bool options, std::string_view run)
{
    if (!group) return;
    const std::vector<std::string> names = options ? numerics_names() : std::vector<std::string>{};
    for (const odex::Member *m : group->members()) {
        const bool own = std::ranges::find(run_arguments, m->key) != run_arguments.end();
        if (options && own)
            group->fail(*m, xpp::format("{} is not an option: a run's kind, start and homoclinic equilibria are its own keys", m->key));
        if (options && !own) check_option_name(*group, *m, run, names);
        if (!given.insert(m->key).second) group->fail(*m, xpp::format("{} is given twice in this run", m->key));
        append_member(out, m->key, m->value, *group);
    }
}

/* one run of the list, read whole; `known` the names of the runs before it */
AutoxScriptRun read_run(odex::Block &r, const std::vector<AutoxScriptRun> &known, const std::string &file, std::string_view text)
{
    AutoxScriptRun run;
    run.place = line_place(file, text, r.member("name") ? r.member("name")->line : 1);
    if (!r.text("name", run.name) || run.name.empty()) r.fail("a run needs name: \"its name\"");
    if (std::ranges::any_of(known, [&](const AutoxScriptRun &k) { return k.name == run.name; }))
        r.fail(*r.member("name"), xpp::format("there is already a run named \"{}\"", run.name));
    std::string kind;
    if (!r.word("kind", kind)) r.fail("a run needs kind: what it computes (equilibrium, periodic, two_parameter, fixed_period, bvp, homoclinic, heteroclinic)");
    std::set<std::string> given{"kind"};
    run.arguments = ",\"kind\":";
    json_append_string(run.arguments, kind);
    if (bool on = false; r.flag("switch", on)) {
        given.insert("switch");
        run.arguments += on ? ",\"switch\":true" : ",\"switch\":false";
    }
    if (bool on = false; r.flag("replace_diagram", on)) {
        given.insert("replace_diagram");
        run.replaces = on;
        run.arguments += on ? ",\"replace_diagram\":true" : ",\"replace_diagram\":false";
    }
    if (r.texts("parameters", run.parameters, 2) && run.parameters.empty())
        r.fail(*r.member("parameters"), "parameters names the one or two parameters the run continues");
    if (std::optional<odex::Block> start = r.group("start")) {
        AutoxScriptStart from;
        std::string type;
        if (!start->text("run", from.run) || !start->word("type", type) || !start->whole("index", from.index))
            start->fail("a start needs run: \"an earlier run\", type: a point type and index: its number among that run's points of the type");
        const auto known_run = std::ranges::find_if(known, [&](const AutoxScriptRun &k) { return k.name == from.run; });
        if (known_run == known.end())
            start->fail(*start->member("run"), xpp::format("\"{}\" is not a run before this one", from.run));
        if (!autox_point_kind_from_name(type)) start->fail(*start->member("type"), xpp::format("\"{}\" is not a point type", type));
        if (from.index < 1) start->fail(*start->member("index"), "the index counts from 1");
        from.type = type;
        start->done();
        run.from = std::move(from);
    }
    std::optional<odex::Block> settle = r.group("settle"), homoclinic = r.group("homoclinic"), options = r.group("options");
    if (settle && run.from) settle->fail("a run from a stored point has no settling: it starts at the point");
    append_group(run.arguments, settle, given, false, run.name);
    append_group(run.arguments, homoclinic, given, false, run.name);
    append_group(run.arguments, options, given, true, run.name);
    if (settle) settle->done();
    if (homoclinic) homoclinic->done();
    if (options) options->done();
    return run;
}

} // namespace

std::string autox_script_command(const AutoxScriptRun &run, std::string_view type, int index)
{
    std::string line{"{\"cmd\":\""};
    line += command_name;
    line += '"';
    line += run.arguments;
    if (!type.empty()) {
        line += ",\"type\":";
        json_append_string(line, type);
        line += xpp::format(",\"index\":{}", index);
    }
    return line + '}';
}

std::string autox_script_command(const AutoxScriptRun &run)
{
    return run.from ? autox_script_command(run, run.from->type, run.from->index) : autox_script_command(run, {}, 0);
}

std::string autox_script_axes_command(const AutoxScriptRun &run)
{
    if (run.parameters.empty()) return {};
    std::string line{"{\"cmd\":\"autox.configure_axes\",\"parameter1\":"};
    json_append_string(line, run.parameters[0]);
    if (run.parameters.size() == 2) {
        line += ",\"parameter2\":";
        json_append_string(line, run.parameters[1]);
        line += ",\"plot_type\":\"two_parameter\"";
    }
    return line + '}';
}

Result<AutoxScript> autox_script_read(std::string_view text, const std::string &file, const ScriptShapeCheck &shape)
{
    return odex::read_block("autox script", file, text, false, [&](odex::Block &b) {
        AutoxScript script;
        script.file = file;
        int version = 0;
        if (!b.whole(version_word, version)) b.fail(xpp::format("an autoX script starts with {}: {}", version_word, format_version));
        if (version != format_version) b.fail(*b.member(version_word), wrong_version(version, "autoX scripts"));
        if (!b.text("model", script.model) || !files::name_ok(script.model) || !files::has_extension(script.model, ".odex"))
            b.fail("an autoX script names its model: model: \"name.odex\", a base name");
        if (b.text("diagram", script.diagram_file) && (!files::name_ok(script.diagram_file) || !files::has_extension(script.diagram_file, ".csv")))
            b.fail(*b.member("diagram"), "diagram must be a .csv base name, the file --silent writes");
        std::vector<odex::Block> runs;
        if (!b.groups("runs", runs, autox_script_runs_limit) || runs.empty()) b.fail("an autoX script lists its runs: runs: [ { name: \"...\", kind: ... }, ... ]");
        for (odex::Block &r : runs) {
            script.runs.push_back(read_run(r, script.runs, file, text));
            AutoxScriptRun &run = script.runs.back();
            r.done();
            /* the command's own validator, on the run as it will be sent (the stored point's type and index stand in for its start) */
            if (const Result<> shaped = shape(autox_script_command(run), run.place); !shaped) throw ReadFailed{shaped.error()};
        }
        return script;
    });
}

Result<AutoxScript> autox_script_load(FileSystem &fs, const std::string &file, const ScriptShapeCheck &shape)
{
    const Result<std::string> bytes = fs.read("autox script", file, autox_script_bytes_limit);
    if (!bytes) return std::unexpected(bytes.error());
    return autox_script_read(*bytes, file, shape);
}

} // namespace xpp
