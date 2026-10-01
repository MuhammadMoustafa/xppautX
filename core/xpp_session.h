#ifndef _xpp_session_h_
#define _xpp_session_h_

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "model_files.h"
#include "snapx.h"
#include "xpp_zip.h"

namespace xpp {
struct Session; /* session.h */
}

/* Save session and Open session (W57, docs/protocol.md "Session files"):
   continuing where the user stopped, from one file, name.snapx -- a zip
   of ordinary files (snapx.h names them): the manifest, the model itself
   (W103: every file it read, model/<name>), model.set (File/Write set's
   file: values, numerics, the active window's graphics), AUTO's members
   (auto/: the diagram, AUTO's settings and solutions, autox.h), windows.set
   (every plot window's axes, variables and zoom, AUTO's view), marks.set
   and frozen.npz (labels, arrows and markers, frozen curves) and data.npz
   (the data table, NPZ as Save data writes it). The earlier runs a window
   keeps until Erase are not saved: the data table is the last run's.

   xpp_session_save writes name.snapx (.snapx added unless name has it;
   NULL or empty asks for one, the way File/Write set does). data: 1 the
   data table goes in, 0 it is left out, -1 it goes in unless it is above
   50 MB, when the user is asked whether to leave it out (Go computes it
   again).

   xpp_session_load(name) opens name.snapx (name when it ends so) as File >
   Open model opens it (xpp_model_open, model_switch.h): its saved model,
   then everything as it was saved. NULL or empty asks for one.

   Return 1 on success, 0 on failure or a cancel (err_msg names a
   problem). The session file last saved or opened is the Session's
   saved_session (below): core/json_state.cpp reports it as the state
   event's "session" member. Both work on the session s. */
int xpp_session_save(xpp::Session &s, const char *name, int data);
int xpp_session_load(xpp::Session &s, const char *name);
/* the session file of s as it would be saved without the data table, as
   its bytes: a recording's snapshot (W59d, recx.h), made asking nothing;
   nothing, with an error message, when it cannot be made (a model not
   read from files) */
std::optional<std::string> xpp_session_snapshot(xpp::Session &s);

/* the session file last saved or opened; a Session's (session.h) */
struct SavedSession {
    std::string file; /* the .snapx */
};

/* A file that carries a model (W103): an AUTO file (.autox, autox.h) or
   a session file (.snapx), read whole by the one reader both share. The
   model is always in it: opening one loads that model, from those saved
   files and never the disk (model_files.h), then what the file adds. */
struct SavedFile {
    std::string path;              /* absolute */
    std::string name;              /* what a message calls it: its file name, or a recording's snapshot */
    bool session = false;          /* a .snapx, else an .autox */
    bool snapshot = false;         /* a recording's snapshot (recx.h): restored as a session, but no session file */
    xpp::snapx::Manifest manifest;
    std::map<std::string, std::string> members; /* every member, by its name */
    xpp::SavedModel model;         /* the model's files, saved in path */
};

/* path is named as a file that carries a model: .autox or .snapx (case
   ignored) */
bool xpp_saved_file_name(std::string_view path);
/* the file path (a .autox or .snapx, xpp_saved_file_name) read whole:
   nothing, with an error message, when it cannot be read, is not a zip,
   has no manifest of its kind or has no model */
std::optional<SavedFile> xpp_saved_read(const std::string &path);
/* what a file that carries a model holds */
enum class SavedKind { autox, session, snapshot };
/* bytes, a file of kind kind, read as xpp_saved_read reads one: path is
   where it is (absolute; for a snapshot the recording's), name what an
   error message calls it */
std::optional<SavedFile> xpp_saved_parse(const std::string &path, const std::string &name, std::string_view bytes, SavedKind kind);
/* the command line's arguments that load f's model: its file, and
   -anifile's animation when it was loaded with one */
std::vector<std::string> xpp_saved_args(const SavedFile &f);
/* the first members of a file of kind (snapx.h's session_kind, autox.h's
   kind) that carries the model of s: the manifest man and the model's
   files; nothing, with an error message, when the model was not read
   from files (a model typed in) */
std::optional<std::vector<xpp::zip::Entry>> xpp_saved_entries(const xpp::Session &s, xpp::snapx::Manifest man, std::string_view kind);
/* f's members read for the session s its model's load made, before the
   load keeps it (xpp::load_model's check), or for the session open when
   the model is the same (an AUTO file's diagram): a member missing or one
   that does not read or holds a value this model refuses fails the open,
   and the session before stays as it was. Nothing when f can be
   restored, otherwise the error at its place: a member's line
   ("name.snapx/windows.set", line 12, the line as written), or the file
   f.name when a member is missing. Nothing is changed (W125: every member
   is read whole and checked before anything is applied). */
std::optional<xpp::Error> xpp_saved_check(xpp::Session &s, const SavedFile &f);
/* what f adds to its model into the session s, whose model is f's
   (loaded from it, or the same files): AUTO's diagram, or the session;
   false, with an error message, when it could not be read or put in
   place (a file xpp_saved_check passed fails only on the disk) */
bool xpp_saved_restore(xpp::Session &s, const SavedFile &f);

/* m's file name without .ode/.odex, and ext (".snapx", ".autox"): the
   name Save session and AUTO's Save diagram offer */
std::string xpp_session_file_name(const xpp::Model &m, std::string_view ext);

#endif
