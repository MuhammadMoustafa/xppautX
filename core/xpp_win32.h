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
#endif

#ifdef __cplusplus
}
#endif
#endif
