#pragma once

/* Compression, the one module for it (xpp_zip.cpp, over the vendored
   third_party/miniz, which no other file includes): gzip (RFC 1952) and
   zip archives, built and read in memory. Writing the bytes to a file is
   xpp_io's (xpp::Writer::binary), reading them xpp::read_bytes. C++ only.
   None of these throws: a failed allocation ends the program through
   xpp::out_of_memory (xpp_mem.h), like every other core allocation. */
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace xpp::zip {

/* bytes as one gzip member (deflated, no name, time stamp 0: the same
   bytes give the same file) */
std::string gzip(std::string_view bytes);

/* Cap archive input and total expanded bytes (a zip's entries together, a
   gzip's members together): enough for a large data table, while bounding
   what a sent file can make the program allocate; 4096 members covers
   models and data columns without letting metadata create an unbounded
   list. */
inline constexpr std::size_t archive_bytes_limit = 512 * 1024 * 1024;
inline constexpr std::size_t archive_entries_limit = 4096;

/* the bytes of a gzip file (every member, one after the other), or
   nothing when it is not one or is damaged (a CRC or length that does
   not match) or expands past limit (then *over_limit, when given, is true) */
std::optional<std::string> gunzip(std::string_view gz, std::size_t limit = archive_bytes_limit, bool *over_limit = nullptr);

/* an entry of a zip archive: its name and its (uncompressed) bytes */
struct Entry {
    std::string name;
    std::string bytes;
};

/* a zip archive of these entries, in this order, each deflated */
std::string make_zip(const std::vector<Entry> &entries);

/* bytes begin as a zip archive does (its first local file header) */
bool is_zip(std::string_view bytes);

/* the file entries of a zip archive (folders left out), in the archive's
   order, or nothing when it is not one or an entry cannot be read */
std::optional<std::vector<Entry>> read_zip(std::string_view zip);

} // namespace xpp::zip

