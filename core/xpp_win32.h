#ifndef XPP_WIN32_H
#define XPP_WIN32_H

/* xpp_win32.c, Windows only */
#ifdef _WIN32
/* up to n bytes of stdin, blocking; -1 at end of input or on an error */
int xpp_read_stdin(char *buf, int n);
void xpp_binary_mode(int fd); /* no \r\n translation */
#endif

#endif
