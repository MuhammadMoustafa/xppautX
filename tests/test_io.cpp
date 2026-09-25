/* xpp_io: the safe formatting/copying the whole core uses instead of
   sprintf/strcpy into fixed buffers (issue: W11 step 2), and its file
   half -- a line reader, a token reader and a writer (W11 step 3, W7b).
   Checks truncation, NUL termination, the returned lengths, XPP_SPRINTF/
   XPP_STRCPY/XPP_STRCAT taking sizeof(dst) automatically, that a
   truncating call logs exactly one WARN (redirecting stderr, since
   xpp_log's default threshold already prints WARN and up), the C++ API
   (xpp::format, XPP_FORMAT_TO_BUF, xpp::number), and the file half: long
   lines, no trailing newline, CRLF, an empty file, a failed open, and a
   writer's commit vs. abandon (xpp_writer_abort/an uncommitted
   xpp::Writer leave the original file untouched). */
#include "xpptest.h"
#include "xpp_io.h"
#include "xpp_log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace {

/* a scratch file in the system's temp folder, named after this process so
   two test runs never share one, removed when it goes out of scope (the
   caller closes it first: Windows cannot remove an open file) */
class TempFile {
public:
    explicit TempFile(const std::string &name)
        : path_(std::filesystem::temp_directory_path() / (std::to_string(getpid()) + "_" + name)),
          path_str_(path_.string())
    {
    }
    const std::filesystem::path &path() const { return path_; }
    const char *c_str() const { return path_str_.c_str(); }
    ~TempFile()
    {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
private:
    std::filesystem::path path_;
    std::string path_str_;
};

/* Captures everything written to stderr between begin() and end() by
   redirecting the process's stderr to a scratch file and reading it
   back; good enough for one test binary that exits right after. */
class StderrCapture {
public:
    StderrCapture() : tempfile_("test_io_stderr.tmp"), saved_stderr_fd_(-1) {}

    void begin()
    {
        std::fflush(stderr);
        // Save the current stderr file descriptor
        saved_stderr_fd_ = dup(fileno(stderr));
        CHECK(saved_stderr_fd_ >= 0);
        // Redirect stderr to the temp file
        FILE *f = freopen(tempfile_.c_str(), "w", stderr);
        CHECK(f != NULL);
    }
    std::string end()
    {
        std::fflush(stderr);
        // Restore stderr from saved file descriptor
        if (saved_stderr_fd_ >= 0) {
            dup2(saved_stderr_fd_, fileno(stderr));
            close(saved_stderr_fd_);
            saved_stderr_fd_ = -1;
        }
        std::string text;
        FILE *f = std::fopen(tempfile_.c_str(), "r");
        if (f) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
                text.append(buf, n);
            std::fclose(f);
        }
        return text;
    }
    ~StderrCapture()
    {
        if (saved_stderr_fd_ >= 0) {
            dup2(saved_stderr_fd_, fileno(stderr));
            close(saved_stderr_fd_);
        }
    }
private:
    TempFile tempfile_;
    int saved_stderr_fd_;
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

/* the line end xpp_writer_open writes: text mode, as the fopen "w" it
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
    char small[8];
    int want;

    /* fits: normal snprintf behaviour, return is the written length */
    want = xpp_snprintf(small, sizeof small, "%s", "hi");
    CHECK(want == 2);
    CHECK_STR(small, "hi");

    /* truncates: still NUL-terminated, return is the length it wanted
       (not -1, not the truncated length) */
    want = xpp_snprintf(small, sizeof small, "%s", "far too long for this");
    CHECK(want == (int)strlen("far too long for this"));
    CHECK(strlen(small) == sizeof(small) - 1);
    CHECK(small[sizeof small - 1] == '\0');

    /* XPP_SPRINTF takes the size from sizeof(dst) */
    char arr[6];
    int w2 = XPP_SPRINTF(arr, "%d", 123456789);
    CHECK(w2 == 9);
    CHECK(strlen(arr) == 5);
    CHECK(arr[5] == '\0');

    /* xpp_strlcpy: fits */
    char dst[10];
    size_t n = xpp_strlcpy(dst, "abc", sizeof dst);
    CHECK(n == 3);
    CHECK_STR(dst, "abc");

    /* xpp_strlcpy: truncates, still NUL-terminated, returns src's length */
    n = xpp_strlcpy(dst, "abcdefghijklmnop", sizeof dst);
    CHECK(n == strlen("abcdefghijklmnop"));
    CHECK(strlen(dst) == sizeof(dst) - 1);

    /* XPP_STRCPY takes the size from sizeof(dst) */
    char cbuf[4];
    n = XPP_STRCPY(cbuf, "hi");
    CHECK(n == 2);
    CHECK_STR(cbuf, "hi");
    n = XPP_STRCPY(cbuf, "toolong");
    CHECK(n == strlen("toolong"));
    CHECK(strlen(cbuf) == sizeof(cbuf) - 1);

