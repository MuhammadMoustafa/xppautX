#ifndef AUTO_C_H
#define AUTO_C_H
/* AUTO's own API between its files (autlib1-3/5, conpar2,
   setubv2) and the AUTO front end (gogoauto, auto_nox, autevd). */
#include <stdio.h>
#include <vector>
#include <span>
#include <functional>
#include "auto_f2c.h"
#include "auto_codes.h"
#include "xAuto.h" /* AutoMark */
#include "xpp_math.h" /* xpp::Matrix, Matrix3 */
#include "xpp_math_auto.h" /* AUTO numerical interfaces, owned by xpp_math */

namespace xpp { class TokenReader; struct AutoSolutionHeader; }

struct AutoLib; /* auto_state.h */

/* AUTO's numerical thresholds and tolerances (autlib1-5, auto_jacobian). */
inline constexpr doublereal auto_hmach = 1.0e-7;            // Relative finite-difference step and convergence tolerance
inline constexpr doublereal auto_rlarge = 1.0e+30;          // Practical infinity for uncomputed bounds and eigenvalues
inline constexpr doublereal auto_hmach1 = auto_hmach + 1.0; // 1 + HMACH for multiplier unit circle comparisons
inline constexpr doublereal auto_hmachho = 1.0e-13;         // HomCont component zero threshold (compzero)

/* AUTO's file schemas and output record layouts (autlib1-2, auto_nox). */
inline constexpr int auto_solution_columns = 7;             // Floating-point values per line in AUTO solution files (fort.8 / .s / fort.3)
inline constexpr int auto_solution_header_fields = 12;      // Number of fields in AUTO solution file header records
inline constexpr int auto_solution_parameter_indices_per_line = 20; // Free parameter indices per line in fort.8 (5 characters each)
inline constexpr int auto_diagram_columns = 7;              // Maximum parameter and state component columns on a bifurcation diagram row (fort.7/fort.9)
inline constexpr int auto_heading_columns = auto_diagram_columns + 2; // Column heading slots in headng (1-based index with vaxis and 7 components)
inline constexpr int auto_column_width = 14;                // Fixed character width for bifurcation diagram columns in fort.7/fort.9 and window
inline constexpr integer auto_point_label_modulo = 10000;   // Wraps diagnostic and output point numbers to four decimal digits (Fortran fixed-width PT column)


struct iap_type {
  /* 1 */ integer ndim;
  /* 2 */ xpp::AutoProblem ips;
  /* 3 */ integer irs;
  /* 4 */ integer ilp;
  /* 5 */ integer ntst;
  /* 6 */ integer ncol;
  /* 7 */ integer iad;
  /* 8 */ integer iads;
  /* 9 */ integer isp;
  /* 10 */ integer isw;
  /* 11 */ integer iplt;
  /* 12 */ integer nbc;
  /* 13 */ integer nint;
  /* 14 */ integer nmx;
  /* 15 */ integer nuzr;
  /* 16 */ integer npr;
  /* 17 */ integer mxbf;
  /* 18 */ integer iid;
  /* 19 */ integer itmx;
  /* 20 */ integer itnw;
  /* 21 */ integer nwtn;
  /* 22 */ integer jac;
  /* 23 */ integer ndm;
  /* 24 */ integer nbc0;
  /* 25 */ integer nnt0;
  /* 26 */ integer iuzr;
  /* 27 */ xpp::AutoPointCode itp;
  /* 28 */ xpp::AutoPointKind itpst; /* the start kind of a two-parameter continuation */
  /* 29 */ integer nfpr;
  /* 30 */ integer ibr;
  /* 31 */ integer nit;
  /* 32 */ integer ntot;
  /* 33 */ integer nins;
  /* 34 */ integer istop;
  /* 35 */ integer nbif;
  /* 36 */ integer ipos;
  /* 37 */ integer lab;
  /* 41 */ integer nicp;
  /* xppautX: the run's own state (auto_state.h), and through it the
     Session the run belongs to; go_go_auto sets it before init */
  struct AutoLib *lib;
};

typedef struct {
  /* 1 */ doublereal ds;
  /* 2 */ doublereal dsmin;
  /* 3 */ doublereal dsmax;
  /* There is no 4 */
  /* 5 */ doublereal dsold;
  /* 6 */ doublereal rl0;
  /* 7 */ doublereal rl1;
  /* 8 */ doublereal a0;
  /* 9 */ doublereal a1;
  /* 11 */ doublereal epsl;
  /* 12 */ doublereal epsu;
  /* 13 */ doublereal epss;
  /* 14 */ doublereal det;
  /* 15 */ doublereal tivp;
} rap_type;

