#pragma once

/* What a stored AUTO point continued in (W330, #391): a tagged value, not an
   integer that means a model parameter below the model's size and an AUTO
   unknown above it. An axis is a model parameter (its Model index), or one
   of AUTO's own numerical unknowns (its slot in auto_unknowns.h: the period
   or truncation interval, ...). Growing or reordering the model's parameters
   never rewrites a stored axis: a parameter's identity is its Model index
   and an unknown's is its slot.

   The integer exists only at the files' boundary (diagram.csv's icp columns,
   an imported XPPAUT diagram): a model parameter k is k, an unknown slot is
   model_parameters + slot, auto_unknown::block_size slots at most; auto_axis_encode
   and auto_axis_decode are that boundary and the only place the sum is made.
   The run's own array layout is AutoParameterLayout (auto_state.h). */

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "auto_unknowns.h"

namespace xpp {

struct AutoAxis {
    enum class Kind : int { ModelParameter, Unknown };
    Kind kind = Kind::ModelParameter;
    int index = 0; /* the Model index of a parameter, or the unknown's slot */

    static constexpr AutoAxis parameter(int k) { return {Kind::ModelParameter, k}; }
    static constexpr AutoAxis unknown(int slot) { return {Kind::Unknown, slot}; }
    static constexpr AutoAxis period() { return unknown(auto_unknown::period); }

    constexpr bool is_parameter() const { return kind == Kind::ModelParameter; }
    constexpr bool is_period() const { return kind == Kind::Unknown && index == auto_unknown::period; }
    /* whether this is the model parameter k */
    constexpr bool is_parameter(int k) const { return is_parameter() && index == k; }
    bool operator==(const AutoAxis &) const = default;
};

/* What a diagram view plots up its side against the first parameter
   (W381 question 12): a variable's maximum, the norm, the variable's
   maximum and minimum, the period, a second parameter (a two-parameter
   view), the frequency, the variable's average. Its name is its word in
   the protocol and the session files; its label is the name with spaces
   and a capital first letter. */
enum class AutoProjection { maximum, norm, maximum_and_minimum, period, two_parameter, frequency, average };
inline constexpr std::array<std::string_view, 7> auto_projection_names{
    "maximum", "norm", "maximum_and_minimum", "period", "two_parameter", "frequency", "average"};

constexpr std::string_view auto_projection_name(AutoProjection p)
{
    return auto_projection_names[static_cast<std::size_t>(p)];
}

/* the projection a name names, or none */
constexpr std::optional<AutoProjection> auto_projection_from_name(std::string_view name)
{
    for (std::size_t i = 0; i < auto_projection_names.size(); ++i)
        if (auto_projection_names[i] == name) return static_cast<AutoProjection>(i);
    return std::nullopt;
}

/* why a word is no projection: "\"x\" is not a projection (maximum, norm, ...)" */
inline std::string auto_projection_unknown(std::string_view word)
{
    std::string text = "\"" + std::string(word) + "\" is not a projection (";
    for (std::size_t i = 0; i < auto_projection_names.size(); ++i)
        text += (i ? ", " : "") + std::string(auto_projection_names[i]);
    return text + ")";
}

/* XPPAUT's Auto.plot number (its .auto file, the Axes menu's item): 0 Hi,
   1 Norm, 2 hI-lo, 3 Period, 4 Two par, 10 fRequency, 11 Average; read on
   import only */
constexpr std::optional<AutoProjection> auto_projection_from_xppaut(long code)
{
    switch (code) {
    case 0: return AutoProjection::maximum;
    case 1: return AutoProjection::norm;
    case 2: return AutoProjection::maximum_and_minimum;
    case 3: return AutoProjection::period;
    case 4: return AutoProjection::two_parameter;
    case 10: return AutoProjection::frequency;
    case 11: return AutoProjection::average;
    default: return std::nullopt;
    }
}

/* the file form of an axis of a model with that many parameters */
constexpr int auto_axis_encode(AutoAxis a, int model_parameters)
{
    return a.is_parameter() ? a.index : model_parameters + a.index;
}

/* the axis of a file's integer, or none: negative, or beyond AUTO's slots */
constexpr std::optional<AutoAxis> auto_axis_decode(long code, int model_parameters)
{
    if (code < 0) return std::nullopt;
    if (code < model_parameters) return AutoAxis::parameter(static_cast<int>(code));
    if (code - model_parameters < auto_unknown::block_size) return AutoAxis::unknown(static_cast<int>(code - model_parameters));
    return std::nullopt;
}

} // namespace xpp

