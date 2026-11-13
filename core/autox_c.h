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
#include "autox_f2c.h"
#include "autox_codes.h"
#include "xAutox.h" /* AutoxMark */
#include "xpp_math.h" /* xpp::Matrix, Matrix3 */
#include "xpp_math_autox.h" /* autoX numerical interfaces, owned by xpp_math */

namespace xpp { class TokenReader; struct AutoxSolutionHeader; }

struct AutoxRun; /* autox_state.h */

/* autoX's numerical thresholds and tolerances (autox_lib1-5, autox_jacobian). */
inline constexpr doublereal autox_hmach = 1.0e-7;            // Relative finite-difference step and convergence tolerance
inline constexpr doublereal autox_rlarge = 1.0e+30;          // Practical infinity for uncomputed bounds and eigenvalues
inline constexpr doublereal autox_hmach1 = autox_hmach + 1.0; // 1 + HMACH for multiplier unit circle comparisons
inline constexpr doublereal autox_hmachho = 1.0e-13;         // HomCont component zero threshold (compzero)
inline constexpr doublereal autox_svd_tolerance = 1.0e-16;   // FLOWKM's SVD tolerance (autoX's own value)
inline constexpr doublereal autox_divergence_ratio = 20;     // A Newton step this many times the first one diverges (autoX's DELREF factor)
inline constexpr doublereal autox_homoclinic_norm_start = 1.0e20; // HomCont's search for the orbit's point nearest the equilibrium starts above any norm
/* autoX's JAC: who supplies the derivatives problem_function passes on */
inline constexpr integer autox_jac_differences = 0;          // autoX differences the right-hand side
inline constexpr integer autox_jac_supplied = 1;             // user_right_hand_side supplies them: the model's exact Jacobian (W260)

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
  /* 1 */ integer ndim;
  /* 2 */ xpp::AutoxProblem ips;
  /* 3 */ integer irs;
  /* 4 */ integer ilp;
  /* 5 */ integer ntst;
  /* 6 */ integer ncol;
  /* 7 */ integer iad;
  /* 8 */ integer iads;
  /* 9 */ integer isp;
  /* 10 */ integer isw;
  /* 11 */ integer iplt;
  /* 12 */ integer nbc;
  /* 13 */ integer nint;
  /* 14 */ integer nmx;
  /* 15 */ integer nuzr;
  /* 16 */ integer npr;
  /* 17 */ integer mxbf;
  /* 18 */ integer iid;
  /* 19 */ integer itmx;
  /* 20 */ integer itnw;
  /* 21 */ integer nwtn;
  /* 22 */ integer jac;
  /* 23 */ integer ndm;
  /* 24 */ integer nbc0;
  /* 25 */ integer nnt0;
  /* 26 */ integer iuzr;
  /* 27 */ xpp::AutoxPointCode itp;
  /* 28 */ xpp::AutoxPointKind itpst; /* the start kind of a two-parameter continuation */
  /* 29 */ integer nfpr;
  /* 30 */ integer ibr;
  /* 31 */ integer nit;
  /* 32 */ integer ntot;
  /* 33 */ integer nins;
  /* 34 */ integer istop;
  /* 35 */ integer nbif;
  /* 36 */ integer ipos;
  /* 37 */ integer lab;
  /* 41 */ integer nicp;
  /* xppautX: the run's own state (autox_state.h), and through it the
     Session the run belongs to; run_autox_problem sets it before init */
  struct AutoxRun *run;
};

typedef struct {
  /* 1 */ doublereal ds;
  /* 2 */ doublereal dsmin;
  /* 3 */ doublereal dsmax;
  /* There is no 4 */
  /* 5 */ doublereal dsold;
  /* 6 */ doublereal rl0;
  /* 7 */ doublereal rl1;
  /* 8 */ doublereal a0;
  /* 9 */ doublereal a1;
  /* 11 */ doublereal epsl;
  /* 12 */ doublereal epsu;
  /* 13 */ doublereal epss;
  /* 14 */ doublereal det;
  /* 15 */ doublereal tivp;
} rap_type;

/*This is the type for all functions which can be used as "problem_function" the function
  which evaluates the right hand side of the equations and generates
  the Jacobian*/
#define FUNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp)

/*This is the type for all functions which can be used as "boundary_function" the function
  which evaluates the boundary conditions */
