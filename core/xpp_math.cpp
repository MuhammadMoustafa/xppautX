/* The core's numerics toolbox: Fourier transform, random numbers, linear
   algebra, special functions. See xpp_math.h for what each does and for
   the copies left elsewhere on purpose. */
#include "xpp_math.h"

#include "xpp_log.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <numbers>
#include <random>
#include <vector>

/* the core is single-threaded: no thread pool */
#define POCKETFFT_NO_MULTITHREADING
#include "../third_party/pocketfft/pocketfft_hdronly.h"

/* ------------------------------------------------------------------ */
/* Fourier transform                                                   */

namespace {

using cplx = std::complex<double>;

/* pocketfft throws only std::bad_alloc here (the shapes and strides are
   always valid): treat it like a failed xpp_malloc, loud and fatal */
[[noreturn]] void fft_failed(const std::exception &e, size_t n)
{
    xpp::log(XPP_LOG_ERROR, "Fourier transform of {} points failed: {}\n", n, e.what());
    std::exit(1);
}

} // namespace

void xpp_fft(size_t n, double *re, double *im, int sign, double factor)
{
    if (n == 0) return;
    try {
        std::vector<cplx> c(n);
        for (size_t i = 0; i < n; i++) c[i] = cplx(re[i], im[i]);
        const pocketfft::stride_t stride{static_cast<ptrdiff_t>(sizeof(cplx))};
        /* pocketfft's forward transform has the exponent's minus sign */
        pocketfft::c2c<double>({n}, stride, stride, {0}, sign < 0, c.data(), c.data(), factor);
        for (size_t i = 0; i < n; i++) {
            re[i] = c[i].real();
            im[i] = c[i].imag();
        }
    } catch (const std::exception &e) {
        fft_failed(e, n);
    }
}

void xpp_fft_real(size_t n, const double *in, double *re, double *im, int sign, double factor)
{
    if (n == 0) return;
    try {
        std::vector<cplx> c(n / 2 + 1);
        pocketfft::r2c<double>({n}, {static_cast<ptrdiff_t>(sizeof(double))},
                               {static_cast<ptrdiff_t>(sizeof(cplx))}, size_t{0}, sign < 0, in,
                               c.data(), factor);
        for (size_t i = 0; i < c.size(); i++) {
            re[i] = c[i].real();
            im[i] = c[i].imag();
        }
    } catch (const std::exception &e) {
        fft_failed(e, n);
    }
}

/* ------------------------------------------------------------------ */
/* Random numbers                                                      */

namespace {

/* std::mt19937_64's algorithm and seeding are fixed by the standard, so
   every C++ library gives the same sequence for a seed */
std::mt19937_64 engine(1);
/* normal() draws its deviates in pairs and keeps the second */
bool have_spare = false;
double spare = 0.0;

} // namespace

void nsrand48(int seed)
{
    engine.seed(static_cast<std::uint64_t>(static_cast<std::int64_t>(seed)));
    have_spare = false;
}

double ndrand48(void)
{
    /* the top 53 bits, centred in their interval: (0,1), never 0 (the
       callers take its log) nor 1 */
    return (static_cast<double>(engine() >> 11) + 0.5) * 0x1.0p-53;
}

double normal(double mean, double std)
{
    if (have_spare) {
        have_spare = false;
        return spare * std + mean;
    }
    double v1, v2, r;
    do {
        v1 = 2.0 * ndrand48() - 1.0;
        v2 = 2.0 * ndrand48() - 1.0;
        r = v1 * v1 + v2 * v2;
    } while (r >= 1.0 || r == 0.0);
    const double fac = std::sqrt(-2.0 * std::log(r) / r);
    spare = v1 * fac;
    have_spare = true;
    return v2 * fac * std + mean;
}

double poidev(double xm)
{
    double em, t, y;
    if (xm < 12.0) { /* multiply uniforms until the product drops below e^-xm */
        const double g = std::exp(-xm);
        em = -1;
        t = 1.0;
        do {
            ++em;
            t *= ndrand48();
        } while (t > g);
    } else { /* rejection from a Lorentzian */
        const double sq = std::sqrt(2.0 * xm);
        const double alxm = std::log(xm);
        const double g = xm * alxm - std::lgamma(xm + 1.0);
        do {
            do {
                y = std::tan(std::numbers::pi * ndrand48());
                em = sq * y + xm;
            } while (em < 0.0);
            em = std::floor(em);
            t = 0.9 * (1.0 + y * y) * std::exp(em * alxm - std::lgamma(em + 1.0) - g);
        } while (ndrand48() > t);
    }
    return em;
}

