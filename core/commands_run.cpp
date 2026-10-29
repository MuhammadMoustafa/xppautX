/* The run resource's registry rows (W521): integration from the initial data,
   the last state or the old data, Continue and the steady-state run, the
   range sweeps, the DAE guesses, Stop and the numerics that shape a run. Each
   handler gets its arguments validated and typed (command_def.h) and calls
   the core function the imported XPPAUT button sequences call too
   (integrate.h's run_from, run_range ...). */
#include "command_def.h"
#include "command_range.h"
#include "command_registry.h"
#include "dae_fun.h"
#include "integrate.h"
#include "model_options.h"
#include "numerics.h"
#include "numerics_settings.h"
#include "session.h"
#include "value_undo.h"
#include "xpp_io.h"
#include "xpp_util.h"
#include <algorithm>
#include <array>

namespace xpp::command {

void set_range_settings(RangeVars &range, const Arguments &args)
{
    range.rtype = RANGE_SINGLE;
    if (const auto item = args.optional<std::string>("variable")) range.item = *item;
    if (const auto low = args.optional<double>("start")) range.plow = *low;
    if (const auto high = args.optional<double>("end")) range.phigh = *high;
    if (const auto steps = args.optional<long long>("steps")) range.steps = static_cast<int>(*steps);
    if (const auto reset = args.optional<bool>("reset_storage")) range.reset = *reset;
    if (const auto old = args.optional<bool>("use_old_ic")) range.oldic = *old;
    if (const auto cycle = args.optional<bool>("cycle_color")) range.cycle = *cycle;
    if (const auto movie = args.optional<bool>("movie")) range.movie = *movie;
    if (const auto files = args.optional<bool>("write_each_run_to_file")) range.write_files = *files;
}

namespace {

constexpr const char *STATUS_COMPLETED = "completed", *STATUS_ENDED_EARLY = "ended_early";

std::string run_result(const Session &s, bool ended_early)
{
    return ResultObject()
        .text("status", ended_early ? STATUS_ENDED_EARLY : STATUS_COMPLETED)
        .integer("steps", s.data_store.rows)
        .real("time", s.data_store.current_time)
        .done();
}

template <RunFrom from> Result<std::string> run_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<bool> ran = run_from(s, from, args.optional<bool>("reset_integrals").value_or(false));
    if (!ran) return std::unexpected<Error>(ran.error());
    return run_result(s, *ran);
}

Result<std::string> run_shoot(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<bool> ran = run_from_manifold(s, static_cast<int>(args.get<long long>("manifold_index")));
    if (!ran) return std::unexpected<Error>(ran.error());
    return run_result(s, *ran);
}

Result<std::string> run_continue(Session &s, const Command &, const Arguments &args, const Place &)
{
    const std::optional<double> until = args.optional<double>("until"), extra = args.optional<double>("extra");
    const Result<bool> ran = until ? continue_to(s, *until) : extra ? continue_for(s, *extra) : continue_total(s);
    if (!ran) return std::unexpected<Error>(ran.error());
    return ResultObject()
        .text("status", *ran ? STATUS_ENDED_EARLY : STATUS_COMPLETED)
        .real("end_time", s.data_store.current_time)
        .done();
}

Result<std::string> run_steady(Session &s, const Command &, const Arguments &args, const Place &)
{
    const Result<> ran = run_to_steady_state(
        s, {static_cast<int>(args.get<long long>("decimals")), args.get<double>("hold"), args.get<double>("maximum")});
    if (!ran) return std::unexpected<Error>(ran.error());
    const SteadyStateResult &steady = *s.integrator.steady_result;
    return ResultObject().text("status", steady.status).integer("decimals", steady.decimals).real("time", steady.time).done();
}

/* what a range sweep gives back: the runs it completed (named by the command) and the rows they stored */
std::string sweep_result(const RangeProgress &done, const char *runs_name)
{
    return ResultObject().integer(runs_name, done.runs).integer("total_steps", done.steps).done();
}

Result<std::string> run_range_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    set_range_settings(s.integrator.range, args);
    if (const Result<> items = resolve_range_items(s, false); !items) return std::unexpected<Error>(items.error());
    RangeProgress done;
    if (const Result<int> swept = run_range(s, 0, &done); !swept) return std::unexpected<Error>(swept.error());
    return sweep_result(done, "runs_completed");
}