#define BCNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0, const doublereal *u1, doublereal *f, integer ijac, doublereal *dbc)

/*This is the type for all functions which can be used as "integral_function" the function
  which evaluates kernel of the integral constraints */
#define ICNI_TYPE(X) int X(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, integer ijac, doublereal *dint)

/*This is the type for all functions which can be used as additional
  output functions for algebraic problems */
#define PVLI_TYPE_AE(X) int X(iap_type *iap, rap_type *rap, doublereal *u, doublereal *par)

/*This is the type for all functions which can be used as additional
  output functions for BVPs */
#define PVLI_TYPE_BVP(X) int X(iap_type *iap, rap_type *rap, integer *icp, doublereal *dtm, integer *ndxloc, doublereal *ups, integer *ndim, doublereal *p0, doublereal *p1, doublereal *par)

/* This is the type for all functions that can be used at starting points
   for algebraic problems */
#define STPNT_TYPE_AE(X) int X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *u)

/* This is the type for all functions that can be used at starting points
   for BVPs */
#define STPNT_TYPE_BVP(X) int X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsrs, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu)

/*This is the type for all functions which can be used to detect
  special points for algebraic problems */
#define FNCS_TYPE_AE(X) doublereal X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*problem_function)), integer *m1aaloc, doublereal *aa, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot, doublereal *rhs, doublereal *dfdu, doublereal *dfdp, std::span<const AutoxMark> marks)

/*This is the type for all functions which can be used to detect
  special points for BVPS */
#define FNCS_TYPE_BVP(X) doublereal X(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, bool *chng, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks)

/* W419 (#481): what a test function returns where its test does not apply
   at a point (a Floquet test on a planar orbit, an inaccurate trivial
   multiplier, no Hopf pair, a resonance test with every kind disabled):
   no value, never a zero. A location neither starts from it nor stops on it. */
