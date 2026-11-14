#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <cstddef>

namespace xpp {

/* the color scales (what colors a variable's range, the array plot and the
   color-coded orbit), by the words @ colormap, the files and the protocol use:
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

/* the colors a curve, a mark or an @ option's color takes: 0 (black) to
   10, the named ones (graf_par.h's color_names) */
constexpr int LAST_PLOT_COLOR = 10;

/* A color: red, green, blue and alpha (255 opaque), or the foreground, the
   default curve color, which the page draws dark on a light theme and light on
   a dark one (its r, g, b are 0). It is written `#rrggbb`, `#rrggbbaa` (alpha
   only when not opaque) or `foreground`; read also by a CSS color name. */
struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
    bool foreground = false;
    friend bool operator==(const Color &, const Color &) = default;
};
inline constexpr Color foreground_color{0, 0, 0, 255, true};

/* the color text names: `#rrggbb`, `#rrggbbaa`, a CSS color name (any case) or
   `foreground`; nothing for any other text */
std::optional<Color> parse_color(std::string_view text);
/* the color as a file writes it: `foreground`, `#rrggbb`, or `#rrggbbaa` when not opaque */
std::string print_color(const Color &c);
/* the same as a JSON string (what the protocol's events carry) */
std::string quoted_color(const Color &c);
/* XPPAUT's ten named colors, red to purple (its colors 1 to 10), 8 bit each */
inline constexpr std::array<std::array<unsigned char, 3>, 10> named_colors{{
    {255, 0, 0}, {240, 100, 0}, {255, 165, 0}, {255, 205, 0}, {200, 200, 0},
    {200, 235, 75}, {0, 225, 0}, {0, 200, 200}, {0, 0, 255}, {160, 32, 240}}};
/* XPPAUT's color number 0..LAST_PLOT_COLOR (a .ode's, read on import only) as
   the color it is in XPPAUT's own table: 0 is the foreground, 1..10 their RGB */
constexpr Color xppaut_color(int index)
{
    if (index == 0) return foreground_color;
    const auto &rgb = named_colors[static_cast<std::size_t>(index - 1)];
    return Color{rgb[0], rgb[1], rgb[2], 255, false};
}

/* A color scale as 256 colors, from its low end to its high one: what a Session holds for its
   colormap (Session::ramp) and what the array plot, the color-coded orbit and the animation
   color by, in the page and in the pictures */
inline constexpr int ramp_size = 256;
using Ramp = std::array<Color, ramp_size>;
Ramp make_ramp(Colormap type);
/* the color at a fraction 0 to 1 of the scale (clamped; NaN is the low end) */
Color ramp_color(const Ramp &ramp, double fraction);

/* whether the front end shows color at all (0 headless: color-by-variable is refused) */
struct XppColorTable {
    int enabled;
};
extern XppColorTable color_table;

/* the colors the program draws with (W655): the user's (kept in the preferences, xpp_prefs.h), not a model's, one list for every model and every view. Each is
   read when something is drawn with it: a diagram point's by its kind when it is stored, a nullcline's and a manifold's
   when it is computed. The defaults are XPPAUT's. The names are the keys of the file and the arguments of
   preferences.configure; their order is the option table's order of the settings they were (the diagram's, the nullclines',
   the manifolds'). */
struct Colors {
    Color stable_equilibrium = xppaut_color(1), unstable_equilibrium = xppaut_color(0);
    Color stable_periodic = xppaut_color(7), unstable_periodic = xppaut_color(9);
    Color x_nullcline = xppaut_color(2), y_nullcline = xppaut_color(7);
    Color stable_manifold = xppaut_color(8), unstable_manifold = xppaut_color(5);
};

/* one color's key (the file's, preferences.configure's argument and result field) and what it is: the one list the
   file, the command and the dialog read */
struct ColorKey {
    std::string_view key;
    Color Colors::*member;
    std::string_view description;
};
inline constexpr std::array COLOR_KEYS = {
    ColorKey{"stable_equilibrium_color", &Colors::stable_equilibrium, "The color of the stable equilibria in the continuation diagram."},
    ColorKey{"unstable_equilibrium_color", &Colors::unstable_equilibrium, "The color of the unstable equilibria in the continuation diagram."},
    ColorKey{"stable_periodic_color", &Colors::stable_periodic, "The color of the stable periodic orbits in the continuation diagram."},
    ColorKey{"unstable_periodic_color", &Colors::unstable_periodic, "The color of the unstable periodic orbits in the continuation diagram."},
    ColorKey{"x_nullcline_color", &Colors::x_nullcline, "The color of the x nullcline."},
    ColorKey{"y_nullcline_color", &Colors::y_nullcline, "The color of the y nullcline."},
    ColorKey{"stable_manifold_color", &Colors::stable_manifold, "The color of the stable manifold of a saddle."},
    ColorKey{"unstable_manifold_color", &Colors::unstable_manifold, "The color of the unstable manifold of a saddle."}};

} // namespace xpp
