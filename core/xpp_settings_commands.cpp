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
#include "xpp_process.h"
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

/* the target of a command's `scope` (the user's file when it gives none), the model folder from the Session, never from the page */
settings::Target target_of(const Session &s, const Arguments &args)
{
    const std::string scope = args.optional<std::string>("scope").value_or(std::string(settings::scope_names[0]));
    return {choice_index(settings::scope_names, scope) == static_cast<int>(settings::Scope::folder) ? settings::Scope::folder : settings::Scope::user, model_folder(s.model())};
}

/* "group.key": "user" or "folder" for each key a file holds, a folder's winning (a key in neither is the default) */
std::string sources_json(const settings::KeysSet &keys, bool effective)
{
    ResultObject sources;
    for (const std::string &key : keys.user) sources.text(key, settings::scope_names[0]);
    if (effective)
        for (const std::string &key : keys.folder) sources.text(key, settings::scope_names[1]);
    return sources.done();
}

/* the settings as they stand: with scope user, the user's file's (no folder on top); else for the model (the user's, the model folder's on top):
   the thread count (and whether it is automatic), what the log shows and whether it goes to the model folder's file, the colors, the page's
   and the update check, and where each key is set */
Result<std::string> settings_now(const Session &s, const Arguments &args)
{
    const settings::Target target = target_of(s, args);
    const bool effective = args.optional<std::string>("scope").value_or("") != settings::scope_names[0];
    const Result<settings::Settings> user = settings::load_settings(*s.model().fs);
    const Result<settings::Settings> kept = effective ? settings::effective_settings(*s.model().fs, user, target.model_folder) : user;
    const settings::Settings defaults;
    const settings::Settings &now = kept ? *kept : defaults;
    const unsigned threads = effective ? s.threads : now.threads;
    const bool automatic = threads == cpu::THREADS_AUTO;
    ResultObject result;
    result.integer("threads", automatic ? AutoxParallel::automatic_threads() : threads).boolean("automatic", automatic)
        .text("log_level", log_level_names[static_cast<std::size_t>(now.log_level)]).boolean("log_to_file", now.log_to_file)
        .boolean("check_for_updates", now.check_for_updates);
    for (const ColorKey &c : COLOR_KEYS) result.text(c.key, print_color(now.colors.*c.member));
    result.text("theme", now.page.theme).boolean("dock_hidden", now.page.dock_hidden);
    result.raw("dock_width", now.page.dock_width ? std::to_string(*now.page.dock_width) : std::string("null"));
    result.integer("threads_max", cpu::logical_processors()).text("scope", settings::scope_names[effective ? 1 : 0]).text("folder", target.model_folder);
    const settings::Target shown{effective ? settings::Scope::folder : settings::Scope::user, target.model_folder};
    result.text("file", settings::target_path(*s.model().fs, shown));
    const Result<std::optional<std::string>> there = settings::existing_file(*s.model().fs, shown);
    result.boolean("file_exists", there && *there);
    result.raw("sources", sources_json(settings::keys_set(*s.model().fs, target.model_folder), effective));
    return result.done();
}

Result<std::string> get_settings(Session &s, const Command &, const Arguments &args, const Place &) { return settings_now(s, args); }

/* what the program runs with once a file has changed: the session's thread count and colors are the effective settings' (a bad file leaves them) */
void apply_effective(Session &s)
{
    const Result<settings::Settings> now = settings::effective_settings(*s.model().fs, settings::load_settings(*s.model().fs), model_folder(s.model()));
    if (!now) return;
    s.threads = now->threads;
    s.colors = now->colors;
}

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
    return settings_now(s, Arguments());
}

/* settings.set group key value [scope]: one value of settings.json (the user's, or with scope folder the model folder's `.xppautx/settings.json`), the value as
   JSON text (a string, a number, true or false, null or an array of strings), checked as the file checks it and written in place; the answer is the
   value it now has. The keymap has its own commands. */
Result<std::string> set_setting(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string group = args.get<std::string>("group");
    const std::string key = args.get<std::string>("key");
    const std::string value = args.get<std::string>("value");
    if (files::serving_reads()) return fail(command.name, "a recording's replay leaves the settings files alone", place);
    if (group == "keymap") return fail(command.name, "the keymap is set by the keymap commands", place);
    const Result<> saved = settings::set_settings(*s.model().fs, {{group, key, value}}, target_of(s, args));
    if (!saved) return fail(command.name, saved.error().text(), place);
    apply_effective(s);
    return ResultObject().text("group", group).text("key", key).raw("value", value).done();
}

/* settings.reset group key [scope]: the key taken out of the file (its comments and the other keys stay): the setting is then the one below it, the user's
   or the default */
Result<std::string> reset_setting(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string group = args.get<std::string>("group");
    const std::string key = args.get<std::string>("key");
    if (files::serving_reads()) return fail(command.name, "a recording's replay leaves the settings files alone", place);
    const Result<> saved = settings::reset_setting(*s.model().fs, target_of(s, args), group, key);
    if (!saved) return fail(command.name, saved.error().text(), place);
    apply_effective(s);
    return ResultObject().text("group", group).text("key", key).boolean("reset", true).done();
}

/* settings.open_file [scope]: the settings file the window shows, in the system's own handler for it. The path is made here from the config folder or the
   model folder, never given; a file that is not there, or a link in its place, is refused (the reader's rule). */