constexpr std::array VARY_CHOICES = {"together", "independently"};

Result<std::string> run_double_range_handler(Session &s, const Command &, const Arguments &args, const Place &)
{
    RangeVars &range = s.integrator.range;
    if (const auto item = args.optional<std::string>("variable1")) range.item = *item;
    if (const auto low = args.optional<double>("start1")) range.plow = *low;
    if (const auto high = args.optional<double>("end1")) range.phigh = *high;
    if (const auto item = args.optional<std::string>("variable2")) range.item2 = *item;
    if (const auto low = args.optional<double>("start2")) range.plow2 = *low;
    if (const auto high = args.optional<double>("end2")) range.phigh2 = *high;
    if (const auto steps = args.optional<long long>("steps1")) range.steps = static_cast<int>(*steps);
    if (const auto steps = args.optional<long long>("steps2")) range.steps2 = static_cast<int>(*steps);
    /* the form's own default: both together unless the last range varied them independently */
    const std::string vary = args.optional<std::string>("vary").value_or(range.rtype == RANGE_INDEPENDENT ? VARY_CHOICES[1] : VARY_CHOICES[0]);
    range.rtype = vary == VARY_CHOICES[1] ? RANGE_INDEPENDENT : RANGE_TOGETHER;
    if (const auto reset = args.optional<bool>("reset_storage")) range.reset = *reset;
    if (const auto old = args.optional<bool>("use_old_ic")) range.oldic = *old;
    if (const auto cycle = args.optional<bool>("cycle_color")) range.cycle = *cycle;
    if (const auto movie = args.optional<bool>("movie")) range.movie = *movie;
    if (const auto files = args.optional<bool>("write_each_run_to_file")) range.write_files = *files;
    if (const Result<> items = resolve_range_items(s, true); !items) return std::unexpected<Error>(items.error());
    RangeProgress done;
    const Result<int> swept = run_range(s, 1, &done);
    if (!swept) return std::unexpected<Error>(swept.error());
    return sweep_result(done, "grid_points");
}

Result<std::string> run_dae_guess(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    Model &m = s.model();
    if (m.nsvar < 1) return fail(command.name, "The model has no algebraic variables (no 0= equations with a guess to set)", place);
    std::vector<std::pair<int, std::string>> chosen;
    for (const auto &[name, formula] : args.map<std::string>("guesses")) {
        const auto at = std::ranges::find_if(m.svars, [&](const auto &var) { return equal_ignoring_case(var.name, name); });
        if (at == m.svars.end()) return fail(command.name, xpp::format("\"{}\" is not an algebraic variable of the model", name), place);
        chosen.emplace_back(static_cast<int>(at - m.svars.begin()), formula);
    }
    for (const auto &[index, formula] : chosen)
        if (const Result<double> set = set_dae_guess(s, index, formula); !set) return std::unexpected<Error>(set.error());
    std::string guesses = "{";
    for (int i = 0; i < m.nsvar; ++i) {
        if (i) guesses += ',';
        json_append_string(guesses, m.svars[static_cast<std::size_t>(i)].name);
        guesses += ':';
        json::json_append_number_shortest(guesses, s.dae.svar_last[static_cast<std::size_t>(i)]);
    }
    return ResultObject().raw("guesses", guesses + '}').done();
}

/* The stop itself is made on the reader thread (ui_json.cpp classify), where a computation takes it at its
   next check; a line that reaches here found nothing to stop. */
Result<std::string> run_abort(Session &s, const Command &, const Arguments &, const Place &)
{
    return ResultObject().real("stopped_at", s.data_store.current_time).done();
}

} // namespace