inline constexpr doublereal autox_test_not_applicable = std::numeric_limits<doublereal>::quiet_NaN();
inline bool autox_test_applies(doublereal value) { return !std::isnan(value); }

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
  doublereal value = 0;
  std::optional<doublereal> offset;
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
inline constexpr doublereal autox_crossing_slope_ratio_max = 3;
inline doublereal autox_crossing_slope_ratio(doublereal step_change, doublereal step_length,
                                            doublereal final_change, doublereal final_width) {
  if (!(step_length > 0 && final_width > 0 && step_change != 0)) return std::numeric_limits<doublereal>::quiet_NaN();
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
inline doublereal autox_zero_order(doublereal q_a, doublereal d_a, doublereal q_b, doublereal d_b) {
  const doublereal distances = xpp::math::log(std::abs(d_a) / std::abs(d_b));
  const doublereal order = xpp::math::log(std::abs(q_a) / std::abs(q_b)) / distances;
  return std::isfinite(order) && distances != 0 ? std::round(order) : 0;
}
inline bool autox_zero_crosses(doublereal order) { return order > 0 && std::fmod(order, 2) == 1; }

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
inline bool autox_ratio_in_scale(doublereal start, doublereal end, doublereal step_length, doublereal tolerance) {
  return std::max(std::abs(start), std::abs(end)) * tolerance <= step_length;
}

/* the two continuation drivers: continue_algebraic_problem (algebraic problems) and continue_boundary_problem
   (boundary value problems, periodic orbits included) */
enum class AutoxSolver { Algebraic, BoundaryValue };

/* autobv_function_list: the earlier name of this table */
struct boundary_function_table {
  FUNI_TYPE((*problem_function));
  BCNI_TYPE((*boundary_function));
  ICNI_TYPE((*integral_function));
  STPNT_TYPE_BVP((*user_start));
  PVLI_TYPE_BVP((*pvli));
};

/* autoae_function_list: the earlier name of this table */
struct algebraic_function_table {
  FUNI_TYPE((*problem_function));
  STPNT_TYPE_AE((*user_start));
  PVLI_TYPE_AE((*pvli));
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
int continue_algebraic_problem(iap_type *iap, rap_type *rap, doublereal *par, 
integer *icp, 
FUNI_TYPE((*problem_function)), 
STPNT_TYPE_AE((*user_start)), 
PVLI_TYPE_AE((*pvli)), 
doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks);

int continue_boundary_problem(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), STPNT_TYPE_BVP((*user_start)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks);
int initialize_constants(iap_type *iap, rap_type *rap, integer *icp, doublereal *par);
int control_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), STPNT_TYPE_AE((*user_start)), PVLI_TYPE_AE((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks);
STPNT_TYPE_AE(user_start_algebraic);
STPNT_TYPE_AE(algebraic_start);
int start_direction_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int second_point_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int extrapolate_algebraic(iap_type *iap, rap_type *rap, doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *uold, doublereal *udot);
int solve_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), doublereal *rds, integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int locate_special_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_AE((*fncs)), FUNI_TYPE((*problem_function)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, AutoxTestSample &q, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept = nullptr);
int mueller_root(doublereal *q0, doublereal *q1, doublereal *q, doublereal *s0, doublereal *s1, doublereal *s, doublereal *rds);
FNCS_TYPE_AE(branch_point_test_algebraic);
FNCS_TYPE_AE(fold_test_algebraic);
FNCS_TYPE_AE(hopf_test_algebraic);
FNCS_TYPE_AE(bogdanov_takens_test_algebraic);
FNCS_TYPE_AE(resonance_test_algebraic);
FNCS_TYPE_AE(cusp_test_algebraic);
FNCS_TYPE_AE(generalized_hopf_test_algebraic);
FNCS_TYPE_AE(user_zero_test_algebraic);
FNCS_TYPE_AE(closed_test_algebraic);
int store_branching_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *m1aaloc, doublereal *aa, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *udot, doublereal *dfdu, doublereal *dfdp, doublereal *thl, doublereal *thu);
int retrieve_branching_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *stud, doublereal *stu, doublereal *stla, doublereal *stld, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *udot);
int second_branch_point(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), integer *m1aaloc, doublereal *aa, doublereal *rhs, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *u, doublereal *du, doublereal *uold, doublereal *udot, doublereal *f, doublereal *dfdu, doublereal *dfdp, doublereal *rds, doublereal *thl, doublereal *thu);
int write_header(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, doublereal *thu);
int write_headings(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer iunit, integer *n1, integer *n2);
int store_diagram_algebraic(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, doublereal *u, const doublereal *udot, const doublereal *rldot);
int write_diagram_line(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *icu, integer *ibr, integer *ntot, integer *lab, doublereal *vaxis, doublereal *u);
int write_special_point_restart(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *lab, doublereal *rlcur, doublereal *u, const doublereal *udot, const doublereal *rldot);
int write_jacobian(iap_type *iap, integer *n, integer *m1aaloc, doublereal *aa, doublereal *rhs);
// Identical starting mesh preparation; formulation equations stay in callers.
int autox_uniform_start(iap_type *iap, rap_type *rap, doublereal *par,
                       integer *icp, integer *ntsr, integer *ncolrs,
                       doublereal *rlcur, integer ndxloc, doublereal *ups,
                       doublereal *tm, integer *nodir,
                       const std::function<void(doublereal *, doublereal &)> &evaluate);
void autox_hopf_mesh(const iap_type *iap, const rap_type *rap, integer ndxloc,
                    doublereal *ups, doublereal *udotps, doublereal *upoldp,
                    doublereal *tm, doublereal *dtm, doublereal *rldot,
                    const doublereal *u, const doublereal *rnllv, doublereal tpi);
int uniform_mesh(const iap_type *iap, const rap_type *rap, doublereal *tm);
int collocation_weights(const integer ncol, const integer n1, doublereal *wt, doublereal *wp);
int collocation_point_positions(const integer ncol, doublereal *zm);
int central_difference_coefficients(integer *n, doublereal *d);
int integration_weights(const integer n, doublereal *wi);
int adapt_step(iap_type *iap, rap_type *rap, doublereal *rds);
int adapt_mesh(iap_type *iap, rap_type *rap, integer *nold, integer *ncold, integer *nnew, integer *ncnew, doublereal *tm, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *vps);
int interpolate_mesh(iap_type *iap, rap_type *rap, integer *ndim, integer *n, integer *nc, doublereal *tm, integer *ndxloc, doublereal *ups, integer *n1, integer *nc1, doublereal *tm1, doublereal *ups1, doublereal *tm2, integer *itm1);
int redistribute_mesh(iap_type *iap, rap_type *rap, integer *ndxloc, doublereal *ups, integer *nold, integer *ncold, doublereal *tmold, doublereal *dtmold, integer *nnew, doublereal *tmnew, integer *iper);
int order_mesh_points(iap_type *iap, rap_type *rap, integer *n, doublereal *tm, integer *n1, doublereal *tm1, integer *itm1);
int lagrange_weights(iap_type *iap, rap_type *rap, integer *n, doublereal *z__, doublereal *x, doublereal *wts);
int equidistribution_function(iap_type *iap, rap_type *rap, integer *ntst, integer *ndim, integer *ncol, doublereal *dtm, integer *ndxloc, doublereal *ups, doublereal *eqf, std::span<doublereal> density, integer *iper);
int normalize_vector(integer *ndim, doublereal *v);
doublereal multiple_of_pi(doublereal r__);
int new_restart_label(iap_type *iap, rap_type *rap);
// Locate and re-read the validated restart header once for all formulations.
/* fort.3's restart data: a missing label or a short read ends the run
   (xpp::AutoxFailed), naming the label, the file and what was missing */
