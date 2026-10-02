#ifndef XPP_MATH_H
#define XPP_MATH_H

/* The core's numerics toolbox (xpp_math.cpp, W32a): the one place for the
   Fourier transform, random numbers, the dense and banded linear solves,
   the equilibrium eigenvalues and the special functions the parser offers.
   A new routine of one of these kinds goes here, never into the file that
   needs it (CLAUDE.md "Single source").

   Left where they are, each for a reason:
   - AUTO's own Gaussian elimination, ge() in autlib1.cpp: complete
     pivoting, a determinant and a pivot log in fort.9; LINPACK's partial
     pivoting below would change AUTO's results.
   - EISPACK (eispack.cpp): AUTO's eigenvalues and Floquet multipliers.
     xpp_eigenvalues below is the same orthes/hqr pair written with a
     fixed 1e-10 convergence test, where EISPACK's hqr iterates to machine
     precision: the two give eigenvalues that differ in the last digits
     (up to ~1e-10), so the equilibria keep their own.
   - del_stab.cpp's and EISPACK's complex divisions: the same quotient
     rounded differently (std::complex's differs again between C++
     libraries).

   Everything below is C++ in namespace xpp (W109a; the parser's function
   table holds some of these as plain function pointers). None of them
   throws. */

#include "../third_party/core-math/core_math.h"

#include <cstddef>
#include <random>
#include <span>
#include <string>

