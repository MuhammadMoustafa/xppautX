#pragma once

#include "xpp_error.h"

#include <string_view>

namespace xpp {

/* What a reader of a model calls when the model cannot be loaded. The readers
   (the parser, the compiler, the .odex and set readers) are inside, below the
   Session; the load that catches the failure is the application's. The
   kernel owns what they need of it, the LoadSite below, which the Session's
   Load implements, as XppUi is implemented outside. */

/* what model_failed throws while a Load is in progress: the model
   cannot be loaded (a parse or compile error), what is wrong and where */
struct LoadFailed {
  Error error;
};

/* The load in progress, as the readers meet it: the Session's Load is one
   (session.h) and, while it lives, the only one. */
class LoadSite {
public:
  /* a load is in progress */
  static bool running() noexcept;
  /* e.place.source, when empty, the line e.place.line of e.place.file as
     the model of the load in progress reads it; nothing when no load is in
     progress */
  static void add_source(Error &e);
  /* what went wrong where the load is (a load is in progress): the place it
     is at, and as what failed the ERROR and WARN messages logged since (no
     longer kept to be written), then also */
  static Error error(std::string_view also = {});
  /* The load is at line (and column) of file: the model's readers and builder say where they are, so that a problem is
     reported there; line 0, the model as a whole. The ERROR and WARN messages logged while a load is in progress are kept
     and written with the place they are about once the load moves on from it (here), commits or ends. Nothing when no
     load is in progress. */
  static void at(std::string_view file, int line = 0, int col = 0);
  /* where the load is (at()'s place); empty when no load is in progress */
  static Place place();

protected:
  LoadSite() noexcept;
  ~LoadSite();
  LoadSite(const LoadSite &) = delete;
  LoadSite &operator=(const LoadSite &) = delete;
  /* the load ends before its members go */
  static void leave() noexcept;
  virtual Error describe(std::string_view also) = 0;
  /* at()'s and place()'s: the load's own record of where it is */
  virtual void arrive(std::string_view file, int line, int col) = 0;
  virtual Place where_is() const = 0;
  virtual void locate(Error &e) = 0;
};

/* the model cannot be loaded (a parse or compile error, logged while the
   load is at its place): model_failed(LoadSite::error()), at the place the
   load is: during a load, the load fails (LoadFailed, caught
   by load_model); otherwise the program exits with status 1. In browser
   and window mode the page stays open on the log until it is closed
   (xpp_http's at_exit). */
[[noreturn]] void model_failed();
/* the same, for the reason what (the parser's own): at the place the load
   is, after what was logged there (LoadSite::error(what)) */
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
