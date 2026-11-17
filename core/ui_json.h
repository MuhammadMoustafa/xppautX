#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "xpp_error.h"
#include "model_files.h"
namespace xpp { namespace settings { struct Read; } struct CommandLine; struct ModelLoader; /* model_load.h */ }

/* a run of the About text: plain, or a link when `url` is not empty */
struct AboutPart {
    std::string text;
    std::string url;
};

struct AboutLine {
    std::vector<AboutPart> parts; /* none: a blank line */
};

namespace xpp {
struct Session; /* session.h */
struct ModelRequest; /* model_request.h */
class FileSystem; /* xpp_fs.h */

/* line-delimited JSON front end (ui_json.cpp, docs/protocol.md) */
/* the protocol's version (format_version.h, the one number of the protocol and every file), in hello and the desktop window's About */
#define JSON_UI_STR_(x) #x
#define JSON_UI_STR(x) JSON_UI_STR_(x)
/* What hello (and a recording's header) carries that only the program's start knows: the front end owns this
   shape and the program fills it before it installs the front end. */
struct HelloInfo {
    std::vector<AboutLine> about;        /* Help > About's text (xpp_about.h) */
    std::string version;                 /* the release tag, or "dev" (xpp_version_string) */
    std::string update_install, update_asset_suffix, update_why; /* how a newer release is installed over this program, and which release file fits (xpp_install.h) */
    std::function<std::string()> start;  /* a session with no model: hello's `start` object, the recent models and the examples (xpp_recent.h, xpp_examples.h) */
    /* the page the program serves (browser mode, the window), which the platform implements and the program's start wires in (xpp_http.h,
       xpp_window.h): whether one is up (the events then go to it, not to stdout), the one line it is sent, and the model's title for the window */
    /* what the program does that the front end asks for, from the model switch (model_switch.h, the application): File > Quit's
       question and exit (`saving`: the page asked it already and the answer was Save; `download`: a browser takes the saved session as a download, W796, and the file comes back), the question before a model replaces this
       one (named by its file), and the load a command's request asks for */
    std::function<std::string(Session &s, bool saving, bool download)> quit;
    std::function<bool(Session &s, const std::string &file)> model_may_leave;
    std::function<Result<Session *>(const Session &now, const ModelRequest &req)> load_requested;
    /* what the front end asks of the system's standard streams (xpp_platform.h): up to n bytes of stdin, blocking (<= 0 at the end or on an
       error), and no line-end translation on a descriptor (Windows) */
    std::function<long(char *buf, std::size_t n)> read_input;
    std::function<void(int fd)> set_binary_mode;
    std::function<bool()> page_active = [] { return false; };
    std::function<void(std::string_view line)> page_emit;
    std::function<void(const char *title)> page_title;
};
void json_ui_install(HelloInfo hello, bool silent = false); /* protocol, or no interface */
void json_ui_loop(void);           /* read and run commands until EOF */
/* xppautX model.ode --silent: load the model with no interface, then run
   the built-in script its options make (core/json_silent.cpp) through
   this front end, its events going nowhere; exits 0 when the script
   ends, 1 when the model does not load */
int json_ui_silent(FileSystem &fs, const ModelLoader &loader, CommandLine cl, const settings::Read *read_settings, HelloInfo hello);

/* the command line run at once, as if the client had sent it; the
   Session it ended in: the client's, or the one a model it loaded (a
   recording's, the player) put in its place */
Session &json_ui_handle(const char *line);

/* the model of s just loaded (load_model): the front end's set-up,
   then hello, the main window and state (json_model.cpp) */
void json_ui_start_model(Session &s);

/* The program started with no model file (xpp::is_no_model, model_failed.h): the
   client's Session has no model, hello carries the start screen
   (`start`: the recent models, docs/protocol.md "Start screen"), and the
   Open model dialog opens at once, as File > Open model does; a cancel
   leaves the start screen (json_model.cpp). */
void json_ui_start_screen();

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
