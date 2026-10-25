#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

/* the colour scales (what colours a variable's range, the array plot and the
   colour-coded orbit), by the words @ colormap, the files and the protocol use:
   XPPAUT's seven first, in the order of its numbers 0 to 6, which a .ode reads on
   import, then the ones xppautX adds (W381 decision 11, W492) */
enum class Colormap {
    jet, periodic, hot, cool, blue_red, gray, cubehelix,
    viridis, plasma, inferno, magma, cividis, coolwarm, rdbu, seismic, twilight, hsv, turbo,
    greys, blues, reds, greens, oranges, purples
};
inline constexpr std::array<std::string_view, 24> colormap_names{
    "jet", "periodic", "hot", "cool", "blue_red", "gray", "cubehelix",
    "viridis", "plasma", "inferno", "magma", "cividis", "coolwarm", "rdbu", "seismic", "twilight", "hsv", "turbo",
    "greys", "blues", "reds", "greens", "oranges", "purples"};
/* how many of them are XPPAUT's */
inline constexpr std::size_t xppaut_colormap_count = 7;

/* the colours a curve, a mark or an @ option's colour takes: 0 (black) to
   10, the named ones (graf_par.h's color_names) */
constexpr int LAST_PLOT_COLOR = 10;

/* A colour: red, green, blue and alpha (255 opaque), or the foreground, the
   default curve colour, which the page draws dark on a light theme and light on
   a dark one (its r, g, b are 0). It is written `#rrggbb`, `#rrggbbaa` (alpha
   only when not opaque) or `foreground`; read also by a CSS colour name. */
struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
    bool foreground = false;
    friend bool operator==(const Color &, const Color &) = default;
};
inline constexpr Color foreground_color{0, 0, 0, 255, true};

/* the colour text names: `#rrggbb`, `#rrggbbaa`, a CSS colour name (any case) or
   `foreground`; nothing for any other text */
std::optional<Color> parse_color(std::string_view text);
/* the colour as a file writes it: `foreground`, `#rrggbb`, or `#rrggbbaa` when not opaque */
std::string print_color(const Color &c);
/* the same as a JSON string (what the protocol's events carry) */
std::string quoted_color(const Color &c);
/* XPPAUT's ten named colours, red to purple (its colours 1 to 10), 8 bit each */
inline constexpr std::array<std::array<unsigned char, 3>, 10> named_colors{{
    {255, 0, 0}, {240, 100, 0}, {255, 165, 0}, {255, 205, 0}, {200, 200, 0},
    {200, 235, 75}, {0, 225, 0}, {0, 200, 200}, {0, 0, 255}, {160, 32, 240}}};
/* XPPAUT's colour number 0..LAST_PLOT_COLOR (a .ode's, read on import only) as
   the colour it is in XPPAUT's own table: 0 is the foreground, 1..10 their RGB */
constexpr Color xppaut_color(int index)
{
    if (index == 0) return foreground_color;
    const auto &rgb = named_colors[static_cast<std::size_t>(index - 1)];
    return Color{rgb[0], rgb[1], rgb[2], 255, false};
}

/* A colour scale as 256 colours, from its low end to its high one: what a Session holds for its
   colormap (Session::ramp) and what the array plot, the colour-coded orbit and the animation
   colour by, in the page and in the pictures */
inline constexpr int ramp_size = 256;
using Ramp = std::array<Color, ramp_size>;
Ramp make_ramp(Colormap type);
/* the colour at a fraction 0 to 1 of the scale (clamped; NaN is the low end) */
Color ramp_color(const Ramp &ramp, double fraction);

/* whether the front end shows colour at all (0 headless: colour-by-variable is refused) */
struct XppColorTable {
    int enabled;
};
extern XppColorTable color_table;

} // namespace xpp
