/* xpp_io: the C++ API (xpp::format, xpp::number) and the file half -- a
   line reader, a token reader and a writer (W11 step 3, W7b). Checks
   xpp::format's std::format syntax, xpp::number's round-trip text, and
   the file half: long lines, no trailing newline, CRLF, an empty file, a
   failed open, and a writer's commit vs. abandon (abort() or an
   uncommitted xpp::Writer leave the original file untouched). The C
   text-formatting API this used to also check (xpp_snprintf/xpp_strlcpy/
   xpp_strlcat, XPP_SPRINTF/XPP_STRCPY/XPP_STRCAT, XPP_FORMAT_TO_BUF) was
   retired at W48, the C API under the handles at W109a: no core file
   called either any more. */
#include "xpptest.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace {

/* this run's private scratch folder (xpp::files::make_temp_dir, the core's own,
   so two runs never share one), removed with what is left in it at exit.
   Not std::filesystem: the test binaries link libstdc++ dynamically, and
   on Windows CI an older libstdc++-6.dll ahead on PATH lacks its symbols,
   so test_io.exe did not even load (exit 127). */
struct ScratchDir {
    std::string path;
    bool made;
    ScratchDir()
    {
        path = xpp::files::make_temp_dir();
        made = !path.empty();
        if (!made)
            path = ".";
    }
    /* at exit, with the path still alive (an atexit handler registered
       while this was being built ran after its destructor: ASan) */
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

/* a scratch file there, removed when it goes out of scope (the caller
   closes it first: Windows cannot remove an open file) */
class TempFile {
public:
    explicit TempFile(const std::string &name) : path_(scratch_dir() + "/" + name) {}
    const char *c_str() const { return path_.c_str(); }
    ~TempFile() { std::remove(path_.c_str()); }
private:
    std::string path_;
};

/* writes data (strlen(data) bytes) to path, no text-mode translation, for
   tests that need control over the raw bytes (a CRLF line, a file with
   no trailing newline, ...) */
void write_raw(const char *path, const char *data)
{
    size_t n = std::strlen(data);
    FILE *f = std::fopen(path, "wb");
    CHECK(f != NULL);
    if (f) {
        CHECK(std::fwrite(data, 1, n, f) == n);
        std::fclose(f);
    }
}

/* the line end a Writer writes: text mode, as the fopen "w" it
   replaced, so CR LF on Windows */
#ifdef _WIN32
#define TEXT_NL "\r\n"
#else
#define TEXT_NL "\n"
#endif

std::string read_raw(const char *path)
{
    std::string s;
    FILE *f = std::fopen(path, "rb");
    if (!f) return s;
    char buf[256];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    std::fclose(f);
    return s;
}

} // namespace

