/* W334 (#395): AUTO's points and work arrays are owned data, and what is
   borrowed is a bounded view made when it is used. Checked here: the
   column-major views index as ARRAY2D/ARRAY3D do and refuse storage that is
   too short; SolvbvWork states its dimensions once and its views follow a
   resize, a copy and a move; a diagram's points own their arrays through
   moves, copies and a restore; the work arrays of a library copy
   independently. */
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "diagram.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "integrate.h"

#include <deque>
#include <cmath>
#include <numeric>
#include <utility>
#include <vector>

namespace {

/* ARRAY2D(array,i,j) = array[i + j*array_dim1], ARRAY3D(array,i,j,k) =
   array[i + (j + k*array_dim2)*array_dim1] (auto_f2c.h) */
void check_layout()
{
    std::vector<double> v(24);
    std::iota(v.begin(), v.end(), 0.0);
    const xpp::Matrix<double> m(v, 4, 6);
    const xpp::Matrix3<double> t(v, 2, 3, 4);
    bool same = true;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 6; j++) same = same && &m(i, j) == &v[i + j * 4];
    CHECK(same);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 3; j++)
            for (int k = 0; k < 4; k++) same = same && &t(i, j, k) == &v[i + (j + k * 3) * 2];
    CHECK(same);
    m(3, 5) = -1;
    CHECK(v[23] == -1);

    const std::vector<double> input(6, 2.0);
    const xpp::Matrix<const double> in(input, 2, 3); /* an input view cannot be written: const T */
    CHECK(in(1, 2) == 2.0);

    auto refused = [](auto make) {
        try {
            make();
        } catch (const std::length_error &) {
            return true;
        }
        return false;
    };
    CHECK(refused([&] { xpp::Matrix<double> bad(v, 5, 5); }));
    CHECK(refused([&] { xpp::Matrix3<double> bad(v, 2, 3, 5); }));
    CHECK(refused([&] { xpp::Matrix<double> bad(v, -1, 2); }));
    CHECK(!refused([&] { xpp::Matrix<double> exact(v, 6, 4); }));
    CHECK(!refused([&] { xpp::Matrix3<double> empty(std::span<double>(), 0, 3, 4); }));
}

/* the arrays solvbv keeps: sizes from one function, views bounded by them */
void check_solvbv_work()
{
    SolvbvWork w;
    for (long ndim : {1L, 2L, 3L, 2L, 1L}) { /* repeated resizes, up and down */
        const long ncol = 4, ntst = 5, nbc = ndim, nint = 1, parameters = 6;
        const long nrow = ndim * ncol, nclm = nrow + ndim, nrc = nbc + nint + 1, nfpr = 2;
        w.resize(ndim, ncol, ntst, nbc, nint, parameters);
        CHECK(w.a.size() == static_cast<size_t>(nclm * nrow * (ntst + 1)));
        CHECK(w.cc.size() == static_cast<size_t>(nrc * ndim * (ntst + 1)) && w.c2.size() == w.cc.size());
        CHECK(w.icf.size() == static_cast<size_t>(nclm * (ntst + 1)) && w.ipr.size() == static_cast<size_t>(ndim * (ntst + 1)));
        // A fresh factorization overwrites its active entries; sizing must
        // preserve existing storage rather than clearing the whole workspace.
        w.a.front() = 17;
        const double *storage = w.a.data();
        w.resize(ndim, ncol, ntst, nbc, nint, parameters);
        CHECK(w.a.data() == storage && w.a.front() == 17);
        /* the call's own dimensions (nfpr <= parameters, na = ntst) fit */
        auto a = w.a_matrix(nclm, nrow, ntst);
        auto b = w.b_matrix(nfpr, nrow, ntst);
        auto c = w.c_matrix(nclm, nrc, ntst);
        auto d = w.d_matrix(nfpr, nrc);
        a(nclm - 1, nrow - 1, ntst - 1) = 1;
        b(nfpr - 1, nrow - 1, ntst - 1) = 2;
        c(nclm - 1, nrc - 1, ntst - 1) = 3;
        d(nfpr - 1, nrc - 1) = 4;
        CHECK(w.a[(nclm - 1) + ((nrow - 1) + (ntst - 1) * nrow) * nclm] == 1);
        CHECK(w.d[(nfpr - 1) + (nrc - 1) * nfpr] == 4);
        /* a mesh one interval longer than the work was sized for is refused */
        bool refused = false;
        try {
            w.a_matrix(nclm, nrow, ntst + 2);
        } catch (const std::length_error &) {
            refused = true;
        }
        CHECK(refused);
    }

    /* a copy (LocationState keeps one) and a move own their storage */
    w.resize(2, 3, 4, 2, 0, 5);
    w.a_matrix(8, 6, 4)(0, 0, 0) = 7;
    SolvbvWork copy = w;
    copy.a_matrix(8, 6, 4)(0, 0, 0) = 8;
    CHECK(w.a_matrix(8, 6, 4)(0, 0, 0) == 7 && copy.a_matrix(8, 6, 4)(0, 0, 0) == 8);
    SolvbvWork moved = std::move(copy);
    CHECK(moved.a_matrix(8, 6, 4)(0, 0, 0) == 8);
    w = moved; /* the restore path: assignment, then fresh views */
    CHECK(w.a_matrix(8, 6, 4)(0, 0, 0) == 8);
    w.resize(1, 1, 1, 1, 0, 1);
    CHECK(moved.a_matrix(8, 6, 4)(0, 0, 0) == 8 && w.a.size() == 4);
}

