// W358 (#420): the comparison build LINALG=lapack. Gives AUTO's eigenvalue code (autlib1's eig,
// autlib5's eigenvectors for HomCont, autlib4's flowkm) LAPACK in place of EISPACK, through
// the four EISPACK entry points it calls, with their signatures (core/xpp_math.h):
//   rg                    -> DGEEV   (eigenvalues, and eigenvectors when matz)
//   qzhes, qzit           -> nothing (DGGEV reduces the pencil itself; qzit reports no error)
//   qzval                 -> DGGEV   (generalized eigenvalues: alfr, alfi, beta)
// The Makefile compiles core/xpp_math_eispack.cpp with these four names renamed (eispack_rg, ...), so the
// rest of that file (the SVD and BLAS copies other code calls) stays, and links third_party/lapack
// (Fortran, verbatim). Compiled only by `make LINALG=lapack BUILDDIR=<its own>`; the default
// build and every release use EISPACK. Neither throws: a failure is a return code, as EISPACK's.
#include "auto_c.h"
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

int rg(integer nm, integer n, doublereal *a, doublereal *wr, doublereal *wi, integer matz, doublereal *z__,
       integer * /*iv1*/, doublereal * /*fv1*/, integer *ierr)
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

int qzhes(integer, integer, doublereal *, doublereal *, bool, doublereal *)
{
    return 0;
}

int qzit(integer, integer, doublereal *, doublereal *, doublereal, bool, doublereal *, integer *ierr)
{
    *ierr = 0;
    return 0;
}

int qzval(integer nm, integer n, doublereal *a, doublereal *b, doublereal *alfr, doublereal *alfi, doublereal *beta,
          bool, doublereal *)
{
    const int order = static_cast<int>(n), ld = static_cast<int>(nm), one = 1;
    double unused = 0;
    const int info = with_workspace([&](double *work, const int *lwork, int *code) {
        dggev_("N", "N", &order, a, &ld, b, &ld, alfr, alfi, beta, &unused, &one, &unused, &one, work, lwork, code, 1, 1);
    });
    if (info != 0) {  // EISPACK's QZIT returned its ierr to flowkm, which warns; qzval has no way to say it
        xpp::log(XPP_LOG_WARN, "DGGEV returned INFO = {}: the eigenvalues after the first {} are right", info, info);
    }
    return 0;
}

} // namespace xpp
