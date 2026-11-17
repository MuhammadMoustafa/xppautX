/* The settings resource's registry rows (W287, W622, W790): the user's settings (xpp_settings.h: settings.json) that the program
   keeps for each run, and the one command that sets one value of them by its group and key (`settings.set`, the page's theme, panels
   and folds). The rows are the application's, so they are declared in command_def.h and joined into the registry by command_registry.cpp. */
#include "command_def.h"
#include "autox_parallel.h"
#include "colormap.h"
#include "command_category.h"
#include "event_registry.h"
#include "model_options.h"
#include "session.h"
#include "model.h"
#include "xpp_cpu.h"
#include "xpp_error.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include "model_files.h"
#include "xpp_settings.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace xpp::command {
namespace {

/* the settings as they stand for the model (the user's, the model folder's on top): the thread count (and whether it is automatic), what the log
   shows and whether it goes to the model folder's file, and the colors */
Result<std::string> settings_now(const Session &s)
{
    const Result<settings::Settings> kept = settings::effective_settings(*s.model().fs, settings::load_settings(*s.model().fs), model_folder(s.model()));
    const settings::Settings defaults;
    const settings::Settings &now = kept ? *kept : defaults;
    const bool automatic = s.threads == cpu::THREADS_AUTO;
    ResultObject result;
    result.integer("threads", automatic ? AutoxParallel::automatic_threads() : s.threads).boolean("automatic", automatic)
        .text("log_level", log_level_names[static_cast<std::size_t>(now.log_level)]).boolean("log_to_file", now.log_to_file)
        .boolean("check_for_updates", now.check_for_updates);
    for (const ColorKey &c : COLOR_KEYS) result.text(c.key, print_color(s.colors.*c.member));
    return result.done();
}

Result<std::string> get_settings(Session &s, const Command &, const Arguments &, const Place &) { return settings_now(s); }

/* the program's settings: the thread count autoX and sweeps use, and what the log shows and whether it goes to the model folder's xppautx.log (settings.json; the log applies
   from the next start, and --verbose and --debug win over it; a color applies to what is drawn or computed after it) */
Result<std::string> configure_settings(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const bool threads = args.has("threads") || args.has("automatic"), log = args.has("log_level") || args.has("log_to_file") || args.has("check_for_updates");
    const bool colors = std::ranges::any_of(COLOR_KEYS, [&](const ColorKey &c) { return args.has(std::string(c.key)); });
    if (!threads && !log && !colors) return fail(command.name, "Give the thread count, the log level, whether the log goes to a file, whether to check for updates or a color", place);
    if ((log || colors) && files::serving_reads()) return fail(command.name, "a recording's replay leaves the user's settings alone", place);
    std::vector<settings::Change> changes;
    unsigned count = s.threads;
    if (threads) {
        count = cpu::THREADS_AUTO;
        if (const auto given = args.optional<long long>("threads")) {
            if (const auto problem = cpu::threads_problem(*given)) return fail(command.name, *problem, place);
            count = static_cast<unsigned>(*given);
        } else if (!args.get<bool>("automatic")) return fail(command.name, "automatic is true; give threads for a count", place);
        /* a recording's replay changes the session's count, never the user's settings */
        if (!files::serving_reads())
            changes.push_back({"general", "threads", count == cpu::THREADS_AUTO ? json_quoted(cpu::THREADS_AUTO_WORD) : std::to_string(count)});
    }
    if (log) {
        if (const auto level = args.optional<std::string>("log_level")) changes.push_back({"general", "log_level", json_quoted(*level)});
        if (const auto file = args.optional<bool>("log_to_file")) changes.push_back({"general", "log_to_file", *file ? "true" : "false"});
        if (const auto check = args.optional<bool>("check_for_updates")) changes.push_back({"updates", "check_for_updates", *check ? "true" : "false"});
    }
    if (colors)
        for (const ColorKey &c : COLOR_KEYS)
            if (const auto color = args.optional<Color>(std::string(c.key))) changes.push_back({"colors", std::string(c.key), json_quoted(print_color(*color))});
    if (!changes.empty()) {
        const Result<> saved = settings::set_settings(*s.model().fs, changes);
        if (!saved) return std::unexpected<Error>(saved.error());
    }
    /* a recording's replay changes the session's count and colors, never the user's settings */
    if (threads) s.threads = count;
    if (colors)
        for (const ColorKey &c : COLOR_KEYS)
            if (const auto color = args.optional<Color>(std::string(c.key))) s.colors.*c.member = *color;
    return settings_now(s);
}

/* settings.set group key value: one value of settings.json, the value as JSON text (a string, a number, true or false, null or an
   array of strings), checked as the file checks it and written; the answer is the value it now has. The keymap has its own commands. */
Result<std::string> set_setting(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string group = args.get<std::string>("group");
    const std::string key = args.get<std::string>("key");
    const std::string value = args.get<std::string>("value");
    if (files::serving_reads()) return fail(command.name, "a recording's replay leaves the user's settings alone", place);
    if (group == "keymap") return fail(command.name, "the keymap is set by the keymap commands", place);
    const Result<> saved = settings::set_settings(*s.model().fs, {{group, key, value}});
    if (!saved) return fail(command.name, saved.error().text(), place);
    return ResultObject().text("group", group).text("key", key).raw("value", value).done();
}

double threads_maximum(const Session &) { return static_cast<double>(cpu::logical_processors()); }

constexpr std::array<const char *, 2> THREADS_ONE = {"threads", "automatic"};
constexpr auto LOG_LEVELS = c_names(log_level_names);
constexpr auto COLOR_ARGUMENTS = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{Argument{.name = COLOR_KEYS[I].key.data(), .type = Type::color, .required = false, .description = COLOR_KEYS[I].description.data()}...};
}(std::make_index_sequence<COLOR_KEYS.size()>{});
constexpr std::array THREAD_AND_LOG_ARGUMENTS = {
    Argument{.name = "threads", .type = Type::integer, .required = false, .min = constant(1), .max = session_quantity("threads_max", threads_maximum),
             .description = "How many threads autoX and a sweep's runs use, at most the computer's logical processors."},
    Argument{.name = "automatic", .type = Type::boolean, .required = false, .description = "Let the program choose: all the physical cores."},
    Argument{.name = "log_level", .type = Type::choice, .required = false, .choices = LOG_LEVELS,
             .description = "What the log shows; it applies from the next start."},
    Argument{.name = "log_to_file", .type = Type::boolean, .required = false,
             .description = "Write the log to xppautx.log in the model folder's .xppautx folder; no is the terminal."},
    Argument{.name = "check_for_updates", .type = Type::boolean, .required = false,
             .description = "Look for a newer release when the window or the browser page opens, at most once a day."}};
