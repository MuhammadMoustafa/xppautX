#pragma once

#include <cstdint>
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
   (W103: every file it read, model/<name>), the state file session.odex (.odex syntax,
   odex.h's read_state_file: the parameters and initial values as par and init lines, the numerics as an options block, and the
   rest in one session statement: the manifest, the run position, delays, boundary conditions, autoX's settings and
   views, every plot window's axes, variables and zoom, sliders, nullclines, labels, arrows and markers, the random
   generator), autoX's diagram members (autox/: xpp_session_autox.h),
   frozen.npz (the frozen curves' points) and data.npz
   (the data table, NPZ as Save data writes it). The earlier runs a window
   keeps until Erase are not saved: the data table is the last run's.

   xpp_session_save writes name.snapx (.snapx added unless name has it;
   NULL or empty asks for one, the way Save data does). data: 1 the
   data table goes in, 0 it is left out, -1 it goes in unless it is above
   50 MB, when the user is asked whether to leave it out (Go computes it
   again).

   Return 1 on success, 0 on failure or a cancel (err_msg names a
   problem). The session file last saved or opened is the Session's
   saved_session (below): core/json_state.cpp reports it as the state
   event's "session" member. Both work on the session s. */
/* Save through the shared owner; false when declined, error shown by the caller.
   copy: Save a copy of the session (W218), which leaves the Session's file
   and its changed flag as they were. */
xpp::Result<bool> xpp_session_save_file(xpp::Session &s, const std::string &file, bool data, bool replace = false, bool copy = false);
int xpp_session_save(xpp::Session &s, const char *name, int data, bool copy = false, bool replace = false);
/* File > Save session (Ctrl+S): to the session file the Session remembers
   (saved_session) with no dialog and no question about replacing it; with
   none (a first save, a model never saved) it is Save as, which asks for
   a path and before replacing an existing file (xpp_session_save with no
   name). Quit's "Save this session first?" saves the same way. */
int xpp_session_save_here(xpp::Session &s, int data = -1);
/* The unsaved-changes dot's definition, once (state's `changed`): the
   session's values differ from those it was last saved, opened or loaded
   with. A value is a parameter's, an initial condition's, a numerics
   setting of the time span, method, tolerances and bounds, or the run's
   data (the rows stored and where the run ended). A view, a window or an
   autoX diagram alone is not a change. */
bool xpp_session_changed(const xpp::Session &s);
/* the session's values as they are now are the saved ones: a save, an
   open of a session file and the end of every load call it */
void xpp_session_mark_clean(xpp::Session &s);
/* the session file of s as it would be saved without the data table, as
   its bytes: a recording's snapshot (W59d, recx.h), made asking nothing;
   nothing, with an error message, when it cannot be made (a model not
   read from files) */
std::optional<std::string> xpp_session_snapshot(xpp::Session &s);

/* the session file last saved or opened; a Session's (session.h) */
struct SavedSession {
    std::string file;           /* the .snapx */
    std::uint64_t clean = 0;    /* the values' fingerprint when it was saved, opened or loaded */
};

/* A session archive carries its saved model and all session members.
   The reader checks the whole file before the model is kept. */
struct SavedFile {
    std::string path;              /* absolute */
    std::string name;              /* what a message calls it: its file name, or a recording's snapshot */
    bool snapshot = false;         /* a recording's snapshot (recx.h): restored as a session, but no session file */
    xpp::snapx::Manifest manifest;
    std::map<std::string, std::string> members; /* every member, by its name */
    xpp::SavedModel model;         /* the model's files, saved in path */
};

/* path ends in .snapx (case ignored). */
bool xpp_saved_file_name(std::string_view path);
/* the session file path read whole:
   nothing, with an error message, when it cannot be read, is not a zip,
   has no manifest of its kind or has no model */
std::optional<SavedFile> xpp_saved_read(const std::string &path);
/* what a file that carries a model holds */
enum class SavedKind { session, snapshot };
/* bytes, a file of kind kind, read as xpp_saved_read reads one: path is
   where it is (absolute; for a snapshot the recording's), name what an
   error message calls it */
std::optional<SavedFile> xpp_saved_parse(const std::string &path, const std::string &name, std::string_view bytes, SavedKind kind);
/* The first members of the session archive carrying the model of s: the state file with the manifest alone
   (Save session writes the whole state there) and the model's files; man (its data as given) is filled with the
   model's name and animation; the error when the model was not read from files (a model typed in) */
xpp::Result<std::vector<xpp::zip::Entry>> xpp_saved_entries(const xpp::Session &s, xpp::snapx::Manifest &man);
/* f's members read for the session s its model's load made, before the
   load keeps it (xpp::load_model's check): a member missing or one
   that does not read or holds a value this model refuses fails the open,
   and the session before stays as it was. Nothing when f can be
   restored, otherwise the error at its place: a line of the state file
   ("name.snapx/session.odex", line 12, the line as written), or the file
   f.name when a member is missing. Nothing is changed (W125: every member
   is read whole and checked before anything is applied). */
std::optional<xpp::Error> xpp_saved_check(xpp::Session &s, const SavedFile &f);
/* what f adds to its model into the session s, whose model is f's
   (loaded from it): the whole session;
   false, with an error message, when it could not be read or put in
   place (a file xpp_saved_check passed fails only on the disk) */
bool xpp_saved_restore(xpp::Session &s, const SavedFile &f);

