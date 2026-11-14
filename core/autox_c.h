#pragma once
/* autoX's own API between its files (autox_lib1-3/5, conpar2,
   setubv2) and the autoX front end (gogoautox, autox_nox, autox_evd). */
#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
#include <span>
#include <string_view>
#include <functional>
#include "autox_codes.h"
#include "xAutox.h" /* AutoxMark */
#include "xpp_math.h" /* xpp::Matrix, Matrix3 */
#include "xpp_math_autox.h" /* autoX numerical interfaces, owned by xpp_math; IWYU pragma: keep (the prototypes below need its enums complete) */

namespace xpp { class TokenReader; struct AutoxSolutionHeader; }

struct AutoxRun; /* autox_state.h */

/* autoX's numerical thresholds and tolerances (autox_lib1-5, autox_jacobian). */
inline constexpr double autox_hmach = 1.0e-7;            // Relative finite-difference step and convergence tolerance
inline constexpr double autox_rlarge = 1.0e+30;          // Practical infinity for uncomputed bounds and eigenvalues
inline constexpr double autox_hmach1 = autox_hmach + 1.0; // 1 + HMACH for multiplier unit circle comparisons
inline constexpr double autox_hmachho = 1.0e-13;         // HomCont component zero threshold (compzero)
inline constexpr double autox_svd_tolerance = 1.0e-16;   // FLOWKM's SVD tolerance (autoX's own value)
inline constexpr double autox_divergence_ratio = 20;     // A Newton step this many times the first one diverges (autoX's DELREF factor)
inline constexpr double autox_homoclinic_norm_start = 1.0e20; // HomCont's search for the orbit's point nearest the equilibrium starts above any norm
/* autoX's JAC: who supplies the derivatives problem_function passes on */
inline constexpr std::ptrdiff_t autox_jac_differences = 0;          // autoX differences the right-hand side
inline constexpr std::ptrdiff_t autox_jac_supplied = 1;             // user_right_hand_side supplies them: the model's exact Jacobian (W260)

/* autoX's file schemas and output record layouts (autox_lib1-2, autox_nox).
   Integer output widths are minima: explicit spaces keep full point counts
   separate when they outgrow a column (AUTO-07p's Python readers split tokens). */
inline constexpr int autox_solution_columns = 7;             // Floating-point values per line in autoX solution files (fort.8 / .s / fort.3)
inline constexpr int autox_solution_header_fields = 12;      // Number of fields in autoX solution file header records
inline constexpr int autox_solution_parameter_indices_per_line = 20; // Free parameter indices per line in fort.8 (5 characters each)
inline constexpr int autox_diagram_columns = 7;              // Maximum parameter and state component columns on a bifurcation diagram row (fort.7/fort.9)
inline constexpr int autox_heading_columns = autox_diagram_columns + 2; // Column heading slots in write_headings (1-based index with vaxis and 7 components)
inline constexpr int autox_column_width = 14;                // Fixed character width for bifurcation diagram columns in fort.7/fort.9 and window


struct iap_type {
  /* 1 */ std::ptrdiff_t ndim;
  /* 2 */ xpp::AutoxProblem ips;
  /* 3 */ std::ptrdiff_t irs;
  /* 4 */ std::ptrdiff_t ilp;
  /* 5 */ std::ptrdiff_t ntst;
  /* 6 */ std::ptrdiff_t ncol;
  /* 7 */ std::ptrdiff_t iad;
  /* 8 */ std::ptrdiff_t iads;
  /* 9 */ std::ptrdiff_t isp;
  /* 10 */ std::ptrdiff_t isw;
  /* 11 */ std::ptrdiff_t iplt;
  /* 12 */ std::ptrdiff_t nbc;
  /* 13 */ std::ptrdiff_t nint;
  /* 14 */ std::ptrdiff_t nmx;
  /* 15 */ std::ptrdiff_t nuzr;
  /* 16 */ std::ptrdiff_t npr;
  /* 17 */ std::ptrdiff_t mxbf;
  /* 18 */ std::ptrdiff_t iid;
  /* 19 */ std::ptrdiff_t itmx;
  /* 20 */ std::ptrdiff_t itnw;
  /* 21 */ std::ptrdiff_t nwtn;
  /* 22 */ std::ptrdiff_t jac;
  /* 23 */ std::ptrdiff_t ndm;
  /* 24 */ std::ptrdiff_t nbc0;
  /* 25 */ std::ptrdiff_t nnt0;
  /* 26 */ std::ptrdiff_t iuzr;
  /* 27 */ xpp::AutoxPointCode itp;
  /* 28 */ xpp::AutoxPointKind itpst; /* the start kind of a two-parameter continuation */
  /* 29 */ std::ptrdiff_t nfpr;
  /* 30 */ std::ptrdiff_t ibr;
  /* 31 */ std::ptrdiff_t nit;
  /* 32 */ std::ptrdiff_t ntot;
  /* 33 */ std::ptrdiff_t nins;
  /* 34 */ std::ptrdiff_t istop;
  /* 35 */ std::ptrdiff_t nbif;
  /* 36 */ std::ptrdiff_t ipos;
  /* 37 */ std::ptrdiff_t lab;
  /* 41 */ std::ptrdiff_t nicp;
  /* xppautX: the run's own state (autox_state.h), and through it the
     Session the run belongs to; run_autox_problem sets it before init */
  struct AutoxRun *run;
};

