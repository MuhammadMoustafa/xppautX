/* W560 (#625): problem_function's difference Jacobian by column groups (jacobian.h's
   DifferenceGroups) is the same bits as one column at a time; W564 (#629): so is the fold
   system's, and a difference step restores the parameter itself. */
#include "load_args.h"
#include "model_text.h"
#include "xpptest.h"
#include "session.h"
#include "jacobian.h"
#include "autox_c.h"
#include "autox_jacobian.h"
#include "autox_unknowns.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include <array>
#include <bit>
#include <cstdint>
#include <vector>

TEST_CASE("test_autox_groups") {
    constexpr int n = 6;
    xpp::TempDir temp;
    /* a ring of neighbours: each equation reads three states, so the columns fall in fewer groups than states */
    std::string model = temp.path() + "/ring.odex";
    auto writer = xpp::Writer::binary(model);
    CHECK(writer.write(versioned("g=tanh(x1)+0.5*x4\n"
                                 "x1'=-x1+p*x6*x2+g+q\nx2'=-x2+p*x1*x3\nx3'=-x3+p*x2*x4\n"
                                 "x4'=-x4+p*x3*x5\nx5'=-x5+p*x4*x6\nx6'=-x6+p*x5*x1\n"
                                 "par p=1.3,q=0.7\ninit x1=.1,x2=-.2,x3=.3,x4=-.4,x5=.5,x6=.6\n")) && writer.commit());
    char name[] = "test_autox_groups";
    char *argv[] = {name, model.data(), nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    auto &s = xpp::client_session();
    AutoxRun run;
    run.session = &s;
    run.parameter_count = s.autox_state.parameter_layout.size();
    iap_type iap{};
    iap.run = &run;
    iap.ndm = n;
    iap.ndim = n;
    iap.nfpr = 1;
    iap.jac = 0;
    iap.ips = xpp::AutoxProblem::SteadyState;
    rap_type rap{};
    std::array<integer, 1> icp{s.autox_state.parameter_layout.solver_index(0)};
    std::vector<double> par(run.parameter_count, 0.);
    par[icp[0]] = 1.3;
    const std::array<double, n> u{.1, -.2, .3, -.4, .5, .6};
    std::array<double, n> f{}, f_columns{};
    std::array<double, n * n> grouped{}, columns{};
    std::vector<double> dp(n * run.parameter_count);

    problem_function(&iap, &rap, n, u.data(), u.data(), icp.data(), par.data(), 2, f.data(), grouped.data(), dp.data());
    const xpp::DifferenceGroups *groups = xpp::difference_groups(s.model(), run.jacobian_scratch, n);
    CHECK(groups != nullptr && groups->groups.size() + 1 < n);
    /* no groups: every column on its own */
    run.jacobian_scratch.groups = xpp::DifferenceGroups{};
    CHECK(xpp::difference_groups(s.model(), run.jacobian_scratch, n) == nullptr);
    problem_function(&iap, &rap, n, u.data(), u.data(), icp.data(), par.data(), 2, f_columns.data(), columns.data(), dp.data());
    int differ = 0;
    for (int i = 0; i < n * n; ++i) differ += std::bit_cast<std::uint64_t>(grouped[i]) != std::bit_cast<std::uint64_t>(columns[i]);
    for (int i = 0; i < n; ++i) differ += std::bit_cast<std::uint64_t>(f[i]) != std::bit_cast<std::uint64_t>(f_columns[i]);
    CHECK(differ == 0);
    CHECK(grouped[0 + 0 * n] != 0 && grouped[2 + 0 * n] == 0); /* the Jacobian is not trivially zero, and x1 is not read by x3 */

    /* the fold system (x, v, p2): its columns in groups give the same bits as one by one, and the
       parameter, however small against the step, is restored exactly */
    constexpr int ndim = 2 * n + 1;
    iap.ndim = ndim;
    iap.nfpr = 2;
    std::array<integer, 2> icp2{icp[0], s.autox_state.parameter_layout.solver_index(1)};
    const std::array<double, ndim> w{.1, -.2, .3, -.4, .5, .6, .3, .1, -.2, .4, .5, -.1, 0.7};
    std::array<double, ndim * ndim> fold_grouped{}, fold_single{};
    std::array<double, ndim> g{}, g_columns{};
    std::vector<double> gp(ndim * run.parameter_count), gp_columns(ndim * run.parameter_count);
    constexpr double tiny = 1e-9;
    par[icp2[0]] = tiny;
    par[icp2[1]] = w[ndim - 1];
    run.jacobian_scratch.groups.reset();
    CHECK(xpp::autox_jacobian::state_groups(&iap, n) != nullptr);
    const auto columns_of = xpp::autox_jacobian::fold_columns(*xpp::autox_jacobian::state_groups(&iap, n), n, ndim, w.data(),
                                                             xpp::autox_jacobian::central_step(ndim, w.data()));
    CHECK(columns_of.groups.size() < n + 1);
    fold_equations(&iap, &rap, ndim, w.data(), w.data(), icp2.data(), par.data(), 2, g.data(), fold_grouped.data(), gp.data());
    CHECK(par[icp2[0]] == tiny && par[icp2[1]] == w[ndim - 1]);
    run.jacobian_scratch.groups = xpp::DifferenceGroups{};
    fold_equations(&iap, &rap, ndim, w.data(), w.data(), icp2.data(), par.data(), 2, g_columns.data(), fold_single.data(), gp_columns.data());
    differ = 0;
    for (int i = 0; i < ndim * ndim; ++i) differ += std::bit_cast<std::uint64_t>(fold_grouped[i]) != std::bit_cast<std::uint64_t>(fold_single[i]);
    for (int i = 0; i < ndim; ++i) differ += std::bit_cast<std::uint64_t>(g[i]) != std::bit_cast<std::uint64_t>(g_columns[i]);
    for (std::size_t i = 0; i < gp.size(); ++i) differ += std::bit_cast<std::uint64_t>(gp[i]) != std::bit_cast<std::uint64_t>(gp_columns[i]);
    CHECK(differ == 0);
    TEST_REPORT("autox groups");
}
