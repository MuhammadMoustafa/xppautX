#ifndef _adj2_h_
#define _adj2_h_

#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

void dump_transpose_info(FILE *fp, int f);
void alloc_liap(int n);
void norm_vec(double *v, double *mu, int n);



#ifdef __cplusplus
}

#include "xpp_error.h"


namespace xpp {
struct Session; /* session.h */
}

void adjoint_parameters(xpp::Session &s);
void adj_back(xpp::Session &s);
void h_back(xpp::Session &s);

void init_trans(xpp::Session &s);
int do_transpose(xpp::Session &s);
int create_transpose(xpp::Session &s);
void alloc_h_stuff(xpp::Session &s);
void data_back(xpp::Session &s);
void make_adj_com(xpp::Session &s, int com);
void new_h_fun(xpp::Session &s, int silent);
void dump_h_stuff(xpp::Session &s, FILE *fp, int f);
int make_h(xpp::Session &s, float **orb, float **adj, float **h, int nt, double dt, int node,int silent);
void new_adjoint(xpp::Session &s);
void do_liapunov(xpp::Session &s);
void do_this_liaprun(xpp::Session &s, int i, double p);

/* adjoints and the maximal Liapunov exponent return why they failed; the
   command (new_adjoint, do_liapunov) shows it */
xpp::Result<> adjoint(xpp::Session &s, float **orbit, float **adjnt, int nt, double dt, double eps, double minerr, int maxit, int node);
xpp::Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt);
xpp::Result<double> hrw_liapunov(xpp::Session &s, double eps);
#endif
#endif

