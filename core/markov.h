#pragma once

#include <functional>
#include <span>
#include <string>
#include <vector>
#include "storage.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */
struct RunReport; /* integrate.h */

void  make_gill_nu(Session &s, double *nu, int n, int m, double *v);
void  one_gill_step(Session &s, int meth, int nrxn, int *rxn, double *v, double *r);

/* the Markov variables and the Wiener parameters (markov.cpp), in the Session s: a run's;
   a load's are model_markov.h's */
/* every Ornstein-Uhlenbeck process to its start (a run's) */
void start_ou(Session &s);
/* the noise of the step of dt about to be taken: Markov chains, Wiener parameters and Ornstein-Uhlenbeck processes, once for all its stages */
void set_wieners(Session &s, double dt, double *x, double t);
void update_markov(Session &s, double *x, double t, double dt);
double new_state(Session &s, double old, int index, double dt);
/* the seed the next run starts from (the Session's random numbers seeded with it at once), or why it is not one */
Result<> set_random_seed(Session &s, int seed);
} // namespace xpp
