/* The data file formats and their registry (data_formats.h, W52).

   .dat is XPP's own: each row's values in "%.8g" followed by a blank, one
   row per line, no names -- byte for byte what XPP always wrote (the
   batch run's output.dat and the browser's Write), read back field by
   field with the first line's width. CSV has a header row of the column
   names and each stored float in its shortest text that reads back as
   the same float. CSV.gz is that CSV, gzipped. NPZ is numpy.savez's
   format: a zip of .npy files, one float64 array per column named after
   it, or for what the plot shows one (points x 2 or 3) array per curve,
   curve1, curve2, ... */
#include "data_formats.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <optional>

#include "xpp_mem.h"
#include "xpp_zip.h"

namespace xpp {

namespace {

static_assert(std::endian::native == std::endian::little, "the .npy writer and reader assume little-endian");

constexpr float nan_value = std::numeric_limits<float>::quiet_NaN();

/* text written a chunk at a time: a table's text is built up to this size
   before it goes to the file */
constexpr std::size_t chunk = 1 << 16;

/* ---- .dat ---- */

bool write_dat(const DataTable &t, Writer &w)
{
    std::string text;
    const std::size_t n = t.rows();
    for (std::size_t i = 0; i < n; i++) {
        for (const std::vector<float> &c : t.columns) format_append(text, "{:.8g} ", static_cast<double>(c[i]));
        text += '\n';
        if (text.size() >= chunk) {
            if (!w.write(text)) return false;
            text.clear();
        }
    }
    return w.write(text);
}

bool read_dat(const char *path, DataTable &t)
{
    std::size_t count = 0;
    {
        LineReader lr(path);
        if (!lr) return false;
        /* the width: the whitespace-separated fields of the first line */
        if (std::optional<std::string_view> line = lr.next()) {
            bool white = true;
            for (unsigned char c : *line) {
                const bool space = std::isspace(c) != 0;
                if (!space && white) ++count;
                white = space;
            }
        }
    }
    t.columns.assign(count, {});
    if (!count) return true;
    TokenReader tr(path);
    std::vector<float> row(count);
    for (;;) {
        for (float &z : row)
            if (!tr.read(z)) return true; /* a last row cut short is left out */
        for (std::size_t k = 0; k < count; k++) t.columns[k].push_back(row[k]);
    }
}

/* ---- CSV ---- */

/* a header field, quoted when it has to be (RFC 4180) */
void append_csv_field(std::string &o, std::string_view s)
{
    if (s.find_first_of(",\"\r\n") == std::string_view::npos) {
        o += s;
        return;
    }
    o += '"';
    for (char c : s) {
        if (c == '"') o += '"';
        o += c;
    }
    o += '"';
}

std::string csv_text(const DataTable &t)
{
    std::string o;
    if (t.seed) format_append(o, "# seed {}\n", *t.seed);
    for (std::size_t j = 0; j < t.columns.size(); j++) {
        if (j) o += ',';
        append_csv_field(o, data_column_name(t, j));
    }
    o += '\n';
    const std::size_t n = t.rows();
    for (std::size_t i = 0; i < n; i++) {
        for (std::size_t j = 0; j < t.columns.size(); j++) {
            if (j) o += ',';
            format_append(o, "{}", t.columns[j][i]);
        }
        o += '\n';
    }
    return o;
}

/* one line's fields, a quoted field unquoted */
std::vector<std::string> csv_fields(std::string_view line)
{
    std::vector<std::string> f(1);
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); i++) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') f.back() += line[++i];
            else if (c == '"') quoted = false;
            else f.back() += c;
        } else if (c == '"') quoted = true;
        else if (c == ',') f.emplace_back();
        else f.back() += c;
    }
    return f;
}

/* a field as a float; false when it is not a number (blanks around it
   allowed, like strtof's leading ones) */
bool csv_number(const std::string &s, float &v)
{
    const char *b = s.c_str();
    char *end = nullptr;
    v = std::strtof(b, &end);
    if (end == b) return false;
    while (*end == ' ' || *end == '\t') end++;
    return *end == 0;
}

