/* xpp_process.h: posix_spawn here, CreateProcess in xpp_win32.cpp */
#include "xpp_process.h"

#include <algorithm>

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

namespace {

constexpr std::string_view BLANKS_AND_QUOTE = " \t\n\v\"";
constexpr std::string_view WHERE = "start a program";

/* a property name: what the installer allows in a public one */
bool property_name(std::string_view name)
{
    return !name.empty() && std::ranges::all_of(name, [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'; });
}

} // namespace

std::string quote_argument(std::string_view arg, Quoting quoting)
{
    const std::size_t equals = arg.find('=');
    if (quoting == Quoting::installer && equals != std::string_view::npos && property_name(arg.substr(0, equals))) {
        const std::string_view value = arg.substr(equals + 1);
        if (value.find_first_of(BLANKS_AND_QUOTE) == std::string_view::npos) return std::string(arg);
        std::string out = std::string(arg.substr(0, equals + 1)) + "\"";
        for (const char c : value) out += c == '"' ? std::string("\"\"") : std::string(1, c);
        return out + "\"";
    }
    if (!arg.empty() && arg.find_first_of(BLANKS_AND_QUOTE) == std::string_view::npos) return std::string(arg);
    std::string out = "\"";
    std::size_t slashes = 0;
    for (const char c : arg) {
        if (c == '\\') {
            slashes++;
            continue;
        }
        if (c == '"') out.append(slashes * 2 + 1, '\\');
        else out.append(slashes, '\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, '\\');
    return out + "\"";
}

#ifdef _WIN32

Result<> spawn(std::span<const std::string> argv, Wait wait, Quoting quoting)
{
    if (argv.empty() || argv.front().empty()) return fail(std::string(WHERE), "no program is named", Place{std::string(WHERE)});
    std::string line;
    for (const std::string &a : argv) line += (line.empty() ? "" : " ") + quote_argument(a, quoting);
    return win32::run_process(argv.front(), line, wait == Wait::until_it_ends);
}

#else

Result<> spawn(std::span<const std::string> argv, Wait wait, Quoting)
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
