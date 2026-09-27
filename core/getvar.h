#ifndef XPP_GETVAR_H
#define XPP_GETVAR_H
#include "model.h"
#include "session.h"
#define SETVAR(i,x) if((i)<xpp::model().nvar)xpp::session().parser.variables[(i)]=(x);
#define GETVAR(i) (i)<xpp::model().nvar ? xpp::session().parser.variables[(i)]:0.0
#endif
