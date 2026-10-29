/* The ic resource's registry rows (W521): the initial conditions set by
   values, a click, a file, a formula or a row of the data. Setting commands
   change the initial values and run nothing; the click commands start the
   run from the point clicked, as Initialconds' Mouse and mIce always did. */
#include "command_def.h"
#include "command_registry.h"
#include "integrate.h"
#include "lunch-new.h"
#include "many_pops.h"
#include "browse.h"
#include "session.h"
#include "typed_values.h"
#include "value_undo.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"
#include <algorithm>
#include <array>

namespace xpp::command {
namespace {

constexpr const char *WINDOW_KEY = "window";

/* the initial values of the states `indices` of the model, as {"name":value,...}: what a setting command did */
std::string applied(const Session &s, std::span<const int> indices)
{
    std::string text = "{";
    for (const int index : indices) {
        if (text.size() > 1) text += ',';
        json_append_string(text, s.model().uvar_names[static_cast<std::size_t>(index)]);
        text += ':';
        json::json_append_number_shortest(text, s.last_ic[static_cast<std::size_t>(index)]);
    }
    return text + '}';
}

std::vector<int> all_states(const Session &s)
{
    std::vector<int> indices(static_cast<std::size_t>(s.model().node + s.model().nmarkov));
    for (std::size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<int>(i);
    return indices;
}

/* the field name the page keys a value's error by (docs/protocol.md "Errors") */
std::string field_of(std::string_view name) { return "ic:" + lower_case(std::string(name)); }

Result<std::string> ic_set(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const auto entries = args.map<double>("values");
    std::vector<int> indices;
    for (const auto &[name, value] : entries) {
        const int index = find_user_name(s.model(), ICBOX, name);
        if (index < 0) return std::unexpected<Error>(Error{command.name, xpp::format("The model has no initial condition \"{}\"", name), place, field_of(name)});
        indices.push_back(index);
    }
    if (indices.empty()) return fail(command.name, "Give at least one value", place);
    push_value_undo(s);
    for (std::size_t i = 0; i < indices.size(); ++i) s.last_ic[static_cast<std::size_t>(indices[i])] = entries[i].second;
    redraw_ics();
    return ResultObject().raw("applied", applied(s, indices)).done();
}

/* the graph a click command works on, its two states, or why it has none */
Result<Plane> clicked_plane(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string name = args.optional<std::string>(WINDOW_KEY).value_or(s.selected_window);
    const auto target = find_window(s, name);
    if (!target) return fail(command.name, xpp::format("Unknown window \"{}\"", name), place);
    if (target->kind != WindowKind::graph) return fail(command.name, "Clicks set initial conditions in a graph", place);
    return window_plane(s, target->index);
}

/* the click's coordinates keyed by the plane's two states, as ic.at takes them */
Result<std::pair<double, double>> clicked_point(const Session &s, const Command &command, const Arguments &args, Plane plane, const Place &place)
{
    std::optional<double> horizontal, vertical;
    const std::string &h = s.model().uvar_names[static_cast<std::size_t>(plane.horizontal)], &v = s.model().uvar_names[static_cast<std::size_t>(plane.vertical)];
    for (const auto &[name, value] : args.map<double>("coordinates")) {
        if (equal_ignoring_case(name, h)) horizontal = value;
        else if (equal_ignoring_case(name, v)) vertical = value;
        else return fail(command.name, xpp::format("The window plots {} against {}: \"{}\" is neither", v, h, name), place);
    }
    if (!horizontal || !vertical) return fail(command.name, xpp::format("Give both {} and {}, the two states the window plots", h, v), place);
    return std::pair{*horizontal, *vertical};
}

Result<std::string> ic_at(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<Plane> plane = clicked_plane(s, command, args, place);
    if (!plane) return std::unexpected<Error>(plane.error());
    const Result<std::pair<double, double>> point = clicked_point(s, command, args, *plane, place);
    if (!point) return std::unexpected<Error>(point.error());
    const Result<int> ran = run_from_clicks(s, *plane, std::span(&*point, 1), false);
    if (!ran) return std::unexpected<Error>(ran.error());
    return ResultObject().raw("applied", applied(s, all_states(s))).done();
}

Result<std::string> ic_mouse_stream(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const Result<Plane> plane = clicked_plane(s, command, args, place);
    if (!plane) return std::unexpected<Error>(plane.error());
    const std::vector<double> x = args.list<double>("x"), y = args.list<double>("y");
    if (x.size() != y.size()) return fail(command.name, xpp::format("x has {} values and y {}: one pair for each click", x.size(), y.size()), place);
    if (x.empty()) return fail(command.name, "Give at least one click", place);
    std::vector<std::pair<double, double>> clicks;
    for (std::size_t i = 0; i < x.size(); ++i) clicks.emplace_back(x[i], y[i]);
    const Result<int> ran = run_from_clicks(s, *plane, clicks, true);
    if (!ran) return std::unexpected<Error>(ran.error());
    return ResultObject().integer("trajectories_run", *ran).done();
}

Result<std::string> ic_load(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &file = args.get<std::string>("file");
    const Result<> loaded = load_ic_file_named(s, file);
    if (!loaded) return std::unexpected<Error>(loaded.error());
    redraw_ics();
    return ResultObject().text("file", file).raw("applied", applied(s, all_states(s))).done();
}

Result<std::string> ic_formula(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &variable = args.get<std::string>("variable"), &formula = args.get<std::string>("formula");
    const Result<std::vector<int>> set = set_ic_formula(s, variable, formula);
    if (!set) return std::unexpected<Error>(Error{command.name, set.error().what, place, field_of(variable)});
    return ResultObject().text("variable", variable).text("formula", formula).done();
}

Result<std::string> ic_array(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string &name = args.get<std::string>("name");
    const Result<std::vector<int>> set = set_ic_array(s, name, static_cast<int>(args.get<long long>("size")), args.get<std::string>("formula"));
    if (!set) return std::unexpected<Error>(Error{command.name, set.error().what, place, field_of(name)});
    return ResultObject().text("name", name).integer("size", static_cast<long long>(set->size())).done();
}

Result<std::string> ic_from_data_row(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const long long row = args.get<long long>("row");
    if (row >= s.data_store.rows)
        return fail(command.name, xpp::format("Row {} is past the data: it has {} rows", row, s.data_store.rows), place);
    ic_of_row(s, static_cast<int>(row));
    std::string values = "{";
    for (int i = 0; i < s.model().node + s.model().nmarkov; ++i) {
        if (i) values += ',';
        json_append_string(values, s.model().uvar_names[static_cast<std::size_t>(i)]);
        values += ':';
        json::json_append_number_shortest(values, s.last_ic[static_cast<std::size_t>(i)]);
    }
    return ResultObject().integer("row", row).raw("values", values + '}').done();
}

constexpr std::array SET_ARGUMENTS = {Argument{"values", Type::map, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::real}};
constexpr std::array AT_ARGUMENTS = {Argument{"coordinates", Type::map, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable, Type::real},
                                     Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array STREAM_ARGUMENTS = {Argument{"x", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real},
                                         Argument{"y", Type::list, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::none, Type::real},
                                         Argument{WINDOW_KEY, Type::window, false}};
constexpr std::array LOAD_ARGUMENTS = {Argument{"file", Type::file, true}};
constexpr std::array FORMULA_ARGUMENTS = {Argument{"variable", Type::text, true}, Argument{"formula", Type::text, true}};
constexpr std::array ARRAY_ARGUMENTS = {Argument{"name", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable},
                                        Argument{"size", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1)},
                                        Argument{"formula", Type::expression, true}};
constexpr std::array ROW_ARGUMENTS = {Argument{"row", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)}};

constexpr std::array SET_RESULTS = {ResultField{"applied", "map[name, real]"}};
constexpr std::array STREAM_RESULTS = {ResultField{"trajectories_run", "integer"}};
constexpr std::array LOAD_RESULTS = {ResultField{"file", "file"}, ResultField{"applied", "map[name, real]"}};
constexpr std::array FORMULA_RESULTS = {ResultField{"variable", "text"}, ResultField{"formula", "text"}};
constexpr std::array ARRAY_RESULTS = {ResultField{"name", "name"}, ResultField{"size", "integer"}};
constexpr std::array ROW_RESULTS = {ResultField{"row", "integer"}, ResultField{"values", "map[name, real]"}};

constexpr std::array STATE_EVENTS = {"state", "done"};
constexpr std::array RUN_EVENTS = {"computing", "progress", "series", "stopped", "state", "done"};

constexpr std::array ROWS = {
    Command{"ic.set", SET_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Run/Initial conditions/New", "IN",
            "Set the initial values of some states; the others keep theirs.", SET_RESULTS, STATE_EVENTS, true, ic_set},
    Command{"ic.at", AT_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Run/Initial conditions/Mouse", "IM",
            "Run from a point of a graph: the two states it plots take the coordinates, the others their initial values.", SET_RESULTS,
            RUN_EVENTS, true, ic_at},
    Command{"ic.mouse_stream", STREAM_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Run/Initial conditions/mIce", "II",
            "Run from each point of a graph in turn, x and y the coordinates of the two states it plots; the runs add up on the graph.",
            STREAM_RESULTS, RUN_EVENTS, true, ic_mouse_stream},
    Command{"ic.load", LOAD_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Run/Initial conditions/File", "IF",
            "Read the initial values from a file of the model's folder, one number per differential equation.", LOAD_RESULTS, STATE_EVENTS,
            true, ic_load},
    Command{"ic.formula", FORMULA_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Run/Initial conditions/formUla", "IUF",
            "Set a state's initial value to a formula, or an array range u[1..5] to a formula reading [j] as the element's number.",
            FORMULA_RESULTS, STATE_EVENTS, true, ic_formula},
    Command{"ic.from_data_row", ROW_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Run/Data/Get", "G",
            "Make a row of the stored data (the first is 0) the initial values.", ROW_RESULTS, STATE_EVENTS, true, ic_from_data_row,
            {}, {}, "browser"},
    Command{"ic.array", ARRAY_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Run/Initial conditions/formUla", "IUA",
            "Set size states from a state on to a formula of t, the element's offset from the first.", ARRAY_RESULTS, STATE_EVENTS, true,
            ic_array}};

} // namespace

std::span<const Command> ic_commands() { return ROWS; }

} // namespace xpp::command
