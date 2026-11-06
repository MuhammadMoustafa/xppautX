#pragma once
/* autoX's jobs at once (W596): a run from the current state with
   follow_all_branches on continues both directions, +DS and -DS, as two jobs
   (the settings' step_size is the direction: the jobs take |step_size| with
   each sign). Each job is an AutoxRun (autox_state.h) with its own evaluation
   constants, point buffer, table lines and fort folder, so the jobs share
   nothing they write; each runs on a thread of its own (at most as many at
   once as the thread budget has cores), the model's evaluation allowing (a
   model with function tables, networks or a delay runs its jobs one after
   another on the Session itself, the same result). The start Newton's method
   refines is refined once, before they begin. When all have ended the main
   thread joins them in a fixed order, +DS before -DS, renumbering branches and
   labels as if the jobs had run one after another, so the diagram, the .b, .s
   and .d files and the labels are those of two runs in a row, whichever thread
   finished first.

   autox_jobs.cpp. */
#include "autox_nox.h"

namespace xpp {

struct Session; /* session.h */

/* The run from the current state every starter of one ends with (autox_run.cpp): with follow_all_branches off the one
   run do_autox makes, on both directions as jobs. iold and isave as do_autox's; irs is 0. */
Result<> autox_run_from_current_state(Session &s, int iold, int isave);

} // namespace xpp
