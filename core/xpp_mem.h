#pragma once

/* xpp_mem.cpp. Every core allocation is C++ now (std::vector, std::string,
   std::unique_ptr, ...): the raw allocator this header used to declare
   (xpp_malloc/xpp_calloc/xpp_realloc/xpp_strdup/xpp_free, the
   XPP_MEM_FAIL_AT test hook, XPP_MEM_INIT, xpp_mem_stats()) was retired
   at W48, once tabular.cpp -- its last caller -- moved to a std::vector.
   This owner checks allocation extent arithmetic and provides the loud,
   final exit for allocation failures that cannot be recovered.

   C++ code whose std::string or std::vector could not allocate (it caught
   std::bad_alloc where no exception may pass: a callback a C library or
   the system calls, AGENTS.md "C and C++") ends the program the same way
   xpp_malloc used to: an ERROR
   "out of memory <what>", then exit(1).

   The core never calls the C library's malloc/calloc/realloc/strdup/free
   directly (ZeroedFloats, below, is the one owner of calloc); the exceptions are memory that crosses a boundary with a
   library: dirname()
   (auto_nox.cpp, aniparse.cpp) returns a pointer into its argument; getcwd()
   (xpp_files_dir.cpp) fills the caller's buffer). A new exception
   (getline, scandir, realpath(p, NULL), asprintf ...) is listed here and
   in tools/alloccheck.sh, which fails any other direct call
   (tools/sourcecheck.sh runs it).

   pocketfft (third_party/pocketfft, compiled into xpp_math.cpp) allocates
   its scratch memory with the C library's malloc/free inside its own
   header and hands none of it out; alloccheck reads core/ only. So does
   miniz (third_party/miniz, its own object, used only by xpp_zip.cpp):
   everything it produces comes back through callbacks into std::string.
   TinyCC (third_party/tinycc, its own object, used only by xpp_tcc.cpp)
   owns its allocations and executable pages until tcc_delete; the adapter's
   RAII state releases them, and returned function addresses are borrowed.

   make asan (build/asan, AddressSanitizer + UBSan) and tools/asancheck.sh
   check that nothing leaks. */

#include <string_view>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace xpp {
/* Allocation extents: reject an unrepresentable factor, sum or product
   before arithmetic wraps. Callers supply their computation's diagnostic. */
template<class... T> std::size_t size_product(T... factors)
{
  std::size_t size = 1;
  const auto multiply = [&](auto factor) {
    if (!std::in_range<std::size_t>(factor) ||
        (factor != 0 && size > std::numeric_limits<std::size_t>::max() / static_cast<std::size_t>(factor)))
      throw std::length_error("allocation size overflow");
    size *= static_cast<std::size_t>(factor);
  };
  (multiply(factors), ...);
  return size;
}
template<class... T> std::size_t size_sum(T... terms)
{
  std::size_t size = 0;
  const auto add = [&](auto term) {
    if (!std::in_range<std::size_t>(term) ||
        size > std::numeric_limits<std::size_t>::max() - static_cast<std::size_t>(term))
      throw std::length_error("allocation size overflow");
    size += static_cast<std::size_t>(term);
  };
  (add(terms), ...);
  return size;
}
/* n floats, all zero, whose memory the system hands over untouched: pages
   are mapped and zero-filled only when first written, so a big block that
   a run fills slowly costs only what it uses (the data store's columns,
   storage.cpp: 16 MB for a 200-equation model, of which a short run
   writes a few rows). Move-only; the constructor throws std::bad_alloc
   when there is no memory. The one place that calls the C library's
   calloc (tools/alloccheck.sh): a std::vector fills what it makes. */
class ZeroedFloats {
public:
  ZeroedFloats() noexcept = default;
  explicit ZeroedFloats(std::size_t n);
  ZeroedFloats(ZeroedFloats &&other) noexcept : p_(std::exchange(other.p_, nullptr)) {}
  ZeroedFloats &operator=(ZeroedFloats &&other) noexcept
  {
    std::swap(p_, other.p_);
    return *this;
  }
  ~ZeroedFloats();
  float *data() const noexcept { return p_; }
private:
  float *p_ = nullptr;
};

/* what says what was being built ("reading a command"): the ERROR is
   "out of memory <what>". It allocates nothing itself. */
[[noreturn]] void out_of_memory(std::string_view what) noexcept;
/* the same on a thread of its own (the protocol's readers, xpp_inbox.cpp
   and xpp_http.cpp) or with a lock held that an exit handler takes
   (xpp_http's at_exit): the ERROR, then std::_Exit(1), which runs no exit
   handler and destroys no static that another thread may be using. */
[[noreturn]] void out_of_memory_now(std::string_view what) noexcept;
} // namespace xpp

