#pragma once

/* Compression, the one module for it (xpp_zip.cpp, over the vendored
   third_party/miniz, which no other file includes): gzip (RFC 1952) and
   zip archives, written as the bytes come (to a callback: a Sink, or a string) and read from memory. Writing the bytes to a file is
   xpp_io's (xpp::Writer::binary), reading them xpp::read_bytes. C++ only.
   None of these throws: a failed allocation ends the program through
   xpp::out_of_memory (xpp_mem.h), like every other core allocation. */
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "xpp_error.h"

namespace xpp::zip {

/* Compression as it is made (W748): the streaming writers below take the bytes in pieces and hand what they compress
   to an Emit as it comes (a Sink's write, or a string's append), so nothing holds the whole data or the whole result.
   An Emit returns the error that stopped it, which ends the writer: every later call returns the same error. The place
   names the file for an error of the writer's own (an entry or archive past what the format counts in 32 bits). No file
   is opened here. */
using Emit = std::function<Result<>(std::string_view)>;

/* raw deflate (RFC 1951) with the CRC-32 and the length of what went in, for a gzip trailer or a zip entry */
class Deflater {
public:
    Deflater(Emit emit, Place place);
    ~Deflater();
    Deflater(const Deflater &) = delete;
    Deflater &operator=(const Deflater &) = delete;
    Result<> write(std::string_view bytes);
    /* the deflate stream is complete; crc() and size() are then final */
    Result<> finish();
    std::uint32_t crc() const;
    std::uint64_t size() const;

    struct Impl; /* its state, named by the compressor's callback */

private:
    std::unique_ptr<Impl> impl_;
};

/* one gzip member written in pieces (deflated, no name, time stamp 0: the same bytes give the same file) */
class GzipWriter {
public:
    GzipWriter(Emit emit, Place place);
    Result<> write(std::string_view bytes);
    Result<> finish();

private:
    Result<> begin();
    Emit emit_;
    Deflater deflater_;
    bool begun_ = false;
};

/* a zip archive written in pieces: each entry deflated as its bytes come, its sizes and CRC in a data descriptor after
   them (sizes are not known before: the local header is written first and the sink cannot be gone back into), the
   central directory in finish(). Entry names, the order and the timestamp (1980-01-01, so the same bytes give the same
   file) are what make_zip always gave. Every reader of zips (numpy's, Python's zipfile, read_zip) reads the descriptor. */
class ZipWriter {
public:
    ZipWriter(Emit emit, Place place);
    ~ZipWriter();
    ZipWriter(const ZipWriter &) = delete;
    ZipWriter &operator=(const ZipWriter &) = delete;
    Result<> begin_entry(std::string_view name);
    Result<> write(std::string_view bytes);
    Result<> end_entry();
    /* the central directory and the end record: the archive is complete */
    Result<> finish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/* Cap archive input and total expanded bytes (a zip's entries together, a
   gzip's members together): enough for a large data table, while bounding
   what a sent file can make the program allocate; 4096 members covers
   models and data columns without letting metadata create an unbounded
   list. */
inline constexpr std::size_t archive_bytes_limit = 512 * 1024 * 1024;
inline constexpr std::size_t archive_entries_limit = 4096;

/* Account for expanded objects, not their compressed text, against archive_bytes_limit. No multiplication
   overflows and a refused claim leaves the running byte estimate unchanged. */
bool account_memory(std::size_t &used, std::size_t count, std::size_t bytes_each);

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

/* the rule for a name an archive gives a file it holds, applied before the name is used for anything (W622: the
   program a downloaded archive carries): a plain relative path of at most ENTRY_NAME_BYTES, "/" between names, none of
   them empty, "." or "..", and no backslash, drive colon, control character or leading "/" (a folder's trailing "/" is
   the caller's to remove) */
inline constexpr std::size_t ENTRY_NAME_BYTES = 255;
bool entry_name_ok(std::string_view name);

/* the file entries of a tar archive (ustar or GNU, as `tar -c` writes it), the members of a .tar.gz after gunzip
   (read_tar_gz: `file` is its name, for an error's place). A tar is read whole and checked before anything is
   returned: every header's checksum, every name by entry_name_ok, a size that fits the bytes that follow it, the
   sum of the sizes, the member count and a name twice; a member that is neither a file nor a folder (a link,
   a device, an extended header, a sparse or very large file) is refused, never skipped, so what it does not say
   cannot be applied. The error names the member. */
Result<std::vector<Entry>> read_tar(std::string_view tar, std::string_view file);
Result<std::vector<Entry>> read_tar_gz(std::string_view gz, std::string_view file);

} // namespace xpp::zip

