/* core/xpp_io.h's implementation: the file handles (LineReader,
   TokenReader, Writer) and xpp::format/xpp::vformat. Nothing here throws:
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

xpp::Result<> xpp::seek_stream(FILE *file, long position, std::string_view path)
{
    if (std::fseek(file, position, SEEK_SET) != 0)
        return fail_reading("I/O", "cannot seek in stream", std::string(path));
    return {};
}

xpp::Result<long> xpp::tell_stream(FILE *file, std::string_view path)
{
    const long position = std::ftell(file);
    if (position == -1L)
        return fail_reading("I/O", "cannot tell stream position", std::string(path));
    return position;
}

/* A reader reads the stream of: owned when it opened the file itself
   (closed with the reader), attached when it was handed one; with
   neither, the lines are text's, from pos on (LineReader::of_text) */
struct xpp::LineReader::State {
    xpp::UniqueFile owned;
    std::FILE *attached = nullptr;
    std::string line;
    std::string text;
    size_t pos = 0;
    std::FILE *fp() const noexcept { return owned ? owned.get() : attached; }
};

struct xpp::TokenReader::State {
    std::FILE *attached = nullptr;
    std::FILE *fp() const noexcept { return attached; }
    std::string_view text;
    std::size_t pos = 0;
    bool text_mode = false;
    bool has_input() const noexcept { return fp() || text_mode; }
    int get() noexcept
    {
        return text_mode ? (pos < text.size() ? static_cast<unsigned char>(text[pos++]) : EOF) : std::fgetc(fp());
    }
    /* c, the last character read, back to the stream (one at a time) */
    void unget(int c) noexcept
    {
        if (c == EOF) return;
        if (text_mode) --pos;
        else std::ungetc(c, fp());
    }
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

/* the next line of r's text, as read_line reads a file's */
bool text_line(xpp::LineReader::State &r)
{
    if (r.pos >= r.text.size()) return false;
    size_t nl = r.text.find('\n', r.pos);
    if (nl == std::string::npos) nl = r.text.size();
    r.line.assign(r.text, r.pos, nl - r.pos);
    r.pos = nl + 1;
    if (!r.line.empty() && r.line.back() == '\r') r.line.pop_back();
    return true;
}

/* ---- tokens --------------------------------------------------------- */

/* Skips leading whitespace, then collects the run of non-whitespace
   characters into `tok`. False at end of file before any token starts.
   The whitespace that ends the token goes back to the stream, as fscanf
   leaves it, so a line read after a token on an attached FILE* sees the
   same rest of the line. */
bool read_token(xpp::TokenReader::State &in, std::string &tok)
{
    tok.clear();
    int c;
    do {
        c = in.get();
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    if (c == EOF) return false;
    while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
        tok.push_back(static_cast<char>(c));
        c = in.get();
    }
    in.unget(c);
    return true;
}

/* the next token of fp converted by strto (strtod, strtof, or strtol in
   base 10): false at the end of the file or when it does not start with
   a number */
template <class T, class Convert>
bool read_number(xpp::TokenReader::State *in, T &out, Convert strto)
{
    std::string tok;
    try {
        if (!in || !in->has_input() || !read_token(*in, tok)) return false;
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading a number");
    }
    char *end = nullptr;
    const auto v = strto(tok.c_str(), &end);
    if (end == tok.c_str()) return false;
    out = static_cast<T>(v);
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
Ptr attached_state(std::FILE *fp) noexcept
{
    if (!fp) return nullptr;
    Ptr st = new_state<Ptr>();
    if (st) st->attached = fp;
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

LineReader LineReader::of_text(std::string text) noexcept
{
    LineReader l;
    l.state_ = new_state<decltype(l.state_)>();
    if (l.state_) l.state_->text = std::move(text);
    return l;
}

LineReader LineReader::attach(FILE *fp) noexcept
{
    LineReader l;
    l.state_ = attached_state<decltype(l.state_)>(fp);
    return l;
}

std::optional<std::string_view> LineReader::next()
{
    if (!state_) return std::nullopt;
    State &r = *state_;
    if (!(r.fp() ? read_line(r.fp(), r.line) : text_line(r))) return std::nullopt;
    return std::string_view(r.line);
}

std::string LineReader::line(int n)
{
    for (int i = 1; std::optional<std::string_view> l = next(); i++)
        if (i == n) return std::string(*l);
    return {};
}

/* ---- TokenReader ----------------------------------------------------- */

void TokenReader::Free::operator()(State *s) const noexcept { delete s; }

TokenReader TokenReader::attach(FILE *fp) noexcept
{
    TokenReader t;
    t.state_ = attached_state<decltype(t.state_)>(fp);
    return t;
}

TokenReader TokenReader::of_text(std::string_view text) noexcept
{
    TokenReader t;
    t.state_ = new_state<decltype(t.state_)>();
    if (t.state_) { t.state_->text = text; t.state_->text_mode = true; }
    return t;
}

bool TokenReader::at_end() noexcept
{
    if (!state_) return true;
    int c;
    do { c = state_->get(); } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    state_->unget(c);
    return c == EOF;
}

bool TokenReader::read(double &x) noexcept
{
    return read_number(state_.get(), x, [](const char *s, char **e) { return std::strtod(s, e); });
}

bool TokenReader::read(int &x) noexcept
{
    return read_number(state_.get(), x, [](const char *s, char **e) { return std::strtol(s, e, 10); });
}

/* fscanf "%ld"'s own grammar, one character of lookahead pushed back as
   fscanf pushes it back, so a field that follows with no space between
   (autoX's "%5ld" columns) is the next read's. */
bool TokenReader::read(long long &x) noexcept
{
    if (!state_ || !state_->has_input()) return false;
    State &in = *state_;
    std::string num;
    int c;
    do {
        c = in.get();
    } while (c != EOF && std::isspace(static_cast<unsigned char>(c)));
    try {
        if (c == '+' || c == '-') {
            num.push_back(static_cast<char>(c));
            c = in.get();
        }
        while (c != EOF && std::isdigit(static_cast<unsigned char>(c))) {
            num.push_back(static_cast<char>(c));
            c = in.get();
        }
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading a number");
    }
    in.unget(c);
    if (num.empty() || !std::isdigit(static_cast<unsigned char>(num.back()))) return false;
    errno = 0;
    const long long value = std::strtoll(num.c_str(), nullptr, 10);
    if(errno == ERANGE)return false;
    x = value;
    return true;
}

bool TokenReader::read(long &x) noexcept
{
    long long value;
    if(!read(value) || !std::in_range<long>(value))return false;
    x=static_cast<long>(value);
    return true;
}

bool TokenReader::skip_line() noexcept
{
    if (!state_ || !state_->has_input()) return false;
    int c;
    if (state_->text_mode) {
        while ((c = state_->get()) != EOF)
            if (c == '\n') return true;
        return false;
    }
    /* a file is locked once and read without the lock per character (as read_line does): autoX's label search skips
       every line of a session's solutions this way, megabytes after a few runs (W701) */
    const StreamLock held(state_->fp());
    while ((c = get_char_locked(state_->fp())) != EOF)
        if (c == '\n') return true;
    return false;
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

/* ---- Lines: a file of ours, read (W125) ---- */

/* s split at each sep (empty fields kept) */
std::vector<std::string_view> split_fields(std::string_view s, char sep)
{
    std::vector<std::string_view> f;
    while (true) {
        const std::size_t i = s.find(sep);
        f.push_back(s.substr(0, i));
        if (i == std::string_view::npos) return f;
        s = s.substr(i + 1);
    }
}

std::vector<std::string_view> split_lines(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const std::size_t nl = text.find('\n');
        std::string_view l = text.substr(0, nl);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        if (nl == std::string_view::npos) break;
        text.remove_prefix(nl + 1);
    }
    return lines;
}

Place line_place(std::string file, std::string_view text, int n)
{
    const std::vector<std::string_view> lines = split_lines(text);
    const std::string source = n >= 1 && static_cast<std::size_t>(n) <= lines.size() ? std::string(lines[static_cast<std::size_t>(n) - 1]) : std::string();
    return Place{std::move(file), n, 0, source};
}

Lines::Lines(std::string where, std::string file, std::string_view text)
    : where_(std::move(where)), file_(std::move(file)), text_(text), lines_(split_lines(text_))
{
}

std::string_view Lines::text(int n) const noexcept
{
    return n >= 1 && static_cast<std::size_t>(n) <= lines_.size() ? lines_[static_cast<std::size_t>(n) - 1]
                                                                    : std::string_view();
}

Error Lines::error(int n, std::string what) const
{
    return Error{where_, std::move(what), Place{file_, n, 0, std::string(text(n))}};
}

void Lines::fail(int n, std::string what) const { throw ReadFailed{error(n, std::move(what))}; }

std::string_view Lines::next(std::string_view what)
{
    if (at_end()) {
        std::string cause = "the file ends here";
        what = trim_blanks(what);
        if (!what.empty()) cause += xpp::format(", before {}", what);
        fail(line() + 1, std::move(cause));
    }
    return lines_[next_++];
}

namespace {

/* the next line's first word through parse (parse_int or parse_number),
   kind naming what it must be */
template <class T>
T first_word(Lines &lines, std::string_view what, const char *kind, bool (*parse)(std::string_view, T &))
{
    const std::string_view line = lines.next(what);
    const std::string_view text = trim_blanks(line);
    T value{};
    if (!parse(text.substr(0, text.find_first_of(" \t")), value)) {
        const std::string_view name = trim_blanks(what);
        lines.fail(xpp::format("\"{}\" is not {}{}", line, kind, name.empty() ? std::string() : xpp::format(" ({})", name)));
    }
    return value;
}

} // namespace

int Lines::whole(std::string_view what, bool named)
{
    const int value = first_word(*this, what, "a whole number", parse_int);
    if (named) check_name(what);
    return value;
}

double Lines::real(std::string_view what, bool named)
{
    const double value = first_word(*this, what, "a number", parse_number);
    if (named) check_name(what);
    return value;
}

std::string_view Lines::value_name() const
{
    const std::string_view value = trim_blanks(text(line()));
    const std::size_t end = value.find_first_of(" \t");
    return end == std::string_view::npos ? std::string_view() : trim_blanks(value.substr(end));
}

void Lines::check_name(std::string_view what) const
{
    const std::string_view name = value_name();
    if (name != trim_blanks(what))
        fail(xpp::format("name \"{}\" differs from \"{}\"", name, trim_blanks(what)));
}

void Lines::heading(std::string_view heading)
{
    const std::string_view line = next(heading);
    if (!line.starts_with("#")) fail(xpp::format("\"{}\" is not the heading \"{}\"", line, trim_blanks(heading)));
}

void Lines::end()
{
    while (!at_end())
        if (!trim_blanks(next()).empty()) fail(xpp::format("\"{}\" after the end of what the file holds", text(line())));
}


Error no_config_folder(std::string_view where, std::string_view name)
{
    return Error{std::string(where),
                 xpp::format("there is no per-user config folder (no HOME or APPDATA, or {} is not an absolute path): set {} to one",
                             files::CONFIG_DIR_ENV, files::CONFIG_DIR_ENV),
                 Place{std::string(name)}};
}

Result<std::optional<std::string>> read_config_file(std::string_view where, std::string_view name, std::size_t limit)
{
    const std::string file = files::config_path(name);
    if (file.empty()) return std::unexpected(no_config_folder(where, name));
    if (!files::exists(file)) return std::optional<std::string>();
    UniqueFile f(files::open_read_within(file, files::config_dir(), {}));
    std::string text;
    if (!f || !read_bytes(f.get(), text, limit + 1))
        return std::unexpected(files::open_error(std::string(where), file,
                                                  xpp::format("it cannot be read, or is longer than {} bytes", limit)));
    return std::optional<std::string>(std::move(text));
}

Result<> write_config_file(std::string_view where, std::string_view name, std::string_view bytes)
{
    const std::string file = files::config_path(name);
    if (file.empty()) return std::unexpected(no_config_folder(where, name));
    if (!files::make_dirs(files::config_dir(), files::config_base()))
        return fail(std::string(where), "cannot be saved: its folder cannot be made", Place{files::config_dir()});
    Writer w = Writer::binary(file);
    if (!w) return fail(std::string(where), "cannot be saved: the file cannot be opened for writing", Place{file});
    if (!w.write(bytes)) {
        w.abort();
        return fail(std::string(where), "cannot be saved: the write failed", Place{file});
    }
    return w.commit();
}

} // namespace xpp
