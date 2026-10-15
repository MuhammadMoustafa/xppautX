/* W337 (#398): compare the numerical owner with frozen pre-move algorithms,
   including overwritten workspaces, determinants, diagnostics and continuity.
   The oracle is confined to this test, not an alternative production path. */
#include "xpptest.h"
#include "auto_numerics_reference.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "jacobian.h"
#include "expr_native.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>

namespace {
constexpr integer n = 3; // Small matrices allow every overwritten element to be compared.

void same(std::span<const double> a, std::span<const double> b)
{
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        CHECK(std::bit_cast<std::uint64_t>(a[i]) == std::bit_cast<std::uint64_t>(b[i]));
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

void solve(std::vector<double> input, integer rhs, bool expect_failure = false)
{
    auto a = input, b = input;
    std::array<double, 6> f{.3, -2., 4., 1., 0., -3.}, g = f, u{}, v{};
    double det = 0., old_det = 0.;
    xpp::UniqueFile log(std::tmpfile()), old_log(std::tmpfile());
    CHECK(log && old_log);
    if (!log || !old_log) return;
    const auto error = failure([&] { xpp::ge(log.get(), n, n, a.data(), rhs, n, u.data(), n, f.data(), &det); });
    const auto old_error = failure([&] { auto_numerics_reference::ge(old_log.get(), n, n, b.data(), rhs, n, v.data(), n, g.data(), &old_det); });
    CHECK(error == old_error);
    CHECK(!error.empty() == expect_failure);
    same(a, b); same(f, g); same(u, v); same({&det, 1}, {&old_det, 1});
    CHECK(contents(log.get()) == contents(old_log.get()));
}

void null_vector(std::vector<double> a, integer nullity)
{
    auto b = a;
    std::array<double, n> u{}, v{};
    xpp::UniqueFile log(std::tmpfile()), old_log(std::tmpfile());
    CHECK(log && old_log);
    if (!log || !old_log) return;
    CHECK(xpp::nlvc(log.get(), n, n, nullity, a.data(), u.data()) ==
          auto_numerics_reference::nlvc(old_log.get(), n, n, nullity, b.data(), v.data()));
    same(a, b); same(u, v);
    CHECK(contents(log.get()) == contents(old_log.get()));
}

void roots(std::vector<double> input, integer vectors)
{
    /* Padded leading dimension exercises the interface gear's solver lacks. */
    constexpr integer ld = n + 1;
    std::array<double, ld*n> a{}, b{}, z{}, old_z{};
    for (integer j = 0; j < n; ++j)
        for (integer i = 0; i < n; ++i) a[i+j*ld] = input[i+j*n];
    b = a;
    std::array<double, n> wr{}, wi{}, old_wr{}, old_wi{}, work{}, old_work{};
    std::array<integer, n> piv{}, old_piv{};
    integer status = -1, old_status = -1;
    xpp::rg(ld, n, a.data(), wr.data(), wi.data(), vectors, z.data(), piv.data(), work.data(), &status);
    auto_numerics_reference::rg(ld, n, b.data(), old_wr.data(), old_wi.data(), vectors, old_z.data(), old_piv.data(), old_work.data(), &old_status);
    CHECK(status == 0 && status == old_status);
    same(a, b); same(wr, old_wr); same(wi, old_wi); same(z, old_z); same(work, old_work);
    CHECK(piv == old_piv);
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
}

void svd(std::vector<double> a)
{
    auto b = a;
    std::array<double, n+1> s{}, e{}, work{}, old_s{}, old_e{}, old_work{};
    std::array<double, n*n> u{}, v{}, old_u{}, old_v{};
    integer ld = n, rows = n, cols = n, job = 11, status = -1, old_status = -1;
    double tol = 1e-14; // SVD's documented relative tolerance above machine precision.
    xpp::ezsvd(a.data(), &ld, &rows, &cols, s.data(), e.data(), u.data(), &ld, v.data(), &ld, work.data(), &job, &status, &tol);
    auto_numerics_reference::ezsvd(b.data(), &ld, &rows, &cols, old_s.data(), old_e.data(), old_u.data(), &ld, old_v.data(), &ld, old_work.data(), &job, &old_status, &tol);
    CHECK(status == 0 && status == old_status);
    same(a, b); same(s, old_s); same(e, old_e); same(u, old_u); same(v, old_v); same(work, old_work);
}

void floquet(bool deflate, bool infinite)
{
    /* A real multiplier plus a conjugate pair; first multiplier is exactly 1
       when deflated, as for a periodic orbit. A singular pencil also checks
       the infinite-multiplier representation and its diagnostic. */
    std::array<double, n*n> a{1.,0.,0., 0.,.5,-.25, 0.,.25,.5};
    std::array<double, n*n> b{-1.,0.,0., 0.,-1.,0., 0.,0.,-1.};
    if (infinite) b[0] = 0.;
    auto old_a = a, old_b = b;
    std::array<double, n*n> work{}, old_work{};
    std::array<doublecomplex, n> ev{}, old_ev{};
    integer dim = n, diagnostic = 4;
    xpp::UniqueFile log(std::tmpfile()), old_log(std::tmpfile());
    CHECK(log && old_log);
    if (!log || !old_log) return;
    CHECK(xpp::flowkm(log.get(), &dim, a.data(), b.data(), &diagnostic, work.data(), ev.data(), deflate) ==
          auto_numerics_reference::flowkm(old_log.get(), &dim, old_a.data(), old_b.data(), &diagnostic, old_work.data(), old_ev.data(), deflate));
    same(a, old_a); same(b, old_b); same(work, old_work);
    for (integer i = 0; i < n; ++i) {
        same({&ev[i].r, 1}, {&old_ev[i].r, 1}); same({&ev[i].i, 1}, {&old_ev[i].i, 1});
    }
    CHECK(contents(log.get()) == contents(old_log.get()));
}

void schur(std::vector<double> a, integer manifold)
{
    auto b = a;
    std::array<double, n> ort{}, old_ort{}, er{}, ei{}, old_er{}, old_ei{};
    std::array<double, n*n> v{}, old_v{};
    std::array<integer, n> type{}, old_type{};
    integer dim = n, low = 1;
    double eps = 1e-13; // HomCont's component-zero tolerance.
    xpp::orthes(&dim, &dim, &low, &dim, a.data(), ort.data());
    auto_numerics_reference::orthes(&dim, &dim, &low, &dim, b.data(), old_ort.data());
    same(a,b); same(ort,old_ort);
    xpp::ortran(&dim, &dim, &low, &dim, a.data(), ort.data(), v.data());
    auto_numerics_reference::ortran(&dim, &dim, &low, &dim, b.data(), old_ort.data(), old_v.data());
    same(v,old_v);
    for (integer i = 2; i < n; ++i)
        for (integer j = 0; j < i-1; ++j) a[i+j*n] = b[i+j*n] = 0.;
    CHECK(xpp::hqr3lc(a.data(), v.data(), &dim, &low, &dim, &eps, er.data(), ei.data(), type.data(), &dim, &dim, &manifold) ==
          auto_numerics_reference::hqr3lc(b.data(), old_v.data(), &dim, &low, &dim, &eps, old_er.data(), old_ei.data(), old_type.data(), &dim, &dim, &manifold));
    same(a,b); same(v,old_v); same(er,old_er); same(ei,old_ei); CHECK(type == old_type);
}

void qz_vectors()
{
    std::array<double,n*n> a{.25,-2.,3., 4.,.5,-1., -3.,2.,.75};
    std::array<double,n*n> b{3.,0.,0., .25,2.,0., -.5,.75,1.};
    auto old_a=a, old_b=b;
    std::array<double,n*n> z{}, old_z{};
    std::array<double,n> ar{}, ai{}, beta{}, old_ar{}, old_ai{}, old_beta{};
    integer error=-1, old_error=-1;
    CHECK(xpp::qzhes(n,n,a.data(),b.data(),1,z.data()) ==
          auto_numerics_reference::qzhes(n,n,old_a.data(),old_b.data(),1,old_z.data()));
    same(a,old_a); same(b,old_b); same(z,old_z);
    CHECK(xpp::qzit(n,n,a.data(),b.data(),0.,1,z.data(),&error) ==
          auto_numerics_reference::qzit(n,n,old_a.data(),old_b.data(),0.,1,old_z.data(),&old_error));
    CHECK(error == 0 && error == old_error);
    same(a,old_a); same(b,old_b); same(z,old_z);
    CHECK(xpp::qzval(n,n,a.data(),b.data(),ar.data(),ai.data(),beta.data(),1,z.data()) ==
          auto_numerics_reference::qzval(n,n,old_a.data(),old_b.data(),old_ar.data(),old_ai.data(),old_beta.data(),1,old_z.data()));
    same(a,old_a); same(b,old_b); same(z,old_z);
    same(ar,old_ar); same(ai,old_ai); same(beta,old_beta);
}

void scalar_helpers()
{
    /* Cover the SVD rotations independently, including zero and disparate
       scales, so a shortcut cannot hide behind composite eigenvalue tests. */
    for (double f : {0.,-0.,1e-100,-3.,1e100}) for (double g : {0.,2.,-1e-100,1e100}) {
        double old_f=f, old_g=g, c=0., s=0., old_c=0., old_s=0.;
        CHECK(xpp::ndrotg(&f,&g,&c,&s) == auto_numerics_reference::ndrotg(&old_f,&old_g,&old_c,&old_s));
        same({&f,1},{&old_f,1}); same({&g,1},{&old_g,1}); same({&c,1},{&old_c,1}); same({&s,1},{&old_s,1});
        CHECK(xpp::sndrtg(&f,&g,&c,&s) == auto_numerics_reference::sndrtg(&old_f,&old_g,&old_c,&old_s));
        same({&c,1},{&old_c,1}); same({&s,1},{&old_s,1});
        double h=.75, old_h=h;
        std::array<double,6> out{}, old_out{};
        CHECK(xpp::sig22(&f,&g,&h,&out[0],&out[1],&out[2],&out[3],&out[4],&out[5]) ==
              auto_numerics_reference::sig22(&old_f,&old_g,&old_h,&old_out[0],&old_out[1],&old_out[2],&old_out[3],&old_out[4],&old_out[5]));
        same(out,old_out);
        const double small=xpp::sigmin(&f,&g,&h), old_small=auto_numerics_reference::sigmin(&old_f,&old_g,&old_h);
        same({&small,1},{&old_small,1});
        const double eps=xpp::epslon(f), old_eps=auto_numerics_reference::epslon(old_f);
        same({&eps,1},{&old_eps,1});
    }
    double ar=1e200, ai=-3e199, br=2e200, bi=7e199, cr=0., ci=0., old_cr=0., old_ci=0.;
    xpp::cdiv(&ar,&ai,&br,&bi,&cr,&ci);
    auto_numerics_reference::cdiv(&ar,&ai,&br,&bi,&old_cr,&old_ci);
    same({&cr,1},{&old_cr,1}); same({&ci,1},{&old_ci,1});
}

void blas_helpers()
{
    for (integer stride : {1,2}) { // Legacy dnrm2/dscal require positive increments.
        integer count=n;
        std::array<double,9> x{1.,-4.,.5,2.,-3.,.25,7.,1.,-2.}, y=x;
        std::array<double,9> old_x=x, old_y=y;
        const double norm=xpp::dnrm2(&count,x.data(),&stride), old_norm=auto_numerics_reference::dnrm2(&count,old_x.data(),&stride);
        same({&norm,1},{&old_norm,1});
        const double dot=xpp::ddot(&count,x.data(),&stride,y.data(),&stride), old_dot=auto_numerics_reference::ddot(&count,old_x.data(),&stride,old_y.data(),&stride);
        same({&dot,1},{&old_dot,1});
        CHECK(xpp::idamax(&count,x.data(),&stride) == auto_numerics_reference::idamax(&count,old_x.data(),&stride));
        double scale=-.3, c=.6, s=.8;
        CHECK(xpp::dscal(&count,&scale,x.data(),&stride) == auto_numerics_reference::dscal(&count,&scale,old_x.data(),&stride));
        same(x,old_x);
        CHECK(xpp::daxpy(&count,&scale,x.data(),&stride,y.data(),&stride) == auto_numerics_reference::daxpy(&count,&scale,old_x.data(),&stride,old_y.data(),&stride));
        same(y,old_y);
        CHECK(xpp::drot(&count,x.data(),&stride,y.data(),&stride,&c,&s) == auto_numerics_reference::drot(&count,old_x.data(),&stride,old_y.data(),&stride,&c,&s));
        same(x,old_x); same(y,old_y);
        CHECK(xpp::dswap(&count,x.data(),&stride,y.data(),&stride) == auto_numerics_reference::dswap(&count,old_x.data(),&stride,old_y.data(),&stride));
        same(x,old_x); same(y,old_y);
    }
    for (const char *ta : {"n","T"}) for (const char *tb : {"N","t"}) {
        integer dim=n;
        std::array<double,n*n> a{1.,-4.,.5,2.,-3.,.25,7.,1.,-2.}, b=a, c=a, old_c=a;
        double alpha=.3, beta=-.7;
        CHECK(xpp::dgemm(ta,tb,&dim,&dim,&dim,&alpha,a.data(),&dim,b.data(),&dim,&beta,c.data(),&dim,1,1) ==
              auto_numerics_reference::dgemm(ta,tb,&dim,&dim,&dim,&alpha,a.data(),&dim,b.data(),&dim,&beta,old_c.data(),&dim,1,1));
        same(c,old_c);
        for (logical transpose : {0,1}) {
            CHECK(xpp::dgemc(&dim,&dim,a.data(),&dim,c.data(),&dim,&transpose) ==
                  auto_numerics_reference::dgemc(&dim,&dim,a.data(),&dim,old_c.data(),&dim,&transpose));
            same(c,old_c);
        }
        CHECK(xpp::lsame(ta,tb,1,1) == auto_numerics_reference::lsame(ta,tb,1,1));
    }
}

void homcont()
{
    xpp::TempDir temp;
    std::string path = temp.path() + "/numerics.odex";
    auto writer = xpp::Writer::binary(path);
    /* One unstable real root and a stable conjugate pair; p changes the basis
       so second calls exercise vector sign and Beyn projection continuity. */
    CHECK(writer.write("par p=0.2\nx'=x+p*y\ny'=-y-2*z\nz'=2*y-z\n") && writer.commit());
    char name[] = "test_auto_numerics";
    char *argv[] = {name, path.data(), nullptr};
    const auto loaded = xpp::load_model(2, argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) return;
    auto &session = xpp::client_session();
    CHECK(xpp::ensure_exact_jacobian(session));
    session.numerics.jacobian = xpp::JACOBIAN_EXACT;
    auto &lib = session.auto_lib;
    lib.session = &session;
    lib.parameter_count = session.auto_state.parameter_layout.size();
    lib.homcont.blhme.ieigc.assign(2,0);
    lib.homcont.blhme.vrprev.assign(2*n*n,0.);
    lib.homcont.beyn.iflag.assign(4,0);
    lib.homcont.beyn.cprev.assign(4*n*n,0.);
    lib.homcont.blhom.nstab = 2;
    lib.homcont.blhom.nunstab = 1;
    lib.homcont.blhma.compzero = auto_hmachho;
    auto new_state = lib.homcont, old_state = lib.homcont;
    std::array projection_new{lib.homcont,lib.homcont};
    auto projection_old = projection_new;
    iap_type iap{};
    iap.lib = &lib; iap.ndim = n; iap.ndm = n; iap.nfpr = 1;
    std::array<integer, 1> icp{session.auto_state.parameter_layout.solver_index(0)};
    std::vector<double> par(lib.parameter_count), dfdp(n*lib.parameter_count), old_dfdp = dfdp;
    std::array<double,n> equilibrium{}, rr{}, ri{}, old_rr{}, old_ri{};
    std::array<double,n*n> a{}, old_a{}, zz{}, old_zz{}, vectors{}, old_vectors{}, bound{}, old_bound{};
    xpp::UniqueFile log(std::tmpfile()), old_log(std::tmpfile());
    CHECK(log && old_log);
    if (!log || !old_log) return;
    integer dim = n;
    for (integer transpose : {1,2}) for (integer endpoint : {1,2}) for (double p : {.2,.3}) {
        par[icp[0]] = p;
        lib.homcont = new_state;
        lib.fp9 = log.get();
        CHECK(xpp::eigho(&iap, &endpoint, &transpose, rr.data(), ri.data(), vectors.data(), equilibrium.data(), icp.data(), par.data(), &dim, a.data(), dfdp.data(), zz.data()) == 0);
        new_state = lib.homcont;
        lib.homcont = old_state;
        lib.fp9 = old_log.get();
        CHECK(auto_numerics_reference::eigho(&iap, &endpoint, &transpose, old_rr.data(), old_ri.data(), old_vectors.data(), equilibrium.data(), icp.data(), par.data(), &dim, old_a.data(), old_dfdp.data(), old_zz.data()) == 0);
        old_state = lib.homcont;
        same(rr,old_rr); same(ri,old_ri); same(vectors,old_vectors); same(a,old_a); same(zz,old_zz); same(dfdp,old_dfdp);
        same(new_state.blhme.vrprev,old_state.blhme.vrprev);
        CHECK(new_state.blhme.ieigc == old_state.blhme.ieigc);
        for (integer manifold : {-1,1}) {
            const size_t side = manifold == -1 ? 0 : 1;
            lib.homcont = projection_new[side];
            lib.fp9 = log.get();
            CHECK(xpp::prjctn(&iap, bound.data(), equilibrium.data(), icp.data(), par.data(), &manifold, &endpoint, &transpose, &dim, a.data(), dfdp.data()) == 0);
            projection_new[side] = lib.homcont;
            lib.homcont = projection_old[side];
            lib.fp9 = old_log.get();
            CHECK(auto_numerics_reference::prjctn(&iap, old_bound.data(), equilibrium.data(), icp.data(), par.data(), &manifold, &endpoint, &transpose, &dim, old_a.data(), old_dfdp.data()) == 0);
            projection_old[side] = lib.homcont;
            same(bound,old_bound); same(a,old_a); same(dfdp,old_dfdp);
            same(projection_new[side].beyn.cprev,projection_old[side].beyn.cprev);
            CHECK(projection_new[side].beyn.iflag == projection_old[side].beyn.iflag);
        }
    }
    /* AUTO's root adapter and its failed-dimension diagnostic are independent
       of the HomCont vector interface above. */
    for (integer leading : {n, n-1}) {
        a = {2.,0.,0., 0.,-1.,2., 0.,-2.,-1.}; old_a = a;
        std::array<doublecomplex,n> ev{}, old_ev{};
        integer error = -1, old_error = -1;
        lib.fp9 = log.get();
        const auto message = failure([&] { xpp::eig(&iap,&dim,&leading,a.data(),ev.data(),&error); });
        lib.fp9 = old_log.get();
        const auto old_message = failure([&] { auto_numerics_reference::eig(&iap,&dim,&leading,old_a.data(),old_ev.data(),&old_error); });
        CHECK(message == old_message && error == old_error);
        same(a,old_a);
        for (integer i=0; i<n; ++i) {
            same({&ev[i].r,1},{&old_ev[i].r,1}); same({&ev[i].i,1},{&old_ev[i].i,1});
        }
    }
    CHECK(contents(log.get()) == contents(old_log.get()));
    lib.fp9 = nullptr;
}
} // namespace

int main()
{
    /* Large off-diagonal and tied candidates distinguish complete from partial
       pivoting and pin AUTO's row/column tie order. */
    const std::vector<double> pivot{1e-20,2.,-2., 100.,3.,1., -100.,1.,4.};
    const std::vector<double> singular{1.,2.,3., 2.,4.,6., 0.,0.,0.};
    const std::vector<double> conjugate{2.,0.,0., 0.,-1.,2., 0.,-2.,-1.};
    const std::vector<double> dense{.25,-2.,3., 4.,.5,-1., -3.,2.,.75};
    solve(pivot,2); solve(pivot,0); solve(singular,0);
    solve({1.,2.,3., 2.,4.,6., 0.,1.,1.},1,true);
    null_vector(singular,2);
    null_vector({1.,0.,0., 0.,1e-32,0., 0.,0.,0.},1); // NLVC's tiny-pivot diagnostic.
    for (integer vectors : {0,1}) for (const auto &matrix : {pivot,singular,conjugate,dense}) roots(matrix,vectors);
    svd(pivot); svd(singular); svd(dense);
    floquet(false,false); floquet(true,false); floquet(false,true);
    qz_vectors(); scalar_helpers(); blas_helpers();
    for (integer manifold : {-1,1}) { schur(conjugate,manifold); schur(dense,manifold); }
    homcont();
    TEST_REPORT("auto_numerics");
}
