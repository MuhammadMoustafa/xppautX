#ifndef _newpars_h
#define _newpars_h

#include "xpplim.h"

#define COMMAND -1
#define FIXED 0
#define FUNCTION 1
#define IC 2
#define MAP 3
#define ODE 4
#define VEQ 5
#define MARKOV_VAR 6
#define AUX_VAR 7
#define TABLE 8

#define SPEC_FUN 11

#define DAE 12
#define DERIVE_PAR 13
#define SOL_VAR 14


#define ONLY 26

#define GROUP 27

#define VECTOR 28

#define NAMLEN XPP_NAME_MAX
#define MAXARG 20
#define MAXEXPLEN 1024

#ifdef __cplusplus
#include <string>
#include <vector>

/* one line of a model as form_ode.cpp's parse_a_string splits it: its
   kind, the text left and right of its '=' and a function's argument
   names */
struct VAR_INFO {
  int type=0;
  std::string lhs,rhs;
  std::vector<std::string> args;
};
#endif

#endif
