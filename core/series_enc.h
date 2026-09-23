#ifndef SERIES_ENC_H
#define SERIES_ENC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* How the series event (core/ui_json.c, docs/protocol.md "The plot as
   data") writes a column of stored values.

   The values v[0..n) as the text of one JSON value, in a malloc'd,
   NUL-terminated string whose length goes to *len (NULL when memory runs
   out):
   - f32 0: an array of numbers printed with 9 significant digits, which
     read back as exactly the stored floats; null for NaN and infinities;
   - f32 1: a string, the base64 (RFC 4648, padded) of the values as
     little-endian IEEE float32, 4 bytes each, whatever the host's byte
     order; NaN and infinities travel as they are.

   series_enc.cpp; C++ with a C API, nothing escapes it. */
char *xpp_series_values(const float *v, int n, int f32, size_t *len);

#ifdef __cplusplus
}
#endif
#endif
