#ifndef XPP_SHA256_H
#define XPP_SHA256_H

/* SHA-256 (FIPS 180-4), xpp_sha256.cpp. The file endpoints (xpp_files.h)
   list each file's digest so the page can tell whether a file it is about
   to upload is already there with the same content. Pure: no I/O. C++
   only (xpp_files.cpp and its unit test use it). */

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace xpp {

class Sha256 {
public:
    Sha256() noexcept;
    void update(const void *data, std::size_t n) noexcept;
    /* the digest as 64 lowercase hex digits; call it once, last */
    std::string hex();

private:
    std::array<std::uint32_t, 8> h_;
    std::array<unsigned char, 64> block_{};
    unsigned long long bytes_ = 0; /* hashed so far */
    std::size_t fill_ = 0;         /* bytes waiting in block_ */
};

} // namespace xpp

#endif
