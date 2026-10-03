/* The data formats (data_formats.h, W52) and the compression under them
   (xpp_zip.h): each registered format writes a table and reads it back;
   .dat byte for byte as XPP always wrote it, CSV's header and values,
   CSV.gz decompressed equal to the CSV, and NPZ's zip and .npy headers
   parsed here, independently of the reader. */
#include "xpptest.h"
#include "data_formats.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_zip.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

/* this run's private scratch folder, removed at exit (as test_io's) */
struct ScratchDir {
    std::string path;
    bool made;
    ScratchDir()
    {
        path = xpp::files::make_temp_dir();
        made = !path.empty();
        if (!made) path = ".";
    }
    ~ScratchDir()
    {
        if (made) xpp::files::remove_temp_dir(path.c_str());
    }
};

const std::string &scratch_dir()
{
    static const ScratchDir dir;
    return dir.path;
}

std::string scratch(const char *name) { return scratch_dir() + "/" + name; }

xpp::DataTable sample()
{
    xpp::DataTable t;
    t.names = {"T", "x", "y"};
    t.columns = {{0.0f, 0.1f, 0.2f, 1e-30f},
                 {1.0f, -3.5f, 123456.79f, 2.0f / 3.0f},
                 {100.0f, 0.333333343f, -7.25e8f, 1e30f}};
    return t;
}

/* table written as format f to path; true when it was, and committed */
bool write_as(const xpp::DataFormat &f, const xpp::DataTable &t, const std::string &path)
{
    xpp::Writer w = f.binary ? xpp::Writer::binary(path.c_str()) : xpp::Writer(path.c_str());
    return w && f.write(t, w) && w.commit();
}

std::string bytes_of(const std::string &path)
{
    std::string b;
    xpp::read_bytes(path.c_str(), b);
    std::string o;
    for (char c : b)
        if (c != '\r') o += c; /* a text file on Windows */
    return o;
}

bool same_values(const xpp::DataTable &a, const xpp::DataTable &b)
{
    if (a.columns.size() != b.columns.size()) return false;
    for (std::size_t j = 0; j < a.columns.size(); j++) {
        if (a.columns[j].size() != b.columns[j].size()) return false;
        for (std::size_t i=0; i<a.columns[j].size(); ++i)
            if (static_cast<float>(a.columns[j][i]) != static_cast<float>(b.columns[j][i])) return false;
    }
    return true;
}

unsigned le16(const std::string &s, std::size_t at)
{
    return static_cast<unsigned char>(s[at]) | static_cast<unsigned>(static_cast<unsigned char>(s[at + 1])) << 8;
}

std::uint32_t le32(const std::string &s, std::size_t at)
{
    return le16(s, at) | static_cast<std::uint32_t>(le16(s, at + 2)) << 16;
}

/* a zip's entries as its central directory lists them: name, compression
   method, and whether a local file header of that name is where it says */
struct LocalEntry {
    std::string name;
    unsigned method;
    bool local_ok;
};
std::vector<LocalEntry> local_entries(const std::string &zip)
{
    std::vector<LocalEntry> out;
    /* the end of central directory record, the last 22 bytes (no comment) */
    if (zip.size() < 22) return out;
    const std::size_t end = zip.size() - 22;
    if (le32(zip, end) != 0x06054b50) return out;
    const unsigned n = le16(zip, end + 10);
    std::size_t p = le32(zip, end + 16);
    for (unsigned i = 0; i < n && p + 46 <= zip.size() && le32(zip, p) == 0x02014b50; i++) {
        const unsigned method = le16(zip, p + 10);
        const unsigned nlen = le16(zip, p + 28), xlen = le16(zip, p + 30), clen = le16(zip, p + 32);
        const std::uint32_t local = le32(zip, p + 42);
        const std::string name = zip.substr(p + 46, nlen);
        const bool ok = local + 30 <= zip.size() && le32(zip, local) == 0x04034b50 &&
                        zip.compare(local + 30, le16(zip, local + 26), name) == 0;
        out.push_back({name, method, ok});
        p += 46 + nlen + xlen + clen;
    }
    return out;
}

