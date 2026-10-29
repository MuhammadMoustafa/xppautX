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
    do_txt_action(s, comments[static_cast<std::size_t>(index)].action);
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
constexpr std::array CALCULATOR_ARGUMENTS = {Argument{"expression", Type::expression, true}};
constexpr std::array INDEX_ARGUMENTS = {INDEX};
constexpr std::array CALCULATOR_RESULTS = {ResultField{"expression", "expression"}, ResultField{"value", "real"}};
constexpr std::array RAN_RESULTS = {ResultField{"ran", "boolean"}};
constexpr std::array CALCULATOR_EVENTS = {"done"};
constexpr std::array RAN_EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"tool.calculator", CALCULATOR_ARGUMENTS, XPP_KIND_VIEW, CommandCategory::Tools, "File/Calculator", "FC",
            "Work out a formula of the model's names and functions.", CALCULATOR_RESULTS, CALCULATOR_EVENTS, true, calculator},
    Command{"tool.run_source_action", INDEX_ARGUMENTS, XPP_KIND_DATA, CommandCategory::Panels, "", "",
            "Run the action of an active comment of the model's source (a `{par=value,...}` block): comment `index` of session.get_source's list.",
            RAN_RESULTS, RAN_EVENTS, true, run_source_action},
    Command{"tool.run_user_button", INDEX_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Panels, "", "",
            "Press a button of the model (an `@ button=name:keys` line), counted from 0 in the order the model gives them.", RAN_RESULTS,
            RAN_EVENTS, true, run_user_button}};

} // namespace

std::span<const Command> tool_commands() { return ROWS; }

} // namespace xpp::command
