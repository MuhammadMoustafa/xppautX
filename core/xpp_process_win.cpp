/* xpp_process_internal.h on Windows: CreateProcess (xpp_win32.cpp's run_process)
   with the arguments quoted into one command line. */
#include "xpp_process_internal.h"
#include "xpp_win32.h"

#include <span>
#include <string>

namespace xpp::process {

Result<> start_program(std::span<const std::string> argv, Wait wait, Quoting quoting)
{
    std::string line;
    for (const std::string &a : argv) line += (line.empty() ? "" : " ") + quote_argument(a, quoting);
    return win32::run_process(argv.front(), line, wait == Wait::until_it_ends);
}

} // namespace xpp::process
