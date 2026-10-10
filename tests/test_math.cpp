/* xpp::math: the correctly rounded transcendental functions (CORE-MATH,
   third_party/core-math), the same bits on every CPU (W159, issue #211).
   The expected bits are MPFR's correct rounding of the exact value, taken
   once for CORE-MATH's own hard-to-round inputs (its .wc worst-case lists:
   the exact value lies within 2^-44 or less of a rounding boundary, where
   a C library's variants disagree) and for the ends of each function's
   range; a C library's result is never the reference, since it is what
   differs between machines. */
#include "xpptest.h"
#include "xpp_math.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

static bool same(double got, std::uint64_t want_bits)
{
    return std::bit_cast<std::uint64_t>(got) == want_bits;
}

int main(void)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(std::isnan(xpp::bessel_i(nan, 1.0)));
    CHECK(std::isnan(xpp::bessel_i(2.0, nan)));
    CHECK(xpp::bessel_i(-3.0, 1.25) == xpp::bessel_i(3.0, 1.25));
    CHECK(std::isnan(xpp::bessel_i(100001.0, 1.0)));
    CHECK(xpp::bessel_i(0.0, 1.0) == xpp::bessel_i(0.9, 1.0)); /* XPPAUT truncates noninteger orders. */
    CHECK(std::isfinite(xpp::bessel_i(2.0, 1.0)));
    /* exact results and special values (IEEE 754-2019 9.2) */
    CHECK(xpp::math::exp(0.0) == 1.0);
    CHECK(xpp::math::exp(-std::numeric_limits<double>::infinity()) == 0.0);
    CHECK(std::isinf(xpp::math::exp(1000.0)));
    CHECK(xpp::math::log(1.0) == 0.0);
    CHECK(std::isinf(xpp::math::log(0.0)) && xpp::math::log(0.0) < 0);
    CHECK(std::isnan(xpp::math::log(-1.0)));
    CHECK(xpp::math::log10(1000.0) == 3.0);
    CHECK(xpp::math::pow(2.0, 10.0) == 1024.0);
    CHECK(xpp::math::pow(-8.0, 3.0) == -512.0);
    CHECK(xpp::math::pow(0.0, 0.0) == 1.0);
    CHECK(xpp::math::pow(7.5, 0.0) == 1.0);
    CHECK(std::isinf(xpp::math::pow(0.0, -1.0)));
    CHECK(std::isnan(xpp::math::pow(-8.0, 1.0 / 3.0)));
    CHECK(xpp::math::sin(0.0) == 0.0 && !std::signbit(xpp::math::sin(0.0)));
    CHECK(std::signbit(xpp::math::sin(-0.0)));
    CHECK(xpp::math::cos(0.0) == 1.0);
    CHECK(xpp::math::tan(0.0) == 0.0);
    CHECK(xpp::math::atan(0.0) == 0.0);
    CHECK(xpp::math::asin(1.0) == 0x1.921fb54442d18p+0);   /* pi/2 rounded */
    CHECK(xpp::math::acos(0.0) == 0x1.921fb54442d18p+0);
    CHECK(xpp::math::acos(1.0) == 0.0);
    CHECK(std::isnan(xpp::math::asin(1.5)));
    CHECK(xpp::math::atan2(1.0, 1.0) == 0x1.921fb54442d18p-1); /* pi/4 */
    CHECK(xpp::math::atan2(0.0, -1.0) == 0x1.921fb54442d18p+1); /* pi */
    CHECK(xpp::math::sinh(0.0) == 0.0);
    CHECK(xpp::math::cosh(0.0) == 1.0);
    CHECK(xpp::math::tanh(0.0) == 0.0);
    CHECK(xpp::math::tanh(40.0) == 1.0 && xpp::math::tanh(-40.0) == -1.0);
    CHECK(xpp::math::hypot(3.0, 4.0) == 5.0);
    CHECK(xpp::math::erf(0.0) == 0.0);
    CHECK(xpp::math::erf(10.0) == 1.0);
    CHECK(xpp::math::erfc(0.0) == 1.0);
    CHECK(xpp::math::lgamma(1.0) == 0.0);
    CHECK(xpp::math::lgamma(2.0) == 0.0);

    /* correctly rounded: the nearest double to the exact value */
    CHECK(same(xpp::math::exp(0x1.62e42fefa39efp+9), 0x7fefffffffffff2aULL));
    CHECK(same(xpp::math::exp(-0x1.74385446d71c3p+9), 0x0000000000000001ULL));
    CHECK(same(xpp::math::exp(0x1p-50), 0x3ff0000000000004ULL));
    CHECK(same(xpp::math::exp(-0x1p-52), 0x3feffffffffffffeULL));
    CHECK(same(xpp::math::log(0x1.a6ae5142326b5p+0), 0x3fe00bcc31ebded7ULL));
    CHECK(same(xpp::math::log(0x1.c877cba59d0acp+0), 0x3fe281c515a0a3dbULL));
    CHECK(same(xpp::math::log(0x1.c9f6ad292698fp+0), 0x3fe29c9138f38434ULL));
    CHECK(same(xpp::math::log(0x1.1e852e6951306p+1), 0x3fe9c8f0dc2695d4ULL));
    CHECK(same(xpp::math::log10(0x1.0341fa5659dbp-987), 0xc07291c71f735f43ULL));
    CHECK(same(xpp::math::log10(0x1.2be10ecbeb1aep-987), 0xc07290c4323427d8ULL));
    CHECK(same(xpp::math::log10(0x1.5107c5b6d236fp-987), 0xc0728ff46f839842ULL));
    CHECK(same(xpp::math::log10(0x1.77d31ad14ad2cp-987), 0xc0728f32a0dee2d1ULL));
    CHECK(same(xpp::math::sin(0x1.005023d32fee5p+1), 0x3fed109ad145c88fULL));
    CHECK(same(xpp::math::sin(0x1.00f620e3203b4p-7), 0x3f800f5744a7cb3aULL));
    CHECK(same(xpp::math::sin(0x1.01a1c30445418p-4), 0x3fb01764856366baULL));
    CHECK(same(xpp::math::sin(0x1.02a31aed55ef6p-9), 0x3f602a30fed68bc7ULL));
    CHECK(same(xpp::math::cos(0x1.0009effd4bedap-12), 0x3fefffffeffec1fcULL));
    CHECK(same(xpp::math::cos(0x1.00c82b482ed92p-10), 0x3feffffefe6f0e46ULL));
    CHECK(same(xpp::math::cos(0x1.018eea667029bp-8), 0x3fefffefcdfd370fULL));
    CHECK(same(xpp::math::cos(0x1.0281750d15454p-13), 0x3feffffffbebdb3aULL));
    CHECK(same(xpp::math::tan(0x1.0004b89dcb553p-11), 0x3f40004b9f3338f8ULL));
    CHECK(same(xpp::math::tan(0x1.008054c8de4cap-14), 0x3f1008054ce3bab4ULL));
    CHECK(same(xpp::math::tan(0x1.010f8a3ed8741p-19), 0x3ec010f8a3ed9cdaULL));
    CHECK(same(xpp::math::tan(0x1.01961ed94b362p-11), 0x3f401962035031f7ULL));
    CHECK(same(xpp::math::asin(0x1.fffffffffffffp-7), 0x3f90002aabdde94cULL));
    CHECK(same(xpp::math::asin(0x1.7137449123e7ep-26), 0x3e57137449123e7eULL));
    CHECK(same(xpp::math::asin(0x1.7137449123e83p-26), 0x3e57137449123e83ULL));
    CHECK(same(xpp::math::asin(0x1.7137449123e88p-26), 0x3e57137449123e88ULL));
    CHECK(same(xpp::math::acos(0x1.ffffffffffdcp-1), 0x3e98000000000024ULL));
    CHECK(same(xpp::math::acos(0x1.064abf8e63253p-1), 0x3ff086f4ccb61e07ULL));
    CHECK(same(xpp::math::acos(0x1.0e5dc0380efd6p-1), 0x3ff03b5290412f07ULL));
    CHECK(same(xpp::math::acos(0x1.246d2ca0e55f4p-4), 0x3ff7fd4e6a04bcb0ULL));
    CHECK(same(xpp::math::atan(0x1.000321dec01a8p-10), 0x3f500031c8938d9eULL));
    CHECK(same(xpp::math::atan(0x1.000a9ac7bfd91p-4), 0x3faff6adbe10151eULL));
    CHECK(same(xpp::math::atan(0x1.000ee76f5862ep-7), 0x3f8000d91e195f03ULL));
    CHECK(same(xpp::math::atan(0x1.0013c55306abbp-5), 0x3f9fffcdc7fb02e1ULL));
    CHECK(same(xpp::math::sinh(0x1.002ee87ae6797p-8), 0x3f7002f133d0bd22ULL));
    CHECK(same(xpp::math::sinh(0x1.011f8bf589229p-13), 0x3f2011f8c0057e73ULL));
    CHECK(same(xpp::math::sinh(0x1.01ae878849e03p-9), 0x3f601ae926920535ULL));
    CHECK(same(xpp::math::sinh(0x1.027ce505df535p-16), 0x3ef027ce5060b3f9ULL));
    CHECK(same(xpp::math::cosh(0x1.003c1d82e8bb4p+0), 0x3ff8b4e05c82a049ULL));
    CHECK(same(xpp::math::cosh(0x1.011f5eb541445p-21), 0x3ff0000000000205ULL));
    CHECK(same(xpp::math::cosh(0x1.01c3a61601eecp-14), 0x3ff000000081c534ULL));
    CHECK(same(xpp::math::cosh(0x1.024b660894d0cp-5), 0x3ff00209432db877ULL));
    CHECK(same(xpp::math::tanh(0x1.000b47bdb383fp-9), 0x3f6000b32658e41fULL));
    CHECK(same(xpp::math::tanh(0x1.008054cd28fecp-14), 0x3f1008054c7cba02ULL));
    CHECK(same(xpp::math::tanh(0x1.012950d84e3e5p-22), 0x3e9012950d84e38fULL));
    CHECK(same(xpp::math::tanh(0x1.01ae2526d9494p+2), 0x3feffac95de71f14ULL));
    CHECK(same(xpp::math::erf(0x1.037b548d9d7a6p-61), 0x3c224cb3732e544bULL));
    CHECK(same(xpp::math::erf(0x1.015e492c2b2b9p-61), 0x3c22268b65b228c8ULL));
    CHECK(same(xpp::math::erf(0x1.03868321b477cp-61), 0x3c224d7d545d171eULL));
    CHECK(same(xpp::math::erf(0x1.01679aa793a96p-61), 0x3c222733a1592077ULL));
    CHECK(same(xpp::math::erfc(0x1.c5bf891b4ef6bp-55), 0x3fefffffffffffffULL));
    CHECK(same(xpp::math::erfc(0x1.c5bf891b4ef7p-55), 0x3fefffffffffffffULL));
    CHECK(same(xpp::math::erfc(0x1.c5bf891b4ef75p-55), 0x3fefffffffffffffULL));
    CHECK(same(xpp::math::erfc(0x1.c5bf891b4ef7ap-55), 0x3fefffffffffffffULL));
    CHECK(same(xpp::math::lgamma(0x1.57480a40108eep-997), 0x40859631e6c31baeULL));
    CHECK(same(xpp::math::lgamma(0x1.58e719e4ad7c2p-997), 0x40859628405b78fcULL));
    CHECK(same(xpp::math::lgamma(0x1.5ae65d2f38a36p-997), 0x4085961c6d4839d1ULL));
    CHECK(same(xpp::math::lgamma(0x1.5c2bd26df0259p-997), 0x40859614ef619521ULL));
    CHECK(same(xpp::math::pow(0x1.c4269c893fd34p+50, 0x1.4p-2), 0x40ed79ca618b9632ULL));
    CHECK(same(xpp::math::pow(0x1.585e1192393dep-1, 0x1.274cefe28808p-3), 0x3fee38a93eb53e1eULL));
    CHECK(same(xpp::math::pow(0x1.5ac6d6dc2a1b2p-1, 0x1.e467e38d4404p-2), 0x3fea9d065f15fab6ULL));
    CHECK(same(xpp::math::pow(0x1.00929fb6649cap-1, 0x1.a87174451417ep-1), 0x3fe20c0425b8fa06ULL));
    CHECK(same(xpp::math::atan2(0x1p+0, 0x1p+0), 0x3fe921fb54442d18ULL));
    CHECK(same(xpp::math::atan2(0x1.430244f0c1042p+54, 0x1.bd1ce2a10f176p+51), 0x3ff6674aac0643e2ULL));
    CHECK(same(xpp::math::atan2(0x1.187097f0b3affp+59, 0x1.48459b689b68dp+52), 0x3ff8fc865c71209fULL));
    CHECK(same(xpp::math::atan2(0x1.02205f02753ebp+54, 0x1.3aa3dc4f62578p+52), 0x3ff466682adca5e7ULL));
    CHECK(same(xpp::math::hypot(0x1.000001cc784cbp+52, 0x1.fdd67bceb35fp+52), 0x4341d400aba3c4daULL));
    CHECK(same(xpp::math::hypot(0x1.87a86d92466cbp-28, 0x1p+0), 0x3ff0000000000000ULL));
    CHECK(same(xpp::math::hypot(0x1.e2b6844db320dp-1, 0x1.60396fa1ae709p-2), 0x3ff00eafdaf3b812ULL));
    CHECK(same(xpp::math::hypot(0x1.40f6c790598eep-1, 0x1.1ba94ac6bcc81p-2), 0x3fe5ee77ea43753bULL));

