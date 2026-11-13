#pragma once

/* autoX-specific interfaces of the numerical owner. Kept out of xpp_math.h
   so f2c's global types stay out of other numerical interfaces. */
#include "autox_f2c.h"
#include <cstdio>

struct iap_type; // The adapter borrows the run's AutoxRun and diagnostics.

namespace xpp {

inline constexpr std::ptrdiff_t autox_ge_pivot_iid = 4; // Full pivot listing accompanies reduced-Jacobian diagnostics.
inline constexpr std::ptrdiff_t autox_ge_note_iid = 1; // IID zero suppresses tiny-pivot notes, as AUTO-07p does.
inline constexpr double autox_rsmall = 1.0e-30;          // Zero pivot threshold for Gaussian elimination and null space
inline constexpr double autox_qzeps1 = 0.0;              // QZ eigenvalue zero tolerance
inline constexpr std::ptrdiff_t autox_householder_left = 1;         // Householder plane rotation applied from the left (householder_apply)

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
int autox_eigenvalues(iap_type *iap, std::ptrdiff_t *ndim, std::ptrdiff_t *m1a, double *a, xpp::ComplexPair *ev, std::ptrdiff_t *ier);
int matrix_null_vector(FILE *fp9, std::ptrdiff_t n, std::ptrdiff_t m, std::ptrdiff_t k, double *a, double *u);
double gauss_eliminate(FILE *fp9, std::ptrdiff_t iid, std::ptrdiff_t n, std::ptrdiff_t m1a, double *a, std::ptrdiff_t nrhs, std::ptrdiff_t ndxloc, double *u, std::ptrdiff_t m1f, double *f, double *det);
int floquet_multipliers(FILE *fp9, std::ptrdiff_t *ndim, double *c0, double *c1, std::ptrdiff_t *iid, double *rwork, xpp::ComplexPair *ev, bool deflate);
int householder_compute(std::ptrdiff_t *k, std::ptrdiff_t *j, std::ptrdiff_t *n, double *x, std::ptrdiff_t *incx, double *beta, double *v);
int householder_apply(std::ptrdiff_t *k, std::ptrdiff_t *j, std::ptrdiff_t *n, std::ptrdiff_t *q, double *beta, double *v, std::ptrdiff_t *job, double *a, std::ptrdiff_t *lda);
int homoclinic_eigen_ordered(const iap_type *iap, std::ptrdiff_t *isign, std::ptrdiff_t *itrans, double *rr, double *ri, double *vret, double *xequib, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t *ndm, double *dfdu, double *dfdp, double *zz);
enum class HomcontManifold { stable, unstable };
enum class HomcontEndpoint { left, right };
int homoclinic_projection_conditions(const iap_type *iap, double *bound, double *xequib, const std::ptrdiff_t *icp, double *par, HomcontManifold manifold, HomcontEndpoint endpoint, bool transpose, std::ptrdiff_t n, double *dfdu, double *dfdp, bool save_basis);
int real_general_eigen(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *wr, double *wi, std::ptrdiff_t matz, double *z__, std::ptrdiff_t *iv1, double *fv1, std::ptrdiff_t *ierr);
int hessenberg_qr_values(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *h__, double *wr, double *wi, std::ptrdiff_t *ierr);
int hessenberg_qr_vectors(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *h__, double *wr, double *wi, double *z__, std::ptrdiff_t *ierr);
int complex_divide_scaled(const double *ar, const double *ai, const double *br, const double *bi, double *cr, double *ci);
int balance_matrix(std::ptrdiff_t *nm, std::ptrdiff_t *n, double *a, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *scale);
int balance_back_transform(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *scale, std::ptrdiff_t *m, double *z__);
int hessenberg_reduce_elimination(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *a, std::ptrdiff_t *int__);
int hessenberg_transform_elimination(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *a, std::ptrdiff_t *int__, double *z__);
int qz_reduce_hessenberg(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, bool matz, double *z__);
int qz_iterate(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, double eps1, bool matz, double *z__, std::ptrdiff_t *ierr);
int qz_eigenvalues(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, double *alfr, double *alfi, double *beta, bool matz, double *z__);
double machine_epsilon(double x);
double euclidean_norm(const std::ptrdiff_t *n, const double *dx, const std::ptrdiff_t *incx);
double dot_product(const std::ptrdiff_t *n, const double *dx, const std::ptrdiff_t *incx, const double *dy, const std::ptrdiff_t *incy);
int rotate_plane(const std::ptrdiff_t *n, double *dx, const std::ptrdiff_t *incx, double *dy, const std::ptrdiff_t *incy, const double *c, const double *s);
int swap_vectors(const std::ptrdiff_t *n, double *dx, const std::ptrdiff_t *incx, double *dy, const std::ptrdiff_t *incy);
int copy_matrix_transposed(std::ptrdiff_t *m, std::ptrdiff_t *n, double *a, std::ptrdiff_t *lda, double *b, std::ptrdiff_t *ldb, bool *trans);
int blas_error(const char *srname, std::ptrdiff_t *info, std::ptrdiff_t srname_len);
bool same_letter(const char *ca, const char *cb, std::ptrdiff_t ca_len, std::ptrdiff_t cb_len);
int matrix_multiply(const char *transa, const char *transb, std::ptrdiff_t *m, std::ptrdiff_t *n, std::ptrdiff_t *k, double *alpha, double *a, std::ptrdiff_t *lda, double *b, std::ptrdiff_t *ldb, double *beta, double *c, std::ptrdiff_t *ldc, std::ptrdiff_t transa_len, std::ptrdiff_t transb_len);
int singular_value_decomposition(double *x, std::ptrdiff_t *ldx, std::ptrdiff_t *n, std::ptrdiff_t *p, double *s, double *e, double *u, std::ptrdiff_t *ldu, double *v, std::ptrdiff_t *ldv, double *work, std::ptrdiff_t *job, std::ptrdiff_t *info, double *tol);
int givens_rotation(double *f, double *g, double *cs, double *sn);
int singular_value_decomposition_detailed(double *x, std::ptrdiff_t *ldx, std::ptrdiff_t *n, std::ptrdiff_t *p, double *s, double *e, double *u, std::ptrdiff_t *ldu, double *v, std::ptrdiff_t *ldv, double *work, std::ptrdiff_t *job, std::ptrdiff_t *info, std::ptrdiff_t *maxitr, double *tol, std::ptrdiff_t *idbg, std::ptrdiff_t *ifull, std::ptrdiff_t *kount, std::ptrdiff_t *kount1, std::ptrdiff_t *kount2, std::ptrdiff_t *skip, std::ptrdiff_t *limshf, double *maxsin, std::ptrdiff_t *iidir);
int print_bidiagonal(const std::ptrdiff_t *ll, const std::ptrdiff_t *m, const std::ptrdiff_t *nrow, const std::ptrdiff_t *ncol, const double *s, const double *e);
int singular_values_2x2(double *a, double *b, double *c, double *smallest_singular_value_2x2, double *sigmax, double *snr, double *csr, double *snl, double *csl);
double smallest_singular_value_2x2(double *a, double *b, double *c);
int rotation_matching_signs(double *f, double *g, double *cs, double *sn);
int hessenberg_quasi_triangular(double *a, double *v, std::ptrdiff_t *n, std::ptrdiff_t *nlow, std::ptrdiff_t *nup, double *eps, double *er, double *ei, std::ptrdiff_t *type__, std::ptrdiff_t *na, std::ptrdiff_t *nv, std::ptrdiff_t *imfd);
int split(double *a, double *v, std::ptrdiff_t *n, std::ptrdiff_t *l, double *e1, double *e2, std::ptrdiff_t *na, std::ptrdiff_t *nv);
int exchange_diagonal_blocks(double *a, double *v, std::ptrdiff_t *n, std::ptrdiff_t *l, const std::ptrdiff_t *b1, const std::ptrdiff_t *b2, double *eps, bool *fail, std::ptrdiff_t *na, std::ptrdiff_t *nv);
int implicit_qr_step(double *a, double *v, double *p, double *q, double *r__, std::ptrdiff_t *nl, std::ptrdiff_t *nu, std::ptrdiff_t *n, std::ptrdiff_t *na, std::ptrdiff_t *nv);
int hessenberg_reduce_householder(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *a, double *ort);
int hessenberg_transform_orthogonal(std::ptrdiff_t *nm, std::ptrdiff_t *n, std::ptrdiff_t *low, std::ptrdiff_t *igh, double *a, double *ort, double *z__);

} // namespace xpp

