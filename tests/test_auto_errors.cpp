/* W63a: what AUTO's numerics used to exit() on throws xpp::AutoFailed,
   which do_auto (auto_nox.cpp) catches and reports. tools/autocheck.py's
   `errors` section drives the ones a model's settings reach (Ncol above
   7, Ntst 0) through the protocol and runs AUTO again afterwards; this
   covers every throw site directly, including the ones no model reaches:
   the dimension check (xppautX never frees more than a few parameters),
   a singular solve, and the BLAS argument checks (AUTO always calls them
   with arguments in range). */
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"

#include <array>
#include <cstdio>
#include <string>

namespace {

/* the message f throws, or "" when it returns */
template <class F> std::string failure(F f)
{
    try {
        f();
    } catch (const xpp::AutoFailed &e) {
        return e.what.empty() ? "(empty)" : e.what;
    }
    return "";
}

bool has(const std::string &s, const char *part) { return s.find(part) != std::string::npos; }

} // namespace

int main()
{
    /* ge writes its pivots to fort.9 */
    FILE *fp9 = std::tmpfile();
    xpp::session().auto_lib.fp9 = fp9;

    /* chdim: more free parameters than AUTO's arrays hold */
    iap_type iap{};
    iap.nfpr = NPARX + 1;
    CHECK(has(failure([&] { chdim(&iap); }), "free parameters"));
    iap.nfpr = 2;
    CHECK(failure([&] { chdim(&iap); }).empty());

    /* cpnts: collocation weights exist for Ncol up to 7 */
    std::array<doublereal, 8> zm{};
    CHECK(has(failure([&] { cpnts(8, zm.data()); }), "Ncol is 8"));
    CHECK(failure([&] { cpnts(4, zm.data()); }).empty());

    /* ge: a singular system (the second row twice the first) */
    std::array<doublereal, 4> a{1, 2, 2, 4};
    std::array<doublereal, 2> u{}, f{1, 1};
    doublereal det = 0;
    CHECK(has(failure([&] { ge(nullptr, 2, 2, a.data(), 1, 2, u.data(), 2, f.data(), &det); }), "division by zero"));
    std::array<doublereal, 4> b{2, 0, 0, 4};
    f = {2, 4};
    CHECK(failure([&] { ge(nullptr, 2, 2, b.data(), 1, 2, u.data(), 2, f.data(), &det); }).empty());
    CHECK(u[0] == 1 && u[1] == 1);

    /* dhhpr, dhhap: Householder arguments out of range */
    std::array<doublereal, 4> x{1, 2, 3, 4}, v{};
    doublereal beta = 0;
    integer k = 0, j = 2, n = 2, q = 2, incx = 1, job = 1, lda = 2;
    CHECK(has(failure([&] { dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "K in DHHPR"));
    k = 1, j = 3;
    CHECK(has(failure([&] { dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "J in DHHPR"));
    j = 2, incx = 0;
    CHECK(has(failure([&] { dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "INCX in DHHPR"));
    incx = 1, job = 3;
    CHECK(has(failure([&] { dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "JOB in DHHAP"));
    job = 1, k = 0;
    CHECK(has(failure([&] { dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "K in DHHAP"));
    k = 1, j = 3;
    CHECK(has(failure([&] { dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "J in DHHAP"));
    job = 2;
    CHECK(has(failure([&] { dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "J in DHHAP"));

    /* dgemm's argument check (xerbla): a negative row count */
    integer m = -1, nn = 2, kk = 2, ld = 2;
    doublereal one = 1, zero = 0;
    std::array<doublereal, 4> c{};
    const std::string blas =
        failure([&] { dgemm("N", "N", &m, &nn, &kk, &one, a.data(), &ld, b.data(), &ld, &zero, c.data(), &ld, 1, 1); });
    CHECK(has(blas, "DGEMM") && has(blas, "parameter number 3"));

    xpp::session().auto_lib.fp9 = nullptr;
    std::fclose(fp9);
    TEST_REPORT("auto_errors");
}
