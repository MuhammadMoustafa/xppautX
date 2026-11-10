/* Compression over the vendored miniz (xpp_zip.h). Every byte miniz hands
   back comes through a callback that appends it to a std::string, so no
   memory miniz allocates leaves it (core/xpp_mem.h); the callbacks never
   let an exception reach miniz's C code. */
#include "xpp_zip.h"

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <new>
#include <set>
#include <limits>

#include "miniz.h"
#include "xpp_io.h"
#include "xpp_mem.h"

namespace xpp::zip {

namespace {

/* the output of a callback: the bytes, and whether one could not be kept */
struct Sink {
    std::string bytes;
    bool out_of_memory = false;
    bool over_limit = false; /* a gunzip's output would pass limit */
    std::size_t limit = std::numeric_limits<std::size_t>::max();
};

mz_bool put_deflated(const void *buf, int len, void *user)
{
    Sink &s = *static_cast<Sink *>(user);
    try {
        if (static_cast<std::size_t>(len) > s.limit - s.bytes.size()) {
            s.over_limit = true;
            return MZ_FALSE;
        }
        s.bytes.append(static_cast<const char *>(buf), static_cast<std::size_t>(len));
    } catch (const std::bad_alloc &) {
        s.out_of_memory = true;
        return MZ_FALSE;
    }
    return MZ_TRUE;
}

int put_inflated(const void *buf, int len, void *user)
{
    return put_deflated(buf, len, user) ? 1 : 0;
}

/* a zip entry's bytes, or the archive being written, at offset ofs (the
   writer goes back to fill in a local header) */
std::size_t put_at(void *user, mz_uint64 ofs, const void *buf, std::size_t n)
{
    Sink &s = *static_cast<Sink *>(user);
    try {
        if (ofs > s.limit || n > s.limit - static_cast<std::size_t>(ofs)) return 0;
        const std::size_t at = static_cast<std::size_t>(ofs);
        if (s.bytes.size() < at + n) s.bytes.resize(at + n);
        std::memcpy(s.bytes.data() + at, buf, n);
    } catch (const std::bad_alloc &) {
        s.out_of_memory = true;
        return 0;
    }
    return n;
}

void put_le32(std::string &o, std::uint32_t v)
{
    for (int i = 0; i < 4; i++) o += static_cast<char>((v >> (8 * i)) & 0xff);
}

std::uint32_t get_le32(std::string_view s, std::size_t at)
{
    std::uint32_t v = 0;
    for (int i = 0; i < 4; i++) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(s[at + i])) << (8 * i);
    return v;
}

std::uint32_t crc32_of(std::string_view s)
{
    return static_cast<std::uint32_t>(
        mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>(s.data()), s.size()));
}

/* the gzip header's flag bits (RFC 1952 2.3.1) */
constexpr unsigned FHCRC = 2, FEXTRA = 4, FNAME = 8, FCOMMENT = 16;

/* one gzip member of gz from pos: its bytes appended to out, pos past it;
   false when it is not one or is damaged, or out would pass limit (over) */
bool gunzip_member(std::string_view gz, std::size_t &pos, std::string &out, std::size_t limit, bool &oom, bool &over)
{
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(gz[i]); };
    if (gz.size() - pos < 18 || byte(pos) != 0x1f || byte(pos + 1) != 0x8b || byte(pos + 2) != 8) return false;
    const unsigned flags = byte(pos + 3);
    std::size_t p = pos + 10;
    if (flags & FEXTRA) {
        if (gz.size() - p < 2) return false;
        p += 2 + (byte(p) | (byte(p + 1) << 8));
    }
    for (unsigned f : {FNAME, FCOMMENT})
        if (flags & f) {
            while (p < gz.size() && gz[p]) p++;
            p++;
        }
    if (flags & FHCRC) p += 2;
    if (p >= gz.size()) return false;
    Sink s;
    s.limit = limit - out.size();
    std::size_t in = gz.size() - p;
    const int ok = tinfl_decompress_mem_to_callback(gz.data() + p, &in, put_inflated, &s, 0);
    if (s.out_of_memory) oom = true;
    if (s.over_limit) over = true;
    if (ok != 1) return false;
    p += in;
    if (gz.size() - p < 8) return false;
    if (get_le32(gz, p) != crc32_of(s.bytes) || get_le32(gz, p + 4) != static_cast<std::uint32_t>(s.bytes.size()))
        return false;
    out += s.bytes;
    pos = p + 8;
    return true;
}

} // namespace

