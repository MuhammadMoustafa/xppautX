#ifndef XPP_SHA256_H
#define XPP_SHA256_H

#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* SHA-256 (FIPS 180-4), xpp_sha256.cpp. The file endpoints (xpp_files.h)
   list each file's digest so the page can tell whether a file it is about
   to upload is already there with the same content. Pure: no I/O. */

typedef struct {
    unsigned int h[8];
    unsigned char block[64];
    unsigned long long bytes; /* hashed so far */
    size_t fill;              /* bytes waiting in block */
} XppSha256;

void xpp_sha256_init(XppSha256 *c);
void xpp_sha256_update(XppSha256 *c, const void *data, size_t n);
/* the digest as 64 lowercase hex digits and a NUL */
void xpp_sha256_hex(XppSha256 *c, char out[65]);

#ifdef __cplusplus
}
#endif
#endif