/* ------------------------------------------------------------------ */
/* Dense LU (LINPACK's sgefa/sgesl, row-major)                          */

namespace {

/* the BLAS-1 kernels sgefa/sgesl use, with their stride conventions */
void saxpy(int n, double sa, const double *sx, int incx, double *sy, int incy)
{
    if (n <= 0) return;
    if (sa == 0.0) return;
    int ix = 0, iy = 0;
    if (incx < 0) ix = -n * incx;
    if (incy < 0) iy = -n * incy;
    for (int i = 0; i < n; i++, ix += incx, iy += incy) sy[iy] = sy[iy] + sa * sx[ix];
}

int isamax(int n, const double *sx, int incx)
{
    if (n < 1) return -1;
    if (n == 1) return 0;
    int imax = 0;
    double smax = std::fabs(sx[0]);
    int ix = incx;
    for (int i = 1; i < n; i++, ix += incx) {
        if (std::fabs(sx[ix]) > smax) {
            imax = i;
            smax = std::fabs(sx[ix]);
        }
    }
    return imax;
}

void sscal(int n, double sa, double *sx, int incx)
{
    if (n <= 0) return;
    const int nincx = n * incx;
    for (int i = 0; i < nincx; i += incx) sx[i] *= sa;
}

} // namespace

void sgefa(double *a, int lda, int n, int *ipvt, int *info)
{
    *info = -1;
    for (int k = 1; k <= n - 1; k++) {
        const int l = isamax(n - k + 1, &a[(k - 1) * lda + k - 1], lda) + k - 1;
        ipvt[k - 1] = l;
        if (a[l * lda + k - 1] == 0.0) {
            *info = k - 1;
            continue;
        }
        double t;
        if (l != (k - 1)) {
            t = a[l * lda + k - 1];
            a[l * lda + k - 1] = a[(k - 1) * lda + k - 1];
            a[(k - 1) * lda + k - 1] = t;
        }
        t = -1.0 / a[(k - 1) * lda + k - 1];
        sscal(n - k, t, (a + k * lda + k - 1), lda);
        for (int j = k + 1; j <= n; j++) {
            t = a[l * lda + j - 1];
            if (l != (k - 1)) {
                a[l * lda + j - 1] = a[(k - 1) * lda + j - 1];
                a[(k - 1) * lda + j - 1] = t;
            }
            saxpy(n - k, t, (a + k * lda + k - 1), lda, (a + k * lda + j - 1), lda);
        }
    }
    ipvt[n - 1] = n - 1;
    if (a[(n - 1) * lda + n - 1] == 0.0) *info = n - 1;
}

void sgesl(double *a, int lda, int n, int *ipvt, double *b)
{
    for (int k = 1; k <= n - 1; k++) {
        const int l = ipvt[k - 1];
        const double t = b[l];
        if (l != (k - 1)) {
            b[l] = b[k - 1];
            b[k - 1] = t;
        }
        saxpy(n - k, t, (a + lda * k + k - 1), lda, (b + k), 1);
    }
    for (int kb = 1; kb <= n; kb++) {
        const int k = n + 1 - kb;
        b[k - 1] = b[k - 1] / a[(k - 1) * lda + k - 1];
        const double t = -b[k - 1];
        saxpy(k - 1, t, (a + k - 1), lda, b, 1);
    }
}

/* ------------------------------------------------------------------ */
/* Banded LU, no pivoting                                              */

int bandfac(double *a, int ml, int mr, int n)
{
    const int n1 = n - 1, mt = ml + mr + 1;
    for (int row = 0; row < n; row++) {
        const int r0 = row * mt + ml;
        double al;
        if ((al = a[r0]) == 0.0) return -1 - row;
        al = 1.0 / al;
        const int m = std::min(mr, n1 - row);
        for (int j = 1; j <= m; j++) a[r0 + j] = a[r0 + j] * al;
        a[r0] = al;
        for (int i = 1; i <= ml; i++) {
            const int rowi = row + i;
            if (rowi > n1) break;
            const int ri0 = rowi * mt + ml;
            al = a[ri0 - i];
            if (al == 0.0) continue;
            for (int k = 1; k <= m; k++) a[ri0 - i + k] = a[ri0 - i + k] - (al * a[r0 + k]);
            a[ri0 - i] = -al;
        }
    }
    return 0;
}

