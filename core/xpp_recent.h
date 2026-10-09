#ifndef XPP_RECENT_H
#define XPP_RECENT_H
/* recent.txt (W232): the models the user opened lately, kept by the program
   for the user in the per-user config folder (files::config_dir), newest
   first; the start screen lists them (hello's `start`, docs/protocol.md
   "Start screen"). C++ only.

   One absolute path per line, nothing else: no blank line, no comment, no
   control character, no repeated path. The file loads all or nothing like
   our other files (W125): a line that is wrong is an error naming the file,
   the line and the path, and nothing of it is used, so the start screen
   lists none and says why; the next Open model does not overwrite a file it
   could not read. A path in it is only ever opened through File > Open
   model's one owner (model_switch.h), which checks it again; a path that is
   no longer there is listed as missing, never dropped. */
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::recent {

inline constexpr std::string_view FILE_NAME = "recent.txt";

/* how many models are kept: the start screen lists them all at once, and a
   longer list is a search, which the Open model dialog does better */
inline constexpr std::size_t MAX_ENTRIES = 10;
/* the longest path of an entry: PATH_MAX on Linux and macOS; a Windows path
   past it needs the \?\ form no model's folder uses. A hostile file cannot
   make the reader take more than MAX_ENTRIES lines of this length. */
inline constexpr std::size_t MAX_PATH_BYTES = 4096;
/* what a file may hold: every entry at its longest, with its line end */
inline constexpr std::size_t MAX_FILE_BYTES = MAX_ENTRIES * (MAX_PATH_BYTES + 2);

/* The entries in `text`, the contents of `file` (the name its errors give),
   all or nothing: a line that is empty, longer than MAX_PATH_BYTES, with a
   control character, not an absolute path or already listed, or more than
   MAX_ENTRIES lines, is an error at that line naming it. */
Result<std::vector<std::string>> parse_recent(std::string_view text, std::string file);
/* the file's text for these entries: parse_recent(serialize_recent(e)) is e */
std::string serialize_recent(const std::vector<std::string> &entries);

/* the config folder's recent.txt ("" when there is no config folder) */
std::string recent_path();
/* the entries of the file: none when it is not there, the error when it is bad */
Result<std::vector<std::string>> load_recent();
/* `model` (an absolute path) first in the file, the rest after it without a
   repeat of it and cut at MAX_ENTRIES. The file is read first: a bad one is
   the error, left as it is. Nothing is kept while a recording plays
   (files::serving_reads): that is not the user opening it. */
Result<> note(std::string_view model);

/* hello's `start` object for a session with no model: {"path":..., "limit":N,
   "recent":[{"path":..., "missing":bool}, ...]} and, when the file is bad,
   "error" (the common error fields) with no entries */
std::string start_json(const Result<std::vector<std::string>> &loaded);

} // namespace xpp::recent

#endif
