/* W333 (#394): fort.8 restart layouts, including augmented formulations.
   Fixtures have distinct states and tangents at every collocation node;
   untouched columns are canaries, so a stride/range mixup cannot pass. */
#include "load_args.h"
#include "xpptest.h"
#include "session.h"
#include "autox_c.h"
#include "autox_nox.h"
#include "autox_parallel.h"
#include "autox_unknowns.h"
#include "xpp_batch.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_math.h"
#include <array>
#include <limits>
#include <vector>

namespace {
constexpr double canary = 77.;

std::string solution(long parameters, bool bvp) {
    std::string text = xpp::format("3 1 5 7 {} 2 5 3 13 2 2 {}\n",
                                   bvp ? 1 : 2, parameters);
    text += "0 2 3\n0.25 4 5\n0.5 6 7\n0.75 8 9\n1 10 11\n";
    text += bvp ? "10\n0.75\n" : "10 11\n0.75 -0.5\n";
    text += "12 13\n14 15\n16 17\n18 19\n20 21\n";
    for (long k = 0; k < parameters; ++k)
        text += xpp::format("{}{}", k ? " " : "", k + 0.125);
    return text + "\n";
}

void formulation(xpp::Session &s, decltype(&periodic_fold_starting_data) restart,
                 long dimension, long free, bool bvp, bool torus,
                 bool optimization, bool truncated) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.3";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    CHECK(file != nullptr);
    AutoxRun run;
    run.session = &s;
    run.parameter_count = s.autox_state.parameter_layout.size();
    run.fp3 = file.get();
    run.fort3 = path;
    std::string text = solution(run.parameter_count, bvp);
    if (truncated) text.resize(text.find_last_of(' '));
    xpp::print(file.get(), "{}", text);
    CHECK(xpp::seek_stream(file.get(), 0, path).has_value());
    iap_type iap{};
    iap.run = &run;
    iap.ndim = dimension;
    iap.ndm = 2;
    iap.nfpr = free;
    iap.ntst = 2;
    iap.ncol = 2;
    iap.irs = 7;
    iap.ips = xpp::AutoxProblem::Periodic;
    iap.isw = 2;
    rap_type rap{};
    std::array<std::ptrdiff_t, 4> icp = {10, 11, 10, 11};
    std::vector<double> par(run.parameter_count, canary);
    std::vector<double> ups(3 * dimension * 2, canary);
    std::vector<double> tangent(ups.size(), canary), old(ups.size(), canary);
    std::array<double, 4> current{}, direction{};
    std::array<double, 3> tm{}, dtm{}, thl{}, thu{};
    std::ptrdiff_t ntst = 0, ncol = 0, stride = 3, nodir = -2;
    std::string failed;
    try {
        CHECK(restart(&iap, &rap, par.data(), icp.data(), &ntst, &ncol,
                      current.data(), direction.data(), &stride,
                      ups.data(), tangent.data(), old.data(), tm.data(),
                      dtm.data(), &nodir, thl.data(), thu.data()) == 0);
    } catch (const xpp::AutoxFailed &e) {
        failed = e.error.what;
    }
    /* a short read ends the run, naming the label and what is missing */
    CHECK(failed.empty() != truncated);
    if (truncated) {
        CHECK(failed.find("label 7") != std::string::npos && failed.find("ends before") != std::string::npos);
        return;
    }
    CHECK(ntst == 2 && ncol == 2 && iap.ibr == 3);
    CHECK(tm == (std::array<double, 3>{0., .5, 1.}));
    CHECK(nodir == (bvp || optimization ? 1 : 0));
    for (int node = 0; node < 5; ++node) {
        const int row = node == 4 ? 2 : node / 2;
        const int column = node == 4 ? 0 : node % 2;
        const long at = row + column * dimension * stride;
        CHECK(ups[at] == 2 + 2 * node && ups[at + stride] == 3 + 2 * node);
        if (bvp) {
            CHECK(ups[at + 2 * stride] == 12 + 2 * node);
            CHECK(ups[at + 3 * stride] == 13 + 2 * node);
            CHECK(tangent[at] == canary);
        } else {
            CHECK(tangent[at] == 12 + 2 * node && tangent[at + stride] == 13 + 2 * node);
            if (torus) {
                const double sine = node == 4 ? 0. : xpp::math::sin(node * .25) * 1e-4;
                const double cosine = node == 4 ? 0. : xpp::math::cos(node * .25) * 1e-4;
                CHECK(ups[at + 2 * stride] == sine && ups[at + 3 * stride] == sine);
                CHECK(ups[at + 4 * stride] == cosine && ups[at + 5 * stride] == cosine);
                // Interior torus direction columns were not initialized by
                // the original reader; only the endpoint was zeroed.
                CHECK(tangent[at + 2 * stride] == (node == 4 ? 0. : canary));
            } else {
                CHECK(ups[at + 2 * stride] == 0. && ups[at + 3 * stride] == 0.);
            }
        }
    }
    CHECK(direction[0] == (optimization ? 0. : .75));
    if (!bvp && !optimization) CHECK(direction[2] == (torus ? 0. : restart == periodic_fold_starting_data ? -.5 : 0.));
}

