/* W337 (#398): bytewise FNV-1a digests of every compared numerical output,
   workspace, determinant, status, diagnostic and continuity state.
   Constants were recorded from the pre-W337 code (e03d41d0).
   A deliberate numerical change must regenerate them and say so in its commit. */
#include "load_args.h"
#include "xpptest.h"
#include "xpp_math.h"
#include "session.h"
#include "auto_c.h"
#include "xpp_io.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "jacobian.h"
#include "expr_native.h"
#include <array>
#include <cstddef>
#include <span>
#include <vector>
#include <type_traits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace {
constexpr std::array expected{
    0xc4c5d0baa047199fULL, // GE pivot, two RHS: error, RHS, solution, determinant, fort.9
    0xac6207bd79708ce2ULL, // GE pivot, determinant only: error, RHS, solution, determinant, fort.9
    0x165587c4679f1271ULL, // GE singular, determinant only (its NaN folded as one value): error, RHS, solution, determinant, fort.9
    0xc6e9ae0eee1df6e9ULL, // GE singular solve: error, RHS, solution, determinant, fort.9
    0x44024f41db685dd8ULL, // NLVC nullity two: status, matrix, null vector, fort.9
    0xf81615fb6d07755dULL, // NLVC tiny pivot: status, matrix, null vector, fort.9
    0x7f661e0e57524baaULL, // RG pivot, values only: status, padded matrix, roots, vectors, work, pivots
    0x349c19e0f66f3064ULL, // RG singular, values only: status, padded matrix, roots, vectors, work, pivots
    0x0bbde7244b2fc9f8ULL, // RG conjugate, values only: status, padded matrix, roots, vectors, work, pivots
    0x9cd341a44aa97cf5ULL, // RG dense, values only: status, padded matrix, roots, vectors, work, pivots
    0x0f4ce6ca9a50b4f9ULL, // RG pivot, eigenvectors: status, padded matrix, roots, vectors, work, pivots
    0x67a5b946706c2655ULL, // RG singular, eigenvectors: status, padded matrix, roots, vectors, work, pivots
    0x709dfb16ffaddb58ULL, // RG conjugate, eigenvectors: status, padded matrix, roots, vectors, work, pivots
    0x1168e9a8a711f240ULL, // RG dense, eigenvectors: status, padded matrix, roots, vectors, work, pivots
    0xe30e08c5a86e21a5ULL, // SVD pivot: status, matrix, singular values, off-diagonal, U, V, work
    0x0b08be3354f11dbfULL, // SVD singular: status, matrix, singular values, off-diagonal, U, V, work
    0x175dbf80968a3629ULL, // SVD dense: status, matrix, singular values, off-diagonal, U, V, work
    0x2e86b7c46c18ce9cULL, // Floquet conjugate pair: status, pencil, work, multipliers, fort.9
    0x796d55cf896e1128ULL, // Floquet deflation: status, pencil, work, multipliers, fort.9
    0x9b121411143fd27eULL, // Floquet infinite multiplier: status, pencil, work, multipliers, fort.9
    0x75d8add69a35b7ecULL, // QZ stages: statuses, error, pencil, vectors, alpha real/imaginary, beta
    0x60e747649c920116ULL, // SVD rotations, sig22/sigmin, epslon, complex division: statuses and outputs
    0x656ad85266cca367ULL, // BLAS strides/transposes: norms, dot, pivot, statuses, vectors, matrices
    0xe93b1d3f1a696b09ULL, // Schur conjugate, stable: work, status, vectors, roots, types
    0xdd349a10d75201e4ULL, // Schur dense, stable: work, status, vectors, roots, types
    0x0e63480b71848686ULL, // Schur conjugate, unstable: work, status, vectors, roots, types
    0xa13b817f19765a9eULL, // Schur dense, unstable: work, status, vectors, roots, types
};

constexpr integer n = 3; // Small matrices allow every overwritten element to be compared.

constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL; // FNV-1a's specified 64-bit seed.
constexpr std::uint64_t fnv_prime = 1099511628211ULL; // FNV-1a's specified 64-bit prime.

