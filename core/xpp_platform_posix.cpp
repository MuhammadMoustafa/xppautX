/* xpp_platform.h for Linux and macOS: stdin through read(2) and poll(2), no line-end
   translation, no console to attach. */
#include "xpp_platform.h"

#include <cerrno>
#include <chrono>
#include <string>

#include <poll.h>
#include <sys/types.h>
#include <unistd.h>

namespace xpp::platform {

long read_input(char *buf, std::size_t n)
{
    for (;;) {
        ssize_t r = read(0, buf, n);
        if (r < 0 && errno == EINTR) continue;
        return static_cast<long>(r);
    }
}

std::optional<std::string> read_input_line(int seconds, std::size_t cap)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    std::string line;
    while (line.size() < cap) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (left.count() <= 0) return std::nullopt;
        pollfd input{STDIN_FILENO, POLLIN, 0};
        if (poll(&input, 1, static_cast<int>(left.count())) < 0) {
            if (errno == EINTR) continue;
            return std::nullopt;
        }
        if (!(input.revents & (POLLIN | POLLHUP))) return std::nullopt;
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) return std::nullopt;
        if (c == '\n') return line;
        if (c != '\r') line += c;
    }
    return std::nullopt;
}

void set_binary_mode(int) {}

void attach_parent_console() {}

} // namespace xpp::platform
