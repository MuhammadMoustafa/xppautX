#ifndef XPP_UI_JSON_H
#define XPP_UI_JSON_H
#ifdef __cplusplus
extern "C" {
#endif

/* line-delimited JSON front end (ui_json.cpp, docs/protocol.md) */
/* the protocol's version, in hello (and the desktop window's About) */
#define JSON_UI_PROTOCOL 2
#define JSON_UI_STR_(x) #x
#define JSON_UI_STR(x) JSON_UI_STR_(x)
void json_ui_install(void);        /* protocol on the current stdout */
void json_ui_handle(const char *line);
void json_ui_loop(void);           /* read and run commands until EOF */
/* {"cmd":"open","file":path} into the inbox, as if the page had sent it:
   the desktop window's File > Open model, from the window's thread */
void json_ui_push_open(const char *path);

/* --script FILE: play FILE's lines instead of reading stdin (docs/protocol.md
   "Scripts"). Call before json_ui_install(), which then skips the stdin
   reader. Returns 0 when FILE cannot be opened. Once set, the process exits
   1 at end of file if a "message" "error" event was sent, or a script line
   could not be matched to the ask it was meant to answer; 0 otherwise. */
int json_ui_set_script(const char *path);

/* xppautX model.ode -silent: load the model with no interface, then run
   the built-in script its options make (core/json_silent.cpp) through
   this front end, its events going nowhere; exits 0 when the script
   ends, 1 when the model does not load */
int json_ui_silent(int argc, char **argv);

#ifdef __cplusplus
}

#include "diagnostic.h"

namespace xpp {
struct Session; /* session.h */
}

/* the model of s just loaded (xpp_load_model): the front end's set-up,
   then hello, the main window and state (json_model.cpp) */
void json_ui_start_model(xpp::Session &s);

/* the model did not load: why and where, as the `error` event (in place
   of hello; docs/protocol.md "A model that does not load") */
void json_ui_load_error(const xpp::Diagnostic &d);
#endif
#endif
