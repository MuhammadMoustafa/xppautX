/* xpp_mem.h's implementation: the one thing the raw allocator (retired at
   W48) left behind, the loud, final exit a failed allocation always took.
   Nothing here throws. */
#include "xpp_mem.h"
#include "xpp_log.h"

#include <cstdio>
#include <cstdlib>

void xpp::out_of_memory(std::string_view what) noexcept
{
    xpp::log_printf(XPP_LOG_ERROR, "out of memory %.*s\n", static_cast<int>(what.size()), what.data());
    std::exit(1);
}

void xpp::out_of_memory_now(std::string_view what) noexcept
{
    xpp::log_printf(XPP_LOG_ERROR, "out of memory %.*s\n", static_cast<int>(what.size()), what.data());
    std::fflush(nullptr);
    std::_Exit(1);
}
