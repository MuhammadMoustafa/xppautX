#pragma once

/* Why autoX ended a branch (docs/ui-v2.md T23, docs/protocol.md "The autoX
   diagram as data", `autoxinfo` `stop`): a parameter or norm limit, the
   point count, the user's Stop, a Mark value set to stop, or no
   convergence. autoX itself only labels the end EP or MX; autox_lib1.cpp's
   stplae and stplbv, which decide that the branch ends, report the point
   here, and the places that write fort.9's "No convergence" NOTEs say
   which one it was. Nothing here changes what autoX computes: it only
   records and reports.

   Each branch end writes a line in autoX's Output ("Branch 1 stopped at
   point 57: parameter iapp reached Par Max (0.45)", xpp::log_autox); the
   last one is what the `autoxinfo` event's "stop" holds until the next run
   starts or autoX's window is new.

   autox_stop.cpp; nothing escapes it. */

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "autox_ends.h"
#include "autox_global.h"

struct AutoxRun; /* autox_state.h */

namespace xpp {

enum AutoxStopWhy {
    AUTOX_STOP_NONE = 0,
    AUTOX_STOP_PAR_MIN,     /* the parameter below Par Min (RL0) */
    AUTOX_STOP_PAR_MAX,     /* the parameter above Par Max (RL1) */
    AUTOX_STOP_NORM_MIN,    /* the norm below Norm Min (A0) */
    AUTOX_STOP_NORM_MAX,    /* the norm above Norm Max (A1) */
    AUTOX_STOP_NPTS,        /* Max points (NMX) reached */
    AUTOX_STOP_USER,        /* Stop (a cancel) */
    AUTOX_STOP_MARK,        /* a Mark value set to stop (a UZR endpoint) */
    AUTOX_STOP_NOCONV,      /* no convergence, no NOTE said how */
    AUTOX_STOP_NOCONV_FIXED,        /* ... with a fixed step size (IADS 0) */
    AUTOX_STOP_NOCONV_MIN,          /* ... even at the smallest step (Dsmin) */
    AUTOX_STOP_NOCONV_SWITCH_FIXED, /* ... switching branches, fixed step */
    AUTOX_STOP_NOCONV_SWITCH_MIN,   /* ... switching branches, smallest step */
    AUTOX_STOP_JACOBIAN,    /* the exact Jacobian was not finite (W275): a cancel the run made itself */
    AUTOX_STOP_PERIOD,      /* a cycle branch's period grows without bound, the end not named (W518) */
    AUTOX_STOP_SNIC,        /* ... at a saddle-node on an invariant circle */
    AUTOX_STOP_HOMOCLINIC,  /* ... at a homoclinic orbit to a saddle */
    AUTOX_STOP_HETEROCLINIC,/* ... at a heteroclinic cycle between saddles */
    AUTOX_STOP_HOPF_END,    /* a cycle branch's amplitude fell to about zero at a Hopf point (W604) */
    AUTOX_STOP_CLOSED,      /* the branch came back to its own start (W604) */
    AUTOX_STOP_N
};

/* a "No convergence" NOTE: which one (an AUTOX_STOP_NOCONV_*), the step
   size then, and the settings that gave up: Dsmin and the Newton
   iterations' ITNW, EPSL, EPSU (0 and NAN where a solver has none); the
   last linear solve's smallest pivot magnitude (xpp::ge; NAN: not known),
   which says a singular system when below autox_rsmall */
struct AutoxNoConvergence {
    int why = AUTOX_STOP_NOCONV;
    double ds = NAN, dsmin = NAN;
    std::ptrdiff_t itnw = 0;
    double epsl = NAN, epsu = NAN;
    double pivot = NAN;
};

/* what autoX runs recorded: one run's (AutoxRun::stop), or the last runs'
   a Session shows (AutoxState::stop) */
struct AutoxStop {
    /* the last "No convergence" NOTE */
    AutoxNoConvergence noconv;
    /* the last branch end: why, where, what reached which limit, in words */
    int last_why = AUTOX_STOP_NONE;
    long last_br = 0, last_pt = 0;
    double last_value = NAN, last_limit = NAN;
    std::string last_text;
    /* what the run lost or could not trust (autox_stop_note), in words */
    std::vector<std::string> notes;
    /* the run's start was refined by Newton before it ran (autox_evd.cpp), so it is not the start as given */
    bool start_refined = false;
    /* the cycle branch's periods so far, and the end its period growing without bound was (W518) */
    AutoxPeriodWatch period_watch;
    AutoxGlobalEnd global;
    /* the cycle branch's amplitudes so far, and the Hopf point its amplitude fell to zero at (W604) */
    AutoxAmplitudeWatch amplitude_watch;
    AutoxHopfEnd hopf_end;
    bool hopf_failure_noted = false; /* the model failed where the Hopf end was checked: said once per run */
    /* the branch came back to its start: autox_point_gap there, kept from the crossing that found it until the point
       is stored (W604) */
    std::optional<double> closure_gap;
    double closure_epss = NAN; /* the EPSS it is measured in */
};

/* a "No convergence" NOTE; the branch end that follows says "no
   convergence" in its words, with the settings */
void autox_stop_noconv(AutoxStop &st, const AutoxNoConvergence &noconv);

/* the point at which stplae/stplbv end a branch, and what they saw */
struct AutoxStopAt {
    long br, pt;      /* autoX's branch and point number (ibr, ntot) */
    long ipar;        /* autoX's index of the continuation parameter, icp[0] */
    double par, norm; /* its value and the norm autoX checks (amp) */
    double rl0, rl1, a0, a1;
    long nmx;
    int noconv; /* the solver gave up (istop 1, no cancel): autoX labels it MX */
    int mark;   /* istop -1: a Mark value set to stop */
    int user;   /* stopped by the user (byeautox's iflag, a cancel) */
    int global; /* a cycle branch's period grows without bound: AutoxStop::global says where (W518) */
    bool hopf_end; /* a cycle branch's amplitude fell to zero at a Hopf point: AutoxStop::hopf_end says where (W604) */
    bool closed;   /* the branch came back to its start: AutoxStop::closure_gap says how exactly (W604) */
};

/* why a branch ending at `at` ended; the first of: the user, no
   convergence, a Mark value, Par Min, Par Max, Norm Min, Norm Max, Max
   points; a solver that gave up, in the words of st's last NOTE. Pure */
int autox_stop_why(const AutoxStop &st, const AutoxStopAt *at);


/* a point the run lost or computed unreliably (what fort.9's NOTEs say: a
   special point not located, inaccurate Floquet multipliers): kept for the
   autoxinfo event's "notes" until the next run starts, and written in
   autoX's Output (a WARN where autoX's Output is not shown) */
void autox_stop_note(AutoxRun &run, std::string text);

/* a run's end: what it recorded joins what the Session shows (shown): its
   last branch end, if it had one, replaces the last one; its notes follow
   the earlier ones; a refined start stays marked */
void autox_stop_merge(AutoxStop &shown, AutoxStop &&run);

/* a run starts, or autoX's window is new: no reason until a branch ends */
void autox_stop_clear(AutoxStop &st);

/* the last branch end, for the autoxinfo event; why is AUTOX_STOP_NONE when
   there is none. key and text stay valid until the next branch end or clear */
struct AutoxStopInfo {
    int why;
    const char *key;  /* "parmax", "noconv-min", ... (docs/protocol.md) */
    const char *text; /* "parameter iapp reached Par Max (0.45)" */
    long br, pt;
    double value; /* what reached the limit (NAN when nothing did) */
    double limit; /* the limit (NAN when there is none) */
};
void autox_stop_last(const AutoxStop &st, AutoxStopInfo *out);

/* the branch ended at `at` in the run: record why (its parameter by the
   name its Session gives it) and write it in autoX's Output */
void autox_stop_branch_end(AutoxRun &run, const AutoxStopAt *at);

} // namespace xpp
