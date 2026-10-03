#ifndef DATA_FORMATS_H
#define DATA_FORMATS_H

/* The data file formats (data_formats.cpp, docs/roadmap.md W52): a table
   of numbers written as XPP's own .dat, CSV with a header row, gzipped
   CSV or NumPy's .npz, and read back from any of them. The registry is
   one explicit table, one line per format (data_formats.cpp): the Save
   data dialog (browse_data.cpp data_write) lists it and the browser's
   Load picks from it by a file's extension. C++ only. */
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_io.h"

namespace xpp {

/* Named columns of equal length; readers retain the file's double values. */
struct DataTable {
    std::vector<std::string> names; /* a column without one is col<i+1> */
    std::vector<std::vector<double>> columns;
    /* Storage is float: its CSV values must still use float's shortest text.
       Readers clear this marker because the file can contain doubles. */
    bool stored_floats = true;
    /* CSV-only tables (AUTO): text and full-precision numeric fields share
       the registry's quoter, without rounding AUTO's doubles to floats. */
    std::vector<std::vector<std::string>> fields;
    /* Source lines for text tables, so consumers report the actual row,
       including CSV's header and seed comment. Binary tables have none. */
    std::vector<int> lines;
    int error_line = 0; /* a failed text read keeps only its offending line */
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
std::string data_column_name(const DataTable &t, std::size_t i);

struct DataFormat {
    const char *id;        /* the protocol's name for it ("dat", "csv", ...) */
    const char *title;     /* the Save data menu's item */
    const char *extension; /* with its dot: ".dat", ".csv.gz" */
    char key;              /* its key in that menu */
    bool binary;           /* written byte for byte (xpp::Writer::binary) */
    /* table into w (opened by the caller, binary or not as above, and
       committed by it); false when it could not be written */
    bool (*write)(const DataTable &table, xpp::Writer &w);
    /* path's table; false (and table left empty) when it is not one of
       this format; nullptr for a format that is only written */
    bool (*read)(const char *path, DataTable &table);
};
/* The format functions never throw: a failed allocation ends the program
   through xpp::out_of_memory (xpp_mem.h). */

/* every format, in the Save data menu's order */
std::span<const DataFormat> data_formats();
/* the format of that id (case ignored), or nullptr */
const DataFormat *data_format_named(std::string_view id);
/* the format whose extension path ends with (case ignored), or nullptr */
const DataFormat *data_format_of_file(std::string_view path);
/* The same registry reader, with a file/line error for command consumers. */
Result<DataTable> read_data_table(const char *path);

/* NPZ's writer and reader on bytes in memory rather than a file: the
   session file's data.npz and frozen.npz (xpp_session.cpp, W57) */
std::string npz_bytes(const DataTable &table) noexcept;
/* false (and table empty) when bytes are not a .npz this reads */
bool npz_table(std::string_view bytes, DataTable &table) noexcept;

} // namespace xpp

#endif
