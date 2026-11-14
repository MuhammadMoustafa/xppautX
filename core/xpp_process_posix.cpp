/* xpp_process_internal.h on Linux and macOS: posix_spawn, the arguments as they are. */
#include "xpp_process_internal.h"

#include <cerrno>
#include <span>
#include <string>
#include <vector>

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ; /* posix_spawnp passes the environment on */

namespace xpp::process {

Result<> start_program(std::span<const std::string> argv, Wait wait, Quoting)
{
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

} // namespace xpp::process
