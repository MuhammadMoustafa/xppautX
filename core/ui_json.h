#ifndef XPP_UI_JSON_H
#define XPP_UI_JSON_H

#include <optional>
#include <string>
#include "xpp_error.h"
#include "model_files.h"

namespace xpp {
struct Session; /* session.h */

/* line-delimited JSON front end (ui_json.cpp, docs/protocol.md) */
/* the protocol's version, in hello (and the desktop window's About) */
#define JSON_UI_PROTOCOL 3
#define JSON_UI_STR_(x) #x
#define JSON_UI_STR(x) JSON_UI_STR_(x)
void json_ui_install(bool silent = false); /* protocol, or no interface */
void json_ui_loop(void);           /* read and run commands until EOF */
/* {"cmd":"open","file":path} into the inbox, as if the page had sent it:
   the desktop window's File > Open model, from the window's thread */
void json_ui_push_open(const char *path);

/* xppautX model.ode --silent: load the model with no interface, then run
   the built-in script its options make (core/json_silent.cpp) through
   this front end, its events going nowhere; exits 0 when the script
   ends, 1 when the model does not load */
int json_ui_silent(int argc, char **argv);

/* the command line run at once, as if the client had sent it; the
   Session it ended in: the client's, or the one a model it loaded (a
   recording's, the player) put in its place */
Session &json_ui_handle(const char *line);

/* the model of s just loaded (load_model): the front end's set-up,
   then hello, the main window and state (json_model.cpp) */
void json_ui_queue_runnow(Session &s);
void json_ui_start_model(Session &s);

/* the model did not load: why and where, as the `error` event (in place
   of hello; docs/protocol.md "A model that does not load") */
void json_ui_load_error(const Error &e);

/* A recording (.recx) given on the command line or opened by the OS
   (W59c): json_ui_recording_launch reads it and returns the model it
   holds, to start the session with (load_model's `saved`, `model`
   the name the model's file is given as); nullopt, with the reason
   logged, when it is not a recording. json_ui_play_launched, once that
   model has started, opens the recording in the player without the
   question File > Open model asks (nothing is to be saved yet). */
struct RecordingLaunch {
    SavedModel saved;
    std::string model;
    std::string folder; /* even the initial model load writes only in scratch */
};
std::optional<RecordingLaunch> json_ui_recording_launch(const std::string &path);
void json_ui_play_launched(Session &s, const std::string &path);

} // namespace xpp
#endif
