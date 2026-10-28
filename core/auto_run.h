#pragma once
/* A run of AUTO (W516, W306): what is computed (a kind, and a branch switch
   when it is one), from where (the current state, or the grabbed point), and
   the one table that says which kinds a start point's type allows. The
   registry's auto.run (commands_auto.cpp), the AUTO window's Run key and
   the error action that goes on from an end point all go through
   auto_run_start; hello sends the table, and the `autoinfo` event the kinds
   that fit the point grabbed now, so a front end offers only those.

   auto_run.cpp. */
#include <array>
#include <optional>
#include <string>
#include <vector>
#include "auto_nox.h"
#include "auto_codes.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the branch a start point is on: its problem as the diagram records it */
enum class AutoFamily : int { any, steady, periodic, boundary_value, homoclinic };
inline constexpr std::array<const char *, 5> auto_family_names = {"any", "steady", "periodic", "boundary_value", "homoclinic"};
constexpr const char *auto_family_name(AutoFamily family) { return auto_family_names[static_cast<std::size_t>(family)]; }

constexpr unsigned auto_run_bit(AutoRunKind kind) { return 1u << static_cast<unsigned>(kind); }
constexpr unsigned auto_run_bits(std::initializer_list<AutoRunKind> kinds)
{
    unsigned bits = 0;
    for (const AutoRunKind kind : kinds) bits |= auto_run_bit(kind);
    return bits;
}

/* one row: a point type (its name; `other` for any other type, `neutral_saddle` for a point of that curve), the
   family it must be of (`any` for all), the kinds it allows and the ones of those that may be a branch switch */
struct AutoRunRow {
    const char *point;
    AutoFamily family;
    unsigned kinds;
    unsigned switch_kinds;
};

/* the kinds a run from the current state computes (no start point) */
inline constexpr unsigned auto_run_current_kinds =
    auto_run_bits({AutoRunKind::equilibrium, AutoRunKind::periodic, AutoRunKind::bvp, AutoRunKind::homoclinic,
                   AutoRunKind::heteroclinic});

/* The kind-by-point-type table: the first row of the start point's effective type (a point or the point a
   two-parameter curve started from, in this order: Hopf, period doubling, fold, cycle fold, branch point, torus)
   whose family fits. XPPAUT's Run menus offered exactly these. */
inline constexpr std::array<AutoRunRow, 14> auto_run_rows = {{
    {"neutral_saddle", AutoFamily::any, auto_run_bits({AutoRunKind::two_parameter}), 0},
    {"hopf", AutoFamily::any,
     auto_run_bits({AutoRunKind::equilibrium, AutoRunKind::periodic, AutoRunKind::two_parameter}), 0},
    {"period_doubling", AutoFamily::any,
     auto_run_bits({AutoRunKind::periodic, AutoRunKind::fixed_period, AutoRunKind::two_parameter}),
     auto_run_bits({AutoRunKind::periodic})},
    {"fold", AutoFamily::any, auto_run_bits({AutoRunKind::two_parameter}), 0},
    {"cycle_fold", AutoFamily::any, auto_run_bits({AutoRunKind::two_parameter}), 0},
    {"cycle_branch_point", AutoFamily::any, auto_run_bits({AutoRunKind::periodic, AutoRunKind::two_parameter}),
     auto_run_bits({AutoRunKind::periodic})},
    {"branch_point", AutoFamily::steady, auto_run_bits({AutoRunKind::equilibrium, AutoRunKind::two_parameter}),
     auto_run_bits({AutoRunKind::equilibrium})},
    {"branch_point", AutoFamily::periodic, auto_run_bits({AutoRunKind::periodic, AutoRunKind::two_parameter}),
     auto_run_bits({AutoRunKind::periodic})},
    {"branch_point", AutoFamily::boundary_value, auto_run_bits({AutoRunKind::bvp, AutoRunKind::two_parameter}),
     auto_run_bits({AutoRunKind::bvp})},
    {"neimark_sacker", AutoFamily::any,
     auto_run_bits({AutoRunKind::periodic, AutoRunKind::fixed_period, AutoRunKind::two_parameter}), 0},
    {"other", AutoFamily::steady, auto_run_bits({AutoRunKind::equilibrium}), 0},
    {"other", AutoFamily::periodic, auto_run_bits({AutoRunKind::periodic, AutoRunKind::fixed_period}), 0},
    {"other", AutoFamily::boundary_value, auto_run_bits({AutoRunKind::bvp}), 0},
    {"other", AutoFamily::homoclinic, auto_run_bits({AutoRunKind::homoclinic, AutoRunKind::heteroclinic}), 0}}};

/* the family of the branch the grabbed point is on */
AutoFamily auto_family_of(const GRABPT &point);
/* the row for the grabbed point */
const AutoRunRow &auto_run_row_for(const GRABPT &point);

/* an equilibrium (or two) a homoclinic or heteroclinic run starts from, with the dimensions of their unstable and
   stable manifolds */
struct HomoclinicStart {
    int unstable = 0, stable = 0;
    std::vector<double> left, right;
};

struct AutoRunRequest {
    AutoRunKind kind = AutoRunKind::equilibrium;
    bool switch_branch = false;
    /* the start is the grabbed point; else the current state (the initial conditions and the last integration) */
    bool from_point = false;
    /* a new start from the current state begins the diagram afresh; else the new branch is added to it */
    bool replace_diagram = false;
    /* a homoclinic or heteroclinic run's equilibria; none: the session's own */
    std::optional<HomoclinicStart> homoclinic;
};

/* The run: its start checked against the table, the model against t (auto_time_check), then the problem set up and
   AUTO run; what went wrong is the error (a start the kind does not fit names both). */
Result<> auto_run_start(Session &s, const AutoRunRequest &request);

/* A model whose equations AUTO runs depends on t, directly or through a function or a fixed variable (the auxiliary
   quantities do not count), is refused: AUTO evaluates them at t = 0. The error names each such equation's file and
   line. */
Result<> auto_time_check(const Session &s);

/* the form XPPAUT's homoclinic starts asked: the equilibria's dimensions and values, none when cancelled */
std::optional<HomoclinicStart> auto_ask_homoclinic(Session &s);

/* the AUTO window's Run key: the questions of XPPAUT's Run menus, then auto_run_start */
void auto_run(Session &s);

/* the table as hello sends it ("auto_run": {...}) and the kinds that fit the grabbed point as `autoinfo` sends them
   ("start": {...} or null) */
std::string auto_run_table_json();
std::string auto_run_start_json(const Session &s);

} // namespace xpp