std::string gzip(std::string_view bytes)
{
    try {
        std::string o("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
        Sink s;
        const int flags = static_cast<int>(tdefl_create_comp_flags_from_zip_params(MZ_DEFAULT_LEVEL, -MZ_DEFAULT_WINDOW_BITS,
                                                                                  MZ_DEFAULT_STRATEGY));
        if (!tdefl_compress_mem_to_output(bytes.data(), bytes.size(), put_deflated, &s, flags)) xpp::out_of_memory("compressing");
        o += s.bytes;
        put_le32(o, crc32_of(bytes));
        put_le32(o, static_cast<std::uint32_t>(bytes.size()));
        return o;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("compressing");
    }
}

std::optional<std::string> gunzip(std::string_view gz, std::size_t limit, bool *over_limit)
{
    try {
        std::string out;
        std::size_t pos = 0;
        bool oom = false, over = false;
        do {
            if (!gunzip_member(gz, pos, out, limit, oom, over)) {
                if (oom) xpp::out_of_memory("decompressing");
                if (over_limit) *over_limit = over;
                return std::nullopt;
            }
            /* what follows the last member: nothing, or zeros some tools pad with */
            while (pos < gz.size() && gz[pos] == 0) pos++;
        } while (pos < gz.size());
        return out;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("decompressing");
    }
}

std::string make_zip(const std::vector<Entry> &entries)
{
    Sink s;
    mz_zip_archive z;
    mz_zip_zero_struct(&z);
    z.m_pWrite = put_at;
    z.m_pIO_opaque = &s;
    bool ok = mz_zip_writer_init(&z, 0);
    for (const Entry &e : entries)
        ok = ok && mz_zip_writer_add_mem(&z, e.name.c_str(), e.bytes.data(), e.bytes.size(), MZ_DEFAULT_LEVEL);
    ok = ok && mz_zip_writer_finalize_archive(&z);
    mz_zip_writer_end(&z);
    /* in memory, only an allocation can fail */
    if (!ok || s.out_of_memory) xpp::out_of_memory("building a zip archive");
    return std::move(s.bytes);
}

bool is_zip(std::string_view bytes) { return bytes.starts_with(std::string_view("PK\x03\x04", 4)); }

std::optional<std::vector<Entry>> read_zip(std::string_view zip)
{
    if (zip.size() > archive_bytes_limit) return std::nullopt;
    mz_zip_archive z;
    mz_zip_zero_struct(&z);
    if (!mz_zip_reader_init_mem(&z, zip.data(), zip.size(), 0)) return std::nullopt;
    std::optional<std::vector<Entry>> out;
    bool oom = false;
    try {
        std::vector<Entry> entries;
        const mz_uint n = mz_zip_reader_get_num_files(&z);
        bool ok = n <= archive_entries_limit;
        std::size_t total = 0;
        std::set<std::string> names;
        for (mz_uint i = 0; ok && i < n; i++) {
            if (mz_zip_reader_is_file_a_directory(&z, i)) continue;
            mz_zip_archive_file_stat stat;
            if (!mz_zip_reader_file_stat(&z, i, &stat) || stat.m_uncomp_size > archive_bytes_limit - total) {
                ok = false;
                break;
            }
            total += static_cast<std::size_t>(stat.m_uncomp_size);
            Entry e;
            e.name.resize(mz_zip_reader_get_filename(&z, i, nullptr, 0));
            mz_zip_reader_get_filename(&z, i, e.name.data(), static_cast<mz_uint>(e.name.size()));
            if (!e.name.empty()) e.name.pop_back(); /* its NUL */
            if (!names.insert(e.name).second) { ok = false; break; }
            Sink s;
            s.limit = static_cast<std::size_t>(stat.m_uncomp_size);
            ok = mz_zip_reader_extract_to_callback(&z, i, put_at, &s, 0);
            oom = oom || s.out_of_memory;
            e.bytes = std::move(s.bytes);
            entries.push_back(std::move(e));
        }
        if (ok) out = std::move(entries);
    } catch (const std::bad_alloc &) {
        oom = true;
    }
    mz_zip_reader_end(&z);
    if (oom) xpp::out_of_memory("reading a zip archive");
    return out;
}

} // namespace xpp::zip

namespace xpp::zip {

bool entry_name_ok(std::string_view name)
{
    if (name.empty() || name.size() > ENTRY_NAME_BYTES || name.front() == '/') return false;
    if (std::ranges::any_of(name, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7f || c == '\\' || c == ':'; })) return false;
    std::size_t at = 0;
    while (at <= name.size()) {
        const std::size_t slash = std::min(name.find('/', at), name.size());
        const std::string_view part = name.substr(at, slash - at);
        if (part.empty() || part == "." || part == "..") return false;
        at = slash + 1;
    }
    return true;
}

