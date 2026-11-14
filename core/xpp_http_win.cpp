/* xpp_http_internal.h on Windows: Winsock, the CRT's descriptors and pipe, rand_s and ShellExecute.
   One of the files that include <windows.h> (the rule of the *_win.cpp files): it includes no core header but
   xpp_http_internal.h, so the macros of windows.h cannot clash with core names. */
#include "xpp_http_internal.h"

#include <cstdio>
#include <cstdlib>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>

extern "C" errno_t rand_s(unsigned int *); /* the C library's; stdlib.h declares it only with _CRT_RAND_S */

namespace xpp::http {

namespace {

SOCKET native(sock_t s) { return static_cast<SOCKET>(s); }
sock_t portable(SOCKET s) { return static_cast<sock_t>(s); }

constexpr std::size_t SEND_LIMIT_BYTES = 65536; /* send takes an int: bound each step (xpp_http.cpp's CHUNK) */
constexpr unsigned LOG_PIPE_BYTES = 65536; /* the log pipe's buffer: printed text waits here for log_main */

} // namespace

bool start_sockets()
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

sock_t listen_loopback(int port, int &bound_port)
{
    struct sockaddr_in addr{};
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) return INVALID_SOCK;
    no_inherit_socket(portable(listener));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* this machine only */
    addr.sin_port = htons(static_cast<unsigned short>(port));
    constexpr int LISTEN_BACKLOG = 16; /* queue a burst of browser connections while accept dispatches them */
    if (bind(listener, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr) != 0 || listen(listener, LISTEN_BACKLOG) != 0) {
        closesocket(listener);
        return INVALID_SOCK;
    }
    socklen_t len = sizeof addr;
    getsockname(listener, reinterpret_cast<struct sockaddr *>(&addr), &len);
    bound_port = ntohs(addr.sin_port);
    return portable(listener);
}

sock_t accept_connection(sock_t listener)
{
    const SOCKET s = accept(native(listener), nullptr, nullptr);
    return s == INVALID_SOCKET ? INVALID_SOCK : portable(s);
}

void close_socket(sock_t s) { closesocket(native(s)); }

long send_some(sock_t s, const char *data, std::size_t n)
{
    return send(native(s), data, static_cast<int>(n > SEND_LIMIT_BYTES ? SEND_LIMIT_BYTES : n), 0);
}

long send_gather(sock_t s, std::span<const std::string_view> parts)
{
    std::array<WSABUF, 3> buf;
    DWORD n = 0, sent = 0;
    for (std::size_t i = 0; i < parts.size(); i++, n++) {
        buf[n].buf = const_cast<char *>(parts[i].data()); /* WSABUF's own type: WSASend only reads it */
        buf[n].len = static_cast<ULONG>(parts[i].size());
    }
    if (WSASend(native(s), buf.data(), n, &sent, 0, nullptr, nullptr) != 0) return -1;
    return static_cast<long>(sent);
}

void end_sending(sock_t s) { shutdown(native(s), SD_SEND); }

long receive(sock_t s, char *buf, int n) { return recv(native(s), buf, n, 0); }

void set_nodelay(sock_t s)
{
    const int one = 1;
    setsockopt(native(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof one);
}

bool set_timeout(sock_t s, Timeout which, int milliseconds)
{
    DWORD ms = static_cast<DWORD>(milliseconds);
    return setsockopt(native(s), SOL_SOCKET, which == Timeout::receive ? SO_RCVTIMEO : SO_SNDTIMEO,
                      reinterpret_cast<const char *>(&ms), sizeof ms) == 0;
}

void no_inherit_socket(sock_t s) { SetHandleInformation(reinterpret_cast<HANDLE>(native(s)), HANDLE_FLAG_INHERIT, 0); }

void no_inherit_fd(int) {} /* the pipe is made _O_NOINHERIT; a second xppautX is started inheriting nothing */

bool make_log_pipe(std::array<int, 2> &fds) { return _pipe(fds.data(), LOG_PIPE_BYTES, _O_BINARY | _O_NOINHERIT) == 0; }

int dup_fd(int fd) { return _dup(fd); }

int dup2_fd(int from, int to) { return _dup2(from, to); }

long read_fd(int fd, char *buf, std::size_t n) { return _read(fd, buf, static_cast<unsigned>(n)); }

void random_bytes(std::span<unsigned char> out)
{
    for (unsigned char &b : out) {
        unsigned int v = 0;
        rand_s(&v);
        b = static_cast<unsigned char>(v);
    }
}

void end_process()
{
    /* the core thread may sit in exit() (at_exit waiting for a page that is
       gone: a stopped model's log), holding the C runtime's exit lock, which
       _exit() would wait on forever (W112) */
    TerminateProcess(GetCurrentProcess(), 0);
    _exit(0);
}

bool open_in_browser(const std::string &url)
{
    constexpr INT_PTR SHELL_ERROR_MAX = 32; /* ShellExecute returns an error code up to 32, success above it */
    return reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > SHELL_ERROR_MAX;
}

} // namespace xpp::http