int main(void)
{
#ifdef XPP_IO_HAVE_STD_FORMAT
    /* xpp::format: unbounded, std::format syntax, compile-time checked */
    CHECK_STR(xpp::format("{} and {}", 1, "two").c_str(), "1 and two");
    CHECK_STR(xpp::format("{:.2f}", 3.14159).c_str(), "3.14");
#endif
#ifdef XPP_IO_HAVE_TO_CHARS
    /* xpp::number: shortest round-trip text, no trailing garbage digits */
    CHECK_STR(xpp::number(3.5).c_str(), "3.5");
    CHECK_STR(xpp::number(100.0).c_str(), "100");
#endif

    /* ---- the file half: line reader, token reader, writer (W11 step 3) */

    /* failed open: a path that cannot exist */
    CHECK(!xpp::LineReader("no/such/directory/file.txt"));
    CHECK(!xpp::TokenReader("no/such/directory/file.txt"));

    /* empty file: no lines at all */
    {
        TempFile tf("test_io_empty.tmp");
        write_raw(tf.c_str(), "");
        xpp::LineReader r(tf.c_str());
        CHECK(static_cast<bool>(r));
        CHECK(!r.next());
    }

    /* no trailing newline: the last, unterminated line still comes back
       once (not read twice, not dropped) */
    {
        TempFile tf("test_io_notrail.tmp");
        write_raw(tf.c_str(), "first\nsecond");
        xpp::LineReader r(tf.c_str());
        CHECK(static_cast<bool>(r));
        std::optional<std::string_view> l1 = r.next();
        CHECK(l1 && *l1 == "first");
        std::optional<std::string_view> l2 = r.next();
        CHECK(l2 && *l2 == "second" && l2->size() == 6);
        CHECK(!r.next());
    }

    /* CRLF tolerant: \r\n and a bare \n both end a line, no stray \r left */
    {
        TempFile tf("test_io_crlf.tmp");
        write_raw(tf.c_str(), "one\r\ntwo\nthree\r\n");
        xpp::LineReader r(tf.c_str());
        CHECK(static_cast<bool>(r));
        CHECK(r.next().value_or("") == "one");
        CHECK(r.next().value_or("") == "two");
        CHECK(r.next().value_or("") == "three");
        CHECK(!r.next());
    }

    /* a long line (well past any fixed fgets buffer this replaces): comes
       back whole, not cut */
    {
        TempFile tf("test_io_long.tmp");
        std::string big(5000, 'x');
        std::string content = big + "\nshort\n";
        write_raw(tf.c_str(), content.c_str());
        xpp::LineReader r(tf.c_str());
        CHECK(static_cast<bool>(r));
        std::optional<std::string_view> l1 = r.next();
        CHECK(l1 && *l1 == big);
        CHECK(r.next().value_or("") == "short");
    }

    /* attach: reads through a FILE* the caller keeps open and owns --
       closing the reader must not close it */
    {
        TempFile tf("test_io_attach.tmp");
        write_raw(tf.c_str(), "only line\n");
        FILE *fp = std::fopen(tf.c_str(), "r");
        CHECK(fp != NULL);
        {
            xpp::LineReader r = xpp::LineReader::attach(fp);
            CHECK(r.next().value_or("") == "only line");
        }
        CHECK(std::feof(fp) == 0 || std::fgetc(fp) == EOF); /* fp still usable */
        std::fclose(fp);
    }

    /* token reader: fscanf "%lg"/"%d" equivalents, whitespace separated,
       strtod on the same token agreeing with the double read; a token
       that is no number is no read */
    {
        TempFile tf("test_io_tok.tmp");
        write_raw(tf.c_str(), "  3.5 -7 hello   1e3\n");
        xpp::TokenReader r(tf.c_str());
        CHECK(static_cast<bool>(r));
        double d;
        int i;
        CHECK(r.read(d));
        CHECK(d == strtod("3.5", NULL));
        CHECK(r.read(i));
        CHECK(i == -7);
        CHECK(!r.read(d)); /* "hello" */
        CHECK(r.read(d));
        CHECK(d == 1e3);
        CHECK(!r.read(d)); /* end of file */
    }

    /* a float token reads as fscanf "%g" reads it (rounded straight to
       float), and the whitespace after a token stays in the stream */
    {
        TempFile tf("test_io_tok.tmp");
        write_raw(tf.c_str(), "0.1 16777217 2.5\nnext\n");
        std::FILE *fp = std::fopen(tf.c_str(), "r");
        CHECK(fp != NULL);
        float a, b, sa, sb;
        xpp::TokenReader r = xpp::TokenReader::attach(fp);
        CHECK(r.read(a));
        CHECK(r.read(b));
        CHECK(r.read(sa));
        CHECK(std::fgetc(fp) == '\n');
        r.close();
        std::rewind(fp);
        CHECK(std::fscanf(fp, "%g %g", &sa, &sb) == 2);
        CHECK(a == sa && b == sb);
        std::fclose(fp);
    }

    /* long fields: fscanf "%ld" field by field, AUTO's "%5ld" columns
       printed flush ("    2-1234" is 2 then -1234), the rest of the line
       skipped, the stream left where fscanf leaves it */
    {
        TempFile tf("test_io_tok.tmp");
        write_raw(tf.c_str(), "    2-1234  +7 x\n   1.5E+00 tail\nlast");
        std::FILE *fp = std::fopen(tf.c_str(), "r");
        CHECK(fp != NULL);
        long a = 0, b = 0, c = 0, d = 0;
        xpp::TokenReader tr = xpp::TokenReader::attach(fp);
        CHECK(tr.read(a) && a == 2);
        CHECK(tr.read(b) && b == -1234);
        CHECK(tr.read(c) && c == 7);
        CHECK(!tr.read(d));                /* "x" is no number, left in the stream */
        CHECK(std::fgetc(fp) == 'x');
        CHECK(tr.skip_line());             /* the "\n" after x */
        double x = 0;
        CHECK(tr.read(x) && x == 1.5);
        CHECK(tr.skip_line());             /* " tail\n" */
        CHECK(!tr.skip_line());            /* "last" ends at end of file */
        tr.close();
        std::rewind(fp);
        long s1, s2, s3;
        CHECK(std::fscanf(fp, "%ld%ld%ld", &s1, &s2, &s3) == 3);
        CHECK(s1 == a && s2 == b && s3 == c);
        std::fclose(fp);
    }

    /* the binary writer copies line ends as they are */
    {
        TempFile tf("test_io_write.tmp");
        xpp::Writer w = xpp::Writer::binary(tf.c_str());
        CHECK(static_cast<bool>(w));
        std::fputs("a\r\nb\n", w.file());
        CHECK(w.commit());
        std::FILE *fp = std::fopen(tf.c_str(), "rb");
        char buf[16] = {0};
        CHECK(fp != NULL && std::fread(buf, 1, sizeof buf - 1, fp) == 5);
        if (fp) std::fclose(fp);
        CHECK(std::strcmp(buf, "a\r\nb\n") == 0);
    }

    /* writer: commit renames the temp file into place, byte for byte */
    {
        TempFile tf("test_io_write.tmp");
        xpp::Writer w(tf.c_str());
        CHECK(static_cast<bool>(w));
        w.print("{} {}\n", 42, "answer");
        CHECK(w.commit());
        CHECK(read_raw(tf.c_str()) == "42 answer" TEXT_NL);
    }

    /* writer: abandoned (abort()) leaves an existing file completely
       untouched */
    {
        TempFile tf("test_io_write.tmp");
        write_raw(tf.c_str(), "original\n");
        xpp::Writer w(tf.c_str());
        CHECK(static_cast<bool>(w));
        w.print("clobber");
        w.abort();
        CHECK(read_raw(tf.c_str()) == "original\n");
    }

    /* writer: failed open (no such directory) */
    CHECK(!xpp::Writer("no/such/directory/file.txt"));

    /* the C++ RAII wrappers: LineReader over a long/CRLF file, and Writer
       whose destructor aborts (leaves the file untouched) when not
       committed, commits when it is */
    {
        TempFile tf("test_io_cpp.tmp");
        write_raw(tf.c_str(), "alpha\r\nbeta\n");
        xpp::LineReader lr(tf.c_str());
        CHECK(static_cast<bool>(lr));
        auto l1 = lr.next();
        CHECK(l1.has_value() && *l1 == "alpha");
        auto l2 = lr.next();
        CHECK(l2.has_value() && *l2 == "beta");
        CHECK(!lr.next().has_value());
    }
    {
        TempFile tf("test_io_cppw.tmp");
        write_raw(tf.c_str(), "kept\n");
        {
            xpp::Writer w(tf.c_str());
            CHECK(static_cast<bool>(w));
            std::fprintf(w.file(), "not committed");
            /* w destructed here without commit(): aborts */
        }
        CHECK(read_raw(tf.c_str()) == "kept\n");

        xpp::Writer w2(tf.c_str());
        std::fprintf(w2.file(), "replaced\n");
        CHECK(w2.commit());
        CHECK(read_raw(tf.c_str()) == "replaced" TEXT_NL);

        /* print (type-checked), xpp::print on a stream, and the read
           handle (W32b) */
        xpp::Writer w3(tf.c_str());
        w3.print("{} {:.3f}\n", "x", 1.5);
        xpp::print(w3.file(), "end\n");
        xpp::print(nullptr, "nowhere\n"); /* nothing written, no crash */
        CHECK(w3.commit());
        CHECK(read_raw(tf.c_str()) == "x 1.500" TEXT_NL "end" TEXT_NL);
        xpp::UniqueFile rf = xpp::open_read(tf.c_str());
        CHECK(rf != nullptr && std::fgetc(rf.get()) == 'x');
        rf.reset();
        CHECK(!xpp::open_read_binary("no-such-dir/no-such-file"));
    }

    /* xpp::json_encode_string/json_decode_string (card W35b): the one
       JSON string codec json_io.cpp's protocol lines and xpp_files.cpp's
       file names/listing both use, merged so a UTF-8 bug (the protocol
       used to treat bytes >=0x80 as Latin-1) is fixed in one place. */
    {
        /* encode: valid UTF-8 (2/3/4-byte) passes through unchanged */
        std::string out;
        xpp::json_encode_string(out, "I\xce\xb1pp"); /* "Iαpp" */
        CHECK_STR(out.c_str(), "I\xce\xb1pp");
        out.clear();
        xpp::json_encode_string(out, "\xe2\x82\xac"); /* U+20AC EURO SIGN */
        CHECK_STR(out.c_str(), "\xe2\x82\xac");
        out.clear();
        xpp::json_encode_string(out, "\xf0\x9f\x98\x80"); /* U+1F600, 4 bytes */
        CHECK_STR(out.c_str(), "\xf0\x9f\x98\x80");

        /* encode: '"', '\\' and control characters are escaped */
        out.clear();
        xpp::json_encode_string(out, "a\"b\\c\nd\te");
        CHECK_STR(out.c_str(), "a\\\"b\\\\c\\nd\\te");
        out.clear();
        xpp::json_encode_string(out, std::string(1, '\x01'));
        CHECK_STR(out.c_str(), "\\u0001");

        /* encode: a byte that is not part of valid UTF-8 (a lone
           continuation byte, an overlong/truncated sequence) is escaped
           as \u00XX -- its own byte value, Latin-1 style, since it is
           not decodable text at all */
        out.clear();
        xpp::json_encode_string(out, "\xff\xfe");
        CHECK_STR(out.c_str(), "\\u00ff\\u00fe");
        out.clear();
        xpp::json_encode_string(out, "\xc3"); /* truncated 2-byte sequence */
        CHECK_STR(out.c_str(), "\\u00c3");

        /* decode: escapes, including a surrogate pair, into UTF-8 */
        std::string in;
        CHECK(xpp::json_decode_string("\"I\\u03b1pp\"", in, static_cast<size_t>(-1), false));
        CHECK_STR(in.c_str(), "I\xce\xb1pp"); /* U+03B1 -> UTF-8 */
        CHECK(xpp::json_decode_string("\"\\ud83d\\ude00\"", in, static_cast<size_t>(-1), false));
        CHECK_STR(in.c_str(), "\xf0\x9f\x98\x80"); /* U+1F600 via its surrogate pair */
        CHECK(xpp::json_decode_string("\"a\\nb\\t\\\"\\\\\"", in, static_cast<size_t>(-1), false));
        CHECK_STR(in.c_str(), "a\nb\t\"\\");

        /* decode: raw UTF-8 bytes in the input pass through unchanged */
        CHECK(xpp::json_decode_string("\"caf\xc3\xa9\"", in, static_cast<size_t>(-1), false));
        CHECK_STR(in.c_str(), "caf\xc3\xa9");

        /* decode, strict (xpp_files' file-name rule): a control
           character, raw or via \u0000, and an unpaired surrogate are
           refused; a valid escape and a surrogate pair still work */
        CHECK(!xpp::json_decode_string("\"a\\u0000b\"", in, static_cast<size_t>(-1), true));
        CHECK(!xpp::json_decode_string(std::string("\"a\x01" "b\"").c_str(), in, static_cast<size_t>(-1), true));
        CHECK(!xpp::json_decode_string("\"\\ud83d\"", in, static_cast<size_t>(-1), true)); /* lone high */
        CHECK(!xpp::json_decode_string("\"\\ude00\"", in, static_cast<size_t>(-1), true)); /* lone low */
        CHECK(xpp::json_decode_string("\"I\\u03b1pp\"", in, static_cast<size_t>(-1), true));
        CHECK_STR(in.c_str(), "I\xce\xb1pp");
        CHECK(xpp::json_decode_string("\"\\ud83d\\ude00\"", in, static_cast<size_t>(-1), true));
        CHECK_STR(in.c_str(), "\xf0\x9f\x98\x80");

        /* decode: not a string at all */
        CHECK(!xpp::json_decode_string("42", in, static_cast<size_t>(-1), false));
        CHECK(!xpp::json_decode_string(NULL, in, static_cast<size_t>(-1), false));

        /* decode: a max byte cap truncates, same as the old js_string */
        CHECK(xpp::json_decode_string("\"hello\"", in, 4, false));
        CHECK_STR(in.c_str(), "hel");

        /* round trip: encode then decode gives the original text back */
        std::string enc;
        xpp::json_encode_string(enc, "I\xce\xb1pp \"quoted\"\n");
        std::string quoted = "\"" + enc + "\"";
        CHECK(xpp::json_decode_string(quoted.c_str(), in, static_cast<size_t>(-1), false));
        CHECK_STR(in.c_str(), "I\xce\xb1pp \"quoted\"\n");
    }

    /* parse_number: the whole text, one decimal number (the .odex reader) */
    {
        double v = 0;
        CHECK(xpp::parse_number("2.5e-3", v) && v == 2.5e-3);
        CHECK(xpp::parse_number("-4", v) && v == -4);
        CHECK(xpp::parse_number(".5", v) && v == 0.5);
        CHECK(xpp::parse_number(std::string_view("7xyz", 1), v) && v == 7); /* no NUL needed */
        v = 1;
        CHECK(!xpp::parse_number("", v));
        CHECK(!xpp::parse_number(" 1", v));
        CHECK(!xpp::parse_number("1 ", v));
        CHECK(!xpp::parse_number("1.5a", v));
        CHECK(!xpp::parse_number("0x10", v));
        CHECK(!xpp::parse_number("1e999", v));
        CHECK(!xpp::parse_number("1e-999", v)); /* underflows to 0 */
        CHECK(v == 1); /* untouched on failure */
        CHECK(xpp::parse_number("4.9406564584124654e-324", v) && v > 0 && v < 1e-320); /* a subnormal is a number */
        int k = 5;
        CHECK(xpp::parse_int("-42", k) && k == -42);
        CHECK(!xpp::parse_int("", k) && !xpp::parse_int("4.5", k) && !xpp::parse_int(" 1", k) && !xpp::parse_int("1x", k));
        CHECK(!xpp::parse_int("99999999999", k) && k == -42); /* out of range, untouched */
    }

    TEST_REPORT("test_io");
}