namespace {

/* the numerics settings with no key in the main table: the method's own (numerics.h's NumericsExtras) */
constexpr std::array EXTRA_NAMES = {"vmaxpts", "autoeval", "bandlo", "bandup", "delay_real_guess", "delay_imaginary_guess", "delay_grid"};

bool is_numerics_extra(std::string_view name)
{
    return std::ranges::find(EXTRA_NAMES, name) != EXTRA_NAMES.end();
}

NumericsExtras numerics_extras(const Arguments &args)
{
    NumericsExtras extras;
    if (auto v = args.optional<long long>("vmaxpts")) extras.max_points = static_cast<int>(*v);
    extras.auto_evaluate = args.optional<bool>("autoeval");
    if (auto v = args.optional<long long>("bandlo")) extras.band_lower = static_cast<int>(*v);
    if (auto v = args.optional<long long>("bandup")) extras.band_upper = static_cast<int>(*v);
    extras.delay_real_guess = args.optional<double>("delay_real_guess");
    extras.delay_imaginary_guess = args.optional<double>("delay_imaginary_guess");
    if (auto v = args.optional<long long>("delay_grid")) extras.delay_grid = static_cast<int>(*v);
    return extras;
}

bool has_numerics_extras(const NumericsExtras &e)
{
    return e.max_points || e.auto_evaluate || e.band_lower || e.band_upper || e.delay_real_guess || e.delay_imaginary_guess ||
           e.delay_grid;
}

/* the extras the call gave, as "name":value members of the settings result */
std::string extras_json(const Session &s, const Arguments &args)
{
    std::string members;
    const auto add = [&](const char *name, const std::string &value) {
        if (!args.has(name)) return;
        if (!members.empty()) members += ',';
        json_append_string(members, name);
        members += ':' + value;
    };
    add("vmaxpts", std::to_string(s.numerics.max_points));
    add("autoeval", s.numerics.auto_evaluate ? "true" : "false");
    add("bandlo", std::to_string(s.numerics.cv_bandlower));
    add("bandup", std::to_string(s.numerics.cv_bandupper));
    add("delay_real_guess", xpp::number(s.delay.alpha_max));
    add("delay_imaginary_guess", xpp::number(s.delay.omega_max));
    add("delay_grid", std::to_string(s.delay.grid));
    return members;
}

} // namespace

Result<std::string> configure_numerics(Session &s, const Command &command, const Arguments &args, const Place &place,
                                       std::string_view key_prefix)
{
    std::vector<std::string> keys, texts;
    for (const Argument &arg : command.arguments) {
        if (!args.has(arg.name) || is_numerics_extra(arg.name)) continue;
        keys.push_back(std::string(key_prefix) + arg.name);
        switch (arg.type) {
        case Type::real: texts.push_back(xpp::number(args.get<double>(arg.name))); break;
        case Type::integer: texts.push_back(std::to_string(args.get<long long>(arg.name))); break;
        case Type::boolean: texts.push_back(args.get<bool>(arg.name) ? "true" : "false"); break;
        default: texts.push_back(args.get<std::string>(arg.name)); break;
        }
    }
    const NumericsExtras extras = numerics_extras(args);
    if (keys.empty() && !has_numerics_extras(extras)) return fail(command.name, "Give at least one setting", place);
    std::vector<NumericsValue> values;
    for (std::size_t i = 0; i < keys.size(); ++i) values.push_back({keys[i], texts[i]});
    std::string why;
    push_value_undo(s);
    if (!values.empty() && numerics_settings_set_all(s, values, why, &place) != 0) {
        Error error{command.name, why, place};
        if (keys.size() == 1) error.field = "num:" + keys.front();
        return std::unexpected<Error>(std::move(error));
    }
    if (has_numerics_extras(extras)) {
        /* the extras are judged against the method and delays this call has just set; a refusal takes the whole call back */
        if (auto set = set_numerics_extras(s, extras); !set) {
            undo_values(s);
            return std::unexpected<Error>(std::move(set.error()));
        }
    }
    std::string settings = "{";
    for (const std::string &key : keys) {
        if (settings.size() > 1) settings += ',';
        json_append_string(settings, key);
        settings += ':';
        settings += numerics_settings_json_value(s, key);
    }
    if (const std::string extra = extras_json(s, args); !extra.empty()) settings += (settings.size() > 1 ? "," : "") + extra;
    return ResultObject().raw("settings", settings + '}').done();
}

