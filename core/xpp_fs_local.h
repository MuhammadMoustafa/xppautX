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
    Result<std::string> read_range(std::string where, std::string_view path, std::size_t offset, std::size_t length) override;
    std::optional<std::string> read_within(std::string_view path, std::string_view folder,
                                           std::span<const std::string> allowed_outside, std::size_t limit) override;
    PathCheck resolve_path(std::string_view path, std::string_view folder) override;
    Result<std::unique_ptr<Sink>> open_sink(std::string_view path, bool binary, const Place &at) override;
    Result<> check_destination(std::string_view path, const Place &at) override;
    bool destination_ok(std::string_view path) override;
    bool exists(std::string_view path) override;
    bool is_dir(std::string_view path) override;
    Result<unsigned long long> size_of(std::string_view path) override;
    bool dir_writable(std::string_view dir) override;
    int remove_file(std::string_view path) override;
    Result<> move_file(std::string_view from, std::string_view to, const Place &at) override;
    Result<std::string> make_temp_dir(std::string_view model_folder) override;
    Result<std::optional<std::string>> read_model_state(std::string_view where, std::string_view model_folder, std::string_view name,
                                                        std::size_t limit) override;
    Result<> write_model_state(std::string_view where, std::string_view model_folder, std::string_view name, std::string_view bytes) override;
    void remove_temp_dir(std::string_view dir) override;
    bool is_download(std::string_view path) override;
    void offer_download(std::string_view folder, std::string_view name) override;
    std::string config_path(std::string_view name) override;
    Result<std::optional<std::string>> read_config(std::string_view where, std::string_view name, std::size_t limit) override;
    Result<> write_config(std::string_view where, std::string_view name, std::string_view bytes) override;
    Result<> remove_config(std::string_view where, std::string_view name) override;
    bool serving_reads() override;
    std::string absolute(std::string_view path) override;
    std::string working_dir() override;
    int change_dir(std::string_view path) override;
};

} // namespace xpp
