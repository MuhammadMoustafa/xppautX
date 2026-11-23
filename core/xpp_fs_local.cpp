/* core/xpp_fs_local.h's implementation: each operation is the files module's own. */
#include "xpp_fs_local.h"

#include <utility>

#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_ui.h"

namespace xpp {

namespace {

/* the Writer's temp-then-rename write, behind Sink */
class WriterSink final : public Sink {
public:
    explicit WriterSink(Writer w) : w_(std::move(w)) {}
    Result<> write(std::string_view bytes) override
    {
        if (w_.write(bytes)) return {};
        return std::unexpected(files::write_error("save", w_.path()));
    }
    Result<> commit() override { return w_.commit(); }
    void abort() override { w_.abort(); }
    const std::string &path() const override { return w_.path(); }
    bool open() const { return static_cast<bool>(w_); }

private:
    Writer w_;
};

/* failure at the place the caller named (a model line, a recording's step), unless reads are served: then the
   system's own place stays */
Error placed_at(Error failure, const Place &at)
{
    if ((!at.file.empty() || !at.source.empty()) && !files::serving_reads()) failure.place = at;
    return failure;
}

} // namespace

Result<std::string> LocalFileSystem::read(std::string where, std::string_view path, std::size_t limit)
{
    return read_file_bytes(std::move(where), path, limit);
}

Result<std::string> LocalFileSystem::read_range(std::string where, std::string_view path, std::size_t offset, std::size_t length)
{
    if (length > file_range_limit) return std::unexpected(files::open_error(std::move(where), path, "is read in smaller pieces"));
    return read_file_range(std::move(where), path, offset, length);
}

std::optional<std::string> LocalFileSystem::read_within(std::string_view path, std::string_view folder,
                                                        std::span<const std::string> allowed_outside, std::size_t limit)
{
    UniqueFile file(files::open_read_within(path, folder, allowed_outside));
    std::string bytes;
    if (!read_bytes(file.get(), bytes, limit)) return std::nullopt;
    return bytes;
}

PathCheck LocalFileSystem::resolve_path(std::string_view path, std::string_view folder)
{
    return files::resolve_path(path, folder);
}

Result<std::unique_ptr<Sink>> LocalFileSystem::open_sink(std::string_view path, bool binary, const Place &at)
{
    auto sink = std::make_unique<WriterSink>(binary ? Writer::binary(path) : Writer(path));
    if (!sink->open()) return std::unexpected(placed_at(files::write_error("save", path), at));
    return std::unique_ptr<Sink>(std::move(sink));
}

Result<> LocalFileSystem::check_destination(std::string_view path, const Place &at)
{
    const int status = files::output_status(path);
    if (status == XPP_FILES_OK) return {};
    Error failure = status == XPP_FILES_IO ? files::write_error("save", path)
                                           : Error{"save", xpp::format("Cannot save {}: {}", path, files::status_text(status)), command_place()};
    /* A replay's path guard must stop the step before later commands run,
       even when the basename was rejected before a Sink could open it. */
    files::write_path_ok(path, true);
    return std::unexpected(placed_at(std::move(failure), at));
}

bool LocalFileSystem::destination_ok(std::string_view path) { return files::output_status(path) == XPP_FILES_OK; }

bool LocalFileSystem::exists(std::string_view path) { return files::exists(path); }
bool LocalFileSystem::is_dir(std::string_view path) { return files::is_dir(path); }
Result<unsigned long long> LocalFileSystem::size_of(std::string_view path)
{
    unsigned long long size = 0;
    const int status = files::size_of(path, size);
    if (status != XPP_FILES_OK) return std::unexpected(Error{"file size", files::status_text(status), Place{std::string(path)}});
    return size;
}

bool LocalFileSystem::dir_writable(std::string_view dir) { return files::dir_writable(dir); }
int LocalFileSystem::remove_file(std::string_view path) { return files::remove(path); }
Result<> LocalFileSystem::move_file(std::string_view from, std::string_view to, const Place &at)
{
    if (files::replace_file(from, to) == 0) return {};
    return std::unexpected(placed_at(files::write_error("save", to), at));
}
Result<std::string> LocalFileSystem::make_temp_dir(std::string_view model_folder) { return files::make_temp_dir(model_folder); }
void LocalFileSystem::remove_temp_dir(std::string_view dir) { files::remove_temp_dir(dir); }
bool LocalFileSystem::is_download(std::string_view path) { return files::is_download(path); }
void LocalFileSystem::offer_download(std::string_view folder, std::string_view name) { files::offer_download(folder, name); }
Result<std::optional<std::string>> LocalFileSystem::read_model_state(std::string_view where, std::string_view model_folder,
                                                                    std::string_view name, std::size_t limit)
{
    return read_model_state_file(where, model_folder, name, limit);
}
Result<> LocalFileSystem::write_model_state(std::string_view where, std::string_view model_folder, std::string_view name, std::string_view bytes)
{
    return write_model_state_file(where, model_folder, name, bytes);
}
std::string LocalFileSystem::config_path(std::string_view name) { return files::config_path(name); }

Result<std::optional<std::string>> LocalFileSystem::read_config(std::string_view where, std::string_view name, std::size_t limit)
{
    return read_config_file(where, name, limit);
}

Result<> LocalFileSystem::write_config(std::string_view where, std::string_view name, std::string_view bytes)
{
    return write_config_file(where, name, bytes);
}

Result<> LocalFileSystem::remove_config(std::string_view where, std::string_view name)
{
    const std::string file = files::config_path(name);
    if (file.empty()) return std::unexpected(no_config_folder(where, name));
    if (files::exists(file) && files::remove(file) != 0)
        return fail(std::string(where), "cannot be reset: the file cannot be removed", Place{file});
    return {};
}

bool LocalFileSystem::serving_reads() { return files::serving_reads(); }

std::string LocalFileSystem::absolute(std::string_view path) { return files::absolute(path); }
std::string LocalFileSystem::working_dir() { return files::working_dir(); }
int LocalFileSystem::change_dir(std::string_view path) { return files::change_dir(path); }

} // namespace xpp
