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

/* named columns of equal length, single precision like the stored data */
struct DataTable {
    std::vector<std::string> names; /* a column without one is col<i+1> */
    std::vector<std::vector<float>> columns;
    /* column 0 numbers each row's curve (what the plot shows, one long
       table): NPZ writes one array per curve instead of one per column */
    bool curves = false;
    /* the run's own seed (session.h numerics.last_seed, W71), when the
       data came from one: a CSV/CSV.gz header comment line, an NPZ
       "seed" array; empty (the common case outside a stochastic model,
       or data never run at all) writes neither. */
    std::optional<int> seed;
    std::size_t rows() const { return columns.empty() ? 0 : columns[0].size(); }
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
   through xpp_out_of_memory (xpp_mem.h). */

/* every format, in the Save data menu's order */
std::span<const DataFormat> data_formats();
/* the format of that id (case ignored), or nullptr */
const DataFormat *data_format_named(std::string_view id);
/* the format whose extension path ends with (case ignored), or nullptr */
const DataFormat *data_format_of_file(std::string_view path);

} // namespace xpp

#endif