constexpr auto THREADS_ARGUMENTS = join(THREAD_AND_LOG_ARGUMENTS, COLOR_ARGUMENTS);
constexpr std::array<Group, 1> THREADS_GROUPS = {Group{GroupRule::at_most_one, THREADS_ONE}};

constexpr auto RESULT_THREADS = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{ResultField{"threads", "integer"}, ResultField{"automatic", "boolean"}, ResultField{"log_level", "choice"}, ResultField{"log_to_file", "boolean"}, ResultField{"check_for_updates", "boolean"},
                      ResultField{COLOR_KEYS[I].key.data(), "text"}...};
}(std::make_index_sequence<COLOR_KEYS.size()>{});
constexpr std::array EVENTS_OPTIONS = {xpp::events::AUTOX_OPTIONS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_BROWSER = {xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array SETTING_ARGUMENTS = {
    Argument{.name = "group", .type = Type::text, .required = true, .description = "The group of settings.json: general, colors, page or updates."},
    Argument{.name = "key", .type = Type::text, .required = true, .description = "The setting's key in that group."},
    Argument{.name = "value", .type = Type::text, .required = true, .description = "The new value as JSON text: a string, a number, true or false, null or an array of strings."}};
constexpr std::array RESULT_SETTING = {ResultField{"group", "text"}, ResultField{"key", "text"}, ResultField{"value", "text"}};

constexpr std::array ROWS = {
    Command{"settings.get", {}, XPP_KIND_CONTROL, CommandCategory::Analysis, "Get settings", "",
            "The program's settings as they stand: the thread count and whether it is automatic, the log level and whether the log goes to the model folder's file.", RESULT_THREADS, EVENTS_BROWSER, false, get_settings, {}, {}},
    Command{"settings.configure", THREADS_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Settings", "File",
            "Set the program's settings, kept in settings.json: the thread count of autoX and of sweeps (a count, or automatic) and what the log shows and whether it goes to xppautx.log in the model folder's .xppautx folder (no: the terminal), and the colors the diagram, the nullclines and the manifolds are drawn with, and whether the window and the page look for a newer release when they open; the log applies from the next start.", RESULT_THREADS, EVENTS_OPTIONS, true, configure_settings, THREADS_GROUPS, {}},
    Command{"settings.set", SETTING_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Set a setting", "",
            "Set one value of settings.json by its group and key, the value as JSON text, checked and written whole; the answer is the value it has now.", RESULT_SETTING, EVENTS_BROWSER, true, set_setting, {}, {}}};

} // namespace

std::span<const Command> settings_commands() { return ROWS; }

} // namespace xpp::command
