/* A column of the series event as JSON numbers or base64 float32
   (series_enc.h). Pure: no core state, no I/O. */
#include "series_enc.h"
#include "xpp_mem.h"
#include "json_number.h"
#include "xpp_io.h"

#include <bit>
#include <new>
#include <string>
#include <string_view>

namespace {

/* the floats' bytes as they are in memory, which is the encoding's own
   order (little-endian float32: every platform xppautX builds for) */
static_assert(std::endian::native == std::endian::little, "series_enc: f32 columns are little-endian");

void base64(const float *v, int n, std::string &s)
{
    s += '"';
    xpp::base64_append(s, std::string_view(reinterpret_cast<const char *>(v), sizeof(float) * static_cast<std::size_t>(n)));
    s += '"';
}

/* the shared JSON number writer (json_number.h), 9 digits: a float32
   round-trips exactly in that many significant digits */
void numbers(const float *v, int n, std::string &s)
{
    s.reserve(s.size() + 2 + 13 * static_cast<std::size_t>(n));
    s += '[';
    for (int i = 0; i < n; i++) {
        if (i) s += ',';
        xpp::json::json_append_number(s, v[i], 9);
    }
    s += ']';
}

} // namespace

void xpp_series_append(std::string &out, const float *v, int n, int f32) noexcept
{
    try {
        if (n < 0) n = 0;
        if (f32) base64(v, n, out);
        else numbers(v, n, out);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("encoding a series");
    }
}