[[noreturn]] void autox_restart_label_missing(const iap_type *iap);
void autox_restart_read(const iap_type *iap, bool ok, std::string_view what);
void read_autox_restart_header(iap_type *iap, const rap_type *rap,
                              xpp::TokenReader &reader, xpp::AutoxSolutionHeader &header,
                              integer *ntst, integer *ncol);
int find_restart_label(iap_type *iap, const rap_type *rap, integer irs, integer *nfpr, bool *found);
void read_restart_label(const iap_type *iap, const rap_type *rap, doublereal *u, doublereal *par);
doublereal mesh_inner_product(iap_type *iap, const integer *ndim1, const integer *ndxloc, doublereal *ups, doublereal *vps, doublereal *dtm, doublereal *thu);
doublereal norm_squared(iap_type *iap, integer *ndim1, integer *ndxloc, doublereal *ups, doublereal *dtm, doublereal *thu);
doublereal integral_of_component(iap_type *iap, integer *ndxloc, integer ic, doublereal *ups, doublereal *dtm);
doublereal component_norm(iap_type *iap, integer *ndxloc, integer *ic, doublereal *ups, doublereal *dtm);
doublereal component_maximum(iap_type *iap, integer *ndxloc, integer *i__, doublereal *ups);
doublereal component_minimum(iap_type *iap, integer *ndxloc, integer *i__, doublereal *ups);
int scale_direction(iap_type *iap, integer *icp, integer *ndxloc, doublereal *dvps, doublereal *rld, doublereal *dtm, doublereal *thl, doublereal *thu);
int control_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), STPNT_TYPE_BVP((*user_start)), PVLI_TYPE_BVP((*pvli)), doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks);
int extrapolate_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *thl, doublereal *thu);
int predict_boundary_null_space(iap_type *iap, rap_type *rap, FUNI_TYPE((*problem_function)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps);
int store_derivative(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *upoldp);
int step_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), PVLI_TYPE_BVP((*pvli)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int restart_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), STPNT_TYPE_BVP((*user_start)), doublereal *rds, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *tm, doublereal *dtm, doublecomplex *ev, integer *nodir, doublereal *thl, doublereal *thu);
STPNT_TYPE_BVP(boundary_start);
STPNT_TYPE_BVP(user_start_boundary);
int initialize_rotations(iap_type *iap, integer *ntst, integer *ndxloc, doublereal *ups, doublereal *par);
int start_direction_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *dtm, integer iperp, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int locate_special_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FNCS_TYPE_BVP((*fncs)), FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), PVLI_TYPE_BVP((*pvli)), AutoxTestSample &q, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *fa, doublereal *fc, doublereal *tm, doublereal *dtm, doublereal *p0, doublereal *p1, doublecomplex *ev, doublereal *thl, doublereal *thu, std::span<const AutoxMark> marks, double &dstest, bool &stepped, const std::function<bool()> *accept = nullptr);
FNCS_TYPE_BVP(fold_test_boundary);
FNCS_TYPE_BVP(branch_point_test_boundary);
FNCS_TYPE_BVP(secondary_bifurcation_test_boundary);
// Moves the multiplier nearest a fold or period-doubling curve's imposed +1/-1
// (or the conjugate pair it has split into) next to the neutral one at [0];
// returns how many multipliers now lead the rest.
std::size_t autox_cycle_place_imposed(xpp::AutoxPointKind start, std::span<doublecomplex> spectrum);
// Cycle-curve part of secondary_bifurcation_test_boundary, using its ordered original Floquet spectrum.
double autox_cycle_test(const iap_type &iap, const rap_type &rap, const double *par,
                       std::span<const doublecomplex> spectrum, bool *changed);
