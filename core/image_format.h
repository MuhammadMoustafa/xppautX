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
    const char *plot_title = nullptr; /* the file selector's title, a vector picture's */
    const char *plot_key = nullptr;   /* the key of the Graphic stuff menu that writes it (--silent's answer) */
};

/* the vector picture formats' names, as @ plotfmt takes them */
inline constexpr std::array<std::string_view, 2> picture_format_names{"pdf", "svg"};

extern const ImageFormat image_formats[];
constexpr int n_image_formats = 4; /* one row for each supported picture format */
/* image_formats' own order, for a caller that picks by index (the AUTO
   diagram's export menu, graf_par.cpp's plot-window export) rather than
   by name */
constexpr int IMAGE_FORMAT_PDF = 0;
constexpr int IMAGE_FORMAT_SVG = 1;
constexpr int IMAGE_FORMAT_GIF = 2;
constexpr int IMAGE_FORMAT_PPM = 3;
constexpr int IMAGE_STILL_FRAME = -1; /* a still has no index in a movie */

/* the picture parameters (in colour, the axes' font size) asked before the file selector, 0 if the user
   cancelled, and their values as --silent answers them */
int ask_picture_params(Session &s);
std::vector<std::string> picture_parameter_values(const Session &s);
/* the default file name of the session's picture in the format */
std::string picture_filename(const Session &s, const ImageFormat &format);

/* A vector picture begun into filename (s.plot_file.writer is empty when the user declined to replace
   it), or why it cannot be written; the plot is then drawn through graphics.cpp's primitives and the
   picture ended: written and renamed into place */
Result<> begin_picture(Session &s, const ImageFormat &format, std::string_view filename, bool color);
void end_picture(Session &s);

Result<bool> save_pixels(const ImageFormat &format, const char *filename, std::span<const unsigned char> rgb, int width, int height);

/* the format whose extension is `extension` (a Session's plot_export.format's
   own values, "pdf"/"svg": many_pops.cpp's dump_ps), or nullptr */
const ImageFormat *find_image_format_by_extension(std::string_view extension);

} // namespace xpp