namespace xpp {

/* ---- the transcendental functions: the same bits on every CPU (W159) ----

   xpp::math::exp, log, log10, pow, sin, cos, tan, asin, acos, atan, atan2,
   sinh, cosh, tanh, hypot, erf, erfc and lgamma are CORE-MATH's
   (third_party/core-math), correctly rounded in binary64: each returns the
   exact value rounded to nearest, so it is the same number whatever the C
   library, CPU or compiler. The C library's are not: glibc picks FMA or
   SSE2 variants of exp, log, pow, sin, cos, tan, atan, asin, acos, atan2
   at run time (and lgamma, erf and the hyperbolic functions call them),
   they round differently in about one call in 1500, and UCRT and macOS have
   algorithms of their own; an adaptive step size or a chaotic model turns
   that into a different output on another machine (issue #211). Core code
   calls these, never <cmath>'s (tools/mathcheck.sh fails it). Exact in
   IEEE and so the C library's: sqrt, fabs, floor, ceil, fmod, ldexp, frexp.
   Bessel J/Y use the musl implementation over these functions (W163). */
namespace math {

/* On x86, CPUs with FMA run the copy of each function built with FMA
   instructions (third_party/core-math/README.md): the same bits, much
   faster than the plain copy. Whether the CPU has it is the compiler
   runtime's own table, read for free. */
#ifdef XPP_CORE_MATH_FMA
#define XPP_CR_PICK(name, ...) (__builtin_cpu_supports("fma") ? cr_##name##_fma(__VA_ARGS__) : cr_##name(__VA_ARGS__))
#else
#define XPP_CR_PICK(name, ...) cr_##name(__VA_ARGS__)
#endif

inline double exp(double x) { return XPP_CR_PICK(exp, x); }
inline double log(double x) { return XPP_CR_PICK(log, x); }
inline double log10(double x) { return XPP_CR_PICK(log10, x); }
inline double pow(double x, double y) { return XPP_CR_PICK(pow, x, y); }
inline double sin(double x) { return XPP_CR_PICK(sin, x); }
inline double cos(double x) { return XPP_CR_PICK(cos, x); }
inline double tan(double x) { return XPP_CR_PICK(tan, x); }
inline double asin(double x) { return XPP_CR_PICK(asin, x); }
inline double acos(double x) { return XPP_CR_PICK(acos, x); }
inline double atan(double x) { return XPP_CR_PICK(atan, x); }
inline double atan2(double y, double x) { return XPP_CR_PICK(atan2, y, x); }
inline double sinh(double x) { return XPP_CR_PICK(sinh, x); }
inline double cosh(double x) { return XPP_CR_PICK(cosh, x); }
inline double tanh(double x) { return XPP_CR_PICK(tanh, x); }
inline double hypot(double x, double y) { return XPP_CR_PICK(hypot, x, y); }
inline double erf(double x) { return XPP_CR_PICK(erf, x); }
inline double erfc(double x) { return XPP_CR_PICK(erfc, x); }
inline double lgamma(double x) { return XPP_CR_PICK(lgamma, x); }

#undef XPP_CR_PICK

} // namespace math

/* ---- Fourier transform (pocketfft, third_party/pocketfft) ----

   fft: the discrete Fourier transform of the n = re.size() complex points,
   in place (im as long as re),
       X[k] = factor * sum_j x[j] exp(sign * 2 pi i j k / n),
   sign +1 or -1, the real parts in re[], the imaginary ones in im[].
   factor 1 leaves it unscaled; 1/n or 1/sqrt(n) normalise it.

   fft_real: the same transform of the n = in.size() real points in[], of
   which only X[0..n/2] are stored into re[] and im[] (n/2+1 each): the
   others are their complex conjugates, X[n-k] = conj(X[k]). */
void fft(std::span<double> re, std::span<double> im, int sign, double factor);
void fft_real(std::span<const double> in, std::span<double> re, std::span<double> im, int sign,
              double factor);

/* ---- random numbers ----

   A generator, std::mt19937_64, drawn through our own distributions (not
   the C++ library's, whose results differ between libstdc++, libc++ and
   MSVC): the same seed gives the same numbers on every platform. A
   Session has its own (Session::random: its stochastic runs, the parser's
   ran(), normal() and poisson(), Monte Carlo's guesses), so a load or
   another Session never draws from it. seed() seeds it (and forgets a
   normal() deviate kept from the last pair); uniform() is uniform in
   (0,1), never 0 or 1; normal() is Gaussian (Marsaglia's polar method);
   poisson() is Poisson with mean xm (Numerical Recipes' algorithm).

   save() is its full state -- std::mt19937_64's state and normal()'s
   spare deviate -- as opaque text, and load() restores it (false and
   leaving the generator untouched if the text is not one save() wrote).
   Continuing a session (W57's session file) after Open then draws
   exactly the numbers it would have without stopping. */
class Random {
public:
    void seed(int seed);
    double uniform();
    double normal(double mean, double std);
    double poisson(double xm);
    std::string save() const;
    bool load(const std::string &state);

private:
    /* std::mt19937_64's algorithm and seeding are fixed by the standard,
       so every C++ library gives the same sequence for a seed */
    std::mt19937_64 engine_{1};
    /* normal() draws its deviates in pairs and keeps the second */
    bool have_spare_ = false;
    double spare_ = 0.0;
};

/* next_seed(seed) is a separate seed stream: a pure function of seed
   (touches no Random), used to pick the following run's seed once a run
   has used this one (W71's "a seed per run"), so an untouched Session
   still gets fresh noise every run while every run's own seed stays a
   small loggable int. */
int next_seed(int seed);

/* ---- linear algebra ----

   sgefa/sgesl: LINPACK's dense LU factorisation with partial pivoting and
   its solve. a is n x n, row i at a[i*lda]; sgefa factors it in place,
   the pivots in ipvt[n], *info -1 when it succeeded, else the (0-based)
   index of a zero pivot; sgesl then solves A x = b, x replacing b.
   sgefa_det_sign: the sign of the factored matrix's determinant (a and
   ipvt as sgefa left them): 1, -1, or 0 when a pivot is zero.

   bandfac/bandsol: the same for a banded matrix, without pivoting: ml
   sub- and mr super-diagonals, row i at a[i*(ml+mr+1)], its diagonal at
   a[i*(ml+mr+1)+ml]. bandfac returns 0, or -1-i when row i's pivot is 0.

   eigenvalues: the eigenvalues of the n x n matrix a (overwritten),
   stored as pairs ev[2i] (real), ev[2i+1] (imaginary); work holds n
   doubles. *ierr is 0, or the index of an eigenvalue that did not converge
   in 30 iterations.

   Their matrices keep LINPACK's pointer and dimensions (a 2-D layout a
   span would not describe); the solvers call them inside their steps. */
void sgefa(double *a, int lda, int n, int *ipvt, int *info);
void sgesl(double *a, int lda, int n, int *ipvt, double *b);
int sgefa_det_sign(const double *a, int lda, int n, const int *ipvt);
int bandfac(double *a, int ml, int mr, int n);
void bandsol(double *a, double *b, int ml, int mr, int n);
void eigenvalues(int n, double *a, double *ev, double *work, int *ierr);

/* ---- small helpers ----
   sign: Fortran's SIGN, |a| with the sign of b (b >= 0: +|a|). */
double sign(double a, double b);

/* ---- special functions (the parser's besselj, bessely, besseli,
   besselis) ----
   The order n is truncated to an int. bessel_j/_y are the vendored musl
   implementation over xpp::math; bessel_i is the modified Bessel function I_n(x) and
   bessel_i_scaled exp(-|x|) I_n(x) (Numerical Recipes' polynomial
   approximations and downward recurrence). */
double bessel_j(double n, double x);
double bessel_y(double n, double x);
double bessel_i(double n, double x);
double bessel_i_scaled(double n, double x);

} // namespace xpp

#endif
