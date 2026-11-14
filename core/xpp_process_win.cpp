/* xpp_process_internal.h on Windows: CreateProcess of the program at argv[0] (its path: no search of the working
   folder) with the arguments quoted into one command line, no shell, the child's streams the null device. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "xpp_process_internal.h"

#include <span>
#include <string>
#include <vector>

namespace xpp::process {

Result<> start_program(std::span<const std::string> argv, Wait wait_for, Quoting quoting)
{
    std::string line;
    for (const std::string &a : argv) line += (line.empty() ? "" : " ") + quote_argument(a, quoting);
    const std::string &program = argv.front();
    const bool wait = wait_for == Wait::until_it_ends;
    constexpr std::string_view WHERE = "start a program";
    const Place place{program};
    std::vector<char> command(line.begin(), line.end());
    command.push_back('\0'); /* CreateProcessA may write into its command line */
    STARTUPINFOA startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION child{};
    /* lpApplicationName is the program itself: no search of the working folder or the path */
    if (!CreateProcessA(program.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child))
        return fail(std::string(WHERE), "the program could not be started (Windows error " + std::to_string(GetLastError()) + ")", place);
    CloseHandle(child.hThread);
    DWORD status = 0;
    const bool ended = wait && WaitForSingleObject(child.hProcess, INFINITE) == WAIT_OBJECT_0 && GetExitCodeProcess(child.hProcess, &status);
    CloseHandle(child.hProcess);
    if (wait && (!ended || status != 0)) return fail(std::string(WHERE), "the program did not end with status 0", place);
    return {};
}

} // namespace xpp::process
