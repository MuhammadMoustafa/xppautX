#ifndef XPP_BATCH_H
#define XPP_BATCH_H

/* Reset the "which options were explicitly set" table. Called at the start
   of both the X11 do_main() and the headless xpp_batch_main(). */
void xpp_reset_options(void);

/* Command-line scan for -quiet / -logfile (they must win over .xpprc) */
void check_for_quiet(int argc, char **argv);

/* .xpprc, environment and command-line option processing */
void do_vis_env(void);

/* Headless run: parse argv like xppaut does, force batch mode, load the
   ODE file, integrate, write output.dat. Returns 0 on success. */
int xpp_batch_main(int argc, char **argv);

#endif
