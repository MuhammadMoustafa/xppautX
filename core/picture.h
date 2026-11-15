#pragma once
/* The picture the plot windows record while they are redrawn into a PDF or SVG file, and what the array plot builds the same way
   (W479, #542); the data is picture_state.h's. Everything the two formats share is decided here once. The writers (my_pdf.cpp,
   my_svg.cpp) only emit syntax for the items. Below, the picture-export registry the plot asks (W53, #101: one row per
   format instead of a per-format branch at each place a picture is written), declared here, owned by the plot, and defined by
   image_format.cpp, the export that writes the formats out (W737). C++ only. */

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "picture_state.h"
#include "xpp_error.h"

namespace xpp {

struct Session;     /* session.h */

/* the color the picture draws c in: the foreground is black */
Color ink(const Color &c);

/* the dash pattern of a line (on, off, on, off ... lengths in points; none: solid) */
std::vector<double> dash_pattern(LineDash dash, double width);

/* a marker as drawn around its centre, in points: a filled circle (circle) or polygon, and strokes */
struct MarkerGlyph {
    double circle_radius = 0; /* a filled circle when above 0 */
    std::vector<std::array<double, 2>> polygon; /* filled, closed */
    std::vector<std::array<std::array<double, 2>, 2>> strokes;
};
MarkerGlyph marker_glyph(LineMarker marker, double size);
/* what a front end needs to draw the styles as the pictures do (hello's `line_styles`): each dash's pattern at a
   width of 1 (a wider line takes max(width, 1) times it) and each marker's glyph at size 1 (a size s takes
   max(s, .5) times it), as a JSON object */
std::string line_styles_json();

/* text with XPPAUT's markup as runs: a backslash and 0 or 1 switch to the roman or symbol font, s and S a
   subscript and a superscript one size smaller (they add up), n back to the baseline and size; any other
   character after a backslash is dropped. symbol: the whole text starts in the symbol font */
std::vector<TextRun> text_runs(std::string_view text, double size, bool symbol = false);
/* a symbol run's text in Unicode (its Latin letters as the Greek ones they show) */
std::string symbol_to_unicode(std::string_view text);

/* text of one size centred on y vertically (the baseline is where the picture needs it), black in the foreground */
PictureText centred_text(double x, double y, TextAnchor anchor, double angle, std::string_view text, double size);

/* the primitives graphics.cpp's drawing calls while a picture is being written: they record into
   s.plot_file->picture with the pen set by set_pen */
void picture_line(Session &s, int x1, int y1, int x2, int y2);
void picture_marker(Session &s, int x, int y);
void picture_bead(Session &s, int x, int y);
void picture_rect(Session &s, int x, int y, int w, int h);
/* ordinary text (the axes' numbers and names) justified by s.drawing->text_justify, centred on y */
void picture_text(Session &s, int x, int y, std::string_view text);
/* a label (size 0 to 4, markup) starting at x, y */
void picture_label(Session &s, int x, int y, std::string_view text, int size, bool symbol);
/* the y axis' label, turned along the axis and centred on it */
void picture_y_label(Session &s, std::string_view text);
/* the items until the end are one group (a curve, the direction field's arrows); nothing when no picture is begun */
void picture_group(Session &s, bool begin);
void set_pen(Session &s, const Color &color, const LineStyle &style);
/* the pen's color or style alone */
void set_pen_color(Session &s, const Color &color);
void set_pen_style(Session &s, const LineStyle &style);
/* the pens of the box around the plot and of the zero axes */
void set_border_pen(Session &s);
void set_zero_axes_pen(Session &s);
/* the color of the text recorded next (a label's color; the foreground for the axes) */
void set_text_color(Session &s, const Color &color);

} // namespace xpp