FNCS_TYPE_BVP(user_zero_test_boundary);
FNCS_TYPE_BVP(closed_test_boundary);
int secondary_bifurcation_type(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublecomplex *ev);
int store_diagram_boundary(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu);
int write_solution_restart(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *dtm);
int write_solution_extra(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rlcur, integer *ndxloc, doublereal *ups, doublereal *tm, doublereal *dtm, doublereal *thl, doublereal *thu);
PVLI_TYPE_AE(output_parameters_algebraic);
PVLI_TYPE_BVP(output_parameters_boundary);
/* autox_lib2.cpp */
int solve_boundary(integer *ifst, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), doublereal *rds, integer *nllv, doublereal *rlcur, doublereal *rlold, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *dups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dtm, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, doublereal *thl, doublereal *thu);
int transpose_right_hand_side(doublereal *ff, doublereal *fa, integer *ntst, integer *nrow, integer *ndxloc);
int build_right_hand_side(integer *ndim, xpp::AutoxProblem *ips, integer *na, integer *ncol, integer *nbc, integer *nint, integer *ncb, integer *nrc, integer *nra, integer *nca, FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), integer *ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *rds, doublereal *fa, doublereal *fc, doublereal *rlcur, doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1);
int solve_bordered_block_diagonal(AutoxRun &run, FILE *fp9, doublereal *fa, doublereal *fc, doublereal *p0, doublereal *p1, integer *ifst, integer *idb, integer *nllv, doublereal *det, integer *nov, integer *na, integer *nbc, integer *nra, integer *nca, integer *ncb, integer *nrc);
int zero_right_hand_side(doublereal *fa, doublereal *fc, integer *na, integer *nra, integer *nrc);
int condense_right_hand_side(integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *nbc, integer *nrc, doublereal *c, doublereal *fa, doublereal *fc, integer *irf, integer *icf);
int copy_condensed_system(integer *na, integer *nov, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nrc, doublereal *c, doublereal *a1, doublereal *a2, doublereal *bb, doublereal *cc, doublereal *c2, integer *irf);
int copy_right_hand_side(integer *na, integer *nov, integer *nra, doublereal *faa, doublereal *fa, integer *irf);
int solve_end_system(AutoxRun &run, FILE *fp9, doublereal *e, doublereal *cc, doublereal *c2, doublereal *d, doublereal *fc, integer *ifst, integer *na, integer *nrc, integer *nov, integer *ncb, integer *idb, integer *nllv, doublereal *fcc, doublereal *p0, doublereal *p1, doublereal *det, doublereal *a1, doublereal *a2, doublereal *faa, doublereal *bb);
int print_system(FILE *fp9, integer *nov, integer *na, integer *nra, integer *nca, integer *ncb, integer *nrc, doublereal *a, doublereal *b, doublereal *c, doublereal *d, doublereal *fa, doublereal *fc);
/* autox_lib3.cpp */
FUNI_TYPE(fold_equations);
int fold_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(fold_starting_data);
FUNI_TYPE(optimization_single_equations);
STPNT_TYPE_AE(optimization_single_starting_data);
FUNI_TYPE(optimization_multi_equations);
int optimization_multi_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(optimization_multi_starting_data);
FUNI_TYPE(discrete_map_equations);
FUNI_TYPE(time_integration_equations);
FUNI_TYPE(hopf_equations);
int hopf_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(hopf_starting_data);
FUNI_TYPE(hopf_wave_equations);
int hopf_wave_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_AE(hopf_wave_starting_data);
FUNI_TYPE(periodic_equations);
BCNI_TYPE(periodic_boundary_conditions);
ICNI_TYPE(periodic_integral_conditions);
int period_doubling_restart(const iap_type *iap, const rap_type *rap, integer *ndim, integer *ntst, integer *ncol, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par);
STPNT_TYPE_BVP(periodic_starting_data);
FUNI_TYPE(wave_steady_equations);
int wave_steady_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
FUNI_TYPE(wave_train_equations);
int wave_train_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(parabolic_stationary_equations);
int parabolic_stationary_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
FUNI_TYPE(parabolic_evolution_equations);
int parabolic_evolution_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp, integer ndm, doublereal *dfu, doublereal *dfp);
ICNI_TYPE(parabolic_evolution_integral_conditions);
FUNI_TYPE(periodic_fold_equations);
int periodic_fold_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, integer ijac, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(periodic_fold_boundary_conditions);
ICNI_TYPE(periodic_fold_integral_conditions);
int periodic_fold_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(period_doubling_equations);
int period_doubling_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(period_doubling_boundary_conditions);
ICNI_TYPE(period_doubling_integral_conditions);
int period_doubling_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(torus_equations);
int torus_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
int torus_boundary_conditions(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, const doublereal *u0,const  doublereal *u1, doublereal *f, integer ijac, doublereal *dbc);
ICNI_TYPE(torus_integral_conditions);
int torus_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
FUNI_TYPE(periodic_optimization_equations);
int periodic_optimization_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const doublereal *upold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(periodic_optimization_boundary_conditions);
ICNI_TYPE(periodic_optimization_integral_conditions);
int periodic_optimization_integral_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint, integer ndmt, doublereal *dfdu, doublereal *dfdp);
STPNT_TYPE_BVP(periodic_optimization_starting_data);
FUNI_TYPE(boundary_fold_equations);
int boundary_fold_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(boundary_fold_boundary_conditions);
int boundary_fold_boundary_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *f, doublereal *dbc);
ICNI_TYPE(boundary_fold_integral_conditions);
int boundary_fold_integral_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *f, doublereal *dint);
STPNT_TYPE_BVP(boundary_fold_starting_data);
FUNI_TYPE(problem_function);
BCNI_TYPE(boundary_function);
ICNI_TYPE(integral_function);
int objective_interface(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const integer *icp, doublereal *par, integer ijac, doublereal *f, doublereal *dfdu, doublereal *dfdp);
/* autox_lib5.cpp */
FUNI_TYPE(homoclinic_equations);
int homoclinic_function(const iap_type *iap, const rap_type *rap, integer ndim, const doublereal *u, const doublereal *uold, const integer *icp, doublereal *par, doublereal *f, integer ndm, doublereal *dfdu, doublereal *dfdp);
BCNI_TYPE(homoclinic_boundary_conditions);
int homoclinic_boundary_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nbc, integer nbc0, const doublereal *u0, const doublereal *u1, doublereal *fb, doublereal *dbc, bool save_basis);
ICNI_TYPE(homoclinic_integral_conditions);
int homoclinic_integral_function(const iap_type *iap, const rap_type *rap, integer ndim, doublereal *par, const integer *icp, integer nint, integer nnt0, const doublereal *u, const doublereal *uold, const doublereal *udot, const doublereal *upold, doublereal *fi, doublereal *dint);
int homoclinic_read_constants(iap_type *iap, integer *icp, doublereal *par);
int homoclinic_preprocess(AutoxRun &run, integer *ndx, integer *ntsr, integer *nar, integer *ndim, integer *ncolrs, doublereal *ups, doublereal *udotps, doublereal *tm, doublereal *par);
int homoclinic_starting_data(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, integer *ntsr, integer *ncolrs, doublereal *rlcur, doublereal *rldot, integer *ndxloc, doublereal *ups, doublereal *udotps, doublereal *upoldp, doublereal *tm, doublereal *dtm, integer *nodir, doublereal *thl, doublereal *thu);
int homoclinic_start_point(iap_type *iap, integer *icp, doublereal *u, doublereal *par, doublereal *t);
PVLI_TYPE_BVP(homoclinic_output_parameters);
doublereal homoclinic_degeneracy_condition(const iap_type *iap, integer is, doublereal *rr, doublereal *ri, doublereal *v, doublereal *vt, const integer *icp, doublereal *par);
int homoclinic_eigen(const iap_type *iap, integer isign, integer itrans, doublereal *rr, doublereal *ri, doublereal *vret, doublereal *xequib, const integer *icp, doublereal *par, integer *ndm);
int homoclinic_projection(const iap_type *iap, doublereal *bound, doublereal *xequib, const integer *icp, doublereal *par, xpp::HomcontManifold manifold, xpp::HomcontEndpoint endpoint, bool transpose, integer *ndm, bool save_basis);


