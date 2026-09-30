#ifndef _my_rhs_h_
#define _my_rhs_h_
/* The model's right-hand side (my_rhs.cpp), in the Session s: the run's,
   which the solvers reach through IntegratorState::rhs (integrate.h).
   C++ only. */
namespace xpp {
struct Session; /* session.h */
}

/* the extras (fixed quantities, Markov variables' values, auxiliaries)
   of y__y at t, nod ODEs of neq values */
void extra(xpp::Session &s, double *y__y, double t, int nod, int neq);
/* the variables at t, y and the fixed quantities and networks they give */
void set_fix_rhs(xpp::Session &s, double t, double *y);
/* ydot, the right-hand side at t, y (neq values): the solvers' */
int my_rhs(xpp::Session &s, double t, double *y, double *ydot, int neq);
/* the fixed quantities and networks from the variables as they are */
void update_based_on_current(xpp::Session &s);
/* the fixed quantities from the variables as they are */
void fix_only(xpp::Session &s);
/* ydot, the ODEs' right-hand sides from the variables as they are */
void rhs_only(xpp::Session &s, double *ydot);
void vec_rhs(double t, double *y, double *ydot, int neq);
#endif