bool parse_csv(std::string_view text, DataTable &t)
{
    bool first = true;
    std::size_t width = 0;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view line = text.substr(0, nl);
        text = nl == std::string_view::npos ? std::string_view() : text.substr(nl + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.find_first_not_of(" \t") == std::string_view::npos) continue;
        if (line.starts_with("# seed ")) {
            const std::string digits(line.substr(7));
            char *end = nullptr;
            const long parsed = std::strtol(digits.c_str(), &end, 10);
            if (end != digits.c_str()) t.seed = static_cast<int>(parsed);
            continue;
        }
        if (line.starts_with("#")) continue; /* any other comment: ignored */
        std::vector<std::string> f = csv_fields(line);
        std::vector<float> v(f.size());
        bool numbers = true;
        for (std::size_t j = 0; j < f.size(); j++)
            if (f[j].find_first_not_of(" \t") == std::string::npos) v[j] = nan_value; /* empty: no value */
            else if (!csv_number(f[j], v[j])) numbers = false;
        if (first) {
            width = f.size();
            t.columns.assign(width, {});
            first = false;
            if (!numbers) {
                t.names = std::move(f);
                continue;
            }
        } else if (!numbers) {
            t = DataTable();
            return false; /* a second line of text: not a table of numbers */
        }
        for (std::size_t j = 0; j < width; j++) t.columns[j].push_back(j < v.size() ? v[j] : nan_value);
    }
    return true;
}

bool write_csv(const DataTable &t, Writer &w) { return w.write(csv_text(t)); }

bool read_csv(const char *path, DataTable &t)
{
    std::string text;
    return read_bytes(path, text) && parse_csv(text, t);
}

bool write_csv_gz(const DataTable &t, Writer &w) { return w.write(zip::gzip(csv_text(t))); }

bool read_csv_gz(const char *path, DataTable &t)
{
    std::string gz;
    if (!read_bytes(path, gz)) return false;
    const std::optional<std::string> text = zip::gunzip(gz);
    return text && parse_csv(*text, t);
}

/* ---- NPZ ---- */

/* a .npy file of float64 values in C order, of shape (rows,) or (rows, cols) */
std::string npy(std::span<const double> values, std::size_t rows, std::size_t cols)
{
    std::string header = "{'descr': '<f8', 'fortran_order': False, 'shape': (" + std::to_string(rows) +
                         (cols ? ", " + std::to_string(cols) + "), }" : ",), }");
    /* padded with blanks and a newline so the data starts on a multiple of
       64 bytes, numpy's own alignment */
    const std::size_t total = (10 + header.size() + 1 + 63) / 64 * 64;
    header.append(total - 10 - header.size() - 1, ' ');
    header += '\n';
    std::string o("\x93NUMPY\x01\x00", 8);
    o += static_cast<char>(header.size() & 0xff);
    o += static_cast<char>(header.size() >> 8);
    o += header;
    const std::size_t at = o.size();
    o.resize(at + values.size() * sizeof(double));
    if (!values.empty()) std::memcpy(o.data() + at, values.data(), values.size() * sizeof(double));
    return o;
}

std::string npz_of(const DataTable &t)
{
    std::vector<zip::Entry> entries;
    const std::size_t n = t.rows();
    if (!t.curves) {
        std::vector<double> v;
        for (std::size_t j = 0; j < t.columns.size(); j++) {
            v.assign(t.columns[j].begin(), t.columns[j].end());
            entries.push_back({data_column_name(t, j) + ".npy", npy(v, n, 0)});
        }
    } else if (!t.columns.empty()) {
        /* one array per curve, the curves in the order they first appear */
        std::vector<float> ids;
        for (float id : t.columns[0])
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        const std::size_t m = t.columns.size() - 1;
        for (float id : ids) {
            std::vector<double> v;
            std::size_t k = 0;
            for (std::size_t i = 0; i < n; i++)
                if (t.columns[0][i] == id) {
                    for (std::size_t j = 1; j <= m; j++) v.push_back(t.columns[j][i]);
                    k++;
                }
            entries.push_back({xpp::format("{}{}.npy", data_column_name(t, 0), id), npy(v, k, m)});
        }
    }
    if (t.seed) {
        const double seed_value = *t.seed;
        entries.push_back({"seed.npy", npy(std::span<const double>(&seed_value, 1), 1, 0)});
    }
    return zip::make_zip(entries);
}

