/* xpp_http_internal.h on Linux and macOS: BSD sockets and the POSIX calls the two share.
   The browser opener is xpp_http_linux.cpp's or xpp_http_mac.cpp's. */
#include "xpp_http_internal.h"
#include "xpp_process.h"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

/* BSD and macOS have no MSG_NOSIGNAL; SIGPIPE is ignored in start_sockets */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace xpp::http {

bool start_sockets()
{
    signal(SIGPIPE, SIG_IGN);
    return true;
}

sock_t listen_loopback(int port, int &bound_port)
{
    struct sockaddr_in addr{};
    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return INVALID_SOCK;
    no_inherit_socket(listener);
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof one);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* this machine only */
    addr.sin_port = htons(static_cast<unsigned short>(port));
    constexpr int LISTEN_BACKLOG = 16; /* queue a burst of browser connections while accept dispatches them */
    if (bind(listener, reinterpret_cast<struct sockaddr *>(&addr), sizeof addr) != 0 || listen(listener, LISTEN_BACKLOG) != 0) {
        close(listener);
        return INVALID_SOCK;
    }
    socklen_t len = sizeof addr;
    getsockname(listener, reinterpret_cast<struct sockaddr *>(&addr), &len);
    bound_port = ntohs(addr.sin_port);
    return listener;
}

sock_t accept_connection(sock_t listener) { return accept(static_cast<int>(listener), nullptr, nullptr); }

void close_socket(sock_t s) { close(static_cast<int>(s)); }

long send_some(sock_t s, const char *data, std::size_t n) { return send(static_cast<int>(s), data, n, MSG_NOSIGNAL); }

long send_gather(sock_t s, std::span<const std::string_view> parts)
{
    std::array<iovec, 3> buf;
    msghdr m{};
    for (std::size_t i = 0; i < parts.size(); i++, m.msg_iovlen++) {
        buf[m.msg_iovlen].iov_base = const_cast<char *>(parts[i].data()); /* iovec's own type: sendmsg only reads it */
        buf[m.msg_iovlen].iov_len = parts[i].size();
    }
    m.msg_iov = buf.data();
    return sendmsg(static_cast<int>(s), &m, MSG_NOSIGNAL);
}

void end_sending(sock_t s) { shutdown(static_cast<int>(s), SHUT_WR); }

long receive(sock_t s, char *buf, int n) { return recv(static_cast<int>(s), buf, static_cast<std::size_t>(n), 0); }

void set_nodelay(sock_t s)
{
    const int one = 1;
    setsockopt(static_cast<int>(s), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&one), sizeof one);
}

bool set_timeout(sock_t s, Timeout which, int milliseconds)
{
    struct timeval tv;
    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = (milliseconds % 1000) * 1000;
    return setsockopt(static_cast<int>(s), SOL_SOCKET, which == Timeout::receive ? SO_RCVTIMEO : SO_SNDTIMEO,
                      reinterpret_cast<const char *>(&tv), sizeof tv) == 0;
}

void no_inherit_socket(sock_t s) { fcntl(static_cast<int>(s), F_SETFD, FD_CLOEXEC); }

void no_inherit_fd(int fd) { fcntl(fd, F_SETFD, FD_CLOEXEC); }

bool make_log_pipe(std::array<int, 2> &fds) { return pipe(fds.data()) == 0; }

int dup_fd(int fd) { return dup(fd); }

int dup2_fd(int from, int to) { return dup2(from, to); }

long read_fd(int fd, char *buf, std::size_t n) { return read(fd, buf, n); }

void random_bytes(std::span<unsigned char> out)
{
    std::ifstream f("/dev/urandom", std::ios::binary);
    if (!f.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(out.size()))) {
        srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));
        for (unsigned char &b : out) b = static_cast<unsigned char>(rand());
    }
}

void end_process() { _exit(0); }

bool open_in_browser(const std::string &url)
{
    /* No shell: the URL is one argv entry of the opener, whatever characters it holds (xpp_process.h). */
    std::vector<std::string> argv{browser_opener};
    /* WSL: rundll32.exe takes the URL as one command-line argument and starts the Windows default browser
       with no command interpreter (cmd.exe /c start would parse & ^ % in it); it exits 0 when it started */
    if (getenv("WSL_DISTRO_NAME")) argv = {"rundll32.exe", "url.dll,FileProtocolHandler"};
    argv.push_back(url);
    return xpp::process::spawn(argv, xpp::process::Wait::until_it_ends).has_value();
}

} // namespace xpp::http
