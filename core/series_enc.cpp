/* A column of the series event as JSON numbers or base64 float32
   (series_enc.h). Pure: no core state, no I/O. */
#include "series_enc.h"
#include "xpp_mem.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

namespace {

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* the float's 4 bytes, least significant first */
void le_bytes(float f, unsigned char out[4])
{
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    for (int i = 0; i < 4; i++) out[i] = static_cast<unsigned char>(u >> (8 * i));
}

void base64(const float *v, int n, std::string &s)
{
    const std::size_t bytes = 4 * static_cast<std::size_t>(n);
    s.reserve(s.size() + 2 + (bytes + 2) / 3 * 4);
    s += '"';
    unsigned char q[3];
    int nq = 0;
    for (int i = 0; i < n; i++) {
        unsigned char b[4];
        le_bytes(v[i], b);
        for (unsigned char c : b) {
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

/* as ui_json.c's buf_float(b, z, 9): the same text for the same number */
void numbers(const float *v, int n, std::string &s)
{
    s.reserve(s.size() + 2 + 13 * static_cast<std::size_t>(n));
    s += '[';
    for (int i = 0; i < n; i++) {
        const double z = v[i];
        if (i) s += ',';
        if (z != z || z > 1e300 || z < -1e300) {
            s += "null";
        } else {
            char t[32];
            const int k = std::snprintf(t, sizeof t, "%.9g", z);
            if (k > 0) s.append(t, static_cast<std::size_t>(k));
        }
    }
    s += ']';
}

} // namespace

char *xpp_series_values(const float *v, int n, int f32, size_t *len)
{
    try {
        std::string s;
        if (n < 0) n = 0;
        if (f32) base64(v, n, s);
        else numbers(v, n, s);
        char *out = static_cast<char *>(xpp_malloc(s.size() + 1));
        if (!out) return nullptr;
        std::memcpy(out, s.c_str(), s.size() + 1);
        *len = s.size();
        return out;
    } catch (...) { /* std::bad_alloc: no exception may reach C */
        return nullptr;
    }
}