namespace {

Result<std::string> run_configure(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    return configure_numerics(s, command, args, place, {});
}

constexpr std::array RESET_ARGUMENTS = {Argument{"reset_integrals", Type::boolean, false}};
constexpr std::array MANIFOLD_ARGUMENTS = {Argument{"manifold_index", Type::integer, true, MAX_TEXT_BYTES, {}, constant(1)}};
constexpr std::array CONTINUE_ARGUMENTS = {Argument{"until", Type::real, false}, Argument{"extra", Type::real, false}};
constexpr std::array CONTINUE_EXCLUSIVE = {"until", "extra"};
constexpr std::array CONTINUE_GROUPS = {Group{GroupRule::at_most_one, CONTINUE_EXCLUSIVE}};
constexpr std::array STEADY_ARGUMENTS = {Argument{"decimals", Type::integer, true, MAX_TEXT_BYTES, {}, constant(0), constant(MAX_STEADY_DECIMALS)},
                                         Argument{"hold", Type::real, true}, Argument{"maximum", Type::real, true}};
constexpr std::array DOUBLE_RANGE_ARGUMENTS = {
    Argument{"variable1", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either},
    Argument{"start1", Type::real, false},
    Argument{"end1", Type::real, false},
    Argument{"variable2", Type::model_name, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::either},
    Argument{"start2", Type::real, false},
    Argument{"end2", Type::real, false},
    Argument{"steps1", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"steps2", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"vary", Type::choice, false, MAX_TEXT_BYTES, VARY_CHOICES},
    Argument{"reset_storage", Type::boolean, false},
    Argument{"use_old_ic", Type::boolean, false},
    Argument{"cycle_color", Type::boolean, false},
    Argument{"movie", Type::boolean, false},
    Argument{"write_each_run_to_file", Type::boolean, false}};
constexpr std::array DAE_ARGUMENTS = {
    Argument{"guesses", Type::map, false, MAX_TEXT_BYTES, {}, {}, {}, NameSet::any, Type::expression}};
/* run.configure's arguments are the numerics settings' keys (numerics_settings.h); the method is a word of the
   settings' own list, which its owner checks (numerics.fields' choices) */
constexpr std::array CONFIGURE_ARGUMENTS = {
    Argument{"total_time", Type::real, false},
    Argument{"start_time", Type::real, false},
    Argument{"store_from", Type::real, false},
    Argument{"dt", Type::real, false},
    Argument{"method", Type::text, false},
    Argument{"store_every", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"state_limit", Type::real, false},
    Argument{"stop_at_limit", Type::boolean, false},
    Argument{"delay", Type::real, false},
    Argument{"relative_tolerance", Type::real, false},
    Argument{"absolute_tolerance", Type::real, false},
    Argument{"min_step", Type::real, false},
    Argument{"max_step", Type::real, false},
    Argument{"eul_tol", Type::real, false},
    Argument{"eul_iter", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)},
    Argument{"vmaxpts", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1), constant(option_limit::history_points)},
    Argument{"autoeval", Type::boolean, false},
    Argument{"bandlo", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(option_limit::bandwidth)},
    Argument{"bandup", Type::integer, false, MAX_TEXT_BYTES, {}, constant(0), constant(option_limit::bandwidth)},
    Argument{"delay_real_guess", Type::real, false},
    Argument{"delay_imaginary_guess", Type::real, false},
    Argument{"delay_grid", Type::integer, false, MAX_TEXT_BYTES, {}, constant(1)}};

