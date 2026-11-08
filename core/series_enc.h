#pragma once

#include <string>

#include "storage.h"

/* How the series event (core/ui_json.cpp, docs/protocol.md "The plot as
   data") writes a column of stored values.

   The values v[from..from+n), as the text of one JSON value, appended to
   out ("[]" when memory runs out):
   - f32 0: an array of numbers, each the shortest text that reads back as
     exactly the stored value (a float's under a single store, a double's
     under a double one: the same digits as the data files); null for NaN
     and infinities;
   - f32 1: a string, the base64 (RFC 4648, padded) of the values as
     little-endian IEEE float32, 4 bytes each, whatever the host's byte
     order, each rounded to a float when the store holds doubles; NaN and
     infinities travel as they are.

   series_enc.cpp; nothing escapes it. */
void xpp_series_append(std::string &out, const xpp::Column &v, int from, int n, int f32) noexcept;
/* the same for n floats a drawing's data holds (a nullcline, a direction field, marks): always a float's text */
void xpp_series_append(std::string &out, const float *v, int n, int f32) noexcept;
