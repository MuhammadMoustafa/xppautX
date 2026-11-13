/* autox_codes: autoX's problem (IPS) and point (ITP) codes as types (W329,
   #390). Every supported problem and point kind round-trips through its
   integer, the signed and composite forms of ITP included, an unknown
   integer is refused, and the predicates give exactly what the integer
   expressions they replaced gave. */
#include "xpptest.h"
#include "autox_codes.h"
#include "autox_nox.h"
#include "xpp_io.h"

#include <array>

namespace {

struct ProblemCase {
    int code;
    xpp::AutoxProblem problem;
};

constexpr std::array<ProblemCase, 15> problems = {{
    {-2, xpp::AutoxProblem::TimeIntegration},
    {-1, xpp::AutoxProblem::DiscreteMap},
    {0, xpp::AutoxProblem::Algebraic},
    {1, xpp::AutoxProblem::SteadyState},
    {2, xpp::AutoxProblem::Periodic},
    {4, xpp::AutoxProblem::BoundaryValue},
    {5, xpp::AutoxProblem::AlgebraicOptimization},
    {7, xpp::AutoxProblem::FloquetBoundaryValue},
    {9, xpp::AutoxProblem::Homoclinic},
    {11, xpp::AutoxProblem::WaveSteady},
    {12, xpp::AutoxProblem::WavePeriodic},
    {14, xpp::AutoxProblem::ParabolicPeriodicEvolution},
    {15, xpp::AutoxProblem::PeriodicOptimization},
    {16, xpp::AutoxProblem::ParabolicUserEvolution},
    {17, xpp::AutoxProblem::ParabolicUserSteady},
}};

constexpr std::array<xpp::AutoxPointKind, 13> kinds = {
    xpp::AutoxPointKind::Pending,       xpp::AutoxPointKind::Regular,        xpp::AutoxPointKind::BranchPoint,
    xpp::AutoxPointKind::Fold,          xpp::AutoxPointKind::Hopf,           xpp::AutoxPointKind::Output,
    xpp::AutoxPointKind::CycleFold,  xpp::AutoxPointKind::CycleBranch, xpp::AutoxPointKind::PeriodDoubling,
    xpp::AutoxPointKind::NeimarkSacker,         xpp::AutoxPointKind::End,            xpp::AutoxPointKind::UserZero,
    xpp::AutoxPointKind::MaxSteps,
};

int plain_kind(long itp) { return static_cast<int>(itp % 10); }

}

