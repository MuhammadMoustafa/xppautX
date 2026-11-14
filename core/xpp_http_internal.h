/* The platform primitives of xpp_http.cpp (the HTTP server and its token check, the
   routes, the page's event streams, the log pipe), implemented once per platform and
   picked by the build: xpp_http_win.cpp (Winsock, ShellExecute, rand_s),
   xpp_http_posix.cpp (BSD sockets and the POSIX calls Linux and macOS share) and the
   browser opener of xpp_http_linux.cpp / xpp_http_mac.cpp. Each is one socket or
   system call or a short sequence of them, so xpp_http.cpp includes no socket,
   Windows or POSIX system header and none of their names (macros included) can
   clash with core names. Internal to the xpp_http*.cpp files. */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace xpp::http {

/* a socket as the server holds it: an int's range on POSIX, Winsock's SOCKET (a pointer-sized integer) on Windows */
using sock_t = std::intptr_t;
inline constexpr sock_t INVALID_SOCK = -1;

/* before the first socket: Winsock's start; elsewhere a write to a closed connection must be an error, not SIGPIPE */
bool start_sockets();
/* the listening socket on 127.0.0.1 (this machine only) at port (0: any free one), its port in bound_port; INVALID_SOCK when it could not */
sock_t listen_loopback(int port, int &bound_port);
/* the next connection, INVALID_SOCK when accept failed */
sock_t accept_connection(sock_t listener);
void close_socket(sock_t s);
/* what send wrote (at most a bounded step of data), <= 0 on an error; never raises SIGPIPE */
long send_some(sock_t s, const char *data, std::size_t n);
/* the parts in one gathered write (no copy, and a small event goes out in one packet): the bytes written, <= 0 on an error */
long send_gather(sock_t s, std::span<const std::string_view> parts);
/* the sending half closed: the peer reads what was sent, then the end */
void end_sending(sock_t s);
/* up to n bytes, the bytes read, <= 0 at the end or on an error */
long receive(sock_t s, char *buf, int n);
/* each small write goes out at once, not held back for the next (Nagle's algorithm) */
void set_nodelay(sock_t s);
enum class Timeout { receive, send };
/* a timeout in milliseconds (zero would disable it) */
bool set_timeout(sock_t s, Timeout which, int milliseconds);
/* a process xppautX starts must not inherit the socket, or a descriptor */
void no_inherit_socket(sock_t s);
void no_inherit_fd(int fd);
/* the log pipe, whose ends no started process inherits; false when it could not be made */
bool make_log_pipe(std::array<int, 2> &fds);
int dup_fd(int fd);
int dup2_fd(int from, int to);
long read_fd(int fd, char *buf, std::size_t n);
/* the bytes of the token: the system's random source (a fallback to a seeded rand only where /dev/urandom cannot be read) */
void random_bytes(std::span<unsigned char> out);
/* ends the process at once, whatever the core's thread holds (exit() would run at_exit and wait on a lock) */
[[noreturn]] void end_process();
/* the system's browser on url, without a shell; true when it started */
bool open_in_browser(const std::string &url);
/* the program that opens an address on Linux ("xdg-open") and macOS ("open"): xpp_http_posix.cpp starts it */
const char *browser_opener();

} // namespace xpp::http
