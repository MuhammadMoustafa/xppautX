#ifndef _comline_h_
#define _comline_h_
#ifdef __cplusplus
extern "C" {
#endif






void do_comline(int argc, char **argv);
int if_needed_select_sets(void);
/* after a load, in every mode: the files the command line names
   (-setfile, -parfile, -icfile) and its -readset/-with options */
void load_command_line_values(void);
int parse_it(const char *com);

/* the command line's switches: -include (loadincludefile), -qsets/-qpars/
   -qics (querysets, querypars, queryics), -dryrun and -newseed (the
   internal sets it picked are batch_options') */
extern int loadincludefile;
extern int querysets,querypars,queryics,dryrun,newseed;


#ifdef __cplusplus
}

#include <string>
#include <vector>
/* -include's files, in order (form_ode.cpp parses them with the model) */
extern std::vector<std::string> include_files;
#endif
#endif
