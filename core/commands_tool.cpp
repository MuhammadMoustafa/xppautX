/* The tool resource's registry rows (W528): the calculator, a comment of the model's source that has an action, and a
   button of the model (an `@ button`). Each calls what the File menu's item and the page's button call. */
#include "command_def.h"
#include "menudrive.h"
#include "session.h"
#include "typed_values.h"
#include "xpp_util.h"
#include <array>

namespace xpp::command {
namespace {

Result<std::string> calculator(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::string &expression = args.get<std::string>("expression");
    const Result<double> value = evaluate_formula(s, expression);
    if (!value) return std::unexpected<Error>(value.error());
    return ResultObject().text("expression", expression).real("value", *value).done();
}

Result<std::string> run_source_action(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const long long index = args.get<long long>("index");
    const std::vector<Model::Comment> &comments = s.model().comments;
    if (static_cast<std::size_t>(index) >= comments.size() || comments[static_cast<std::size_t>(index)].aflag <= 0)
        return fail(command.name, xpp::format("Comment {} of the source has no action ({} comments)", index, comments.size()), place);
    if (auto applied = do_txt_action(s, comments[static_cast<std::size_t>(index)].action); !applied) return std::unexpected<Error>(applied.error());
    return ResultObject().boolean("ran", true).done();
}

Result<std::string> run_user_button(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const long long index = args.get<long long>("index");
    if (index >= s.nuserbut) return fail(command.name, xpp::format("The model has no button {} ({} buttons)", index, s.nuserbut), place);
    run_the_commands(s, static_cast<MenuCommand>(s.userbut[static_cast<std::size_t>(index)].com));
    return ResultObject().boolean("ran", true).done();
}

constexpr Argument INDEX = {"index", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0)};
constexpr std::array CALCULATOR_ARGUMENTS = {Argument{"expression", Type::expression, true, MAX_EXPRESSION_BYTES}};
constexpr std::array INDEX_ARGUMENTS = {INDEX};
constexpr std::array CALCULATOR_RESULTS = {ResultField{"expression", "expression"}, ResultField{"value", "real"}};
constexpr std::array RAN_RESULTS = {ResultField{"ran", "boolean"}};
constexpr std::array CALCULATOR_EVENTS = {xpp::events::DONE};
constexpr std::array RAN_EVENTS = {xpp::events::SESSION_STATE, xpp::events::DONE};

Result<std::string> page_tool(Session &, const Command &, const Arguments &, const Place &) { return "{}"; }
constexpr std::array PALETTE_KEYS = {"Ctrl+Shift+P"};
constexpr std::array MANUAL_KEYS = {"F1"};
constexpr std::array ROWS = {
    Command{"tool.values", {}, XPP_KIND_VIEW, CommandCategory::Tools, "Data/Values",
            "Show the parameters and initial conditions.", {}, CALCULATOR_EVENTS, false, page_tool},
    Command{"tool.browser", {}, XPP_KIND_VIEW, CommandCategory::Tools, "Data/Browser",
            "Open the stored data browser.", {}, CALCULATOR_EVENTS, false, page_tool},
    Command{"tool.command_palette", {}, XPP_KIND_VIEW, CommandCategory::Tools, "Help/Command palette",
            "Search every registered command by name, menu path and description.", {}, CALCULATOR_EVENTS, false, page_tool, {}, PALETTE_KEYS},
    Command{"tool.keyboard_shortcuts", {}, XPP_KIND_VIEW, CommandCategory::Tools, "Help/Keyboard shortcuts",
            "Edit command keys and pins.", {}, CALCULATOR_EVENTS, false, page_tool},
    Command{"tool.manual", {}, XPP_KIND_VIEW, CommandCategory::Tools, "Help/Manual",
            "Read the manual.", {}, CALCULATOR_EVENTS, false, page_tool, {}, MANUAL_KEYS},
    Command{"tool.calculator", CALCULATOR_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Tools, "Analysis/Calculator",
            "Work out a formula of the model's names and functions.", CALCULATOR_RESULTS, CALCULATOR_EVENTS, true, calculator},
    Command{"tool.run_source_action", INDEX_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "Help/Tool/Run source action",
            "Run the action of an active comment of the model's source (a `{par=value,...}` block): comment `index` of session.get_source's list.",
            RAN_RESULTS, RAN_EVENTS, true, run_source_action},
    Command{"tool.run_user_button", INDEX_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Panels, "Help/Tool/Run user button",
            "Press a button of the model (an `@ button=name:keys` line), counted from 0 in the order the model gives them.", RAN_RESULTS,
            RAN_EVENTS, true, run_user_button}};

} // namespace

std::span<const Command> tool_commands() { return ROWS; }

} // namespace xpp::command