// IEEE 754 leaves a NaN's sign unspecified: 0/0 is -nan on x86 and nan on
// ARM. Every NaN folds as one value so the digests are the same on every CPU
// (macOS CI on ARM failed GE singular otherwise).
template <class T> void fold(std::uint64_t &digest, const T &value)
{
    if constexpr (std::is_floating_point_v<T>) {
        const T canonical = std::isnan(value) ? std::numeric_limits<T>::quiet_NaN() : value;
        for (const auto byte : std::as_bytes(std::span{&canonical, 1}))
            digest = (digest ^ std::to_integer<unsigned char>(byte)) * fnv_prime;
    } else if constexpr (std::is_arithmetic_v<T>) {
        for (const auto byte : std::as_bytes(std::span{&value, 1}))
            digest = (digest ^ std::to_integer<unsigned char>(byte)) * fnv_prime;
    } else {
        for (const auto &element : value) fold(digest, element);
    }
}

std::string contents(FILE *file)
{
    std::rewind(file);
    std::string text;
    for (int c; (c = std::fgetc(file)) != EOF;) text += static_cast<char>(c);
    return text;
}

template <class F> std::string failure(F call)
{
    try { call(); }
    catch (const xpp::AutoFailed &e) { return e.error.where + ":" + e.error.what; }
    return {};
}

std::uint64_t solve(std::vector<double> input, integer rhs, bool expect_failure = false)
{
    std::uint64_t digest = fnv_offset_basis;
    auto a = input;
    std::array<double, 6> f{.3, -2., 4., 1., 0., -3.}, u{};
    double det = 0.;
    xpp::UniqueFile log(std::tmpfile());
    CHECK(log);
    if (!log) return digest;
    const auto error = failure([&] { xpp::ge(log.get(), xpp::auto_ge_pivot_iid, n, n, a.data(), rhs, n, u.data(), n, f.data(), &det); });
    // W498 adds the pivot diagnostic; keep the numerical digest and verify the new detail separately.
    if(expect_failure)CHECK(error.ends_with(" (smallest pivot 0 < 1e-30)"));
    fold(digest, error.substr(0,error.find(" (smallest pivot")));
    CHECK(!error.empty() == expect_failure);
    /* W491: GE's matrix after the call is its scratch (rows and columns
       swapped in place), no output; the digests without it were checked
       equal to the pre-W491 GE's */
    fold(digest, f); fold(digest, u); fold(digest, det);
    fold(digest, contents(log.get()));
    return digest;
}

std::uint64_t null_vector(std::vector<double> a, integer nullity)
{
    std::uint64_t digest = fnv_offset_basis;
    std::array<double, n> u{};
    xpp::UniqueFile log(std::tmpfile());
    CHECK(log);
    if (!log) return digest;
    fold(digest, xpp::nlvc(log.get(), n, n, nullity, a.data(), u.data()));
    fold(digest, a); fold(digest, u);
    fold(digest, contents(log.get()));
    return digest;
}

std::uint64_t roots(std::vector<double> input, integer vectors)
{
    std::uint64_t digest = fnv_offset_basis;
    /* Padded leading dimension exercises the interface gear's solver lacks. */
    constexpr integer ld = n + 1;
    std::array<double, ld*n> a{}, z{};
    for (integer j = 0; j < n; ++j)
        for (integer i = 0; i < n; ++i) a[i+j*ld] = input[i+j*n];
    std::array<double, n> wr{}, wi{}, work{};
    std::array<integer, n> piv{};
    integer status = -1;
    xpp::rg(ld, n, a.data(), wr.data(), wi.data(), vectors, z.data(), piv.data(), work.data(), &status);
    CHECK(status == 0); fold(digest, status);
    fold(digest, a); fold(digest, wr); fold(digest, wi); fold(digest, z); fold(digest, work);
    fold(digest, piv);
    if (vectors) {
        /* EISPACK packs a conjugate pair as consecutive real/imaginary columns. */
        for (integer j = 0; j < n; ++j) {
            if (wi[j] < 0.) continue;
            for (integer i = 0; i < n; ++i) {
                double real = 0., imag = 0.;
                for (integer k = 0; k < n; ++k) {
                    real += input[i+k*n] * z[k+j*ld];
                    if (wi[j] > 0.) imag += input[i+k*n] * z[k+(j+1)*ld];
                }
                const double zr = z[i+j*ld], zi = wi[j] > 0. ? z[i+(j+1)*ld] : 0.;
                CHECK(std::abs(real - (wr[j]*zr - wi[j]*zi)) < 1e-10);
                CHECK(std::abs(imag - (wi[j]*zr + wr[j]*zi)) < 1e-10);
            }
        }
    }
    return digest;
}

