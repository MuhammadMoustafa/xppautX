/* Colour table computation: PostScript and SVG export need the RGB
   values with no display. */
#include "colormap.h"
#include "xpp_math.h"
#include <algorithm>
#include <cctype>
#include <math.h>
#include <stdio.h>

namespace xpp {

/* the first of the ten named colours' slots in cmap_rgb */
#define RED 20

#define COL_TOTAL 150

/* the CSS colour names (CSS Color Module Level 4), with their RGB: sorted, so a name is found by search */
struct CssColor {
    std::string_view name;
    std::uint32_t rgb;
};
constexpr std::array<CssColor, 148> css_colors{{
    {"aliceblue", 0xf0f8ff}, {"antiquewhite", 0xfaebd7}, {"aqua", 0x00ffff}, {"aquamarine", 0x7fffd4}, {"azure", 0xf0ffff},
    {"beige", 0xf5f5dc}, {"bisque", 0xffe4c4}, {"black", 0x000000}, {"blanchedalmond", 0xffebcd}, {"blue", 0x0000ff},
    {"blueviolet", 0x8a2be2}, {"brown", 0xa52a2a}, {"burlywood", 0xdeb887}, {"cadetblue", 0x5f9ea0}, {"chartreuse", 0x7fff00},
    {"chocolate", 0xd2691e}, {"coral", 0xff7f50}, {"cornflowerblue", 0x6495ed}, {"cornsilk", 0xfff8dc}, {"crimson", 0xdc143c},
    {"cyan", 0x00ffff}, {"darkblue", 0x00008b}, {"darkcyan", 0x008b8b}, {"darkgoldenrod", 0xb8860b}, {"darkgray", 0xa9a9a9},
    {"darkgreen", 0x006400}, {"darkgrey", 0xa9a9a9}, {"darkkhaki", 0xbdb76b}, {"darkmagenta", 0x8b008b}, {"darkolivegreen", 0x556b2f},
    {"darkorange", 0xff8c00}, {"darkorchid", 0x9932cc}, {"darkred", 0x8b0000}, {"darksalmon", 0xe9967a}, {"darkseagreen", 0x8fbc8f},
    {"darkslateblue", 0x483d8b}, {"darkslategray", 0x2f4f4f}, {"darkslategrey", 0x2f4f4f}, {"darkturquoise", 0x00ced1},
    {"darkviolet", 0x9400d3}, {"deeppink", 0xff1493}, {"deepskyblue", 0x00bfff}, {"dimgray", 0x696969}, {"dimgrey", 0x696969},
    {"dodgerblue", 0x1e90ff}, {"firebrick", 0xb22222}, {"floralwhite", 0xfffaf0}, {"forestgreen", 0x228b22}, {"fuchsia", 0xff00ff},
    {"gainsboro", 0xdcdcdc}, {"ghostwhite", 0xf8f8ff}, {"gold", 0xffd700}, {"goldenrod", 0xdaa520}, {"gray", 0x808080},
    {"green", 0x008000}, {"greenyellow", 0xadff2f}, {"grey", 0x808080}, {"honeydew", 0xf0fff0}, {"hotpink", 0xff69b4},
    {"indianred", 0xcd5c5c}, {"indigo", 0x4b0082}, {"ivory", 0xfffff0}, {"khaki", 0xf0e68c}, {"lavender", 0xe6e6fa},
    {"lavenderblush", 0xfff0f5}, {"lawngreen", 0x7cfc00}, {"lemonchiffon", 0xfffacd}, {"lightblue", 0xadd8e6},
    {"lightcoral", 0xf08080}, {"lightcyan", 0xe0ffff}, {"lightgoldenrodyellow", 0xfafad2}, {"lightgray", 0xd3d3d3},
    {"lightgreen", 0x90ee90}, {"lightgrey", 0xd3d3d3}, {"lightpink", 0xffb6c1}, {"lightsalmon", 0xffa07a},
    {"lightseagreen", 0x20b2aa}, {"lightskyblue", 0x87cefa}, {"lightslategray", 0x778899}, {"lightslategrey", 0x778899},
    {"lightsteelblue", 0xb0c4de}, {"lightyellow", 0xffffe0}, {"lime", 0x00ff00}, {"limegreen", 0x32cd32}, {"linen", 0xfaf0e6},
    {"magenta", 0xff00ff}, {"maroon", 0x800000}, {"mediumaquamarine", 0x66cdaa}, {"mediumblue", 0x0000cd},
    {"mediumorchid", 0xba55d3}, {"mediumpurple", 0x9370db}, {"mediumseagreen", 0x3cb371}, {"mediumslateblue", 0x7b68ee},
    {"mediumspringgreen", 0x00fa9a}, {"mediumturquoise", 0x48d1cc}, {"mediumvioletred", 0xc71585}, {"midnightblue", 0x191970},
    {"mintcream", 0xf5fffa}, {"mistyrose", 0xffe4e1}, {"moccasin", 0xffe4b5}, {"navajowhite", 0xffdead}, {"navy", 0x000080},
    {"oldlace", 0xfdf5e6}, {"olive", 0x808000}, {"olivedrab", 0x6b8e23}, {"orange", 0xffa500}, {"orangered", 0xff4500},
    {"orchid", 0xda70d6}, {"palegoldenrod", 0xeee8aa}, {"palegreen", 0x98fb98}, {"paleturquoise", 0xafeeee},
    {"palevioletred", 0xdb7093}, {"papayawhip", 0xffefd5}, {"peachpuff", 0xffdab9}, {"peru", 0xcd853f}, {"pink", 0xffc0cb},
    {"plum", 0xdda0dd}, {"powderblue", 0xb0e0e6}, {"purple", 0x800080}, {"rebeccapurple", 0x663399}, {"red", 0xff0000},
    {"rosybrown", 0xbc8f8f}, {"royalblue", 0x4169e1}, {"saddlebrown", 0x8b4513}, {"salmon", 0xfa8072}, {"sandybrown", 0xf4a460},
    {"seagreen", 0x2e8b57}, {"seashell", 0xfff5ee}, {"sienna", 0xa0522d}, {"silver", 0xc0c0c0}, {"skyblue", 0x87ceeb},
    {"slateblue", 0x6a5acd}, {"slategray", 0x708090}, {"slategrey", 0x708090}, {"snow", 0xfffafa}, {"springgreen", 0x00ff7f},
    {"steelblue", 0x4682b4}, {"tan", 0xd2b48c}, {"teal", 0x008080}, {"thistle", 0xd8bfd8}, {"tomato", 0xff6347},
    {"turquoise", 0x40e0d0}, {"violet", 0xee82ee}, {"wheat", 0xf5deb3}, {"white", 0xffffff}, {"whitesmoke", 0xf5f5f5},
    {"yellow", 0xffff00}, {"yellowgreen", 0x9acd32}}};

static_assert(std::ranges::is_sorted(css_colors, {}, &CssColor::name) && css_colors.back().name == "yellowgreen",
              "the CSS colours are sorted and all 148 are there");

/* 16-bit RGB per colour index, same scale X11 uses */
unsigned short cmap_rgb[XPP_MAX_COLORS][3];
XppColorTable color_table;

int rfun(double y, int per)
{
    double x;
    x = y;
    if ((y > .666666) && (per == 1)) x = 1. - y;
    if (x > .33333333333) return 0;
    return static_cast<int>(3. * 255 * sqrt((.333334 - x) * (x + .33334)));
}

int gfun(double y, int)
{
    if (y > .666666) return 0;
    return static_cast<int>(3. * 255 * sqrt((.6666667 - y) * (y)));
}

int bfun(double y, int)
{
    if (y < .333334) return 0;
    return static_cast<int>(2.79 * 255 * sqrt((1.05 - y) * (y - .333333333)));
}

void make_cmaps(int *r, int *g, int *b, int n, Colormap type)
{
    double x;
    int i, i1, i2, i3;
    double pii = 3.1415926;
    double start = .5, rots = -1.5, hue = 1.2, gamma = 1.;
    double angle, amp;
    double rr, gg, bb;
    switch (type) {
    case Colormap::jet:
        for (i = 0; i < n; i++) {
            x = static_cast<double>(i) / static_cast<double>(n);
            r[i] = rfun(1 - x, 0) << 8;
            g[i] = gfun(1 - x, 0) << 8;
            b[i] = bfun(1 - x, 0) << 8;
        }
        break;
    case Colormap::periodic:
        for (i = 0; i < n; i++) {
            x = static_cast<double>(i) / static_cast<double>(n);
            r[i] = rfun(x, 1) << 8;
            g[i] = gfun(x, 1) << 8;
            b[i] = bfun(x, 1) << 8;
        }
        break;
    case Colormap::hot:
        i1 = .375 * n;
        i2 = 2 * i1;
        i3 = n - i2;
        for (i = 0; i < i1; i++) {
            x = 256 * 255 * static_cast<double>(i) / static_cast<double>(i1);
            r[i] = static_cast<int>(x);
            g[i] = 0;
            b[i] = 0;
            g[i + i1] = static_cast<int>(x);
            b[i + i1] = 0;
        }
        for (i = i1; i < n; i++) r[i] = 256 * 255;
        for (i = i2; i < n; i++) {
            x = 256 * 255 * static_cast<double>(i - i2) / static_cast<double>(i3);
            g[i] = 256 * 255;
            b[i] = static_cast<int>(x);
        }
        break;
    case Colormap::cool:
        for (i = 0; i < n; i++) {
            x = static_cast<double>(i) / static_cast<double>(n);
            r[i] = static_cast<int>(256 * 255 * x);
            b[i] = static_cast<int>(256 * 255 * (1 - x));
            g[i] = 256 * 255;
        }
        break;
    case Colormap::blue_red:
        for (i = 0; i < n; i++) {
            x = static_cast<double>(i) / static_cast<double>(n);
            r[i] = static_cast<int>(256 * 255 * x);
            b[i] = static_cast<int>(256 * 255 * (1 - x));
            g[i] = 0;
        }
        break;
    case Colormap::gray:
        for (i = 0; i < n; i++) {
            r[i] = i * 256 * 255 / n;
            b[i] = i * 256 * 255 / n;
            g[i] = i * 256 * 255 / n;
        }
        break;
    case Colormap::cubehelix:
        for (i = 0; i < n; i++) {
            x = static_cast<double>(i) / static_cast<double>(n);
            angle = 2 * pii * (start / 3.0 + 1 + rots * x);
            x = xpp::math::pow(x, gamma);
            amp = hue * x * (1 - x) / 2.0;
            rr = x + amp * (-.14861 * xpp::math::cos(angle) + 1.78277 * xpp::math::sin(angle));
            gg = x + amp * (-.29227 * xpp::math::cos(angle) - .90649 * xpp::math::sin(angle));
            bb = x + amp * (1.97294 * xpp::math::cos(angle));
            if (rr < 0.0) rr = 0.0;
            if (rr > 1.0) rr = 1.0;
            if (gg < 0.0) gg = 0.0;
            if (gg > 1.0) gg = 1.0;
            if (bb < 0.0) bb = 0.0;
            if (bb > 1.0) bb = 1.0;
            r[i] = 256 * 255 * rr;
            b[i] = 256 * 255 * bb;
            g[i] = 256 * 255 * gg;
        }
        break;
    }
}

/* Fill cmap_rgb and color_table's first/last/count. This is the
   device-independent half of what MakeColormap() in color.c used to do. */
void xpp_build_colormap(Colormap type)
{
    int i;
    int r[256], g[256], b[256];

    color_table.first = 30;
    color_table.last = XPP_MAX_COLORS - 1;
    color_table.count = color_table.last - color_table.first + 1;
    if (color_table.count > COL_TOTAL) color_table.count = COL_TOTAL;
    color_table.last = color_table.first + color_table.count;

    for (i = 0; i < XPP_MAX_COLORS; i++)
        cmap_rgb[i][0] = cmap_rgb[i][1] = cmap_rgb[i][2] = 0;

    /* the ten named colours, 8-bit values shifted up to 16 bit */
    for (std::size_t k = 0; k < named_colors.size(); k++) {
        cmap_rgb[RED + k][0] = static_cast<unsigned short>(named_colors[k][0] << 8);
        cmap_rgb[RED + k][1] = static_cast<unsigned short>(named_colors[k][1] << 8);
        cmap_rgb[RED + k][2] = static_cast<unsigned short>(named_colors[k][2] << 8);
    }

    make_cmaps(r, g, b, color_table.count + 1, type);
    for (i = color_table.first; i <= color_table.last; i++) {
        cmap_rgb[i][0] = r[i - color_table.first];
        cmap_rgb[i][1] = g[i - color_table.first];
        cmap_rgb[i][2] = b[i - color_table.first];
    }
}

void get_ps_color(int i, float *r, float *g, float *b)
{
    float z = 1. / (65535);
    *r = z * static_cast<float>(cmap_rgb[i][0]);
    *g = z * static_cast<float>(cmap_rgb[i][1]);
    *b = z * static_cast<float>(cmap_rgb[i][2]);
}

void get_svg_color(int i, int *r, int *g, int *b)
{
    *r = cmap_rgb[i][0] / 255;
    *g = cmap_rgb[i][1] / 255;
    *b = cmap_rgb[i][2] / 255;
}

std::optional<Color> parse_color(std::string_view text)
{
    if (text == "foreground") return foreground_color;
    if (text.starts_with('#')) {
        if (text.size() != 7 && text.size() != 9) return std::nullopt;
        std::uint32_t value = 0;
        for (const char c : text.substr(1)) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return std::nullopt;
            value = value << 4 | static_cast<std::uint32_t>(c <= '9' ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10);
        }
        if (text.size() == 7) value = value << 8 | 0xff;
        return Color{static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
                     static_cast<std::uint8_t>(value), false};
    }
    std::string lower(text);
    std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto named = std::ranges::lower_bound(css_colors, lower, {}, &CssColor::name);
    if (named == css_colors.end() || named->name != lower) return std::nullopt;
    return Color{static_cast<std::uint8_t>(named->rgb >> 16), static_cast<std::uint8_t>(named->rgb >> 8), static_cast<std::uint8_t>(named->rgb), 255, false};
}

std::string print_color(const Color &c)
{
    if (c.foreground) return "foreground";
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text = "#";
    for (const std::uint8_t part : {c.r, c.g, c.b}) {
        text += digits[part >> 4];
        text += digits[part & 15];
    }
    if (c.a != 255) {
        text += digits[c.a >> 4];
        text += digits[c.a & 15];
    }
    return text;
}

std::optional<int> xppaut_color_index(const Color &c)
{
    for (int i = 0; i <= LAST_PLOT_COLOR; i++)
        if (xppaut_color(i) == c) return i;
    return std::nullopt;
}

int plot_color(const Color &c) { return xppaut_color_index(c).value(); }

} // namespace xpp
