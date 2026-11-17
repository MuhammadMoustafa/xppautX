/* W731: a model loads from files an in-memory FileSystem holds: the loader needs no disk, only the
   interface xpp_fs.h defines; and a save goes through a Sink that replaces the file at commit only. */
#include "xpptest.h"
#include "load_args.h"
#include "model_text.h"
#include "model_files.h"
#include "model_record.h"
#include "model.h"
#include "session.h"
#include "comline.h"
#include "model_load.h"
#include "set_load.h"
#include "xpp_fs.h"
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace {

/* the files are a map from name to text; every name is inside the (only) folder */
class MemoryFileSystem final : public xpp::FileSystem {
public:
    std::map<std::string, std::string> files;
    int sinks_committed = 0;

    xpp::Result<std::string> read_range(std::string where, std::string_view path, std::size_t offset, std::size_t length) override
    {
        const xpp::Result<std::string> whole = read(std::move(where), path, xpp::file_range_limit);
        if (!whole) return whole;
        return whole->substr(std::min(offset, whole->size()), length);
    }
    xpp::Result<std::string> read(std::string where, std::string_view path, std::size_t limit) override
    {
        const auto it = files.find(std::string(path));
        if (it == files.end()) return std::unexpected(xpp::Error{std::move(where), "no such file", xpp::Place{std::string(path)}});
        if (it->second.size() > limit) return std::unexpected(xpp::Error{std::move(where), "too long", xpp::Place{std::string(path)}});
        return it->second;
    }
    std::optional<std::string> read_within(std::string_view path, std::string_view, std::span<const std::string>, std::size_t limit) override
    {
        const xpp::Result<std::string> bytes = read("", path, limit);
        if (!bytes) return std::nullopt;
        return *bytes;
    }
    xpp::PathCheck resolve_path(std::string_view path, std::string_view) override { return {xpp::PathKind::inside, std::string(path)}; }
    xpp::Result<std::unique_ptr<xpp::Sink>> open_sink(std::string_view path, bool, const xpp::Place &) override
    {
        return std::unique_ptr<xpp::Sink>(std::make_unique<MemorySink>(*this, std::string(path)));
    }
    xpp::Result<> check_destination(std::string_view, const xpp::Place &) override { return {}; }
    bool destination_ok(std::string_view) override { return true; }
    bool exists(std::string_view path) override { return files.contains(std::string(path)); }
    bool is_dir(std::string_view) override { return false; }
    xpp::Result<unsigned long long> size_of(std::string_view path) override
    {
        const auto it = files.find(std::string(path));
        if (it == files.end()) return std::unexpected(xpp::Error{"file size", "no such file", xpp::Place{std::string(path)}});
        return it->second.size();
    }
    bool dir_writable(std::string_view) override { return true; }
    int remove_file(std::string_view path) override { return files.erase(std::string(path)) ? 0 : 1; }
    xpp::Result<std::string> make_temp_dir(std::string_view) override { return std::string("memory-temp"); }
    xpp::Result<std::optional<std::string>> read_model_state(std::string_view, std::string_view model_folder, std::string_view name, std::size_t) override
    {
        const auto it = files.find(xpp::files::join(xpp::files::join(model_folder, xpp::files::MODEL_STATE_FOLDER), name));
        return it == files.end() ? std::optional<std::string>() : std::optional<std::string>(it->second);
    }
    void remove_temp_dir(std::string_view) override {}
    std::string config_path(std::string_view name) override { return "config/" + std::string(name); }
    xpp::Result<std::optional<std::string>> read_config(std::string_view, std::string_view name, std::size_t) override
    {
        const auto it = files.find(config_path(name));
        return it == files.end() ? std::optional<std::string>() : std::optional<std::string>(it->second);
    }
    xpp::Result<> write_config(std::string_view, std::string_view name, std::string_view bytes) override
    {
        files[config_path(name)] = std::string(bytes);
        return {};
    }
    xpp::Result<> remove_config(std::string_view, std::string_view name) override
    {
        files.erase(config_path(name));
        return {};
    }
    bool serving_reads() override { return false; }
    std::string absolute(std::string_view path) override { return std::string(path); }
    std::string working_dir() override { return "memory"; }
    int change_dir(std::string_view) override { return 0; }

private:
    class MemorySink final : public xpp::Sink {
    public:
        MemorySink(MemoryFileSystem &fs, std::string path) : fs_(fs), path_(std::move(path)) {}
        xpp::Result<> write(std::string_view bytes) override
        {
            pending_.append(bytes);
            return {};
        }
        xpp::Result<> commit() override
        {
            fs_.files[path_] = std::move(pending_);
            fs_.sinks_committed++;
            return {};
        }
        void abort() override { pending_.clear(); }
        const std::string &path() const override { return path_; }

    private:
        MemoryFileSystem &fs_;
        std::string path_, pending_;
    };
};

} // namespace

TEST_CASE("test_fs")
{
    MemoryFileSystem memory;
    memory.files["mem.odex"] = versioned("par a=0.5\nx' = -a*x\ninit x=1\n");
    xpp::Result<xpp::CommandLine> line = xpp::parse_command_line({"test_fs", "mem.odex"});
    CHECK(line.has_value());
    if (!line) TEST_REPORT("filesystem");
    /* the model is read from the in-memory file system: no such file is on the disk */
    const xpp::Loaded loaded = xpp::load_model(memory, std::move(*line), 1);
    CHECK(loaded.has_value());
    if (!loaded) TEST_REPORT("filesystem");
    xpp::Session &s = **loaded;
    CHECK(s.model().fs == &memory);
    CHECK(s.model().neq == 1 && s.model().nupar == 1);
    CHECK(s.model().record->files.size() == 1 && s.model().record->files[0].name == "mem.odex");

    /* a file that is not there is the file system's error, naming it */
    xpp::Result<xpp::CommandLine> missing = xpp::parse_command_line({"test_fs", "gone.odex"});
    CHECK(missing.has_value());
    const xpp::Loaded none = xpp::load_model(memory, std::move(*missing), 1);
    CHECK(!none.has_value());

    /* a sink replaces its file at commit only */
    memory.files["out.txt"] = "before";
    xpp::Result<std::unique_ptr<xpp::Sink>> sink = memory.open_sink("out.txt", false, {});
    CHECK(sink.has_value());
    CHECK((*sink)->write("after").has_value());
    CHECK(memory.files["out.txt"] == "before");
    CHECK((*sink)->commit().has_value() && memory.files["out.txt"] == "after");
    TEST_REPORT("filesystem");
}
