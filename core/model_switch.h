#pragma once
/* File > Open model and File > Reload (W61), and File > Quit's question
   (W59d): every way of leaving a session asks the one question,
   xpp_session_may_leave. Open model and Reload: another model, or the same
   file again, loaded in this process. The command asks what it needs and
   leaves a request in the Session; the front end carries it out once the
   command has returned, when nothing of the Session before is in use
   (ui_json.cpp handle_line, json_model.cpp): load_requested builds the new
   Model and Session through xpp::load_model, and a load that fails leaves
   the ones before current, untouched, and says so. */

#include "model_numerics.h"
#include "autox_options.h"
#include "model_files.h"
#include "xpp_session.h"
#include "model_request.h"
#include "xpp_leave.h"
#include "xpp_error.h"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xpp {
struct Session; /* session.h */
class FileSystem; /* xpp_fs.h */
}

/* What File > Open model asks of the program's own services, below the
   Session: the recent models and the bundled examples (xpp_recent.cpp,
   xpp_examples.cpp, the application's), which implement these. */
namespace xpp::recent {
/* the entries of state.json: none when it is not there, the error when it is bad */
Result<std::vector<std::string>> load_recent(FileSystem &fs);
/* `model` (an absolute path) first in the file, the rest after it without a
   repeat of it and cut at settings::MAX_RECENT_MODELS. The file is read first: a bad one is
   the error, left as it is. Nothing is kept while a recording plays
   (files::serving_reads): that is not the user opening it. */
Result<> note(FileSystem &fs, std::string_view model);
} // namespace xpp::recent
namespace xpp::examples {
/* The copy of the example `name` in the config folder's examples/, made
   (the folder too) after asking before replacing one that is there (the
   Replace question of every save, browse.h open_sink_asking): its path,
   or "" when the user declined (nothing is changed). An error when `name` is
   not in list() (a path, "..", a link, a file that is not there are all
   that), when it cannot be read or when there is no config folder. */
Result<std::string> install(FileSystem &fs, std::string_view name);
} // namespace xpp::examples

/* File > Open model (key m) and {"cmd":"open"}: path, or the file the
   user picks when it is NULL or empty; asks before this model goes,
   offering to save its session first (xpp_session_save). A file that
   carries a model (.snapx, xpp_session.h) always loads its saved model,
   then the whole session, after asking whether to save first.
   A recording (.recx) opens in the player (play_recording, W59b). True
   when a model (or recording) was asked for; false when a question was
   declined or the reader of a session file showed why; the error when the
   file cannot be read at all (the caller shows or returns it). */
xpp::Result<bool> xpp_model_open(xpp::Session &s, const char *path);
/* Open model of a bundled example (W233, {"cmd":"open","example":name}):
   `name` is one of the examples beside the program (xpp_examples.h), copied
   into the user's config folder (asking before it replaces a copy that is
   there; declining ends it, no change), and that copy opened by
   xpp_model_open, the one owner. Says where the copy is; as xpp_model_open
   answers, and the error when the copy cannot be made. */
xpp::Result<bool> xpp_model_open_example(xpp::Session &s, std::string_view name);
/* File > Reload (key e) and {"cmd":"reload"}: asks first, as Open model
   does, then the model's file again, with
   the command line it was loaded with (a saved model's from its saved
   files); the parameters, initial data, numerics and autoX options keep their values by
   name (restore_values). True when it asked for the load, false when the
   question was declined, the error when the model was not read from a file */
xpp::Result<bool> xpp_model_reload(xpp::Session &s);
/* The one question before the session s goes (W59d): question, answered
   Save session (key s), Don't save (d) or Cancel (Esc). Save saves the
   session first (xpp_session_save, its file asked), and with_recording
   the recording in progress too (save_recording, its name asked, as its
   stop asks). True when the session may go: false when the user cancels,
   or a save is cancelled or fails. */
bool xpp_session_may_leave(xpp::Session &s, const std::string &question, bool with_recording);
/* the leave question already answered by the page (session.quit's, update.restart's and update.install's save, W689): save true
   saves the session (and the recording in progress when with_recording) and says it may go; false says it may go at once */
bool xpp_session_leave_saved(xpp::Session &s, bool save, bool with_recording);
/* before file replaces the model of s: xpp_session_may_leave, the question
   naming file */
bool xpp_model_may_leave(xpp::Session &s, const std::string &file);
/* File > Quit (F Q), and the protocol's quit that asks (the desktop
   window's File > Quit and its close box while the core is idle):
   xpp::quit_question, then exits (bye_bye) unless cancelled; a recording
   in progress is saved with the session. saving: the question was asked
   already, by the page, and answered Save session (W110: the page asks
   while a computation runs, {"cmd":"quit","save":true}); a save cancelled
   or failing keeps the session. */
void xpp_quit(xpp::Session &s, bool saving = false);
namespace xpp {

/* loads what req, made in the session now, asks for: the new Session,
   current now (its Model its model()), now gone; the error (not shown: the caller
   shows or returns it) when the file cannot be read or loaded, now and its Model
   still current and the working folder the one before */
Result<Session *> load_requested(const Session &now, const ModelRequest &req);

}