std::uint64_t svd(std::vector<double> a)
{
    std::uint64_t digest = fnv_offset_basis;
    std::array<double, n+1> s{}, e{}, work{};
    std::array<double, n*n> u{}, v{};
    integer ld = n, rows = n, cols = n, job = 11, status = -1;
    double tol = 1e-14; // SVD's documented relative tolerance above machine precision.
    xpp::ezsvd(a.data(), &ld, &rows, &cols, s.data(), e.data(), u.data(), &ld, v.data(), &ld, work.data(), &job, &status, &tol);
    CHECK(status == 0); fold(digest, status);
    fold(digest, a); fold(digest, s); fold(digest, e); fold(digest, u); fold(digest, v); fold(digest, work);
    return digest;
}

std::uint64_t floquet(bool deflate, bool infinite)
{
    std::uint64_t digest = fnv_offset_basis;
    /* A real multiplier plus a conjugate pair; first multiplier is exactly 1
       when deflated, as for a periodic orbit. A singular pencil also checks
       the infinite-multiplier representation and its diagnostic. */
    std::array<double, n*n> a{1.,0.,0., 0.,.5,-.25, 0.,.25,.5};
    std::array<double, n*n> b{-1.,0.,0., 0.,-1.,0., 0.,0.,-1.};
    if (infinite) b[0] = 0.;
    std::array<double, n*n> work{};
    std::array<doublecomplex, n> ev{};
    integer dim = n, diagnostic = 4;
    xpp::UniqueFile log(std::tmpfile());
    CHECK(log);
    if (!log) return digest;
    fold(digest, xpp::flowkm(log.get(), &dim, a.data(), b.data(), &diagnostic, work.data(), ev.data(), deflate));
    fold(digest, a); fold(digest, b); fold(digest, work);
    for (integer i = 0; i < n; ++i) {
        fold(digest, ev[i].r); fold(digest, ev[i].i);
    }
    fold(digest, contents(log.get()));
    return digest;
}

std::uint64_t schur(std::vector<double> a, integer manifold)
{
    std::uint64_t digest = fnv_offset_basis;
    std::array<double, n> ort{}, er{}, ei{};
    std::array<double, n*n> v{};
    std::array<integer, n> type{};
    integer dim = n, low = 1;
    double eps = 1e-13; // HomCont's component-zero tolerance.
    xpp::orthes(&dim, &dim, &low, &dim, a.data(), ort.data());
    fold(digest, a); fold(digest, ort);
    xpp::ortran(&dim, &dim, &low, &dim, a.data(), ort.data(), v.data());
    fold(digest, v);
    for (integer i = 2; i < n; ++i)
        for (integer j = 0; j < i-1; ++j) a[i+j*n] = 0.;
    fold(digest, xpp::hqr3lc(a.data(), v.data(), &dim, &low, &dim, &eps, er.data(), ei.data(), type.data(), &dim, &dim, &manifold));
    fold(digest, a); fold(digest, v); fold(digest, er); fold(digest, ei); fold(digest, type);
    return digest;
}

