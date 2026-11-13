#pragma once

/* autoX-specific interfaces of the numerical owner. Kept out of xpp_math.h
   so f2c's global types stay out of other numerical interfaces. */
#include "autox_f2c.h"
#include <cstdio>

struct iap_type; // The adapter borrows the run's AutoxRun and diagnostics.

namespace xpp {

inline constexpr integer autox_ge_pivot_iid = 4; // Full pivot listing accompanies reduced-Jacobian diagnostics.
inline constexpr integer autox_ge_note_iid = 1; // IID zero suppresses tiny-pivot notes, as AUTO-07p does.
inline constexpr doublereal autox_rsmall = 1.0e-30;          // Zero pivot threshold for Gaussian elimination and null space
inline constexpr doublereal autox_qzeps1 = 0.0;              // QZ eigenvalue zero tolerance
inline constexpr integer autox_householder_left = 1;         // Householder plane rotation applied from the left (householder_apply)

/* ---- autoX algorithms (W337, #398), original names retained ----
   gauss_eliminate/matrix_null_vector: complete pivoting with strict > ties and autoX pivot diagnostics;
   gauss_eliminate also returns the determinant. See definitions for singular semantics.
   autox_eigenvalues: balanced elementary Hessenberg/QR roots, machine-precision convergence.
   floquet_multipliers: SVD-deflated generalized QZ Floquet multipliers, optional deflation.
   homoclinic_eigen_ordered: LAPACK DGEEV's unit eigenvectors (a complex one's largest component
   real), their real parts, signs continuous with the previous call.
   homoclinic_projection_conditions: NSTAB/NUNSTAB-dimensional machine-precision ordered Schur subspaces,
   reversed with negative period; save_basis=false keeps the Beyn history.
   real_general_eigen: matz=0 roots only, otherwise conjugate eigenvector columns; machine
   precision QR, 30*n total iterations, ierr identifies an unconverged root.
   hessenberg_qr_values/hessenberg_qr_vectors: that same QR with/without eigenvector accumulation.
   qz_reduce_hessenberg/qz_iterate/qz_eigenvalues: generalized eigenvalues, optional vector accumulation;
   qz_iterate uses eps1 (machine precision when zero), with 30*n iteration budget.
   singular_value_decomposition/singular_value_decomposition_detailed: Demmel-Kahan SVD, job selects vector accumulation, tol controls
   convergence and info reports the unconverged bidiagonal block.
   hessenberg_reduce_householder/hessenberg_transform_orthogonal: Householder reduction and basis, caller's leading dimension;
   hessenberg_quasi_triangular: ordered real Schur basis with the caller's convergence tolerance.
   BLAS/rotation helpers retain stride, scaling and summation order. */
int autox_eigenvalues(iap_type *iap, integer *ndim, integer *m1a, doublereal *a, doublecomplex *ev, integer *ier);
int matrix_null_vector(FILE *fp9, integer n, integer m, integer k, doublereal *a, doublereal *u);
double gauss_eliminate(FILE *fp9, integer iid, integer n, integer m1a, doublereal *a, integer nrhs, integer ndxloc, doublereal *u, integer m1f, doublereal *f, doublereal *det);
int floquet_multipliers(FILE *fp9, integer *ndim, doublereal *c0, doublereal *c1, integer *iid, doublereal *rwork, doublecomplex *ev, bool deflate);
int householder_compute(integer *k, integer *j, integer *n, doublereal *x, integer *incx, doublereal *beta, doublereal *v);
int householder_apply(integer *k, integer *j, integer *n, integer *q, doublereal *beta, doublereal *v, integer *job, doublereal *a, integer *lda);
int homoclinic_eigen_ordered(const iap_type *iap, integer *isign, integer *itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm, doublereal *dfdu, doublereal *dfdp, doublereal *zz);
enum class HomcontManifold { stable, unstable };
enum class HomcontEndpoint { left, right };
int homoclinic_projection_conditions(const iap_type *iap, double *bound, double *xequib, const integer *icp, double *par, HomcontManifold manifold, HomcontEndpoint endpoint, bool transpose, integer n, double *dfdu, double *dfdp, bool save_basis);
int real_general_eigen(integer nm, integer n, doublereal *a, doublereal *wr, doublereal *wi, integer matz, doublereal *z__, integer *iv1, doublereal *fv1, integer *ierr);
int hessenberg_qr_values(integer *nm, integer *n, integer *low, integer *igh, doublereal *h__, doublereal *wr, doublereal *wi, integer *ierr);
int hessenberg_qr_vectors(integer *nm, integer *n, integer *low, integer *igh, doublereal *h__, doublereal *wr, doublereal *wi, doublereal *z__, integer *ierr);
int complex_divide_scaled(const doublereal *ar, const doublereal *ai, const doublereal *br, const doublereal *bi, doublereal *cr, doublereal *ci);
int balance_matrix(integer *nm, integer *n, doublereal *a, integer *low, integer *igh, doublereal *scale);
int balance_back_transform(integer *nm, integer *n, integer *low, integer *igh, doublereal *scale, integer *m, doublereal *z__);
int hessenberg_reduce_elimination(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, integer *int__);
int hessenberg_transform_elimination(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, integer *int__, doublereal *z__);
int qz_reduce_hessenberg(integer nm, integer n, doublereal *a, doublereal *b, bool matz, doublereal *z__);
int qz_iterate(integer nm, integer n, doublereal *a, doublereal *b, doublereal eps1, bool matz, doublereal *z__, integer *ierr);
int qz_eigenvalues(integer nm, integer n, doublereal *a, doublereal *b, doublereal *alfr, doublereal *alfi, doublereal *beta, bool matz, doublereal *z__);
doublereal machine_epsilon(doublereal x);
doublereal euclidean_norm(const integer *n, const doublereal *dx, const integer *incx);
doublereal dot_product(const integer *n, const doublereal *dx, const integer *incx, const doublereal *dy, const integer *incy);
int rotate_plane(const integer *n, doublereal *dx, const integer *incx, doublereal *dy, const integer *incy, const doublereal *c, const doublereal *s);
int swap_vectors(const integer *n, doublereal *dx, const integer *incx, doublereal *dy, const integer *incy);
int copy_matrix_transposed(integer *m, integer *n, doublereal *a, integer *lda, doublereal *b, integer *ldb, bool *trans);
int blas_error(const char *srname, integer *info, integer srname_len);
bool same_letter(const char *ca, const char *cb, integer ca_len, integer cb_len);
int matrix_multiply(const char *transa, const char *transb, integer *m, integer *n, integer *k, doublereal *alpha, doublereal *a, integer *lda, doublereal *b, integer *ldb, doublereal *beta, doublereal *c, integer *ldc, integer transa_len, integer transb_len);
int singular_value_decomposition(doublereal *x, integer *ldx, integer *n, integer *p, doublereal *s, doublereal *e, doublereal *u, integer *ldu, doublereal *v, integer *ldv, doublereal *work, integer *job, integer *info, doublereal *tol);
int givens_rotation(doublereal *f, doublereal *g, doublereal *cs, doublereal *sn);
int singular_value_decomposition_detailed(doublereal *x, integer *ldx, integer *n, integer *p, doublereal *s, doublereal *e, doublereal *u, integer *ldu, doublereal *v, integer *ldv, doublereal *work, integer *job, integer *info, integer *maxitr, doublereal *tol, integer *idbg, integer *ifull, integer *kount, integer *kount1, integer *kount2, integer *skip, integer *limshf, doublereal *maxsin, integer *iidir);
int print_bidiagonal(const integer *ll, const integer *m, const integer *nrow, const integer *ncol, const doublereal *s, const doublereal *e);
int singular_values_2x2(doublereal *a, doublereal *b, doublereal *c, doublereal *smallest_singular_value_2x2, doublereal *sigmax, doublereal *snr, doublereal *csr, doublereal *snl, doublereal *csl);
doublereal smallest_singular_value_2x2(doublereal *a, doublereal *b, doublereal *c);
int rotation_matching_signs(doublereal *f, doublereal *g, doublereal *cs, doublereal *sn);
int hessenberg_quasi_triangular(doublereal *a, doublereal *v, integer *n, integer *nlow, integer *nup, doublereal *eps, doublereal *er, doublereal *ei, integer *type__, integer *na, integer *nv, integer *imfd);
int split(doublereal *a, doublereal *v, integer *n, integer *l, doublereal *e1, doublereal *e2, integer *na, integer *nv);
int exchange_diagonal_blocks(doublereal *a, doublereal *v, integer *n, integer *l, const integer *b1, const integer *b2, doublereal *eps, bool *fail, integer *na, integer *nv);
int implicit_qr_step(doublereal *a, doublereal *v, doublereal *p, doublereal *q, doublereal *r__, integer *nl, integer *nu, integer *n, integer *na, integer *nv);
int hessenberg_reduce_householder(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, doublereal *ort);
int hessenberg_transform_orthogonal(integer *nm, integer *n, integer *low, integer *igh, doublereal *a, doublereal *ort, doublereal *z__);

} // namespace xpp