/* problem defined functions (user_right_hand_side, user_start and user_boundary_conditions, which run the
   model, are below) */
int user_integral_conditions(integer ndim, const doublereal *par, const integer *icp, integer nint, 
	 const doublereal *u, const doublereal *uold, const doublereal *udot, 
	 const doublereal *upold, integer ijac,
	 doublereal *fi, doublereal *dint);
int user_objective(integer ndim, const doublereal *u, const integer *icp, 
	 const doublereal *par, integer ijac, 
	 doublereal *fs, doublereal *dfdu, doublereal *dfdp);
int user_output_parameters(integer ndim, const doublereal *u, doublereal *par);
/* conpar2.cpp */
int condense_parameters(AutoxRun &run, integer *nov, integer *na, integer *nra, integer *nca, doublereal *a, integer *ncb, doublereal *b, integer *nbc, integer *nrc, doublereal *c, doublereal *d, integer *irf, integer *icf);
/* setubv2.cpp */
int setup_boundary_system(integer ndim, xpp::AutoxProblem ips, integer na, integer ncol, integer nbc, integer nint, integer ncb, integer nrc, integer nra, integer nca, 
	   FUNI_TYPE((*problem_function)), BCNI_TYPE((*boundary_function)), ICNI_TYPE((*integral_function)), integer ndxloc, iap_type *iap, rap_type *rap, doublereal *par, integer *icp, 
	   doublereal rds, xpp::Matrix3<doublereal> aa, xpp::Matrix3<doublereal> bb, xpp::Matrix3<doublereal> cc, xpp::Matrix<doublereal> dd, doublereal *fa, doublereal *fc, doublereal *rlcur, 
	   doublereal *rlold, doublereal *rldot, doublereal *ups, doublereal *uoldps, doublereal *udotps, doublereal *upoldp, doublereal *dups, 
	   doublereal *dtm, doublereal *thl, doublereal *thu, doublereal *p0, doublereal *p1);
