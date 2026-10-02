/* W163, #215: independent published references, not platform libm.
   John Burkardt, TEST_VALUES, bessel_j0/j1/jn/y0/y1/yn_values:
   https://people.sc.fsu.edu/~jburkardt/cpp_src/test_values/test_values.cpp
   Retrieved 2026-10-02; tables cite Abramowitz & Stegun and Mathematica.
   The printed values have about 16 significant digits. SciPy was not
   accessible in this sandbox. J1(7), Y0(4), J2(5) and Y2(10) are near zeros.
   This tests approximation accuracy, not correct rounding of Bessel J/Y. */
#include "xpptest.h"
#include "xpp_math.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
constexpr double reference_ulps = 16; // Allow table rounding and fdlibm recurrence error.
constexpr double near_zero_ulps = 64; // Phase/recurrence cancellation amplifies error near zeros.

bool within_ulps(double got, double want, double tolerance)
{
    const double spacing = std::nextafter(want, std::numeric_limits<double>::infinity())-want;
    return std::fabs(got-want) <= tolerance*spacing;
}

struct Reference {
    int n;
    double x, j, y;
    double tolerance = reference_ulps;
};
constexpr Reference references[] = {
    {0,1, 0.7651976865579666, 0.08825696421567696},
    {0,2, 0.2238907791412357, 0.5103756726497451},
    {0,4,-0.3971498098638474,-0.01694073932506499,near_zero_ulps},
    {0,7, 0.3000792705195556,-0.02594974396720926},
    {0,10,-0.2459357644513483,0.05567116728359939},
    {0,15,-0.01422447282678077,0.2054642960389183},
    {1,1, 0.4400505857449335,-0.7812128213002887},
    {1,2, 0.5767248077568734,-0.1070324315409375},
    {1,7,-0.004682823482345833,-0.3026672370241849,near_zero_ulps},
    {1,15,0.2051040386135228,0.02107362803687351},
    {2,1, 0.1149034849319005,-1.650682606816254},
    {2,2, 0.3528340286156377,-0.6174081041906827},
    {2,5, 0.04656511627775222,0.3676628826055245,near_zero_ulps},
    {2,10,0.2546303136851206,-0.005868082442208615,near_zero_ulps},
    {2,50,-0.05971280079425882,0.09579316872759649},
    {5,1, 0.0002497577302112344,-260.4058666258122},
    {5,2, 0.007039629755871685,-9.935989128481975},
    {5,5, 0.2611405461201701,-0.4536948224911019},
    {5,10,-0.2340615281867936,0.1354030476893623},
    {5,50,-0.08140024769656964,-0.07854841391308165}
};
} // namespace

int main()
{
    // Fold actual bits so UCRT/clang and CI can also compare sample results.
    std::uint64_t fingerprint = 0;
    for (const auto &r : references) {
        const double j = xpp::bessel_j(r.n,r.x), y = xpp::bessel_y(r.n,r.x);
        CHECK(within_ulps(j,r.j,r.tolerance));
        CHECK(within_ulps(y,r.y,r.tolerance));
        const auto jbits = std::bit_cast<std::uint64_t>(j), ybits = std::bit_cast<std::uint64_t>(y);
        CHECK(jbits == std::bit_cast<std::uint64_t>(xpp::bessel_j(r.n,r.x)));
        CHECK(ybits == std::bit_cast<std::uint64_t>(xpp::bessel_y(r.n,r.x)));
        fingerprint = std::rotl(fingerprint,1)^jbits;
        fingerprint = std::rotl(fingerprint,1)^ybits;
        const double sign = (r.n&1) ? -1 : 1;
        CHECK(xpp::bessel_j(-r.n,r.x) == sign*j);
        CHECK(xpp::bessel_j(r.n,-r.x) == sign*j);
        CHECK(xpp::bessel_y(-r.n,r.x) == sign*y);
        CHECK(xpp::bessel_j(r.n+0.75,r.x) == j);
        CHECK(xpp::bessel_y(r.n+0.75,r.x) == y);
    }
    CHECK(xpp::bessel_j(-2.75,1) == xpp::bessel_j(-2,1));
    CHECK(xpp::bessel_y(-2.75,1) == xpp::bessel_y(-2,1));
    CHECK(within_ulps(xpp::bessel_y(0,0.1),-1.534238651350367,reference_ulps));
    CHECK(within_ulps(xpp::bessel_y(1,0.1),-6.458951094702027,reference_ulps));
    constexpr double tiny = 0x1p-40; // Exercise fdlibm's tiny-argument Taylor branch.
    CHECK(xpp::bessel_j(0,tiny) == 1);
    CHECK(xpp::bessel_j(1,tiny) == tiny/2);
    CHECK(within_ulps(xpp::bessel_j(2,tiny),tiny*tiny/8,reference_ulps));
    CHECK(within_ulps(xpp::bessel_j(5,tiny),0x1p-205/120,reference_ulps));
    CHECK(xpp::bessel_j(0,0) == 1);
    CHECK(xpp::bessel_j(2,0) == 0);
    CHECK(std::signbit(xpp::bessel_j(1,-0.0)));
    CHECK(std::signbit(xpp::bessel_j(5,-0.0)));
    CHECK(std::isinf(xpp::bessel_y(0,0)) && xpp::bessel_y(0,0) < 0);
    CHECK(std::isinf(xpp::bessel_y(5,0)) && xpp::bessel_y(5,0) < 0);
    CHECK(std::isnan(xpp::bessel_y(2,-1)));
    constexpr double inf = std::numeric_limits<double>::infinity();
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    for (const int n : {0,1,2,5}) {
        CHECK(xpp::bessel_j(n,inf) == 0);
        CHECK(xpp::bessel_y(n,inf) == 0);
        CHECK(std::isnan(xpp::bessel_j(n,nan)));
        CHECK(std::isnan(xpp::bessel_y(n,nan)));
        CHECK(std::isnan(xpp::bessel_y(n,-inf)));
    }
    CHECK(xpp::bessel_j(std::numeric_limits<int>::min(),tiny) == 0);
    CHECK(std::isinf(xpp::bessel_y(std::numeric_limits<int>::min(),tiny)));
    // the same bits on every CPU, system and compiler (W159): CI runs this on
    // Linux, macOS, Windows UCRT and clang
    constexpr std::uint64_t expected_fingerprint = 0xcf87a488ffc0a3adULL;
    CHECK(fingerprint == expected_fingerprint);
    TEST_REPORT("bessel");
}
