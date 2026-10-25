/* The picture an export writes: the primitives that record into it and what its two writers share
   (picture.h). */
#include "picture.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "graphics.h"
#include "session.h"

namespace xpp {

namespace {

constexpr Color black{0, 0, 0, 255, false};

/* the base of the dash lengths and of a marker's strokes: at least a point wide */
double stroke_unit(double width) { return std::max(width, 1.0); }

constexpr double sub_shift = .3, sup_shift = .6, script_size = .6; /* XPPAUT's: of the base size, as XPPAUT shifts them */

} // namespace

Color ink(const Picture &p, const Color &c)
{
    if (c.foreground || !p.color) return Color{0, 0, 0, c.foreground ? std::uint8_t{255} : c.a, false};
    return c;
}

Color ink_fill(const Picture &p, const Color &c)
{
    if (c.foreground) return black;
    if (p.color) return c;
    const auto gray = static_cast<std::uint8_t>(std::lround(.299 * c.r + .587 * c.g + .114 * c.b));
    return Color{gray, gray, gray, c.a, false};
}

std::vector<double> dash_pattern(LineDash dash, double width)
{
    const double u = stroke_unit(width);
    switch (dash) {
    case LineDash::solid: return {};
    case LineDash::dashed: return {6 * u, 3 * u};
    case LineDash::dotted: return {u, 2 * u};
    case LineDash::dash_dot: return {6 * u, 3 * u, u, 3 * u};
    }
    return {};
}

MarkerGlyph marker_glyph(LineMarker marker, double size)
{
    const double r = std::max(size, 0.5); /* a size of 0 is a dot */
    MarkerGlyph g;
    switch (marker) {
    case LineMarker::circle: g.circle_radius = r; break;
    case LineMarker::square: g.polygon = {{-r, -r}, {r, -r}, {r, r}, {-r, r}}; break;
    case LineMarker::triangle: g.polygon = {{0, -r * 1.15}, {r * 1.05, r * .85}, {-r * 1.05, r * .85}}; break;
    case LineMarker::diamond: g.polygon = {{0, -r * 1.2}, {r * 1.2, 0}, {0, r * 1.2}, {-r * 1.2, 0}}; break;
    case LineMarker::cross: g.strokes = {{{{-r, -r}, {r, r}}}, {{{-r, r}, {r, -r}}}}; break;
    case LineMarker::plus: g.strokes = {{{{-r, 0}, {r, 0}}}, {{{0, -r}, {0, r}}}}; break;
    case LineMarker::star:
        g.strokes = {{{{-r, 0}, {r, 0}}}, {{{0, -r}, {0, r}}}, {{{-r * .7, -r * .7}, {r * .7, r * .7}}}, {{{-r * .7, r * .7}, {r * .7, -r * .7}}}};
        break;
    }
    return g;
}

std::string line_styles_json()
{
    const auto point = [](const std::array<double, 2> &p) { return xpp::format("[{:g},{:g}]", p[0], p[1]); };
    std::string o = "{\"dash\":{";
    for (std::size_t i = 0; i < line_dash_names.size(); i++) {
        o += xpp::format("{}\"{}\":[", i ? "," : "", line_dash_names[i]);
        const std::vector<double> pattern = dash_pattern(static_cast<LineDash>(i), 1);
        for (std::size_t k = 0; k < pattern.size(); k++) o += xpp::format("{}{:g}", k ? "," : "", pattern[k]);
        o += ']';
    }
    o += "},\"markers\":{";
    for (std::size_t i = 0; i < line_marker_names.size(); i++) {
        const MarkerGlyph g = marker_glyph(static_cast<LineMarker>(i), 1);
        o += xpp::format("{}\"{}\":{{\"circle\":{:g},\"polygon\":[", i ? "," : "", line_marker_names[i], g.circle_radius);
        for (std::size_t k = 0; k < g.polygon.size(); k++) o += (k ? "," : "") + point(g.polygon[k]);
        o += "],\"strokes\":[";
        for (std::size_t k = 0; k < g.strokes.size(); k++) o += xpp::format("{}[{},{}]", k ? "," : "", point(g.strokes[k][0]), point(g.strokes[k][1]));
        o += "]}";
    }
    return o + "}}";
}

std::string symbol_to_unicode(std::string_view text)
{
    /* the symbol font's Latin letters and the Greek letter each shows (Adobe Symbol's encoding) */
    static constexpr std::string_view latin = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr std::array<std::string_view, 52> greek{
        "α", "β", "χ", "δ", "ε", "φ", "γ", "η", "ι", "ϕ", "κ", "λ", "μ", "ν", "ο", "π", "θ", "ρ", "σ", "τ", "υ", "ϖ", "ω", "ξ", "ψ", "ζ",
        "Α", "Β", "Χ", "Δ", "Ε", "Φ", "Γ", "Η", "Ι", "ϑ", "Κ", "Λ", "Μ", "Ν", "Ο", "Π", "Θ", "Ρ", "Σ", "Τ", "Υ", "ς", "Ω", "Ξ", "Ψ", "Ζ"};
    std::string out;
    for (const char c : text) {
        const std::size_t at = latin.find(c);
        if (at == std::string_view::npos) out += c;
        else out += greek[at];
    }
    return out;
}

std::vector<TextRun> text_runs(std::string_view text, double size, bool symbol)
{
    std::vector<TextRun> runs;
    std::string current;
    bool in_symbol = symbol;
    double run_size = size, rise = 0;
    const auto flush = [&] {
        if (!current.empty()) runs.push_back(TextRun{current, run_size, rise, in_symbol});
        current.clear();
    };
    for (std::size_t i = 0; i < text.size(); i++) {
        if (text[i] != '\\') {
            current += text[i];
            continue;
        }
        flush();
        const char e = ++i < text.size() ? text[i] : '\0';
        if (e == '0') in_symbol = false;
        else if (e == '1') in_symbol = true;
        else if (e == 'n') {
            rise = 0;
            run_size = size;
        } else if (e == 's') {
            rise -= sub_shift * size;
            run_size = script_size * size;
        } else if (e == 'S') {
            rise += sup_shift * size;
            run_size = script_size * size;
        }
    }
    flush();
    return runs;
}

void set_pen(Session &s, const Color &color, const LineStyle &style) { s.plot_file.pen = Pen{color, style}; }

void set_pen_color(Session &s, const Color &color) { s.plot_file.pen.color = color; }

void set_pen_style(Session &s, const LineStyle &style) { s.plot_file.pen.style = style; }

void set_border_pen(Session &s)
{
    LineStyle style;
    style.width = 1.5;
    s.plot_file.pen = Pen{foreground_color, style};
}

void set_zero_axes_pen(Session &s)
{
    LineStyle style;
    style.width = .75;
    style.dash = LineDash::dotted;
    s.plot_file.pen = Pen{foreground_color, style};
}

void set_text_color(Session &s, const Color &color) { s.plot_file.text_color = color; }

void picture_line(Session &s, int x1, int y1, int x2, int y2)
{
    PlotFileState &f = s.plot_file;
    Pen pen = f.pen;
    pen.color = ink(f.picture, pen.color);
    const std::array<double, 2> from{static_cast<double>(x1), static_cast<double>(y1)}, to{static_cast<double>(x2), static_cast<double>(y2)};
    /* a segment that starts where the last one in the same pen ended goes on its path */
    if (!f.picture.items.empty())
        if (auto *last = std::get_if<PicturePath>(&f.picture.items.back()); last && last->pen == pen && last->points.back() == from) {
            last->points.push_back(to);
            return;
        }
    f.picture.items.emplace_back(PicturePath{{from, to}, pen});
}

void picture_marker(Session &s, int x, int y)
{
    PlotFileState &f = s.plot_file;
    Pen pen = f.pen;
    pen.color = ink(f.picture, pen.color);
    f.picture.items.emplace_back(PictureMarker{static_cast<double>(x), static_cast<double>(y), pen});
}

void picture_bead(Session &s, int x, int y)
{
    constexpr double bead_radius = 1.5; /* a direction field arrow's head, a dot */
    PlotFileState &f = s.plot_file;
    Pen pen = f.pen;
    pen.color = ink(f.picture, pen.color);
    pen.style.draw = LineDraw::points;
    pen.style.marker = LineMarker::circle;
    pen.style.marker_size = bead_radius;
    f.picture.items.emplace_back(PictureMarker{static_cast<double>(x), static_cast<double>(y), pen});
}

void picture_rect(Session &s, int x, int y, int w, int h)
{
    PlotFileState &f = s.plot_file;
    /* the corner (x, y) is the bottom left, as the plot's coordinates grow up: the picture's rectangle starts at the top */
    f.picture.items.emplace_back(PictureRect{static_cast<double>(x), static_cast<double>(y - h), static_cast<double>(w), static_cast<double>(h),
                                             ink_fill(f.picture, f.pen.color)});
}

namespace {

constexpr double baseline_below_centre = .35; /* a text centred on y has its baseline this many text heights below it */

void put_runs(Session &s, double x, double y, TextAnchor anchor, double angle, std::vector<TextRun> runs, double size)
{
    PlotFileState &f = s.plot_file;
    f.picture.items.emplace_back(PictureText{x, y + baseline_below_centre * size, anchor, angle, ink(f.picture, f.text_color), std::move(runs)});
}

} // namespace

PictureText centred_text(double x, double y, TextAnchor anchor, double angle, std::string_view text, double size)
{
    return PictureText{x, y + baseline_below_centre * size, anchor, angle, black, text_runs(text, size)};
}

void picture_text(Session &s, int x, int y, std::string_view text)
{
    constexpr int left = 0, center = 1; /* DrawingState::text_justify: 0 left, 1 centre, 2 right */
    const int justify = s.drawing.text_justify;
    const double size = s.plot_file.picture.font_size;
    put_runs(s, x, y, justify == left ? TextAnchor::start : justify == center ? TextAnchor::middle : TextAnchor::end, 0, text_runs(text, size), size);
}

void picture_label(Session &s, int x, int y, std::string_view text, int size, bool symbol)
{
    const double points = label_sizes[static_cast<std::size_t>(std::clamp(size, 0, static_cast<int>(label_sizes.size()) - 1))];
    put_runs(s, x, y, TextAnchor::start, 0, text_runs(text, points, symbol), points);
}

void picture_y_label(Session &s, std::string_view text)
{
    constexpr double columns_left_of_axis = 5.75; /* in characters: clear of the tick numbers */
    const double size = s.plot_file.picture.font_size;
    const DrawingState &d = s.drawing;
    put_runs(s, d.d_left - columns_left_of_axis * d.h_char, .5 * (d.d_top + d.d_bottom), TextAnchor::middle, 90, text_runs(text, size), size);
}

void picture_group(Session &s, bool begin)
{
    if (s.plot_file.format) s.plot_file.picture.items.emplace_back(PictureGroup{begin});
}

} // namespace xpp
