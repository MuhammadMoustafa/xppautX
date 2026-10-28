/* auto_codes: AUTO's problem (IPS) and point (ITP) codes as types (W329,
   #390). Every supported problem and point kind round-trips through its
   integer, the signed and composite forms of ITP included, an unknown
   integer is refused, and the predicates give exactly what the integer
   expressions they replaced gave. */
#include "xpptest.h"
#include "auto_codes.h"
#include "auto_nox.h"
#include "xpp_io.h"

#include <array>

namespace {

struct ProblemCase {
    int code;
    xpp::AutoProblem problem;
};

constexpr std::array<ProblemCase, 15> problems = {{
    {-2, xpp::AutoProblem::TimeIntegration},
    {-1, xpp::AutoProblem::DiscreteMap},
    {0, xpp::AutoProblem::Algebraic},
    {1, xpp::AutoProblem::SteadyState},
    {2, xpp::AutoProblem::Periodic},
    {4, xpp::AutoProblem::BoundaryValue},
    {5, xpp::AutoProblem::AlgebraicOptimization},
    {7, xpp::AutoProblem::FloquetBoundaryValue},
    {9, xpp::AutoProblem::Homoclinic},
    {11, xpp::AutoProblem::WaveSteady},
    {12, xpp::AutoProblem::WavePeriodic},
    {14, xpp::AutoProblem::ParabolicPeriodicEvolution},
    {15, xpp::AutoProblem::PeriodicOptimization},
    {16, xpp::AutoProblem::ParabolicUserEvolution},
    {17, xpp::AutoProblem::ParabolicUserSteady},
}};

constexpr std::array<xpp::AutoPointKind, 13> kinds = {
    xpp::AutoPointKind::Pending,       xpp::AutoPointKind::Regular,        xpp::AutoPointKind::BranchPoint,
    xpp::AutoPointKind::Fold,          xpp::AutoPointKind::Hopf,           xpp::AutoPointKind::Output,
    xpp::AutoPointKind::CycleFold,  xpp::AutoPointKind::CycleBranch, xpp::AutoPointKind::PeriodDoubling,
    xpp::AutoPointKind::NeimarkSacker,         xpp::AutoPointKind::End,            xpp::AutoPointKind::UserZero,
    xpp::AutoPointKind::MaxSteps,
};

int plain_kind(long itp) { return static_cast<int>(itp % 10); }

}

