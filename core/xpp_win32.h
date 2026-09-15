#ifndef XPP_WIN32_H
#define XPP_WIN32_H

/* xpp_win32.c, Windows only */
#ifdef _WIN32
/* up to n bytes of stdin, waiting wait_ms (< 0 blocks); 0 on timeout,
   -1 at end of input */
int xpp_read_stdin(char *buf, int n, int wait_ms);
void xpp_binary_mode(int fd); /* no \r\n translation */
#endif

#endif
