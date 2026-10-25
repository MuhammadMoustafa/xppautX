/* The value types of settings (W484, #548): the colour (core/colormap.h), the line style
   (core/line_style.h) and the colour scales' names. make test runs this from the top of the tree. */
#include "xpptest.h"
#include "colormap.h"
#include "line_style.h"

#include <cmath>
#include <string>

namespace {
/* a colour of its parts (not the foreground) */
xpp::Color rgba(int r, int g, int b, int a = 255)
{
    return xpp::Color{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a), false};
}
} // namespace

int main(void)
{
    /* a colour is written #rrggbb (#rrggbbaa when not opaque) or foreground, read also by a CSS name in any case */
    CHECK(xpp::parse_color("#d62839") == rgba(0xd6, 0x28, 0x39));
    CHECK(xpp::parse_color("#D62839AA") == rgba(0xd6, 0x28, 0x39, 0xaa));
    CHECK(xpp::parse_color("foreground") == xpp::foreground_color);
    CHECK(xpp::parse_color("Red") == rgba(255, 0, 0) && xpp::parse_color("rebeccapurple") == rgba(0x66, 0x33, 0x99));
    CHECK(xpp::parse_color("yellowgreen") == rgba(0x9a, 0xcd, 0x32) && xpp::parse_color("aliceblue") == rgba(0xf0, 0xf8, 0xff));
    for (const char *bad : {"", "#", "#fff", "#12345", "#1234567", "#12345g", "ff0000", "redd", "Foreground", "rgb(1,2,3)", "0"})
        CHECK(!xpp::parse_color(bad).has_value());
    CHECK(xpp::print_color(rgba(0xd6, 0x28, 0x39)) == "#d62839");
    CHECK(xpp::print_color(rgba(0xd6, 0x28, 0x39, 0xaa)) == "#d62839aa");
    CHECK(xpp::print_color(xpp::foreground_color) == "foreground");
    CHECK(xpp::print_color(*xpp::parse_color("tomato")) == "#ff6347");
    for (const std::string text : {"#000000", "#ffffff80", "#0a0b0c", "foreground"}) CHECK(xpp::print_color(*xpp::parse_color(text)) == text);

    /* XPPAUT's 0..10: the foreground and the RGB of its own table */
    CHECK(xpp::xppaut_color(0) == xpp::foreground_color);
    CHECK(xpp::print_color(xpp::xppaut_color(1)) == "#ff0000" && xpp::print_color(xpp::xppaut_color(2)) == "#f06400"
          && xpp::print_color(xpp::xppaut_color(6)) == "#c8eb4b" && xpp::print_color(xpp::xppaut_color(9)) == "#0000ff"
          && xpp::print_color(xpp::xppaut_color(10)) == "#a020f0");
    for (int i = 0; i <= xpp::LAST_PLOT_COLOR; i++) CHECK(xpp::parse_color(xpp::print_color(xpp::xppaut_color(i))) == xpp::xppaut_color(i));
    CHECK(xpp::quoted_color(rgba(1, 2, 3, 4)) == "\"#01020304\"" && xpp::quoted_color(xpp::foreground_color) == "\"foreground\"");

    /* the colour scales: XPPAUT's seven first, by number 0..6 in the order of their words, then the 17 more */
    CHECK(xpp::colormap_names.size() == 24 && xpp::xppaut_colormap_count == 7);
    CHECK(xpp::colormap_names[static_cast<std::size_t>(xpp::Colormap::jet)] == "jet"
          && xpp::colormap_names[static_cast<std::size_t>(xpp::Colormap::cubehelix)] == "cubehelix"
          && xpp::colormap_names[static_cast<std::size_t>(xpp::Colormap::blue_red)] == "blue_red"
          && xpp::colormap_names[static_cast<std::size_t>(xpp::Colormap::viridis)] == "viridis"
          && xpp::colormap_names[static_cast<std::size_t>(xpp::Colormap::purples)] == "purples");
    /* every scale is 256 opaque colours, and no two scales are alike */
    for (std::size_t m = 0; m < xpp::colormap_names.size(); m++) {
        const xpp::Ramp ramp = xpp::make_ramp(static_cast<xpp::Colormap>(m));
        CHECK(ramp.size() == 256);
        for (const xpp::Color &c : ramp) CHECK(c.a == 255 && !c.foreground);
        for (std::size_t n = 0; n < m; n++) CHECK(ramp != xpp::make_ramp(static_cast<xpp::Colormap>(n)));
    }
    const xpp::Ramp viridis = xpp::make_ramp(xpp::Colormap::viridis), gray = xpp::make_ramp(xpp::Colormap::gray);
    CHECK(viridis.front() == rgba(68, 1, 84) && viridis.back() == rgba(253, 231, 37)); /* matplotlib's table's ends */
    CHECK(gray.front() == rgba(0, 0, 0) && gray.back() == rgba(254, 254, 254));
    /* a fraction picks a colour of the ramp: 0 the first, 1 the last, outside 0..1 the nearest end, NaN the first */
    CHECK(xpp::ramp_color(viridis, 0) == viridis.front() && xpp::ramp_color(viridis, 1) == viridis.back() && xpp::ramp_color(viridis, 2) == viridis.back()
          && xpp::ramp_color(viridis, -1) == viridis.front() && xpp::ramp_color(viridis, std::nan("")) == viridis.front()
          && xpp::ramp_color(viridis, .5) == viridis[128]);

    /* the line style: words, sizes and XPPAUT's line types */
    CHECK(xpp::parse_line_draw("both") == xpp::LineDraw::both && !xpp::parse_line_draw("Line").has_value());
    CHECK(xpp::parse_line_dash("dash_dot") == xpp::LineDash::dash_dot && !xpp::parse_line_dash("dashdot").has_value());
    CHECK(xpp::parse_line_marker("star") == xpp::LineMarker::star && xpp::parse_line_marker("circle") == xpp::LineMarker::circle
          && !xpp::parse_line_marker("5").has_value());
    CHECK(xpp::parse_line_size("2.5", false) == 2.5 && !xpp::parse_line_size("0", false).has_value() && xpp::parse_line_size("0", true) == 0.0
          && !xpp::parse_line_size("-1", true).has_value() && !xpp::parse_line_size("nan", true).has_value() && !xpp::parse_line_size("x", true).has_value());
    CHECK(xpp::line_style_of_xppaut(1) == xpp::LineStyle{});
    CHECK(xpp::line_style_of_xppaut(0) && xpp::line_style_of_xppaut(0)->draw == xpp::LineDraw::points && xpp::line_style_of_xppaut(0)->marker_size == 0);
    CHECK(xpp::line_style_of_xppaut(-3) && xpp::line_style_of_xppaut(-3)->draw == xpp::LineDraw::points && xpp::line_style_of_xppaut(-3)->marker_size == 3
          && xpp::line_style_of_xppaut(-3)->marker == xpp::LineMarker::circle);
    CHECK(!xpp::line_style_of_xppaut(2).has_value() && !xpp::line_style_of_xppaut(-6).has_value());
    for (int lt = -5; lt <= 1; lt++) CHECK(xpp::line_style_of_xppaut(lt).has_value());
    CHECK(xpp::line_style_json(xpp::LineStyle{}) == "\"draw\":\"line\",\"dash\":\"solid\",\"width\":1,\"marker\":\"circle\",\"marker_size\":3");

    TEST_REPORT("colours and line styles");
}