std::uint64_t qz_vectors()
{
    std::uint64_t digest = fnv_offset_basis;
    std::array<double,n*n> a{.25,-2.,3., 4.,.5,-1., -3.,2.,.75};
    std::array<double,n*n> b{3.,0.,0., .25,2.,0., -.5,.75,1.};
    std::array<double,n*n> z{};
    std::array<double,n> ar{}, ai{}, beta{};
    integer error=-1;
    fold(digest, xpp::qzhes(n,n,a.data(),b.data(),1,z.data()));
    fold(digest, a); fold(digest, b); fold(digest, z);
    fold(digest, xpp::qzit(n,n,a.data(),b.data(),0.,1,z.data(),&error));
    CHECK(error == 0); fold(digest, error);
    fold(digest, a); fold(digest, b); fold(digest, z);
    fold(digest, xpp::qzval(n,n,a.data(),b.data(),ar.data(),ai.data(),beta.data(),1,z.data()));
    fold(digest, a); fold(digest, b); fold(digest, z);
    fold(digest, ar); fold(digest, ai); fold(digest, beta);
    return digest;
}

std::uint64_t scalar_helpers()
{
    std::uint64_t digest = fnv_offset_basis;
    /* Cover the SVD rotations independently, including zero and disparate
       scales, so a shortcut cannot hide behind composite eigenvalue tests. */
    for (double f : {0.,-0.,1e-100,-3.,1e100}) for (double g : {0.,2.,-1e-100,1e100}) {
        double c=0., s=0.;
        fold(digest, xpp::ndrotg(&f,&g,&c,&s));
        fold(digest, f); fold(digest, g); fold(digest, c); fold(digest, s);
        fold(digest, xpp::sndrtg(&f,&g,&c,&s));
        fold(digest, c); fold(digest, s);
        double h=.75;
        std::array<double,6> out{};
        fold(digest, xpp::sig22(&f,&g,&h,&out[0],&out[1],&out[2],&out[3],&out[4],&out[5]));
        fold(digest, out);
        const double small=xpp::sigmin(&f,&g,&h);
        fold(digest, small);
        const double eps=xpp::epslon(f);
        fold(digest, eps);
    }
    double ar=1e200, ai=-3e199, br=2e200, bi=7e199, cr=0., ci=0.;
    xpp::cdiv(&ar,&ai,&br,&bi,&cr,&ci);
    fold(digest, cr); fold(digest, ci);
    return digest;
}

std::uint64_t blas_helpers()
{
    std::uint64_t digest = fnv_offset_basis;
    for (integer stride : {1,2}) { // Legacy dnrm2/dscal require positive increments.
        integer count=n;
        std::array<double,9> x{1.,-4.,.5,2.,-3.,.25,7.,1.,-2.}, y=x;
        const double norm=xpp::dnrm2(&count,x.data(),&stride);
        fold(digest, norm);
        const double dot=xpp::ddot(&count,x.data(),&stride,y.data(),&stride);
        fold(digest, dot);
        fold(digest, xpp::idamax(&count,x.data(),&stride));
        double scale=-.3, c=.6, s=.8;
        fold(digest, xpp::dscal(&count,&scale,x.data(),&stride));
        fold(digest, x);
        fold(digest, xpp::daxpy(&count,&scale,x.data(),&stride,y.data(),&stride));
        fold(digest, y);
        fold(digest, xpp::drot(&count,x.data(),&stride,y.data(),&stride,&c,&s));
        fold(digest, x); fold(digest, y);
        fold(digest, xpp::dswap(&count,x.data(),&stride,y.data(),&stride));
        fold(digest, x); fold(digest, y);
    }
    for (const char *ta : {"n","T"}) for (const char *tb : {"N","t"}) {
        integer dim=n;
        std::array<double,n*n> a{1.,-4.,.5,2.,-3.,.25,7.,1.,-2.}, b=a, c=a;
        double alpha=.3, beta=-.7;
        fold(digest, xpp::dgemm(ta,tb,&dim,&dim,&dim,&alpha,a.data(),&dim,b.data(),&dim,&beta,c.data(),&dim,1,1));
        fold(digest, c);
        for (logical transpose : {0,1}) {
            fold(digest, xpp::dgemc(&dim,&dim,a.data(),&dim,c.data(),&dim,&transpose));
            fold(digest, c);
        }
        fold(digest, xpp::lsame(ta,tb,1,1));
    }
    return digest;
}

