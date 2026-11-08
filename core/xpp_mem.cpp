/* xpp_mem.h's implementation: the one thing the raw allocator (retired at
   W48) left behind, the loud, final exit a failed allocation always took.
   Nothing here throws. */
#include "xpp_mem.h"
#include "xpp_log.h"

#include <cstdio>
#include <cstdlib>
#include <new>

void xpp::out_of_memory(std::string_view what) noexcept
{
    xpp::log(XPP_LOG_ERROR, "out of memory {}\n", what);
    std::exit(1);
}

void xpp::out_of_memory_now(std::string_view what) noexcept
{
    xpp::log(XPP_LOG_ERROR, "out of memory {}\n", what);
    std::fflush(nullptr);
    std::_Exit(1);
}

xpp::ZeroedBytes::ZeroedBytes(std::size_t n)
{
    p_ = std::calloc(n ? n : 1, 1);
    if (!p_)
        throw std::bad_alloc();
}

xpp::ZeroedBytes::~ZeroedBytes()
{
    std::free(p_);
}
