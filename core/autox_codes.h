#pragma once

/* The one owner of autoX's problem and point-type codes (W329, #390):
   what autoX's IPS and ITP integers mean, as types. Integers stay only at
   the explicit boundaries: autoX's fort files and the diagram's saved
   columns (autox_problem_from_code / autox_point_decode on the way in, the
   _code / autox_point_encode on the way out), where a code that is none of
   these is refused by the reader with its file and line. The diagram's
   stable/periodic classification is DiagramPointType below and the
   stability values' kind autox_stability.h's AutoxStabilityKind. */

#include <array>
#include <string_view>
#include <optional>
#include <string>
#include <cstddef>

#include "xpp_io.h"

namespace xpp {

/* A diagram point's type (get_bif_type, autox_evd.cpp, from the signs autoX
   gives its branch and point numbers; a run's `ty` in docs/protocol.md's
   `diagram`, `type` in `autoxinfo`): a steady state, judged by its
   eigenvalues, or a periodic orbit, by its Floquet multipliers; stable
   or not. The events carry stable and periodic flags beside it. It lives
   here, not in diagram.h, so that the headers a point's type crosses
   (autox_evd.h, autox_nox.h, autox_data.h, xpp_ui.h) can name it. */
enum DiagramPointType : int { STABLE_EQ = 1, UNSTABLE_EQ = 2, STABLE_PERIODIC = 3, UNSTABLE_PERIODIC = 4 };
constexpr bool point_is_stable(DiagramPointType type) { return type == STABLE_EQ || type == STABLE_PERIODIC; }
constexpr bool point_is_periodic(DiagramPointType type) { return type == STABLE_PERIODIC || type == UNSTABLE_PERIODIC; }
/* the protocol's `ty`/`type` number (docs/protocol.md): the explicit encoding of the type */
constexpr int diagram_point_type_code(DiagramPointType type) { return static_cast<int>(type); }

/* autoX's IPS: the kind of problem a run solves. Only these have a function
   table (gogoautox.cpp problem_functions); the code is autoX's own number
   (AUTO-07p manual, IPS). */
enum class AutoxProblem : int {
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

/* the problem of autoX's code, or none: a code with no function table */
constexpr std::optional<AutoxProblem> autox_problem_from_code(long code)
{
    switch (code) {
    case -2: case -1: case 0: case 1: case 2: case 4: case 5: case 7: case 9:
    case 11: case 12: case 14: case 15: case 16: case 17:
        return static_cast<AutoxProblem>(code);
    default:
        return std::nullopt;
    }
}
constexpr int autox_problem_code(AutoxProblem p) { return static_cast<int>(p); }

/* a steady state or a map's fixed point: IPS 1 or -1 (abs(ips) == 1) */
constexpr bool problem_is_steady_or_map(AutoxProblem p) { return p == AutoxProblem::SteadyState || p == AutoxProblem::DiscreteMap; }
/* IPS -1, 0, 1 (abs(ips) <= 1): algebraic, ODE steady state or map */
constexpr bool problem_is_algebraic_family(AutoxProblem p) { return p == AutoxProblem::Algebraic || problem_is_steady_or_map(p); }
/* IPS <= 1: the algebraic family and time integration */
constexpr bool problem_at_most_steady(AutoxProblem p) { return p == AutoxProblem::TimeIntegration || problem_is_algebraic_family(p); }
/* IPS 2, 7, 12: periodic orbits with Floquet multipliers */
constexpr bool problem_has_orbit_multipliers(AutoxProblem p) {
    return p == AutoxProblem::Periodic || p == AutoxProblem::FloquetBoundaryValue || p == AutoxProblem::WavePeriodic;
}
/* IPS 14, 16: parabolic evolution (the time is a parameter) */
constexpr bool problem_is_parabolic_evolution(AutoxProblem p) {
    return p == AutoxProblem::ParabolicPeriodicEvolution || p == AutoxProblem::ParabolicUserEvolution;
}
/* IPS 5, 15: optimization (the objective is a parameter) */
constexpr bool problem_is_optimization(AutoxProblem p) {
    return p == AutoxProblem::AlgebraicOptimization || p == AutoxProblem::PeriodicOptimization;
}

/* a solution that is a function of time or space (an orbit, a BVP, a
   homoclinic orbit, a parabolic problem): IPS 2-4, 6-9, 12-17; the output
   headings name its U columns MAX */
constexpr bool problem_has_solution_profile(AutoxProblem p) {
    return p == AutoxProblem::Periodic || p == AutoxProblem::BoundaryValue || p == AutoxProblem::FloquetBoundaryValue ||
           p == AutoxProblem::Homoclinic || p == AutoxProblem::WavePeriodic || problem_is_parabolic_evolution(p) ||
           p == AutoxProblem::PeriodicOptimization || p == AutoxProblem::ParabolicUserSteady;
}
/* what a problem is, for a message: AUTO-07p's manual's names of the IPS */
constexpr const char *autox_problem_description(AutoxProblem p)
{
    switch (p) {
    case AutoxProblem::TimeIntegration: return "time integration (IPS -2)";
    case AutoxProblem::DiscreteMap: return "fixed points of a map (IPS -1)";
    case AutoxProblem::Algebraic: return "an algebraic system (IPS 0)";
    case AutoxProblem::SteadyState: return "steady states (IPS 1)";
    case AutoxProblem::Periodic: return "periodic orbits (IPS 2)";
    case AutoxProblem::BoundaryValue: return "a boundary value problem (IPS 4)";
    case AutoxProblem::AlgebraicOptimization: return "algebraic optimization (IPS 5)";
    case AutoxProblem::FloquetBoundaryValue: return "a boundary value problem with Floquet multipliers (IPS 7)";
    case AutoxProblem::Homoclinic: return "homoclinic orbits (IPS 9)";
    case AutoxProblem::WaveSteady: return "spatially homogeneous waves (IPS 11)";
    case AutoxProblem::WavePeriodic: return "wave trains (IPS 12)";
    case AutoxProblem::ParabolicPeriodicEvolution: return "parabolic evolution, periodic boundary conditions (IPS 14)";
    case AutoxProblem::PeriodicOptimization: return "periodic optimization (IPS 15)";
    case AutoxProblem::ParabolicUserEvolution: return "parabolic evolution, user boundary conditions (IPS 16)";
    case AutoxProblem::ParabolicUserSteady: return "parabolic steady states (IPS 17)";
    }
    return "an unknown problem";
}
/* PAR(11) is headed PERIOD: every problem but IPS <= 0 and the BVP (IPS 4) */
constexpr bool problem_heads_period(AutoxProblem p) { return autox_problem_code(p) > 0 && p != AutoxProblem::BoundaryValue; }

/* The calculation recorded by a point; foreign XPPAUT codes are decoded
   only at import. Native files write the corresponding name. */
enum AutoxCalculation : int {
    no_calculation=0, LPE2=1, LPP2=2, autox_hopf_curve=3, TR2=4, BR2=5, PD2=6,
    FP2=7, BV1=8, EQ1=9, PE1=10, DI1=11, HO2=12, autox_neutral_saddle_curve=13
};
inline constexpr auto autox_calculation_names=std::to_array<std::string_view>({
    "none", "fold", "cycle_fold", "hopf", "neimark_sacker", "branch_point", "period_doubling",
    "fixed_period", "boundary_value", "equilibrium", "periodic", "discrete", "homoclinic", "neutral_saddle"});
constexpr std::string_view autox_calculation_name(AutoxCalculation kind) {
    return autox_calculation_names[static_cast<std::size_t>(kind)];
}
constexpr std::optional<AutoxCalculation> autox_calculation_from_code(int code) {
    if(code<0 || code>=static_cast<int>(autox_calculation_names.size())) return std::nullopt;
    return static_cast<AutoxCalculation>(code);
}
constexpr std::optional<AutoxCalculation> autox_calculation_from_name(std::string_view name) {
    return xpp::enum_from_name<AutoxCalculation>(autox_calculation_names,name);
}
constexpr AutoxCalculation autox_hopf_curve_kind(double kappa) {
    return kappa < 0. ? autox_neutral_saddle_curve : autox_hopf_curve;
}

/* What a run computes (W516: autox.run's `kind`); a branch switch is the same
   kind with `switch`. autox_run.h's table says which kinds a start point's
   type allows. */
enum class AutoxRunKind : int { equilibrium, periodic, two_parameter, fixed_period, bvp, homoclinic, heteroclinic };
inline constexpr std::array<const char *, 7> autox_run_kind_names = {
    "equilibrium", "periodic", "two_parameter", "fixed_period", "bvp", "homoclinic", "heteroclinic"};
constexpr const char *autox_run_kind_name(AutoxRunKind kind) { return autox_run_kind_names[static_cast<std::size_t>(kind)]; }
constexpr std::optional<AutoxRunKind> autox_run_kind_from_name(std::string_view name)
{
    return xpp::enum_from_name<AutoxRunKind>(autox_run_kind_names, name);
}

/* A special point's kind. Most use ITP mod 10; equilibrium
   codimension-two kinds have explicit composite encodings below. */
enum class AutoxPointKind : int {
    Pending = -1,         /* a special point autoX is locating (in a run only, never stored) */
    Regular = 0,          /* an ordinary point */
    BranchPoint = 1,      /* BP of an algebraic problem */
    Fold = 2,             /* LP of an algebraic problem */
    Hopf = 3,             /* AH: Andronov-Hopf */
    Output = 4,           /* a point written every NPR steps */
    CycleFold = 5,        /* LPC: a fold of a periodic orbit or a BVP */
    CycleBranch = 6,      /* BPC: a branch point of a periodic orbit or a BVP */
    PeriodDoubling = 7,   /* PD */
    NeimarkSacker = 8,    /* NS: a torus bifurcation */
    End = 9,              /* EP: the end of the branch */
    UserZero = -4,        /* UZ: a user-requested output value */
    BogdanovTakens = 10,  /* BT: a double zero eigenvalue */
    Cusp = 11,           /* CP: vanishing fold quadratic coefficient */
    ZeroHopf = 12,       /* ZH: a zero eigenvalue and imaginary pair */
    GeneralizedHopf = 13,/* GH: vanishing first Lyapunov coefficient */
    Resonance1 = 14,     /* R1: 1:1 resonance of a map or cycle */
    Resonance2 = 15,     /* R2: 1:2 resonance of a map or cycle */
    Resonance3 = 16,     /* R3: 1:3 resonance of a map or cycle */
    Resonance4 = 17,     /* R4: 1:4 resonance of a map or cycle */
    FoldFlip = 18,        /* LPPD: fold and flip of a map */
    FoldNeimarkSacker = 19,   /* LPNS: fold and Neimark-Sacker of a map */
    FlipNeimarkSacker = 20,   /* PDNS: flip and Neimark-Sacker of a map */
    DoubleNeimarkSacker = 21, /* NSNS: two Neimark-Sacker pairs of a map */
    Snic = 22,            /* SNIC: a periodic branch ends on a saddle-node on an invariant circle (W518) */
    Homoclinic = 23,      /* HOC: a periodic branch ends on a homoclinic orbit to a saddle (W518) */
    Heteroclinic = 24,    /* HEC: a periodic branch ends on a heteroclinic cycle between saddles (W518) */
    HopfEnd = 25,         /* HBE: a periodic branch ends where its cycles shrink to zero amplitude at a Hopf point (W604) */
    BranchClosed = 26,    /* CL: a branch ends back at its own start (W604) */
    MaxSteps = -9         /* MX: no convergence, or the step limit */
};

/* A point's ITP: its kind, and the kind of the point a two-parameter
   continuation started from (ITPST; Regular for a one-parameter run, "the
   composite form" 10*start + kind otherwise). Signed kinds (UZ, MX) carry
   their start as -(10*start) + kind: -4 - 10*start, -9 - 10*start. */
struct AutoxPointCode {
    AutoxPointKind kind = AutoxPointKind::Regular;
    AutoxPointKind start = AutoxPointKind::Regular;
    bool operator==(const AutoxPointCode &) const = default;
    /* ITP == k with no start: a plain point of that kind */
    constexpr bool is(AutoxPointKind k) const { return kind == k && start == AutoxPointKind::Regular; }
    /* abs(ITP) / 10 == k: continued in two parameters from a point of kind k */
    constexpr bool started_from(AutoxPointKind k) const { return start == k; }
    /* ITP % 10 == k, or abs(ITP) / 10 == k: the point or its start is of kind k */

};

constexpr bool autox_point_kind_valid(int v)
{
    return v == -1 || v == -4 || v == -9 || (v >= 0 && v <= 9);
}

/* ITP's integer, the file and protocol form */
constexpr int autox_point_encode(AutoxPointCode c)
{
    // AUTO-07p equilibrium-curve encodings; -32 denotes GH on Hopf.
    if (c.kind == AutoxPointKind::BogdanovTakens) return c.start == AutoxPointKind::Fold ? -21 : -31;
    if (c.kind == AutoxPointKind::Cusp) return -22;
    if (c.kind == AutoxPointKind::ZeroHopf) return c.start == AutoxPointKind::Fold ? -23 : -33;
    if (c.kind == AutoxPointKind::GeneralizedHopf) return -32;
    if (c.kind >= AutoxPointKind::Resonance1 && c.kind <= AutoxPointKind::Resonance4)
        return -10 * static_cast<int>(c.start) - 5 -
            (static_cast<int>(c.kind) - static_cast<int>(AutoxPointKind::Resonance1));
    if (c.kind == AutoxPointKind::FoldFlip) return c.start == AutoxPointKind::Fold ? 28 : 78;
    if (c.kind == AutoxPointKind::FoldNeimarkSacker) return c.start == AutoxPointKind::Fold ? 23 : 83;
    if (c.kind == AutoxPointKind::FlipNeimarkSacker) return c.start == AutoxPointKind::PeriodDoubling ? 77 : 87;
    if (c.kind == AutoxPointKind::DoubleNeimarkSacker) return 88;
    // W518, W604: the ends of a branch no autoX code names, ITP outside the composite range 0..99
    if (c.kind == AutoxPointKind::Snic) return 100;
    if (c.kind == AutoxPointKind::Homoclinic) return 101;
    if (c.kind == AutoxPointKind::Heteroclinic) return 102;
    if (c.kind == AutoxPointKind::HopfEnd) return 103;
    if (c.kind == AutoxPointKind::BranchClosed) return 104;
    const int kind = static_cast<int>(c.kind), start = static_cast<int>(c.start);
    return kind < 0 ? kind - 10 * start : 10 * start + kind;
}

/* The code of ITP's integer, or none: a start is 0..9, a kind one of the
   enum's; Pending is a run's own state, so a stored value passes
   allow_pending = false. Both the plain and the signed composite forms
   round-trip with autox_point_encode. */
constexpr std::optional<AutoxPointCode> autox_point_decode(long itp, bool allow_pending = false)
{
    switch (itp) {
    case 28: return AutoxPointCode{AutoxPointKind::FoldFlip, AutoxPointKind::Fold};
    case 78: return AutoxPointCode{AutoxPointKind::FoldFlip, AutoxPointKind::PeriodDoubling};
    case 23: return AutoxPointCode{AutoxPointKind::FoldNeimarkSacker, AutoxPointKind::Fold};
    case 83: return AutoxPointCode{AutoxPointKind::FoldNeimarkSacker, AutoxPointKind::NeimarkSacker};
    case 77: return AutoxPointCode{AutoxPointKind::FlipNeimarkSacker, AutoxPointKind::PeriodDoubling};
    case 87: return AutoxPointCode{AutoxPointKind::FlipNeimarkSacker, AutoxPointKind::NeimarkSacker};
    case 88: return AutoxPointCode{AutoxPointKind::DoubleNeimarkSacker, AutoxPointKind::NeimarkSacker};
    case 100: return AutoxPointCode{AutoxPointKind::Snic, AutoxPointKind::Regular};
    case 101: return AutoxPointCode{AutoxPointKind::Homoclinic, AutoxPointKind::Regular};
    case 102: return AutoxPointCode{AutoxPointKind::Heteroclinic, AutoxPointKind::Regular};
    case 103: return AutoxPointCode{AutoxPointKind::HopfEnd, AutoxPointKind::Regular};
    case 104: return AutoxPointCode{AutoxPointKind::BranchClosed, AutoxPointKind::Regular};
    case -21: return AutoxPointCode{AutoxPointKind::BogdanovTakens, AutoxPointKind::Fold};
    case -22: return AutoxPointCode{AutoxPointKind::Cusp, AutoxPointKind::Fold};
    case -23: return AutoxPointCode{AutoxPointKind::ZeroHopf, AutoxPointKind::Fold};
    case -31: return AutoxPointCode{AutoxPointKind::BogdanovTakens, AutoxPointKind::Hopf};
    case -32: return AutoxPointCode{AutoxPointKind::GeneralizedHopf, AutoxPointKind::Hopf};
    case -33: return AutoxPointCode{AutoxPointKind::ZeroHopf, AutoxPointKind::Hopf};
    default: break;
    }
    for (auto start : {AutoxPointKind::Fold, AutoxPointKind::CycleFold, AutoxPointKind::PeriodDoubling, AutoxPointKind::NeimarkSacker})
        for (auto kind : {AutoxPointKind::Resonance1, AutoxPointKind::Resonance2,
                          AutoxPointKind::Resonance3, AutoxPointKind::Resonance4})
            if (autox_point_encode({kind,start})==itp) return AutoxPointCode{kind,start};
    if (itp < -99 || itp > 99) return std::nullopt;
    const int kind = static_cast<int>(itp % 10); /* C's: it has the sign of ITP */
    const int start = static_cast<int>((itp < 0 ? -itp : itp) / 10);
    if (!autox_point_kind_valid(kind)) return std::nullopt;
    /* a negative ITP is a signed kind: UZ or MX, or the pending -1 alone */
    if (itp < 0 && kind >= 0) return std::nullopt;
    if (kind == -1 && (!allow_pending || start != 0)) return std::nullopt;
    return AutoxPointCode{static_cast<AutoxPointKind>(kind), static_cast<AutoxPointKind>(start)};
}

/* the kind abs(ITP) < 10 names as a start: UZ's 4 is the start Output, MX's 9 End
   (autox_lib1.cpp find_restart_label: itpst = abs(itp)) */
constexpr AutoxPointKind autox_point_start_of(AutoxPointCode c)
{
    if (c.start != AutoxPointKind::Regular) return c.start;
    const int kind = static_cast<int>(c.kind);
    return static_cast<AutoxPointKind>(kind < 0 ? -kind : kind);
}

/* One table per point type (W519, maintainer 2026-10-28): its name (the only
   input: files, protocol, settings keys), its label (the menus, hover, legend,
   info strip, manual; written here, never derived) and its mark (the short text
   drawn at the point on plots and the log's table column; empty for none).
   Pending is a run's own state, listed for its display only: a file never
   holds it. */
struct AutoxPointName {
    AutoxPointKind kind;
    std::string_view name;
    std::string_view label;
    std::string_view mark;
};
inline constexpr auto autox_point_names = std::to_array<AutoxPointName>({
    {AutoxPointKind::Pending, "pending", "Pending", ""},
    {AutoxPointKind::Regular, "regular", "Regular", ""},
    {AutoxPointKind::BranchPoint, "branch_point", "Branch point", "BP"},
    {AutoxPointKind::Fold, "fold", "Fold", "LP"},
    {AutoxPointKind::Hopf, "hopf", "Andronov–Hopf", "AH"},
    {AutoxPointKind::Output, "output", "Output point", ""},
    {AutoxPointKind::CycleFold, "cycle_fold", "Fold of cycles", "LPC"},
    {AutoxPointKind::CycleBranch, "cycle_branch_point", "Branch point of cycles", "BPC"},
    {AutoxPointKind::PeriodDoubling, "period_doubling", "Period doubling (flip)", "PD"},
    {AutoxPointKind::NeimarkSacker, "neimark_sacker", "Neimark–Sacker (torus)", "NS"},
    {AutoxPointKind::End, "end", "End of branch", "EP"},
    {AutoxPointKind::UserZero, "user_point", "User point", "UZ"},
    {AutoxPointKind::BogdanovTakens, "bogdanov_takens", "Bogdanov–Takens", "BT"},
    {AutoxPointKind::Cusp, "cusp", "Cusp", "CP"},
    {AutoxPointKind::ZeroHopf, "zero_hopf", "Zero-Hopf (fold–Hopf)", "ZH"},
    {AutoxPointKind::GeneralizedHopf, "generalized_hopf", "Generalized Hopf (Bautin)", "GH"},
    {AutoxPointKind::Resonance1, "resonance_1_1", "Resonance 1:1", "R1"},
    {AutoxPointKind::Resonance2, "resonance_1_2", "Resonance 1:2", "R2"},
    {AutoxPointKind::Resonance3, "resonance_1_3", "Resonance 1:3", "R3"},
    {AutoxPointKind::Resonance4, "resonance_1_4", "Resonance 1:4", "R4"},
    {AutoxPointKind::FoldFlip, "fold_flip", "Fold–flip", "LPPD"},
    {AutoxPointKind::FoldNeimarkSacker, "fold_neimark_sacker", "Fold–Neimark–Sacker", "LPNS"},
    {AutoxPointKind::FlipNeimarkSacker, "flip_neimark_sacker", "Flip–Neimark–Sacker", "PDNS"},
    {AutoxPointKind::DoubleNeimarkSacker, "double_neimark_sacker", "Double Neimark–Sacker", "NSNS"},
    {AutoxPointKind::Snic, "snic", "SNIC (saddle-node on invariant circle)", "SNIC"},
    {AutoxPointKind::Homoclinic, "homoclinic", "Homoclinic", "HOC"},
    {AutoxPointKind::Heteroclinic, "heteroclinic", "Heteroclinic", "HEC"},
    {AutoxPointKind::HopfEnd, "hopf_end", "End at a Hopf point", "HBE"},
    {AutoxPointKind::BranchClosed, "branch_closed", "Branch closed", "CL"},
    {AutoxPointKind::MaxSteps, "no_convergence", "No convergence", "MX"},
});

/* a point type's name, label and mark (Pending's included) */
constexpr const AutoxPointName &autox_point_entry(AutoxPointKind kind)
{
    for (const AutoxPointName &p : autox_point_names)
        if (p.kind == kind) return p;
    return autox_point_names[0];
}
constexpr std::string_view autox_point_kind_name(AutoxPointKind kind) { return autox_point_entry(kind).name; }
constexpr std::string_view autox_point_kind_label(AutoxPointKind kind) { return autox_point_entry(kind).label; }
constexpr std::string_view autox_point_kind_mark(AutoxPointKind kind) { return autox_point_entry(kind).mark; }

/* the kind a name gives: none for an unknown name, and for Pending, which no file holds */
constexpr std::optional<AutoxPointKind> autox_point_kind_from_name(std::string_view name)
{
    for (const AutoxPointName &p : autox_point_names)
        if (p.kind != AutoxPointKind::Pending && p.name == name) return p.kind;
    return std::nullopt;
}

/* the joint between a kind and the start of a point continued in two
   parameters: fold_flip_from_fold */
inline constexpr std::string_view autox_point_from_joint = "_from_";

/* a point's type as our files and the protocol write it: its kind's name,
   and for a point continued from another kind, that kind's name after "_from_" */
inline std::string autox_point_text(AutoxPointCode c)
{
    if (c.start == AutoxPointKind::Regular) return std::string(autox_point_kind_name(c.kind));
    return xpp::format("{}{}{}", autox_point_kind_name(c.kind), autox_point_from_joint, autox_point_kind_name(c.start));
}

/* the code a point type's text names, or none: the inverse of autox_point_text,
   and only a code autox_point_decode gives (the same check as the numeric form) */
inline std::optional<AutoxPointCode> autox_point_from_text(std::string_view text)
{
    const std::size_t joint = text.find(autox_point_from_joint);
    const auto kind = autox_point_kind_from_name(text.substr(0, joint));
    if (!kind) return std::nullopt;
    AutoxPointCode c{*kind, AutoxPointKind::Regular};
    if (joint != std::string_view::npos) {
        const auto start = autox_point_kind_from_name(text.substr(joint + autox_point_from_joint.size()));
        if (!start) return std::nullopt;
        c.start = *start;
    }
    const auto back = autox_point_decode(autox_point_encode(c));
    if (!back || !(*back == c)) return std::nullopt;
    return c;
}

// Settings and protocol names for codimension-two detection (W364, W400).
inline constexpr std::array<AutoxPointKind, 12> autox_codim_kinds = {
    AutoxPointKind::BogdanovTakens, AutoxPointKind::Cusp, AutoxPointKind::ZeroHopf, AutoxPointKind::GeneralizedHopf,
    AutoxPointKind::Resonance1, AutoxPointKind::Resonance2, AutoxPointKind::Resonance3, AutoxPointKind::Resonance4,
    AutoxPointKind::FoldFlip, AutoxPointKind::FoldNeimarkSacker, AutoxPointKind::FlipNeimarkSacker, AutoxPointKind::DoubleNeimarkSacker};
/* the names of autox_codim_kinds, in the same order: the settings' keys */
inline constexpr auto autox_codim_keys = [] {
    std::array<std::string_view, autox_codim_kinds.size()> keys{};
    for (std::size_t i = 0; i < keys.size(); ++i) keys[i] = autox_point_kind_name(autox_codim_kinds[i]);
    return keys;
}();

constexpr bool autox_codim_enabled(const std::array<bool, autox_codim_kinds.size()> &enabled, AutoxPointKind kind)
{
    for (std::size_t i=0;i<autox_codim_kinds.size();++i)
        if (autox_codim_kinds[i]==kind) return enabled[i];
    return false;
}

/* the settings' keys as one list, "bogdanov_takens, cusp, ...", for a message */
inline std::string autox_codim_key_list()
{
    std::string list;
    for (std::string_view key : autox_codim_keys) list += (list.empty() ? "" : ", ") + std::string(key);
    return list;
}

/* what a reader says of a number it refuses, for its error's text (a foreign file's code) */
inline std::string autox_point_unknown(long itp) { return xpp::format("{} is not an autoX point type", itp); }
/* the same for a name a file holds */
inline std::string autox_point_text_unknown(std::string_view text) { return xpp::format("\"{}\" is not an autoX point type", text); }

} // namespace xpp
