#pragma once
/* A run of autoX (W516, W306): what is computed (a kind, and a branch switch
   when it is one), from where (the current state, or the grabbed point), and
   the one table that says which kinds a start point's type allows. The
   registry's autox.run (commands_autox.cpp), the autoX window's Run key and
   the error action that goes on from an end point all go through
   autox_run_start; hello sends the table, and the `autoxinfo` event the kinds
   that fit the point grabbed now, so a front end offers only those.

   autox_run.cpp. */
#include <array>
#include <optional>
#include <string>
#include <vector>
#include <cstddef>
#include "autox_nox.h"
#include "autox_codes.h"
#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */

/* the branch a start point is on: its problem as the diagram records it */
enum class AutoxFamily : int { any, steady, periodic, boundary_value, homoclinic };
inline constexpr std::array<const char *, 5> autox_family_names = {"any", "steady", "periodic", "boundary_value", "homoclinic"};
constexpr const char *autox_family_name(AutoxFamily family) { return autox_family_names[static_cast<std::size_t>(family)]; }

constexpr unsigned autox_run_bit(AutoxRunKind kind) { return 1u << static_cast<unsigned>(kind); }
constexpr unsigned autox_run_bits(std::initializer_list<AutoxRunKind> kinds)
{
    unsigned bits = 0;
    for (const AutoxRunKind kind : kinds) bits |= autox_run_bit(kind);
    return bits;
}

/* one row: a point type (its name; `other` for any other type, `neutral_saddle` for a point of that curve), the
   family it must be of (`any` for all), the kinds it allows and the ones of those that may be a branch switch */
struct AutoxRunRow {
    const char *point;
    AutoxFamily family;
    unsigned kinds;
    unsigned switch_kinds;
};

/* the kinds a run from the current state computes (no start point) */
inline constexpr unsigned autox_run_current_kinds =
    autox_run_bits({AutoxRunKind::equilibrium, AutoxRunKind::periodic, AutoxRunKind::bvp, AutoxRunKind::homoclinic,
                   AutoxRunKind::heteroclinic});

/* The kind-by-point-type table: the first row of the start point's effective type (a point or the point a
   two-parameter curve started from, in this order: Hopf, period doubling, fold, cycle fold, branch point, torus)
   whose family fits. XPPAUT's Run menus offered exactly these. */
inline constexpr std::array<AutoxRunRow, 14> autox_run_rows = {{
    {"neutral_saddle", AutoxFamily::any, autox_run_bits({AutoxRunKind::two_parameter}), 0},
    {"hopf", AutoxFamily::any,
     autox_run_bits({AutoxRunKind::equilibrium, AutoxRunKind::periodic, AutoxRunKind::two_parameter}), 0},
    {"period_doubling", AutoxFamily::any,
     autox_run_bits({AutoxRunKind::periodic, AutoxRunKind::fixed_period, AutoxRunKind::two_parameter}),
     autox_run_bits({AutoxRunKind::periodic})},
    {"fold", AutoxFamily::any, autox_run_bits({AutoxRunKind::two_parameter}), 0},
    {"cycle_fold", AutoxFamily::any, autox_run_bits({AutoxRunKind::two_parameter}), 0},
    {"cycle_branch_point", AutoxFamily::any, autox_run_bits({AutoxRunKind::periodic, AutoxRunKind::two_parameter}),
     autox_run_bits({AutoxRunKind::periodic})},
    {"branch_point", AutoxFamily::steady, autox_run_bits({AutoxRunKind::equilibrium, AutoxRunKind::two_parameter}),
     autox_run_bits({AutoxRunKind::equilibrium})},
    {"branch_point", AutoxFamily::periodic, autox_run_bits({AutoxRunKind::periodic, AutoxRunKind::two_parameter}),
     autox_run_bits({AutoxRunKind::periodic})},
    {"branch_point", AutoxFamily::boundary_value, autox_run_bits({AutoxRunKind::bvp, AutoxRunKind::two_parameter}),
     autox_run_bits({AutoxRunKind::bvp})},
    {"neimark_sacker", AutoxFamily::any,
     autox_run_bits({AutoxRunKind::periodic, AutoxRunKind::fixed_period, AutoxRunKind::two_parameter}), 0},
    {"other", AutoxFamily::steady, autox_run_bits({AutoxRunKind::equilibrium}), 0},
    {"other", AutoxFamily::periodic, autox_run_bits({AutoxRunKind::periodic, AutoxRunKind::fixed_period}), 0},
    {"other", AutoxFamily::boundary_value, autox_run_bits({AutoxRunKind::bvp}), 0},
    {"other", AutoxFamily::homoclinic, autox_run_bits({AutoxRunKind::homoclinic, AutoxRunKind::heteroclinic}), 0}}};

/* the family of the branch the grabbed point is on */
AutoxFamily autox_family_of(const GRABPT &point);
/* the row for the grabbed point */
const AutoxRunRow &autox_run_row_for(const GRABPT &point);

/* an equilibrium (or two) a homoclinic or heteroclinic run starts from, with the dimensions of their unstable and
   stable manifolds */
struct HomoclinicStart {
    int unstable = 0, stable = 0;
    std::vector<double> left, right;
};

struct AutoxRunRequest {
    AutoxRunKind kind = AutoxRunKind::equilibrium;
    bool switch_branch = false;
    /* the start is the grabbed point; else the current state (the initial conditions and the last integration) */
    bool from_point = false;
    /* a new start from the current state begins the diagram afresh; else the new branch is added to it */
    bool replace_diagram = false;
    /* a homoclinic or heteroclinic run's equilibria; none: the session's own */
    std::optional<HomoclinicStart> homoclinic;
};

/* Prospective diagram grab and run checks, without changing values or the selected point. */
const DIAGRAM *autox_point_by_type(const Session &s, AutoxPointKind type, int index);
/* the number autox.run's `index` takes (autox_point_by_type's) of the `index`-th point of `type` among the diagram's points
   [first, last): a script's start from the points one of its runs stored; 0 when there is none */
int autox_type_number(const Session &s, AutoxPointKind type, int first, int last, int index);
GRABPT autox_point_data(const DIAGRAM &point);
Result<> autox_run_check(const Session &s, const AutoxRunRequest &request, const GRABPT *point = nullptr, bool settling = false);

/* The run: its start checked against the table, the model against t (autox_time_check), then the problem set up and
   autoX run; what went wrong is the error (a start the kind does not fit names both). */
Result<> autox_run_start(Session &s, const AutoxRunRequest &request);

/* A model whose equations autoX runs depends on t, directly or through a function or a fixed variable (the auxiliary
   quantities do not count), is refused: autoX evaluates them at t = 0. The error names each such equation's file and
   line. */
Result<> autox_time_check(const Session &s);

/* the form XPPAUT's homoclinic starts asked: the equilibria's dimensions and values, none when cancelled */

/* the autoX window's Run key: the questions of XPPAUT's Run menus, then autox_run_start */

/* the table as hello sends it ("autox_run": {...}) and the kinds that fit the grabbed point as `autoxinfo` sends them
   ("start": {...} or null) */
std::string autox_run_table_json();
std::string autox_run_start_json(const Session &s);

} // namespace xpp
