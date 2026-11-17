#pragma once

#include <array>
#include <climits>
#include <cstddef>
#include <span>
#include <string>

#include "xpp_error.h"

/* autoX's Numerics fields as the option table names them: their indices, limits, the words of the output
   detail and what autoX accepts for each. Pure data, below both the option table (model_options.cpp) and
   autoX's options (autox_options.cpp) that read it (W736). */
namespace xpp {

/* The most points a saved or running diagram holds (autox_diagram_points_limit, autox_evd.cpp): the bound where autoX's memory grows, and what a
   session file accepts back; far above normal diagrams while keeping tiny rows from allocating without limit. */
inline constexpr std::size_t saved_points_limit = 1000000;

/* 10000 intervals far exceed shipped meshes while bounding autoX's mesh-sized matrices. */
inline constexpr int autox_max_mesh_intervals = 10000;
/* autoX's collocation coefficients support only orders 2 through 7 (autox_lib1.cpp collocation_point_positions). */
inline constexpr int autox_min_collocation_points = 2;
inline constexpr int autox_max_collocation_points = 7;

/* the Numerics fields in their order, each named for what it sets (W531);
   autoX's own name for it (NTST, DS, MXBF, ...) is a search word in the
   manual and the page's hover, never an input */
enum {
    AUTOX_NUM_MESH_INTERVALS, AUTOX_NUM_MAX_STEPS, AUTOX_NUM_OUTPUT_EVERY, AUTOX_NUM_COLLOCATION_POINTS,
    AUTOX_NUM_STEP_SIZE, AUTOX_NUM_MIN_STEP_SIZE, AUTOX_NUM_MAX_STEP_SIZE,
    AUTOX_NUM_PARAMETER_MIN, AUTOX_NUM_PARAMETER_MAX, AUTOX_NUM_NORM_MIN, AUTOX_NUM_NORM_MAX,
    AUTOX_NUM_PARAMETER_TOLERANCE, AUTOX_NUM_STATE_TOLERANCE, AUTOX_NUM_SPECIAL_POINT_TOLERANCE,
    AUTOX_NUM_MESH_ADAPT_EVERY, AUTOX_NUM_MAX_BRANCH_SWITCHES, AUTOX_NUM_SWITCH_ONE_DIRECTION, AUTOX_NUM_OUTPUT_DETAIL,
    AUTOX_NUM_MAX_LOCATION_ITERATIONS, AUTOX_NUM_MAX_NEWTON_ITERATIONS, AUTOX_NUM_FULL_NEWTON_AFTER,
    AUTOX_NUM_STEP_ADAPT_EVERY, AUTOX_NUM_SUPPRESS_BRANCH_POINTS, AUTOX_NUM_FOLLOW_ALL_BRANCHES,
    AUTOX_NUM_N
};

/* what autoX's IID levels print, named by it (0 to 5 are autoX's own numbers, read on import only) */
inline constexpr std::array<const char *, 6> autox_output_detail_names = {
    "none", "little", "normal", "start_jacobian", "extensive", "debugging"};

enum class AutoxRule { any, positive, nonzero, range };

constexpr int autox_output_detail_levels = static_cast<int>(autox_output_detail_names.size());

/* what autoX accepts for each field, by AUTOX_NUM_*; the key and the shape are the option table's row (autox_options.h) */
struct AutoxNumRule {
    AutoxRule rule;
    double lo, hi; /* AutoxRule::range, inclusive; a boolean's and a choice's index range too */
};
constexpr std::array<AutoxNumRule, AUTOX_NUM_N> autox_num_rules = {{
    {AutoxRule::range, 1, autox_max_mesh_intervals},       /* mesh_intervals */
    {AutoxRule::range, 1, INT_MAX},                       /* max_steps */
    {AutoxRule::range, 1, INT_MAX},                       /* output_every */
    {AutoxRule::range, autox_min_collocation_points, autox_max_collocation_points}, /* collocation_points */
    {AutoxRule::nonzero, 0, 0},                           /* step_size */
    {AutoxRule::positive, 0, 0},                          /* min_step_size */
    {AutoxRule::positive, 0, 0},                          /* max_step_size */
    {AutoxRule::any, 0, 0},                               /* parameter_min */
    {AutoxRule::any, 0, 0},                               /* parameter_max */
    {AutoxRule::any, 0, 0},                               /* norm_min */
    {AutoxRule::any, 0, 0},                               /* norm_max */
    {AutoxRule::positive, 0, 0},                          /* parameter_tolerance */
    {AutoxRule::positive, 0, 0},                          /* state_tolerance */
    {AutoxRule::positive, 0, 0},                          /* special_point_tolerance */
    {AutoxRule::range, 0, INT_MAX},                       /* mesh_adapt_every */
    {AutoxRule::range, 0, INT_MAX},                       /* max_branch_switches */
    {AutoxRule::range, 0, 1},                             /* switch_one_direction */
    {AutoxRule::range, 0, autox_output_detail_levels - 1},      /* output_detail */
    {AutoxRule::range, 1, INT_MAX},                       /* max_location_iterations */
    {AutoxRule::range, 1, INT_MAX},                       /* max_newton_iterations */
    {AutoxRule::range, 1, INT_MAX},                       /* full_newton_after */
    {AutoxRule::range, 0, INT_MAX},                       /* step_adapt_every */
    {AutoxRule::range, 0, 1},                             /* suppress_branch_points */
    {AutoxRule::range, 0, 1},                             /* follow_all_branches */
}};

/* what a field holds: a whole number, a real, a boolean, or one of named choices
   (stored in the options' numbers as the index, 0 or 1 for a boolean) */
enum class AutoxFieldShape { whole, real, boolean, choice };

/* Numerics field i as the option table and the rules give it: its key (the row's), its shape, what autoX accepts */
struct AutoxNumField {
    int index; /* AUTOX_NUM_* */
    const char *key;
    AutoxFieldShape shape;
    AutoxRule rule;
    double lo, hi; /* AutoxRule::range, inclusive; a boolean's and a choice's index range too */
};
AutoxNumField autox_num_field(int i);
inline bool autox_num_is_whole(const AutoxNumField &f) { return f.shape != AutoxFieldShape::real; }
/* the names a choice field takes (none for the others) */
std::span<const char *const> autox_num_choices(int i);
/* what a field takes, as the refusal of a value it does not take says it (the event sends it, so the page says the same before it sends) */
std::string autox_num_rule_message(const AutoxNumField &f);
/* whether a value of field i is autoX's (an integer where the field is one, in the field's range); why names the field and its
   range otherwise. Pure over the rules and the option table's keys. Nothing is thrown. */
bool autox_num_ok(int i, double v, std::string &why);
/* the values that must be in order: lo below hi (strict) or at most hi */
struct NumPair {
    int lo, hi;
    bool strict;
};
constexpr NumPair num_pairs[] = {
    {AUTOX_NUM_MIN_STEP_SIZE, AUTOX_NUM_MAX_STEP_SIZE, false}, {AUTOX_NUM_PARAMETER_MIN, AUTOX_NUM_PARAMETER_MAX, true},
    {AUTOX_NUM_NORM_MIN, AUTOX_NUM_NORM_MAX, true}};
/* what the ordering of a pair, and the first step's size within the step sizes (whichever its sign), say when they fail */
std::string step_rule();
std::string pair_message(const NumPair &p);
/* the key of Numerics field i in the event, the command and the files ("mesh_intervals", ...); NULL out of range */
const char *autox_settings_num_key(int i);
/* a value of an autoX options set that is wrong: what, and its key ("ntst", "var", "mark2", ...; "" for the whole set) as the
   Error's field; at the command's place (a file of options gives its own line instead) */
Error autox_refused(std::string what, std::string key = {});
/* Check supplied numbers together, including ordering and signed step magnitude. */
Result<> autox_settings_numbers_check(std::span<const double, AUTOX_NUM_N> num,
                                    std::span<const int, AUTOX_NUM_N> supplied);

} // namespace xpp
