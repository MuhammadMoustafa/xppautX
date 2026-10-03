#ifndef XPP_WIN32_H
#define XPP_WIN32_H

/* xpp_win32.cpp, Windows only: the Windows API behind the few POSIX calls
   the core and the protocol front end use, in namespace xpp::win32
   (W109f). */
#ifdef _WIN32
#include <span>
#include <string>

namespace xpp::win32 {

/* up to buf.size() bytes of stdin, blocking; -1 at end of input or on an
   error */
int read_stdin(std::span<char> buf);
void binary_mode(int fd); /* no \r\n translation */
/* true when path is a symbolic link or another reparse point (a junction) */
bool path_is_link(const char *path);
/* rename from to to, replacing to when it exists (rename() does not);
   true on success */
bool move_over(const char *from, const char *to);
/* true while pid names a process that has not exited (xpp_files.cpp's
   sweep of stale scratch folders) */
bool process_running(unsigned long pid);
/* the system temp folder without its trailing backslash; empty when
   there is none (xpp_files.cpp's scratch folders) */
std::string temp_folder();
/* xppautX links -mwindows (a GUI-subsystem exe: no console pops up when
   Explorer or a file association starts it) so a command-line mode
   (--server, --silent, --version, --help, --browser, or any log to
   stderr) needs this before its first output: when stdout/stderr/stdin are
   not already a real pipe, file or null device (an inherited handle, e.g. --server piped
   by the VS Code extension or a test script, which is left alone), it
   attaches to a console-subsystem parent's console (AttachConsole) and
   reopens the three standard streams on it, so a plain terminal run (or
   `--version`) still prints; when there is no parent console (Explorer)
   this is a harmless no-op, matching the current no-console behaviour. */
void attach_console();

} // namespace xpp::win32
#endif
#endif