#ifdef XPP_CORE_MATH_FMA
    /* the plain copy and the FMA copy of each function are one function:
       the same bits at every input (a CPU with FMA runs the second, one
       without the first, and the numbers must not depend on which) */
    if (__builtin_cpu_supports("fma")) {
        std::uint64_t state = 0x9E3779B97F4A7C15ULL;
        auto next = [&] { /* xorshift64: a fixed sequence, the same on every machine */
            state ^= state << 13; state ^= state >> 7; state ^= state << 17;
            return static_cast<double>(state >> 11) * (1.0 / 9007199254740992.0);
        };
        auto equal = [](double a, double b) {
            return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b) || (std::isnan(a) && std::isnan(b));
        };
        long differ = 0;
        for (int i = 0; i < 20000; i++) {
            const double u = next(), v = next(), w = next();
            const double x = (u - 0.5) * (i % 3 == 0 ? 2.0 : i % 3 == 1 ? 40.0 : 1400.0); /* and beyond the ranges' ends */
            const double p = u * (i % 2 ? 4.0 : 1e3) + 1e-3, e = (v - 0.5) * 30.0, a = (w - 0.5) * 200.0;
            differ += !equal(cr_exp(x), cr_exp_fma(x)) + !equal(cr_log(p), cr_log_fma(p)) + !equal(cr_log10(p), cr_log10_fma(p)) +
                      !equal(cr_pow(p, e), cr_pow_fma(p, e)) + !equal(cr_sin(x), cr_sin_fma(x)) + !equal(cr_cos(x), cr_cos_fma(x)) +
                      !equal(cr_tan(x), cr_tan_fma(x)) + !equal(cr_asin(u * 2 - 1), cr_asin_fma(u * 2 - 1)) +
                      !equal(cr_acos(u * 2 - 1), cr_acos_fma(u * 2 - 1)) + !equal(cr_atan(x), cr_atan_fma(x)) +
                      !equal(cr_atan2(a, e), cr_atan2_fma(a, e)) + !equal(cr_sinh(x / 2), cr_sinh_fma(x / 2)) +
                      !equal(cr_cosh(x / 2), cr_cosh_fma(x / 2)) + !equal(cr_tanh(x), cr_tanh_fma(x)) +
                      !equal(cr_hypot(a, e), cr_hypot_fma(a, e)) + !equal(cr_erf(x / 3), cr_erf_fma(x / 3)) +
                      !equal(cr_erfc(x / 3), cr_erfc_fma(x / 3)) + !equal(cr_lgamma(a), cr_lgamma_fma(a));
        }
        CHECK(differ == 0);
    }
#endif

    TEST_REPORT("xpp::math");
}