TEST_CASE("test_autox_codes")
{
    /* problems: each supported code maps to its type and back; the rest are refused */
    for (const ProblemCase &c : problems) {
        const auto p = xpp::autox_problem_from_code(c.code);
        CHECK(p && *p == c.problem);
        CHECK(xpp::autox_problem_code(c.problem) == c.code);
    }
    for (int bad : {-3, 3, 6, 8, 10, 13, 18, 100, -100})
        CHECK(!xpp::autox_problem_from_code(bad));

    /* the groups the autoX sources tested with integer ranges */
    for (long ips = -20; ips <= 20; ips++) {
        const auto p = xpp::autox_problem_from_code(ips);
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
    for (xpp::AutoxPointKind kind : kinds) {
        for (xpp::AutoxPointKind start : kinds) {
            if (static_cast<int>(start) < 0) continue; /* a start is a kind 0..9 */
            if (kind == xpp::AutoxPointKind::Pending && start != xpp::AutoxPointKind::Regular) continue;
            const xpp::AutoxPointCode code{kind, start};
            const int itp = xpp::autox_point_encode(code);
            const auto back = xpp::autox_point_decode(itp, kind == xpp::AutoxPointKind::Pending);
            if (itp==23 || itp==28 || itp==77 || itp==78 || itp==83 || itp==87 || itp==88) continue; // reserved map intersections
            CHECK(back && *back == code);
            /* the autoX integer: 10*start + kind, and -4 / -9 - 10*start for UZ and MX */
            CHECK(itp == (static_cast<int>(kind) < 0 ? static_cast<int>(kind) - 10 * static_cast<int>(start)
                                                     : 10 * static_cast<int>(start) + static_cast<int>(kind)));
            valid++;
        }
    }
    CHECK(valid > 0);

    /* signed codes: UZ and MX, plain and started from a type */
    CHECK(xpp::autox_point_decode(-4) && xpp::autox_point_decode(-4)->kind == xpp::AutoxPointKind::UserZero);
    CHECK(xpp::autox_point_decode(-9) && xpp::autox_point_decode(-9)->kind == xpp::AutoxPointKind::MaxSteps);
    CHECK(xpp::autox_point_encode({xpp::AutoxPointKind::UserZero, xpp::AutoxPointKind::Hopf}) == -34);
    CHECK(xpp::autox_point_encode({xpp::AutoxPointKind::MaxSteps, xpp::AutoxPointKind::NeimarkSacker}) == -89);
    {
        const auto uz = xpp::autox_point_decode(-34);
        CHECK(uz && uz->kind == xpp::AutoxPointKind::UserZero && uz->start == xpp::AutoxPointKind::Hopf);
        const auto mx = xpp::autox_point_decode(-89);
        CHECK(mx && mx->kind == xpp::AutoxPointKind::MaxSteps && mx->start == xpp::AutoxPointKind::NeimarkSacker);
    }
    /* composite codes: 10*start + kind */
    {
        const auto pd = xpp::autox_point_decode(27); /* a period doubling found continuing a fold */
        CHECK(pd && pd->kind == xpp::AutoxPointKind::PeriodDoubling && pd->start == xpp::AutoxPointKind::Fold);
        CHECK(!pd->is(xpp::AutoxPointKind::PeriodDoubling) && pd->started_from(xpp::AutoxPointKind::Fold));
        const auto plain = xpp::autox_point_decode(7);
        CHECK(plain && plain->is(xpp::AutoxPointKind::PeriodDoubling) && !plain->started_from(xpp::AutoxPointKind::Fold));
    }
    /* a stored value is never the run's pending -1 */
    CHECK(!xpp::autox_point_decode(-1));
    CHECK(xpp::autox_point_decode(-1, true) && xpp::autox_point_decode(-1, true)->kind == xpp::AutoxPointKind::Pending);

    for (int value : {-21,-22,-23,-31,-32,-33,-55,-56,-57,-58,-75,-76,-77,-78,-85,-86,-87,-88}) {
        const auto point=xpp::autox_point_decode(value);
        CHECK(point && xpp::autox_point_encode(*point)==value);
        CHECK(xpp::autox_point_from_text(xpp::autox_point_text(*point)) == point);
    }
    for (int value : {23,28,77,78,83,87,88,100,101,102,-25,-26,-27,-28}) {
        const auto point=xpp::autox_point_decode(value);
        CHECK(point && xpp::autox_point_encode(*point)==value);
    }
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(23)->kind) == "fold_neimark_sacker");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(78)->kind) == "fold_flip");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(87)->kind) == "flip_neimark_sacker");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(88)->kind) == "double_neimark_sacker");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-32)->kind) == "generalized_hopf");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-22)->kind) == "cusp");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-55)->kind) == "resonance_1_1");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-76)->kind) == "resonance_1_2");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-87)->kind) == "resonance_1_3");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(-88)->kind) == "resonance_1_4");
    /* W518: the ends of a periodic branch only xppautX names, outside the composite range 0..99 */
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(100)->kind) == "snic");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(101)->kind) == "homoclinic");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(102)->kind) == "heteroclinic");
    CHECK(xpp::autox_point_kind_label(xpp::AutoxPointKind::Snic) == "SNIC (saddle-node on invariant circle)");
    CHECK(xpp::autox_point_kind_mark(xpp::AutoxPointKind::Homoclinic) == "HOC" && xpp::autox_point_kind_mark(xpp::AutoxPointKind::Heteroclinic) == "HEC");
    CHECK(xpp::autox_point_text(*xpp::autox_point_decode(101)) == "homoclinic");
    /* W604: the end at a Hopf point and the branch closed on itself */
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(103)->kind) == "hopf_end");
    CHECK(xpp::autox_point_kind_name(xpp::autox_point_decode(104)->kind) == "branch_closed");
    CHECK(xpp::autox_point_encode({xpp::AutoxPointKind::HopfEnd, xpp::AutoxPointKind::Regular}) == 103);
    CHECK(xpp::autox_point_encode({xpp::AutoxPointKind::BranchClosed, xpp::AutoxPointKind::Regular}) == 104);
    CHECK(xpp::autox_point_kind_label(xpp::AutoxPointKind::BranchClosed) == "Branch closed");
    CHECK(xpp::autox_point_kind_mark(xpp::AutoxPointKind::HopfEnd) == "HBE" && xpp::autox_point_kind_mark(xpp::AutoxPointKind::BranchClosed) == "CL");

    /* an unknown code is refused: out of range, a negative kind autoX has no
       such point for, a negative composite of a plain kind */
    for (long bad : {105L, -100L, 1000L, -5L, -3L, -2L, -11L, -10L, -20L, -99L - 1, -65L})
        CHECK(!xpp::autox_point_decode(bad));
    CHECK_STR(xpp::autox_point_unknown(55 + 100).c_str(), "155 is not an autoX point type");

    /* decoding gives what autoX's integer expressions gave, on every legal code */
    for (long itp = -99; itp <= 99; itp++) {
        const auto code = xpp::autox_point_decode(itp);
        if (!code || static_cast<int>(code->kind)>9) continue;
        CHECK(static_cast<int>(code->kind) == plain_kind(itp));              /* itp % 10 */
        const long magnitude = itp < 0 ? -itp : itp;
        CHECK(static_cast<int>(code->start) == magnitude / 10);              /* abs(itp) / 10 */
        CHECK((code->start == xpp::AutoxPointKind::Regular) == (magnitude < 10)); /* abs(itp) < 10 */
        for (int k = 1; k <= 9; k++) {
            const auto kind = static_cast<xpp::AutoxPointKind>(k);
            CHECK(code->is(kind) == (itp == k));
            CHECK(code->started_from(kind) == (magnitude / 10 == k));
        }
        const auto start = xpp::autox_point_start_of(*code);
        CHECK(static_cast<long>(start) == (magnitude < 10 ? magnitude : magnitude / 10)); /* find_restart_label's itpst */
    }

    /* the type names the diagram's type column and the files show */
    struct NameCase { long itp; const char *name; };
    for (const NameCase &c : {NameCase{0, "regular"}, {1, "branch_point"}, {2, "fold"}, {3, "hopf"}, {4, "output"},
                              {-4, "user_point"}, {5, "cycle_fold"}, {6, "cycle_branch_point"}, {7, "period_doubling"},
                              {8, "neimark_sacker"}, {9, "end"}, {-9, "no_convergence"}, {27, "period_doubling"}, {-34, "user_point"},
                              {-89, "no_convergence"}}) {
        const auto code = xpp::autox_point_decode(c.itp);
        CHECK(code);
        if (code) CHECK(xpp::autox_point_kind_name(code->kind) == c.name);
    }
    /* every kind has one name, and each name reads back as its kind (Pending is named for display, never read) */
    for (const xpp::AutoxPointName &e : xpp::autox_point_names) {
        CHECK(xpp::autox_point_kind_name(e.kind) == e.name);
        if (e.kind == xpp::AutoxPointKind::Pending) CHECK(!xpp::autox_point_kind_from_name(e.name));
        else CHECK(xpp::autox_point_kind_from_name(e.name) == e.kind);
    }
    CHECK(xpp::autox_point_kind_name(xpp::AutoxPointKind::MaxSteps) == "no_convergence");
    CHECK(xpp::autox_point_kind_name(xpp::AutoxPointKind::UserZero) == "user_point");
    /* the codimension-two keys are the names of their kinds, in the settings' order */
    CHECK(xpp::autox_codim_keys[0] == "bogdanov_takens" && xpp::autox_codim_keys[11] == "double_neimark_sacker");
    CHECK(xpp::autox_codim_key_list().find("zero_hopf, generalized_hopf") != std::string::npos);
    /* a file's text: a name, or a name from another kind (a two-parameter start), and only a code autoX has */
    CHECK(xpp::autox_point_text(*xpp::autox_point_decode(78)) == "fold_flip_from_period_doubling");
    CHECK(xpp::autox_point_text(*xpp::autox_point_decode(-34)) == "user_point_from_hopf");
    CHECK(xpp::autox_point_text(*xpp::autox_point_decode(7)) == "period_doubling");
    CHECK(xpp::autox_point_from_text("fold_flip_from_period_doubling") == xpp::autox_point_decode(78));
    CHECK(!xpp::autox_point_from_text("HB") && !xpp::autox_point_from_text("LP") && !xpp::autox_point_from_text("pending"));
    CHECK(!xpp::autox_point_from_text("fold_from_") && !xpp::autox_point_from_text("fold_from_pending") && !xpp::autox_point_from_text(""));
    CHECK(xpp::autox_point_from_text("regular_from_fold") == xpp::autox_point_decode(20));
    /* the label a person reads: the name with its underscores as spaces and a capital first letter (W381) */

    /* the diagram classification's protocol number */
    CHECK(xpp::diagram_point_type_code(xpp::STABLE_EQ) == 1 && xpp::diagram_point_type_code(xpp::UNSTABLE_PERIODIC) == 4);
    CHECK(xpp::point_is_stable(xpp::STABLE_PERIODIC) && !xpp::point_is_stable(xpp::UNSTABLE_EQ));
    CHECK(xpp::point_is_periodic(xpp::UNSTABLE_PERIODIC) && !xpp::point_is_periodic(xpp::STABLE_EQ));

    /* fort.8's header: its point type is decoded as it is read, an unknown one is a bad header */
    {
        xpp::AutoxSolutionHeader h;
        xpp::TokenReader good = xpp::TokenReader::of_text("1 1 -34 1 1 1 5 3 5 0 0 2");
        CHECK(xpp::read_autox_solution_header(good, h, 10) && h.itp.kind == xpp::AutoxPointKind::UserZero
              && h.itp.start == xpp::AutoxPointKind::Hopf);
        for (const char *text : {"1 1 -5 1 1 1 5 3 5 0 0 2", "1 1 105 1 1 1 5 3 5 0 0 2", "1 1 -1 1 1 1 5 3 5 0 0 2"}) {
            xpp::TokenReader bad = xpp::TokenReader::of_text(text);
            CHECK(!xpp::read_autox_solution_header(bad, h, 10));
        }
    }

    TEST_REPORT("autox_codes");
}