/* gogoautox.cpp */

namespace xpp {
struct Session; /* session.h */
}

/* autox_pp.cpp: a Mark value reads a free parameter from autoX, a constant
   model parameter from the Session, or an autoX unknown from its own slots. */
// autoX's wave Jacobian classifies its free columns at the model boundary.
bool autox_model_parameter(const xpp::Session &s, integer slot);
double autox_parameter_value(const AutoxRun &run, integer slot,
                           std::span<const integer> free, std::span<const double> parameters);
/* the constants the run evaluates on: its own copies when it has them (a
   collocation worker, a job run at once with others), else the Session's parser */
std::span<const double> autox_constants(const AutoxRun &run);
/* the constants and variables the run's evaluations leave their last values in (the same two vectors as above) */
std::pair<std::vector<double> &,std::vector<double> &> autox_rhs_values(AutoxRun &run);
/* the model's parameters, as the run holds them, into autoX's par (the model's part of it): a start's */
void autox_fill_parameters(const AutoxRun &run, double *par);
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
int user_right_hand_side(AutoxRun &run, integer ndim, const doublereal *u, const integer *icp, integer nfpr,
	 const doublereal *par, integer ijac,
	 doublereal *f, doublereal *dfdu, doublereal *dfdp);
/* f at u, right after a user_right_hand_side call at the same par and icp (the differences
   in the states: problem_function's): the constants user_right_hand_side put in are current, so a
   collocation worker's run only evaluates the right-hand side */
void func_again(AutoxRun &run, integer ndim, const doublereal *u, const integer *icp, integer nfpr,
	 const doublereal *par, doublereal *f);
int user_start(AutoxRun &run, integer ndim, doublereal t,
	  doublereal *u, doublereal *par);
int user_boundary_conditions(AutoxRun &run, integer ndim, const doublereal *par, const integer *icp, integer nbc,
	 const doublereal *u0, const doublereal *u1, integer ijac,
	 doublereal *f, doublereal *dbc);
/* gogoautox.cpp: the autoX run, from the parameters do_autox (autox_nox.cpp)
   put in run.params */
void run_autox_problem(AutoxRun &run);
/* autox_lib1.cpp: fills thu (ndim*8 entries, each 1.) that thu_vec owns;
   thu_vec.data() is what the AE/BVP solves take as their doublereal *thu */
int initialize_run(iap_type *iap, rap_type *rap, doublereal *par, integer *icp, doublereal *thl, std::vector<doublereal> &thu_vec, std::span<AutoxMark> marks);
