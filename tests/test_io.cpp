/* xpp_io: the safe formatting/copying the whole core uses instead of
   sprintf/strcpy into fixed buffers (issue: W11 step 2). Checks
   truncation, NUL termination, the returned lengths, XPP_SPRINTF/
   XPP_STRCPY/XPP_STRCAT taking sizeof(dst) automatically, and that a
   truncating call logs exactly one WARN (redirecting stderr, since
   xpp_log's default threshold already prints WARN and up). */
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

    /* XPP_STRCPY takes sizeof(dst) and evaluates to dst, like strcpy */
    char cbuf[4];
    char *ret = XPP_STRCPY(cbuf, "hi");
    CHECK(ret == cbuf);
    CHECK_STR(cbuf, "hi");
    ret = XPP_STRCPY(cbuf, "toolong");
    CHECK(ret == cbuf);
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
    char *catret = XPP_STRCAT(cat2, "z");
    CHECK(catret == cat2);
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

    TEST_REPORT("test_io");
}
