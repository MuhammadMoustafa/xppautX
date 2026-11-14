/* core/xpp_fs_local.h's implementation: each operation is the files module's own. */
#include "xpp_fs_local.h"

#include <utility>

#include "xpp_files.h"
#include "xpp_io.h"

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

} // namespace

Result<std::string> LocalFileSystem::read(std::string where, std::string_view path, std::size_t limit)
{
    return read_file_bytes(std::move(where), path, limit);
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

Result<std::unique_ptr<Sink>> LocalFileSystem::open_sink(std::string_view path, bool binary)
{
    auto sink = std::make_unique<WriterSink>(binary ? Writer::binary(path) : Writer(path));
    if (!sink->open()) return std::unexpected(files::write_error("save", path));
    return std::unique_ptr<Sink>(std::move(sink));
}

bool LocalFileSystem::destination_ok(std::string_view path) { return files::output_status(path) == XPP_FILES_OK; }

bool LocalFileSystem::exists(std::string_view path) { return files::exists(path); }
bool LocalFileSystem::is_dir(std::string_view path) { return files::is_dir(path); }
std::string LocalFileSystem::absolute(std::string_view path) { return files::absolute(path); }
std::string LocalFileSystem::working_dir() { return files::working_dir(); }
int LocalFileSystem::change_dir(std::string_view path) { return files::change_dir(path); }

} // namespace xpp