    /* xpp_strlcat / XPP_STRCAT */
    char cat[8] = "ab";
    size_t total = xpp_strlcat(cat, "cd", sizeof cat);
    CHECK(total == 4);
    CHECK_STR(cat, "abcd");
    total = xpp_strlcat(cat, "efghij", sizeof cat);
    CHECK(total == 4 + strlen("efghij"));
    CHECK(strlen(cat) == sizeof(cat) - 1);

    char cat2[6] = "xy";
    size_t catn = XPP_STRCAT(cat2, "z");
    CHECK(catn == 3);
    CHECK_STR(cat2, "xyz");

    /* struct-member and array-element destinations: sizeof(dst) still
       sees the whole member/element array, not just one element */
    struct { char name[5]; } rec[2];
    XPP_STRCPY(rec[1].name, "abcdefgh");
    CHECK(strlen(rec[1].name) == 4);

    /* size 0: no write, no crash */
    want = xpp_snprintf(small, 0, "%s", "x");
    CHECK(want >= 0);

    /* a truncating call logs exactly one WARN, and a second call at the
       same call site does not log again */
    {
        StderrCapture cap;
        cap.begin();
        char buf[4];
        for (int i = 0; i < 3; i++)
            XPP_SPRINTF(buf, "%s", "way too long for four bytes");
        std::string out = cap.end();
        size_t count = 0, pos = 0;
        while ((pos = out.find("truncated", pos)) != std::string::npos) {
            count++;
            pos += 9;
        }
        CHECK(count == 1);
    }

    /* a call that fits logs nothing */
    {
        StderrCapture cap;
        cap.begin();
        char buf[64];
        XPP_SPRINTF(buf, "%s", "fits fine");
        std::string out = cap.end();
        CHECK(out.empty());
    }

#ifdef XPP_IO_HAVE_STD_FORMAT
    /* xpp::format: unbounded, std::format syntax, compile-time checked */
    CHECK_STR(xpp::format("{} and {}", 1, "two").c_str(), "1 and two");
    CHECK_STR(xpp::format("{:.2f}", 3.14159).c_str(), "3.14");

    /* XPP_FORMAT_TO_BUF: fits */
    {
        char fb[16];
        XPP_FORMAT_TO_BUF(fb, "{}-{}", 12, "ab");
        CHECK_STR(fb, "12-ab");
    }
    /* XPP_FORMAT_TO_BUF: truncates, still NUL-terminated, warns once */
    {
        StderrCapture cap;
        cap.begin();
        char fb[6];
        XPP_FORMAT_TO_BUF(fb, "{}", "way too long for six bytes");
        std::string out = cap.end();
        CHECK(strlen(fb) == sizeof(fb) - 1);
        CHECK(out.find("truncated") != std::string::npos);
    }
#endif
#ifdef XPP_IO_HAVE_TO_CHARS
    /* xpp::number: shortest round-trip text, no trailing garbage digits */
    CHECK_STR(xpp::number(3.5).c_str(), "3.5");
    CHECK_STR(xpp::number(100.0).c_str(), "100");
#endif

    /* ---- the file half: line reader, token reader, writer (W11 step 3) */

    /* failed open: a path that cannot exist */
    CHECK(xpp_line_reader_open("no/such/directory/file.txt") == NULL);
    CHECK(xpp_token_reader_open("no/such/directory/file.txt") == NULL);

    /* empty file: no lines at all */
    {
        TempFile tf("test_io_empty.tmp");
        write_raw(tf.c_str(), "");
        XppLineReader *r = xpp_line_reader_open(tf.c_str());
        CHECK(r != NULL);
        size_t len = 999;
        CHECK(xpp_line_reader_next(r, &len) == NULL);
        CHECK(len == 0);
        xpp_line_reader_close(r);
    }

    /* no trailing newline: the last, unterminated line still comes back
       once (not read twice, not dropped) */
    {
        TempFile tf("test_io_notrail.tmp");
        write_raw(tf.c_str(), "first\nsecond");
        XppLineReader *r = xpp_line_reader_open(tf.c_str());
        CHECK(r != NULL);
        size_t len;
        const char *l1 = xpp_line_reader_next(r, &len);
        CHECK(l1 != NULL);
        CHECK_STR(l1, "first");
        const char *l2 = xpp_line_reader_next(r, &len);
        CHECK(l2 != NULL);
        CHECK_STR(l2, "second");
        CHECK(len == 6);
        CHECK(xpp_line_reader_next(r, &len) == NULL);
        xpp_line_reader_close(r);
    }

    /* CRLF tolerant: \r\n and a bare \n both end a line, no stray \r left */
    {
        TempFile tf("test_io_crlf.tmp");
        write_raw(tf.c_str(), "one\r\ntwo\nthree\r\n");
        XppLineReader *r = xpp_line_reader_open(tf.c_str());
        CHECK(r != NULL);
        size_t len;
        CHECK_STR(xpp_line_reader_next(r, &len), "one");
        CHECK_STR(xpp_line_reader_next(r, &len), "two");
        CHECK_STR(xpp_line_reader_next(r, &len), "three");
        CHECK(xpp_line_reader_next(r, &len) == NULL);
        xpp_line_reader_close(r);
    }

