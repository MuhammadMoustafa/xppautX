#pragma once

/* The core's numerics toolbox (xpp_math.cpp, W32a): the one place for the
   Fourier transform, random numbers, the dense and banded linear solves,
   the equilibrium eigenvalues and the special functions the parser offers.
   A new routine of one of these kinds goes here, never into the file that
   needs it (AGENTS.md "Single source").

   autoX's exact algorithms live in xpp_math_autox.cpp and
   xpp_math_eispack.cpp (API: xpp_math_autox.h). Complete-pivot solves,
   EISPACK machine-precision eigenvalues/eigenvectors, QZ/SVD Floquet multipliers and
   HomCont Schur projections retain their own operation order. They are
   not substitutions for LINPACK partial pivoting or gear's fixed-tolerance
   Householder/QR eigenvalues; the declarations state each contract.
   del_stab.cpp's complex division still rounds differently from EISPACK's.

   Everything below is C++ in namespace xpp (W109a; the parser's function
   table holds some of these as plain function pointers). Matrix/Matrix3
   reject short spans; autoX's numerical failures retain their AutoxFailed
   error values for the command dispatcher. */


#include <cstddef>
#include <functional>
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "xpp_mem.h"
#include "../third_party/core-math/core_math.h"

namespace xpp {

/* A complex number as two doubles, real part then imaginary: was FortranComplex (f2c's
   doublecomplex, autoX) and del_stab.h's COMPLEX, the same pair. Each operation keeps its own formula. */
struct ComplexPair {
  double r, i;
};

/* A numerical result may be accepted only when every component is finite. */
bool all_finite(std::span<const double> values);

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
#define XPP_CR_UNARY(F) F(exp) F(log) F(log10) F(sin) F(cos) F(tan) F(asin) F(acos) F(atan) \
    F(sinh) F(cosh) F(tanh) F(erf) F(erfc) F(lgamma)
#define XPP_CR_BINARY(F) F(pow) F(atan2) F(hypot)

/* name(x) picks the copy per call; resolve(f) is, for a pointer to one of
   them, the copy itself with the CPU asked once, for code that calls the
   function through a pointer (the model's native code): no check and no
   thunk in each call. */
#ifdef XPP_CORE_MATH_FMA
#define XPP_CR_HAS_FMA __builtin_cpu_supports("fma")
#define XPP_CR_COPY(name) (XPP_CR_HAS_FMA ? cr_##name##_fma : cr_##name)
#else
#define XPP_CR_COPY(name) cr_##name
#endif
#define XPP_CR_DEFINE1(name) inline double name(double x) { return XPP_CR_COPY(name)(x); }
#define XPP_CR_DEFINE2(name) inline double name(double x, double y) { return XPP_CR_COPY(name)(x, y); }
XPP_CR_UNARY(XPP_CR_DEFINE1)
XPP_CR_BINARY(XPP_CR_DEFINE2)
#undef XPP_CR_DEFINE1
#undef XPP_CR_DEFINE2

#define XPP_CR_RESOLVE1(name) if (f == static_cast<double (*)(double)>(name)) return reinterpret_cast<const void *>(XPP_CR_COPY(name));
#define XPP_CR_RESOLVE2(name) if (f == static_cast<double (*)(double, double)>(name)) return reinterpret_cast<const void *>(XPP_CR_COPY(name));
inline const void *resolve(double (*f)(double)) {
    XPP_CR_UNARY(XPP_CR_RESOLVE1)
    return reinterpret_cast<const void *>(f);
}
inline const void *resolve(double (*f)(double, double)) {
    XPP_CR_BINARY(XPP_CR_RESOLVE2)
    return reinterpret_cast<const void *>(f);
}
#undef XPP_CR_RESOLVE1
#undef XPP_CR_RESOLVE2
#undef XPP_CR_COPY
#undef XPP_CR_HAS_FMA
#undef XPP_CR_UNARY
#undef XPP_CR_BINARY

/* ---- a model's x^n for a small whole n: multiplied out (W471) ----

   The largest exponent a model's literal x^n is multiplied out for, from
   2 (x*x, the correctly rounded square itself, W466): each product rounds
   once, so the error against the correctly rounded power grows with n,
   at most 1.2 ulp at 3, 1.8 at 4, 4.9 at 8 and 10.9 at 16 (relative
   2.4e-15, over random x), and gcc's __builtin_powi multiplies an integer
   constant alike; a larger exponent is pow's. */
constexpr int MAX_INTEGER_POWER = 16;

/* n is a whole exponent x^n is multiplied out for */
constexpr bool is_integer_power(double n)
{
    return n >= 2 && n <= MAX_INTEGER_POWER && n == static_cast<double>(static_cast<int>(n));
}

/* x^n, n >= 1, by squaring, in the one order both the expression engine's
   interpreter and its generated C (expr_native.cpp, V a C value there)
   take, so the two agree bit for bit: over n's bits from the lowest, x
   squared to x^2, x^4, x^8, ..., the result the first of these n has,
   times each further one it has, in turn (x^3 is x*(x*x), x^6 is
   (x*x)*((x*x)*(x*x))). multiply(a, b) is a*b: exact IEEE, so the same
   bits on every CPU and compiler (no FMA: -ffp-contract=off). */
template <class V, class Multiply> V power_by_squaring(V x, int n, Multiply multiply)
{
    for (; (n & 1) == 0; n >>= 1) x = multiply(x, x);
    V r = x;
    while ((n >>= 1) != 0) {
        x = multiply(x, x);
        if (n & 1) r = multiply(r, x);
    }
    return r;
}

inline double integer_power(double x, int n)
{
    return power_by_squaring(x, n, [](double a, double b) { return a * b; });
}

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