/* a point owns six arrays of n; the diagram's list can be moved, copied and
   restored without any of them pointing into another point's storage */
xpp::DIAGRAM point_of(int n, double base)
{
    xpp::DIAGRAM p{};
    p.size_variables(static_cast<std::size_t>(n));
    for (int i = 0; i < n; i++) {
        p.uhi[i] = base + i;
        p.ulo[i] = -base - i;
        p.u0[i] = 2 * base + i;
        p.ubar[i] = 3 * base + i;
        p.evr[i] = 4 * base + i;
        p.evi[i] = 5 * base + i;
    }
    return p;
}

void check_diagram_points(xpp::Session &s)
{
    constexpr int n = 3;
    start_diagram(s, n);
    xpp::DIAGRAM *first = &s.diagram.points.front();
    CHECK(first && first->u0.size() == n && first->evi.size() == n && first->index == 0);
    for (int k = 1; k <= 20; k++) { /* a deque keeps the earlier points' addresses while it grows */
        xpp::DIAGRAM p = point_of(n, k);
        p.branch = std::make_shared<xpp::DiagramBranch>();
        add_diagram(s, p);
    }
    CHECK(diagram_count(s.diagram) == 21 && &s.diagram.points.front() == first);
    CHECK(s.diagram.points[7].u0[2] == 2 * 7 + 2 && s.diagram.points[7].index == 7);

    /* restore: the list is taken whole, indices renumbered, arrays kept */
    std::deque<xpp::DIAGRAM> list;
    for (int k = 0; k < 5; k++) list.push_back(point_of(n, 100 + k));
    const std::deque<xpp::DIAGRAM> snapshot = list;
    for (int round = 0; round < 3; round++) { /* repeated restores, as a session load does */
        diagram_restore(s, list);
        CHECK(diagram_count(s.diagram) == 5);
        bool ok = true;
        for (int i = 0; i < 5; i++) {
            const xpp::DIAGRAM *d = &s.diagram.points[static_cast<std::size_t>(i)];
            ok = ok && d->index == i && d->uhi == snapshot[i].uhi && d->evi == snapshot[i].evi &&
                 d->uhi.data() != snapshot[i].uhi.data();
        }
        CHECK(ok);
        list = std::move(s.diagram.points); /* moved out and back in */
    }
    diagram_restore(s, std::move(list));
    s.diagram.points[0].u0.assign(n, -1.0);
    CHECK(snapshot[0].u0[0] == 200); /* no point shares an array with a copy */

    /* a run's point of the wrong length is the caller's: the first point takes the point whole */
    start_diagram(s, n);
    xpp::DIAGRAM p = point_of(n, 9);
    edit_start(s, p);
    CHECK(s.diagram.points.front().ubar == p.ubar && s.diagram.points.front().ubar.data() != p.ubar.data());
}

