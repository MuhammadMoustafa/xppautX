#pragma once

/* What this run of the program is. The rest of the state lives with the module that owns it (batch
   options in xpp_batch.h, logging in xpp_log.h, the plot windows in
   many_pops.h, and so on; AGENTS.md lists them). Defined in
   xpp_globals.cpp, which has no UI dependency. C++ (a std::string
   member). */


struct XppProgram {
    int interactive = 0; /* a front end is up (0: --silent, headless) */
    /* where the page is shown: no page (--silent, --server), the desktop window or the user's browser (W622: a restart opens the same) */
    enum class Page { none, window, browser } page = Page::none;
    float version_major = 0, version_minor = 0; /* XPPAUT's version, for titles */
};
extern XppProgram program;

