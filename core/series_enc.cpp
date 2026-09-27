/* A column of the series event as JSON numbers or base64 float32
   (series_enc.h). Pure: no core state, no I/O. */
#include "series_enc.h"
#include "json_number.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* the float's 4 bytes, least significant first */
std::array<unsigned char, 4> le_bytes(float f)
{
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    std::array<unsigned char, 4> out;
    for (int i = 0; i < 4; i++) out[i] = static_cast<unsigned char>(u >> (8 * i));
    return out;
}

void base64(const float *v, int n, std::string &s)
{
    const std::size_t bytes = 4 * static_cast<std::size_t>(n);
    s.reserve(s.size() + 2 + (bytes + 2) / 3 * 4);
    s += '"';
    std::array<unsigned char, 3> q{};
    int nq = 0;
    for (int i = 0; i < n; i++) {
        for (unsigned char c : le_bytes(v[i])) {
            q[nq++] = c;
            if (nq == 3) {
                s += B64[q[0] >> 2];
                s += B64[(q[0] & 3) << 4 | q[1] >> 4];
                s += B64[(q[1] & 15) << 2 | q[2] >> 6];
                s += B64[q[2] & 63];
                nq = 0;
            }
        }
    }
    if (nq == 1) {
        s += B64[q[0] >> 2];
        s += B64[(q[0] & 3) << 4];
        s += "==";
    } else if (nq == 2) {
        s += B64[q[0] >> 2];
        s += B64[(q[0] & 3) << 4 | q[1] >> 4];
        s += B64[(q[1] & 15) << 2];
        s += '=';
    }
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
        std::string s;
        if (n < 0) n = 0;
        if (f32) base64(v, n, s);
        else numbers(v, n, s);
        out += s;
    } catch (...) { /* std::bad_alloc */
        try {
            out += "[]";
        } catch (...) {
        }
    }
}