void homcont()
{
    xpp::TempDir temp;
    std::string path = temp.path() + "/numerics.odex";
    auto writer = xpp::Writer::binary(path);
    CHECK(writer.write("par p=0.2,s=0\nx'=(1+s)*x+p*y\ny'=(-1+s)*y-2*z\nz'=2*y+(-1+s)*z\n") && writer.commit());
    char name[] = "test_auto_numerics";
    char *argv[] = {name, path.data(), nullptr};
    const auto loaded = load_args(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) return;
    auto &session = xpp::client_session();
    CHECK(xpp::ensure_exact_jacobian(session));
    session.numerics.jacobian = xpp::JACOBIAN_EXACT;
    auto &lib = session.auto_lib;
    lib.session = &session;
    lib.parameter_count = session.auto_state.parameter_layout.size();
    iap_type iap{};
    iap.lib = &lib; iap.ndim = n; iap.ndm = n; iap.nfpr = 2;
    std::array<integer, 2> icp{session.auto_state.parameter_layout.solver_index(0),
                               session.auto_state.parameter_layout.solver_index(1)};
    std::vector<double> par(lib.parameter_count), dfdp(n*lib.parameter_count);
    std::array<double,n> equilibrium{}, rr{}, ri{};
    std::array<double,n*n> a{}, zz{}, vectors{}, bound{};
    xpp::UniqueFile log(std::tmpfile());
    CHECK(log);
    if (!log) return;
    lib.fp9 = log.get();
    integer dim = n;
    constexpr double tolerance = 1e-12; // Analytic invariant-subspace identities on a small, well-conditioned matrix.
    for (bool reverse : {false,true}) for (double shift : {0.,2.}) {
        par[xpp::auto_unknown::period] = reverse ? -1. : 1.;
        par[icp[1]] = shift;
        lib.homcont.blhom.nstab = reverse ? 1 : 2;
        lib.homcont.blhom.nunstab = reverse ? 2 : 1;
        lib.homcont.blhme.ieigc.assign(2,0);
        lib.homcont.blhme.vrprev.assign(2*n*n,0.);
        lib.homcont.beyn.iflag.assign(4,0);
        lib.homcont.beyn.cprev.assign(4*n*n,0.);
        for (bool transpose : {true,false}) for (auto endpoint : {xpp::HomcontEndpoint::left,xpp::HomcontEndpoint::right}) {
            // Actual boundary/adjoint calls use one manifold per endpoint/transpose slot.
            const bool stable = (endpoint == xpp::HomcontEndpoint::left) == transpose;
            const auto manifold = stable ? xpp::HomcontManifold::stable : xpp::HomcontManifold::unstable;
            const integer count = stable ? lib.homcont.blhom.nstab : lib.homcont.blhom.nunstab;
            const integer first = stable ? 0 : n-count;
            const bool pair = stable != reverse;
            for (double p : {.2,.3}) {
                par[icp[0]] = p;
                CHECK(xpp::prjctn(&iap,bound.data(),equilibrium.data(),icp.data(),par.data(),manifold,endpoint,transpose,n,a.data(),dfdp.data(),true) == 0);
                for (integer row=first; row<first+count; ++row) {
                    const double x=bound[row], y=bound[row+n], z=bound[row+2*n];
                    CHECK(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
                    CHECK(x*x+y*y+z*z > .1);
                    if (pair) CHECK(std::abs(transpose ? x : x+p*(y-z)/4.) < tolerance);
                    else if (transpose) {
                        CHECK(std::abs(y-p*x/4.) < tolerance);
                        CHECK(std::abs(z+p*x/4.) < tolerance);
                    } else CHECK(std::abs(y)+std::abs(z) < tolerance);
                }
                if (count == 2) {
                    double aa=0., bb=0., ab=0.;
                    for (integer col=0; col<n; ++col) {
                        const double a0=bound[first+col*n], b0=bound[first+1+col*n];
                        aa+=a0*a0; bb+=b0*b0; ab+=a0*b0;
                    }
                    CHECK(aa*bb-ab*ab > .1); // The two boundary rows span a plane.
                }
                const auto saved = lib.homcont.beyn.cprev;
                const auto flags = lib.homcont.beyn.iflag;
                const auto original = bound;
                par[icp[0]] += auto_hmach;
                CHECK(xpp::prjctn(&iap,bound.data(),equilibrium.data(),icp.data(),par.data(),manifold,endpoint,transpose,n,a.data(),dfdp.data(),false) == 0);
                CHECK(lib.homcont.beyn.cprev == saved && lib.homcont.beyn.iflag == flags);
                par[icp[0]] = p;
                CHECK(xpp::prjctn(&iap,bound.data(),equilibrium.data(),icp.data(),par.data(),manifold,endpoint,transpose,n,a.data(),dfdp.data(),false) == 0);
                for (integer row=first; row<first+count; ++row)
                    for (integer col=0; col<n; ++col) CHECK(std::abs(bound[row+col*n]-original[row+col*n]) < tolerance);
            }
        }
        // The real root has a unit eigenvector; the complex pair's shared
        // real part is orthogonal to it in the uncoupled block (p=0).
        par[icp[0]] = 0.;
        for (integer transpose : {1,2}) {
            integer endpoint = 1;
            CHECK(xpp::eigho(&iap,&endpoint,&transpose,rr.data(),ri.data(),vectors.data(),equilibrium.data(),icp.data(),par.data(),&dim,a.data(),dfdp.data(),zz.data()) == 0);
            CHECK(rr[0] <= rr[1] && rr[1] <= rr[2]);
            const integer real = reverse ? 0 : 2;
            CHECK(std::abs(vectors[real]-1.) < tolerance);
            CHECK(std::abs(rr[real]-(reverse ? -1.-shift : 1.+shift)) < tolerance);
            // The pair's unit vector (0,1,-+i)/sqrt(2), turned so that its largest
            // component is real (LAPACK DGEEV): a real part of length 1/sqrt(2) on an axis.
            for (integer row : {(real+1)%n, (real+2)%n}) {
                const double x=vectors[row], y=vectors[row+n], z=vectors[row+2*n];
                CHECK(std::abs(x) < tolerance);
                CHECK(std::abs(std::max(std::abs(y),std::abs(z))-std::sqrt(.5)) < tolerance);
                CHECK(std::abs(std::min(std::abs(y),std::abs(z))) < tolerance);
            }
        }
    }
    // Retain the independent root adapter and invalid-leading-dimension checks.
    for (integer leading : {n,n-1}) {
        a = {2.,0.,0., 0.,-1.,2., 0.,-2.,-1.};
        std::array<doublecomplex,n> ev{};
        integer error = -1;
        const auto message = failure([&] { xpp::eig(&iap,&dim,&leading,a.data(),ev.data(),&error); });
        CHECK((leading == n) == message.empty());
        CHECK(error == (leading == n ? 0 : 1));
        if (leading == n) {
            bool real_found=false, positive_found=false, negative_found=false;
            for (const auto value : ev) {
                if (std::abs(value.r-2.) < tolerance && std::abs(value.i) < tolerance) real_found=true;
                if (std::abs(value.r+1.) < tolerance && std::abs(value.i-2.) < tolerance) positive_found=true;
                if (std::abs(value.r+1.) < tolerance && std::abs(value.i+2.) < tolerance) negative_found=true;
            }
            CHECK(real_found && positive_found && negative_found);
        }
    }
    // The phase row has exact linear derivatives in states and zero parameter derivatives.
    lib.homcont.blhom.nrev = 0;
    lib.homcont.blhom.itwist = 0;
    iap.nint = 1;
    rap_type rap{};
    const std::array<double,n> old{1.,2.,3.}, state{1.5,1.,5.}, tangent{.25,-.5,2.};
    std::array<double,1> phase{};
    std::vector<double> derivative(n+lib.parameter_count, 99.);
    CHECK(icho(&iap,&rap,n,par.data(),icp.data(),1,state.data(),old.data(),tangent.data(),tangent.data(),phase.data(),1,derivative.data()) == 0);
    CHECK(phase[0] == 4.625);
    for (integer column=0; column<n+lib.parameter_count; ++column)
        CHECK(derivative[column] == (column<n ? tangent[column] : 0.));
    lib.fp9 = nullptr;
}
} // namespace

