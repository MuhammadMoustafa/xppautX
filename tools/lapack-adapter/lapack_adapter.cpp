// W358 (#420): the comparison build LINALG=lapack. Gives autoX's eigenvalue code (autox_lib1's eig,
// autox_lib5's eigenvectors for HomCont, autox_lib4's floquet_multipliers) LAPACK in place of EISPACK, through
// the four EISPACK entry points it calls, with their signatures (core/xpp_math.h):
//   real_general_eigen                    -> DGEEV   (eigenvalues, and eigenvectors when matz)
//   qz_reduce_hessenberg, qz_iterate           -> nothing (DGGEV reduces the pencil itself; qz_iterate reports no error)
//   qz_eigenvalues                 -> DGGEV   (generalized eigenvalues: alfr, alfi, beta)
// The Makefile compiles core/xpp_math_eispack.cpp with these four names renamed (eispack_real_general_eigen, ...), so the
// rest of that file (the SVD and BLAS copies other code calls) stays, and links third_party/lapack
// (Fortran, verbatim). Compiled only by `make LINALG=lapack BUILDDIR=<its own>`; the default
// build and every release use EISPACK. Neither throws: a failure is a return code, as EISPACK's.
#include "autox_c.h"
#include "xpp_log.h"

#include <cstddef>
#include <vector>

namespace {

// gfortran's calling convention: every argument by reference, a hidden length after the list
// for each character argument
extern "C" {
void dgeev_(const char *jobvl, const char *jobvr, const int *n, double *a, const int *lda, double *wr, double *wi,
            double *vl, const int *ldvl, double *vr, const int *ldvr, double *work, const int *lwork, int *info,
            std::size_t jobvl_len, std::size_t jobvr_len);
void dggev_(const char *jobvl, const char *jobvr, const int *n, double *a, const int *lda, double *b, const int *ldb,
            double *alphar, double *alphai, double *beta, double *vl, const int *ldvl, double *vr, const int *ldvr,
            double *work, const int *lwork, int *info, std::size_t jobvl_len, std::size_t jobvr_len);
}

// LAPACK's optimal workspace for a routine called with lwork = -1, then the call itself
template <class Call>
int with_workspace(Call call)
{
    constexpr int query = -1;
    double best = 0;
    int info = 0;
    call(&best, &query, &info);
    std::vector<double> work(static_cast<std::size_t>(best) + 1);
    const int lwork = static_cast<int>(work.size());
    call(work.data(), &lwork, &info);
    return info;
}

} // namespace

namespace xpp {

int real_general_eigen(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *wr, double *wi, std::ptrdiff_t matz, double *z__,
       std::ptrdiff_t * /*iv1*/, double * /*fv1*/, std::ptrdiff_t *ierr)
{
    const int order = static_cast<int>(n), lda = static_cast<int>(nm);
    const int one = 1;
    double unused = 0;
    const char *vectors = matz ? "V" : "N";
    const int ldvr = matz ? lda : one;
    double *vr = matz ? z__ : &unused;
    *ierr = with_workspace([&](double *work, const int *lwork, int *info) {
        dgeev_("N", vectors, &order, a, &lda, wr, wi, &unused, &one, vr, &ldvr, work, lwork, info, 1, 1);
    });
    return 0;
}

int qz_reduce_hessenberg(std::ptrdiff_t, std::ptrdiff_t, double *, double *, bool, double *)
{
    return 0;
}

int qz_iterate(std::ptrdiff_t, std::ptrdiff_t, double *, double *, double, bool, double *, std::ptrdiff_t *ierr)
{
    *ierr = 0;
    return 0;
}

int qz_eigenvalues(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, double *alfr, double *alfi, double *beta,
          bool, double *)
{
    const int order = static_cast<int>(n), ld = static_cast<int>(nm), one = 1;
    double unused = 0;
    const int info = with_workspace([&](double *work, const int *lwork, int *code) {
        dggev_("N", "N", &order, a, &ld, b, &ld, alfr, alfi, beta, &unused, &one, &unused, &one, work, lwork, code, 1, 1);
    });
    if (info != 0) {  // EISPACK's QZIT returned its ierr to floquet_multipliers, which warns; qz_eigenvalues has no way to say it
        xpp::log(XPP_LOG_WARN, "DGGEV returned INFO = {}: the eigenvalues after the first {} are right", info, info);
    }
    return 0;
}

} // namespace xpp
