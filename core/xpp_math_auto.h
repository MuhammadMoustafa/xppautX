#ifndef XPP_MATH_AUTO_H
#define XPP_MATH_AUTO_H

/* AUTO-specific interfaces of the numerical owner. Kept out of xpp_math.h
   because f2c and CVODE define incompatible global real/integer types. */
#include "auto_f2c.h"
#include <cstdio>

struct iap_type; // The adapter borrows the run's AutoLib and diagnostics.

namespace xpp {

inline constexpr integer auto_ge_pivot_iid = 4; // Full pivot listing accompanies reduced-Jacobian diagnostics.
inline constexpr integer auto_ge_note_iid = 1; // IID zero suppresses tiny-pivot notes, as AUTO-07p does.
inline constexpr doublereal auto_rsmall = 1.0e-30;          // Zero pivot threshold for Gaussian elimination and null space
inline constexpr doublereal auto_qzeps1 = 0.0;              // QZ eigenvalue zero tolerance
inline constexpr integer auto_householder_left = 1;         // Householder plane rotation applied from the left (dhhap)

/* ---- AUTO algorithms (W337, #398), original names retained ----
   ge/nlvc: complete pivoting with strict > ties and AUTO pivot diagnostics;
   ge also returns the determinant. See definitions for singular semantics.
   eig: balanced elementary Hessenberg/QR roots, machine-precision convergence.
   flowkm: SVD-deflated generalized QZ Floquet multipliers, optional deflation.
   eigho: sorted complex/conjugate eigenvectors with sign continuity.
   prjctn: ordered Schur invariant subspaces with Beyn projection continuity.
   rg: matz=0 roots only, otherwise conjugate eigenvector columns; machine
   precision QR, 30*n total iterations, ierr identifies an unconverged root.
   hqr/hqr2: that same QR with/without eigenvector accumulation.
   qzhes/qzit/qzval: generalized eigenvalues, optional vector accumulation;
   qzit uses eps1 (machine precision when zero), with 30*n iteration budget.
   ezsvd/ndsvd: Demmel-Kahan SVD, job selects vector accumulation, tol controls
   convergence and info reports the unconverged bidiagonal block.
   orthes/ortran: Householder reduction and basis, caller's leading dimension;
   hqr3lc: ordered real Schur basis with caller's compzero convergence tolerance.
   BLAS/rotation helpers retain stride, scaling and summation order. */
int eig(iap_type *iap, integer *ndim, integer *m1a, doublereal *a, doublecomplex *ev, integer *ier);
int nlvc(FILE *fp9, integer n, integer m, integer k, doublereal *a, doublereal *u);
int ge(FILE *fp9, integer iid, integer n, integer m1a, doublereal *a, integer nrhs, integer ndxloc, doublereal *u, integer m1f, doublereal *f, doublereal *det);
int flowkm(FILE *fp9, integer *ndim, doublereal *c0, doublereal *c1, integer *iid, doublereal *rwork, doublecomplex *ev, bool deflate);
int dhhpr(integer *k, integer *j, integer *n, doublereal *x, integer *incx, doublereal *beta, doublereal *v);
int dhhap(integer *k, integer *j, integer *n, integer *q, doublereal *beta, doublereal *v, integer *job, doublereal *a, integer *lda);
int eigho(const iap_type *iap, integer *isign, integer *itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm, doublereal *dfdu, doublereal *dfdp, doublereal *zz);
int prjctn(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, integer *imfd, integer *is, integer *itrans, integer *ndm, doublereal *dfdu, doublereal *dfdp);
int rg(integer nm, integer n, doublereal *a, doublereal *wr, doublereal *wi, integer matz, doublereal *z__, integer *iv1, doublereal *fv1, integer *ierr);
int hqr(integer *nm, integer *n, integer *low, integer *igh, doublereal *h__, doublereal *wr, doublereal *wi, integer *ierr);
int hqr2(integer *nm, integer *n, integer *low, integer *igh, doublereal *h__, doublereal *wr, doublereal *wi, doublereal *z__, integer *ierr);
int cdiv(const doublereal *ar, const doublereal *ai, const doublereal *br, const doublereal *bi, doublereal *cr, doublereal *ci);
int balanc(integer *nm, integer *n, doublereal *a, integer *low, integer *igh, doublereal *scale);
int balbak(integer *nm, integer *n, integer *low, integer *igh, doublereal *scale, integer *m, doublereal *z__);
int elmhes(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, integer *int__);
int eltran(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, integer *int__, doublereal *z__);
int qzhes(integer nm, integer n, doublereal *a, doublereal *b, logical matz, doublereal *z__);
int qzit(integer nm, integer n, doublereal *a, doublereal *b, doublereal eps1, logical matz, doublereal *z__, integer *ierr);
int qzval(integer nm, integer n, doublereal *a, doublereal *b, doublereal *alfr, doublereal *alfi, doublereal *beta, logical matz, doublereal *z__);
doublereal epslon(doublereal x);
doublereal dnrm2(const integer *n, const doublereal *dx, const integer *incx);
doublereal ddot(const integer *n, const doublereal *dx, const integer *incx, const doublereal *dy, const integer *incy);
int dscal(const integer *n, const doublereal *da, doublereal *dx, const integer *incx);
integer idamax(const integer *n, const doublereal *dx, const integer *incx);
int daxpy(const integer *n, const doublereal *da, const doublereal *dx, const integer *incx, doublereal *dy, const integer *incy);
int drot(const integer *n, doublereal *dx, const integer *incx, doublereal *dy, const integer *incy, const doublereal *c, const doublereal *s);
int dswap(const integer *n, doublereal *dx, const integer *incx, doublereal *dy, const integer *incy);
int dgemc(integer *m, integer *n, doublereal *a, integer *lda, doublereal *b, integer *ldb, logical *trans);
int xerbla(const char *srname, integer *info, integer srname_len);
logical lsame(const char *ca, const char *cb, integer ca_len, integer cb_len);
int dgemm(const char *transa, const char *transb, integer *m, integer *n, integer *k, doublereal *alpha, doublereal *a, integer *lda, doublereal *b, integer *ldb, doublereal *beta, doublereal *c, integer *ldc, integer transa_len, integer transb_len);
int ezsvd(doublereal *x, integer *ldx, integer *n, integer *p, doublereal *s, doublereal *e, doublereal *u, integer *ldu, doublereal *v, integer *ldv, doublereal *work, integer *job, integer *info, doublereal *tol);
int ndrotg(doublereal *f, doublereal *g, doublereal *cs, doublereal *sn);
int ndsvd(doublereal *x, integer *ldx, integer *n, integer *p, doublereal *s, doublereal *e, doublereal *u, integer *ldu, doublereal *v, integer *ldv, doublereal *work, integer *job, integer *info, integer *maxitr, doublereal *tol, integer *idbg, integer *ifull, integer *kount, integer *kount1, integer *kount2, integer *skip, integer *limshf, doublereal *maxsin, integer *iidir);
int prse(const integer *ll, const integer *m, const integer *nrow, const integer *ncol, const doublereal *s, const doublereal *e);
int sig22(doublereal *a, doublereal *b, doublereal *c, doublereal *sigmin, doublereal *sigmax, doublereal *snr, doublereal *csr, doublereal *snl, doublereal *csl);
doublereal sigmin(doublereal *a, doublereal *b, doublereal *c);
int sndrtg(doublereal *f, doublereal *g, doublereal *cs, doublereal *sn);
int hqr3lc(doublereal *a, doublereal *v, integer *n, integer *nlow, integer *nup, doublereal *eps, doublereal *er, doublereal *ei, integer *type__, integer *na, integer *nv, integer *imfd);
int split(doublereal *a, doublereal *v, integer *n, integer *l, doublereal *e1, doublereal *e2, integer *na, integer *nv);
int exchng(doublereal *a, doublereal *v, integer *n, integer *l, const integer *b1, const integer *b2, doublereal *eps, logical *fail, integer *na, integer *nv);
int qrstep(doublereal *a, doublereal *v, doublereal *p, doublereal *q, doublereal *r__, integer *nl, integer *nu, integer *n, integer *na, integer *nv);
int orthes(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, doublereal *ort);
int ortran(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, doublereal *ort, doublereal *z__);

} // namespace xpp

#endif
