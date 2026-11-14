#pragma once

#include "xpp_error.h"

#include <string_view>

namespace xpp {

/* What a reader of a model calls when the model cannot be loaded. The readers
   (the parser, the compiler, the .odex and set readers) are inside, below the
   Session; the load that catches the failure is the application's, so these
   are implemented in xpp_batch.cpp, as XppUi is implemented outside. */

/* the model cannot be loaded (a parse or compile error, logged while the
   load is at its place): model_failed(Load::error()), at the place the
   load is: during a load, the load fails (LoadFailed, caught
   by load_model); otherwise the program exits with status 1. In browser
   and window mode the page stays open on the log until it is closed
   (xpp_http's at_exit). */
[[noreturn]] void model_failed();
/* the same, for the reason what (the parser's own): at the place the load
   is, after what was logged there (Load::error(what)) */
[[noreturn]] void model_failed(std::string_view what);
/* the model cannot be loaded, for the reason e: during a load, the load
   fails (LoadFailed with e, the line e.place.line of e.place.file added
   as it is written; load_model writes it); otherwise e is written and the
   program exits with status 1 */
[[noreturn]] void model_failed(Error e);

/* A start with no model file on the command line (a Start menu entry, the
   Dock, a launcher) is not a load that failed: it is the error whose
   `where` is NO_MODEL_WHERE, which load_model does not write to the log in
   an interactive start. The program then serves the start screen, whose
   Open model is File > Open model's (model_switch.h, W232); --silent, with
   nothing to open, takes it as any other error. */
inline constexpr std::string_view NO_MODEL_WHERE = "no model";
inline bool is_no_model(const Error &e) { return e.where == NO_MODEL_WHERE; }

} // namespace xpp