typedef struct {
  /* 1 */ double ds;
  /* 2 */ double dsmin;
  /* 3 */ double dsmax;
  /* There is no 4 */
  /* 5 */ double dsold;
  /* 6 */ double rl0;
  /* 7 */ double rl1;
  /* 8 */ double a0;
  /* 9 */ double a1;
  /* 11 */ double epsl;
  /* 12 */ double epsu;
  /* 13 */ double epss;
  /* 14 */ double det;
  /* 15 */ double tivp;
} rap_type;

/*This is the type for all functions which can be used as "problem_function" the function
  which evaluates the right hand side of the equations and generates
  the Jacobian*/
using ProblemFunction = int (*)(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);

/*This is the type for all functions which can be used as "boundary_function" the function
  which evaluates the boundary conditions */
using BoundaryFunction = int (*)(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);

/*This is the type for all functions which can be used as "integral_function" the function
  which evaluates kernel of the integral constraints */
using IntegralFunction = int (*)(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);

/*This is the type for all functions which can be used as additional
  output functions for algebraic problems */
using OutputParametersAlgebraic = int (*)(iap_type *iap, rap_type *rap, double *u, double *par);

/*This is the type for all functions which can be used as additional
  output functions for BVPs */
using OutputParametersBoundary = int (*)(iap_type *iap, rap_type *rap, std::ptrdiff_t *icp, double *dtm, std::ptrdiff_t *ndxloc, double *ups, std::ptrdiff_t *ndim, double *p0, double *p1, double *par);

/* This is the type for all functions that can be used at starting points
   for algebraic problems */
using UserStartAlgebraic = int (*)(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);

/* This is the type for all functions that can be used at starting points
   for BVPs */
using UserStartBoundary = int (*)(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);

/*This is the type for all functions which can be used to detect
  special points for algebraic problems */
using AlgebraicTestFunction = double (*)(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);

/*This is the type for all functions which can be used to detect
  special points for BVPS */
using BoundaryTestFunction = double (*)(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);

/* W419 (#481): what a test function returns where its test does not apply
   at a point (a Floquet test on a planar orbit, an inaccurate trivial
   multiplier, no Hopf pair, a resonance test with every kind disabled):
   no value, never a zero. A location neither starts from it nor stops on it. */
inline constexpr double autox_test_not_applicable = std::numeric_limits<double>::quiet_NaN();
inline bool autox_test_applies(double value) { return !std::isnan(value); }

/* W545 (#610): what a test function keeps from one step to the next. `value`
   is the test at the last stored point, 0 where none is known. After a step
   that located a zero it is the test at the discarded end of that step
   instead, so a second zero ahead of the located point shows as a sign
   change (AUTO-07p stores 0, which hides it: docs/auto07p-findings.md 11);
   `offset` is then the arclength from the stored point to where that value
   was sampled. `after_branch_point` (W585) marks a torus curve's resonance
   whose zero coincided with the branch point located at the stored point,
   left for the next step to locate. */
struct AutoxTestSample {
  double value = 0;
  std::optional<double> offset;
  bool after_branch_point = false;
};

/* W545 (#610): a located zero is a transversal crossing of a smooth test,
   a jump, noise, or a zero of higher order. A smooth transversal crossing
   has about the same slope across the step as at the end of the location, a
   tolerance wide; a jump or noise has a slope that grows as the bracket
   shrinks, a zero of higher order one that shrinks. The slope across the
   final bracket over the slope across the step: 0 or more, NaN where a
   length is not positive. Measured (docs/auto-references.md W545): the new
   zeros that are crossings give 1.02 and 1.76, the ordinary zeros of the 71
   models 0.7 to 2.3, the jumps and noise refused 29.7 and more: a zero whose
   ratio is above this factor is no crossing, one below its inverse is of
   higher order (autox_zero_order says whether it crosses). */
inline constexpr double autox_crossing_slope_ratio_max = 3;
inline double autox_crossing_slope_ratio(double step_change, double step_length,
                                            double final_change, double final_width) {
  if (!(step_length > 0 && final_width > 0 && step_change != 0)) return std::numeric_limits<double>::quiet_NaN();
  return (std::abs(final_change) / final_width) / (std::abs(step_change) / step_length);
}

