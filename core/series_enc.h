#ifndef SERIES_ENC_H
#define SERIES_ENC_H

#include <string>

/* How the series event (core/ui_json.cpp, docs/protocol.md "The plot as
   data") writes a column of stored values.

   The values v[0..n) as the text of one JSON value, appended to out
   ("[]" when memory runs out):
   - f32 0: an array of numbers printed with 9 significant digits, which
     read back as exactly the stored floats; null for NaN and infinities;
   - f32 1: a string, the base64 (RFC 4648, padded) of the values as
     little-endian IEEE float32, 4 bytes each, whatever the host's byte
     order; NaN and infinities travel as they are.

   series_enc.cpp; nothing escapes it. */
void xpp_series_append(std::string &out, const float *v, int n, int f32) noexcept;
#endif
