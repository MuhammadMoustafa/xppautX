/* core/xpp_io.h's implementation: the file handles (LineReader,
   Writer) and xpp::format/xpp::vformat. Nothing here throws:
   a failed allocation is xpp::out_of_memory's. */
#include "xpp_io.h"
#include <cstring>
#include "xpp_files.h"
#include "xpp_log.h"
#include "xpp_mem.h"

#include <iterator>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <span>

#include "xpp_io_internal.h"

/* ===================================================================
   The file half: line reader, token reader, writer (issue: W11 step 3,
   W7b). See xpp_io.h for the contract. */

/* A reader reads the stream it opened itself, closed with the reader */
struct xpp::LineReader::State {
    xpp::UniqueFile owned;
    std::string line;
    std::FILE *fp() const noexcept { return owned.get(); }
};

/* the temp file being written, and the file it replaces at commit */
struct xpp::Writer::State {
    xpp::UniqueFile fp;
    std::string tmp, target;
};

namespace {

/* ---- lines -------------------------------------------------------- */

/* The stream is locked once per line and read without the lock per character (W681: a job's .d of millions of lines
   spent most of its renumbering in fgetc's per-character lock). */
class StreamLock {
public:
    explicit StreamLock(std::FILE *fp) noexcept : fp_(fp) { xpp::io_platform::lock_stream(fp_); }
    ~StreamLock() { xpp::io_platform::unlock_stream(fp_); }
    StreamLock(const StreamLock &) = delete;
    StreamLock &operator=(const StreamLock &) = delete;
private:
    std::FILE *fp_;
};

int get_char_locked(std::FILE *fp) noexcept { return xpp::io_platform::get_char_unlocked(fp); }

/* Reads one line a character at a time, growing `line` without limit; strips a
   trailing \r so \n and \r\n both end a line. Returns false only at a
   real end of file (nothing at all was read) so an unterminated final
   line still comes back once, not twice. */
bool read_line(std::FILE *fp, std::string &line)
{
    line.clear();
    int c;
    bool any = false;
    const StreamLock held(fp);
    while ((c = get_char_locked(fp)) != EOF) {
        any = true;
        if (c == '\n') break;
        line.push_back(static_cast<char>(c));
    }
    if (!any) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

/* ---- writer's temp file ---------------------------------------------- */

std::atomic<unsigned> writer_serial{0};

long long writer_pid() { return xpp::io_platform::process_id(); }

/* A handle's new state (Ptr: the handle's std::unique_ptr<State, Free>),
   over the file path, opened here (closed with the reader), or over the
   stream fp the caller keeps; nullptr when path cannot be opened, fp is
   none, or there is no memory */
template <class Ptr>
Ptr new_state() noexcept
{
    try {
        return Ptr(new typename Ptr::element_type);
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

template <class Ptr>
Ptr opened_state(std::string_view path) noexcept
{
    xpp::UniqueFile fp(xpp::files::open_stream(path, "r"));
    if (!fp) return nullptr;
    Ptr st = new_state<Ptr>();
    if (st) st->owned = std::move(fp);
    return st;
}

template <class Ptr>
Ptr writer_open(std::string_view path, bool binary)
{
    try {
        if (path.empty() || !xpp::files::write_path_ok(path, true) || !xpp::files::prepare_write_folder(path)) return nullptr;
        Ptr w = new_state<Ptr>();
        if (!w) throw std::bad_alloc();
        w->target = path;
        const auto [dir, base] = xpp::files::split_path(path);
        /* exclusive: a name some other run left behind is skipped */
        constexpr int temp_attempts = 100; /* skip exclusive names left by older processes */
        for (int tries = 0; tries < temp_attempts && !w->fp; tries++) {
            w->tmp = std::format("{}{}.{}.tmp-{}-{}", dir, dir.empty() ? "" : "/", base, writer_pid(),
                                 writer_serial.fetch_add(1, std::memory_order_relaxed));
            w->fp.reset(xpp::files::create_new(w->tmp, binary));
            if (!w->fp && errno != EEXIST) break;
        }
        if (!w->fp) return nullptr;
        return w;
    } catch (...) {
        xpp::log(XPP_LOG_ERROR, "out of memory opening {} for writing\n", path);
        return nullptr;
    }
}

/* the temp file closed and removed (Windows cannot remove an open file) */
template <class Ptr>
void writer_discard(Ptr w)
{
    w->fp.reset();
    xpp::files::remove(w->tmp);
}

} // namespace

namespace xpp {

/* ---- LineReader ------------------------------------------------------ */

void LineReader::Free::operator()(State *s) const noexcept { delete s; }

LineReader::LineReader(std::string_view path) noexcept : state_(opened_state<decltype(state_)>(path)) {}

std::optional<std::string_view> LineReader::next()
{
    if (!state_) return std::nullopt;
    State &r = *state_;
    if (!read_line(r.fp(), r.line)) return std::nullopt;
    return std::string_view(r.line);
}

std::string LineReader::line(int n)
{
    for (int i = 1; std::optional<std::string_view> l = next(); i++)
        if (i == n) return std::string(*l);
    return {};
}

/* ---- Writer ----------------------------------------------------------- */

void Writer::Free::operator()(State *s) const noexcept { delete s; }

Writer::~Writer() { abort(); }

Writer &Writer::operator=(Writer &&o) noexcept
{
    if (this != &o) {
        abort();
        path_ = std::move(o.path_);
        state_ = std::move(o.state_);
    }
    return *this;
}

Writer::Writer(std::string_view path) noexcept : path_(path), state_(writer_open<decltype(state_)>(path, false)) {}

Writer Writer::binary(std::string_view path) noexcept
{
    Writer w;
    w.path_ = path;
    w.state_ = writer_open<decltype(w.state_)>(path, true);
    return w;
}

FILE *Writer::file() const noexcept { return state_ ? state_->fp.get() : nullptr; }

Result<> Writer::commit() noexcept
{
    if (!state_) return fail("writer", "cannot be written: no open temporary file", Place{path_});
    decltype(state_) w = std::move(state_);
    const bool failed = std::ferror(w->fp.get()) != 0;
    int closed = std::fclose(w->fp.release()); /* its result: a write that failed at the flush */
    if (failed || closed != 0) {
        const Error error{"writer", "cannot be written: the write failed", Place{w->target}};
        writer_discard(std::move(w));
        return std::unexpected(error);
    }
    if (files::replace_file(w->tmp, w->target) != 0) {
        const Error error = files::write_error("writer", w->target);
        writer_discard(std::move(w));
        return std::unexpected(error);
    }
    return {};
}

void Writer::abort() noexcept
{
    if (state_) writer_discard(std::move(state_));
}

Error no_config_folder(std::string_view where, std::string_view name)
{
    return Error{std::string(where),
                 xpp::format("there is no per-user config folder (no HOME or APPDATA, or {} is not an absolute path): set {} to one",
                             files::CONFIG_DIR_ENV, files::CONFIG_DIR_ENV),
                 Place{std::string(name)}};
}

namespace {

/* the plain file `file` inside `anchor` (no link below it) as text, nullopt when it is not there */
Result<std::optional<std::string>> read_plain_within(std::string_view where, std::string_view anchor, const std::string &file, std::size_t limit)
{
    if (!files::exists(file)) return std::optional<std::string>();
    UniqueFile f(files::open_read_within(file, anchor, {}));
    std::string text;
    if (!f || !read_bytes(f.get(), text, limit + 1))
        return std::unexpected(files::open_error(std::string(where), file,
                                                  xpp::format("it cannot be read (a link, or not a plain file), or is longer than {} bytes", limit)));
    return std::optional<std::string>(std::move(text));
}

} // namespace

Result<std::optional<std::string>> read_config_file(std::string_view where, std::string_view name, std::size_t limit)
{
    const std::string file = files::config_path(name);
    if (file.empty()) return std::unexpected(no_config_folder(where, name));
    return read_plain_within(where, files::config_dir(), file, limit);
}

Result<std::optional<std::string>> read_model_state_file(std::string_view where, std::string_view model_folder, std::string_view name, std::size_t limit)
{
    return read_plain_within(where, model_folder, files::join(files::join(model_folder, files::MODEL_STATE_FOLDER), name), limit);
}

namespace {

/* the temp-then-rename write of `bytes` to `file` */
Result<> write_whole(std::string_view where, const std::string &file, std::string_view bytes)
{
    Writer w = Writer::binary(file);
    if (!w) return fail(std::string(where), "cannot be saved: the file cannot be opened for writing", Place{file});
    if (!w.write(bytes)) {
        w.abort();
        return fail(std::string(where), "cannot be saved: the write failed", Place{file});
    }
    return w.commit();
}

} // namespace

Result<> write_config_file(std::string_view where, std::string_view name, std::string_view bytes)
{
    const std::string file = files::config_path(name);
    if (file.empty()) return std::unexpected(no_config_folder(where, name));
    if (!files::make_dirs(files::config_dir(), files::config_base()))
        return fail(std::string(where), "cannot be saved: its folder cannot be made", Place{files::config_dir()});
    return write_whole(where, file, bytes);
}

Result<> write_model_state_file(std::string_view where, std::string_view model_folder, std::string_view name, std::string_view bytes)
{
    const Result<std::string> dir = files::model_state_dir(model_folder);
    if (!dir) return std::unexpected(dir.error());
    return write_whole(where, files::join(*dir, name), bytes);
}

} // namespace xpp
