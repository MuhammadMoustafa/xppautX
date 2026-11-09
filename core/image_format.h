#pragma once
/* xpp::ImageFormat: the picture-export registry (W53, issue #101; PostScript went and PDF came with
   W479, #542). One row per format instead of a per-format branch at each place a picture is written.
   C++ only (every caller is a .cpp file). */
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp {

struct Session; /* session.h */
class Writer;
struct Picture; /* picture.h */

/* PDF and SVG are vector pictures: what graphics.cpp's primitives draw while a plot is redrawn is
   recorded into a Picture (picture.h) and `write` puts it into the file at the end. GIF (the
   kinescope's and the array plot's, json_windows.cpp) and PPM use the pixels hook; frame -1 is
   a still, zero starts a movie, and subsequent frame numbers append to it. */
struct ImageFormat {
    const char *name;      /* "PDF", "SVG", "GIF" */
    const char *extension; /* no dot: "pdf", "svg", "gif" */
    Result<> (*write)(const Picture &picture, Writer &out) = nullptr; /* a vector picture's writer */
    bool binary = false;   /* its file is bytes, not text */
    void (*pixels)(Writer &w, std::span<const unsigned char> rgb, int width, int height, int frame) = nullptr;
    void (*finish_movie)(Writer &w) = nullptr;
};

/* the vector picture formats' names: the extensions output.picture_file takes */
inline constexpr std::array<std::string_view, 2> picture_format_names{"pdf", "svg"};

extern const ImageFormat image_formats[];
constexpr int n_image_formats = 4; /* one row for each supported picture format */
/* image_formats' own order, for a caller that picks by index (the autoX
   diagram's export menu, graf_par.cpp's plot-window export) rather than
   by name */
constexpr int IMAGE_FORMAT_PDF = 0;
constexpr int IMAGE_FORMAT_SVG = 1;
constexpr int IMAGE_FORMAT_GIF = 2;
constexpr int IMAGE_FORMAT_PPM = 3;
constexpr int IMAGE_STILL_FRAME = -1; /* a still has no index in a movie */

/* A vector picture begun into filename (s.plot_file.writer is empty when the user declined to replace
   it), or why it cannot be written; the plot is then drawn through graphics.cpp's primitives and the
   picture ended: written and renamed into place */
Result<> begin_picture(Session &s, const ImageFormat &format, std::string_view filename);
/* the picture ended: written and renamed into place, or why not */
Result<> end_picture(Session &s);

Result<bool> save_pixels(const ImageFormat &format, const char *filename, std::span<const unsigned char> rgb, int width, int height);

/* the vector picture format a file's extension names (output.picture_file: .pdf or .svg), or nullptr */
const ImageFormat *picture_format_of_file(std::string_view path);

} // namespace xpp
