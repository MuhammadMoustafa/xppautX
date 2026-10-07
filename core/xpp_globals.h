#ifndef XPP_GLOBALS_H
#define XPP_GLOBALS_H

/* What this run of the program is. The rest of the state main.c and the
   X11 files once defined now lives with the module that owns it (batch
   options in xpp_batch.h, logging in xpp_log.h, the plot windows in
   many_pops.h, and so on; AGENTS.md lists them). Defined in
   xpp_globals.cpp, which has no UI dependency. C++ (a std::string
   member). */

#include <string>

struct XppProgram {
    int interactive = 0; /* a front end is up (0: --silent, headless) */
    float version_major = 0, version_minor = 0; /* XPPAUT's version, for titles */
    int tutorial = 0; /* @ tutorial=1: show the tutorial at start-up */
};
extern XppProgram program;

#endif
