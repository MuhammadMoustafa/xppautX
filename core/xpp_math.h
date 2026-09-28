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

   Everything below is C (extern "C"): the parser's function table holds
   some of these as plain function pointers. None of them throws. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Fourier transform (pocketfft, third_party/pocketfft) ----

   xpp_fft: the discrete Fourier transform of n complex points, in place,
       X[k] = factor * sum_j x[j] exp(sign * 2 pi i j k / n),
   sign +1 or -1, the real parts in re[], the imaginary ones in im[].
   factor 1 leaves it unscaled; 1/n or 1/sqrt(n) normalise it.

   xpp_fft_real: the same transform of n real points in[], of which only
   X[0..n/2] are stored into re[] and im[] (n/2+1 each): the others are
   their complex conjugates, X[n-k] = conj(X[k]). */
void xpp_fft(size_t n, double *re, double *im, int sign, double factor);
void xpp_fft_real(size_t n, const double *in, double *re, double *im, int sign,
                  double factor);

/* ---- random numbers ----

   One generator, std::mt19937_64, drawn through our own distributions
   (not the C++ library's, whose results differ between libstdc++, libc++
   and MSVC): the same seed gives the same numbers on every platform.
   nsrand48 seeds it (and forgets a normal() deviate kept from the last
   pair); ndrand48 is uniform in (0,1), never 0 or 1; normal() is Gaussian
   (Marsaglia's polar method); poidev() is Poisson with mean xm (Numerical
   Recipes' algorithm). The names are the historical ones the parser and
   the stochastic code use.

   xpp_next_seed(seed) is a separate seed stream: a pure function of seed
   (touches neither the generator above nor its spare deviate), used to
   pick the following run's seed once a run has used this one (W71's "a
   seed per run"), so an untouched Session still gets fresh noise every
   run while every run's own seed stays a small loggable int. */
void nsrand48(int seed);
double ndrand48(void);
double normal(double mean, double std);
double poidev(double xm);
int xpp_next_seed(int seed);

/* ---- linear algebra ----

   sgefa/sgesl: LINPACK's dense LU factorisation with partial pivoting and
   its solve. a is n x n, row i at a[i*lda]; sgefa factors it in place,
   the pivots in ipvt[n], *info -1 when it succeeded, else the (0-based)
   index of a zero pivot; sgesl then solves A x = b, x replacing b.

   bandfac/bandsol: the same for a banded matrix, without pivoting: ml
   sub- and mr super-diagonals, row i at a[i*(ml+mr+1)], its diagonal at
   a[i*(ml+mr+1)+ml]. bandfac returns 0, or -1-i when row i's pivot is 0.

   xpp_eigenvalues: the eigenvalues of the n x n matrix a (overwritten),
   stored as pairs ev[2i] (real), ev[2i+1] (imaginary); work holds n
   doubles. *ierr is 0, or the index of an eigenvalue that did not converge
   in 30 iterations. */
void sgefa(double *a, int lda, int n, int *ipvt, int *info);
void sgesl(double *a, int lda, int n, int *ipvt, double *b);
int bandfac(double *a, int ml, int mr, int n);
void bandsol(double *a, double *b, int ml, int mr, int n);
void xpp_eigenvalues(int n, double *a, double *ev, double *work, int *ierr);

/* ---- small helpers ----
   xpp_sign: Fortran's SIGN, |a| with the sign of b (b >= 0: +|a|). */
double xpp_sign(double a, double b);

/* ---- special functions (the parser's besselj, bessely, besseli,
   besselis) ----
   The order n is truncated to an int. xpp_bessel_j/_y are the C library's
   jn/yn; xpp_bessel_i is the modified Bessel function I_n(x) and
   xpp_bessel_i_scaled exp(-|x|) I_n(x) (Numerical Recipes' polynomial
   approximations and downward recurrence). */
double xpp_bessel_j(double n, double x);
double xpp_bessel_y(double n, double x);
double xpp_bessel_i(double n, double x);
double xpp_bessel_i_scaled(double n, double x);

#ifdef __cplusplus
}

#include <string>

namespace xpp {

/* the random generator's full state -- std::mt19937_64's state and
   normal()'s spare deviate -- as opaque text (xpp_rand_state_save), and
   restoring it (xpp_rand_state_load, false and leaving the generator
   untouched if the text is not one this function wrote). Continuing a
   session (W57's session file) after Open then draws exactly the numbers
   it would have without stopping. */
std::string xpp_rand_state_save();
bool xpp_rand_state_load(const std::string &state);

} // namespace xpp

#endif

#endif
