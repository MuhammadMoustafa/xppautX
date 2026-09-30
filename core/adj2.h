#ifndef _adj2_h_
#define _adj2_h_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void init_trans(void);
void dump_transpose_info(FILE *fp, int f);
int do_transpose(void);
int create_transpose(void);
void alloc_h_stuff(void);
void data_back(void);
void adj_back(void);
void h_back(void);
void make_adj_com(int com);
void adjoint_parameters(void);
void new_h_fun(int silent);
void dump_h_stuff(FILE *fp, int f);
int make_h(float **orb, float **adj, float **h, int nt, double dt, int node,int silent);
void new_adjoint(void);
void do_liapunov(void);
void alloc_liap(int n);
void do_this_liaprun(int i, double p);
void norm_vec(double *v, double *mu, int n);



#ifdef __cplusplus
}

#include "xpp_error.h"

/* adjoints and the maximal Liapunov exponent return why they failed; the
   command (new_adjoint, do_liapunov) shows it */
xpp::Result<> adjoint(float **orbit, float **adjnt, int nt, double dt, double eps, double minerr, int maxit, int node);
xpp::Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt);
xpp::Result<double> hrw_liapunov(double eps);
#endif
#endif

