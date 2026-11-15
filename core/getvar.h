#pragma once
#include <vector>
#include "expr.h"
/* the expression engine's variable i in the parser p (0 is t, then the
   Model's variables, fixed quantities, Markov variables ...): setvar sets
   it, getvar reads it (0 past the Model's variables) */
inline void setvar(xpp::ParserState &p, int i, double x)
{
  if(i<p.compiler->nvar)p.variables[i]=x;
}
inline double getvar(const xpp::ParserState &p, int i)
{
  return i<p.compiler->nvar ? p.variables[i] : 0.0;
}
