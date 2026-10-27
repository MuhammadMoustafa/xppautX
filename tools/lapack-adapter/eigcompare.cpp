// W358 (#420): runs the matrices tools/auto-profile/dump_matrices.py wrote through both eigenvalue
// libraries of the LINALG=lapack build, to compare them with a reference computed outside
// (tools/auto-profile/eig_accuracy.py):
//   eigcompare MATRICES > RESULTS
// MATRICES: lines "rg NM N MATZ A..." and "qz NM N A... B..." (column-major, C99 hex floats).
// RESULTS, per matrix, an "eispack" and a "lapack" line: the routine's error code, then for rg the
// eigenvalues' real and imaginary parts, for qz the generalized eigenvalues' alfr, alfi and beta.
// Built by `make LINALG=lapack BUILDDIR=<its own> BUILDDIR/eigcompare`; it is a measuring tool and
// is not part of xppautX. EISPACK's entry points are the ones core/xpp_math_eispack.cpp keeps under the
// names the Makefile gave it for this build; the unprefixed names are the adapter's (LAPACK).
#include "auto_c.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace xpp {
int eispack_rg(integer nm, integer n, doublereal *a, doublereal *wr, doublereal *wi, integer matz, doublereal *z__,
               integer *iv1, doublereal *fv1, integer *ierr);
int eispack_qzhes(integer nm, integer n, doublereal *a, doublereal *b, bool matz, doublereal *z__);
int eispack_qzit(integer nm, integer n, doublereal *a, doublereal *b, doublereal eps1, bool matz, doublereal *z__,
                 integer *ierr);
int eispack_qzval(integer nm, integer n, doublereal *a, doublereal *b, doublereal *alfr, doublereal *alfi,
                  doublereal *beta, bool matz, doublereal *z__);
}

namespace {

constexpr doublereal QZ_EPS1 = 0;  // flowkm's QZEPS1 (autlib4.cpp): EISPACK's own default tolerance

void print(const char *library, integer ierr, const std::vector<std::vector<doublereal>> &columns)
{
    std::printf("%s %ld", library, static_cast<long>(ierr));
    for (std::size_t i = 0; i < columns.front().size(); ++i)
        for (const auto &column : columns) std::printf(" %a", column[i]);
    std::printf("\n");
}

std::vector<doublereal> numbers(std::istringstream &in, std::size_t count)
{
    std::vector<doublereal> v(count);
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
        std::vector<integer> iv(static_cast<std::size_t>(n));
        std::vector<doublereal> fv(static_cast<std::size_t>(n)), z(size);
        if (kind == "rg") {
            in >> matz;
            const std::vector<doublereal> a = numbers(in, size);
            for (int library = 0; library < 2; ++library) {
                std::vector<doublereal> m = a, wr(static_cast<std::size_t>(n)), wi(static_cast<std::size_t>(n));
                integer ierr = 0;
                (library ? xpp::rg : xpp::eispack_rg)(n, n, m.data(), wr.data(), wi.data(), 0, z.data(), iv.data(), fv.data(), &ierr);
                print(library ? "lapack" : "eispack", ierr, {wr, wi});
            }
        } else {
            const std::vector<doublereal> a = numbers(in, size), b = numbers(in, size);
            for (int library = 0; library < 2; ++library) {
                std::vector<doublereal> ma = a, mb = b, ar(static_cast<std::size_t>(n)), ai(static_cast<std::size_t>(n)),
                    be(static_cast<std::size_t>(n));
                integer ierr = 0;
                (library ? xpp::qzhes : xpp::eispack_qzhes)(n, n, ma.data(), mb.data(), FALSE_, z.data());
                (library ? xpp::qzit : xpp::eispack_qzit)(n, n, ma.data(), mb.data(), QZ_EPS1, FALSE_, z.data(), &ierr);
                (library ? xpp::qzval : xpp::eispack_qzval)(n, n, ma.data(), mb.data(), ar.data(), ai.data(), be.data(), FALSE_,
                                                   z.data());
                print(library ? "lapack" : "eispack", ierr, {ar, ai, be});
            }
        }
    }
    return 0;
}
