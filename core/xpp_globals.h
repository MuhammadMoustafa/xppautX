#pragma once

/* What this run of the program is: the process-wide state the readers and the
   computations below the program's start read (the program, the batch
   options), declared here at their lowest reader, written by the program's start
   (xppautx_main.cpp, xpp_batch.cpp). The rest of the state lives with the module that owns it
   (logging in xpp_log.h, the plot windows in many_pops.h, and so on; AGENTS.md
   lists them). Defined in xpp_globals.cpp, which has no UI dependency. C++ (a
   std::string member). */

#include <string>
#include "xpp_cpu.h"


struct XppProgram {
    int interactive = 0; /* a front end is up (0: --silent, headless) */
    /* where the page is shown: no page (--silent, --server), the desktop window or the user's browser (W622: a restart opens the same) */
    enum class Page { none, window, browser } page = Page::none;
    float version_major = 0, version_minor = 0; /* XPPAUT's version, for titles */
    /* what measures the physical cores a run of jobs or of a sweep shares when the thread count is automatic: the platform's
       (xpp_cpu_system.h, measuring on its first call only), given by the program's start; none (a unit test that sets none): 1 */
    unsigned (*measure_physical_threads)() = nullptr;
    unsigned physical_threads() const { return measure_physical_threads ? measure_physical_threads() : 1; }
    /* the threads a set of jobs or runs shares: the preferred count, or all the physical cores when it is automatic */
    unsigned thread_budget(unsigned preferred) const { return preferred != xpp::cpu::THREADS_AUTO ? preferred : physical_threads(); }
};
extern XppProgram program;


namespace xpp {

/* How a run without an interface goes: the command line's own flags
   (--silent, --outfile, --equil, -iset), set by the program's start and the
   batch start (xpp_batch.cpp). The model's @ output= and @ range= are the
   Session's (session.h batch_out_file, integrator.range.write_files). */
struct XppBatchOptions {
    int enabled = 0;           /* batch mode: no interface, run and write */
    bool yes = false;          /* -y / --yes: terminal questions answered yes, each answer printed */
    bool check = false;        /* --check: nothing waits on the terminal; a question is left unanswered */
    int equilibria = -1;       /* --equil: <0 none, 1 find and write equilibria */
    std::string user_out_file; /* --outfile as given ("": name it after the set) */
};
extern XppBatchOptions batch_options;

} // namespace xpp
