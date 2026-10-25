#pragma once

#include "xpp_error.h"
#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */

/* the command line's switches: --qsets/--qpars/--qics (querysets, querypars,
   queryics), dryrun (any of them) and --newseed (the internal sets it
   picked are batch_options') */
extern bool querysets,querypars,queryics,dryrun,newseed;
/* --include's files, in order (form_ode.cpp parses them with the model) */
extern std::vector<std::string> include_files;

/* Read the whole command line in one pass: every option's spelling, argument
   and value checked; the first fault is an error naming the option, at its
   place in the command line. Run before any mode starts. */
Result<> check_command_line(int argc, char **argv);
/* the same pass, then its switches into the loading Session s and the model's
   file into its Model's this_file; the model argument's index, or -1 when a
   file still needs choosing. A fault throws LoadFailed. */
int do_comline(Session &s, int argc, char **argv);
/* the internal sets of m the command line picked (--internset, --uset,
   --rset) into batch_options */
int if_needed_select_sets(const Model &m);
/* after a load, in every mode: the files the command line names
   (--setfile, --parfile, --icfile) and its --readset/--with options, into s */
void load_command_line_values(Session &s);

} // namespace xpp
