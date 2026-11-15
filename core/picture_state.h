#pragma once
/* The data of the picture an export writes (W479, #542): what graphics.cpp's primitives record while the plot is
   redrawn into a PDF or SVG file, and what the array plot builds the same way. Everything the two
   formats share is decided here once: a color with its alpha, a line's dash
   pattern, a marker's shape, a text's runs (the sub- and superscripts, the symbol font), the axis
   label's place. The writers (my_pdf.cpp, my_svg.cpp) only emit syntax for the items. Split from picture.h at W737: the pure
   data a Session holds (its plot_file), below the plot's recorder. */
#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "colormap.h"
#include "line_style.h"
#include "xpp_error.h"
#include "xpp_fs.h"

namespace xpp {

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

/* the axes' text size of a picture, the size the page's panel draws it at: web2's --plot-font
   (src/css/tokens.css), 12px Inter, one unit of the picture to a px */
inline constexpr double picture_font_size = 12;

struct Picture {
    double font_size = picture_font_size; /* the axes' text; array_print.cpp sets its own */
    std::vector<PictureItem> items;
};

/* a picture's file state, a Session's (session.h): the vector format being written (nullptr: none, the
   plot only goes to the page), its picture so far, the file and the pen the next stroke uses */
struct ImageFormat;
struct PlotFileState {
    const ImageFormat *format = nullptr;
    Picture picture;
    std::unique_ptr<Sink> writer; /* the file being written (image_format.cpp), none when the user declined to replace it */
    Pen pen;
    Color text_color = foreground_color;
};

/* PDF and SVG are vector pictures: what graphics.cpp's primitives draw while a plot is redrawn is
   recorded into a Picture (picture.h) and `write` puts it into the file at the end. GIF (the
   kinescope's and the array plot's, json_windows.cpp) and PPM use the pixels hook; frame -1 is
   a still, zero starts a movie, and subsequent frame numbers append to it. */
struct ImageFormat {
    const char *name;      /* "PDF", "SVG", "GIF" */
    const char *extension; /* no dot: "pdf", "svg", "gif" */
    Result<> (*write)(const Picture &picture, Sink &out) = nullptr; /* a vector picture's writer */
    bool binary = false;   /* its file is bytes, not text */
    Result<> (*pixels)(Sink &w, std::span<const unsigned char> rgb, int width, int height, int frame) = nullptr;
    Result<> (*finish_movie)(Sink &w) = nullptr;
};

constexpr int n_image_formats = 4; /* one row for each supported picture format */
/* image_formats' own order, for a caller that picks by index (the autoX
   diagram's export menu, graf_par.cpp's plot-window export) rather than
   by name */
constexpr int IMAGE_FORMAT_PDF = 0;
constexpr int IMAGE_FORMAT_SVG = 1;
constexpr int IMAGE_FORMAT_GIF = 2;
constexpr int IMAGE_FORMAT_PPM = 3;
constexpr int IMAGE_STILL_FRAME = -1; /* a still has no index in a movie */

} // namespace xpp
