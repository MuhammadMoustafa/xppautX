#ifndef XPP_BATCH_H
#define XPP_BATCH_H
#ifdef __cplusplus
extern "C" {
#endif

/* How a run without an interface goes and where its output lands: the
   command line (-silent, -outfile, -equil, -iset) and the ODE file's
   @ output=, @ range= options set these. */
typedef struct {
    int enabled;             /* batch mode: no interface, run and write */
    int range;               /* run the range integration in batch mode */
    int equilibria;          /* -equil: <0 none, 1 find and write equilibria */
    int use_intern_sets;     /* run every internal set (1) or the chosen ones */
    char out_file[256];      /* the data file a batch run writes */
    char user_out_file[256]; /* -outfile as given ("": name it after the set) */
} XppBatchOptions;
extern XppBatchOptions batch_options;

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

#ifdef __cplusplus
}
#endif
#endif