void check_ge_verbosity()
{
    // IID changes only diagnostics, including a pivot smaller than RSMALL.
    std::array<double, 4> quiet_matrix{}, quiet_rhs{}, quiet_solution{};
    double quiet_det = 0;
    for (integer iid : {0, 1, 3, 4}) {
        std::array<double, 4> matrix{2e-32, 0, 0, 1e-32}, rhs{1, 2}, solution{};
        double det = 0;
        xpp::UniqueFile log(std::tmpfile());
        CHECK(log);
        if (!log) continue;
        xpp::ge(log.get(), iid, 2, 2, matrix.data(), 1, 2, solution.data(), 2, rhs.data(), &det);
        const auto text = contents(log.get());
        CHECK((text.find("Pivots in GE") != std::string::npos) == (iid >= xpp::auto_ge_pivot_iid));
        CHECK((text.find("NOTE:Pivot") != std::string::npos) == (iid >= xpp::auto_ge_note_iid));
        if (iid == 0) {
            CHECK(text.empty());
            quiet_matrix = matrix; quiet_rhs = rhs; quiet_solution = solution; quiet_det = det;
        } else CHECK(matrix == quiet_matrix && rhs == quiet_rhs && solution == quiet_solution && det == quiet_det);
    }
}

int main()
{
    check_ge_verbosity();
    size_t case_index = 0;
    /* Large off-diagonal and tied candidates distinguish complete from partial
       pivoting and pin AUTO's row/column tie order. */
    const std::vector<double> pivot{1e-20,2.,-2., 100.,3.,1., -100.,1.,4.};
    const std::vector<double> singular{1.,2.,3., 2.,4.,6., 0.,0.,0.};
    const std::vector<double> conjugate{2.,0.,0., 0.,-1.,2., 0.,-2.,-1.};
    const std::vector<double> dense{.25,-2.,3., 4.,.5,-1., -3.,2.,.75};
    CHECK(solve(pivot,2) == expected[case_index++]);
    CHECK(solve(pivot,0) == expected[case_index++]);
    CHECK(solve(singular,0) == expected[case_index++]);
    CHECK(solve({1.,2.,3., 2.,4.,6., 0.,1.,1.},1,true) == expected[case_index++]);
    CHECK(null_vector(singular,2) == expected[case_index++]);
    CHECK(null_vector({1.,0.,0., 0.,1e-32,0., 0.,0.,0.},1) == expected[case_index++]); // NLVC's tiny-pivot diagnostic.
    for (integer vectors : {0,1})
        for (const auto &matrix : {pivot,singular,conjugate,dense})
            CHECK(roots(matrix,vectors) == expected[case_index++]);
    CHECK(svd(pivot) == expected[case_index++]);
    CHECK(svd(singular) == expected[case_index++]);
    CHECK(svd(dense) == expected[case_index++]);
    CHECK(floquet(false,false) == expected[case_index++]);
    CHECK(floquet(true,false) == expected[case_index++]);
    CHECK(floquet(false,true) == expected[case_index++]);
    CHECK(qz_vectors() == expected[case_index++]);
    CHECK(scalar_helpers() == expected[case_index++]);
    CHECK(blas_helpers() == expected[case_index++]);
    for (integer manifold : {-1,1}) {
        CHECK(schur(conjugate,manifold) == expected[case_index++]);
        CHECK(schur(dense,manifold) == expected[case_index++]);
    }
    homcont();
    CHECK(case_index == expected.size());
    TEST_REPORT("auto_numerics");
}
