/* A column of the series event as JSON numbers or base64 float32
   (series_enc.h). Pure: no core state, no I/O. */
#include "series_enc.h"
#include "xpp_mem.h"
#include "json_number.h"
#include "xpp_text.h"
#include "storage.h"

#include <bit>
#include <new>
#include <string>
#include <string_view>
#include <cstddef>
#include <vector>

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

/* n values, get(i) each, in the shortest text of the stored value (xpp::append_stored, the data files' digits): a float's when single */
template<class Get> void numbers(Get get, int n, bool single, std::string &s)
{
    s.reserve(s.size() + 2 + 13 * static_cast<std::size_t>(n));
    s += '[';
    for (int i = 0; i < n; i++) {
        if (i) s += ',';
        const double x = get(i);
        if (xpp::json::json_finite(x)) xpp::append_stored(s, x, single);
        else s += "null";
    }
    s += ']';
}

} // namespace

void xpp_series_append(std::string &out, const xpp::Column &v, int from, int n, int f32) noexcept
{
    try {
        if (n < 0) n = 0;
        if (!f32) numbers([&](int i) { return v.get(from + i); }, n, !v.is_double(), out);
        else if (v.float_data()) base64(v.float_data() + from, n, out);
        else base64(v.as_floats(from, n).data(), n, out);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("encoding a series");
    }
}

void xpp_series_append(std::string &out, const float *v, int n, int f32) noexcept
{
    try {
        if (n < 0) n = 0;
        if (f32) base64(v, n, out);
        else numbers([&](int i) { return static_cast<double>(v[i]); }, n, true, out);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("encoding a series");
    }
}
