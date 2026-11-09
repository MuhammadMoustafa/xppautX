#pragma once
/* The program's preferences (W287): what xppautX keeps for the user across
   runs, in one file of the per-user config folder (files::config_dir),
   preferences.odex, in .odex syntax: `version` and one `preferences { ... }` statement, accepted in this file
   alone (odex.h's read_state_file, docs/odex.md "Preferences").
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
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "colormap.h"
#include "xpp_error.h"
#include "xpp_log.h"

namespace xpp::prefs {

inline constexpr std::string_view FILE_NAME = "preferences.odex";

/* what a file may hold: every list at its longest with room for quoting and
   the groups still to come; the reader refuses a longer one before reading it */
inline constexpr std::size_t MAX_FILE_BYTES = 256 * 1024;

/* how many models are kept: the start screen lists them all at once, and a
   longer list is a search, which the Open model dialog does better */
inline constexpr std::size_t MAX_RECENT_MODELS = 10;
/* the longest path of a recent model: PATH_MAX on Linux and macOS; a Windows
   path past it needs the \?\ form no model's folder uses */
inline constexpr std::size_t MAX_PATH_BYTES = 4096;

/* the thread count of autoX and sweeps (W267, W434): the file says "auto" or a whole number from 1
   to the logical processors (xpp::cpu::logical_processors); THREADS_AUTO
   in the Preferences is "auto", the count AutoxParallel::automatic_threads
   gives. Any count gives the same results (W247). */
inline constexpr unsigned THREADS_AUTO = 0;
inline constexpr std::string_view THREADS_AUTO_WORD = "auto";
/* why n cannot be the thread count, or nothing; the one check of the file's
   reader and writer and of the protocol's `set` */
std::optional<std::string> threads_problem(long long n);

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

/* the colours the program draws with (W655): the user's, not a model's, one list for every model and every view. Each is
   read when something is drawn with it: a diagram point's by its kind when it is stored, a nullcline's and a manifold's
   when it is computed. The defaults are XPPAUT's. The names are the keys of the file and the arguments of
   preferences.configure; their order is the option table's order of the settings they were (the diagram's, the nullclines',
   the manifolds'). */
struct Colors {
    Color stable_equilibrium = xppaut_color(1), unstable_equilibrium = xppaut_color(0);
    Color stable_periodic = xppaut_color(7), unstable_periodic = xppaut_color(9);
    Color x_nullcline = xppaut_color(2), y_nullcline = xppaut_color(7);
    Color stable_manifold = xppaut_color(8), unstable_manifold = xppaut_color(5);
};

/* one colour's key (the file's, preferences.configure's argument and result field) and what it is: the one list the
   file, the command and the dialog read */
struct ColorKey {
    std::string_view key;
    Color Colors::*member;
    std::string_view description;
};
inline constexpr std::array COLOR_KEYS = {
    ColorKey{"stable_equilibrium_color", &Colors::stable_equilibrium, "The colour of the stable equilibria in the continuation diagram."},
    ColorKey{"unstable_equilibrium_color", &Colors::unstable_equilibrium, "The colour of the unstable equilibria in the continuation diagram."},
    ColorKey{"stable_periodic_color", &Colors::stable_periodic, "The colour of the stable periodic orbits in the continuation diagram."},
    ColorKey{"unstable_periodic_color", &Colors::unstable_periodic, "The colour of the unstable periodic orbits in the continuation diagram."},
    ColorKey{"x_nullcline_color", &Colors::x_nullcline, "The colour of the x nullcline."},
    ColorKey{"y_nullcline_color", &Colors::y_nullcline, "The colour of the y nullcline."},
    ColorKey{"stable_manifold_color", &Colors::stable_manifold, "The colour of the stable manifold of a saddle."},
    ColorKey{"unstable_manifold_color", &Colors::unstable_manifold, "The colour of the unstable manifold of a saddle."}};

struct Preferences {
    /* absolute paths of the models opened lately, newest first, none twice */
    std::vector<std::string> recent_models;
    /* THREADS_AUTO, or the number of threads autoX's collocation and a sweep's runs use */
    unsigned threads = THREADS_AUTO;
    /* what the log shows (xpp::log_level_names) and the file it goes to ("" is stderr, an absolute path): the user's, not a
       model's (W619); --verbose and --debug win over them */
    XppLogLevel log_level = XPP_LOG_WARN;
    std::string log_file;
    Colors colors;
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

/* the config folder's preferences.odex ("" when there is no config folder) */
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

