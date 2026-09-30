#ifndef XPP_GETVAR_H
#define XPP_GETVAR_H
#include "model.h"
#include "session.h"
/* the expression engine's variable i in the Session s (0 is t, then the
   Model's variables, fixed quantities, Markov variables ...): setvar sets
   it, getvar reads it (0 past the Model's variables) */
inline void setvar(xpp::Session &s, int i, double x)
{
  if(i<s.model().nvar)s.parser.variables[i]=x;
}
inline double getvar(const xpp::Session &s, int i)
{
  return i<s.model().nvar ? s.parser.variables[i] : 0.0;
}
#endif