/*This is the type for all functions which can be used as "funi" the function
  which evaluates the right hand side of the equations and generates
  the Jacobian*/
#define FUNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)

/*This is the type for all functions which can be used as "bcni" the function
  which evaluates the boundary conditions */
#define BCNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)

/*This is the type for all functions which can be used as "icni" the function
  which evaluates kernel of the integral constraints */
#define ICNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)

/*This is the type for all functions which can be used as additional
  output functions for algebraic problems */
#define PVLI_TYPE_AE(X) int X(iap_type *iap, rap_type *rap, doublereal *u, doublereal *par)

/*This is the type for all functions which can be used as additional
  output functions for BVPs */
#define PVLI_TYPE_BVP(X) int X(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)

/* This is the type for all functions that can be used at starting points
   for algebraic problems */
#define STPNT_TYPE_AE(X) int X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)

/* This is the type for all functions that can be used at starting points
   for BVPs */
#define STPNT_TYPE_BVP(X) int X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)

/*This is the type for all functions which can be used to detect
  special points for algebraic problems */
#define FNCS_TYPE_AE(X) doublereal X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoMark> marks)

/*This is the type for all functions which can be used to detect
  special points for BVPS */
#define FNCS_TYPE_BVP(X) doublereal X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, logical *chng, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks)

#define AUTOAE 0
#define AUTOBV 1

typedef struct {
  FUNI_TYPE((*funi));
  BCNI_TYPE((*bcni));
  ICNI_TYPE((*icni));
  STPNT_TYPE_BVP((*stpnt));
  PVLI_TYPE_BVP((*pvli));
} autobv_function_list;

typedef struct {
  FUNI_TYPE((*funi));
  STPNT_TYPE_AE((*stpnt));
  PVLI_TYPE_AE((*pvli));
} autoae_function_list;

typedef struct {
  int type;
  autobv_function_list bvlist;
  autoae_function_list aelist;
} function_list;

/* autlib1.c */
void allocate_global_memory(const iap_type);
int chdim(iap_type *iap);
int autoae(iap_type *iap, rap_type *rap, doublereal *par, 
integer *icp, 
FUNI_TYPE((*funi)), 
STPNT_TYPE_AE((*stpnt)), 
PVLI_TYPE_AE((*pvli)), 
doublereal *thl, doublereal *thu, std::span<const AutoMark> marks);

int autobv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks);
int init1(iap_type *iap, rap_type *rap, integer *icp, doublereal *par);
int cnrlae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_AE((*stpnt)), PVLI_TYPE_AE((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks);
STPNT_TYPE_AE(stpnus);
STPNT_TYPE_AE(stpnae);
int stprae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int contae(iap_type *iap, rap_type *rap, doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot);
int solvae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int lcspae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_AE((*fncs)), FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *q, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks, double &dstest, bool &stepped);
int mueller(doublereal *q0, doublereal *q1, doublereal *q, doublereal *s0, doublereal *s1, doublereal *s, doublereal *rds);
FNCS_TYPE_AE(fnbpae);
FNCS_TYPE_AE(fnlpae);
FNCS_TYPE_AE(fnhbae);
FNCS_TYPE_AE(fnbtae);
FNCS_TYPE_AE(fncpae);
FNCS_TYPE_AE(fnghae);
FNCS_TYPE_AE(fnuzae);
int stbif(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *m1aaloc, doublereal *aa, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *udot, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int swpnt(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *udot);
int swprc(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *rds, doublereal *thl, doublereal *thu);
int sthd(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, doublereal *thu);
int headng(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer iunit, integer *n1, integer *n2);
int stplae(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, doublereal *u);
int wrline(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *icu, integer *ibr, integer *ntot, integer *lab, doublereal *vaxis, doublereal *u);
int wrtsp8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *lab, doublereal *rlcur, doublereal *u);
int wrjac(iap_type *iap, integer *n, integer *m1aaloc, doublereal *aa, doublereal *rhs);
// Identical starting mesh preparation; formulation equations stay in callers.
int auto_uniform_start(iap_type *iap, rap_type *rap, doublereal *par,
                       integer *icp, integer *ntsr, integer *ncolrs,
                       doublereal *rlcur, integer ndxloc, doublereal *ups,
                       doublereal *tm, integer *nodir,
                       const std::function<void(doublereal *, doublereal &)> &evaluate);
void auto_hopf_mesh(const iap_type *iap, const rap_type *rap, integer ndxloc,
                    doublereal *ups, doublereal *udotps, doublereal *upoldp,
                    doublereal *tm, doublereal *dtm, doublereal *rldot,
                    const doublereal *u, const doublereal *rnllv, doublereal tpi);