void bandsol(double *a, double *b, int ml, int mr, int n)
{
    const int mt = ml + mr + 1, n1 = n - 1;
    for (int i = 0; i < n; i++) {
        const int r0 = i * mt + ml;
        for (int j = std::max(-ml, -i); j < 0; j++) b[i] += a[r0 + j] * b[i + j];
        b[i] *= a[r0];
    }
    for (int row = n1 - 1; row >= 0; row--) {
        const int m = std::min(mr, n1 - row);
        const int r0 = row * mt + ml;
        for (int k = 1; k <= m; k++) b[row] = b[row] - a[r0 + k] * b[row + k];
    }
}

/* ------------------------------------------------------------------ */
/* Eigenvalues: EISPACK's orthes (Householder reduction to Hessenberg  */
/* form) and hqr (shifted QR), column-major, 1-based loops as in the   */
/* Fortran, with a fixed convergence tolerance                         */

double xpp_sign(double a, double b)
{
    if (b >= 0.0) return std::fabs(a);
    return -std::fabs(a);
}

namespace {

void orthes(int n, int low, int igh, double *a, double *ort)
{
    const int la = igh - 1;
    const int kp1 = low + 1;
    if (la < kp1) return;
    for (int m = kp1; m <= la; m++) {
        double h = 0.0;
        ort[m - 1] = 0.0;
        double scale = 0.0;
        for (int i = m; i <= igh; i++) scale = scale + std::fabs(a[i - 1 + (m - 2) * n]);
        if (scale == 0.0) continue;
        const int mp = m + igh;
        for (int ii = m; ii <= igh; ii++) {
            const int i = mp - ii;
            ort[i - 1] = a[i - 1 + (m - 2) * n] / scale;
            h = h + ort[i - 1] * ort[i - 1];
        }
        const double g = -xpp_sign(std::sqrt(h), ort[m - 1]);
        h = h - ort[m - 1] * g;
        ort[m - 1] = ort[m - 1] - g;
        for (int j = m; j <= n; j++) {
            double f = 0.0;
            for (int ii = m; ii <= igh; ii++) {
                const int i = mp - ii;
                f = f + ort[i - 1] * a[i - 1 + (j - 1) * n];
            }
            f = f / h;
            for (int i = m; i <= igh; i++) a[i - 1 + (j - 1) * n] = a[i - 1 + (j - 1) * n] - f * ort[i - 1];
        }
        for (int i = 1; i <= igh; i++) {
            double f = 0.0;
            for (int jj = m; jj <= igh; jj++) {
                const int j = mp - jj;
                f = f + ort[j - 1] * a[i - 1 + (j - 1) * n];
            }
            f = f / h;
            for (int j = m; j <= igh; j++) a[i - 1 + (j - 1) * n] = a[i - 1 + (j - 1) * n] - f * ort[j - 1];
        }
        ort[m - 1] = scale * ort[m - 1];
        a[m - 1 + (m - 2) * n] = scale * g;
    }
}

/* EISPACK's hqr, labels kept from the Fortran (l60: next eigenvalue,
   l70: next iteration, l270: one root found, l280: two, l1000: no
   convergence) */
void hqr(int n, int low, int igh, double *h, double *ev, int *ierr)
{
    int i, j, k, l = 0, m = 0, en, ll, mm, na, its, mp2, enm2;
    double p = 0.0, q = 0.0, r = 0.0, s, t, w, x, y, zz, norm;
    const double machep = 1.e-10;
    bool notlas;
    *ierr = 0;
    norm = 0.0;
    k = 1;
    for (i = 1; i <= n; i++) {
        for (j = k; j <= n; j++) norm = norm + std::fabs(h[i - 1 + (j - 1) * n]);
        k = i;
        if ((i >= low) && (i <= igh)) continue;
        ev[(i - 1) * 2] = h[i - 1 + (i - 1) * n];
        ev[1 + (i - 1) * 2] = 0.0;
    }
    en = igh;
    t = 0.0;
l60:
    if (en < low) return;
    its = 0;
    na = en - 1;
    enm2 = na - 1;
l70:
    for (ll = low; ll <= en; ll++) {
        l = en + low - ll;
        if (l == low) break;
        s = std::fabs(h[l - 2 + (l - 2) * n]) + std::fabs(h[l - 1 + (l - 1) * n]);
        if (s == 0.0) s = norm;
        if (std::fabs(h[l - 1 + (l - 2) * n]) <= machep * s) break;
    }
    x = h[en - 1 + (en - 1) * n];
    if (l == en) goto l270;
    y = h[na - 1 + (na - 1) * n];
    w = h[en - 1 + (na - 1) * n] * h[na - 1 + (en - 1) * n];
    if (l == na) goto l280;
    if (its == 30) goto l1000;
    if ((its != 10) && (its != 20)) goto l130;
    t = t + x;
    for (i = low; i <= en; i++) h[i - 1 + (i - 1) * n] = h[i - 1 + (i - 1) * n] - x;
    s = std::fabs(h[en - 1 + (na - 1) * n]) + std::fabs(h[na - 1 + (enm2 - 1) * n]);
    x = 0.75 * s;
    y = x;
    w = -0.4375 * s * s;
l130:
    its++;
    for (mm = l; mm <= enm2; mm++) {
        m = enm2 + l - mm;
        zz = h[m - 1 + (m - 1) * n];
        r = x - zz;
        s = y - zz;
        p = (r * s - w) / h[m + (m - 1) * n] + h[m - 1 + m * n];
        q = h[m + m * n] - zz - r - s;
        r = h[m + 1 + m * n];
        s = std::fabs(p) + std::fabs(q) + std::fabs(r);
        p = p / s;
        q = q / s;
        r = r / s;
        if (m == l) break;
        if ((std::fabs(h[m - 1 + (m - 2) * n]) * (std::fabs(q) + std::fabs(r))) <=
            (machep * std::fabs(p) *
             (std::fabs(h[m - 2 + (m - 2) * n]) + std::fabs(zz) + std::fabs(h[m + m * n]))))
            break;
    }
    mp2 = m + 2;
    for (i = mp2; i <= en; i++) {
        h[i - 1 + (i - 3) * n] = 0.0;
        if (i == mp2) continue;
        h[i - 1 + (i - 4) * n] = 0.0;
    }
    for (k = m; k <= na; k++) {
        notlas = (k != na);
        if (k != m) {
            p = h[k - 1 + (k - 2) * n];
            q = h[k + (k - 2) * n];
            r = 0.0;
            if (notlas) r = h[k + 1 + (k - 2) * n];
            x = std::fabs(p) + std::fabs(q) + std::fabs(r);
            if (x == 0.0) continue;
            p = p / x;
            q = q / x;
            r = r / x;
        }
        s = xpp_sign(std::sqrt(p * p + q * q + r * r), p);
        if (k != m)
            h[k - 1 + (k - 2) * n] = -s * x;
        else if (l != m)
            h[k - 1 + (k - 2) * n] = -h[k - 1 + (k - 2) * n];
        p = p + s;
        x = p / s;
        y = q / s;
        zz = r / s;
        q = q / p;
        r = r / p;
        for (j = k; j <= en; j++) {
            p = h[k - 1 + (j - 1) * n] + q * h[k + (j - 1) * n];
            if (notlas) {
                p = p + r * h[k + 1 + (j - 1) * n];
                h[k + 1 + (j - 1) * n] = h[k + 1 + (j - 1) * n] - p * zz;
            }
            h[k + (j - 1) * n] = h[k + (j - 1) * n] - p * y;
            h[k - 1 + (j - 1) * n] = h[k - 1 + (j - 1) * n] - p * x;
        }
        j = std::min(en, k + 3);
        for (i = l; i <= j; i++) {
            p = x * h[i - 1 + (k - 1) * n] + y * h[i - 1 + k * n];
            if (notlas) {
                p = p + zz * h[i - 1 + (k + 1) * n];
                h[i - 1 + (k + 1) * n] = h[i - 1 + (k + 1) * n] - p * r;
            }
            h[i - 1 + k * n] = h[i - 1 + k * n] - p * q;
            h[i - 1 + (k - 1) * n] = h[i - 1 + (k - 1) * n] - p;
        }
    }
    goto l70;
l270:
    ev[(en - 1) * 2] = x + t;
    ev[1 + (en - 1) * 2] = 0.0;
    en = na;
    goto l60;
l280:
    p = (y - x) / 2.0;
    q = p * p + w;
    zz = std::sqrt(std::fabs(q));
    x = x + t;
    if (q < 0.0) goto l320;
    zz = p + xpp_sign(zz, p);
    ev[(na - 1) * 2] = x + zz;
    ev[(en - 1) * 2] = ev[(na - 1) * 2];
    if (zz != 0.0) ev[(en - 1) * 2] = x - w / zz;
    ev[1 + (na - 1) * 2] = 0.0;
    ev[1 + (en - 1) * 2] = 0.0;
    goto l330;
l320:
    ev[(na - 1) * 2] = x + p;
    ev[(en - 1) * 2] = x + p;
    ev[1 + (na - 1) * 2] = zz;
    ev[1 + (en - 1) * 2] = -zz;
l330:
    en = enm2;
    goto l60;
l1000:
    *ierr = en;
}

} // namespace

