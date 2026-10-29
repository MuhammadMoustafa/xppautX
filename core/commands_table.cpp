/* The table resource's registry rows (W525): a model's lookup tables, edited (the formula, points and range of a table
   of a formula, or the file a table of a file is read from again) or viewed in the data browser. Each handler calls
   the core function the Tables menu's items call after their questions (tabular.h). */
#include "command_def.h"
#include "session.h"
#include "tabular.h"
#include <array>

namespace xpp::command {
namespace {

Result<std::string> table_edit(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<int> index = find_table(s, args.get<std::string>("name"));
    if (!index) return std::unexpected<Error>(index.error());
    TableEdit edit;
    edit.points = args.optional<long long>("points").transform([](long long points) { return static_cast<int>(points); });
    edit.x_min = args.optional<double>("x_min");
    edit.x_max = args.optional<double>("x_max");
    edit.formula = args.optional<std::string>("formula");
    edit.file = args.optional<std::string>("file");
    edit.auto_evaluate = args.optional<bool>("auto_evaluate");
    const Result<int> points = edit_table(s, *index, edit);
    if (!points) return std::unexpected<Error>(points.error());
    return ResultObject().text("table", s.tables[static_cast<std::size_t>(*index)].name).integer("points_evaluated", *points).done();
}

Result<std::string> table_view(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<int> index = find_table(s, args.get<std::string>("name"));
    if (!index) return std::unexpected<Error>(index.error());
    if (const Result<int> shown = view_table(s, *index); !shown) return std::unexpected<Error>(shown.error());
    return ResultObject().text("table", s.tables[static_cast<std::size_t>(*index)].name).boolean("shown_in_browser", true).done();
}

constexpr Argument NAME = {"name", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::any};
constexpr std::array VIEW_ARGUMENTS = {NAME};
constexpr std::array EDIT_ARGUMENTS = {
    NAME,
    Argument{"formula", Type::expression, false},
    Argument{"x_min", Type::real, false},
    Argument{"x_max", Type::real, false},
    Argument{"points", Type::integer, false, MAX_TEXT_BYTES, {}, constant(2), constant(table_points_limit)},
    Argument{"auto_evaluate", Type::boolean, false},
    Argument{"file", Type::file, false}};

constexpr std::array EDIT_RESULTS = {ResultField{"table", "name"}, ResultField{"points_evaluated", "integer"}};
constexpr std::array VIEW_RESULTS = {ResultField{"table", "name"}, ResultField{"shown_in_browser", "boolean"}};
constexpr std::array EDIT_EVENTS = {"state", "done"};
constexpr std::array VIEW_EVENTS = {"browser", "state", "done"};

constexpr std::array ROWS = {
    Command{"table.edit", EDIT_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "Analysis/Numerics/Tables/Edit",
            "Change a table of a formula (its formula, points, range, and whether it is evaluated again when a parameter changes) or read a "
            "table of a file again from its file or another; what is left out stays.",
            EDIT_RESULTS, EDIT_EVENTS, true, table_edit},
    Command{"table.view_in_browser", VIEW_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Panels, "Analysis/Numerics/Tables/View",
            "Show a table's points in the data browser.", VIEW_RESULTS, VIEW_EVENTS, true, table_view}};

} // namespace

std::span<const Command> table_commands() { return ROWS; }

} // namespace xpp::command