int msh(const iap_type *iap, const rap_type *rap, doublereal *tm);
int genwts(const integer ncol, const integer n1, doublereal *wt, doublereal *wp);
int cpnts(const integer ncol, doublereal *zm);
int cntdif(integer *n, doublereal *d);
int wint(const integer n, doublereal *wi);
int adptds(iap_type *iap, rap_type *rap, doublereal *rds);
int adapt(iap_type *iap, rap_type *rap, integer *nold, integer *ncold, integer *nnew, integer *ncnew, doublereal *tm, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *vps);
int interp(iap_type *iap, rap_type *rap, integer *ndim, integer *n, integer *nc, doublereal *tm, integer *ndxloc, doublereal *ups, integer *n1, integer *nc1, doublereal *tm1, doublereal *ups1, doublereal *tm2, integer *itm1);
int newmsh(iap_type *iap, rap_type *rap, integer *ndxloc, doublereal *ups, integer *nold, integer *ncold, doublereal *tmold, doublereal *dtmold, integer *nnew, doublereal *tmnew, integer *iper);
int ordr(iap_type *iap, rap_type *rap, integer *n, doublereal *tm, integer *n1, doublereal *tm1, integer *itm1);
int intwts(iap_type *iap, rap_type *rap, integer *n, doublereal *z__, doublereal *x, doublereal *wts);
int eqdf(iap_type *iap, rap_type *rap, integer *ntst, integer *ndim, integer *ncol, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *eqf, std::span<doublereal> density, integer *iper);
int nrmlz(integer *ndim, doublereal *v);
doublereal pi(doublereal r__);
int newlab(iap_type *iap, rap_type *rap);
// Locate and re-read the validated restart header once for all formulations.
bool read_auto_restart_header(iap_type *iap, const rap_type *rap,
                              xpp::TokenReader &reader, xpp::AutoSolutionHeader &header,
                              integer *ntst, integer *ncol);
