#ifndef _adj2_h_
#define _adj2_h_

#include <cstdio>
#include <span>

#include "storage.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* adj2.cpp's derived data sets, a Session's (session.h): the adjoint, its
   H function and the transposed data, each its own columns with the
   stored ones lent beside them (storage.h's LentColumns), whether it
   exists and how many rows it has; shown in the browser in the stored
   data's place (adj_back, h_back, create_transpose). A load's new Session
   starts with none, so nothing of the Session before is shown again. */
struct AdjointState {
  LentColumns adjoint, h_function, transposed;
  int adjoint_rows = 0, h_rows = 0;
  bool adjoint_here = false, h_here = false, transposed_here = false;
  /* the H function has its odd and even parts too (more than two
     equations) */
  bool h_odd_even = false;
};

void dump_transpose_info(FILE *fp, int f);
void alloc_liap(int n);
/* v scaled to unit length (unless it is 0), and its length mu */
void norm_vec(std::span<double> v, double &mu);

void adjoint_parameters(Session &s);
void adj_back(Session &s);
void h_back(Session &s);

void init_trans(Session &s);
int do_transpose(Session &s);
int create_transpose(Session &s);
void alloc_h_stuff(Session &s);
void data_back(Session &s);
void make_adj_com(Session &s, int com);
void new_h_fun(Session &s, int silent);
void dump_h_stuff(Session &s, FILE *fp, int f);
int make_h(Session &s, float **orb, float **adj, float **h, int nt, double dt, int node,int silent);
void new_adjoint(Session &s);
void do_liapunov(Session &s);
void do_this_liaprun(Session &s, int i, double p);

/* adjoints and the maximal Liapunov exponent return why they failed; the
   command (new_adjoint, do_liapunov) shows it */
Result<> adjoint(Session &s, float **orbit, float **adjnt, int nt, double dt, double eps, double minerr, int maxit, int node);
Result<> step_eul(double **jac, int k, int k2, double *yold, double *work, int node, double dt);
Result<double> hrw_liapunov(Session &s, double eps);

} // namespace xpp
#endif
