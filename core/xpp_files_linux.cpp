/* The files module's functions Linux answers differently from macOS (the
   rest of Linux and macOS is xpp_files_posix.cpp): where the user's settings
   live, which file is the running program and where the files it ships are. */
#include "xpp_files.h"
#include "xpp_files_internal.h"

#include <cstddef>
#include <cstdlib>
#include <string>

#include <unistd.h>

namespace xpp::files {

std::string system_config_base()
{
    const char *home = std::getenv("HOME");
    const bool have_home = home && home[0];
    std::string base;
    /* the XDG spec: a relative $XDG_CONFIG_HOME is invalid and ignored */
    const char *xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] == '/') base = xdg;
    else if (have_home) base = std::string(home) + "/.config";
    return base;
}

std::string program_file()
{
    std::string exe(PATH_BUFFER_BYTES, '\0');
    const ssize_t n = readlink("/proc/self/exe", exe.data(), exe.size());
    if (n <= 0 || static_cast<std::size_t>(n) == exe.size()) return {}; /* an unreadable or cut-short link */
    exe.resize(static_cast<std::size_t>(n));
    return exe;
}

std::string shipped_files_folder(const std::string &exe_folder) { return exe_folder; }

} // namespace xpp::files