int main()
{
    /* problems: each supported code maps to its type and back; the rest are refused */
    for (const ProblemCase &c : problems) {
        const auto p = xpp::auto_problem_from_code(c.code);
        CHECK(p && *p == c.problem);
        CHECK(xpp::auto_problem_code(c.problem) == c.code);
    }
    for (int bad : {-3, 3, 6, 8, 10, 13, 18, 100, -100})
        CHECK(!xpp::auto_problem_from_code(bad));

    /* the groups the AUTO sources tested with integer ranges */
    for (long ips = -20; ips <= 20; ips++) {
        const auto p = xpp::auto_problem_from_code(ips);
        if (!p) continue;
        CHECK(xpp::problem_is_steady_or_map(*p) == (ips == 1 || ips == -1)); /* abs(ips) == 1 */
        CHECK(xpp::problem_is_algebraic_family(*p) == (ips >= -1 && ips <= 1)); /* abs(ips) <= 1 */
        CHECK(xpp::problem_at_most_steady(*p) == (ips <= 1)); /* ips <= 1 */
        CHECK(xpp::problem_has_orbit_multipliers(*p) == (ips == 2 || ips == 7 || ips == 12));
        CHECK(xpp::problem_is_parabolic_evolution(*p) == (ips == 14 || ips == 16));
        CHECK(xpp::problem_is_optimization(*p) == (ips == 5 || ips == 15));
        CHECK(xpp::problem_has_solution_profile(*p) == ((ips >= 2 && ips <= 4) || (ips >= 6 && ips <= 9) || (ips >= 12 && ips <= 17)));
        CHECK(xpp::problem_heads_period(*p) == (ips > 0 && ips != 4));
    }

    /* point codes: every plain kind, and every kind started from every kind, round-trips */
    int valid = 0;
    for (xpp::AutoPointKind kind : kinds) {
        for (xpp::AutoPointKind start : kinds) {
            if (static_cast<int>(start) < 0) continue; /* a start is a kind 0..9 */
            if (kind == xpp::AutoPointKind::Pending && start != xpp::AutoPointKind::Regular) continue;
            const xpp::AutoPointCode code{kind, start};
            const int itp = xpp::auto_point_encode(code);
            const auto back = xpp::auto_point_decode(itp, kind == xpp::AutoPointKind::Pending);
            if (itp==23 || itp==28 || itp==77 || itp==78 || itp==83 || itp==87 || itp==88) continue; // reserved map intersections
            CHECK(back && *back == code);
            /* the AUTO integer: 10*start + kind, and -4 / -9 - 10*start for UZ and MX */
            CHECK(itp == (static_cast<int>(kind) < 0 ? static_cast<int>(kind) - 10 * static_cast<int>(start)
                                                     : 10 * static_cast<int>(start) + static_cast<int>(kind)));
            valid++;
        }
    }
    CHECK(valid > 0);

    /* signed codes: UZ and MX, plain and started from a type */
    CHECK(xpp::auto_point_decode(-4) && xpp::auto_point_decode(-4)->kind == xpp::AutoPointKind::UserZero);
    CHECK(xpp::auto_point_decode(-9) && xpp::auto_point_decode(-9)->kind == xpp::AutoPointKind::MaxSteps);
    CHECK(xpp::auto_point_encode({xpp::AutoPointKind::UserZero, xpp::AutoPointKind::Hopf}) == -34);
    CHECK(xpp::auto_point_encode({xpp::AutoPointKind::MaxSteps, xpp::AutoPointKind::NeimarkSacker}) == -89);
    {
        const auto uz = xpp::auto_point_decode(-34);
        CHECK(uz && uz->kind == xpp::AutoPointKind::UserZero && uz->start == xpp::AutoPointKind::Hopf);
        const auto mx = xpp::auto_point_decode(-89);
        CHECK(mx && mx->kind == xpp::AutoPointKind::MaxSteps && mx->start == xpp::AutoPointKind::NeimarkSacker);
    }
    /* composite codes: 10*start + kind */
    {
        const auto pd = xpp::auto_point_decode(27); /* a period doubling found continuing a fold */
        CHECK(pd && pd->kind == xpp::AutoPointKind::PeriodDoubling && pd->start == xpp::AutoPointKind::Fold);
        CHECK(!pd->is(xpp::AutoPointKind::PeriodDoubling) && pd->started_from(xpp::AutoPointKind::Fold));
        const auto plain = xpp::auto_point_decode(7);
        CHECK(plain && plain->is(xpp::AutoPointKind::PeriodDoubling) && !plain->started_from(xpp::AutoPointKind::Fold));
    }
    /* a stored value is never the run's pending -1 */
    CHECK(!xpp::auto_point_decode(-1));
    CHECK(xpp::auto_point_decode(-1, true) && xpp::auto_point_decode(-1, true)->kind == xpp::AutoPointKind::Pending);

    for (int value : {-21,-22,-23,-31,-32,-33,-55,-56,-57,-58,-75,-76,-77,-78,-85,-86,-87,-88}) {
        const auto point=xpp::auto_point_decode(value);
        CHECK(point && xpp::auto_point_encode(*point)==value);
        CHECK(xpp::auto_point_from_text(xpp::auto_point_text(*point)) == point);
    }
    for (int value : {23,28,77,78,83,87,88,-25,-26,-27,-28}) {
        const auto point=xpp::auto_point_decode(value);
        CHECK(point && xpp::auto_point_encode(*point)==value);
    }
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(23)->kind) == "fold_neimark_sacker");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(78)->kind) == "fold_flip");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(87)->kind) == "flip_neimark_sacker");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(88)->kind) == "double_neimark_sacker");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-32)->kind) == "generalized_hopf");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-22)->kind) == "cusp");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-55)->kind) == "resonance_1_1");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-76)->kind) == "resonance_1_2");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-87)->kind) == "resonance_1_3");
    CHECK(xpp::auto_point_kind_name(xpp::auto_point_decode(-88)->kind) == "resonance_1_4");

    /* an unknown code is refused: out of range, a negative kind AUTO has no
       such point for, a negative composite of a plain kind */
    for (long bad : {100L, -100L, 1000L, -5L, -3L, -2L, -11L, -10L, -20L, -99L - 1, -65L})
        CHECK(!xpp::auto_point_decode(bad));
    CHECK_STR(xpp::auto_point_unknown(55 + 100).c_str(), "155 is not an AUTO point type");

    /* decoding gives what AUTO's integer expressions gave, on every legal code */
    for (long itp = -99; itp <= 99; itp++) {
        const auto code = xpp::auto_point_decode(itp);
        if (!code || static_cast<int>(code->kind)>9) continue;
        CHECK(static_cast<int>(code->kind) == plain_kind(itp));              /* itp % 10 */
        const long magnitude = itp < 0 ? -itp : itp;
        CHECK(static_cast<int>(code->start) == magnitude / 10);              /* abs(itp) / 10 */
        CHECK((code->start == xpp::AutoPointKind::Regular) == (magnitude < 10)); /* abs(itp) < 10 */
        for (int k = 1; k <= 9; k++) {
            const auto kind = static_cast<xpp::AutoPointKind>(k);
            CHECK(code->is(kind) == (itp == k));
            CHECK(code->started_from(kind) == (magnitude / 10 == k));
        }
        const auto start = xpp::auto_point_start_of(*code);
        CHECK(static_cast<long>(start) == (magnitude < 10 ? magnitude : magnitude / 10)); /* findlb's itpst */
    }

    /* the type names the diagram's type column and the files show */
    struct NameCase { long itp; const char *name; };
    for (const NameCase &c : {NameCase{0, "regular"}, {1, "branch_point"}, {2, "fold"}, {3, "hopf"}, {4, "output"},
                              {-4, "user_point"}, {5, "cycle_fold"}, {6, "cycle_branch_point"}, {7, "period_doubling"},
                              {8, "neimark_sacker"}, {9, "end"}, {-9, "no_convergence"}, {27, "period_doubling"}, {-34, "user_point"},
                              {-89, "no_convergence"}}) {
        const auto code = xpp::auto_point_decode(c.itp);
        CHECK(code);
        if (code) CHECK(xpp::auto_point_kind_name(code->kind) == c.name);
    }
    /* every kind has one name, and each name reads back as its kind (Pending is named for display, never read) */
    for (const xpp::AutoPointName &e : xpp::auto_point_names) {
        CHECK(xpp::auto_point_kind_name(e.kind) == e.name);
        if (e.kind == xpp::AutoPointKind::Pending) CHECK(!xpp::auto_point_kind_from_name(e.name));
        else CHECK(xpp::auto_point_kind_from_name(e.name) == e.kind);
    }
    CHECK(xpp::auto_point_kind_name(xpp::AutoPointKind::MaxSteps) == "no_convergence");
    CHECK(xpp::auto_point_kind_name(xpp::AutoPointKind::UserZero) == "user_point");
    /* the codimension-two keys are the names of their kinds, in the settings' order */
    CHECK(xpp::auto_codim_keys[0] == "bogdanov_takens" && xpp::auto_codim_keys[11] == "double_neimark_sacker");
    CHECK(xpp::auto_codim_key_list().find("zero_hopf, generalized_hopf") != std::string::npos);
    /* a file's text: a name, or a name from another kind (a two-parameter start), and only a code AUTO has */
    CHECK(xpp::auto_point_text(*xpp::auto_point_decode(78)) == "fold_flip_from_period_doubling");
    CHECK(xpp::auto_point_text(*xpp::auto_point_decode(-34)) == "user_point_from_hopf");
    CHECK(xpp::auto_point_text(*xpp::auto_point_decode(7)) == "period_doubling");
    CHECK(xpp::auto_point_from_text("fold_flip_from_period_doubling") == xpp::auto_point_decode(78));
    CHECK(!xpp::auto_point_from_text("HB") && !xpp::auto_point_from_text("LP") && !xpp::auto_point_from_text("pending"));
    CHECK(!xpp::auto_point_from_text("fold_from_") && !xpp::auto_point_from_text("fold_from_pending") && !xpp::auto_point_from_text(""));
    CHECK(xpp::auto_point_from_text("regular_from_fold") == xpp::auto_point_decode(20));
    /* the label a person reads: the name with its underscores as spaces and a capital first letter (W381) */

    /* the diagram classification's protocol number */
    CHECK(xpp::diagram_point_type_code(xpp::STABLE_EQ) == 1 && xpp::diagram_point_type_code(xpp::UNSTABLE_PERIODIC) == 4);
    CHECK(xpp::point_is_stable(xpp::STABLE_PERIODIC) && !xpp::point_is_stable(xpp::UNSTABLE_EQ));
    CHECK(xpp::point_is_periodic(xpp::UNSTABLE_PERIODIC) && !xpp::point_is_periodic(xpp::STABLE_EQ));

    /* fort.8's header: its point type is decoded as it is read, an unknown one is a bad header */
    {
        xpp::AutoSolutionHeader h;
        xpp::TokenReader good = xpp::TokenReader::of_text("1 1 -34 1 1 1 5 3 5 0 0 2");
        CHECK(xpp::read_auto_solution_header(good, h, 10) && h.itp.kind == xpp::AutoPointKind::UserZero
              && h.itp.start == xpp::AutoPointKind::Hopf);
        for (const char *text : {"1 1 -5 1 1 1 5 3 5 0 0 2", "1 1 100 1 1 1 5 3 5 0 0 2", "1 1 -1 1 1 1 5 3 5 0 0 2"}) {
            xpp::TokenReader bad = xpp::TokenReader::of_text(text);
            CHECK(!xpp::read_auto_solution_header(bad, h, 10));
        }
    }

    TEST_REPORT("auto_codes");
}