/* a .npy's header dict, its data offset, or "" when it is not one */
std::string npy_header(const std::string &npy, std::size_t &data_at)
{
    if (npy.size() < 10 || npy.compare(0, 6, "\x93NUMPY") != 0 || npy[6] != 1) return "";
    const unsigned hlen = le16(npy, 8);
    data_at = 10 + hlen;
    return npy.substr(10, hlen);
}

std::vector<double> npy_doubles(const std::string &npy, std::size_t at)
{
    std::vector<double> v((npy.size() - at) / 8);
    if (!v.empty()) std::memcpy(v.data(), npy.data() + at, v.size() * 8);
    return v;
}

} // namespace

int main(void)
{
    /* ---- the registry */
    CHECK(xpp::data_formats().size() == 4);
    CHECK(xpp::data_format_named("CSV.GZ") && std::strcmp(xpp::data_format_named("csv.gz")->extension, ".csv.gz") == 0);
    CHECK(xpp::data_format_of_file("run.CSV.gz") == xpp::data_format_named("csv.gz"));
    CHECK(xpp::data_format_of_file("run.csv") == xpp::data_format_named("csv"));
    CHECK(xpp::data_format_of_file("run.npz") == xpp::data_format_named("npz"));
    CHECK(xpp::data_format_of_file("run.dat") == xpp::data_format_named("dat"));
    CHECK(xpp::data_format_of_file("run.txt") == nullptr);
    CHECK(xpp::data_format_of_file(".csv") == nullptr);
    for (const xpp::DataFormat &f : xpp::data_formats()) CHECK(f.write != nullptr && f.read != nullptr);

    const xpp::DataTable t = sample();

    /* ---- .dat: "%.8g " per value, a row per line, no names */
    {
        const xpp::DataFormat &f = *xpp::data_format_named("dat");
        const std::string p = scratch("t.dat");
        CHECK(write_as(f, t, p));
        const std::string text = bytes_of(p);
        CHECK_STR(text.substr(0, text.find('\n') + 1).c_str(), "0 1 100 \n");
        CHECK(text.find("\n1e-30 0.66666669 1e+30 \n") != std::string::npos);
        xpp::DataTable r;
        CHECK(f.read(p.c_str(), r));
        CHECK(r.columns.size() == 3 && r.rows() == 4 && r.names.empty());
        CHECK(r.rows() == 4 && r.columns[1][2] == 123456.79 && r.columns[0][1] == 0.1);
        CHECK_STR(xpp::data_column_name(r, 2).c_str(), "col3");
    }

    /* ---- CSV: a header of the names, then each float's shortest text */
    const std::string csv_path = scratch("t.csv");
    {
        const xpp::DataFormat &f = *xpp::data_format_named("csv");
        CHECK(write_as(f, t, csv_path));
        const std::string text = bytes_of(csv_path);
        CHECK_STR(text.substr(0, text.find('\n') + 1).c_str(), "T,x,y\n");
        CHECK(text.find("\n0.1,-3.5,0.33333334\n") != std::string::npos);
        xpp::DataTable r;
        CHECK(f.read(csv_path.c_str(), r));
        CHECK(r.names == t.names);
        CHECK(same_values(r, t));

        /* someone else's CSV: CRLF, a quoted header, blank lines, an empty field */
        const std::string p = scratch("other.csv");
        {
            xpp::Writer w = xpp::Writer::binary(p.c_str());
            CHECK(w.write("\"a, b\",c\r\n1,2\r\n\r\n3,\r\n") && w.commit());
        }
        CHECK(f.read(p.c_str(), r));
        CHECK(r.names.size() == 2 && r.names[0] == "a, b" && r.rows() == 2);
        CHECK(r.rows() == 2 && r.columns[0][1] == 3.0f && std::isnan(r.columns[1][1]));
        /* no header: every line numbers */
        {
            xpp::Writer w = xpp::Writer::binary(p.c_str());
            CHECK(w.write("1,2\n3,4\n") && w.commit());
        }
        CHECK(f.read(p.c_str(), r) && r.names.empty() && r.rows() == 2);
        /* text after the header: not a table */
        {
            xpp::Writer w = xpp::Writer::binary(p.c_str());
            CHECK(w.write("a,b\n1,2\nx,y\n") && w.commit());
        }
        CHECK(!f.read(p.c_str(), r) && r.columns.empty());
        CHECK(!f.read(scratch("missing.csv").c_str(), r));
        {
            xpp::Writer w = xpp::Writer::binary(p);
            CHECK(w.write("# seed 7\na,b\n1,2\n3\n") && w.commit());
        }
        CHECK(!f.read(p.c_str(), r) && r.columns.empty() && r.error_line == 4);
        const auto bad = xpp::read_data_table(p.c_str());
        CHECK(!bad && bad.error().place.file == p && bad.error().place.line == 4);
        {
            xpp::Writer w = xpp::Writer::binary(p);
            CHECK(w.write("\"unclosed,b\n1,2\n") && w.commit());
        }
        CHECK(!f.read(p.c_str(), r) && r.error_line == 1);
    }

    {
        const std::string p = scratch("ragged.dat");
        xpp::Writer w = xpp::Writer::binary(p);
        CHECK(w.write("1 2\n3\n") && w.commit());
        xpp::DataTable r;
        CHECK(!xpp::data_format_named("dat")->read(p.c_str(),r) && r.columns.empty() && r.error_line == 2);
    }

    /* Text readers preserve doubles; storage writers retain their exact bytes. */
    for (const char *id : {"dat", "csv"}) {
        const auto &format=*xpp::data_format_named(id);
        const std::string p=scratch((std::string("precise")+format.extension).c_str());
        xpp::Writer w=xpp::Writer::binary(p);
        CHECK(w.write("1.0000000001\n") && w.commit());
        xpp::DataTable r;
        CHECK(format.read(p.c_str(),r));
        CHECK(r.columns.size()==1 && r.rows()==1 && r.columns[0][0]==1.0000000001);
        CHECK(!r.stored_floats);
        const std::string csv=scratch("precise-output.csv");
        CHECK(write_as(*xpp::data_format_named("csv"),r,csv));
        CHECK(bytes_of(csv)=="col1\n1.0000000001\n");
    }

    /* AUTO's mixed CSV keeps doubles exact and quotes headers and cells once. */
    {
        xpp::DataTable mixed;
        mixed.names = {"parameter, name", "value"};
        mixed.fields = {{"a\"b,c"}, {xpp::number(1.0000000000000002)}};
        const std::string path = scratch("mixed.csv");
        CHECK(write_as(*xpp::data_format_named("csv"), mixed, path));
        CHECK(bytes_of(path) == "\"parameter, name\",value\n\"a\"\"b,c\",1.0000000000000002\n");
        CHECK(!write_as(*xpp::data_format_named("dat"), mixed, scratch("mixed.dat")));
        mixed.fields[1].clear();
        CHECK(!write_as(*xpp::data_format_named("csv"), mixed, path));
    }

    /* ---- CSV.gz: the CSV, gzipped */
    {
        const xpp::DataFormat &f = *xpp::data_format_named("csv.gz");
        const std::string p = scratch("t.csv.gz");
        CHECK(write_as(f, t, p));
        std::string gz;
        CHECK(xpp::read_bytes(p.c_str(), gz));
        CHECK(gz.size() > 18 && static_cast<unsigned char>(gz[0]) == 0x1f && static_cast<unsigned char>(gz[1]) == 0x8b);
        const std::optional<std::string> text = xpp::zip::gunzip(gz);
        CHECK(text && *text == bytes_of(csv_path));
        xpp::DataTable r;
        CHECK(f.read(p.c_str(), r));
        CHECK(r.names == t.names && same_values(r, t));
        /* a damaged file */
        std::string bad = gz;
        bad[bad.size() - 6] ^= 0x55; /* its CRC */
        CHECK(!xpp::zip::gunzip(bad));
        CHECK(!xpp::zip::gunzip("not gzip at all, not at all"));
        /* two members, one after the other */
        CHECK(xpp::zip::gunzip(xpp::zip::gzip("ab") + xpp::zip::gzip("cd")) == std::string("abcd"));
        CHECK(xpp::zip::gunzip(xpp::zip::gzip("")) == std::string());
    }

    /* ---- NPZ: a zip of .npy files, one float64 array per column */
    {
        const xpp::DataFormat &f = *xpp::data_format_named("npz");
        const std::string p = scratch("t.npz");
        CHECK(write_as(f, t, p));
        std::string zip;
        CHECK(xpp::read_bytes(p.c_str(), zip));
        const std::vector<LocalEntry> local = local_entries(zip);
        CHECK(local.size() == 3);
        if (local.size() == 3) {
            CHECK_STR(local[0].name.c_str(), "T.npy");
            CHECK_STR(local[2].name.c_str(), "y.npy");
            CHECK(local[0].method == 8); /* deflated */
            CHECK(local[0].local_ok && local[1].local_ok && local[2].local_ok);
        }
        const std::optional<std::vector<xpp::zip::Entry>> entries = xpp::zip::read_zip(zip);
        CHECK(entries && entries->size() == 3);
        if (entries && entries->size() == 3) {
            const std::string &x = (*entries)[1].bytes;
            std::size_t at = 0;
            const std::string h = npy_header(x, at);
            CHECK(h.find("'descr': '<f8'") != std::string::npos);
            CHECK(h.find("'fortran_order': False") != std::string::npos);
            CHECK(h.find("'shape': (4,)") != std::string::npos);
            CHECK(at % 64 == 0 && !h.empty() && h.back() == '\n');
            const std::vector<double> v = npy_doubles(x, at);
            CHECK(v.size() == 4 && v[2] == static_cast<double>(123456.79f) && v[3] == static_cast<double>(2.0f / 3.0f));
        }
        xpp::DataTable r;
        CHECK(f.read(p.c_str(), r));
        CHECK(r.names == t.names && same_values(r, t));

        /* what the plot shows: one (points x 2) array per curve */
        xpp::DataTable c;
        c.curves = true;
        c.names = {"curve", "x", "y"};
        c.columns = {{1, 1, 2}, {0.5f, 1.5f, 9}, {-1, -2, 8}};
        const std::string pc = scratch("c.npz");
        CHECK(write_as(f, c, pc));
        CHECK(xpp::read_bytes(pc.c_str(), zip));
        const std::optional<std::vector<xpp::zip::Entry>> ce = xpp::zip::read_zip(zip);
        CHECK(ce && ce->size() == 2);
        if (ce && ce->size() == 2) {
            CHECK_STR((*ce)[0].name.c_str(), "curve1.npy");
            CHECK_STR((*ce)[1].name.c_str(), "curve2.npy");
            std::size_t at = 0;
            const std::string h = npy_header((*ce)[0].bytes, at);
            CHECK(h.find("'shape': (2, 2)") != std::string::npos);
            const std::vector<double> v = npy_doubles((*ce)[0].bytes, at);
            CHECK(v == std::vector<double>({0.5, -1, 1.5, -2}));
        }
        CHECK(f.read(pc.c_str(), r));
        CHECK(r.names.size() == 4 && r.names[0] == "curve1_0" && r.rows() == 2);
        CHECK(r.rows() == 2 && r.columns[2][0] == 9.0f && std::isnan(r.columns[2][1]));
        CHECK(!f.read(csv_path.c_str(), r));
    }

    /* a zip of entries, read back in order */
    {
        const std::string z = xpp::zip::make_zip({{"a.txt", "alpha"}, {"b/c.txt", std::string(10000, 'x')}});
        const std::optional<std::vector<xpp::zip::Entry>> e = xpp::zip::read_zip(z);
        CHECK(e && e->size() == 2 && (*e)[0].name == "a.txt" && (*e)[0].bytes == "alpha" &&
              (*e)[1].bytes == std::string(10000, 'x'));
        CHECK(z.size() < 1000); /* deflated */
        CHECK(!xpp::zip::read_zip("PK but not a zip"));
        CHECK(!xpp::zip::read_zip(xpp::zip::make_zip({{"same", "a"}, {"same", "b"}})));
        std::vector<xpp::zip::Entry> many;
        for (std::size_t i = 0; i <= xpp::zip::archive_entries_limit; i++) many.push_back({std::to_string(i), ""});
        CHECK(!xpp::zip::read_zip(xpp::zip::make_zip(many)));
        std::string oversized = z;
        const std::size_t central = le32(z, z.size() - 6);
        const std::uint32_t declared = xpp::zip::archive_bytes_limit + 1;
        for (int i = 0; i < 4; i++) oversized[central + 24 + i] = static_cast<char>(declared >> (8 * i));
        CHECK(!xpp::zip::read_zip(oversized));
    }

    TEST_REPORT("test_data_formats");
}