/* W580 (#645): the order of a zero whose slope shrinks as its bracket does,
   from the test at the ends of the step, each against its distance from the
   located zero: an order-k zero has |q| = c d^k on both sides, so
   ln(|q_a| / |q_b|) = k ln(d_a / d_b). An odd order changes sign (a cubic is a
   crossing, however flat: henon_rot_th's branch point at th = pi); an even
   one is a touching the samples straddle only through a value at the noise
   of a test that does not cross there (a double zero c d^2 against noise e
   sampled at d = sqrt(e / c) gives 2). 0 where the ends cannot say (a value
   or a distance of 0, or equal distances). */
inline double autox_zero_order(double q_a, double d_a, double q_b, double d_b) {
  const double distances = xpp::math::log(std::abs(d_a) / std::abs(d_b));
  const double order = xpp::math::log(std::abs(q_a) / std::abs(q_b)) / distances;
  return std::isfinite(order) && distances != 0 ? std::round(order) : 0;
}
inline bool autox_zero_crosses(double order) { return order > 0 && std::fmod(order, 2) == 1; }

/* W545 (#610): a cycle's branch-point test is a ratio of consecutive
   determinants (AUTO-07p's DET0/DET), about 1 along the branch. Where the
   determinant has a simple zero in a step, a sample of the ratio larger than
   the step holds location tolerances lies within one tolerance of it, and the
   secant method on the ratio, which has a pole there, puts the zero a step
   away. A bracket whose values pass that size is not a crossing the search
   can place; the determinant there is at its noise (global_heteroclinic's
   1e13 to 1e18, lecar's 1e7 near its fold). W547 (#612): it gates every
   bracket of the test, AUTO-07p's own included: the real branch points of
   the descriptions, at their steps and at 0.5, 0.75 and 1.5 times them, reach
   244 times the step's length, the refused ones 3.6e9 and more, each shown
   spurious by the cycles' Floquet multipliers (docs/auto-references.md W547). */
inline bool autox_ratio_in_scale(double start, double end, double step_length, double tolerance) {
  return std::max(std::abs(start), std::abs(end)) * tolerance <= step_length;
}

/* the two continuation drivers: continue_algebraic_problem (algebraic problems) and continue_boundary_problem
   (boundary value problems, periodic orbits included) */
enum class AutoxSolver { Algebraic, BoundaryValue };

/* autobv_function_list: the earlier name of this table */
struct boundary_function_table {
  ProblemFunction problem_function;
  BoundaryFunction boundary_function;
  IntegralFunction integral_function;
  UserStartBoundary user_start;
  OutputParametersBoundary pvli;
};

/* autoae_function_list: the earlier name of this table */
struct algebraic_function_table {
  ProblemFunction problem_function;
  UserStartAlgebraic user_start;
  OutputParametersAlgebraic pvli;
};

/* a run's problem functions (gogoautox.cpp problem_functions): the driver's
   list; the other is empty */
/* function_list: the earlier name of this table */
struct problem_function_table {
  AutoxSolver solver;
  boundary_function_table bvlist{};
  algebraic_function_table aelist{};
};

/* autox_lib1.cpp */
void size_run_scratch(const iap_type &iap);
int check_dimensions(iap_type *iap);
int continue_algebraic_problem(iap_type *iap, rap_type *rap, double *par, 
std::ptrdiff_t *icp, 
ProblemFunction problem_function, 
UserStartAlgebraic user_start, 
OutputParametersAlgebraic pvli, 
double *thl, double *thu, std::span<const AutoxMark> marks);

