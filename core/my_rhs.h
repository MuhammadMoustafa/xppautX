#pragma once
#include <cstdint>
/* The model's right-hand side (my_rhs.cpp), in the Session s: the run's,
   which the solvers reach through IntegratorState::rhs (integrate.h).
   C++ only. */
namespace xpp {
struct RunParts; /* run_parts.h */
/* what a formula's network(x) and vector(x) give (ExprHost::network_value, vector_value): the network or vector `i` at index x */
double network_value(RunParts &s, double x, int i);
double vector_value(RunParts &s, double x, int i);
}

/* the extras (fixed quantities, Markov variables' values, auxiliaries)
   of y__y at t, nod ODEs of neq values */
void extra(xpp::RunParts &s, double *y__y, double t, int nod, int neq);
/* the variables at t, y and the fixed quantities and networks they give */
void set_fix_rhs(xpp::RunParts &s, double t, double *y);
/* ydot, the right-hand side at t, y (neq values): the solvers' */
int my_rhs(xpp::RunParts &s, double t, double *y, double *ydot, int neq);
/* the Session's right-hand side is the model's own (my_rhs) and translate_model proved it pure (Model::rhs_impurity): a run on
   a copy of the Session's state gives the Session's numbers (autoX's collocation workers, a sweep's runs) */
bool pure_rhs_ready(const xpp::RunParts &s);
/* autoX only, after translate_model proved this path uses only its arrays. */
void pure_rhs(const xpp::RunParts &s, const double *y, double *ydot, double *c, double *v, std::uint64_t &native_work);
/* the fixed quantities and networks from the variables as they are */
void update_based_on_current(xpp::RunParts &s);
/* the fixed quantities from the variables as they are */
void fix_only(xpp::RunParts &s);
/* ydot, the ODEs' right-hand sides from the variables as they are */
void rhs_only(xpp::RunParts &s, double *ydot);
void vec_rhs(double t, double *y, double *ydot, int neq);