bool write_npz(const DataTable &t, Writer &w) { return w.write(npz_of(t)); }

/* the value after "'key':" in a .npy header's dict, up to the next ',' or
   '}' outside parentheses */
std::string_view npy_field(std::string_view h, std::string_view key)
{
    const std::size_t k = h.find("'" + std::string(key) + "'");
    if (k == std::string_view::npos) return {};
    std::size_t p = h.find(':', k);
    if (p == std::string_view::npos) return {};
    p++;
    while (p < h.size() && h[p] == ' ') p++;
    std::size_t e = p;
    int depth = 0;
    while (e < h.size() && (depth || (h[e] != ',' && h[e] != '}'))) {
        if (h[e] == '(') depth++;
        if (h[e] == ')') depth--;
        e++;
    }
    return h.substr(p, e - p);
}

/* an array of a .npz: its columns (one for a 1-D array, cols for a 2-D
   one), appended to t; false when it is not a .npy numpy can write that
   this reads (float64, float32, int32, int64; at most 2-D) */
bool read_npy(std::string_view name, std::string_view b, DataTable &t)
{
    if (b.size() < 10 || b.substr(0, 6) != std::string_view("\x93NUMPY", 6)) return false;
    const unsigned major = static_cast<unsigned char>(b[6]);
    std::size_t hlen, at;
    const auto byte = [&](std::size_t i) { return static_cast<std::size_t>(static_cast<unsigned char>(b[i])); };
    if (major == 1) {
        hlen = byte(8) | byte(9) << 8;
        at = 10;
    } else {
        if (b.size() < 12) return false;
        hlen = byte(8) | byte(9) << 8 | byte(10) << 16 | byte(11) << 24;
        at = 12;
    }
    if (b.size() - at < hlen) return false;
    const std::string_view h = b.substr(at, hlen);
    const std::string_view data = b.substr(at + hlen);
    std::string_view descr = npy_field(h, "descr");
    if (descr.size() < 2) return false;
    descr = descr.substr(1, descr.size() - 2); /* its quotes */
    if (descr.size() == 3 && (descr[0] == '<' || descr[0] == '=' || descr[0] == '|')) descr.remove_prefix(1);
    std::size_t size;
    if (descr == "f8" || descr == "i8") size = 8;
    else if (descr == "f4" || descr == "i4") size = 4;
    else return false;
    const bool fortran = npy_field(h, "fortran_order") == "True";
    /* the shape: (), (n,) or (n, m) */
    std::vector<std::size_t> shape;
    std::string_view s = npy_field(h, "shape");
    if (s.size() < 2 || s.front() != '(' || s.back() != ')') return false;
    s = s.substr(1, s.size() - 2);
    while (!s.empty()) {
        const std::size_t c = s.find(',');
        std::string_view d = s.substr(0, c);
        while (!d.empty() && d.front() == ' ') d.remove_prefix(1);
        while (!d.empty() && d.back() == ' ') d.remove_suffix(1);
        if (!d.empty()) {
            std::size_t v = 0;
            for (char ch : d) {
                if (ch < '0' || ch > '9') return false;
                v = v * 10 + static_cast<std::size_t>(ch - '0');
            }
            shape.push_back(v);
        }
        s = c == std::string_view::npos ? std::string_view() : s.substr(c + 1);
    }
    if (shape.size() > 2) return false;
    const std::size_t rows = shape.empty() ? 1 : shape[0], cols = shape.size() == 2 ? shape[1] : 1;
    if (cols && rows > data.size() / size / cols) return false;
    const auto value = [&](std::size_t k) -> float {
        const char *p = data.data() + k * size;
        if (descr == "f8") {
            double d;
            std::memcpy(&d, p, 8);
            return static_cast<float>(d);
        }
        if (descr == "f4") {
            float f;
            std::memcpy(&f, p, 4);
            return f;
        }
        if (descr == "i8") {
            std::int64_t i;
            std::memcpy(&i, p, 8);
            return static_cast<float>(i);
        }
        std::int32_t i;
        std::memcpy(&i, p, 4);
        return static_cast<float>(i);
    };
    std::string base(name);
    if (base.size() > 4 && base.ends_with(".npy")) base.resize(base.size() - 4);
    for (std::size_t j = 0; j < cols; j++) {
        std::vector<float> c(rows);
        for (std::size_t i = 0; i < rows; i++) c[i] = value(fortran ? j * rows + i : i * cols + j);
        t.names.push_back(shape.size() == 2 ? xpp::format("{}_{}", base, j) : base);
        t.columns.push_back(std::move(c));
    }
    return true;
}

