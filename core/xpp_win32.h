#ifndef XPP_WIN32_H
#define XPP_WIN32_H
#ifdef __cplusplus
extern "C" {
#endif

/* xpp_win32.c, Windows only */
#ifdef _WIN32
/* up to n bytes of stdin, blocking; -1 at end of input or on an error */
int xpp_read_stdin(char *buf, int n);
void xpp_binary_mode(int fd); /* no \r\n translation */
/* 1 when path is a symbolic link or another reparse point (a junction) */
int xpp_path_is_link(const char *path);
/* rename from to to, replacing to when it exists (rename() does not); 0 on success */
int xpp_replace_file(const char *from, const char *to);
/* xppautX links -mwindows (a GUI-subsystem exe: no console pops up when
   Explorer or a file association starts it) so a command-line mode
   (--server, -silent, --script, --version, --help, --browser, or any log to
   stderr) needs this before its first output: when stdout/stderr/stdin are
   not already a real pipe or file (an inherited handle, e.g. --server piped
   by the VS Code extension or a test script, which is left alone), it
   attaches to a console-subsystem parent's console (AttachConsole) and
   reopens the three standard streams on it, so a plain terminal run (or
   `--version`) still prints; when there is no parent console (Explorer)
   this is a harmless no-op, matching the current no-console behaviour. */
void xpp_win32_attach_console(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
