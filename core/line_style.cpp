#include "line_style.h"

#include <cmath>
#include <cstddef>

#include "xpp_text.h"

namespace xpp {

namespace {

/* XPPAUT's own range of line types */
constexpr int first_line_type = -5, solid_line_type = 1;

} // namespace

std::optional<LineDraw> parse_line_draw(std::string_view word) { return enum_from_name<LineDraw>(line_draw_names, word); }
std::optional<LineDash> parse_line_dash(std::string_view word) { return enum_from_name<LineDash>(line_dash_names, word); }
std::optional<LineMarker> parse_line_marker(std::string_view word) { return enum_from_name<LineMarker>(line_marker_names, word); }

std::optional<double> parse_line_size(std::string_view text, bool allow_zero)
{
    double x = 0;
    if (!parse_number(text, x) || !std::isfinite(x) || x < 0 || (x == 0 && !allow_zero)) return std::nullopt;
    return x;
}

std::optional<LineStyle> line_style_of_xppaut(int lt)
{
    if (lt < first_line_type || lt > solid_line_type) return std::nullopt;
    LineStyle style;
    if (lt == solid_line_type) return style;
    style.draw = LineDraw::points;
    style.marker_size = -lt;
    return style;
}

} // namespace xpp