int continue_boundary_problem(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, UserStartBoundary user_start, OutputParametersBoundary pvli, double *thl, double *thu, std::span<const AutoxMark> marks);
int initialize_constants(iap_type *iap, rap_type *rap, std::ptrdiff_t *icp, double *par);
int control_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, UserStartAlgebraic user_start, OutputParametersAlgebraic pvli, double *thl, double *thu, std::span<const AutoxMark> marks);
int user_start_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int algebraic_start(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int start_direction_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rhs, double *rlold, double *rldot, double *u, double *du, double *uold, double *udot, double *f, double *dfdu, double *dfdp, double *thl, double *thu);
int second_point_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, double *rds, std::ptrdiff_t *m1aaloc, double *aa, double *rhs, double *rlcur, double *rlold, double *rldot, double *u, double *du, double *uold, double *udot, double *f, double *dfdu, double *dfdp, double *thl, double *thu);
int extrapolate_algebraic(iap_type *iap, rap_type *rap, double *rds, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot);
int solve_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, double *rds, std::ptrdiff_t *m1aaloc, double *aa, double *rhs, double *rlcur, double *rlold, double *rldot, double *u, double *du, double *uold, double *udot, double *f, double *dfdu, double *dfdp, double *thl, double *thu);
int locate_special_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, AlgebraicTestFunction fncs, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rhs, double *rlcur, double *rlold, double *rldot, double *u, double *du, double *uold, double *udot, double *f, double *dfdu, double *dfdp, AutoxTestSample &q, double *thl, double *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept = nullptr);
int mueller_root(double *q0, double *q1, double *q, double *s0, double *s1, double *s, double *rds);
double branch_point_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double fold_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double hopf_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double bogdanov_takens_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double resonance_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double cusp_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double generalized_hopf_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double user_zero_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
double closed_test_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rlcur, double *rlold, double *rldot, double *u, double *uold, double *udot, double *rhs, double *dfdu, double *dfdp, std::span<const AutoxMark> marks);
int store_branching_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *m1aaloc, double *aa, double *stud, double *stu, double *stla, double *stld, double *rlcur, double *rlold, double *rldot, double *u, double *du, double *udot, double *dfdu, double *dfdp, double *thl, double *thu);
int retrieve_branching_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rds, double *stud, double *stu, double *stla, double *stld, double *rlcur, double *rlold, double *rldot, double *u, double *udot);
int second_branch_point(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, std::ptrdiff_t *m1aaloc, double *aa, double *rhs, double *rlcur, double *rlold, double *rldot, double *u, double *du, double *uold, double *udot, double *f, double *dfdu, double *dfdp, double *rds, double *thl, double *thu);
int write_header(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *thl, double *thu);
int write_headings(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t iunit, std::ptrdiff_t *n1, std::ptrdiff_t *n2);
int store_diagram_algebraic(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rlcur, double *u, const double *udot, const double *rldot);
int write_diagram_line(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *icu, std::ptrdiff_t *ibr, std::ptrdiff_t *ntot, std::ptrdiff_t *lab, double *vaxis, double *u);
int write_special_point_restart(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *lab, double *rlcur, double *u, const double *udot, const double *rldot);
int write_jacobian(iap_type *iap, std::ptrdiff_t *n, std::ptrdiff_t *m1aaloc, double *aa, double *rhs);
// Identical starting mesh preparation; formulation equations stay in callers.
int autox_uniform_start(iap_type *iap, rap_type *rap, double *par,
                       std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs,
                       double *rlcur, std::ptrdiff_t ndxloc, double *ups,
                       double *tm, std::ptrdiff_t *nodir,
                       const std::function<void(double *, double &)> &evaluate);
void autox_hopf_mesh(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndxloc,
                    double *ups, double *udotps, double *upoldp,
                    double *tm, double *dtm, double *rldot,
                    const double *u, const double *rnllv, double tpi);
int uniform_mesh(const iap_type *iap, const rap_type *rap, double *tm);
int collocation_weights(const std::ptrdiff_t ncol, const std::ptrdiff_t n1, double *wt, double *wp);
int collocation_point_positions(const std::ptrdiff_t ncol, double *zm);
int central_difference_coefficients(std::ptrdiff_t *n, double *d);
int integration_weights(const std::ptrdiff_t n, double *wi);
int adapt_step(iap_type *iap, rap_type *rap, double *rds);
int adapt_mesh(iap_type *iap, rap_type *rap, std::ptrdiff_t *nold, std::ptrdiff_t *ncold, std::ptrdiff_t *nnew, std::ptrdiff_t *ncnew, double *tm, double *dtm, std::ptrdiff_t *ndxloc, double *ups, double *vps);
int interpolate_mesh(iap_type *iap, rap_type *rap, std::ptrdiff_t *ndim, std::ptrdiff_t *n, std::ptrdiff_t *nc, double *tm, std::ptrdiff_t *ndxloc, double *ups, std::ptrdiff_t *n1, std::ptrdiff_t *nc1, double *tm1, double *ups1, double *tm2, std::ptrdiff_t *itm1);
int redistribute_mesh(iap_type *iap, rap_type *rap, std::ptrdiff_t *ndxloc, double *ups, std::ptrdiff_t *nold, std::ptrdiff_t *ncold, double *tmold, double *dtmold, std::ptrdiff_t *nnew, double *tmnew, std::ptrdiff_t *iper);
int order_mesh_points(iap_type *iap, rap_type *rap, std::ptrdiff_t *n, double *tm, std::ptrdiff_t *n1, double *tm1, std::ptrdiff_t *itm1);
int lagrange_weights(iap_type *iap, rap_type *rap, std::ptrdiff_t *n, double *z__, double *x, double *wts);
int equidistribution_function(iap_type *iap, rap_type *rap, std::ptrdiff_t *ntst, std::ptrdiff_t *ndim, std::ptrdiff_t *ncol, double *dtm, std::ptrdiff_t *ndxloc, double *ups, double *eqf, std::span<double> density, std::ptrdiff_t *iper);
int normalize_vector(std::ptrdiff_t *ndim, double *v);
double multiple_of_pi(double r__);
int new_restart_label(iap_type *iap, rap_type *rap);
// Locate and re-read the validated restart header once for all formulations.
/* fort.3's restart data: a missing label or a short read ends the run
   (xpp::AutoxFailed), naming the label, the file and what was missing */