void algebraic(xpp::Session &s, bool truncated) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.3";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    AutoxRun run;
    run.session = &s;
    run.parameter_count = s.autox_state.parameter_layout.size();
    run.fp3 = file.get();
    run.fort3 = path;
    std::string text = xpp::format("3 1 5 7 1 1 1 3 2 0 0 {}\n0 2 3\n", run.parameter_count);
    for (long k = 0; k < run.parameter_count - (truncated ? 1 : 0); ++k)
        text += xpp::format("{} ", k + .125);
    xpp::print(file.get(), "{}\n", text);
    CHECK(xpp::seek_stream(file.get(), 0, path).has_value());
    iap_type iap{};
    iap.run = &run;
    iap.ndim = 2;
    iap.irs = 7;
    rap_type rap{};
    bool found = 0;
    std::ptrdiff_t free = 0;
    find_restart_label(&iap, &rap, 7, &free, &found);
    CHECK(found && free == 1);
    std::array<double, 2> u{};
    std::vector<double> par(run.parameter_count, canary);
    std::string failed;
    try {
        read_restart_label(&iap, &rap, u.data(), par.data());
    } catch (const xpp::AutoxFailed &e) {
        failed = e.error.what;
    }
    /* a short read ends the run, naming the label and what is missing */
    CHECK(failed.empty() != truncated);
    if (truncated) CHECK(failed.find("label 7") != std::string::npos && failed.find("ends before the parameter values") != std::string::npos);
    if (!truncated) {
        CHECK(u == (std::array<double, 2>{2., 3.}));
        CHECK(par.back() == run.parameter_count - 1 + .125);
    }
}
// The UZR routines differ only in diagnostic width; preserve the signed
// index, full attempted point number and emitted text, not just the value.
void user_zero_text(xpp::Session &s) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.9";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    AutoxRun run;
    run.session = &s;
    run.parameter_count = s.autox_state.parameter_layout.size();
    run.fp9 = file.get();
    iap_type iap{};
    iap.run = &run;
    iap.iid = 3;
    iap.ibr = -3;
    iap.ntot = 10000;
    iap.nfpr = 1;
    rap_type rap{};
    std::ptrdiff_t icp[] = {xpp::autox_unknown::period};
    const AutoxMark marks[] = {{-xpp::autox_unknown::period, .125}};
    std::vector<double> par(run.parameter_count);
    par[xpp::autox_unknown::period] = .875;
    bool changed = 0;
    CHECK(user_zero_test_algebraic(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 marks) == .75);
    CHECK(changed == true);
    changed = 0;
    CHECK(user_zero_test_boundary(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, marks) == .75);
    CHECK(changed == true);
    CHECK(xpp::seek_stream(file.get(), 0, path).has_value());
    auto reader = xpp::LineReader::attach(file.get());
    CHECK(reader.next() == "   3 10001        User Func.   0     7.500000E-01");
    CHECK(reader.next() == "   3 10001        User Func.   0   7.500000E-01");
    CHECK(!reader.next());
}