/* the work arrays of a library: sized once, a worker's copy of the rotation
   counts owns them */
// W359: count calls, independent of machine speed. The callbacks implement
// u' = p with periodic ends; the arclength row fixes the constant direction.
struct AssemblyProbe : AutoLib {
    int evaluations = 0, residuals = 0, boundaries = 0;
    bool change_parameters = false;
};

int probe_function(const iap_type *iap, const rap_type *, integer,
                   const double *, const double *, const integer *, double *par,
                   integer ijac, double *f, double *dfdu, double *dfdp)
{
    auto &probe = static_cast<AssemblyProbe &>(*iap->lib);
    ++probe.evaluations;
    if (ijac == 0) ++probe.residuals;
    f[0] = par[0];
    if (ijac != 0) { dfdu[0] = 0; dfdp[0] = 1; }
    return 0;
}

int probe_boundary(const iap_type *iap, const rap_type *, integer, double *par,
                   const integer *, integer, const double *u0, const double *u1,
                   double *f, integer, double *dbc)
{
    auto &probe = static_cast<AssemblyProbe &>(*iap->lib);
    ++probe.boundaries;
    if (probe.change_parameters) par[0] = std::nextafter(par[0], 1.0);
    f[0] = u0[0] - u1[0];
    dbc[0] = 1; dbc[1] = -1; dbc[2] = 0;
    return 0;
}

void check_assembly_passes(xpp::Session &s)
{
    AssemblyProbe lib;
    lib.session = &s;
    lib.parameter_count = 1;
    iap_type iap{};
    iap.lib = &lib;
    iap.ndim = iap.nbc = iap.nfpr = 1;
    iap.ncol = 2; // AUTO supports two through seven collocation points.
    iap.ntst = 2;
    iap.ips = xpp::AutoProblem::Periodic;
    rap_type rap{};
    integer fresh = 1, nullity = 0, stride = 3, icp = 0;
    double par = .25, current = par, old = par, tangent = 0, ds = .1, p0 = 0, p1 = 0;
    std::array<double, 6> ups{.5, .5, .5, .5, .5, .5}, previous = ups, udot{1, 1, 1, 1, 1, 1}, upold{}, dups{}, fa{};
    std::array<double, 2> dtm{.5, .5}, fc{};
    double thl = 1, thu = 1;
    const auto solve = [&] {
        solvbv(&fresh, &iap, &rap, &par, &icp, probe_function, probe_boundary,
               nullptr, &ds, &nullity, &current, &old, &tangent, &stride,
               ups.data(), dups.data(), previous.data(), udot.data(), upold.data(),
               dtm.data(), fa.data(), fc.data(), &p0, &p1, &thl, &thu);
    };
    solve();
    CHECK(lib.evaluations == iap.ntst * iap.ncol && lib.residuals == 0);
    const auto first_fa = fa;
    const auto first_fc = fc;
    const double determinant = rap.det;
    fresh = 0;
    solve();
    CHECK(lib.evaluations == 2 * iap.ntst * iap.ncol && lib.residuals == iap.ntst * iap.ncol);
    CHECK(fa == first_fa && fc == first_fc && rap.det == determinant);
    const int evaluations = lib.evaluations, boundaries = lib.boundaries;
    nullity = -1;
    solve();
    CHECK(lib.evaluations == evaluations && lib.boundaries == boundaries);
    // Quadrature and elimination round the analytically constant direction.
    constexpr double direction_tolerance = 1e-12;
    CHECK(std::abs(fc[0] - 1) < direction_tolerance && std::abs(fc[1]) < direction_tolerance);
    for (int j = 0; j < iap.ncol; ++j)
        for (int i = 0; i < iap.ntst; ++i)
            CHECK(std::abs(fa[i + stride * j] - 1) < direction_tolerance);
    // Boundary assembly can change parameter bits; reuse is then invalid.
    const int residuals = lib.residuals;
    lib.change_parameters = true;
    fresh = 1;
    nullity = 0;
    solve();
    CHECK(lib.evaluations == evaluations + 2 * iap.ntst * iap.ncol);
    CHECK(lib.residuals == residuals + iap.ntst * iap.ncol);
    CHECK(par == std::nextafter(std::nextafter(current, 1.0), 1.0));
    // HomCont's boundary visits advance projection continuity even though
    // the direction solve discards their residual values.
    lib.change_parameters = false;
    const int hom_evaluations = lib.evaluations, hom_boundaries = lib.boundaries;
    iap.ips = xpp::AutoProblem::Homoclinic;
    fresh = 0;
    nullity = -1;
    solve();
    CHECK(lib.evaluations == hom_evaluations + iap.ntst * iap.ncol);
    CHECK(lib.boundaries == hom_boundaries + 1);
}