[[noreturn]] void autox_restart_label_missing(const iap_type *iap);
void autox_restart_read(const iap_type *iap, bool ok, std::string_view what);
void read_autox_restart_header(iap_type *iap, const rap_type *rap,
                              xpp::TokenReader &reader, xpp::AutoxSolutionHeader &header,
                              std::ptrdiff_t *ntst, std::ptrdiff_t *ncol);
int find_restart_label(iap_type *iap, const rap_type *rap, std::ptrdiff_t irs, std::ptrdiff_t *nfpr, bool *found);
void read_restart_label(const iap_type *iap, const rap_type *rap, double *u, double *par);
double mesh_inner_product(iap_type *iap, const std::ptrdiff_t *ndim1, const std::ptrdiff_t *ndxloc, double *ups, double *vps, double *dtm, double *thu);
double norm_squared(iap_type *iap, std::ptrdiff_t *ndim1, std::ptrdiff_t *ndxloc, double *ups, double *dtm, double *thu);
double integral_of_component(iap_type *iap, std::ptrdiff_t *ndxloc, std::ptrdiff_t ic, double *ups, double *dtm);
double component_norm(iap_type *iap, std::ptrdiff_t *ndxloc, std::ptrdiff_t *ic, double *ups, double *dtm);
double component_maximum(iap_type *iap, std::ptrdiff_t *ndxloc, std::ptrdiff_t *i__, double *ups);
double component_minimum(iap_type *iap, std::ptrdiff_t *ndxloc, std::ptrdiff_t *i__, double *ups);
int scale_direction(iap_type *iap, std::ptrdiff_t *icp, std::ptrdiff_t *ndxloc, double *dvps, double *rld, double *dtm, double *thl, double *thu);
int control_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, UserStartBoundary user_start, OutputParametersBoundary pvli, double *thl, double *thu, std::span<const AutoxMark> marks);
int extrapolate_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, double *rds, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *dtm, double *thl, double *thu);
int predict_boundary_null_space(iap_type *iap, rap_type *rap, ProblemFunction problem_function, double *rds, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps);
int store_derivative(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *upoldp);
int step_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, OutputParametersBoundary pvli, double *rds, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *dups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *tm, double *dtm, double *p0, double *p1, double *thl, double *thu);
int restart_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, UserStartBoundary user_start, double *rds, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *dups, double *tm, double *dtm, xpp::ComplexPair *ev, std::ptrdiff_t *nodir, double *thl, double *thu);
int boundary_start(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int user_start_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int initialize_rotations(iap_type *iap, std::ptrdiff_t *ntst, std::ptrdiff_t *ndxloc, double *ups, double *par);
int start_direction_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t ndxloc, double *ups, double *dups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dtm, std::ptrdiff_t iperp, double *p0, double *p1, double *thl, double *thu);
int locate_special_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, BoundaryTestFunction fncs, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, OutputParametersBoundary pvli, AutoxTestSample &q, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *dups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *tm, double *dtm, double *p0, double *p1, xpp::ComplexPair *ev, double *thl, double *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept = nullptr);
double fold_test_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);
double branch_point_test_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);
double secondary_bifurcation_test_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);
// Moves the multiplier nearest a fold or period-doubling curve's imposed +1/-1
// (or the conjugate pair it has split into) next to the neutral one at [0];
// returns how many multipliers now lead the rest.
std::size_t autox_cycle_place_imposed(xpp::AutoxPointKind start, std::span<xpp::ComplexPair> spectrum);
// Cycle-curve part of secondary_bifurcation_test_boundary, using its ordered original Floquet spectrum.
double autox_cycle_test(const iap_type &iap, const rap_type &rap, const double *par,
                       std::span<const xpp::ComplexPair> spectrum, bool *changed);
