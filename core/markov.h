#ifndef _markov_h_
#define _markov_h_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* markov.c */
void add_wiener(int index);
void set_wieners(double dt, double *x, double t);
void add_markov(int nstate, const char *name);
int build_markov(const char *const *ma, const char *name);
int old_build_markov(FILE *fptr, const char *name);
void create_markov(int nstates, double *st, int type, const char *name);
void add_markov_entry(int index, int j, int k, const char *expr);
void compile_all_markov(void);
int compile_markov(int index, int j, int k);
void update_markov(double *x, double t, double dt);
double new_state(double old, int index, double dt);
void  make_gill_nu(double *nu, int n, int m, double *v);
void  one_gill_step(int meth, int nrxn, int *rxn, double *v);
void  do_stochast_com(int i);
void  mean_back(void);
void  variance_back(void);
void  compute_em(void);
void  free_stoch(void);
void  init_stoch(int len);
void  append_stoch(int first, int length);
void  do_stats(int ierr);
double gammln(double xx);
double poidev(double xm);
double ndrand48(void);
void nsrand48(int seed);
double ran1(long *idum);


#ifdef __cplusplus
}
#endif
#endif