void check_library_arrays(xpp::Session &s)
{
    AutoLib &lib = s.auto_lib;
    lib.parameter_count = 5;
    iap_type iap{};
    iap.lib = &lib;
    for (int ndim : {2, 4, 1, 3}) { /* the same library sized again for each run */
        iap.ndim = ndim;
        iap.nbc = ndim + 1;
        size_run_scratch(iap);
        CHECK(lib.scratch.dfu.size() == static_cast<size_t>(ndim * ndim) && lib.scratch.dfp.size() == static_cast<size_t>(ndim * 5));
        CHECK(lib.scratch.uu1.size() == static_cast<size_t>(ndim) && lib.scratch.ff2.size() == static_cast<size_t>(ndim));
        CHECK(lib.rotations.nrtn.size() == static_cast<size_t>(ndim + 1));
    }
    lib.rotations.irtn = 1;
    lib.rotations.nrtn[0] = 3;
    AutoGlobalRotations worker = lib.rotations;
    worker.nrtn[0] = 9;
    CHECK(lib.rotations.nrtn[0] == 3 && worker.nrtn[0] == 9 && worker.irtn == 1);
    lib.rotations = AutoGlobalRotations{};
    CHECK(worker.nrtn[0] == 9 && lib.rotations.nrtn.empty());
}

} // namespace

int main()
{
    char name[] = "test_auto_views";
    char file[] = "examples/ode/lecar.odex";
    char *argv[] = {name, file, nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    check_layout();
    check_solvbv_work();
    check_diagram_points(s);
    check_library_arrays(s);
    check_assembly_passes(s);
    // W366: load a real Markov model; both grab paths see node-only storage.
    xpp::TempDir temp;
    std::string model = temp.path() + "/grab.odex";
    xpp::Writer writer(model);
    CHECK(static_cast<bool>(writer));
    writer.print("x'=0\nmarkov z 2\n{{0}} {{1}}\n{{1}} {{0}}\ninit x=1,z=1\n");
    CHECK(writer.commit());
    char *markov_argv[] = {name, model.data(), nullptr};
    const auto loaded = xpp::load_model(2, markov_argv, 1);
    CHECK(loaded.has_value());
    if (!loaded) return 1;
    auto &markov = xpp::client_session();
    CHECK(markov.model().node == 1 && markov.model().nmarkov == 1);
    markov.auto_state.dir = temp.path();
    xpp::init_auto_win(markov);
    auto point = point_of(1, 3);
    point.ibr = 1;
    point.ntot = 2;
    point.lab = 7;
    point.branch = std::make_shared<xpp::DiagramBranch>();
    edit_start(markov, point);
    point.ntot = 3;
    point.lab = 8;
    add_diagram(markov, point);
    markov.auto_state.diag_flag = true;
    const double saved_markov = markov.last_ic[1];
    CHECK(auto_grab_label(markov, 7) == 1);
    CHECK(markov.last_ic[0] == 6 && markov.last_ic[1] == saved_markov);
    markov.last_ic[0] = -1;
    find_point(markov, 1, 2);
    CHECK(markov.last_ic[0] == 6 && markov.last_ic[1] == saved_markov);
    // The shared owner refuses an invalid length before changing any values.
    const auto before = markov.last_ic;
    bool refused = false;
    try { xpp::set_initial_data(markov, {}); }
    catch (const std::length_error &) { refused = true; }
    CHECK(refused && markov.last_ic == before);
    const std::array<double, 2> full = {4, 0};
    xpp::set_initial_data(markov, full);
    CHECK(markov.last_ic[0] == 4 && markov.last_ic[1] == 0);
    TEST_REPORT("AUTO views");
}
