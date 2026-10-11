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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

/* AUTO's thread count (W267): the file says "auto" or a whole number from 1
   to the logical processors (xpp::cpu::logical_processors); AUTO_THREADS_AUTO
   in the Preferences is "auto", the count AutoParallel::automatic_threads
   gives. Any count gives the same results (W247). */
inline constexpr unsigned AUTO_THREADS_AUTO = 0;
inline constexpr std::string_view AUTO_THREADS_WORD = "auto";
/* why n cannot be AUTO's thread count, or nothing; the one check of the file's
   reader and writer and of the protocol's `set` */
std::optional<std::string> auto_threads_problem(long long n);

/* how many of the user's "Allow always" answers about a path outside a
   model's folder are kept (W286): each is two paths of at most MAX_PATH_BYTES
   and the file holds MAX_FILE_BYTES, with the recent models and the groups
   to come; when it is full the oldest answer goes, and asks again */
inline constexpr std::size_t MAX_OUTSIDE_APPROVALS = 24;

/* the user's answer "Allow always" for one model file and one path it names
   outside its folder (both absolute; the path as the program resolved it,
   files::resolve_path): a changed path is another question */
struct OutsideApproval {
    std::string model, path;
    bool operator==(const OutsideApproval &) const = default;
};

struct Preferences {
    /* absolute paths of the models opened lately, newest first, none twice */
    std::vector<std::string> recent_models;
    /* AUTO_THREADS_AUTO, or the number of threads AUTO's collocation uses */
    unsigned auto_threads = AUTO_THREADS_AUTO;
    /* newest first, none twice */
    std::vector<OutsideApproval> outside_paths;
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
/* whether the user answered "Allow always" for model naming path (a bad file
   is the error) */
Result<bool> outside_path_approved(std::string_view model, std::string_view path);
/* the user's "Allow always": remembered first, the oldest answer dropped when
   MAX_OUTSIDE_APPROVALS are kept, a repeated one moved to the front */
Result<> approve_outside_path(std::string_view model, std::string_view path);
/* item first in list: an equal one earlier is removed, and the oldest
   dropped past max (the one rule of every newest-first list here) */
template <class T> void remember_newest(std::vector<T> &list, T item, std::size_t max)
{
    std::erase(list, item);
    list.insert(list.begin(), std::move(item));
    if (list.size() > max) list.erase(list.begin() + static_cast<std::ptrdiff_t>(max), list.end());
}

/* load, change, save: a bad file is the error, left as it is */
Result<> update_prefs(const std::function<void(Preferences &)> &change);

} // namespace xpp::prefs

#endif
