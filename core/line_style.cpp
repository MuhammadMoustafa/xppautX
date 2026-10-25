#include "line_style.h"

#include <cmath>

#include "model_options.h"
#include "xpp_util.h"

namespace xpp {

namespace {

/* the enum value of a word of names, nothing for any other text */
template <class E, std::size_t N> std::optional<E> parse_name(const std::array<std::string_view, N> &names, std::string_view word)
{
    const int i = choice_index(names, word);
    return i < 0 ? std::nullopt : std::optional<E>(static_cast<E>(i));
}

/* XPPAUT's own range of line types */
constexpr int first_line_type = -5, solid_line_type = 1;

} // namespace

std::optional<LineDraw> parse_line_draw(std::string_view word) { return parse_name<LineDraw>(line_draw_names, word); }
std::optional<LineDash> parse_line_dash(std::string_view word) { return parse_name<LineDash>(line_dash_names, word); }
std::optional<LineMarker> parse_line_marker(std::string_view word) { return parse_name<LineMarker>(line_marker_names, word); }

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

std::optional<int> xppaut_line_type(const LineStyle &style)
{
    const LineStyle line_defaults;
    if (style.dash != LineDash::solid || style.width != line_defaults.width) return std::nullopt;
    if (style.draw == LineDraw::line) return solid_line_type;
    if (style.draw == LineDraw::both || style.marker != LineMarker::circle) return std::nullopt;
    const double radius = style.marker_size;
    if (radius != std::floor(radius) || radius < 0 || radius > -first_line_type) return std::nullopt;
    return -static_cast<int>(radius);
}

} // namespace xpp
