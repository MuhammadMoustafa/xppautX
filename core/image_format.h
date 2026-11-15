#pragma once
/* The picture-export registry (W53, issue #101; PostScript went and PDF came with W479, #542): one row per format instead of a
   per-format branch at each place a picture is written, and the operations that write a picture out (image_format.cpp). The rows'
   type is picture_state.h's; the rings below the export reach the rows and the operations through ExportServices
   (export_services.h), which the program's start fills from here; the rings above call them directly. C++ only. */
#include <span>
#include <string_view>

#include "export_services.h"
#include "picture_state.h"
#include "xpp_error.h"
#include "xpp_fs.h"

namespace xpp {

struct Session; /* session.h */

extern const ImageFormat image_formats[];

/* A vector picture begun into filename (s.plot_file->writer is empty when the user declined to replace
   it), or why it cannot be written; the plot is then drawn through graphics.cpp's primitives and the
   picture ended: written and renamed into place */
Result<> begin_picture(Session &s, const ImageFormat &format, std::string_view filename);
/* the picture ended: written and renamed into place, or why not */
Result<> end_picture(Session &s);
/* a finished picture written to filename in format, asking before it replaces a file: false when the user declined */
Result<bool> write_picture(FileSystem &fs, const Picture &picture, const ImageFormat &format, const char *filename);

Result<bool> save_pixels(FileSystem &fs, const ImageFormat &format, const char *filename, std::span<const unsigned char> rgb, int width, int height);

/* the vector picture format a file's extension names (output.picture_file: .pdf or .svg), or nullptr */
inline const ImageFormat *picture_format_of_file(std::string_view path) { return picture_format_of_file(std::span<const ImageFormat>(image_formats, n_image_formats), path); }

} // namespace xpp
