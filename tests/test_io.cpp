/* xpp_io: the safe formatting/copying the whole core uses instead of
   sprintf/strcpy into fixed buffers (issue: W11 step 2). Checks
   truncation, NUL termination, the returned lengths, XPP_SPRINTF/
   XPP_STRCPY/XPP_STRCAT taking sizeof(dst) automatically, that a
   truncating call logs exactly one WARN (redirecting stderr, since
   xpp_log's default threshold already prints WARN and up), and the
   C++ API (xpp::format, XPP_FORMAT_TO_BUF, xpp::number). */
#include "xpptest.h"
#include "xpp_io.h"
#include "xpp_log.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

/* Captures everything written to stderr between begin() and end() by
   redirecting the process's stderr to a scratch file and reading it
   back; good enough for one test binary that exits right after. */
class StderrCapture {
public:
    void begin()
    {
        std::fflush(stderr);
        path_ = "test_io_stderr.tmp";
        saved_ = freopen(path_, "w", stderr);
        CHECK(saved_ != NULL);
    }
    std::string end()
    {
        std::fflush(stderr);
        std::string text;
        FILE *f = std::fopen(path_, "r");
        if (f) {
            char buf[4096];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
                text.append(buf, n);
            std::fclose(f);
        }
        std::remove(path_);
        return text;
    }
private:
    const char *path_;
    FILE *saved_;
};

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

    TEST_REPORT("test_io");
}
