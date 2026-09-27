#ifndef XPP_BATCH_H
#define XPP_BATCH_H
#ifdef __cplusplus
extern "C" {
#endif

/* Reset the "which options were explicitly set" table. Called at the start
   of the headless xpp_batch_main(). */
void xpp_reset_options(void);

/* Command-line scan for -quiet / -logfile (they must win over .xpprc) */
void check_for_quiet(int argc, char **argv);

/* .xpprc, environment and command-line option processing */
void do_vis_env(void);

/* Headless run: parse argv like xppaut does, force batch mode, load the
   ODE file, integrate, write output.dat. Returns 0 on success. */
int xpp_batch_main(int argc, char **argv);

/* the shared start: options, the ODE file, numerics set-up; batch forces
   batch mode. 1 when the model loaded (its Model and Session are the
   current ones), 0 when it failed (the ones before stay current): the
   caller exits with status 1 when there is nothing to go on with. */
int xpp_load_model(int argc, char **argv, int batch);

/* the model cannot be loaded (a parse or compile error, already logged):
   during a load, the load fails (xpp::LoadFailed, caught by
   xpp_load_model); otherwise the program exits with status 1. In browser
   and window mode the page stays open on the log until it is closed
   (xpp_http's at_exit). */
[[noreturn]] void xpp_model_failed(void);

#ifdef __cplusplus
}

#include <string>
#include <vector>
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
