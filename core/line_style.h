#pragma once
/* The style of a curve's line (W381 decision 10): whether it is drawn as a
   line, as points or both, how the line is dashed, how wide it is and what
   the points look like. The words below are the ones our files and the
   protocol use; XPPAUT's line type (-5..1 in a .ode's `lt`, a form's Line
   type) is read on import only (W484, #548: the types and their parsing;
   the page, SVG and PDF take them into use with W492, #556). */

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

enum class LineDraw { line, points, both };
enum class LineDash { solid, dashed, dotted, dash_dot };
enum class LineMarker { circle, square, triangle, diamond, cross, plus, star };

inline constexpr std::array<std::string_view, 3> line_draw_names{"line", "points", "both"};
inline constexpr std::array<std::string_view, 4> line_dash_names{"solid", "dashed", "dotted", "dash_dot"};
inline constexpr std::array<std::string_view, 7> line_marker_names{"circle", "square", "triangle", "diamond", "cross", "plus", "star"};

/* width and marker_size in pixels (XPPAUT's unit, a circle's radius: its type -n is a circle of radius n) */
struct LineStyle {
    LineDraw draw = LineDraw::line;
    LineDash dash = LineDash::solid;
    double width = 1;
    LineMarker marker = LineMarker::circle;
    double marker_size = 3;
    friend bool operator==(const LineStyle &, const LineStyle &) = default;
};

/* the value a word names, nothing for any other text */
std::optional<LineDraw> parse_line_draw(std::string_view word);
std::optional<LineDash> parse_line_dash(std::string_view word);
std::optional<LineMarker> parse_line_marker(std::string_view word);
/* a width or a marker size: a number above 0, a marker size also 0 (a dot) when allow_zero */
std::optional<double> parse_line_size(std::string_view text, bool allow_zero);

/* the style as the protocol's events carry it, the members of an object: "draw":"line","dash":"solid","width":1,
   "marker":"circle","marker_size":3 */
std::string line_style_json(const LineStyle &style);

/* the style of XPPAUT's line type lt, -5 to 1 (1 a solid line, 0 a dot at each point, -n a circle of
   radius n at each point), nothing outside them */
std::optional<LineStyle> line_style_of_xppaut(int lt);

} // namespace xpp
