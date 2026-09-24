#ifndef XPP_GLOBALS_H
#define XPP_GLOBALS_H

/* What this run of the program is. The rest of the state main.c and the
   X11 files once defined now lives with the module that owns it (batch
   options in xpp_batch.h, logging in xpp_log.h, the plot windows in
   many_pops.h, and so on; CLAUDE.md lists them). Defined in
   xpp_globals.cpp, which has no UI dependency. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int interactive;     /* a front end is up (0: -silent, headless) */
    /* AUTO's scratch directory (fort.3/7/8/9, <model>.ode.b/.d/.s). NULL:
       HOME, as upstream (-silent); xppautX sets a private one per session
       so concurrent sessions never share AUTO files (xppautx_main.c). */
    char *auto_dir;
    float version_major, version_minor; /* XPPAUT's version, for titles */
    int tutorial;        /* @ tutorial=1: show the tutorial at start-up */
} XppProgram;
extern XppProgram program;

#ifdef __cplusplus
}
#endif
#endif
