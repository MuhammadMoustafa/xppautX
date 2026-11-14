/* The preferences resource's registry rows (W287, W622): the program's own settings, kept in its preferences file (xpp_prefs.h), which
   is the application's, so the rows are too (declared in command_def.h, joined into the registry by command_registry.cpp). */
#include "command_def.h"
#include "autox_parallel.h"
#include "colormap.h"
#include "command_category.h"
#include "event_registry.h"
#include "model_options.h"
#include "session.h"
#include "xpp_cpu.h"
#include "xpp_error.h"
#include "xpp_files.h"
#include "xpp_log.h"
#include "xpp_prefs.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace xpp::command {
namespace {

/* the preferences as they stand: the thread count (and whether it is automatic), what the log shows and the file it goes to, and the colors */
Result<std::string> preferences_now(const Session &s)
{
    const Result<prefs::Preferences> kept = prefs::load_prefs();
    const bool automatic = s.threads == cpu::THREADS_AUTO;
    ResultObject result;
    result.integer("threads", automatic ? AutoxParallel::automatic_threads() : s.threads).boolean("automatic", automatic)
        .text("log_level", log_level_names[static_cast<std::size_t>(kept ? kept->log_level : XPP_LOG_WARN)]).text("log_file", kept ? kept->log_file : std::string())
        .boolean("check_for_updates", kept ? kept->check_for_updates : prefs::Preferences{}.check_for_updates);
    for (const ColorKey &c : COLOR_KEYS) result.text(c.key, print_color(s.colors.*c.member));
    return result.done();
}

Result<std::string> get_preferences(Session &s, const Command &, const Arguments &, const Place &) { return preferences_now(s); }

/* the program's preferences: the thread count autoX and sweeps use, and what the log shows and the file it goes to (kept in
   preferences.odex; the log applies from the next start, and --verbose and --debug win over it; a color applies to what is drawn or computed after it) */
Result<std::string> configure_preferences(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const bool threads = args.has("threads") || args.has("automatic"), log = args.has("log_level") || args.has("log_file") || args.has("check_for_updates");
    const bool colors = std::ranges::any_of(COLOR_KEYS, [&](const ColorKey &c) { return args.has(std::string(c.key)); });
    if (!threads && !log && !colors) return fail(command.name, "Give the thread count, the log level, the log file, whether to check for updates or a color", place);
    if ((log || colors) && files::serving_reads()) return fail(command.name, "a recording's replay leaves the user's preferences alone", place);
    if (threads) {
        unsigned count = cpu::THREADS_AUTO;
        if (const auto given = args.optional<long long>("threads")) {
            if (const auto problem = cpu::threads_problem(*given)) return fail(command.name, *problem, place);
            count = static_cast<unsigned>(*given);
        } else if (!args.get<bool>("automatic")) return fail(command.name, "automatic is true; give threads for a count", place);
        /* a recording's replay changes the session's count, never the user's preferences */
        if (!files::serving_reads()) {
            const Result<> saved = prefs::update_prefs([&](prefs::Preferences &p) { p.threads = count; });
            if (!saved) return fail(command.name, saved.error().text(), place);
        }
        s.threads = count;
    }
    if (log) {
        const Result<> saved = prefs::update_prefs([&](prefs::Preferences &p) {
            if (const auto level = args.optional<std::string>("log_level")) p.log_level = static_cast<XppLogLevel>(choice_index(log_level_names, *level));
            if (const auto file = args.optional<std::string>("log_file")) p.log_file = *file;
            if (const auto check = args.optional<bool>("check_for_updates")) p.check_for_updates = *check;
        });
        if (!saved) return std::unexpected<Error>(saved.error());
    }
    if (colors) {
        /* a recording's replay changes nothing of the user's (refused above) */
        for (const ColorKey &c : COLOR_KEYS)
            if (const auto color = args.optional<Color>(std::string(c.key))) s.colors.*c.member = *color;
        const Result<> saved = prefs::update_prefs([&](prefs::Preferences &p) { p.colors = s.colors; });
        if (!saved) return std::unexpected<Error>(saved.error());
    }
    return preferences_now(s);
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
    Argument{.name = "log_file", .type = Type::text, .required = false, .description = "The file the log goes to, an absolute path; empty is the terminal."},
    Argument{.name = "check_for_updates", .type = Type::boolean, .required = false,
             .description = "Look for a newer release when the window or the browser page opens, at most once a day."}};
constexpr auto THREADS_ARGUMENTS = join(THREAD_AND_LOG_ARGUMENTS, COLOR_ARGUMENTS);
constexpr std::array<Group, 1> THREADS_GROUPS = {Group{GroupRule::at_most_one, THREADS_ONE}};

constexpr auto RESULT_THREADS = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{ResultField{"threads", "integer"}, ResultField{"automatic", "boolean"}, ResultField{"log_level", "choice"}, ResultField{"log_file", "text"}, ResultField{"check_for_updates", "boolean"},
                      ResultField{COLOR_KEYS[I].key.data(), "text"}...};
}(std::make_index_sequence<COLOR_KEYS.size()>{});
constexpr std::array EVENTS_SETTINGS = {xpp::events::AUTOX_SETTINGS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_BROWSER = {xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr std::array ROWS = {
    Command{"preferences.get", {}, XPP_KIND_CONTROL, CommandCategory::Analysis, "Get preferences", "",
            "The program's preferences as they stand: the thread count and whether it is automatic, the log level and the log file.", RESULT_THREADS, EVENTS_BROWSER, false, get_preferences, {}, {}},
    Command{"preferences.configure", THREADS_ARGUMENTS, XPP_KIND_SETTING, CommandCategory::Analysis, "Preferences", "File",
            "Set the program's preferences, kept in its preferences file: the thread count of autoX and of sweeps (a count, or automatic) and what the log shows and the file it goes to (empty: the terminal), and the colors the diagram, the nullclines and the manifolds are drawn with, and whether the window and the page look for a newer release when they open; the log applies from the next start.", RESULT_THREADS, EVENTS_SETTINGS, true, configure_preferences, THREADS_GROUPS, {}}};

} // namespace

std::span<const Command> preferences_commands() { return ROWS; }

} // namespace xpp::command
