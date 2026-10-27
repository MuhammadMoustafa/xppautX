/* What the unit tests share beyond the framework in xpptest.h (W510, #574):
   the config folder's environment variable, and a made-up absolute path.
   C++ only; a test includes it after xpptest.h. */
#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H
#include "xpp_files.h"

#include <cstdlib>
#include <string>

/* XPP_CONFIG_DIR set to dir, or unset for nullptr (MinGW has no setenv) */
inline bool set_config_dir(const char *dir)
{
#ifdef _WIN32
    return _putenv_s(xpp::files::CONFIG_DIR_ENV, dir ? dir : "") == 0;
#else
    return dir ? setenv(xpp::files::CONFIG_DIR_ENV, dir, 1) == 0 : unsetenv(xpp::files::CONFIG_DIR_ENV) == 0;
#endif
}

/* the absolute path of a made-up file, on either system */
inline std::string abs_path(const std::string &name)
{
#ifdef _WIN32
    return "C:/models/" + name;
#else
    return "/models/" + name;
#endif
}

#endif