double user_zero_test_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);
double closed_test_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, bool *chng, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *p0, double *p1, xpp::ComplexPair *ev, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *uoldps, double *udotps, double *upoldp, double *fa, double *fc, double *dups, double *tm, double *dtm, double *thl, double *thu, std::span<const AutoxMark> marks);
int secondary_bifurcation_type(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, xpp::ComplexPair *ev);
int store_diagram_boundary(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *tm, double *dtm, double *thl, double *thu);
int write_solution_restart(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *tm, double *dtm);
int write_solution_extra(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rlcur, std::ptrdiff_t *ndxloc, double *ups, double *tm, double *dtm, double *thl, double *thu);
int output_parameters_algebraic(iap_type *iap, rap_type *rap, double *u, double *par);
int output_parameters_boundary(iap_type *iap, rap_type *rap, std::ptrdiff_t *icp, double *dtm, std::ptrdiff_t *ndxloc, double *ups, std::ptrdiff_t *ndim, double *p0, double *p1, double *par);
/* autox_lib2.cpp */
int solve_boundary(std::ptrdiff_t *ifst, iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, double *rds, std::ptrdiff_t *nllv, double *rlcur, double *rlold, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *dups, double *uoldps, double *udotps, double *upoldp, double *dtm, double *fa, double *fc, double *p0, double *p1, double *thl, double *thu);
int transpose_right_hand_side(double *ff, double *fa, std::ptrdiff_t *ntst, std::ptrdiff_t *nrow, std::ptrdiff_t *ndxloc);
int build_right_hand_side(std::ptrdiff_t *ndim, xpp::AutoxProblem *ips, std::ptrdiff_t *na, std::ptrdiff_t *ncol, std::ptrdiff_t *nbc, std::ptrdiff_t *nint, std::ptrdiff_t *ncb, std::ptrdiff_t *nrc, std::ptrdiff_t *nra, std::ptrdiff_t *nca, ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, std::ptrdiff_t *ndxloc, iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *rds, double *fa, double *fc, double *rlcur, double *rlold, double *rldot, double *ups, double *uoldps, double *udotps, double *upoldp, double *dups, double *dtm, double *thl, double *thu, double *p0, double *p1);
int solve_bordered_block_diagonal(AutoxRun &run, FILE *fp9, double *fa, double *fc, double *p0, double *p1, std::ptrdiff_t *ifst, std::ptrdiff_t *idb, std::ptrdiff_t *nllv, double *det, std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nbc, std::ptrdiff_t *nra, std::ptrdiff_t *nca, std::ptrdiff_t *ncb, std::ptrdiff_t *nrc);
int zero_right_hand_side(double *fa, double *fc, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nrc);
int condense_right_hand_side(std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nca, double *a, std::ptrdiff_t *nbc, std::ptrdiff_t *nrc, double *c, double *fa, double *fc, std::ptrdiff_t *irf, std::ptrdiff_t *icf);
int copy_condensed_system(std::ptrdiff_t *na, std::ptrdiff_t *nov, std::ptrdiff_t *nra, std::ptrdiff_t *nca, double *a, std::ptrdiff_t *ncb, double *b, std::ptrdiff_t *nrc, double *c, double *a1, double *a2, double *bb, double *cc, double *c2, std::ptrdiff_t *irf);
int copy_right_hand_side(std::ptrdiff_t *na, std::ptrdiff_t *nov, std::ptrdiff_t *nra, double *faa, double *fa, std::ptrdiff_t *irf);
int solve_end_system(AutoxRun &run, FILE *fp9, double *e, double *cc, double *c2, double *d, double *fc, std::ptrdiff_t *ifst, std::ptrdiff_t *na, std::ptrdiff_t *nrc, std::ptrdiff_t *nov, std::ptrdiff_t *ncb, std::ptrdiff_t *idb, std::ptrdiff_t *nllv, double *fcc, double *p0, double *p1, double *det, double *a1, double *a2, double *faa, double *bb);
int print_system(FILE *fp9, std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nca, std::ptrdiff_t *ncb, std::ptrdiff_t *nrc, double *a, double *b, double *c, double *d, double *fa, double *fc);
/* autox_lib3.cpp */
int fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int optimization_single_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int optimization_single_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int optimization_multi_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int optimization_multi_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int optimization_multi_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int discrete_map_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int time_integration_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int hopf_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int hopf_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int hopf_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int hopf_wave_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int hopf_wave_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int hopf_wave_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *u);
int periodic_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int periodic_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int periodic_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int period_doubling_restart(const iap_type *iap, const rap_type *rap, std::ptrdiff_t *ndim, std::ptrdiff_t *ntst, std::ptrdiff_t *ncol, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *tm, double *par);
int periodic_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int wave_steady_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int wave_steady_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp);
int wave_train_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int wave_train_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int parabolic_stationary_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int parabolic_stationary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp);
int parabolic_evolution_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int parabolic_evolution_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp, std::ptrdiff_t ndm, double *dfu, double *dfp);
int parabolic_evolution_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int periodic_fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int periodic_fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int periodic_fold_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int periodic_fold_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int periodic_fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int period_doubling_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int period_doubling_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int period_doubling_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int period_doubling_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int period_doubling_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int torus_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int torus_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int torus_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0,const  double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int torus_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int torus_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int periodic_optimization_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int periodic_optimization_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const double *upold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int periodic_optimization_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int periodic_optimization_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int periodic_optimization_integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, std::ptrdiff_t nnt0, const double *u, const double *uold, const double *udot, const double *upold, double *fi, double *dint, std::ptrdiff_t ndmt, double *dfdu, double *dfdp);
int periodic_optimization_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int boundary_fold_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int boundary_fold_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int boundary_fold_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int boundary_fold_boundary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, std::ptrdiff_t nbc0, const double *u0, const double *u1, double *f, double *dbc);
int boundary_fold_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int boundary_fold_integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, std::ptrdiff_t nnt0, const double *u, const double *uold, const double *udot, const double *upold, double *f, double *dint);
int boundary_fold_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsrs, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int problem_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int boundary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int objective_interface(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
/* autox_lib5.cpp */
int homoclinic_equations(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t ijac, double *f, double *dfdu, double *dfdp);
int homoclinic_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, const double *u, const double *uold, const std::ptrdiff_t *icp, double *par, double *f, std::ptrdiff_t ndm, double *dfdu, double *dfdp);
int homoclinic_boundary_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, const double *u0, const double *u1, double *f, std::ptrdiff_t ijac, double *dbc);
int homoclinic_boundary_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc, std::ptrdiff_t nbc0, const double *u0, const double *u1, double *fb, double *dbc, bool save_basis);
int homoclinic_integral_conditions(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, const double *u, const double *uold, const double *udot, const double *upold, double *f, std::ptrdiff_t ijac, double *dint);
int homoclinic_integral_function(const iap_type *iap, const rap_type *rap, std::ptrdiff_t ndim, double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, std::ptrdiff_t nnt0, const double *u, const double *uold, const double *udot, const double *upold, double *fi, double *dint);
int homoclinic_read_constants(iap_type *iap, std::ptrdiff_t *icp, double *par);
int homoclinic_preprocess(AutoxRun &run, std::ptrdiff_t *ndx, std::ptrdiff_t *ntsr, std::ptrdiff_t *nar, std::ptrdiff_t *ndim, std::ptrdiff_t *ncolrs, double *ups, double *udotps, double *tm, double *par);
int homoclinic_starting_data(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, std::ptrdiff_t *ntsr, std::ptrdiff_t *ncolrs, double *rlcur, double *rldot, std::ptrdiff_t *ndxloc, double *ups, double *udotps, double *upoldp, double *tm, double *dtm, std::ptrdiff_t *nodir, double *thl, double *thu);
int homoclinic_start_point(iap_type *iap, std::ptrdiff_t *icp, double *u, double *par, double *t);
int homoclinic_output_parameters(iap_type *iap, rap_type *rap, std::ptrdiff_t *icp, double *dtm, std::ptrdiff_t *ndxloc, double *ups, std::ptrdiff_t *ndim, double *p0, double *p1, double *par);
double homoclinic_degeneracy_condition(const iap_type *iap, std::ptrdiff_t is, double *rr, double *ri, double *v, double *vt, const std::ptrdiff_t *icp, double *par);
int homoclinic_eigen(const iap_type *iap, std::ptrdiff_t isign, std::ptrdiff_t itrans, double *rr, double *ri, double *vret, double *xequib, const std::ptrdiff_t *icp, double *par, std::ptrdiff_t *ndm);
int homoclinic_projection(const iap_type *iap, double *bound, double *xequib, const std::ptrdiff_t *icp, double *par, xpp::HomcontManifold manifold, xpp::HomcontEndpoint endpoint, bool transpose, std::ptrdiff_t *ndm, bool save_basis);


