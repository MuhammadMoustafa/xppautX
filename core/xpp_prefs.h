#ifndef XPP_PREFS_H
#define XPP_PREFS_H
/* The program's preferences (W287): what xppautX keeps for the user across
   runs, in one file of the per-user config folder (files::config_dir),
   preferences.setx, in the settings syntax (setx.h, docs/odex.md ".setx").
   It is not a model's setting: a run's results never depend on what it
   holds (the recent models are shown, a later group is a convenience).

   One owner for the file: this module reads it whole, checks every value,
   and only then hands the Preferences over (W125); it writes it whole,
   temp-then-rename (write_config_file). A missing file is the defaults; a
   bad one is an error naming the file, the line and the value, and nothing
   of it is used; it is never overwritten while it is bad. Every list in it
   has a maximum and the file a byte limit, so a hostile file cannot make an
   unbounded allocation. C++ only.

   A group is added by a member of Preferences and a line in `describe`
   (xpp_prefs.cpp), which serves the reader and the writer alike: neither
   changes. */
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::prefs {

inline constexpr std::string_view FILE_NAME = "preferences.setx";

/* what a file may hold: every list at its longest with room for quoting and
   the groups still to come; the reader refuses a longer one before reading it */
inline constexpr std::size_t MAX_FILE_BYTES = 256 * 1024;

/* how many models are kept: the start screen lists them all at once, and a
   longer list is a search, which the Open model dialog does better */
inline constexpr std::size_t MAX_RECENT_MODELS = 10;
/* the longest path of a recent model: PATH_MAX on Linux and macOS; a Windows
   path past it needs the \?\ form no model's folder uses */
inline constexpr std::size_t MAX_PATH_BYTES = 4096;

struct Preferences {
    /* absolute paths of the models opened lately, newest first, none twice */
    std::vector<std::string> recent_models;
};

/* the preferences in `text`, the contents of `file` (the name its errors
   give): the keys it names, the defaults for the rest. A key that is not a
   preference, a value of the wrong kind, a list longer than its maximum, a
   path that is empty, too long, with a control character, not absolute or
   already listed, is an error at that value. */
Result<Preferences> parse_prefs(std::string_view text, std::string file);
/* the file's text for these preferences (parse of it gives them back), or
   the error for a value that cannot be written */
Result<std::string> serialize_prefs(const Preferences &p);

/* the config folder's preferences.setx ("" when there is no config folder) */
std::string prefs_path();
/* the file's preferences: the defaults when it is not there, the error when
   it is bad or there is no config folder */
Result<Preferences> load_prefs();
/* writes them, making the folder */
Result<> save_prefs(const Preferences &p);
/* load, change, save: a bad file is the error, left as it is */
Result<> update_prefs(const std::function<void(Preferences &)> &change);

} // namespace xpp::prefs

#endif
