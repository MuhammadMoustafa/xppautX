#pragma once
/* What the rings below the export need of it, given to a load by the program's start (LoadServices::exports, model_file_types.h): the
   registries of data and picture formats as data, and the three operations that write a picture out as a table of functions the
   export (data_formats.cpp, image_format.cpp) implements and the program's start fills, as XppUi is. The lookups over the
   registries are pure and live with the types (data_table.h, here). W737. */
#include <span>
#include <string_view>

#include "data_table.h"
#include "picture_state.h"
#include "xpp_error.h"
#include "xpp_path.h"

namespace xpp {

struct Session; /* session.h */
class FileSystem; /* xpp_fs.h */

struct ExportServices {
    std::span<const DataFormat> data_formats;   /* data_formats.h */
    std::span<const ImageFormat> image_formats; /* image_format.h */
    /* a vector picture begun into filename (s.plot_file->writer is empty when the user declined to replace it), or why it cannot be
       written; the plot is then drawn through the recorder and the picture ended: written and renamed into place */
    Result<> (*begin_picture)(Session &s, const ImageFormat &format, std::string_view filename) = nullptr;
    Result<> (*end_picture)(Session &s) = nullptr;
    /* a finished picture written to filename in format, asking before it replaces a file: false when the user declined */
    Result<bool> (*write_picture)(FileSystem &fs, const Picture &picture, const ImageFormat &format, const char *filename) = nullptr;
};

/* the vector picture format among formats whose extension path ends with (output.picture_file: .pdf or .svg), or nullptr */
inline const ImageFormat *picture_format_of_file(std::span<const ImageFormat> formats, std::string_view path)
{
    for (const ImageFormat &f : formats)
        if (f.write && files::has_extension(path, std::string(".") + f.extension)) return &f;
    return nullptr;
}

} // namespace xpp
