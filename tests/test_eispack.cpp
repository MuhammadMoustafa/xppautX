/* W336: EISPACK (xpp_math_eispack.cpp, AUTO's eigenvalue, SVD and QZ routines) keeps no
   state between or across calls, so the same input gives the same bits at any
   time and when several threads call it at once (a collocation worker reaching
   an eigenvalue unit must not race the caller's own call). Each routine is run
   serially for a reference, then from several threads, with other inputs
   between the calls, and every result is compared bit for bit. */
#include "xpptest.h"
#include "auto_c.h"
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

constexpr integer order = 5;
constexpr unsigned threads = 4;
constexpr int rounds = 200;

/* a fixed pseudo-random matrix in [-1, 1), column by column (a seed
   picks the input: a different one between calls disturbs any state kept) */
std::vector<doublereal> matrix(std::uint64_t seed, bool upper = false)
{
    std::vector<doublereal> a(static_cast<size_t>(order * order));
    for (auto &x : a) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        x = static_cast<doublereal>(static_cast<std::int64_t>(seed >> 11)) / 4503599627370496.0 - 1.0;
    }
    if (upper)
        for (integer j = 0; j < order; ++j) {
            for (integer i = j + 1; i < order; ++i) a[static_cast<size_t>(i + j * order)] = 0.;
            a[static_cast<size_t>(j + j * order)] += 3.;
        }
    return a;
}

void keep(std::vector<doublereal> &out, const std::vector<doublereal> &v) { out.insert(out.end(), v.begin(), v.end()); }

std::vector<doublereal> run_rg(std::uint64_t seed, integer matz)
{
    auto a = matrix(seed);
    std::vector<doublereal> wr(order), wi(order), z(order * order), fv(order);
    std::vector<integer> iv(order);
    integer ierr = -1;
    xpp::rg(order, order, a.data(), wr.data(), wi.data(), matz, z.data(), iv.data(), fv.data(), &ierr);
    std::vector<doublereal> out{static_cast<doublereal>(ierr)};
    keep(out, wr); keep(out, wi); keep(out, z);
    return out;
}

/* the homoclinic code's way (autlib5.cpp): Hessenberg basis, then the real Schur form
   with the stable (imfd -1) or unstable (1) eigenvalues first */
std::vector<doublereal> run_schur(std::uint64_t seed, integer imfd)
{
    auto a = matrix(seed);
    std::vector<doublereal> ort(order), v(order * order), er(order), ei(order);
    std::vector<integer> type(order);
    integer n = order, one = 1, nm = order, na = order, nv = order, mfd = imfd;
    doublereal eps = 1e-8;
    xpp::orthes(&nm, &n, &one, &n, a.data(), ort.data());
    xpp::ortran(&nm, &n, &one, &n, a.data(), ort.data(), v.data());
    for (integer i = 2; i < order; ++i)
        for (integer j = 0; j < i - 1; ++j) a[static_cast<size_t>(i + j * order)] = 0.;
    const int status = xpp::hqr3lc(a.data(), v.data(), &n, &one, &n, &eps, er.data(), ei.data(), type.data(), &na, &nv, &mfd);
    std::vector<doublereal> out{static_cast<doublereal>(status)};
    keep(out, a); keep(out, v); keep(out, er); keep(out, ei);
    for (integer t : type) out.push_back(static_cast<doublereal>(t));
    return out;
}

std::vector<doublereal> run_svd(std::uint64_t seed)
{
    auto x = matrix(seed);
    std::vector<doublereal> s(order + 1), e(order + 1), u(order * order), v(order * order), work(order + 1);
    integer ldx = order, n = order, p = order, ldu = 1, ldv = order, job = 1, info = -1;
    doublereal tol = 1e-16;
    xpp::ezsvd(x.data(), &ldx, &n, &p, s.data(), e.data(), u.data(), &ldu, v.data(), &ldv, work.data(), &job, &info, &tol);
    std::vector<doublereal> out{static_cast<doublereal>(info)};
    keep(out, s); keep(out, e); keep(out, v);
    return out;
}

std::vector<doublereal> run_qz(std::uint64_t seed)
{
    auto a = matrix(seed), b = matrix(seed + 1, true);
    std::vector<doublereal> z(order * order), ar(order), ai(order), beta(order);
    integer ierr = -1;
    xpp::qzhes(order, order, a.data(), b.data(), 0, z.data());
    xpp::qzit(order, order, a.data(), b.data(), 1e-10, 0, z.data(), &ierr);
    xpp::qzval(order, order, a.data(), b.data(), ar.data(), ai.data(), beta.data(), 0, z.data());
    std::vector<doublereal> out{static_cast<doublereal>(ierr)};
    keep(out, a); keep(out, b); keep(out, ar); keep(out, ai); keep(out, beta);
    return out;
}

constexpr int cases = 7;
std::vector<doublereal> run_case(int which, std::uint64_t seed)
{
    switch (which) {
    case 0: return run_rg(seed, 0);
    case 1: return run_rg(seed, 1);
    case 2: return run_schur(seed, 1);
    case 3: return run_schur(seed, -1);
    case 4: return run_svd(seed);
    case 5: return run_qz(seed);
    default: return run_rg(seed + 7, 1);
    }
}

bool same_bits(const std::vector<doublereal> &x, const std::vector<doublereal> &y)
{
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i)
        if (std::bit_cast<std::uint64_t>(x[i]) != std::bit_cast<std::uint64_t>(y[i])) return false;
    return true;
}

} // namespace

int main()
{
    std::array<std::vector<doublereal>, cases> reference;
    for (int c = 0; c < cases; ++c) {
        reference[static_cast<size_t>(c)] = run_case(c, 100 + static_cast<std::uint64_t>(c));
        CHECK(!reference[static_cast<size_t>(c)].empty());
        CHECK(reference[static_cast<size_t>(c)][0] == 0.); /* every status is success */
    }
    /* a call in between (another input) leaves a later call unchanged */
    for (int c = 0; c < cases; ++c) {
        (void)run_case((c + 3) % cases, 999);
        CHECK(same_bits(reference[static_cast<size_t>(c)], run_case(c, 100 + static_cast<std::uint64_t>(c))));
    }
    /* all threads at once, each running the cases in its own order with
       other inputs in between */
    std::atomic<int> differing = 0;
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < threads; ++t) pool.emplace_back([&, t] {
        for (int r = 0; r < rounds; ++r)
            for (int k = 0; k < cases; ++k) {
                const int c = static_cast<int>((static_cast<unsigned>(k) * (t + 1) + static_cast<unsigned>(r)) % cases);
                (void)run_case((c + 1) % cases, 5000 + t * 1000 + static_cast<unsigned>(r));
                if (!same_bits(reference[static_cast<size_t>(c)], run_case(c, 100 + static_cast<std::uint64_t>(c)))) ++differing;
            }
    });
    for (auto &th : pool) th.join();
    CHECK(differing == 0);
    TEST_REPORT("eispack");
}