Result<std::string> open_settings_file(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    if (files::serving_reads()) return fail(command.name, "a recording's replay leaves the settings files alone", place);
    const Result<std::optional<std::string>> file = settings::existing_file(*s.model().fs, target_of(s, args));
    if (!file) return fail(command.name, file.error().text(), place);
    if (!*file) return fail(command.name, "No file yet: change a value to make it", place);
    if (const Result<> opened = process::open_with_system(**file); !opened) return fail(command.name, opened.error().text(), place);
    return ResultObject().text("file", **file).done();
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
constexpr auto RESULT_GET = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{ResultField{"threads", "integer"}, ResultField{"automatic", "boolean"}, ResultField{"log_level", "choice"}, ResultField{"log_to_file", "boolean"}, ResultField{"check_for_updates", "boolean"},
                      ResultField{COLOR_KEYS[I].key.data(), "text"}..., ResultField{"theme", "choice"}, ResultField{"dock_hidden", "boolean"}, ResultField{"dock_width", "integer"},
                      ResultField{"threads_max", "integer"}, ResultField{"scope", "choice"}, ResultField{"folder", "text"}, ResultField{"file", "text"}, ResultField{"file_exists", "boolean"}, ResultField{"sources", "map[text, choice]"}};
}(std::make_index_sequence<COLOR_KEYS.size()>{});
constexpr std::array EVENTS_OPTIONS = {xpp::events::AUTOX_OPTIONS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_BROWSER = {xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr auto SCOPES = c_names(settings::scope_names);
constexpr Argument SCOPE_ARGUMENT = {.name = "scope", .type = Type::choice, .required = false, .choices = SCOPES,
                                      .description = "Which file: user is the user's settings.json (the default), folder is the model folder's .xppautx/settings.json."};
constexpr std::array GET_ARGUMENTS = {Argument{.name = "scope", .type = Type::choice, .required = false, .choices = SCOPES,
                                               .description = "user: the user's settings.json alone; folder (the default): what the model runs with, the model folder's file on top of the user's."}};
constexpr std::array GET_ARGUMENTS_USER = {SCOPE_ARGUMENT};
constexpr std::array SETTING_ARGUMENTS = {
    Argument{.name = "group", .type = Type::text, .required = true, .description = "The group of settings.json: general, colors, page or updates."},
    Argument{.name = "key", .type = Type::text, .required = true, .description = "The setting's key in that group."},
    Argument{.name = "value", .type = Type::text, .required = true, .description = "The new value as JSON text: a string, a number, true or false, null or an array of strings."},
    SCOPE_ARGUMENT};
constexpr std::array RESET_ARGUMENTS = {
    Argument{.name = "group", .type = Type::text, .required = true, .description = "The group of settings.json: general, colors, page or updates."},
    Argument{.name = "key", .type = Type::text, .required = true, .description = "The setting's key in that group."},
    SCOPE_ARGUMENT};
constexpr std::array RESULT_OPEN_FILE = {ResultField{"file", "text"}};
constexpr std::array RESULT_RESET = {ResultField{"group", "text"}, ResultField{"key", "text"}, ResultField{"reset", "boolean"}};
constexpr std::array RESULT_SETTING = {ResultField{"group", "text"}, ResultField{"key", "text"}, ResultField{"value", "text"}};

constexpr std::array ROWS = {
    Command{"settings.get", GET_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Analysis, "Get settings", "",
            "The program's settings as they stand: the thread count and whether it is automatic, the log level and whether the log goes to the model folder's file, the colors, the page's, the update check, and where each key is set (user or folder; a key in neither has its default).", RESULT_GET, EVENTS_BROWSER, false, get_settings, {}, {}},
    Command{"settings.configure", THREADS_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Configure settings", "",
            "Set the program's settings, kept in settings.json: the thread count of autoX and of sweeps (a count, or automatic) and what the log shows and whether it goes to xppautx.log in the model folder's .xppautx folder (no: the terminal), and the colors the diagram, the nullclines and the manifolds are drawn with, and whether the window and the page look for a newer release when they open; the log applies from the next start.", RESULT_THREADS, EVENTS_OPTIONS, true, configure_settings, THREADS_GROUPS, {}},
    Command{"settings.set", SETTING_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Set a setting", "",
            "Set one value of the user's settings.json, or of the model folder's .xppautx/settings.json with scope folder, by its group and key, the value as JSON text, checked and written in place (the comments stay); the answer is the value it has now.", RESULT_SETTING, EVENTS_OPTIONS, true, set_setting, {}, {}},
    Command{"settings.open_file", GET_ARGUMENTS_USER, XPP_KIND_VIEW, CommandCategory::Tools, "Open settings file", "",
            "Open the settings file the Settings window shows in the system's own program for it: the user's settings.json, or with scope folder the model folder's .xppautx/settings.json. The path is the program's own; a file that is not there yet is refused.", RESULT_OPEN_FILE, EVENTS_BROWSER, false, open_settings_file, {}, {}},
    Command{"settings.reset", RESET_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Reset a setting", "",
            "Remove one key from the user's settings.json, or from the model folder's .xppautx/settings.json with scope folder: the setting is then the one below it (the user's, the default). The comments and the other keys stay.", RESULT_RESET, EVENTS_OPTIONS, true, reset_setting, {}, {}}};

} // namespace

std::span<const Command> settings_commands() { return ROWS; }

} // namespace xpp::command
