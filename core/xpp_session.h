#ifndef _xpp_session_h_
#define _xpp_session_h_
#ifdef __cplusplus
extern "C" {
#endif

/* Save session and Open session (W57, docs/protocol.md "Session files"):
   continuing where the user stopped, from one file, name.snapx -- a zip
   of ordinary files (snapx.h names them): the model's path and
   fingerprint, model.set (File/Write set's file: values, numerics, the
   active window's graphics), model.autox (AUTO's File/Save diagram's,
   autox.h: the diagram, AUTO's settings and solutions), windows.set (every plot window's axes,
   variables and zoom, AUTO's view), marks.set and frozen.npz (labels,
   arrows and markers, frozen curves) and data.npz (the data table, NPZ
   as Save data writes it). The earlier runs a window keeps until Erase
   are not saved: the data table is the last run's.

   xpp_session_save writes name.snapx (.snapx added unless name has it;
   NULL or empty asks for one, the way File/Write set does). data: 1 the
   data table goes in, 0 it is left out, -1 it goes in unless it is above
   50 MB, when the user is asked whether to leave it out (Go computes it
   again). Opening one loads its model in this process (File > Open
   model's switch, model_switch.h) and then restores everything as it was
   saved (xpp_session_restore below); a model file that has changed since
   (its fingerprint) is warned about and keeps what still fits by name.

   xpp_session_load(name): a .snapx (name.snapx, or name when it ends so)
   is opened as File > Open model opens a model (xpp_model_open: it asks
   whether to save this session first); otherwise the older pair of files
   a session was before W57, <name>.set and, when there is one,
   <name>.auto, is read into the current model as before. NULL or empty
   asks for a .snapx.

   Return 1 on success, 0 on failure or a cancel (err_msg names a
   problem). The files last saved or opened are the Session's
   saved_session (below): core/json_state.cpp reports them as the state
   event's "session" member. */
int xpp_session_save(const char *name, int data);
int xpp_session_load(const char *name);

#ifdef __cplusplus
}

#include <string>
#include <string_view>
#include "snapx.h"

/* the files a session was last saved to or opened from: a session file,
   or (an older session loaded) its .set and .auto; a Session's (session.h) */
struct SavedSession {
    std::string file;      /* the .snapx */
    std::string set, auto_file;
};

/* the current model's fingerprint (snapx.h): its file and every file it
   included, read again now (one that cannot be read counts as empty) */
std::string xpp_session_fingerprint();

/* the model's file name without .ode/.odex, and ext (".snapx", ".autox"):
   the name Save session and AUTO's Save diagram offer */
std::string xpp_session_file_name(std::string_view ext);

/* the current model as a manifest names it (snapx.h): its path, file
   name, fingerprint, node, nmarkov, variables and parameters */
xpp::snapx::Manifest xpp_session_manifest();
/* man names the variables and parameters the current model has, in its
   order (a diagram or a set file of it reads by index) */
bool xpp_session_same_names(const xpp::snapx::Manifest &man);
/* a changed model's note, in the log and on the status line */
void xpp_session_warn(const std::string &text);

/* session file snapx's model, found beside it first (its saved name) and
   else at the path it was saved from: its absolute path, or empty (and an
   error message) when snapx is not a session file or neither is there */
std::string xpp_session_model(const std::string &snapx);
/* session file snapx restored into the model just loaded from its
   xpp_session_model (model_switch.h's request, or the command line's):
   false (an error message) when snapx cannot be read at all */
bool xpp_session_restore(const std::string &snapx);
#endif
#endif
