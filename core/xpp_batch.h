#pragma once

#include "comline.h"
#include "model_load.h"

namespace xpp {

struct Session; /* session.h */

/* The program's start: what the readers and the front ends call is declared
   below the application, where they are (model_failed.h, the kernel's: a reader's failure;
   model_load.h: the load and the batch start, and the ModelLoader the converters are handed); this file holds what only the
   program's start and the unit tests call. */

/* the command line's switches into the loading Session s and the model's
   file into its Model's this_file; the Model keeps cl. */
void apply_command_line(Session &s, CommandLine cl);

/* the commands the application defines (a load, the preferences, the updates) handed to the registry, which is below it; once at the start, before any command is looked up */
void install_application_commands();

/* load_model and inspect_model, as the ModelLoader (model_load.h) the converters are handed */
extern const ModelLoader program_loader;

} // namespace xpp