void xpp_eigenvalues(int n, double *a, double *ev, double *work, int *ierr)
{
    orthes(n, 1, n, a, work);
    hqr(n, 1, n, a, ev, ierr);
}

/* ------------------------------------------------------------------ */
/* Special functions                                                   */

double xpp_bessel_j(double n, double x)
{
    return jn(static_cast<int>(n), x);
}

double xpp_bessel_y(double n, double x)
{
    return yn(static_cast<int>(n), x);
}

namespace {

/* I_0 and I_1 (Numerical Recipes' polynomial fits), times exp(-|x|) when
   scaled */
double bessel_i0(double x, bool scaled)
{
    const double ax = std::fabs(x);
    double y;
    if (ax < 3.75) {
        y = x / 3.75;
        y *= y;
        const double p = 1.0 + y * (3.5156229 + y * (3.0899424 + y * (1.2067492 + y * (0.2659732 + y * (0.360768e-1 + y * 0.45813e-2)))));
        return scaled ? p * std::exp(-ax) : p;
    }
    y = 3.75 / ax;
    return (scaled ? 1.0 / std::sqrt(ax) : std::exp(ax) / std::sqrt(ax)) *
           (0.39894228 + y * (0.1328592e-1 + y * (0.225319e-2 + y * (-0.157565e-2 + y * (0.916281e-2 + y * (-0.2057706e-1 + y * (0.2635537e-1 + y * (-0.1647633e-1 + y * 0.392377e-2))))))));
}

double bessel_i1(double x, bool scaled)
{
    const double ax = std::fabs(x);
    double y, ans;
    if (ax < 3.75) {
        y = x / 3.75;
        y *= y;
        ans = (scaled ? std::exp(-ax) * ax : ax) *
              (0.5 + y * (0.87890594 + y * (0.51498869 + y * (0.15084934 + y * (0.2658733e-1 + y * (0.301532e-2 + y * 0.32411e-3))))));
    } else {
        y = 3.75 / ax;
        ans = 0.2282967e-1 + y * (-0.2895312e-1 + y * (0.1787654e-1 - y * 0.420059e-2));
        ans = 0.39894228 + y * (-0.3988024e-1 + y * (-0.362018e-2 + y * (0.163801e-2 + y * (-0.1031555e-1 + y * ans))));
        ans *= scaled ? 1. / std::sqrt(ax) : std::exp(ax) / std::sqrt(ax);
    }
    return x < 0.0 ? -ans : ans;
}

/* I_n for n >= 2 by Miller's downward recurrence, normalised by I_0 */
double bessel_i(double nn, double x, bool scaled)
{
    constexpr double acc = 40.0, bigno = 1.0e10, bigni = 1.0e-10;
    const int n = static_cast<int>(nn);
    if (n == 0) return bessel_i0(x, scaled);
    if (n == 1) return bessel_i1(x, scaled);
    if (x == 0.0) return 0.0;
    const double tox = 2.0 / std::fabs(x);
    double bip = 0.0, ans = 0.0, bi = 1.0;
    for (int j = 2 * (n + static_cast<int>(std::sqrt(acc * n))); j > 0; j--) {
        const double bim = bip + j * tox * bi;
        bip = bi;
        bi = bim;
        if (std::fabs(bi) > bigno) {
            ans *= bigni;
            bi *= bigni;
            bip *= bigni;
        }
        if (j == n) ans = bip;
    }
    ans *= bessel_i0(x, scaled) / bi;
    return x < 0.0 && (n & 1) ? -ans : ans;
}

} // namespace

double xpp_bessel_i(double n, double x)
{
    return bessel_i(n, x, false);
}

double xpp_bessel_i_scaled(double n, double x)
{
    return bessel_i(n, x, true);
}
