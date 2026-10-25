#include "line_style.h"

#include <cmath>

#include "json_number.h"
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

std::string line_style_json(const LineStyle &style)
{
    std::string o = "\"draw\":\"";
    o += line_draw_names[static_cast<std::size_t>(style.draw)];
    o += "\",\"dash\":\"";
    o += line_dash_names[static_cast<std::size_t>(style.dash)];
    o += "\",\"width\":";
    json::json_append_number_shortest(o, style.width);
    o += ",\"marker\":\"";
    o += line_marker_names[static_cast<std::size_t>(style.marker)];
    o += "\",\"marker_size\":";
    json::json_append_number_shortest(o, style.marker_size);
    return o;
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