    /* a long line (well past any fixed fgets buffer this replaces): comes
       back whole, not cut */
    {
        TempFile tf("test_io_long.tmp");
        std::string big(5000, 'x');
        std::string content = big + "\nshort\n";
        write_raw(tf.c_str(), content.c_str());
        XppLineReader *r = xpp_line_reader_open(tf.c_str());
        CHECK(r != NULL);
        size_t len;
        const char *l1 = xpp_line_reader_next(r, &len);
        CHECK(l1 != NULL);
        CHECK(len == big.size());
        CHECK(big == l1);
        CHECK_STR(xpp_line_reader_next(r, &len), "short");
        xpp_line_reader_close(r);
    }

    /* attach: reads through a FILE* the caller keeps open and owns --
       xpp_line_reader_close must not close it */
    {
        TempFile tf("test_io_attach.tmp");
        write_raw(tf.c_str(), "only line\n");
        FILE *fp = std::fopen(tf.c_str(), "r");
        CHECK(fp != NULL);
        XppLineReader *r = xpp_line_reader_attach(fp);
        size_t len;
        CHECK_STR(xpp_line_reader_next(r, &len), "only line");
        xpp_line_reader_close(r);
        CHECK(std::feof(fp) == 0 || std::fgetc(fp) == EOF); /* fp still usable */
        std::fclose(fp);
    }

    /* token reader: fscanf "%lg"/"%d"/"%s" equivalents, whitespace
       separated, strtod on the same token agreeing with the double read */
    {
        TempFile tf("test_io_tok.tmp");
        write_raw(tf.c_str(), "  3.5 -7 hello   1e3\n");
        XppTokenReader *r = xpp_token_reader_open(tf.c_str());
        CHECK(r != NULL);
        double d;
        int i;
        char s[16];
        CHECK(xpp_token_reader_double(r, &d) == 1);
        CHECK(d == strtod("3.5", NULL));
        CHECK(xpp_token_reader_int(r, &i) == 1);
        CHECK(i == -7);
        CHECK(xpp_token_reader_string(r, s, sizeof s) == 1);
        CHECK_STR(s, "hello");
        CHECK(xpp_token_reader_double(r, &d) == 1);
        CHECK(d == 1e3);
        CHECK(xpp_token_reader_double(r, &d) == 0); /* end of file */
        xpp_token_reader_close(r);
    }

    /* a float token reads as fscanf "%g" reads it (rounded straight to
       float), and the whitespace after a token stays in the stream */
    {
        TempFile tf("test_io_tok.tmp");
        write_raw(tf.c_str(), "0.1 16777217 2.5\nnext\n");
        std::FILE *fp = std::fopen(tf.c_str(), "r");
        CHECK(fp != NULL);
        float a, b, sa, sb;
        XppTokenReader *r = xpp_token_reader_attach(fp);
        CHECK(xpp_token_reader_float(r, &a) == 1);
        CHECK(xpp_token_reader_float(r, &b) == 1);
        CHECK(xpp_token_reader_float(r, &sa) == 1);
        CHECK(std::fgetc(fp) == '\n');
        xpp_token_reader_close(r);
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
        XppWriter *w = xpp_writer_open_binary(tf.c_str());
        CHECK(w != NULL);
        std::fputs("a\r\nb\n", xpp_writer_file(w));
        CHECK(xpp_writer_commit(w) == 0);
        std::FILE *fp = std::fopen(tf.c_str(), "rb");
        char buf[16] = {0};
        CHECK(fp != NULL && std::fread(buf, 1, sizeof buf - 1, fp) == 5);
        if (fp) std::fclose(fp);
        CHECK(std::strcmp(buf, "a\r\nb\n") == 0);
    }

    /* writer: commit renames the temp file into place, byte for byte */
    {
        TempFile tf("test_io_write.tmp");
        XppWriter *w = xpp_writer_open(tf.c_str());
        CHECK(w != NULL);
        xpp_writer_printf(w, "%d %s\n", 42, "answer");
        CHECK(xpp_writer_commit(w) == 0);
        CHECK(read_raw(tf.c_str()) == "42 answer" TEXT_NL);
    }

    /* writer: abandoned (xpp_writer_abort) leaves an existing file
       completely untouched */
    {
        TempFile tf("test_io_write.tmp");
        write_raw(tf.c_str(), "original\n");
        XppWriter *w = xpp_writer_open(tf.c_str());
        CHECK(w != NULL);
        xpp_writer_printf(w, "clobber");
        xpp_writer_abort(w);
        CHECK(read_raw(tf.c_str()) == "original\n");
    }

    /* writer: failed open (no such directory) */
    CHECK(xpp_writer_open("no/such/directory/file.txt") == NULL);

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
    }

    TEST_REPORT("test_io");
}
