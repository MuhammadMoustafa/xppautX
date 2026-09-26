/* AUTO's text output (fort.7, fort.8, fort.9 and the other streams AUTO
   writes during a run, which stay FILE *) through std::format instead of
   printf formats: a format that does not match its arguments is a
   compile error. C++ only; include it right after xpp_io.h, before
   auto_f2c.h (whose min/max macros break the standard headers). */
#ifndef XPP_AUTO_PRINT_H
#define XPP_AUTO_PRINT_H

#include <cstdio>
#include <format>
#include <string>
#include <utility>

#include "xpp_io.h"

namespace xpp::auto_out {

/* fprintf(fp, ...) with a std::format format string. Formatting cannot
   throw out of here (xpp::format exits on failure, like xpp_mem). */
template <class... Args>
void print(FILE *fp, std::format_string<Args...> fmt, Args &&...args) noexcept
{
    const std::string s = xpp::format(fmt, std::forward<Args>(args)...);
    std::fwrite(s.data(), 1, s.size(), fp);
}

} // namespace xpp::auto_out

#endif
