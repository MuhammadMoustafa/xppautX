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
    iap.ips = 2;
    iap.isw = 2;
    rap_type rap{};
    std::array<integer, 4> icp = {10, 11, 10, 11};
    std::vector<double> par(lib.parameter_count, canary);
    std::vector<double> ups(3 * dimension * 2, canary);
    std::vector<double> tangent(ups.size(), canary), old(ups.size(), canary);
    std::array<double, 4> current{}, direction{};
    std::array<double, 3> tm{}, dtm{}, thl{}, thu{};
    integer ntst = 0, ncol = 0, stride = 3, nodir = -2;
    const int result = restart(&iap, &rap, par.data(), icp.data(), &ntst, &ncol,
                              current.data(), direction.data(), &stride,
                              ups.data(), tangent.data(), old.data(), tm.data(),
                              dtm.data(), &nodir, thl.data(), thu.data());
    CHECK(result == (truncated ? 1 : 0));
    if (truncated) return;
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
    rap_type rap{};
    logical found = 0;
    integer free = 0;
    findlb(&iap, &rap, 7, &free, &found);
    CHECK(found && free == 1);
    std::array<double, 2> u{};
    std::vector<double> par(lib.parameter_count, canary);
    CHECK(readlb(&iap, &rap, u.data(), par.data()) == (truncated ? 1 : 0));
    if (!truncated) {
        CHECK(u == (std::array<double, 2>{2., 3.}));
        CHECK(par.back() == lib.parameter_count - 1 + .125);
    }
}
// The UZR routines differ only in diagnostic width; preserve the signed
// index, point-number wrap and the actual emitted text, not just the value.
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
    integer iuz[] = {-xpp::auto_unknown::period};
    double vuz[] = {.125};
    std::vector<double> par(lib.parameter_count);
    par[xpp::auto_unknown::period] = .875;
    logical changed = 0;
    CHECK(fnuzae(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 iuz, vuz) == .75);
    CHECK(changed == TRUE_);
    changed = 0;
    CHECK(fnuzbv(&iap, &rap, par.data(), icp, &changed,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                 nullptr, nullptr, nullptr, iuz, vuz) == .75);
    CHECK(changed == TRUE_);
    CHECK(xpp::rewind_stream(file.get(), path).has_value());
    auto reader = xpp::LineReader::attach(file.get());
    CHECK(reader.next() == "   3     1        User Func.   0     7.500000E-01");
    CHECK(reader.next() == "   3     1        User Func.   0   7.500000E-01");
    CHECK(!reader.next());
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
    TEST_REPORT("AUTO restart families");
}
