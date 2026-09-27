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
   batch mode */
void xpp_load_model(int argc, char **argv, int batch);

/* the model cannot be loaded (a parse or compile error, already logged):
   the program exits with status 1. In browser and window mode the page
   stays open on the log until it is closed (xpp_http's at_exit). */
[[noreturn]] void xpp_model_failed(void);

#ifdef __cplusplus
}

#include <string>
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
};
extern XppBatchOptions batch_options;
#endif
#endif