namespace {

/* the tar header's layout (POSIX ustar; GNU's header has the same fields) */
constexpr std::size_t TAR_BLOCK = 512;
constexpr std::size_t TAR_NAME_AT = 0, TAR_NAME_BYTES = 100, TAR_SIZE_AT = 124, TAR_SIZE_BYTES = 12, TAR_SUM_AT = 148, TAR_SUM_BYTES = 8,
                      TAR_TYPE_AT = 156, TAR_PREFIX_AT = 345, TAR_PREFIX_BYTES = 155;
/* a size of up to 11 octal digits (8 GiB) is all the format's plain field holds; larger ones use a binary form we refuse */
constexpr std::size_t TAR_OCTAL_DIGITS = 11;

/* the NUL-terminated text of a header's field */
std::string_view tar_text(std::string_view block, std::size_t at, std::size_t bytes)
{
    const std::string_view field = block.substr(at, bytes);
    return field.substr(0, field.find('\0'));
}

/* an octal number the field holds (digits, then spaces or NULs), or nothing */
std::optional<unsigned long long> tar_octal(std::string_view block, std::size_t at, std::size_t bytes)
{
    std::string_view field = block.substr(at, bytes);
    while (!field.empty() && (field.back() == ' ' || field.back() == '\0')) field.remove_suffix(1);
    while (!field.empty() && field.front() == ' ') field.remove_prefix(1);
    if (field.empty() || field.size() > TAR_OCTAL_DIGITS) return std::nullopt;
    unsigned long long n = 0;
    for (const char c : field) {
        if (c < '0' || c > '7') return std::nullopt;
        n = n * 8 + static_cast<unsigned>(c - '0');
    }
    return n;
}

bool tar_zero_block(std::string_view block) { return std::ranges::all_of(block, [](char c) { return c == 0; }); }

} // namespace

Result<std::vector<Entry>> read_tar(std::string_view tar, std::string_view file)
{
    const auto refuse = [&](std::string what) { return fail("tar archive", std::move(what), Place{std::string(file)}); };
    if (tar.size() > archive_bytes_limit) return refuse("the archive is larger than the limit");
    std::vector<Entry> entries;
    std::set<std::string> names;
    std::size_t at = 0, total = 0;
    while (true) {
        if (tar.size() - at < TAR_BLOCK) return refuse("the archive ends inside a header (no end marker)");
        const std::string_view block = tar.substr(at, TAR_BLOCK);
        at += TAR_BLOCK;
        if (tar_zero_block(block)) {
            if (!std::ranges::all_of(tar.substr(at), [](char c) { return c == 0; })) return refuse("data follows the end marker");
            return entries;
        }
        /* the checksum is the sum of the header's bytes with its own field read as spaces */
        unsigned long long sum = 0;
        for (std::size_t i = 0; i < TAR_BLOCK; i++)
            sum += (i >= TAR_SUM_AT && i < TAR_SUM_AT + TAR_SUM_BYTES) ? ' ' : static_cast<unsigned char>(block[i]);
        const auto want = tar_octal(block, TAR_SUM_AT, TAR_SUM_BYTES);
        if (!want || *want != sum) return refuse("a header's checksum is wrong");
        std::string name(tar_text(block, TAR_NAME_AT, TAR_NAME_BYTES));
        if (const std::string_view prefix = tar_text(block, TAR_PREFIX_AT, TAR_PREFIX_BYTES); !prefix.empty()) name = std::string(prefix) + "/" + name;
        const char type = block[TAR_TYPE_AT];
        const bool folder = type == '5';
        if (folder && name.ends_with('/')) name.pop_back();
        if (!entry_name_ok(name)) return refuse(xpp::format("the member name \"{}\" is not a plain relative path", name));
        if (type != '0' && type != '\0' && !folder)
            return refuse(xpp::format("\"{}\" is not a file or a folder (type {}): links, devices and extended headers are refused", name, static_cast<int>(type)));
        const auto size = tar_octal(block, TAR_SIZE_AT, TAR_SIZE_BYTES);
        if (!size) return refuse(xpp::format("the size of \"{}\" is not a plain octal number", name));
        if (folder) {
            if (*size != 0) return refuse(xpp::format("the folder \"{}\" has data", name));
            continue;
        }
        /* the bytes of the member, padded to a block, must be in the archive: a size larger than it says is refused here,
           before anything is allocated for it */
        if (*size > tar.size() - at || *size > archive_bytes_limit - total) return refuse(xpp::format("\"{}\" is larger than the archive holds or the limit", name));
        if (entries.size() >= archive_entries_limit) return refuse("the archive holds too many members");
        if (!names.insert(name).second) return refuse(xpp::format("\"{}\" is in the archive twice", name));
        total += static_cast<std::size_t>(*size);
        entries.push_back(Entry{std::move(name), std::string(tar.substr(at, static_cast<std::size_t>(*size)))});
        const std::size_t padded = (static_cast<std::size_t>(*size) + TAR_BLOCK - 1) / TAR_BLOCK * TAR_BLOCK;
        at += std::min(padded, tar.size() - at);
    }
}

Result<std::vector<Entry>> read_tar_gz(std::string_view gz, std::string_view file)
{
    bool over = false;
    const std::optional<std::string> tar = gunzip(gz, archive_bytes_limit, &over);
    if (!tar) return fail("tar archive", over ? "the archive expands past the limit" : "it is not a gzip file, or it is damaged", Place{std::string(file)});
    return read_tar(*tar, file);
}

} // namespace xpp::zip
