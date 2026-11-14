/* core/xpp_path.h's implementation: a file's name as text. */
#include "xpp_path.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstring>

#include "xpp_text.h"

namespace {

bool device_name(std::string_view name)
{
    /* CON, NUL, COM1.txt ...: Windows opens the device whatever follows the dot */
    static const char *const devices[] = {"con", "prn", "aux", "nul", "conin$", "conout$"};
    std::string stem;
    for (char c : name.substr(0, name.find('.'))) stem += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    for (const char *d : devices)
        if (stem == d) return true;
    return stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0)
           && stem[3] >= '0' && stem[3] <= '9';
}

/* whether path's root is a drive ("C:\x": sep is the index of its last separator): never off Windows */
bool drive_root(std::string_view path, std::size_t sep)
{
#ifdef _WIN32
    return sep == 2 && path[1] == ':';
#else
    (void)path;
    (void)sep;
    return false;
#endif
}

} // namespace

namespace xpp::files {

bool name_ok(std::string_view name, bool allow_hidden)
{
    if (name.empty() || (!allow_hidden && name[0] == '.') || name[0] == ' ') return false;
    const size_t n = name.size();
    if (n > NAME_MAX_BYTES || name.find("..") != std::string_view::npos) return false;
    constexpr std::string_view reserved = "/\\:<>\"|?*";
    for (char ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c == 0x7f || reserved.find(ch) != std::string_view::npos) return false;
    }
    /* Windows drops a trailing dot or space, so "a.set." would be a.set */
    if (name[n - 1] == '.' || name[n - 1] == ' ') return false;
    return !device_name(name);
}

std::string output_name(std::string_view model_file, std::string_view ext, std::string_view what)
{
    std::string base = split_path(model_file).second;
    const std::size_t dot = base.rfind('.');
    if (dot != std::string::npos && dot > 0) base.resize(dot);
    if (!what.empty()) { base += '-'; base += what; }
    return base + std::string(ext);
}

std::string run_file_name(std::string_view path, int i)
{
    const auto [folder, base] = split_path(path);
    const std::size_t dot = std::min(base.size(), base.rfind('.'));
    const std::string name = xpp::format("{}_{:04d}{}", base.substr(0, dot), i, base.substr(dot));
    return folder.empty() ? name : append_path(folder, name);
}

std::pair<std::string, std::string> split_path(std::string_view path)
{
    const size_t sep = path.find_last_of(SEPARATORS);
    if (sep == std::string_view::npos) return {std::string(), std::string(path)};
    /* "/x" (and on Windows "C:\x"): the root itself */
    size_t keep = sep == 0 ? 1 : sep;
    if (drive_root(path, sep)) keep = 3;
    return {std::string(path.substr(0, keep)), std::string(path.substr(sep + 1))};
}

bool has_extension(std::string_view path, std::string_view ext)
{
    const std::string name = split_path(path).second;
    return !ext.empty() && name.size() > ext.size() &&
           xpp::equal_ignoring_case(std::string_view(name).substr(name.size() - ext.size()), ext);
}

std::string join(std::string_view dir, std::string_view name) { return xpp::format("{}{}{}", dir, SEP, name); }

bool is_absolute(std::string_view path)
{
#ifdef _WIN32
    const auto sep = [](char c) { return c == '/' || c == '\\'; };
    return (!path.empty() && sep(path[0])) ||
           (path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' && sep(path[2]));
#else
    return !path.empty() && path[0] == '/';
#endif
}

std::string append_path(std::string_view dir, std::string_view path)
{
    std::string base(dir);
    if (!base.empty() && SEPARATORS.find(base.back()) == std::string_view::npos) base += '/';
    return base.append(path);
}

Error open_error(std::string where, std::string_view file, std::string_view why)
{
    const std::string reason = why.empty() ? std::strerror(errno) : std::string(why);
    return Error{std::move(where), xpp::format("cannot open {}: {}", file, reason), Place{std::string(file)}};
}

} // namespace xpp::files
