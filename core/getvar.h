#ifndef XPP_GETVAR_H
#define XPP_GETVAR_H
#include "model.h"
#define SETVAR(i,x) if((i)<xpp::model().nvar)variables[(i)]=(x);
#define GETVAR(i) (i)<xpp::model().nvar ? variables[(i)]:0.0
#endif
