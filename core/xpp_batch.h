#ifndef XPP_BATCH_H
#define XPP_BATCH_H
#ifdef __cplusplus
extern "C" {
#endif

/* the model cannot be loaded (a parse or compile error, already logged):
   xpp::model_failed, at the place the load is (xpp::Load::diagnostic):
   during a load, the load fails (xpp::LoadFailed, caught by
   xpp::load_model); otherwise the program exits with status 1. In browser
   and window mode the page stays open on the log until it is closed
   (xpp_http's at_exit). */
[[noreturn]] void xpp_model_failed(void);

#ifdef __cplusplus
}

#include "diagnostic.h"
#include "model_files.h"

#include <expected>
#include <string>
#include <vector>

namespace xpp {
struct Session; /* session.h */
}

/* After a load with no interface (-silent, a unit test): s's browser,
   graphs and colours set up, and the command line's files and internal
   sets taken in (-setfile, -parfile, -icfile, -readset/-with,
   -internset/-uset/-rset). What -silent then runs is its built-in
   script (json_silent.cpp). */
void xpp_batch_start(xpp::Session &s);

/* The start of a load (xpp::load_model), in the Session it builds: the
   "which options were explicitly set" table reset; the command line's
   -quiet and -logfile (they must win over .xpprc); .xpprc's and the
   command line's options */
void xpp_reset_options(xpp::Session &s);
void check_for_quiet(xpp::Session &s, int argc, char **argv);
void do_vis_env(xpp::Session &s);

namespace xpp {
/* what a load gives: the Session it loaded (its Model the Session's),
   or what is wrong and where */
using Loaded = std::expected<Session *, Diagnostic>;
/* The shared start of every mode (the program, -silent, --convert,
   File > Open model, a unit test): options, the ODE file, numerics
   set-up; batch forces batch mode. The Session loaded (the client's in
   the session list from now on, session.h), else what is wrong and where
   (the one before stays; the caller exits with status 1 when there is
   nothing to go on with). saved: a model saved in an
   AUTO or session file, whose files the load reads instead of the disk's
   (model_files.h); argv names its first file. */
Loaded load_model(int argc, char **argv, int batch, const SavedModel *saved = nullptr);
/* the model cannot be loaded, for the reason d (already logged): during a
   load, the load fails (LoadFailed with d, the line d.line of d.file
   added as it is written); otherwise the program exits with status 1 */
[[noreturn]] void model_failed(Diagnostic d);
}

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
#endif
#endif
