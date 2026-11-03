#pragma once

/* What a stored autoX point continued in (W330, #391): a tagged value, not an
   integer that means a model parameter below the model's size and an autoX
   unknown above it. An axis is a model parameter (its Model index), or one
   of autoX's own numerical unknowns (its slot in autox_unknowns.h: the period
   or truncation interval, ...). Growing or reordering the model's parameters
   never rewrites a stored axis: a parameter's identity is its Model index
   and an unknown's is its slot.

   The integer exists only at the files' boundary (diagram.csv's icp columns,
   an imported XPPAUT diagram): a model parameter k is k, an unknown slot is
   model_parameters + slot, autox_unknown::block_size slots at most; autox_axis_encode
   and autox_axis_decode are that boundary and the only place the sum is made.
   The run's own array layout is AutoxParameterLayout (autox_state.h). */

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "autox_unknowns.h"

namespace xpp {

struct AutoxAxis {
    enum class Kind : int { ModelParameter, Unknown };
    Kind kind = Kind::ModelParameter;
    int index = 0; /* the Model index of a parameter, or the unknown's slot */

    static constexpr AutoxAxis parameter(int k) { return {Kind::ModelParameter, k}; }
    static constexpr AutoxAxis unknown(int slot) { return {Kind::Unknown, slot}; }
    static constexpr AutoxAxis period() { return unknown(autox_unknown::period); }

    constexpr bool is_parameter() const { return kind == Kind::ModelParameter; }
    constexpr bool is_period() const { return kind == Kind::Unknown && index == autox_unknown::period; }
    /* whether this is the model parameter k */
    constexpr bool is_parameter(int k) const { return is_parameter() && index == k; }
    bool operator==(const AutoxAxis &) const = default;
};

/* What a diagram view plots up its side against the first parameter
   (W381 question 12): a variable's maximum, the norm, the variable's
   maximum and minimum, the period, a second parameter (a two-parameter
   view), the frequency, the variable's average. Its name is its word in
   the protocol and the session files; its label is the name with spaces
   and a capital first letter. */
enum class AutoxProjection { maximum, norm, maximum_and_minimum, period, two_parameter, frequency, average };
inline constexpr std::array<std::string_view, 7> autox_projection_names{
    "maximum", "norm", "maximum_and_minimum", "period", "two_parameter", "frequency", "average"};

constexpr std::string_view autox_projection_name(AutoxProjection p)
{
    return autox_projection_names[static_cast<std::size_t>(p)];
}

/* the projection a name names, or none */
constexpr std::optional<AutoxProjection> autox_projection_from_name(std::string_view name)
{
    for (std::size_t i = 0; i < autox_projection_names.size(); ++i)
        if (autox_projection_names[i] == name) return static_cast<AutoxProjection>(i);
    return std::nullopt;
}

/* why a word is no projection: "\"x\" is not a projection (maximum, norm, ...)" */
inline std::string autox_projection_unknown(std::string_view word)
{
    std::string text = "\"" + std::string(word) + "\" is not a projection (";
    for (std::size_t i = 0; i < autox_projection_names.size(); ++i)
        text += (i ? ", " : "") + std::string(autox_projection_names[i]);
    return text + ")";
}

/* XPPAUT's Autox.plot number (its .auto file, the Axes menu's item): 0 Hi,
   1 Norm, 2 hI-lo, 3 Period, 4 Two par, 10 fRequency, 11 Average; read on
   import only */
constexpr std::optional<AutoxProjection> autox_projection_from_xppaut(long code)
{
    switch (code) {
    case 0: return AutoxProjection::maximum;
    case 1: return AutoxProjection::norm;
    case 2: return AutoxProjection::maximum_and_minimum;
    case 3: return AutoxProjection::period;
    case 4: return AutoxProjection::two_parameter;
    case 10: return AutoxProjection::frequency;
    case 11: return AutoxProjection::average;
    default: return std::nullopt;
    }
}

/* the file form of an axis of a model with that many parameters */
constexpr int autox_axis_encode(AutoxAxis a, int model_parameters)
{
    return a.is_parameter() ? a.index : model_parameters + a.index;
}

/* the axis of a file's integer, or none: negative, or beyond autoX's slots */
constexpr std::optional<AutoxAxis> autox_axis_decode(long code, int model_parameters)
{
    if (code < 0) return std::nullopt;
    if (code < model_parameters) return AutoxAxis::parameter(static_cast<int>(code));
    if (code - model_parameters < autox_unknown::block_size) return AutoxAxis::unknown(static_cast<int>(code - model_parameters));
    return std::nullopt;
}

} // namespace xpp

