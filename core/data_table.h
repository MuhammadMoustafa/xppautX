#pragma once

/* A table of numbers, and the data file formats that write it and read it back (data_formats.cpp). The table is a plain value,
   its columns named or numbered, which the browser, the plot's curves and the sessions carry; the formats' registry is
   declared here, for the rings that hold or ask for a table, and defined by data_formats.cpp (W737: the export that writes
   them out). */
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"
#include "xpp_fs.h"
#include "xpp_text.h"
#include "xpp_zip.h"

namespace xpp {

/* Named columns of equal length; readers retain the file's double values. */
struct DataTable {
    std::vector<std::string> names; /* a column without one is col<i+1> */
    std::vector<std::vector<double>> columns;
    /* The values are the data store's floats (storage_precision single): .dat and CSV write each in a float's
       shortest text, else in a double's. Readers clear this marker because the file can contain doubles. */
    bool stored_floats = true;
    /* CSV-only tables (autoX): text and full-precision numeric fields share
       the registry's quoter, without rounding autoX's doubles to floats. */
    std::vector<std::vector<std::string>> fields;
    /* Source lines for text tables, so consumers report the actual row,
       including CSV's header and seed comment. Binary tables have none. */
    std::vector<int> lines;
    int error_line = 0; /* a failed text read keeps only its offending line */
    std::string error;  /* and, when the reader knows it, why it refused (a limit, a bad seed) */
    /* column 0 numbers each row's curve (what the plot shows, one long
       table): NPZ writes one array per curve instead of one per column */
    bool curves = false;
    /* the run's own seed (session.h numerics.last_seed, W71), when the
       data came from one: a CSV/CSV.gz header comment line, an NPZ
       "seed" array; empty (the common case outside a stochastic model,
       or data never run at all) writes neither. */
    std::optional<int> seed;
    std::size_t rows() const { return !fields.empty() ? fields[0].size() : columns.empty() ? 0 : columns[0].size(); }
    int line(std::size_t row) const { return row < lines.size() ? lines[row] : static_cast<int>(row) + 1; }
};

/* column i's name */
inline std::string data_column_name(const DataTable &t, std::size_t i)
{
    if (i < t.names.size() && !t.names[i].empty()) return t.names[i];
    return "col" + std::to_string(i + 1);
}


/* The data file formats (data_formats.cpp writes them, W52): a table
   of numbers written as XPP's own .dat, CSV with a header row, gzipped
   CSV or NumPy's .npz, and read back from any of them. The registry is
   one explicit table, one line per format (data_formats.cpp): the Save
   data dialog (browse_data.cpp data_write) lists it and the browser's
   Load picks from it by a file's extension. C++ only. */

/* Most columns one data file may give: an .npy header names its shape
   without the bytes to back it, so the count of columns (each a name and a
   vector) is bounded by itself; the file's bytes are bounded by
   zip::archive_bytes_limit. The same count as a zip's members. */
inline constexpr std::size_t data_columns_limit = 4096;

/* The export choices share the format registry's names, including their protocol spelling. */
inline constexpr const char *DATA_FORMAT_DAT="dat", *DATA_FORMAT_CSV="csv", *DATA_FORMAT_CSV_GZ="csv.gz", *DATA_FORMAT_NPZ="npz";

struct DataFormat {
    const char *id;        /* the protocol's name for it ("dat", "csv", ...) */
    const char *title;     /* the Save data menu's item */
    const char *extension; /* with its dot: ".dat", ".csv.gz" */
    char key;              /* its key in that menu */
    bool binary;           /* written byte for byte (a binary sink) */
    /* table into w (opened by the caller, binary or not as above, and
       committed by it), streamed as it is made where the format allows;
       the error when it could not be written */
    Result<> (*write)(const DataTable &table, xpp::Sink &w);
    /* the table in bytes (a file's whole text, or its bytes); false (and table left empty) when it is not one of
       this format; nullptr for a format that is only written */
    bool (*read)(std::string_view bytes, DataTable &table);
};
/* The format functions never throw: a failed allocation ends the program
   through xpp::out_of_memory (xpp_mem.h). */

/* the formats' ids in the registry's order (a choice of the protocol: data_formats.cpp checks it lists the registry) */
inline constexpr std::array data_format_ids = {DATA_FORMAT_DAT, DATA_FORMAT_CSV, DATA_FORMAT_CSV_GZ, DATA_FORMAT_NPZ};
/* the format of that id (case ignored) among formats (the registry: data_formats.h gives it, ExportServices passes it), or nullptr */
inline const DataFormat *data_format_named(std::span<const DataFormat> formats, std::string_view id)
{
    for (const DataFormat &f : formats)
        if (equal_ignoring_case(f.id, id)) return &f;
    return nullptr;
}
/* the format among formats whose extension path ends with (case ignored), or nullptr */
inline const DataFormat *data_format_of_file(std::span<const DataFormat> formats, std::string_view path)
{
    for (const DataFormat &f : formats) {
        const std::string_view e = f.extension;
        if (path.size() > e.size() && equal_ignoring_case(path.substr(path.size() - e.size()), e)) return &f;
    }
    return nullptr;
}
/* path ends in the extension of a data format (".dat", ".csv", ".csv.gz", ".npz"): the registry's rows say the same, from data_format_ids */
inline bool has_data_extension(std::string_view path)
{
    for (const std::string_view id : data_format_ids)
        if (path.size() > id.size() + 1 && path[path.size() - id.size() - 1] == '.' && equal_ignoring_case(path.substr(path.size() - id.size()), id)) return true;
    return false;
}
/* The same registry reader over the file at path, read through fs (at most zip::archive_bytes_limit bytes), with a
   file/line error for command consumers. */
inline Result<DataTable> read_data_table(std::span<const DataFormat> formats, FileSystem &fs, std::string_view path)
{
    const DataFormat *format = data_format_of_file(formats, path);
    if (!format || !format->read) return fail_reading("data", "has no registered data format", std::string(path));
    const Result<std::string> bytes = fs.read("data", path, zip::archive_bytes_limit);
    if (!bytes) return std::unexpected(bytes.error());
    DataTable table;
    if (!format->read(*bytes, table))
        return fail("data", table.error.empty() ? "cannot be read as a data table" : "cannot be read as a data table: " + table.error,
                    Place{std::string(path), table.error_line});
    return table;
}

} // namespace xpp
