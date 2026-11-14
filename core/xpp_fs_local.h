#pragma once

/* The disk behind xpp_fs.h's FileSystem (W731): a thin class over xpp_io and xpp_files, so every rule
   they keep holds here: bounded reads, no link followed, a write that replaces its file only when
   complete. Nothing is decided here; each operation calls the module that owns it. The program's
   start makes one and passes it to the load. */

#include "xpp_fs.h"

namespace xpp {

class LocalFileSystem final : public FileSystem {
public:
    Result<std::string> read(std::string where, std::string_view path, std::size_t limit) override;
    std::optional<std::string> read_within(std::string_view path, std::string_view folder,
                                           std::span<const std::string> allowed_outside, std::size_t limit) override;
    PathCheck resolve_path(std::string_view path, std::string_view folder) override;
    Result<std::unique_ptr<Sink>> open_sink(std::string_view path, bool binary) override;
    bool destination_ok(std::string_view path) override;
    bool exists(std::string_view path) override;
    bool is_dir(std::string_view path) override;
    std::string absolute(std::string_view path) override;
    std::string working_dir() override;
    int change_dir(std::string_view path) override;
};

} // namespace xpp