// W384 (#446): exercise the real writers at the wrap and beyond their old
// column widths, without computing thousands of continuation points.
void point_files(xpp::Session &s, int count, bool collocation) {
    xpp::TempDir temp;
    AutoxRun run;
    run.session = &s;
    run.parameter_count = s.autox_state.parameter_layout.size();
    const std::string solution_path = temp.path() + "/fort.8";
    run.fp8.reset(xpp::files::open_stream(solution_path, "w+"));
    CHECK(run.fp8 != nullptr);
    iap_type iap{};
    iap.run = &run;
    iap.ibr = 3;
    iap.ntot = count;
    iap.lab = 7;
    iap.nfpr = iap.nicp = iap.ndim = iap.ndm = 1;
    iap.isw = 1;
    iap.itp = {xpp::AutoxPointKind::End, xpp::AutoxPointKind::Regular};
    iap.ntst = collocation ? 1 : 0;
    iap.ncol = collocation ? 2 : 0;
    rap_type rap{};
    std::array<std::ptrdiff_t, 1> icp = {s.autox_state.parameter_layout.solver_index(0)};
    std::vector<double> par(run.parameter_count);
    std::array<double, 4> values = {2., 3., 4., 5.}, tangent = {.5, .5, .5, .5};
    std::array<double, 2> times = {0., 1.};
    double current = .25, direction = 1., interval = 1.;
    std::ptrdiff_t stride = 2;
    if (collocation) {
        CHECK(write_solution_restart(&iap, &rap, par.data(), icp.data(), &direction, &stride,
                      values.data(), tangent.data(), times.data(), &interval) == 0);
    } else {
        CHECK(write_special_point_restart(&iap, &rap, par.data(), icp.data(), &iap.lab, &current, values.data(), tangent.data(), &direction) == 0);
    }
    CHECK(xpp::seek_stream(run.fp8.get(), 0, solution_path).has_value());
    auto solutions = xpp::TokenReader::attach(run.fp8.get());
    xpp::AutoxSolutionHeader header;
    CHECK(xpp::read_autox_solution_header(solutions, header, run.parameter_count));
    CHECK(header.ibr == 3 && header.ntot == count && header.lab == 7);
    CHECK(header.itp == iap.itp && header.ntst == iap.ntst && header.ncol == iap.ncol);
    solutions.close();
    run.fp8.reset();

    const std::string diagram_path = temp.path() + "/fort.7";
    const std::string diagnostic_path = temp.path() + "/fort.9";
    xpp::UniqueFile diagram(xpp::files::open_stream(diagram_path, "w+"));
    xpp::UniqueFile diagnostic(xpp::files::open_stream(diagnostic_path, "w+"));
    CHECK(diagram != nullptr && diagnostic != nullptr);
    run.fp7 = diagram.get();
    run.fp9 = diagnostic.get();
    // Both signs matter: fort.7 uses the sign to encode stability.
    for (std::ptrdiff_t point : {static_cast<std::ptrdiff_t>(count), -static_cast<std::ptrdiff_t>(count)})
        CHECK(write_diagram_line(&iap, &rap, par.data(), icp.data(), icp.data(), &iap.ibr,
                     &point, &iap.lab, &current, values.data()) == 0);
    CHECK(xpp::seek_stream(diagram.get(), 0, diagram_path).has_value());
    auto lines = xpp::LineReader::attach(diagram.get());
    for (int sign : {1, -1}) {
        const auto line = lines.next();
        CHECK(line.has_value());
        if (!line) continue;
        auto row = xpp::TokenReader::of_text(*line);
        int branch = 0, point = 0, type = 0, label = 0;
        CHECK(row.read(branch) && row.read(point) && row.read(type) && row.read(label));
        CHECK(branch == 3 && point == sign * count && type == 9 && label == 7);
    }
    CHECK(!lines.next());
    run.fp7 = run.fp9 = nullptr;
    run.session = nullptr;
}
// W563 (#628): a branch point keeps the direction of its branch (AUTO-07p's NTST=1 record), a regular
// point none; the header counts every row, so a label after the record is found, and read_restart_label gives the
// direction back exactly as written.
void branch_point_direction(xpp::Session &s) {
    for (bool branch_point : {true, false}) {
        xpp::TempDir temp;
        const std::string path = temp.path() + "/fort.8";
        AutoxRun run;  // write_special_point_restart writes the session's own fort.8
        run.session = &s;
        run.parameter_count = s.autox_state.parameter_layout.size();
        run.fp8.reset(xpp::files::open_stream(path, "w+"));
        CHECK(run.fp8 != nullptr);
        iap_type iap{};
        iap.run = &run;
        iap.ibr = 3;
        iap.ntot = 5;
        iap.lab = 7;
        iap.nfpr = iap.nicp = 1;
        iap.ndim = iap.ndm = 2;
        iap.isw = 1;
        iap.itp = {branch_point ? xpp::AutoxPointKind::BranchPoint : xpp::AutoxPointKind::End, xpp::AutoxPointKind::Regular};
        rap_type rap{};
        std::array<std::ptrdiff_t, 1> icp = {s.autox_state.parameter_layout.solver_index(0)};
        std::vector<double> par(run.parameter_count);
        std::array<double, 2> values = {2., 3.}, tangent = {.6, -.8};
        std::array<double, 1> rldot = {.25};
        double current = .5;
        CHECK(write_special_point_restart(&iap, &rap, par.data(), icp.data(), &iap.lab, &current, values.data(), tangent.data(), rldot.data()) == 0);
        iap.lab = 8;
        iap.itp = {xpp::AutoxPointKind::End, xpp::AutoxPointKind::Regular};
        CHECK(write_special_point_restart(&iap, &rap, par.data(), icp.data(), &iap.lab, &current, values.data(), tangent.data(), rldot.data()) == 0);
        run.fp3 = run.fp8.get();
        run.fort3 = path;
        CHECK(std::fflush(run.fp8.get()) == 0); /* find_restart_label reads the file by its path */
        iap.irs = 7;
        bool found = false;
        std::ptrdiff_t free = 0;
        find_restart_label(&iap, &rap, 7, &free, &found);
        CHECK(found);
        xpp::AutoxSolutionHeader header;
        {
            auto reader = xpp::TokenReader::attach(run.fp3);
            CHECK(xpp::read_autox_solution_header(reader, header, run.parameter_count));
            CHECK(header.equilibrium_direction() == branch_point);
        }
        CHECK(xpp::seek_stream(run.fp3, 0, path).has_value());
        std::array<double, 2> u{};
        std::vector<double> restored(run.parameter_count, canary);
        read_restart_label(&iap, &rap, u.data(), restored.data());
        CHECK(u == values && restored[icp[0]] == current);
        CHECK(run.restart_direction.has_value() == branch_point);
        if (branch_point) {
            CHECK(run.restart_direction->parameter == icp[0] && run.restart_direction->rldot == rldot[0]);
            CHECK(run.restart_direction->udot == (std::vector<double>{.6, -.8}));
        }
        // the next label is found past the record: nrowpr counted every row of it
        bool next = false;
        find_restart_label(&iap, &rap, 8, &free, &next);
        CHECK(next);
        run.fp3 = nullptr;
        run.fp8.reset();
        run.restart_direction.reset();
        run.session = nullptr;
    }
}
} // namespace

TEST_CASE("test_autox_restart") {
    char name[] = "test_autox_restart";
    char model[] = "examples/ode/lecar.odex";
    char *argv[] = {name, model, nullptr};
    CHECK(load_args(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    xpp::init_autox_win(s);
    // Directly cover each audit-named augmented fort.8 formulation.
    for (bool truncated : {false, true}) {
        formulation(s, periodic_fold_starting_data, 4, 4, false, false, false, truncated);
        formulation(s, period_doubling_starting_data, 4, 3, false, false, false, truncated);
        formulation(s, torus_starting_data, 6, 4, false, true, false, truncated);
        formulation(s, boundary_fold_starting_data, 4, 2, true, false, false, truncated);
        formulation(s, periodic_optimization_starting_data, 4, 4, false, false, true, truncated);
        algebraic(s, truncated);
    }
    user_zero_text(s);
    branch_point_direction(s);
    for (int count : {9999, 10000, 10001, 100000, std::numeric_limits<int>::max()})
        for (bool collocation : {false, true}) point_files(s, count, collocation);
    TEST_REPORT("autoX restart families");
}
