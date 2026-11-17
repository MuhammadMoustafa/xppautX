#pragma once
/* The user's settings and the program's state (W790, Discussion #873), in
   the per-user config folder (files::config_dir):

   settings.json, JSON with comments (`//` and slash-star), what the user
   sets: how the program runs and looks.
     general  {threads, log_level, log_to_file}
     colors   {the eight diagram colors}
     keymap   {bindings, pinned}                 (xpp_keymap.h)
     page     {theme, dock_hidden, dock_width, folds}
     updates  {check_for_updates}
   state.json, what the program records itself (never edited by hand):
     recent_models, last_update_check, outside_paths.
   Both carry `"version": 4` (format_version.h) and nothing else at the top.

   One owner for the two files (maintainer: "Our files load all or nothing",
   W125): the reader reads the whole text, checks every key and value, and
   only then hands the values over; a bad value is an error naming the file,
   its line and the value, and nothing of that file is used. A missing file is
   the defaults. A change to settings.json edits the one value in place,
   keeping the file's comments, order and spacing: the new file is checked
   whole before it is written (temp then rename, FileSystem::write_config), so
   a bad file is never overwritten. state.json is written whole. C++ only.

   A model's folder may hold `.xppautx/settings.json` (W791), the same syntax and the same reader: its keys are put on top of
   the user's one by one, every key but updates.check_for_updates (what a folder sends must not turn the update check on or
   off). The settings a model runs with are effective_settings. settings.set and the commands built on it write the user's file only.

   A group or key is added by one row of the readers below and one of the
   writer's; the tests show both. */
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "colormap.h"
#include "model_file_types.h"
#include "xpp_cpu.h"
#include "xpp_error.h"
#include "xpp_keymap.h"
#include "xpp_log.h"

namespace xpp {
class FileSystem; /* xpp_fs.h */
}

