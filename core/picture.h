#pragma once
/* The picture an export writes (W479, #542): what graphics.cpp's primitives record while the plot is
   redrawn into a PDF or SVG file, and what the array plot builds the same way. Everything the two
   formats share is decided here once: a colour with its alpha (black and white too), a line's dash
   pattern, a marker's shape, a text's runs (the sub- and superscripts, the symbol font), the axis
   label's place. The writers (my_pdf.cpp, my_svg.cpp) only emit syntax for the items. */

#include <array>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "colormap.h"
#include "line_style.h"
#include "xpp_io.h"

namespace xpp {

struct Session;     /* session.h */
struct ImageFormat; /* image_format.h */

/* the device of every picture: points, the origin at the top left, y growing down */
inline constexpr int picture_width = 640, picture_height = 400;

/* what a stroke is drawn with */
struct Pen {
    Color color = foreground_color;
    LineStyle style;
    bool hollow = false; /* a marker drawn as an outline (an unstable periodic orbit's circle) */
    friend bool operator==(const Pen &, const Pen &) = default;
};

/* XPPAUT's five text sizes (a label's size 0 to 4) in points: the page draws them as 1/2, 3/4, 1, 5/4 and 3/2 of its font */
inline constexpr std::array<double, 5> label_sizes{6, 9, 12, 15, 18};

/* a piece of text in one font and size, its baseline moved by rise points (up positive) */
struct TextRun {
    std::string text; /* as written; a symbol run's letters are the symbol font's (Adobe Symbol: "a" is alpha) */
    double size = 12;
    double rise = 0;
    bool symbol = false;
};
enum class TextAnchor { start, middle, end };

/* connected segments in one pen: a line is a path of two points, a curve of a run is one path */
struct PicturePath {
    std::vector<std::array<double, 2>> points;
    Pen pen;
};
struct PictureMarker {
    double x, y;
    Pen pen;
};
struct PictureRect {
    double x, y, w, h; /* the top left corner and the size */
    Color color;       /* filled; its alpha is drawn */
};
struct PictureText {
    double x, y;   /* the baseline's start, middle or end by anchor */
    TextAnchor anchor;
    double angle;  /* degrees, counterclockwise */
    Color color;
    std::vector<TextRun> runs;
};
struct PictureGroup {
    bool begin; /* a group of the items between its begin and its end (the curve or the direction field's arrows) */
};
using PictureItem = std::variant<PicturePath, PictureMarker, PictureRect, PictureText, PictureGroup>;

struct Picture {
    bool color = true;      /* in colour: otherwise strokes and text are black and fills gray */
    double font_size = 12;  /* the axes' text, points */
    std::vector<PictureItem> items;
};

/* a picture's file state, a Session's (session.h): the vector format being written (nullptr: none, the
   plot only goes to the page), its picture so far, the file and the pen the next stroke uses */
struct PlotFileState {
    const ImageFormat *format = nullptr;
    Picture picture;
    Writer writer;
    Pen pen;
    Color text_color = foreground_color;
};

/* the colour the picture draws c in: the foreground black, a stroke or text black in black and white */
Color ink(const Picture &p, const Color &c);
/* the same for a filled area: gray by its luminance in black and white */
Color ink_fill(const Picture &p, const Color &c);

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
   s.plot_file.picture with the pen set by set_pen */
void picture_line(Session &s, int x1, int y1, int x2, int y2);
void picture_marker(Session &s, int x, int y);
void picture_bead(Session &s, int x, int y);
void picture_rect(Session &s, int x, int y, int w, int h);
/* ordinary text (the axes' numbers and names) justified by s.drawing.text_justify, centred on y */
void picture_text(Session &s, int x, int y, std::string_view text);
/* a label (size 0 to 4, markup) starting at x, y */
void picture_label(Session &s, int x, int y, std::string_view text, int size, bool symbol);
/* the y axis' label, turned along the axis and centred on it */
void picture_y_label(Session &s, std::string_view text);
/* the items until the end are one group (a curve, the direction field's arrows); nothing when no picture is begun */
void picture_group(Session &s, bool begin);
void set_pen(Session &s, const Color &color, const LineStyle &style);
/* the pen's colour or style alone */
void set_pen_color(Session &s, const Color &color);
void set_pen_style(Session &s, const LineStyle &style);
/* the pens of the box around the plot and of the zero axes */
void set_border_pen(Session &s);
void set_zero_axes_pen(Session &s);
/* the colour of the text recorded next (a label's colour; the foreground for the axes) */
void set_text_color(Session &s, const Color &color);
} // namespace xpp
