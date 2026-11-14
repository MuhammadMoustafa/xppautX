/* The files module's functions macOS answers differently from Linux (the
   rest of Linux and macOS is xpp_files_posix.cpp): where the user's settings
   live, which file is the running program and where the files it ships are
   (inside the .app bundle). */
#include "xpp_files.h"
#include "xpp_files_internal.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <mach-o/dyld.h> /* _NSGetExecutablePath */

namespace xpp::files {

std::string system_config_base()
{
    const char *home = std::getenv("HOME");
    const bool have_home = home && home[0];
    std::string base;
    if (have_home) base = std::string(home) + "/Library/Application Support";
    return base;
}

std::string program_file()
{
    std::string exe(PATH_BUFFER_BYTES, '\0');
    std::uint32_t size = static_cast<std::uint32_t>(exe.size());
    if (_NSGetExecutablePath(exe.data(), &size) != 0) return {}; /* too long for the buffer: no path */
    exe.resize(std::strlen(exe.c_str()));
    return exe;
}

std::string shipped_files_folder(const std::string &exe_folder)
{
    /* the .app bundle's layout: the executable in Contents/MacOS, the files it ships in Contents/Resources */
    const std::string contents = split_path(exe_folder).first;
    if (split_path(exe_folder).second == "MacOS" && split_path(contents).second == "Contents")
        return join(contents, "Resources");
    return exe_folder;
}

} // namespace xpp::files