namespace xpp::settings {

inline constexpr std::string_view SETTINGS_FILE = "settings.json";
inline constexpr std::string_view STATE_FILE = "state.json";

/* what a file may hold: the longest list at its maximum with room for the
   groups; the reader refuses a longer file before it reads it */
inline constexpr std::size_t MAX_FILE_BYTES = 256 * 1024;

/* how many models are kept: the start screen lists them all at once, and a
   longer list is a search, which the Open model dialog does better */
inline constexpr std::size_t MAX_RECENT_MODELS = 10;
/* the longest path a file keeps: PATH_MAX on Linux and macOS; a Windows path
   past it needs the \?\ form no model's folder uses */
inline constexpr std::size_t MAX_PATH_BYTES = 4096;
/* how many of the user's "Allow always" answers about a path outside a
   model's folder are kept (W286): when it is full the oldest goes, and asks again */
inline constexpr std::size_t MAX_OUTSIDE_APPROVALS = 24;
/* the page's folded Values sections: a section id is short, and a page has a few dozen */
inline constexpr std::size_t MAX_FOLDS = 256;
inline constexpr std::size_t MAX_FOLD_BYTES = 128;
/* the Values panel's width in CSS px, when the user sets one: the page's own limits (web2 PANEL_LIMITS) */
inline constexpr int MIN_DOCK_WIDTH = 288;
inline constexpr int MAX_DOCK_WIDTH = 800;
/* the page's color themes: each a word of the setting `theme` */
inline constexpr std::array<std::string_view, 3> THEMES = {"system", "light", "dark"};

struct Page {
    std::string theme = "system";
    bool dock_hidden = false;
    /* nullopt: the stylesheet's size */
    std::optional<int> dock_width;
    /* the ids of the folded sections, in the order the page keeps them */
    std::vector<std::string> folds;
};

/* settings.json's values */
struct Settings {
    /* cpu::THREADS_AUTO, or the number of threads autoX's collocation and a sweep's runs use */
    unsigned threads = cpu::THREADS_AUTO;
    /* what the log shows (log_level_names) and whether it goes to xppautx.log in the model folder's `.xppautx` (else stderr);
       --verbose and --debug win over the level */
    XppLogLevel log_level = XPP_LOG_WARN;
    bool log_to_file = false;
    Colors colors;
    keymap::Keymap keymap;
    Page page;
    /* whether a newer release is looked for when the window or the browser page opens, at most once a day (W622) */
    bool check_for_updates = true;
};

/* state.json's values */
struct State {
    /* absolute paths of the models opened lately, newest first, none twice */
    std::vector<std::string> recent_models;
    /* when the last update check began (utc_now's text, "" for never) */
    std::string last_update_check;
    /* the "Allow always" answers, newest first, none twice */
    std::vector<OutsideApproval> outside_paths;
};

/* both files as a start reads them: each all or nothing, the defaults for a missing file */
struct Read {
    Result<Settings> settings;
    Result<State> state;
};

/* the text of settings.json (or state.json) in `text`, the contents of `file` (the name its errors give): the value of each
   key it names, the defaults for the rest; a key that is not one of the file's, a value of the wrong kind, a list longer than its
   maximum, a path or time that is not one, is an error at that value, and nothing of the file is used */
Result<Settings> parse_settings(std::string_view text, std::string file);
/* the text of a model folder's settings.json on top of `user`: the keys it holds replace the user's (a list, bindings or pinned, whole),
   the rest stay; updates.check_for_updates is an error at its line there. All or nothing, as parse_settings. */
Result<Settings> parse_folder_settings(std::string_view text, std::string file, const Settings &user);
Result<State> parse_state(std::string_view text, std::string file);
/* state.json's text for `s` (parse_state gives it back), or the error for a value that cannot be written */
Result<std::string> serialize_state(const State &s, std::string_view file);

/* the paths of the two files ("" when the system names no config folder) */
std::string settings_path(FileSystem &fs);
std::string state_path(FileSystem &fs);
/* the settings a model runs with: `user` (the user's file as the program read it, its error when it is bad), then the keys of the
   settings.json in the `.xppautx` of model_folder ("" for none) on top. A folder's file that is bad (or a link, or too long) is the
   error and nothing of it is used. */
Result<Settings> effective_settings(FileSystem &fs, const Result<Settings> &user, std::string_view model_folder);
/* both files as the program's start reads them */
Read read_all(FileSystem &fs);
/* one of the two files as a load reads it: the defaults for a missing file, the error when it is bad */
Result<Settings> load_settings(FileSystem &fs);
Result<State> load_state(FileSystem &fs);
/* settings.json as the page keeps it: hello's `settings` object, {"ok", "path", "page": {...}} and, when the file is bad, its error
   fields and the defaults of the page */
std::string hello_json(FileSystem &fs, const Result<Settings> &loaded);

/* one value of settings.json: its group and key, and the value as JSON text (a string, a number, true or false, null, an array of
   strings) */
struct Change {
    std::string group;
    std::string key;
    std::string json;
};
/* applies the changes to settings.json in one step: each is checked (the file's kind, the key's and value's rules) and the whole
   file is checked again before it is written; a file that is bad is the error and is not overwritten, a change that is wrong is the
   error and nothing is written. A missing file is made. */
Result<> set_settings(FileSystem &fs, const std::vector<Change> &changes);
/* the keymap as one whole change of settings.json's `keymap` group, checked as the keymap the user sent (its errors name it) */
Result<> set_keymap(FileSystem &fs, const keymap::Keymap &map);
/* the keymap alone, checked the same way without writing (for a keymap a page sends) */
Result<> check_keymap(const keymap::Keymap &map);
/* load, change, save: a bad file is the error, left as it is */
Result<> update_state(FileSystem &fs, const std::function<void(State &)> &change);

/* whether the opening's check for a newer release is due now (the setting is on and none began today, UTC); when it is, the
   check is recorded as begun, so a failed one waits for tomorrow too. A bad file is the error. */
Result<bool> update_check_due(FileSystem &fs);

/* the "Allow always" answers of a load (model_file_types.h): the kept ones from `read` (the state the program's start read, or
   null to read it now) or the file's error when it is bad, and `keep`, which remembers an answer as it is given */
LoadServices load_services(FileSystem &fs, const Read *read);

/* item first in list: an equal one earlier is removed, and the oldest dropped past max (the one rule of every newest-first list here) */
template <class T> void remember_newest(std::vector<T> &list, T item, std::size_t max)
{
    std::erase(list, item);
    list.insert(list.begin(), std::move(item));
    if (list.size() > max) list.erase(list.begin() + static_cast<std::ptrdiff_t>(max), list.end());
}

} // namespace xpp::settings
