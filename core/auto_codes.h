#ifndef AUTO_CODES_H
#define AUTO_CODES_H

/* The one owner of AUTO's problem and point-type codes (W329, #390):
   what AUTO's IPS and ITP integers mean, as types. Integers stay only at
   the explicit boundaries: AUTO's fort files and the diagram's saved
   columns (auto_problem_from_code / auto_point_decode on the way in, the
   _code / auto_point_encode on the way out), where a code that is none of
   these is refused by the reader with its file and line. The diagram's
   stable/periodic classification is DiagramPointType below and the
   stability values' kind auto_stability.h's AutoStabilityKind. */

#include <array>
#include <optional>
#include <string>

#include "xpp_io.h"

namespace xpp {

/* A diagram point's type (get_bif_type, autevd.cpp, from the signs AUTO
   gives its branch and point numbers; a run's `ty` in docs/protocol.md's
   `diagram`, `type` in `autoinfo`): a steady state, judged by its
   eigenvalues, or a periodic orbit, by its Floquet multipliers; stable
   or not. The events carry stable and periodic flags beside it. It lives
   here, not in diagram.h, so that the headers a point's type crosses
   (autevd.h, auto_nox.h, auto_data.h, xpp_ui.h) can name it. */
enum DiagramPointType : int { STABLE_EQ = 1, UNSTABLE_EQ = 2, STABLE_PERIODIC = 3, UNSTABLE_PERIODIC = 4 };
constexpr bool point_is_stable(DiagramPointType type) { return type == STABLE_EQ || type == STABLE_PERIODIC; }
constexpr bool point_is_periodic(DiagramPointType type) { return type == STABLE_PERIODIC || type == UNSTABLE_PERIODIC; }
/* the protocol's `ty`/`type` number (docs/protocol.md): the explicit encoding of the type */
constexpr int diagram_point_type_code(DiagramPointType type) { return static_cast<int>(type); }

/* AUTO's IPS: the kind of problem a run solves. Only these have a function
   table (gogoauto.cpp set_function_pointers); the code is AUTO's own number
   (AUTO-07p manual, IPS). */
enum class AutoProblem : int {
    TimeIntegration = -2,   /* IPS -2 */
    DiscreteMap = -1,       /* IPS -1: fixed points of a map */
    Algebraic = 0,          /* IPS 0 */
    SteadyState = 1,        /* IPS 1: stationary solutions of an ODE */
    Periodic = 2,           /* IPS 2: periodic solutions */
    BoundaryValue = 4,      /* IPS 4 */
    AlgebraicOptimization = 5, /* IPS 5 */
    FloquetBoundaryValue = 7,  /* IPS 7: BVP with Floquet multipliers */
    Homoclinic = 9,         /* IPS 9 */
    WaveSteady = 11,        /* IPS 11: spatially homogeneous waves */
    WavePeriodic = 12,      /* IPS 12: wave trains */
    ParabolicPeriodicEvolution = 14,   /* IPS 14: periodic boundary conditions */
    PeriodicOptimization = 15,         /* IPS 15 */
    ParabolicUserEvolution = 16,       /* IPS 16: user boundary conditions */
    ParabolicUserSteady = 17           /* IPS 17 */
};

/* the problem of AUTO's code, or none: a code with no function table */
constexpr std::optional<AutoProblem> auto_problem_from_code(long code)
{
    switch (code) {
    case -2: case -1: case 0: case 1: case 2: case 4: case 5: case 7: case 9:
    case 11: case 12: case 14: case 15: case 16: case 17:
        return static_cast<AutoProblem>(code);
    default:
        return std::nullopt;
    }
}
constexpr int auto_problem_code(AutoProblem p) { return static_cast<int>(p); }

/* a steady state or a map's fixed point: IPS 1 or -1 (abs(ips) == 1) */
constexpr bool problem_is_steady_or_map(AutoProblem p) { return p == AutoProblem::SteadyState || p == AutoProblem::DiscreteMap; }
/* IPS -1, 0, 1 (abs(ips) <= 1): algebraic, ODE steady state or map */
constexpr bool problem_is_algebraic_family(AutoProblem p) { return p == AutoProblem::Algebraic || problem_is_steady_or_map(p); }
/* IPS <= 1: the algebraic family and time integration */
constexpr bool problem_at_most_steady(AutoProblem p) { return p == AutoProblem::TimeIntegration || problem_is_algebraic_family(p); }
/* IPS 2, 7, 12: periodic orbits with Floquet multipliers */
constexpr bool problem_has_orbit_multipliers(AutoProblem p) {
    return p == AutoProblem::Periodic || p == AutoProblem::FloquetBoundaryValue || p == AutoProblem::WavePeriodic;
}
/* IPS 14, 16: parabolic evolution (the time is a parameter) */
constexpr bool problem_is_parabolic_evolution(AutoProblem p) {
    return p == AutoProblem::ParabolicPeriodicEvolution || p == AutoProblem::ParabolicUserEvolution;
}
/* IPS 5, 15: optimization (the objective is a parameter) */
constexpr bool problem_is_optimization(AutoProblem p) {
    return p == AutoProblem::AlgebraicOptimization || p == AutoProblem::PeriodicOptimization;
}

/* a solution that is a function of time or space (an orbit, a BVP, a
   homoclinic orbit, a parabolic problem): IPS 2-4, 6-9, 12-17; the output
   headings name its U columns MAX */
constexpr bool problem_has_solution_profile(AutoProblem p) {
    return p == AutoProblem::Periodic || p == AutoProblem::BoundaryValue || p == AutoProblem::FloquetBoundaryValue ||
           p == AutoProblem::Homoclinic || p == AutoProblem::WavePeriodic || problem_is_parabolic_evolution(p) ||
           p == AutoProblem::PeriodicOptimization || p == AutoProblem::ParabolicUserSteady;
}
/* PAR(11) is headed PERIOD: every problem but IPS <= 0 and the BVP (IPS 4) */
constexpr bool problem_heads_period(AutoProblem p) { return auto_problem_code(p) > 0 && p != AutoProblem::BoundaryValue; }

// Diagram curve codes: retain the Hopf code and distinguish its real-pair
// extension across kappa=0 without changing the restart's Hopf formulation.
constexpr int auto_hopf_curve = 3;
constexpr int auto_neutral_saddle_curve = 13;
constexpr int auto_hopf_curve_kind(double kappa) {
    return kappa < 0. ? auto_neutral_saddle_curve : auto_hopf_curve;
}

/* A special point's kind. Legacy kinds use ITP mod 10; equilibrium
   codimension-two kinds have explicit composite encodings below. */
enum class AutoPointKind : int {
    Pending = -1,         /* a special point AUTO is locating (in a run only, never stored) */
    Regular = 0,          /* an ordinary point */
    BranchPoint = 1,      /* BP of an algebraic problem */
    Fold = 2,             /* LP of an algebraic problem */
    Hopf = 3,             /* HB */
    Output = 4,           /* a point written every NPR steps */
    PeriodicFold = 5,     /* LP of a periodic orbit or a BVP */
    PeriodicBranch = 6,   /* BP of a periodic orbit or a BVP */
    PeriodDoubling = 7,   /* PD */
    Torus = 8,            /* TR */
    End = 9,              /* EP: the end of the branch */
    UserZero = -4,        /* UZ: a user-requested output value */
    BogdanovTakens = 10,  /* BT: a double zero eigenvalue */
    Cusp = 11,           /* CP: vanishing fold quadratic coefficient */
    ZeroHopf = 12,       /* ZH: a zero eigenvalue and imaginary pair */
    GeneralizedHopf = 13,/* GH: vanishing first Lyapunov coefficient */
    MaxSteps = -9         /* MX: no convergence, or the step limit */
};

/* A point's ITP: its kind, and the kind of the point a two-parameter
   continuation started from (ITPST; Regular for a one-parameter run, "the
   composite form" 10*start + kind otherwise). Signed kinds (UZ, MX) carry
   their start as -(10*start) + kind: -4 - 10*start, -9 - 10*start. */
struct AutoPointCode {
    AutoPointKind kind = AutoPointKind::Regular;
    AutoPointKind start = AutoPointKind::Regular;
    bool operator==(const AutoPointCode &) const = default;
    /* ITP == k with no start: a plain point of that kind */
    constexpr bool is(AutoPointKind k) const { return kind == k && start == AutoPointKind::Regular; }
    /* abs(ITP) / 10 == k: continued in two parameters from a point of kind k */
    constexpr bool started_from(AutoPointKind k) const { return start == k; }
    /* ITP % 10 == k, or abs(ITP) / 10 == k: the point or its start is of kind k */

};

constexpr bool auto_point_kind_valid(int v)
{
    return v == -1 || v == -4 || v == -9 || (v >= 0 && v <= 9);
}

/* ITP's integer, the file and protocol form */
constexpr int auto_point_encode(AutoPointCode c)
{
    // AUTO-07p equilibrium-curve encodings; -32 denotes GH on Hopf.
    if (c.kind == AutoPointKind::BogdanovTakens) return c.start == AutoPointKind::Fold ? -21 : -31;
    if (c.kind == AutoPointKind::Cusp) return -22;
    if (c.kind == AutoPointKind::ZeroHopf) return c.start == AutoPointKind::Fold ? -23 : -33;
    if (c.kind == AutoPointKind::GeneralizedHopf) return -32;
    const int kind = static_cast<int>(c.kind), start = static_cast<int>(c.start);
    return kind < 0 ? kind - 10 * start : 10 * start + kind;
}

/* The code of ITP's integer, or none: a start is 0..9, a kind one of the
   enum's; Pending is a run's own state, so a stored value passes
   allow_pending = false. Both the plain and the signed composite forms
   round-trip with auto_point_encode. */
constexpr std::optional<AutoPointCode> auto_point_decode(long itp, bool allow_pending = false)
{
    switch (itp) {
    case -21: return AutoPointCode{AutoPointKind::BogdanovTakens, AutoPointKind::Fold};
    case -22: return AutoPointCode{AutoPointKind::Cusp, AutoPointKind::Fold};
    case -23: return AutoPointCode{AutoPointKind::ZeroHopf, AutoPointKind::Fold};
    case -31: return AutoPointCode{AutoPointKind::BogdanovTakens, AutoPointKind::Hopf};
    case -32: return AutoPointCode{AutoPointKind::GeneralizedHopf, AutoPointKind::Hopf};
    case -33: return AutoPointCode{AutoPointKind::ZeroHopf, AutoPointKind::Hopf};
    default: break;
    }
    if (itp < -99 || itp > 99) return std::nullopt;
    const int kind = static_cast<int>(itp % 10); /* C's: it has the sign of ITP */
    const int start = static_cast<int>((itp < 0 ? -itp : itp) / 10);
    if (!auto_point_kind_valid(kind)) return std::nullopt;
    /* a negative ITP is a signed kind: UZ or MX, or the pending -1 alone */
    if (itp < 0 && kind >= 0) return std::nullopt;
    if (kind == -1 && (!allow_pending || start != 0)) return std::nullopt;
    return AutoPointCode{static_cast<AutoPointKind>(kind), static_cast<AutoPointKind>(start)};
}

/* the kind abs(ITP) < 10 names as a start: UZ's 4 is the start Output, MX's 9 End
   (autlib1.cpp findlb: itpst = abs(itp)) */
constexpr AutoPointKind auto_point_start_of(AutoPointCode c)
{
    if (c.start != AutoPointKind::Regular) return c.start;
    const int kind = static_cast<int>(c.kind);
    return static_cast<AutoPointKind>(kind < 0 ? -kind : kind);
}

/* the two-letter symbol of a point (BP, LP, HB, UZ, PD, TR, EP, MX; two
   blanks for the others), the diagram's Ty column */
constexpr const char *auto_point_symbol(AutoPointCode c)
{
    switch (c.kind) {
    case AutoPointKind::BranchPoint:
    case AutoPointKind::PeriodicBranch: return "BP";
    case AutoPointKind::Fold:
    case AutoPointKind::PeriodicFold: return "LP";
    case AutoPointKind::Hopf: return "HB";
    case AutoPointKind::BogdanovTakens: return "BT";
    case AutoPointKind::Cusp: return "CP";
    case AutoPointKind::ZeroHopf: return "ZH";
    case AutoPointKind::GeneralizedHopf: return "GH";
    case AutoPointKind::UserZero: return "UZ";
    case AutoPointKind::PeriodDoubling: return "PD";
    case AutoPointKind::Torus: return "TR";
    case AutoPointKind::End: return "EP";
    case AutoPointKind::MaxSteps: return "MX";
    default: return "  ";
    }
}

// Settings and protocol names for equilibrium codimension-two detection (W364).
inline constexpr std::array<AutoPointKind, 4> auto_codim_kinds = {
    AutoPointKind::BogdanovTakens, AutoPointKind::Cusp, AutoPointKind::ZeroHopf, AutoPointKind::GeneralizedHopf};
inline constexpr std::array<const char *, 4> auto_codim_keys = {"BT", "CP", "ZH", "GH"};

constexpr bool auto_codim_enabled(const std::array<bool, auto_codim_kinds.size()> &enabled, AutoPointKind kind)
{
    for (std::size_t i=0;i<auto_codim_kinds.size();++i)
        if (auto_codim_kinds[i]==kind) return enabled[i];
    return false;
}

/* what a reader says of a code it refuses, for its error's text */
inline std::string auto_point_unknown(long itp) { return xpp::format("{} is not an AUTO point type", itp); }

} // namespace xpp

#endif