int findlb(iap_type *iap, const rap_type *rap, integer irs, integer *nfpr, logical *found);
int readlb(const iap_type *iap, const rap_type *rap, doublereal *u, doublereal *par);
int skip3(FILE *fp3, integer *nskip, logical *eof3);
doublereal rinpr(iap_type *iap, const integer *ndim1, const integer *ndxloc, doublereal *ups, doublereal *vps, doublereal *dtm, doublereal *thu);
doublereal rnrmsq(iap_type *iap, integer *ndim1, integer *ndxloc, doublereal *ups, doublereal *dtm, doublereal *thu);
doublereal rintg(iap_type *iap, integer *ndxloc, integer ic, doublereal *ups, doublereal *dtm);
doublereal rnrm2(iap_type *iap, integer *ndxloc, integer *ic, doublereal *ups, doublereal *dtm);
doublereal rmxups(iap_type *iap, integer *ndxloc, integer *i__, doublereal *ups);
doublereal rmnups(iap_type *iap, integer *ndxloc, integer *i__, doublereal *ups);
int scaleb(iap_type *iap, integer *icp, integer *ndxloc, doublereal *dvps, doublereal *rld, doublereal *dtm, doublereal *thl, doublereal *thu);
int cnrlbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), STPNT_TYPE_BVP((*stpnt)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoMark> marks);
int contbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *thl, doublereal *thu);
int extrbv(iap_type *iap, rap_type *rap, FUNI_TYPE((*funi)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps);
int stupbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *upoldp);
int stepbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int rsptbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), STPNT_TYPE_BVP((*stpnt)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *tm, doublereal *dtm, doublecomplex *ev, integer *nodir, doublereal *thl, doublereal *thu);
STPNT_TYPE_BVP(stpnbv);
STPNT_TYPE_BVP(stpnub);
int setrtn(iap_type *iap, integer *ntst, integer *ndxloc, doublereal *ups, doublereal *par);
int stdrbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dtm, integer iperp, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int lcspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_BVP((*fncs)), FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), PVLI_TYPE_BVP((*pvli)), doublereal *q, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *thl, doublereal *thu, std::span<const AutoMark> marks, double &dstest, bool &stepped);
FNCS_TYPE_BVP(fnlpbv);
FNCS_TYPE_BVP(fnbpbv);
FNCS_TYPE_BVP(fnspbv);
FNCS_TYPE_BVP(fnuzbv);
int tpspbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublecomplex *ev);
int stplbv(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu);
int wrtbv8(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm);
int wrtbv9(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, integer *ndxloc, doublereal *ups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu);
PVLI_TYPE_AE(pvlsae);
PVLI_TYPE_BVP(pvlsbv);
/* autlib2.c */
int solvbv(integer *ifst, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), doublereal *rds, integer *nllv, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int faft(doublereal *ff, doublereal *fa, integer *ntst, integer *nrow, integer *ndxloc);
int setrhs(integer *ndim, xpp::AutoProblem *ips, integer *na, integer *ncol, integer *nbc, integer *nint, integer *ncb, integer *nrc, integer *nra, integer *nca, FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), integer *ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *fa, doublereal *fc, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1);
int brbd(AutoLib &lib, FILE *fp9, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, integer *ifst, integer *idb, integer *nllv, doublereal *det, integer *nov, integer *na, integer *nbc, integer *nra, integer *nca, integer *ncb, integer *nrc);
int setzero(doublereal *fa, doublereal *fc, integer *na, integer *nra, integer *nrc);
int conrhs(integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *nbc, integer *nrc, doublereal *c, doublereal *fa, doublereal *fc, integer *irf, integer *icf);
int copycp(integer *na, integer *nov, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nrc, doublereal *c, doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, integer *irf);
int cpyrhs(integer *na, integer *nov, integer *nra, doublereal *faa, doublereal *fa, integer *irf);
int reduce(doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, doublereal *dd, integer *na, integer *nov, integer *ncb, integer *nrc, doublereal *s1, doublereal *s2, integer *icf1, integer *icf2, integer *ipr, integer *nbc);
int redrhs(doublereal *a1, doublereal *a2, doublereal *cc, doublereal *faa, doublereal *fc, integer *na, integer *nov, integer *ncb, integer *nrc, integer *icf1, integer *icf2, integer *ipr, integer *nbc);
int dimrge(AutoLib &lib, FILE *fp9, doublereal *e, doublereal *cc, doublereal *d, doublereal *fc, integer *ifst, integer *na, integer *nrc, integer *nov, integer *ncb, integer *idb, integer *nllv, doublereal *fcc, doublereal *p0, doublereal *p1, doublereal *det, doublereal *s, doublereal *a2, doublereal *faa, doublereal *bb);
int bcksub(doublereal *s1, doublereal *s2, doublereal *a2, doublereal *bb, doublereal *faa, doublereal *fc, doublereal *fcc, doublereal *sol1, doublereal *sol2, doublereal *sol3, integer *na, integer *nov, integer *ncb, integer *icf2);
int infpar(doublereal *a, doublereal *b, doublereal *fa, doublereal *sol1, doublereal *sol2, doublereal *fc, integer *na, integer *nov, integer *nra, integer *nca, integer *ncb, integer *irf, integer *icf);
int print1(FILE *fp9, integer *nov, integer *na, integer *nra, integer *nca, integer *ncb, integer *nrc, doublereal *a, doublereal *b, doublereal *c, doublereal *d, doublereal *fa, doublereal *fc);
/* autlib3.c */
FUNI_TYPE(fnlp);
int fflp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(stpnlp);
FUNI_TYPE(fnc1);
STPNT_TYPE_AE(stpnc1);
FUNI_TYPE(fnc2);
int ffc2(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(stpnc2);
FUNI_TYPE(fnds);
FUNI_TYPE(fnti);
FUNI_TYPE(fnhd);
int ffhd(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(stpnhd);
FUNI_TYPE(fnhb);
int ffhb(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(stpnhb);
FUNI_TYPE(fnhw);
int ffhw(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(stpnhw);
FUNI_TYPE(fnps);
BCNI_TYPE(bcps);
ICNI_TYPE(icps);
int pdble(const iap_type *iap, const rap_type *rap, integer *ndim, integer *ntst, integer *ncol, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par);
STPNT_TYPE_BVP(stpnps);
FUNI_TYPE(fnws);
int ffws(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
FUNI_TYPE(fnwp);
int stpnwp(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(fnsp);
int ffsp(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
FUNI_TYPE(fnpe);
int ffpe(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
ICNI_TYPE(icpe);
FUNI_TYPE(fnpl);
int ffpl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(bcpl);
ICNI_TYPE(icpl);
int stpnpl(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(fnpd);
int ffpd(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(bcpd);
ICNI_TYPE(icpd);
int stpnpd(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(fntr);
int fftr(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
int bctr(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0,const  doublereal *u1, doublereal *f, integer ijac, doublereal *dbc);
ICNI_TYPE(ictr);
int stpntr(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(fnpo);
int ffpo(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const doublereal *upold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(bcpo);
ICNI_TYPE(icpo);
int fipo(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint, integer ndmt, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_BVP(stpnpo);
FUNI_TYPE(fnbl);
int ffbl(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(bcbl);
int fbbl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *f, doublereal *dbc);
ICNI_TYPE(icbl);
int fibl(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, doublereal *dint);
STPNT_TYPE_BVP(stpnbl);
FUNI_TYPE(funi);
BCNI_TYPE(bcni);
ICNI_TYPE(icni);
int fopi(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp);
/* autlib5.c */
FUNI_TYPE(fnho);
int ffho(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(bcho);
int fbho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *fb, doublereal *dbc);
ICNI_TYPE(icho);
int fiho(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint);
int inho(iap_type *iap, integer *icp, doublereal *par);
int preho(AutoLib &lib, integer *ndx, integer *ntsr, integer *nar, integer *ndim, integer *ncolrs, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par);
int stpnho(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
int stpho(iap_type *iap, integer *icp, doublereal *u, doublereal *par, doublereal *t);
PVLI_TYPE_BVP(pvlsho);
doublereal psiho(const iap_type *iap, integer is, doublereal *rr, doublereal *ri, doublereal *v, doublereal *vt, const integer *icp, doublereal *par);
int eighi(const iap_type *iap, integer isign, integer itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm);
int prjcti(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, integer imfd, integer is, integer itrans, integer *ndm);


/* problem defined functions (func, stpnt and bcnd, which run the
   model, are below) */
int icnd(integer ndim, const doublereal *par, const integer *icp, integer nint, 
	 const doublereal *u, const doublereal *uold, const doublereal *udot, 
	 const doublereal *upold, integer ijac,
	 doublereal *fi, doublereal *dint);
int fopt(integer ndim, const doublereal *u, const integer *icp, 
	 const doublereal *par, integer ijac, 
	 doublereal *fs, doublereal *dfdu, doublereal *dfdp);
int pvls(integer ndim, const doublereal *u, doublereal *par);
/* conpar2.cpp */
int conpar(AutoLib &lib, integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nbc, integer *nrc, doublereal *c, doublereal *d, integer *irf, integer *icf);
/* setubv2.cpp */
int setubv(integer ndim, xpp::AutoProblem ips, integer na, integer ncol, integer nbc, integer nint, integer ncb, integer nrc, integer nra, integer nca, 
	   FUNI_TYPE((*funi)), BCNI_TYPE((*bcni)), ICNI_TYPE((*icni)), integer ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, 
	   doublereal rds, xpp::Matrix3<doublereal> aa, xpp::Matrix3<doublereal> bb, xpp::Matrix3<doublereal> cc, xpp::Matrix<doublereal> dd, doublereal *fa, doublereal *fc, doublereal *rlcur, 
	   doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, 
	   doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1);
/* gogoauto.cpp */
int set_function_pointers(const iap_type,function_list *);

namespace xpp {
struct Session; /* session.h */
}

/* autpp.cpp: a Mark value reads a free parameter from AUTO, a constant
   model parameter from the Session, or an AUTO unknown from its own slots. */
// AUTO's wave Jacobian classifies its free columns at the model boundary.
bool auto_model_parameter(const xpp::Session &s, integer slot);
double auto_parameter_value(const xpp::Session &s, integer slot,
                           std::span<const integer> free, std::span<const double> parameters);
/* the problem defined functions that run the model (autpp.cpp): its
   right-hand side and Jacobian, the starting point and the boundary
   conditions, in the Session s whose run this is (iap->lib->session).
   func gives dfdu for ijac 1 and the dfdp columns of the nfpr
   continuation parameters icp too for ijac 2 (JAC=1: the model's exact
   Jacobian, W260) */
int func(xpp::Session &s, integer ndim, const doublereal *u, const integer *icp, integer nfpr,
	 const doublereal *par, integer ijac,
	 doublereal *f, doublereal *dfdu, doublereal *dfdp, AutoLib *worker=nullptr);
int stpnt(xpp::Session &s, integer ndim, doublereal t,
	  doublereal *u, doublereal *par);
int bcnd(xpp::Session &s, integer ndim, const doublereal *par, const integer *icp, integer nbc,
	 const doublereal *u0, const doublereal *u1, integer ijac,
	 doublereal *f, doublereal *dbc);
/* gogoauto.cpp: AUTO's run in the Session s, from the settings do_auto
   (auto_nox.cpp) put in xAuto */
int go_go_auto(xpp::Session &s);
/* autlib1.cpp: fills thu (ndim*8 entries, each 1.) that thu_vec owns;
   thu_vec.data() is what the AE/BVP solves take as their doublereal *thu */
int init(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, std::vector<doublereal> &thu_vec, std::span<AutoMark> marks);
#endif
