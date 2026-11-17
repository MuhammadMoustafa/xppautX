/* The settings resource's registry rows (W287, W622, W790): the user's settings (xpp_settings.h: settings.json) that the program
   keeps for each run, and the commands that read and set them: `settings.get` reads them, `settings.configure` sets some values of one
   group by their keys (the one way a setting is set: the Settings window, the page's theme, panels and folds, scripts), and
   `settings.reset` takes a key out of a file. The rows are the application's, so they are declared in command_def.h and joined into
   the registry by command_registry.cpp. */
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
#include "xpp_text.h"
#include "model_files.h"
#include "xpp_process.h"
#include "xpp_settings.h"
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

/* the arguments of settings.configure that name the group and its values (the same shape as options.configure) */
constexpr const char *GROUP_KEY = "group", *VALUES_KEY = "values";

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

/* settings.configure group values [scope]: some values of one group of settings.json (the user's, or with scope folder the model folder's
   `.xppautx/settings.json`), each key with its value as JSON text (a string, a number, true or false, null or an array of strings). All
   are checked, then written in one step: a wrong one changes nothing. The answer is the values as written. Not a recording's step:
   the settings are the user's, not the session's, as the keymap's are. The keymap has its own
   commands. */
Result<std::string> configure_settings(Session &s, const Command &command, const Arguments &args, const Place &place)
{
    const std::string group = args.get<std::string>(GROUP_KEY);
    if (files::serving_reads()) return fail(command.name, "a recording's replay leaves the settings files alone", place);
    if (group == "keymap") return fail(command.name, "the keymap is set by the keymap commands", place);
    const std::vector<std::pair<std::string, std::string>> given = args.map<std::string>(VALUES_KEY);
    if (given.empty()) return fail(command.name, "Give at least one value", place);
    std::vector<settings::Change> changes;
    for (const auto &[key, json] : given) changes.push_back({group, key, json});
    const Result<> saved = settings::set_settings(*s.model().fs, changes, target_of(s, args));
    if (!saved) return std::unexpected<Error>(saved.error()); /* the file's error keeps its file and line */
    apply_effective(s);
    std::string values = "{";
    for (const auto &[key, json] : given) {
        if (values.size() > 1) values += ',';
        json_append_string(values, key);
        values += ':' + json;
    }
    return ResultObject().text(GROUP_KEY, group).raw(VALUES_KEY, values + '}').done();
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

constexpr auto RESULT_GET = []<std::size_t... I>(std::index_sequence<I...>) {
    return std::array{ResultField{"threads", "integer"}, ResultField{"automatic", "boolean"}, ResultField{"log_level", "choice"}, ResultField{"log_to_file", "boolean"}, ResultField{"check_for_updates", "boolean"},
                      ResultField{COLOR_KEYS[I].key.data(), "text"}..., ResultField{"theme", "choice"}, ResultField{"dock_hidden", "boolean"}, ResultField{"dock_width", "integer"},
                      ResultField{"threads_max", "integer"}, ResultField{"scope", "choice"}, ResultField{"folder", "text"}, ResultField{"file", "text"}, ResultField{"file_exists", "boolean"}, ResultField{"sources", "map[text, choice]"}};
}(std::make_index_sequence<COLOR_KEYS.size()>{});
constexpr std::array EVENTS_OPTIONS = {xpp::events::AUTOX_OPTIONS, xpp::events::SESSION_STATE, xpp::events::DONE};
constexpr std::array EVENTS_BROWSER = {xpp::events::SESSION_STATE, xpp::events::DONE};

constexpr auto SCOPES = c_names(settings::scope_names);
constexpr auto GROUP_CHOICES = c_names(settings::GROUPS);
constexpr Argument SCOPE_ARGUMENT = {.name = "scope", .type = Type::choice, .required = false, .choices = SCOPES,
                                      .description = "Which file: user is the user's settings.json (the default), folder is the model folder's .xppautx/settings.json."};
constexpr std::array GET_ARGUMENTS = {Argument{.name = "scope", .type = Type::choice, .required = false, .choices = SCOPES,
                                               .description = "user: the user's settings.json alone; folder (the default): what the model runs with, the model folder's file on top of the user's."}};
constexpr std::array GET_ARGUMENTS_USER = {SCOPE_ARGUMENT};
constexpr std::array CONFIGURE_ARGUMENTS = {
    Argument{.name = GROUP_KEY, .type = Type::choice, .required = true, .choices = GROUP_CHOICES, .description = "The group of settings.json the values are in."},
    Argument{.name = VALUES_KEY, .type = Type::map, .required = true, .max_length = MAX_COMMAND_BYTES, .element = Type::text, .max_items = MAX_ITEMS,
             .description = "Each key of the group with its value as JSON text; a key's label, help and limits are in hello's settings rows."},
    SCOPE_ARGUMENT};
constexpr std::array RESET_ARGUMENTS = {
    Argument{.name = "group", .type = Type::text, .required = true, .description = "The group of settings.json: general, colors, page or updates."},
    Argument{.name = "key", .type = Type::text, .required = true, .description = "The setting's key in that group."},
    SCOPE_ARGUMENT};
constexpr std::array RESULT_OPEN_FILE = {ResultField{"file", "text"}};
constexpr std::array RESULT_RESET = {ResultField{"group", "text"}, ResultField{"key", "text"}, ResultField{"reset", "boolean"}};
constexpr std::array RESULT_CONFIGURE = {ResultField{GROUP_KEY, "text"}, ResultField{VALUES_KEY, "map[text, text]"}};

constexpr std::array ROWS = {
    Command{"settings.get", GET_ARGUMENTS, XPP_KIND_CONTROL, CommandCategory::Analysis, "Get settings", "",
            "The program's settings as they stand: the thread count and whether it is automatic, the log level and whether the log goes to the model folder's file, the colors, the page's, the update check, and where each key is set (user or folder; a key in neither has its default).", RESULT_GET, EVENTS_BROWSER, false, get_settings, {}, {}},
    Command{"settings.configure", CONFIGURE_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Configure settings", "",
            "Set some values of one group of the user's settings.json, or of the model folder's .xppautx/settings.json with scope folder: each key with its value as JSON text, all checked, then written in one step, in place (the comments stay); the answer is the values as written.", RESULT_CONFIGURE, EVENTS_OPTIONS, false, configure_settings, {}, {}},
    Command{"settings.open_file", GET_ARGUMENTS_USER, XPP_KIND_VIEW, CommandCategory::Tools, "Open settings file", "",
            "Open the settings file the Settings window shows in the system's own program for it: the user's settings.json, or with scope folder the model folder's .xppautx/settings.json. The path is the program's own; a file that is not there yet is refused.", RESULT_OPEN_FILE, EVENTS_BROWSER, false, open_settings_file, {}, {}},
    Command{"settings.reset", RESET_ARGUMENTS, XPP_KIND_OPTION, CommandCategory::Analysis, "Reset a setting", "",
            "Remove one key from the user's settings.json, or from the model folder's .xppautx/settings.json with scope folder: the setting is then the one below it (the user's, the default). The comments and the other keys stay.", RESULT_RESET, EVENTS_OPTIONS, false, reset_setting, {}, {}}};

} // namespace

std::span<const Command> settings_commands() { return ROWS; }

} // namespace xpp::command
