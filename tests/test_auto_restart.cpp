/* W333 (#394): fort.8 restart layouts, including augmented formulations.
   Fixtures have distinct states and tangents at every collocation node;
   untouched columns are canaries, so a stride/range mixup cannot pass. */
#include "xpptest.h"
#include "session.h"
#include "auto_c.h"
#include "auto_nox.h"
#include "auto_parallel.h"
#include "auto_unknowns.h"
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

void formulation(xpp::Session &s, decltype(&stpnpl) restart,
                 long dimension, long free, bool bvp, bool torus,
                 bool optimization, bool truncated) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.3";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    CHECK(file != nullptr);
    AutoLib lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    lib.fp3 = file.get();
    std::string text = solution(lib.parameter_count, bvp);
    if (truncated) text.resize(text.find_last_of(' '));
    xpp::print(file.get(), "{}", text);
    CHECK(xpp::rewind_stream(file.get(), path).has_value());
    iap_type iap{};
    iap.lib = &lib;
    iap.ndim = dimension;
    iap.ndm = 2;
    iap.nfpr = free;
    iap.ntst = 2;
    iap.ncol = 2;
    iap.irs = 7;
    iap.ips = xpp::AutoProblem::Periodic;
    iap.isw = 2;
    rap_type rap{};
    std::array<integer, 4> icp = {10, 11, 10, 11};
    std::vector<double> par(lib.parameter_count, canary);
    std::vector<double> ups(3 * dimension * 2, canary);
    std::vector<double> tangent(ups.size(), canary), old(ups.size(), canary);
    std::array<double, 4> current{}, direction{};
    std::array<double, 3> tm{}, dtm{}, thl{}, thu{};
    integer ntst = 0, ncol = 0, stride = 3, nodir = -2;
    std::string failed;
    try {
        CHECK(restart(&iap, &rap, par.data(), icp.data(), &ntst, &ncol,
                      current.data(), direction.data(), &stride,
                      ups.data(), tangent.data(), old.data(), tm.data(),
                      dtm.data(), &nodir, thl.data(), thu.data()) == 0);
    } catch (const xpp::AutoFailed &e) {
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
    if (!bvp && !optimization) CHECK(direction[2] == (torus ? 0. : restart == stpnpl ? -.5 : 0.));
}

void algebraic(xpp::Session &s, bool truncated) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.3";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    AutoLib lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    lib.fp3 = file.get();
    std::string text = xpp::format("3 1 5 7 1 1 1 3 2 0 0 {}\n0 2 3\n", lib.parameter_count);
    for (long k = 0; k < lib.parameter_count - (truncated ? 1 : 0); ++k)
        text += xpp::format("{} ", k + .125);
    xpp::print(file.get(), "{}\n", text);
    CHECK(xpp::rewind_stream(file.get(), path).has_value());
    iap_type iap{};
    iap.lib = &lib;
    iap.ndim = 2;
    iap.irs = 7;
    rap_type rap{};
    logical found = 0;
    integer free = 0;
    findlb(&iap, &rap, 7, &free, &found);
    CHECK(found && free == 1);
    std::array<double, 2> u{};
    std::vector<double> par(lib.parameter_count, canary);
    std::string failed;
    try {
        readlb(&iap, &rap, u.data(), par.data());
    } catch (const xpp::AutoFailed &e) {
        failed = e.error.what;
    }
    /* a short read ends the run, naming the label and what is missing */
    CHECK(failed.empty() != truncated);
    if (truncated) CHECK(failed.find("label 7") != std::string::npos && failed.find("ends before the parameter values") != std::string::npos);
    if (!truncated) {
        CHECK(u == (std::array<double, 2>{2., 3.}));
        CHECK(par.back() == lib.parameter_count - 1 + .125);
    }
}
// The UZR routines differ only in diagnostic width; preserve the signed
// index, full attempted point number and emitted text, not just the value.
void user_zero_text(xpp::Session &s) {
    xpp::TempDir temp;
    const std::string path = temp.path() + "/fort.9";
    xpp::UniqueFile file(xpp::files::open_stream(path, "w+"));
    AutoLib lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    lib.fp9 = file.get();
    iap_type iap{};
    iap.lib = &lib;
    iap.iid = 3;
    iap.ibr = -3;
    iap.ntot = 10000;
    iap.nfpr = 1;
    rap_type rap{};
    integer icp[] = {xpp::auto_unknown::period};
    const AutoMark marks[] = {{-xpp::auto_unknown::period, .125}};
    std::vector<double> par(lib.parameter_count);
    par[xpp::auto_unknown::period] = .875;
    logical changed = 0;
    CHECK(fnuzae(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 marks) == .75);
    CHECK(changed == TRUE_);
    changed = 0;
    CHECK(fnuzbv(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, marks) == .75);
    CHECK(changed == TRUE_);
    CHECK(xpp::rewind_stream(file.get(), path).has_value());
    auto reader = xpp::LineReader::attach(file.get());
    CHECK(reader.next() == "   3 10001        User Func.   0     7.500000E-01");
    CHECK(reader.next() == "   3 10001        User Func.   0   7.500000E-01");
    CHECK(!reader.next());
}

