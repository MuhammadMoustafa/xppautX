#pragma once

#include "comline.h"

namespace xpp {

struct Session; /* session.h */

/* The program's start: what the readers and the front ends call is declared
   below the application, where they are (model_failed.h: a reader's failure;
   model_load.h: the load and the batch start); this file holds what only the
   program's start and the unit tests call. */

/* the command line's switches into the loading Session s and the model's
   file into its Model's this_file; the Model keeps cl. */
void apply_command_line(Session &s, CommandLine cl);

} // namespace xpp
