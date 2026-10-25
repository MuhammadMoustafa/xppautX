#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace xpp {

/* the colour scales (what colours a variable's range, the array plot and the
   colour-coded orbit), by the words @ colormap, the files and the protocol use;
   XPPAUT's seven, in the order of its numbers 0 to 6, which a .ode reads on import */
enum class Colormap { jet, periodic, hot, cool, blue_red, gray, cubehelix };
inline constexpr std::array<std::string_view, 7> colormap_names{"jet", "periodic", "hot", "cool", "blue_red", "gray", "cubehelix"};

#define XPP_MAX_COLORS 256

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
/* the number of XPPAUT's colour c is, when it is one of its eleven */
std::optional<int> xppaut_color_index(const Color &c);
/* the number the drawing code, which still takes XPPAUT's eleven colours, draws c by: c must be
   one of them (an option refuses any other until the drawing takes any colour, W478, #541) */
int plot_color(const Color &c);

extern unsigned short cmap_rgb[XPP_MAX_COLORS][3];

/* The colour scale's slots in cmap_rgb (xpp_build_colormap sets them)
   and whether the front end shows colour at all. */
typedef struct {
    int enabled; /* 0 headless: colour-by-variable is refused */
    int first;   /* the scale's first slot */
    int last;    /* its last slot */
    int count;   /* its number of colours */
} XppColorTable;
extern XppColorTable color_table;

int rfun(double y, int per);
int gfun(double y, int per);
int bfun(double y, int per);
void make_cmaps(int *r, int *g, int *b, int n, Colormap type);
/* cmap_rgb and color_table for the colour scale of make_cmaps's type
   (a Session's colormap), at each model's start and on a change. (Its
   xpp_ prefix goes, as the others' did at W109e, once W119's
   xpp_batch.cpp, which calls it, has merged.) */
void xpp_build_colormap(Colormap type);
void get_ps_color(int i, float *r, float *g, float *b);
void get_svg_color(int i, int *r, int *g, int *b);

} // namespace xpp