// W384 (#446): exercise the real writers at the wrap and beyond their old
// column widths, without computing thousands of continuation points.
void point_files(xpp::Session &s, int count, bool collocation) {
    xpp::TempDir temp;
    auto &lib = s.auto_lib;
    lib.session = &s;
    lib.parameter_count = s.auto_state.parameter_layout.size();
    const std::string solution_path = temp.path() + "/fort.8";
    lib.fp8.reset(xpp::files::open_stream(solution_path, "w+"));
    CHECK(lib.fp8 != nullptr);
    iap_type iap{};
    iap.lib = &lib;
    iap.ibr = 3;
    iap.ntot = count;
    iap.lab = 7;
    iap.nfpr = iap.nicp = iap.ndim = iap.ndm = 1;
    iap.isw = 1;
    iap.itp = {xpp::AutoPointKind::End, xpp::AutoPointKind::Regular};
    iap.ntst = collocation ? 1 : 0;
    iap.ncol = collocation ? 2 : 0;
    rap_type rap{};
    std::array<integer, 1> icp = {s.auto_state.parameter_layout.solver_index(0)};
    std::vector<double> par(lib.parameter_count);
    std::array<double, 4> values = {2., 3., 4., 5.}, tangent = {.5, .5, .5, .5};
    std::array<double, 2> times = {0., 1.};
    double current = .25, direction = 1., interval = 1.;
    integer stride = 2;
    if (collocation) {
        CHECK(wrtbv8(&iap, &rap, par.data(), icp.data(), &direction, &stride,
                      values.data(), tangent.data(), times.data(), &interval) == 0);
    } else {
        CHECK(wrtsp8(&iap, &rap, par.data(), icp.data(), &iap.lab, &current, values.data()) == 0);
    }
    CHECK(xpp::rewind_stream(lib.fp8.get(), solution_path).has_value());
    auto solutions = xpp::TokenReader::attach(lib.fp8.get());
    xpp::AutoSolutionHeader header;
    CHECK(xpp::read_auto_solution_header(solutions, header, lib.parameter_count));
    CHECK(header.ibr == 3 && header.ntot == count && header.lab == 7);
    CHECK(header.itp == iap.itp && header.ntst == iap.ntst && header.ncol == iap.ncol);
    solutions.close();
    lib.fp8.reset();

    const std::string diagram_path = temp.path() + "/fort.7";
    const std::string diagnostic_path = temp.path() + "/fort.9";
    xpp::UniqueFile diagram(xpp::files::open_stream(diagram_path, "w+"));
    xpp::UniqueFile diagnostic(xpp::files::open_stream(diagnostic_path, "w+"));
    CHECK(diagram != nullptr && diagnostic != nullptr);
    lib.fp7 = diagram.get();
    lib.fp9 = diagnostic.get();
    // Both signs matter: fort.7 uses the sign to encode stability.
    for (integer point : {static_cast<integer>(count), -static_cast<integer>(count)})
        CHECK(wrline(&iap, &rap, par.data(), icp.data(), icp.data(), &iap.ibr,
                     &point, &iap.lab, &current, values.data()) == 0);
    CHECK(xpp::rewind_stream(diagram.get(), diagram_path).has_value());
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
    lib.fp7 = lib.fp9 = nullptr;
    lib.session = nullptr;
}
} // namespace

int main() {
    char name[] = "test_auto_restart";
    char model[] = "examples/ode/lecar.odex";
    char *argv[] = {name, model, nullptr};
    CHECK(xpp::load_model(2, argv, 1).has_value());
    xpp::Session &s = xpp::client_session();
    xpp::init_auto_win(s);
    // Directly cover each audit-named augmented fort.8 formulation.
    for (bool truncated : {false, true}) {
        formulation(s, stpnpl, 4, 4, false, false, false, truncated);
        formulation(s, stpnpd, 4, 3, false, false, false, truncated);
        formulation(s, stpntr, 6, 4, false, true, false, truncated);
        formulation(s, stpnbl, 4, 2, true, false, false, truncated);
        formulation(s, stpnpo, 4, 4, false, false, true, truncated);
        algebraic(s, truncated);
    }
    user_zero_text(s);
    for (int count : {9999, 10000, 10001, 100000, std::numeric_limits<int>::max()})
        for (bool collocation : {false, true}) point_files(s, count, collocation);
    TEST_REPORT("AUTO restart families");
}
