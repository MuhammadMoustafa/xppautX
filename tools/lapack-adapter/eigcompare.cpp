// W358 (#420): runs the matrices tools/autox-profile/dump_matrices.py wrote through both eigenvalue
// libraries of the LINALG=lapack build, to compare them with a reference computed outside
// (tools/autox-profile/eig_accuracy.py):
//   eigcompare MATRICES > RESULTS
// MATRICES: lines "rg NM N MATZ A..." and "qz NM N A... B..." (column-major, C99 hex floats).
// RESULTS, per matrix, an "eispack" and a "lapack" line: the routine's error code, then for real_general_eigen the
// eigenvalues' real and imaginary parts, for qz the generalized eigenvalues' alfr, alfi and beta.
// Built by `make LINALG=lapack BUILDDIR=<its own> BUILDDIR/eigcompare`; it is a measuring tool and
// is not part of xppautX. EISPACK's entry points are the ones core/xpp_math_eispack.cpp keeps under the
// names the Makefile gave it for this build; the unprefixed names are the adapter's (LAPACK).
#include "autox_c.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace xpp {
int eispack_real_general_eigen(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *wr, double *wi, std::ptrdiff_t matz, double *z__,
               std::ptrdiff_t *iv1, double *fv1, std::ptrdiff_t *ierr);
int eispack_qz_reduce_hessenberg(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, bool matz, double *z__);
int eispack_qz_iterate(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, double eps1, bool matz, double *z__,
                 std::ptrdiff_t *ierr);
int eispack_qz_eigenvalues(std::ptrdiff_t nm, std::ptrdiff_t n, double *a, double *b, double *alfr, double *alfi,
                  double *beta, bool matz, double *z__);
}

namespace {

constexpr double QZ_EPS1 = 0;  // floquet_multipliers's QZEPS1 (autox_lib4.cpp): EISPACK's own default tolerance

void print(const char *library, std::ptrdiff_t ierr, const std::vector<std::vector<double>> &columns)
{
    std::printf("%s %ld", library, static_cast<long>(ierr));
    for (std::size_t i = 0; i < columns.front().size(); ++i)
        for (const auto &column : columns) std::printf(" %a", column[i]);
    std::printf("\n");
}

std::vector<double> numbers(std::istringstream &in, std::size_t count)
{
    std::vector<double> v(count);
    std::string token;
    for (auto &x : v) {
        in >> token;
        x = std::strtod(token.c_str(), nullptr);
    }
    return v;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: eigcompare MATRICES > RESULTS\n");
        return 2;
    }
    std::ifstream file(argv[1]);
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string kind;
        long nm, n, matz = 0;
        in >> kind >> nm >> n;
        const std::size_t size = static_cast<std::size_t>(n * n);
        std::printf("%s %ld %ld\n", kind.c_str(), nm, n);
        std::vector<std::ptrdiff_t> iv(static_cast<std::size_t>(n));
        std::vector<double> fv(static_cast<std::size_t>(n)), z(size);
        if (kind == "rg") {
            in >> matz;
            const std::vector<double> a = numbers(in, size);
            for (int library = 0; library < 2; ++library) {
                std::vector<double> m = a, wr(static_cast<std::size_t>(n)), wi(static_cast<std::size_t>(n));
                std::ptrdiff_t ierr = 0;
                (library ? xpp::real_general_eigen : xpp::eispack_real_general_eigen)(n, n, m.data(), wr.data(), wi.data(), 0, z.data(), iv.data(), fv.data(), &ierr);
                print(library ? "lapack" : "eispack", ierr, {wr, wi});
            }
        } else {
            const std::vector<double> a = numbers(in, size), b = numbers(in, size);
            for (int library = 0; library < 2; ++library) {
                std::vector<double> ma = a, mb = b, ar(static_cast<std::size_t>(n)), ai(static_cast<std::size_t>(n)),
                    be(static_cast<std::size_t>(n));
                std::ptrdiff_t ierr = 0;
                (library ? xpp::qz_reduce_hessenberg : xpp::eispack_qz_reduce_hessenberg)(n, n, ma.data(), mb.data(), FALSE_, z.data());
                (library ? xpp::qz_iterate : xpp::eispack_qz_iterate)(n, n, ma.data(), mb.data(), QZ_EPS1, FALSE_, z.data(), &ierr);
                (library ? xpp::qz_eigenvalues : xpp::eispack_qz_eigenvalues)(n, n, ma.data(), mb.data(), ar.data(), ai.data(), be.data(), FALSE_,
                                                   z.data());
                print(library ? "lapack" : "eispack", ierr, {ar, ai, be});
            }
        }
    }
    return 0;
}
