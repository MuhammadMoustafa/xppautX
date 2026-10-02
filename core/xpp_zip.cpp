/* Compression over the vendored miniz (xpp_zip.h). Every byte miniz hands
   back comes through a callback that appends it to a std::string, so no
   memory miniz allocates leaves it (core/xpp_mem.h); the callbacks never
   let an exception reach miniz's C code. */
#include "xpp_zip.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <set>
#include <limits>

#include "miniz.h"
#include "xpp_mem.h"

namespace xpp::zip {

namespace {

/* the output of a callback: the bytes, and whether one could not be kept */
struct Sink {
    std::string bytes;
    bool out_of_memory = false;
    std::size_t limit = std::numeric_limits<std::size_t>::max();
};

mz_bool put_deflated(const void *buf, int len, void *user)
{
    Sink &s = *static_cast<Sink *>(user);
    try {
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
   false when it is not one or is damaged */
bool gunzip_member(std::string_view gz, std::size_t &pos, std::string &out, bool &oom)
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
    std::size_t in = gz.size() - p;
    const int ok = tinfl_decompress_mem_to_callback(gz.data() + p, &in, put_inflated, &s, 0);
    if (s.out_of_memory) oom = true;
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

std::optional<std::string> gunzip(std::string_view gz)
{
    try {
        std::string out;
        std::size_t pos = 0;
        bool oom = false;
        do {
            if (!gunzip_member(gz, pos, out, oom)) {
                if (oom) xpp::out_of_memory("decompressing");
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
