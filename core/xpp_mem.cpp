/* xpp_mem.h's implementation: the one thing the raw allocator (retired at
   W48) left behind, the loud, final exit a failed allocation always took.
   Nothing here throws. */
#include "xpp_mem.h"
#include "xpp_log.h"

#include <cstdlib>

void xpp_out_of_memory(const char *what)
{
    xpp_log(XPP_LOG_ERROR, "out of memory %s\n", what);
    std::exit(1);
}
