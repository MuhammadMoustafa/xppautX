#ifndef XPP_BATCH_H
#define XPP_BATCH_H

#include "xpp_error.h"
#include "model_files.h"

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace xpp {

struct Session; /* session.h */

/* the model cannot be loaded (a parse or compile error, logged while the
   load is at its place): model_failed(Load::error()), at the place the
   load is: during a load, the load fails (LoadFailed, caught
   by load_model); otherwise the program exits with status 1. In browser
   and window mode the page stays open on the log until it is closed
   (xpp_http's at_exit). */
[[noreturn]] void model_failed();

/* After a load with no interface (-silent, a unit test): s's browser,
   graphs and colours set up, and the command line's files and internal
   sets taken in (-setfile, -parfile, -icfile, -readset/-with,
   -internset/-uset/-rset). What -silent then runs is its built-in
   script (json_silent.cpp). */
void batch_start(Session &s);

/* The start of a load (load_model), in the Session it builds (its
   options_set empty: no option set yet): the command line's -quiet and
   -logfile (they must win over .xpprc); .xpprc's and the command line's
   options */
void check_for_quiet(Session &s, int argc, char **argv);
void do_vis_env(Session &s);

/* what a load gives: the Session it loaded (its Model the Session's),
   or what is wrong and where */
using Loaded = std::expected<Session *, Error>;
/* The shared start of every mode (the program, -silent, --convert,
   File > Open model, a unit test): options, the ODE file, numerics
   set-up; batch forces batch mode. The Session loaded (the client's in
   the session list from now on, session.h), else what is wrong and where
   (the one before stays; the caller exits with status 1 when there is
   nothing to go on with). saved: a model saved in an
   AUTO or session file, whose files the load reads instead of the disk's
   (model_files.h); argv names its first file. check, when given, reads
   what such a file adds against the loaded Session before the load keeps
   it (xpp_saved_check, xpp_session.h): what it finds wrong fails the load
   as a model that does not load does. What is wrong is written to the
   log here (Error::text()), once. */
Loaded load_model(int argc, char **argv, int batch, const SavedModel *saved = nullptr,
                  const std::function<std::optional<Error>(Session &)> &check = {});
/* the model cannot be loaded, for the reason e: during a load, the load
   fails (LoadFailed with e, the line e.place.line of e.place.file added
   as it is written; load_model writes it); otherwise e is written and the
   program exits with status 1 */
[[noreturn]] void model_failed(Error e);

/* How a run without an interface goes and where its output lands: the
   command line (-silent, -outfile, -equil, -iset) and the ODE file's
   @ output=, @ range= options set these. */
struct XppBatchOptions {
    int enabled = 0;           /* batch mode: no interface, run and write */
    int range = 0;             /* run the range integration in batch mode */
    int equilibria = -1;       /* -equil: <0 none, 1 find and write equilibria */
    int use_intern_sets = 1;   /* run every internal set (1) or the chosen ones */
    std::string out_file;      /* the data file a batch run writes */
    std::string user_out_file; /* -outfile as given ("": name it after the set) */
    /* whether a batch run uses each of the model's internal sets, as the
       command line picked them (if_needed_select_sets; a set it did not
       pick is used), and how many it picked in all (0: run once, no set) */
    std::vector<int> intern_set_use;
    int intern_sets_used = 0;
    int uses_intern_set(std::size_t j) const {
        return j < intern_set_use.size() ? intern_set_use[j] : 1;
    }
};
extern XppBatchOptions batch_options;

} // namespace xpp
#endif
