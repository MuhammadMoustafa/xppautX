/* xpp::window::load_message: what xppautX says when the Linux window's
   library does not load (W13e). A missing WebKitGTK must name the missing
   library and the install command for the system /etc/os-release
   describes; any other failure is quoted. tools/modecheck.sh checks that
   the message reaches the log and that the browser opens instead. */
#include "xpptest.h"
#include "xpp_window_hint.h"

#include <string>

namespace {

const char MISSING[] =
    "/proc/self/fd/5: libwebkit2gtk-4.1.so.0: cannot open shared object file: No such file or directory";

std::string msg(std::string_view os_release, std::string_view err)
{
    return xpp::window::load_message(os_release, err);
}

bool has(const std::string &s, const char *part) { return s.find(part) != std::string::npos; }

} /* namespace */

int main()
{
    /* the whole line, on Ubuntu */
    CHECK_STR(msg("PRETTY_NAME=\"Ubuntu 22.04.4 LTS\"\nNAME=\"Ubuntu\"\nID=ubuntu\nID_LIKE=debian\n", MISSING).c_str(),
              "xppautX: the window needs WebKitGTK, which is not installed (libwebkit2gtk-4.1.so.0 not found); "
              "install it with: sudo apt install libwebkit2gtk-4.1-0. Using the browser instead.\n");

    /* each family, by ID or by ID_LIKE, quoted or not, CRLF too */
    CHECK(has(msg("ID=debian\n", MISSING), "sudo apt install libwebkit2gtk-4.1-0"));
    CHECK(has(msg("ID=linuxmint\nID_LIKE=\"ubuntu debian\"\n", MISSING), "sudo apt install libwebkit2gtk-4.1-0"));
    CHECK(has(msg("ID=fedora\r\nVERSION_ID=40\r\n", MISSING), "sudo dnf install webkit2gtk4.1"));
    CHECK(has(msg("ID=\"rocky\"\nID_LIKE=\"rhel centos fedora\"\n", MISSING), "sudo dnf install webkit2gtk4.1"));
    CHECK(has(msg("ID=arch\n", MISSING), "sudo pacman -S webkit2gtk-4.1"));
    CHECK(has(msg("ID=manjaro\nID_LIKE=arch\n", MISSING), "sudo pacman -S webkit2gtk-4.1"));
    CHECK(has(msg("ID=\"opensuse-tumbleweed\"\nID_LIKE=\"opensuse suse\"\n", MISSING),
              "sudo zypper install libwebkit2gtk-4_1-0"));
    CHECK(has(msg("ID='opensuse-leap'\n", MISSING), "sudo zypper install libwebkit2gtk-4_1-0"));
    /* ID wins over ID_LIKE; a key that only starts like ID is not ID */
    CHECK(has(msg("VERSION_ID=12\nID_LIKE=debian\nID=fedora\n", MISSING), "dnf"));

    /* an unknown system, or no os-release: the library's name */
    std::string other = msg("ID=gentoo\n", MISSING);
    CHECK(has(other, "libwebkit2gtk-4.1.so.0 not found"));
    CHECK(has(other, "install WebKitGTK 4.1 (libwebkit2gtk-4.1) with your system's package manager"));
    CHECK(!has(other, "sudo"));
    CHECK(has(msg("", MISSING), "your system's package manager"));

    /* the missing one may be GTK or another dependency: named as it is */
    CHECK(has(msg("ID=ubuntu\n", "/tmp/xppautx-window-Ab12Cd: libgtk-3.so.0: cannot open shared object file: No such "
                                 "file or directory"),
              "(libgtk-3.so.0 not found)"));
    CHECK(has(msg("ID=ubuntu\n", "libjavascriptcoregtk-4.1.so.0: cannot open shared object file: x"),
              "(libjavascriptcoregtk-4.1.so.0 not found)"));

    /* anything else is quoted, with no install advice */
    CHECK_STR(msg("ID=ubuntu\n", "/proc/self/fd/5: undefined symbol: webkit_web_view_new").c_str(),
              "xppautX: the window cannot open (/proc/self/fd/5: undefined symbol: webkit_web_view_new); using the "
              "browser instead\n");
    CHECK(has(msg("ID=ubuntu\n", ""), "(unknown error)"));

    /* xpp::webview_error_message (W35e): translate webview error codes */
    CHECK_STR(xpp::webview_error_message(-5, "").c_str(),
              "xppautX: the window cannot open (no web view: on Windows the WebView2 runtime, on Linux a "
              "display); using the browser instead\n");
    CHECK_STR(xpp::webview_error_message(-1, "bad display").c_str(),
              "xppautX: the window cannot open (webview error -1: bad display); using the browser instead\n");
    CHECK_STR(xpp::webview_error_message(-2, "").c_str(),
              "xppautX: the window cannot open (webview error -2: no details); using the browser instead\n");

    TEST_REPORT("window_hint");
}
