#pragma once
/* The data file formats' registry (data_formats.cpp, W52): the one explicit table, one line per format, the Save data dialog lists it
   and the browser's Load picks from it by a file's extension; the table type and the lookups over it are data_table.h's. The rings
   below the export reach it through ExportServices (export_services.h), which the program's start fills from here; the rings
   above call it directly. C++ only. */
#include <span>
#include <string>
#include <string_view>

#include "data_table.h"

namespace xpp {

/* every format, in the Save data menu's order */
std::span<const DataFormat> data_formats();
/* the format of that id (case ignored), or nullptr */
inline const DataFormat *data_format_named(std::string_view id) { return data_format_named(data_formats(), id); }
/* the format whose extension path ends with (case ignored), or nullptr */
inline const DataFormat *data_format_of_file(std::string_view path) { return data_format_of_file(data_formats(), path); }
inline Result<DataTable> read_data_table(FileSystem &fs, std::string_view path) { return read_data_table(data_formats(), fs, path); }

/* NPZ's writer and reader on bytes in memory rather than a file: the
   session file's data.npz and frozen.npz (xpp_session.cpp, W57) */
std::string npz_bytes(const DataTable &table) noexcept;
/* false (and table empty) when bytes are not a .npz this reads */
bool npz_table(std::string_view bytes, DataTable &table) noexcept;

} // namespace xpp
