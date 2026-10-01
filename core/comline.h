#ifndef _comline_h_
#define _comline_h_

#include <string>
#include <string_view>
#include <vector>

namespace xpp {

struct Model;   /* model.h */
struct Session; /* session.h */

/* the command line's switches: -include (loadincludefile), -qsets/-qpars/
   -qics (querysets, querypars, queryics), -dryrun and -newseed (the
   internal sets it picked are batch_options') */
extern int loadincludefile;
extern int querysets,querypars,queryics,dryrun,newseed;
/* -include's files, in order (form_ode.cpp parses them with the model) */
extern std::vector<std::string> include_files;

/* the command line read into the loading Session s (its switches, the
   model's file into its Model's this_file), one argument of it */
void do_comline(Session &s, int argc, char **argv);
int parse_it(Session &s, std::string_view com);
/* the internal sets of m the command line picked (-internset, -uset,
   -rset) into batch_options */
int if_needed_select_sets(const Model &m);
/* after a load, in every mode: the files the command line names
   (-setfile, -parfile, -icfile) and its -readset/-with options, into s */
void load_command_line_values(Session &s);

} // namespace xpp
#endif
