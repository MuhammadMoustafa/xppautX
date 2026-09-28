#ifndef _xpplim_h_
#define _xpplim_h_

#define MAXODE 5000
#define MAXODE1 4999
#define MAXDELAY 50
#define MAXPRIMEVAR (MAXODE-10)/2
#define MAXPAR 400
#define MAXFLAG 2000
#define MAXDAE 400
#define MAX_SYMBS 10000
#define MAXUFUN 50
#define MAX_TAB 50
#define MAXKER 50
#define MAXNET 50
#define MAXMARK 200
#define MAX_INTERN_SET 500
/* a user function's arguments, and the longest compiled program (expr.h) */
#define MAXARG 20
#define MAXEXPLEN 1024

/* A model's names (variables, parameters, auxiliary quantities,
   functions and their arguments, tables, fixed quantities) have no length
   limit (W76): they are std::string throughout. A display or file column
   of fixed width shortens one with short_name (xpp_util.h). */
#endif
