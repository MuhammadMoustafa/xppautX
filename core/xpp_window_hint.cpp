/* The message for a window library that does not load (xpp_window_hint.h):
   pure text in, text out, so tests/test_window_hint.cpp can pin it. */
#include "xpp_window_hint.h"
#include "xpp_io.h"
#include <string>
#include <string_view>
#include <vector>

namespace {

/* dlerror()'s wording for a library the loader cannot find */
constexpr std::string_view NOT_FOUND = ": cannot open shared object file";

/* KEY's value in os-release's KEY=value lines, without its quotes */
std::string os_value(std::string_view text, std::string_view key)
{
    while (!text.empty()) {
        size_t eol = text.find('\n');
        std::string_view line = text.substr(0, eol);
        text = eol == std::string_view::npos ? std::string_view() : text.substr(eol + 1);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=') continue;
        std::string_view v = line.substr(key.size() + 1);
        if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front())
            v = v.substr(1, v.size() - 2);
        return std::string(v);
    }
    return {};
}

std::vector<std::string> words(const std::string &s)
{
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        if (j > i) out.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    return out;
}

bool starts_with(const std::string &s, std::string_view p) { return s.compare(0, p.size(), p) == 0; }

/* the command that installs WebKitGTK 4.1 (and GTK 3 with it) on this
   system: its ID first, then what it is like; empty when none is known */
std::string install_command(std::string_view os_release)
{
    std::vector<std::string> ids = words(os_value(os_release, "ID"));
    for (const std::string &w : words(os_value(os_release, "ID_LIKE"))) ids.push_back(w);
    for (const std::string &id : ids) {
        if (id == "debian" || id == "ubuntu") return "sudo apt install libwebkit2gtk-4.1-0";
        if (id == "fedora" || id == "rhel" || id == "centos") return "sudo dnf install webkit2gtk4.1";
        if (id == "arch") return "sudo pacman -S webkit2gtk-4.1";
        if (starts_with(id, "opensuse") || id == "suse" || id == "sles")
            return "sudo zypper install libwebkit2gtk-4_1-0";
    }
    return {};
}

std::string message(std::string_view os_release, std::string_view err)
{
    size_t at = err.find(NOT_FOUND);
    if (at == std::string_view::npos)
        return "xppautX: the window cannot open (" + std::string(err) + "); using the browser instead\n";
    /* "/proc/self/fd/5: libwebkit2gtk-4.1.so.0: cannot open ...": the name
       between the last ": " before the wording and the wording */
    std::string_view lib = err.substr(0, at);
    size_t colon = lib.rfind(": ");
    if (colon != std::string_view::npos) lib = lib.substr(colon + 2);
    std::string cmd = install_command(os_release);
    std::string how = cmd.empty() ? "install WebKitGTK 4.1 (libwebkit2gtk-4.1) with your system's package manager"
                                  : "install it with: " + cmd;
    return "xppautX: the window needs WebKitGTK, which is not installed (" + std::string(lib) + " not found); " +
           how + ". Using the browser instead.\n";
}

} /* namespace */

std::string xpp::window_load_message(std::string_view os_release, std::string_view dl_error)
{
    return message(os_release, dl_error);
}

void xpp_window_load_message(char *out, size_t size, const char *os_release, const char *dl_error)
{
    if (!out || size == 0) return;
    try {
        std::string m = message(os_release ? os_release : "", dl_error ? dl_error : "unknown error");
        xpp_strlcpy(out, m.c_str(), size);
    } catch (...) {
        xpp_strlcpy(out, "xppautX: the window cannot open; using the browser instead\n", size);
    }
}
