#ifndef _xpplim_h_
#define _xpplim_h_

/* The limits of the parser's fixed tables. A model's variables, parameters
   (constants) and symbols have none (W243): the tables grow with what the
   file declares (Session::size_variables, Model::size_parameters,
   ParserState::room_for_symbol), so MAXODE, MAXODE1, MAXPAR, MAX_SYMBS,
   MAXPRIMEVAR and MAXDELAY are gone.

   What stays counts something other than variables or parameters, in a
   fixed array of its owner that no model has outgrown; raising one is its
   own change. */
#define MAXFLAG 2000       /* global flags (Model::flags) */
#define MAXDAE 400         /* algebraic variables and equations (dae_fun.cpp) */
#define MAXUFUN 50         /* user functions (expr_symbols.cpp) */
#define MAX_TAB 50         /* lookup tables (Session::tables) */
#define MAXKER 50          /* Volterra kernels (Model::kernels) */
#define MAXNET 50          /* networks (simplenet.h) */
#define MAXMARK 200        /* Markov chains (Model::markov) */
#define MAX_INTERN_SET 500 /* internal sets (the @-sets a model names) */
/* a user function's arguments */
#define MAXARG 20

/* A model's names (variables, parameters, auxiliary quantities,
   functions and their arguments, tables, fixed quantities) have no length
   limit (W76): they are std::string throughout. A display or file column
   of fixed width shortens one with short_name (xpp_util.h). */
#endif
