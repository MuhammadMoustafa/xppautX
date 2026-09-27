#ifndef _comline_h_
#define _comline_h_
#ifdef __cplusplus
extern "C" {
#endif






void do_comline(int argc, char **argv);
int if_needed_select_sets(void);
int if_needed_load_set(void);
int if_needed_load_par(void);
int if_needed_load_ic(void);
int if_needed_load_ext_options(void);
int parse_it(const char *com);

/* the command line's switches: -include (loadincludefile), the internal
   sets it picked (Nintern_2_use), -qsets/-qpars/-qics (querysets,
   querypars, queryics), -dryrun and -newseed */
extern int loadincludefile,Nintern_2_use;
extern int querysets,querypars,queryics,dryrun,newseed;


#ifdef __cplusplus
}

#include <string>
#include <vector>
/* -include's files, in order (form_ode.cpp parses them with the model) */
extern std::vector<std::string> include_files;
#endif
#endif