   A generator, MT19937-64, drawn through our own distributions (not
   the C++ library's, whose results differ between libstdc++, libc++ and
   MSVC): the same seed gives the same numbers on every platform. A
   Session has its own (Session::random: its stochastic runs, the parser's
   ran(), normal() and poisson(), Monte Carlo's guesses), so a load or
   another Session never draws from it. seed() seeds it (and forgets a
   normal() deviate kept from the last pair); uniform() is uniform in
   (0,1), never 0 or 1; normal() is Gaussian (Marsaglia's polar method);
   poisson() is Poisson with mean xm (Numerical Recipes' algorithm).

   save() is its full state -- 312 unsigned 64-bit words, an index in
   0..312, and normal()'s spare flag and deviate -- as portable text,
   and load() restores it (false and
   leaving the generator untouched if the text is not one save() wrote).
   Continuing a session (W57's session file) after Open then draws
   exactly the numbers it would have without stopping. */
class Random {
public:
    Random() { seed(1); }
    void seed(int seed);
    double uniform();
    double normal(double mean, double std);
    double poisson(double xm);
    std::string save() const;
    bool load(const std::string &state);

private:
    friend int next_seed(int seed);
    /* MT19937-64's parameters fix both the sequence and our saved layout;
       library stream operators do not agree on the state layout. */
    static constexpr size_t STATE_WORDS = 312; /* MT19937-64's recurrence length */
    static constexpr size_t RECURRENCE_OFFSET = 156; /* MT19937-64's middle word */
    std::array<std::uint64_t, STATE_WORDS> words_{};
    size_t index_ = STATE_WORDS;
    std::uint64_t draw();
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

/* the seeds of n runs one after another: seed, next_seed(seed), ... A sweep gives run i the i-th one before any run
   starts, so run i has the same numbers whichever thread computes it (W434). */
std::vector<int> seed_stream(int seed, std::size_t n);

/* ---- linear algebra ----

   lu_factor/lu_solve: LINPACK's dense LU factorisation with partial pivoting and
   its solve. a is n x n, row i at a[i*lda]; lu_factor factors it in place,
   the pivots in ipvt[n], *info -1 when it succeeded, else the (0-based)
   index of a zero pivot; lu_solve then solves A x = b, x replacing b.
   lu_determinant_sign: the sign of the factored matrix's determinant (a and
   ipvt as lu_factor left them): 1, -1, or 0 when a pivot is zero.

   band_factor/band_solve: the same for a banded matrix, without pivoting: ml
   sub- and mr super-diagonals, row i at a[i*(ml+mr+1)], its diagonal at
   a[i*(ml+mr+1)+ml]. band_factor returns 0, or -1-i when row i's pivot is 0.

   real_eigenvalues: the eigenvalues of the n x n matrix a (overwritten),
   stored as pairs ev[2i] (real), ev[2i+1] (imaginary); work holds n
   doubles. *ierr is 0, or the index of an eigenvalue that did not converge
   in 30 iterations.

   Their matrices keep LINPACK's pointer and dimensions (a 2-D layout a
   span would not describe); the solvers call them inside their steps. */
/* the index of the first of x's n elements (every stride-th) whose
   magnitude is the largest: LINPACK's isamax, lu_factor's pivot search; -1 for
   none */
int largest_magnitude(const double *x, int n, int stride = 1);
/* BLAS level 1: y[i] += a x[i] (add_multiple: LINPACK's saxpy, BLAS's daxpy) and x[i] *= a
   (scale_vector: sscal, dscal) over n elements at strides incx and incy (a negative stride
   starts from the far end, as in the BLAS) */
void add_multiple(int n, double sa, const double *sx, int incx, double *sy, int incy);
void scale_vector(int n, double sa, double *sx, int incx);
void lu_factor(double *a, int lda, int n, int *ipvt, int *info);
void lu_solve(double *a, int lda, int n, int *ipvt, double *b);
/* the same on a column-major matrix (a[j*lda+i] is M(i,j)), in place, the same bits */
void lu_factor_columns(double *a, int lda, int n, int *ipvt, int *info);
void lu_solve_columns(const double *a, int lda, int n, const int *ipvt, double *b);
int lu_determinant_sign(const double *a, int lda, int n, const int *ipvt);

/* BlockLu: lu_factor/lu_solve for a matrix that is block diagonal under a
   partition of its n indices (a model's independent subsystems, W433,
   Model::subsystems: each block's indices ascending), block by block.
   factor() gathers each block, factors it with lu_factor, keeps the factors
   and puts them back into a (as lu_factor leaves them, so a caller that
   scales the matrix again, Rosenbrock's retry, sees the same numbers);
   solve() solves each block of b with lu_solve. The entries outside the blocks
   are never read: they must be exactly 0, as a Jacobian's are where an
   equation does not read a state. Then the result is bit for bit lu_factor's and
   lu_solve's of the whole matrix, for finite values and up to the sign of a
   zero: the elimination adds a zero multiple or skips a zero pivot-row
   entry, and the pivot search meets only zeros outside a block's own rows,
   so neither an interchange nor an update leaves the block; *info is the
   last zero pivot's index, as lu_factor's. Two deliberate differences: lu_solve
   adds t*(+-0) across blocks, which turns a -0.0 of the solution into +0.0
   where this keeps -0.0; and a NaN or inf (a non-finite right-hand side, a
   singular matrix whose info the caller ignores) stays in its block where
   lu_factor and lu_solve spread it into every column. With fewer
   than two blocks, or blocks that do not cover n indices, factor() is lu_factor
   (lu_factor_columns when transposed) in place and solve() lu_solve, so `a` must stay as factor() left it until
   the solves are done. `transposed`: a holds the matrix's transpose (column-major,
   CVODE's DenseMatRec): a split's blocks are gathered the right way round,
   a whole matrix is factored where it lies. The
   buffers are sized by the first factor() and kept. */
class BlockLu {
public:
    void factor(double *a, int lda, int n, std::span<const std::vector<int>> blocks, int *info, bool transposed = false);
    void solve(double *a, int lda, double *b);

private:
    std::span<const std::vector<int>> blocks_; /* the caller's, when split_ */
    bool split_ = false, transposed_ = false;
    std::vector<double> lu_, rhs_;
    std::vector<int> pivots_;
};
int band_factor(double *a, int ml, int mr, int n);
void band_solve(double *a, double *b, int ml, int mr, int n);
void real_eigenvalues(int n, double *a, double *ev, double *work, int *ierr);

/* ---- column-major views (W334): the layout of autoX's Fortran arrays ----
   Matrix(i,j) is data[i + j*dim1] and Matrix3(i,j,k) data[i + (j + k*dim2)*dim1],
   the index ARRAY2D and ARRAY3D (autox_f2c.h) compute, so a loop keeps its
   order and a sum its operation order. The dimensions are stated once, where
   the view is made, from the span that must hold them (std::length_error
   when it is shorter: the owner's allocation is checked against the
   caller's dimensions there); an index stays inside them, as a Fortran
   array's did, unchecked in the hot loops. A view borrows its span: it
   dies with the storage and is made again after the storage is resized,
   moved or restored. T may be const (an input). */
template <class T> class Matrix {
public:
    Matrix() = default;
    Matrix(std::span<T> storage, std::ptrdiff_t dim1, std::ptrdiff_t dim2)
        : data_(storage.data()), dim1_(dim1) {
        if (size_product(dim1,dim2) > storage.size())
            throw std::length_error("a matrix view larger than its storage");
    }
    T &operator()(std::ptrdiff_t i, std::ptrdiff_t j) const { return data_[i + j * dim1_]; }
private:
    T *data_ = nullptr;
    std::ptrdiff_t dim1_ = 0;
};

template <class T> class Matrix3 {
public:
    Matrix3() = default;
    Matrix3(std::span<T> storage, std::ptrdiff_t dim1, std::ptrdiff_t dim2, std::ptrdiff_t dim3)
        : data_(storage.data()), dim1_(dim1), dim2_(dim2) {
        if (size_product(dim1,dim2,dim3) > storage.size())
            throw std::length_error("a matrix view larger than its storage");
    }
    T &operator()(std::ptrdiff_t i, std::ptrdiff_t j, std::ptrdiff_t k) const { return data_[i + (j + k * dim2_) * dim1_]; }
private:
    T *data_ = nullptr;
    std::ptrdiff_t dim1_ = 0, dim2_ = 0;
};

/* c(m x n) += a(m x p) * b(p x n), column-major with leading dimensions
   ldc, lda, ldb. Each c(i,j) gets its p terms added one after another,
   a(i,0)*b(0,j) first, each product rounded before the add (no
   reassociation, no FMA): the bits of `for k: c(i,j) += a(i,k)*b(k,j)`, for a
   c with no -0 (a term with a zero b factor is skipped when a is finite:
   it would add +-0, which changes only a -0). The loops run with i innermost
   so the compiler vectorizes them (the restrict qualifiers say c shares no
   element with a or b, which the caller guarantees; c's own elements are
   only ever added to). */
void product_accumulate(double *__restrict c, std::ptrdiff_t ldc, const double *__restrict a, std::ptrdiff_t lda,
                        const double *__restrict b, std::ptrdiff_t ldb, std::ptrdiff_t m, std::ptrdiff_t n, std::ptrdiff_t p);

/* target[l] -= factor * source[l] for l in [0, n): each product rounded before the
   subtract (no FMA), the bits of the plain loop. Written in groups of four, which
   gcc's -O2 vectorizes where it leaves a loop of unknown length scalar (W475). */
inline void subtract_scaled(double *__restrict target, const double *__restrict source, double factor, std::ptrdiff_t n)
{
    std::ptrdiff_t l = 0;
    for (; l + 4 <= n; l += 4) {
        target[l] -= factor * source[l];
        target[l + 1] -= factor * source[l + 1];
        target[l + 2] -= factor * source[l + 2];
        target[l + 3] -= factor * source[l + 3];
    }
    for (; l < n; ++l) target[l] -= factor * source[l];
}

/* Refine a sign-changing scalar bracket by safeguarded secant/bisection.
   Returns a point in the bracket within 8*epsilon*max(1,abs(time)). */
double bracketed_root(const std::function<double(double)> &f, double a, double b, double fa, double fb);

/* ---- small helpers ----
   transfer_sign: Fortran's SIGN, |a| with the sign of b (b >= 0: +|a|; f2c's libF77 transfer_sign). */
double transfer_sign(double a, double b);

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

