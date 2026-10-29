/* The bvp resource's registry rows (W521): boundary value problems solved by
   shooting from the initial values, with the period unknown, or over a
   range of one parameter, and the numerics they use. Each handler calls the
   core function the imported XPPAUT button sequences call too (pp_shoot.h). */
#include "command_def.h"
#include "command_registry.h"
#include "integrate.h"
#include "pp_shoot.h"
#include "session.h"
#include "typed_values.h"
#include "xpp_io.h"
#include <array>

namespace xpp::command {
namespace {

Result<std::string> bvp_solve_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<> solved = bvp_solve(s, args.get<bool>("show"));
    if (!solved) return std::unexpected<Error>(solved.error());
    return ResultObject().boolean("converged", true).done();
}

Result<std::string> bvp_periodic_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    const int parameter = find_user_name(s.model(), PARAMBOX, args.get<std::string>("frequency_parameter"));
    const int variable = find_user_name(s.model(), ICBOX, args.get<std::string>("section_variable"));
    const Result<double> period = bvp_solve_periodic(s, parameter, variable, args.get<double>("section"), args.get<bool>("show"));
    if (!period) return std::unexpected<Error>(period.error());
    return ResultObject().boolean("converged", true).real("period", *period).done();
}

Result<std::string> bvp_range_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    ShootRange range = s.shoot_range;
    if (const auto item = args.optional<std::string>("parameter")) range.item = *item;
    if (const auto low = args.optional<double>("start")) range.plow = *low;
    if (const auto high = args.optional<double>("end")) range.phigh = *high;
    if (const auto steps = args.optional<long long>("steps")) range.steps = static_cast<int>(*steps);
    if (const auto cycle = args.optional<bool>("cycle_color")) range.cycle = *cycle;
    if (const auto side = args.optional<std::string>("side")) range.side = *side == SHOOT_SIDE_NAMES[1] ? SHOOT_SIDE_END : SHOOT_SIDE_START;
    if (const auto movie = args.optional<bool>("movie")) range.movie = *movie;
    const Result<int> found = bvp_solve_range(s, &range);
    if (!found) return std::unexpected<Error>(found.error());
    return ResultObject().integer("solutions_found", *found).done();
}

Result<std::string> bvp_configure(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return configure_numerics(s, command, args, place, BVP_KEY_PREFIX);
}

constexpr std::array SOLVE_ARGUMENTS = {Argument{"show", Type::boolean, true}};
constexpr std::array PERIODIC_ARGUMENTS = {
    Argument{"frequency_parameter", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter},
    Argument{"section_variable", Type::model_name, true, MAX_TEXT_BYTES, {}, {}, {}, NameSet::variable},
    Argument{"section", Type::real, true}, Argument{"show", Type::boolean, true}};
constexpr std::array RANGE_ARGUMENTS = {
    Argument{"parameter", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::parameter},
    Argument{"start", Type::real, false},
    Argument{"end", Type::real, false},
    Argument{"steps", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"cycle_color", Type::boolean, false},
    Argument{"side", Type::choice, false, MAX_TEXT_BYTES, SHOOT_SIDE_NAMES},
    Argument{"movie", Type::boolean, false}};
/* the settings bvp.configure sets are numerics' bvp_max_iterates, bvp_tolerance and bvp_epsilon */
constexpr std::array CONFIGURE_ARGUMENTS = {
    Argument{"max_iterates", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"tolerance", Type::real, false},
    Argument{"epsilon", Type::real, false}};

constexpr std::array SOLVE_RESULTS = {ResultField{"converged", "boolean"}};
constexpr std::array PERIODIC_RESULTS = {ResultField{"converged", "boolean"}, ResultField{"period", "real"}};
constexpr std::array RANGE_RESULTS = {ResultField{"solutions_found", "integer"}};
constexpr std::array CONFIGURE_RESULTS = {ResultField{"settings", "map[name, value]"}};

constexpr std::array SOLVE_EVENTS = {xpp::events::JOB_COMPUTING, xpp::events::JOB_PROGRESS, xpp::events::PLOT_SERIES, xpp::events::JOB_STOPPED, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array CONFIGURE_EVENTS = {xpp::events::RUN_NUMERICS, xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"bvp.solve_range", RANGE_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Run/Boundary values/Range",
            "Solve the boundary value problem for each value of a parameter between two values, storing a solution's start or end for each.",
            RANGE_RESULTS, SOLVE_EVENTS, true, bvp_range_handler},
    Command{"bvp.solve", SOLVE_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Run/Boundary values/No show, Show",
            "Solve the boundary value problem from the initial values by shooting and make the solution the initial values; show draws every step.",
            SOLVE_RESULTS, SOLVE_EVENTS, true, bvp_solve_handler},
    Command{"bvp.solve_periodic", PERIODIC_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Analysis, "Run/Boundary values/Periodic",
            "Solve for a periodic orbit whose period is the frequency parameter, held to a section of one variable.", PERIODIC_RESULTS,
            SOLVE_EVENTS, true, bvp_periodic_handler},
    Command{"bvp.configure", CONFIGURE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Run/Numerics/bndVal",
            "Set the shooting's iteration limit, tolerance and finite-difference step; the settings are checked together.",
            CONFIGURE_RESULTS, CONFIGURE_EVENTS, true, bvp_configure}};

} // namespace

std::span<const Command> bvp_commands() { return ROWS; }

} // namespace xpp::command
