#ifndef AUTO_STOP_H
#define AUTO_STOP_H

/* Why AUTO ended a branch (docs/ui-v2.md T23, docs/protocol.md "The AUTO
   diagram as data", `autoinfo` `stop`): a parameter or norm limit, the
   point count, the user's Stop, a Mark value set to stop, or no
   convergence. AUTO itself only labels the end EP or MX; autlib1.c's
   stplae and stplbv, which decide that the branch ends, report the point
   here, and the places that write fort.9's "No convergence" NOTEs say
   which one it was. Nothing here changes what AUTO computes: it only
   records and reports.

   Each branch end writes a line in AUTO's Output ("Branch 1 stopped at
   point 57: parameter iapp reached Par Max (0.45)", xpp::log_auto_printf); the
   last one is what the `autoinfo` event's "stop" holds until the next run
   starts or AUTO's window is new.

   auto_stop.cpp; nothing escapes it. */

#include <cmath>
#include <string>

namespace xpp {

struct Session; /* session.h */

enum AutoStopWhy {
    AUTO_STOP_NONE = 0,
    AUTO_STOP_PAR_MIN,     /* the parameter below Par Min (RL0) */
    AUTO_STOP_PAR_MAX,     /* the parameter above Par Max (RL1) */
    AUTO_STOP_NORM_MIN,    /* the norm below Norm Min (A0) */
    AUTO_STOP_NORM_MAX,    /* the norm above Norm Max (A1) */
    AUTO_STOP_NPTS,        /* Max points (NMX) reached */
    AUTO_STOP_USER,        /* Stop (a cancel) */
    AUTO_STOP_MARK,        /* a Mark value set to stop (a UZR endpoint) */
    AUTO_STOP_NOCONV,      /* no convergence, no NOTE said how */
    AUTO_STOP_NOCONV_FIXED,        /* ... with a fixed step size (IADS 0) */
    AUTO_STOP_NOCONV_MIN,          /* ... even at the smallest step (Dsmin) */
    AUTO_STOP_NOCONV_SWITCH_FIXED, /* ... switching branches, fixed step */
    AUTO_STOP_NOCONV_SWITCH_MIN,   /* ... switching branches, smallest step */
    AUTO_STOP_N
};

/* what a Session's AUTO runs recorded (AutoState::stop) */
struct AutoStop {
    /* the last "No convergence" NOTE: which one, the step size then and Dsmin */
    int noconv_why = AUTO_STOP_NOCONV;
    double noconv_ds = NAN, noconv_dsmin = NAN;
    /* the last branch end: why, where, what reached which limit, in words */
    int last_why = AUTO_STOP_NONE;
    long last_br = 0, last_pt = 0;
    double last_value = NAN, last_limit = NAN;
    std::string last_text;
};

/* a "No convergence" NOTE: which one (an AUTO_STOP_NOCONV_*), the step
   size then and Dsmin; the branch end that follows says "no convergence"
   in its words */
void auto_stop_noconv(AutoStop &st, int why, double ds, double dsmin);

/* the point at which stplae/stplbv end a branch, and what they saw */
struct AutoStopAt {
    long br, pt;      /* AUTO's branch and point number (ibr, ntot) */
    long ipar;        /* AUTO's index of the continuation parameter, icp[0] */
    double par, norm; /* its value and the norm AUTO checks (amp) */
    double rl0, rl1, a0, a1;
    long nmx;
    int noconv; /* the solver gave up (istop 1, no cancel): AUTO labels it MX */
    int mark;   /* istop -1: a Mark value set to stop */
    int user;   /* stopped by the user (byeauto's iflag, a cancel) */
};

/* why a branch ending at `at` ended; the first of: the user, no
   convergence, a Mark value, Par Min, Par Max, Norm Min, Norm Max, Max
   points; a solver that gave up, in the words of st's last NOTE. Pure */
int auto_stop_why(const AutoStop &st, const AutoStopAt *at);


/* a run starts, or AUTO's window is new: no reason until a branch ends */
void auto_stop_clear(AutoStop &st);

/* the last branch end, for the autoinfo event; why is AUTO_STOP_NONE when
   there is none. key and text stay valid until the next branch end or clear */
struct AutoStopInfo {
    int why;
    const char *key;  /* "parmax", "noconv-min", ... (docs/protocol.md) */
    const char *text; /* "parameter iapp reached Par Max (0.45)" */
    long br, pt;
    double value; /* what reached the limit (NAN when nothing did) */
    double limit; /* the limit (NAN when there is none) */
};
void auto_stop_last(const AutoStop &st, AutoStopInfo *out);

/* the key of an AutoStopWhy ("parmax", ...), or NULL out of range */
const char *auto_stop_key(int why);

/* the branch ended at `at` in the session s's run: record why (its
   parameter by the name s gives it) and write it in AUTO's Output */
void auto_stop_branch_end(Session &s, const AutoStopAt *at);

} // namespace xpp
#endif
