#pragma once

/* A model has no fixed table: its variables, parameters (constants),
   symbols (W243), tables, user functions, kernels, networks, vectorizers,
   Markov chains, algebraic variables and equations, global flags,
   internal sets, options and lines (W490) are vectors that grow with what
   the file declares, so MAXODE, MAXPAR, MAX_SYMBS, MAXFLAG, MAXDAE,
   MAXUFUN, MAX_TAB, MAXKER, MAXNET, MAXVEC, MAXMARK, MAX_INTERN_SET,
   MAXOPT and MAXLINES are gone. What stays is a size of the method. */

/* the arguments a user function may take: the symbol table's ARG1..ARG20
   (expr_symbols.cpp) are the names its body reads them by */
#define MAXARG 20

/* A model's names (variables, parameters, auxiliary quantities,
   functions and their arguments, tables, fixed quantities) have no length
   limit (W76): they are std::string throughout. A display or file column
   of fixed width shortens one with short_name (xpp_util.h). */