bool parse_npz(std::string_view bytes, DataTable &t)
{
    const std::optional<std::vector<zip::Entry>> entries = zip::read_zip(bytes);
    if (!entries) return false;
    for (const zip::Entry &e : *entries) {
        if (e.name == "seed.npy") {
            DataTable one;
            if (!read_npy(e.name, e.bytes, one) || one.columns.size() != 1 || one.columns[0].empty()) {
                t = DataTable();
                return false;
            }
            t.seed = static_cast<int>(one.columns[0][0]);
            continue;
        }
        if (!read_npy(e.name, e.bytes, t)) {
            t = DataTable();
            return false;
        }
    }
    /* arrays of different lengths: the shorter ones end in no value */
    std::size_t n = 0;
    for (const std::vector<float> &c : t.columns) n = std::max(n, c.size());
    for (std::vector<float> &c : t.columns) c.resize(n, nan_value);
    return true;
}

bool read_npz(const char *path, DataTable &t)
{
    std::string bytes;
    return read_bytes(path, bytes) && parse_npz(bytes, t);
}

/* F(args...), ending the program on a failed allocation (no exception
   leaves the core's formats) */
template <auto F, class... A>
auto guarded(const char *what, A... args) noexcept
{
    try {
        return F(args...);
    } catch (const std::bad_alloc &) {
        xpp_out_of_memory(what);
    }
}

/* a reader F of src into t: t empty unless it read */
template <auto F, class Src>
bool read_clean(Src src, DataTable &t)
{
    t = DataTable();
    if (F(src, t)) return true;
    t = DataTable();
    return false;
}

/* a format function as the registry holds it */
template <auto F>
bool guarded_write(const DataTable &t, Writer &w) noexcept
{
    return guarded<F, const DataTable &, Writer &>("writing a data file", t, w);
}

template <auto F>
bool guarded_read(const char *path, DataTable &t) noexcept
{
    return guarded<read_clean<F, const char *>, const char *, DataTable &>("reading a data file", path, t);
}

/* The registry: one line per format, in the Save data menu's order. */
constexpr std::array<DataFormat, 4> registry{{
    {"dat", "XPP data (.dat)", ".dat", 'd', false, guarded_write<write_dat>, guarded_read<read_dat>},
    {"csv", "CSV with a header (.csv)", ".csv", 'c', true, guarded_write<write_csv>, guarded_read<read_csv>},
    {"csv.gz", "Compressed CSV (.csv.gz)", ".csv.gz", 'g', true, guarded_write<write_csv_gz>, guarded_read<read_csv_gz>},
    {"npz", "NumPy arrays (.npz)", ".npz", 'n', true, guarded_write<write_npz>, guarded_read<read_npz>},
}};

} // namespace

std::string data_column_name(const DataTable &t, std::size_t i)
{
    if (i < t.names.size() && !t.names[i].empty()) return t.names[i];
    return "col" + std::to_string(i + 1);
}

std::span<const DataFormat> data_formats() { return registry; }

std::string npz_bytes(const DataTable &table) noexcept
{
    return guarded<npz_of, const DataTable &>("writing a data file", table);
}

bool npz_table(std::string_view bytes, DataTable &table) noexcept
{
    return guarded<read_clean<parse_npz, std::string_view>, std::string_view, DataTable &>("reading a data file",
                                                                                        bytes, table);
}

const DataFormat *data_format_named(std::string_view id)
{
    for (const DataFormat &f : registry)
        if (equal_ignoring_case(f.id, id)) return &f;
    return nullptr;
}

const DataFormat *data_format_of_file(std::string_view path)
{
    for (const DataFormat &f : registry) {
        const std::string_view e = f.extension;
        if (path.size() > e.size() && equal_ignoring_case(path.substr(path.size() - e.size()), e)) return &f;
    }
    return nullptr;
}

} // namespace xpp