constexpr std::array RUN_RESULTS = {ResultField{"status", "choice"}, ResultField{"steps", "integer"}, ResultField{"time", "real"}};
constexpr std::array CONTINUE_RESULTS = {ResultField{"status", "choice"}, ResultField{"end_time", "real"}};
constexpr std::array STEADY_RESULTS = {ResultField{"status", "choice"}, ResultField{"decimals", "integer"}, ResultField{"time", "real"}};
constexpr std::array RANGE_RESULTS = {ResultField{"runs_completed", "integer"}, ResultField{"total_steps", "integer"}};
constexpr std::array DOUBLE_RANGE_RESULTS = {ResultField{"grid_points", "integer"}, ResultField{"total_steps", "integer"}};
constexpr std::array DAE_RESULTS = {ResultField{"guesses", "map[name, real]"}};
constexpr std::array ABORT_RESULTS = {ResultField{"stopped_at", "real"}};
constexpr std::array CONFIGURE_RESULTS = {ResultField{"settings", "map[name, value]"}};

constexpr std::array RUN_EVENTS = {"computing", "progress", "series", "stopped", "state", "done"};
constexpr std::array CONTROL_EVENTS = {"stopped", "done"};
constexpr std::array STATE_EVENTS = {"numerics", "state", "done"};
constexpr std::array GUESS_EVENTS = {"state", "done"};

constexpr std::array ROWS = {
    Command{"run.initial", {}, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Go", "IG",
            "Run from the initial values.", RUN_RESULTS, RUN_EVENTS, true, run_handler<RunFrom::initial>},
    Command{"run.last", RESET_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Last", "IL",
            "Make the last state the initial values and run from it.", RUN_RESULTS, RUN_EVENTS, true, run_handler<RunFrom::last>},
    Command{"run.old", {}, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Old", "IO",
            "Run from the initial values as they stand, without refreshing the DAE guesses first.", RUN_RESULTS, RUN_EVENTS, true,
            run_handler<RunFrom::old>},
    Command{"run.continue", CONTINUE_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Continue", "C",
            "Extend the trajectory to an end time, by a duration, or by the run's total time.", CONTINUE_RESULTS, RUN_EVENTS, true,
            run_continue, CONTINUE_GROUPS},
    Command{"run.steady", STEADY_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "", "",
            "Run from the initial values until every state stops changing at the chosen decimals.", STEADY_RESULTS, RUN_EVENTS, true,
            run_steady},
    Command{"run.backward", {}, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Backward", "IB",
            "Run from the initial values backward in time.", RUN_RESULTS, RUN_EVENTS, true, run_handler<RunFrom::backward>},
    Command{"run.shift", RESET_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Shift", "IS",
            "Run from the last state with the clock going on from where it stopped.", RUN_RESULTS, RUN_EVENTS, true,
            run_handler<RunFrom::shift>},
    Command{"run.range", RUN_RANGE_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/Range", "IR",
            "Run once for each value of a parameter or variable between two values.", RANGE_RESULTS, RUN_EVENTS, true, run_range_handler},
    Command{"run.double_range", DOUBLE_RANGE_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/2par range", "I2",
            "Run once for each pair of values of two parameters or variables.", DOUBLE_RANGE_RESULTS, RUN_EVENTS, true,
            run_double_range_handler},
    Command{"run.dae_guess", DAE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Initialconds/DAE guess", "ID",
            "Set the initial guesses of the algebraic variables; none given keeps them.", DAE_RESULTS, GUESS_EVENTS, true,
            run_dae_guess},
    Command{"run.abort", {}, XPP_KIND_CONTROL, CommandCategory::Run, "Stop", "Esc",
            "Stop the running command where it is; the stopped command ends with the status stopped.", ABORT_RESULTS, CONTROL_EVENTS, false,
            run_abort},
    Command{"run.configure", CONFIGURE_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Run, "Numerics/Settings", "UT/US/UR/UD/UM/UO/UB/UE",
            "Set the integration's numerics (run length, step, method, tolerances, bounds, delay); the settings are checked together.",
            CONFIGURE_RESULTS, STATE_EVENTS, true, run_configure},
    Command{"run.shoot", MANIFOLD_ARGUMENTS, XPP_KIND_COMPUTE, CommandCategory::Run, "Initialconds/sHoot", "IH",
            "Run from a manifold the equilibria's shooting found.", RUN_RESULTS, RUN_EVENTS, true, run_shoot}};

} // namespace

std::span<const Command> run_commands() { return ROWS; }

} // namespace xpp::command
