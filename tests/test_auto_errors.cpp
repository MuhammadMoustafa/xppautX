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
        return e.error.what.empty() ? "(empty)" : e.error.what;
    }
    return "";
}

bool has(const std::string &s, const char *part) { return s.find(part) != std::string::npos; }

} // namespace

int main()
{
    const xpp::Error file_error=xpp::files::open_error("AUTO", "missing/fort.3");
    try {
        xpp::auto_fail(file_error);
        CHECK(false);
    } catch (const xpp::AutoFailed &e) {
        CHECK(e.error.where == file_error.where && e.error.what == file_error.what);
        CHECK(e.error.place.file == file_error.place.file);
        CHECK(e.error.place.line == file_error.place.line && e.error.place.col == file_error.place.col);
        CHECK(e.error.place.source == file_error.place.source);
    }
    const xpp::Error located{"tables", "bad expression", {"model.odex", 12, 3, "table f %bad"}};
    try {
        xpp::auto_fail(located);
        CHECK(false);
    } catch (const xpp::AutoFailed &e) {
        CHECK(e.error.where == located.where && e.error.what == located.what);
        CHECK(e.error.place.file == located.place.file && e.error.place.line == 12);
        CHECK(e.error.place.col == 3 && e.error.place.source == located.place.source);
    }
    /* ge writes its pivots to fort.9 */
    FILE *fp9 = std::tmpfile();
    xpp::client_session().auto_lib.fp9 = fp9;

    CHECK(fp9 != nullptr);
    /* Explicit text mode exercises Windows' translated newline positions. */
    {
        const std::string path="build/test_auto_positions.txt";
        xpp::UniqueFile positions(xpp::files::open_stream(path, "w+"));
        CHECK(positions != nullptr);
        xpp::print(positions.get(), "first\nsecond\n");
        const auto end=xpp::tell_stream(positions.get(), path);
        CHECK(end && *end > 0);
        CHECK(xpp::rewind_stream(positions.get(), path).has_value());
        auto reader=xpp::LineReader::attach(positions.get());
        CHECK(reader.next() == "first");
        const auto second=xpp::tell_stream(positions.get(), path);
        CHECK(second && *second > 0);
        CHECK(reader.next() == "second");
        CHECK(!reader.next());
        CHECK(xpp::seek_stream(positions.get(), *second, path).has_value());
        CHECK(reader.next() == "second");
        const auto bad_seek=xpp::seek_stream(positions.get(), -1, path);
        CHECK(!bad_seek && bad_seek.error().place.file == path);
        CHECK(xpp::rewind_stream(positions.get(), path).has_value() && !std::feof(positions.get()));
    }
    CHECK(xpp::files::remove("build/test_auto_positions.txt") == 0);

    /* chdim: more free parameters than AUTO's arrays hold */
    iap_type iap{};
    iap.lib = &xpp::client_session().auto_lib;
    iap.lib->parameter_count = xpp::client_session().auto_state.parameter_layout.size();
    iap.nfpr = iap.lib->parameter_count + 1;
    CHECK(has(failure([&] { chdim(&iap); }), "free parameters"));
    iap.nfpr = 2;
    CHECK(failure([&] { chdim(&iap); }).empty());

    /* RG rejects a leading dimension smaller than the matrix dimension. */
    iap.ndim = 2;
    integer ndim = 2, leading = 1, ier = 0;
    std::array<double, 4> matrix{};
    std::array<doublecomplex, 2> spectrum{};
    CHECK(has(failure([&] { xpp::eig(&iap, &ndim, &leading, matrix.data(), spectrum.data(), &ier); }),
              "EISPACK routine RG"));
    CHECK(ier == 1);

    /* cpnts: collocation weights exist for Ncol up to 7 */
    std::array<doublereal, 8> zm{};
    CHECK(has(failure([&] { cpnts(8, zm.data()); }), "collocation_points is 8"));
    CHECK(failure([&] { cpnts(4, zm.data()); }).empty());

    /* ge: a singular system (the second row twice the first) */
    std::array<doublereal, 4> a{1, 2, 2, 4};
    std::array<doublereal, 2> u{}, f{1, 1};
    doublereal det = 0;
    CHECK(has(failure([&] { xpp::ge(nullptr, 0, 2, 2, a.data(), 1, 2, u.data(), 2, f.data(), &det); }), "division by zero"));
    std::array<doublereal, 4> b{2, 0, 0, 4};
    f = {2, 4};
    CHECK(failure([&] { xpp::ge(nullptr, 0, 2, 2, b.data(), 1, 2, u.data(), 2, f.data(), &det); }).empty());
    CHECK(u[0] == 1 && u[1] == 1);

    /* dhhpr, dhhap: Householder arguments out of range */
    std::array<doublereal, 4> x{1, 2, 3, 4}, v{};
    doublereal beta = 0;
    integer k = 0, j = 2, n = 2, q = 2, incx = 1, job = 1, lda = 2;
    CHECK(has(failure([&] { xpp::dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "K in DHHPR"));
    k = 1, j = 3;
    CHECK(has(failure([&] { xpp::dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "J in DHHPR"));
    j = 2, incx = 0;
    CHECK(has(failure([&] { xpp::dhhpr(&k, &j, &n, x.data(), &incx, &beta, v.data()); }), "INCX in DHHPR"));
    incx = 1, job = 3;
    CHECK(has(failure([&] { xpp::dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "JOB in DHHAP"));
    job = 1, k = 0;
    CHECK(has(failure([&] { xpp::dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "K in DHHAP"));
    k = 1, j = 3;
    CHECK(has(failure([&] { xpp::dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "J in DHHAP"));
    job = 2;
    CHECK(has(failure([&] { xpp::dhhap(&k, &j, &n, &q, &beta, v.data(), &job, x.data(), &lda); }), "J in DHHAP"));

    /* dgemm's argument check (xerbla): a negative row count */
    integer m = -1, nn = 2, kk = 2, ld = 2;
    doublereal one = 1, zero = 0;
    std::array<doublereal, 4> c{};
    const std::string blas =
        failure([&] { xpp::dgemm("N", "N", &m, &nn, &kk, &one, a.data(), &ld, b.data(), &ld, &zero, c.data(), &ld, 1, 1); });
    CHECK(has(blas, "DGEMM") && has(blas, "parameter number 3"));

    xpp::client_session().auto_lib.fp9 = nullptr;
    std::fclose(fp9);
    TEST_REPORT("auto_errors");
}