/* problem defined functions (user_right_hand_side, user_start and user_boundary_conditions, which run the
   model, are below) */
int user_integral_conditions(std::ptrdiff_t ndim, const double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nint, 
	 const double *u, const double *uold, const double *udot, 
	 const double *upold, std::ptrdiff_t ijac,
	 double *fi, double *dint);
int user_objective(std::ptrdiff_t ndim, const double *u, const std::ptrdiff_t *icp, 
	 const double *par, std::ptrdiff_t ijac, 
	 double *fs, double *dfdu, double *dfdp);
int user_output_parameters(std::ptrdiff_t ndim, const double *u, double *par);
/* conpar2.cpp */
int condense_parameters(AutoxRun &run, std::ptrdiff_t *nov, std::ptrdiff_t *na, std::ptrdiff_t *nra, std::ptrdiff_t *nca, double *a, std::ptrdiff_t *ncb, double *b, std::ptrdiff_t *nbc, std::ptrdiff_t *nrc, double *c, double *d, std::ptrdiff_t *irf, std::ptrdiff_t *icf);
/* setubv2.cpp */
int setup_boundary_system(std::ptrdiff_t ndim, xpp::AutoxProblem ips, std::ptrdiff_t na, std::ptrdiff_t ncol, std::ptrdiff_t nbc, std::ptrdiff_t nint, std::ptrdiff_t ncb, std::ptrdiff_t nrc, std::ptrdiff_t nra, std::ptrdiff_t nca, 
	   ProblemFunction problem_function, BoundaryFunction boundary_function, IntegralFunction integral_function, std::ptrdiff_t ndxloc, iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, 
	   double rds, xpp::Matrix3<double> aa, xpp::Matrix3<double> bb, xpp::Matrix3<double> cc, xpp::Matrix<double> dd, double *fa, double *fc, double *rlcur, 
	   double *rlold, double *rldot, double *ups, double *uoldps, double *udotps, double *upoldp, double *dups, 
	   double *dtm, double *thl, double *thu, double *p0, double *p1);
