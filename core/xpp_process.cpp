/* xpp_process.h: posix_spawn here, CreateProcess in xpp_win32.cpp */
#include "xpp_process.h"

#include "xpp_io.h"
#include "xpp_win32.h"
#ifndef _WIN32
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <vector>
extern char **environ; /* posix_spawnp passes the environment on */
#endif

namespace xpp::process {

#ifdef _WIN32

Result<> spawn(std::span<const std::string> argv, Wait wait) { return win32::run_process(argv, wait == Wait::until_it_ends); }

#else

namespace {
constexpr std::string_view WHERE = "start a program";
}

Result<> spawn(std::span<const std::string> argv, Wait wait)
{
    if (argv.empty() || argv.front().empty()) return fail(std::string(WHERE), "no program is named", Place{std::string(WHERE)});
    const Place place{argv.front()};
    std::vector<const char *> args;
    for (const std::string &a : argv) args.push_back(a.c_str());
    args.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return fail(std::string(WHERE), "out of resources", place);
    /* the program's output stays out of ours */
    bool ok = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) == 0 &&
              posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) == 0 &&
              posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0) == 0;
    pid_t child = 0;
    ok = ok && posix_spawnp(&child, args[0], &actions, nullptr, const_cast<char *const *>(args.data()), environ) == 0;
    posix_spawn_file_actions_destroy(&actions);
    if (!ok) return fail(std::string(WHERE), "the program could not be started", place);
    if (wait == Wait::not_at_all) return {};
    int status = 0;
    while (waitpid(child, &status, 0) < 0)
        if (errno != EINTR) return fail(std::string(WHERE), "waiting for the program failed", place);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return fail(std::string(WHERE), "the program did not end with status 0", place);
    return {};
}

#endif

} // namespace xpp::process
