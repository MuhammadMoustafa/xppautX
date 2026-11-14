/* The platform primitives of xpp_io.cpp, each one call: lock a stream and read a
   character from it without the per-character lock, and the process id for the
   writer's temp name. Inline here, not in a file per platform, because the
   character read is the file readers' hot path (W681: a call across translation
   units would cost every character) and each is a line. Internal to xpp_io.cpp. */
#pragma once

#include <cstdio>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace xpp::io_platform {

#ifdef _WIN32
inline void lock_stream(std::FILE *fp) noexcept { _lock_file(fp); }
inline void unlock_stream(std::FILE *fp) noexcept { _unlock_file(fp); }
inline int get_char_unlocked(std::FILE *fp) noexcept { return _getc_nolock(fp); }
inline long long process_id() { return _getpid(); }
#else
inline void lock_stream(std::FILE *fp) noexcept { flockfile(fp); }
inline void unlock_stream(std::FILE *fp) noexcept { funlockfile(fp); }
inline int get_char_unlocked(std::FILE *fp) noexcept { return getc_unlocked(fp); }
inline long long process_id() { return getpid(); }
#endif

} // namespace xpp::io_platform