/* gogoautox.cpp */

namespace xpp {
struct Session; /* session.h */
}

/* autox_pp.cpp: a Mark value reads a free parameter from autoX, a constant
   model parameter from the Session, or an autoX unknown from its own slots. */
// autoX's wave Jacobian classifies its free columns at the model boundary.
bool autox_model_parameter(const xpp::Session &s, std::ptrdiff_t slot);
double autox_parameter_value(const AutoxRun &run, std::ptrdiff_t slot,
                           std::span<const std::ptrdiff_t> free, std::span<const double> parameters);
/* the constants the run evaluates on: its own copies when it has them (a
   collocation worker, a job run at once with others), else the Session's parser */
std::span<const double> autox_constants(const AutoxRun &run);
/* the constants and variables the run's evaluations leave their last values in (the same two vectors as above) */
std::pair<std::vector<double> &,std::vector<double> &> autox_rhs_values(AutoxRun &run);
/* the model's parameters, as the run holds them, into autoX's par (the model's part of it): a start's */
void autox_fill_parameters(const AutoxRun &run, double *par);
/* the run's Session, and what its evaluations read from it on every call (AutoxEvaluation) */
void autox_bind_session(AutoxRun &run, xpp::Session &s);
/* autoX's checkpoint (xpp::byeautox_), for a run on the main thread; a job's
   thread only reads whether the job was cancelled: the front end is polled by
   the main thread that waits for it */
void autox_checkpoint(const AutoxRun &run, int *iflag);
/* the problem defined functions that run the model (autox_pp.cpp): its
   right-hand side and Jacobian, the starting point and the boundary
   conditions, on the run's Session (run.session; a collocation worker's run
   evaluates on its own copies, autox_parallel.cpp).
   user_right_hand_side gives dfdu for ijac 1 and the dfdp columns of the nfpr
   continuation parameters icp too for ijac 2 (JAC=1: the model's exact
   Jacobian, W260) */
int user_right_hand_side(AutoxRun &run, std::ptrdiff_t ndim, const double *u, const std::ptrdiff_t *icp, std::ptrdiff_t nfpr,
	 const double *par, std::ptrdiff_t ijac,
	 double *f, double *dfdu, double *dfdp);
/* f at u, right after a user_right_hand_side call at the same par and icp (the differences
   in the states: problem_function's): the constants user_right_hand_side put in are current, so a
   collocation worker's run only evaluates the right-hand side */
void func_again(AutoxRun &run, std::ptrdiff_t ndim, const double *u, const std::ptrdiff_t *icp, std::ptrdiff_t nfpr,
	 const double *par, double *f);
int user_start(AutoxRun &run, std::ptrdiff_t ndim, double t,
	  double *u, double *par);
int user_boundary_conditions(AutoxRun &run, std::ptrdiff_t ndim, const double *par, const std::ptrdiff_t *icp, std::ptrdiff_t nbc,
	 const double *u0, const double *u1, std::ptrdiff_t ijac,
	 double *f, double *dbc);
/* gogoautox.cpp: the autoX run, from the parameters do_autox (autox_nox.cpp)
   put in run.params */
void run_autox_problem(AutoxRun &run);
/* autox_lib1.cpp: fills thu (ndim*8 entries, each 1.) that thu_vec owns;
   thu_vec.data() is what the AE/BVP solves take as their double *thu */
int initialize_run(iap_type *iap, rap_type *rap, double *par, std::ptrdiff_t *icp, double *thl, std::vector<double> &thu_vec, std::span<AutoxMark> marks);
