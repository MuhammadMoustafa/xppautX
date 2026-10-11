/* preferences.setx: see xpp_prefs.h. */
#include "xpp_prefs.h"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>

#include "setx.h"
#include "xpp_files.h"
#include "xpp_cpu.h"
#include "xpp_io.h"

namespace xpp::prefs {

namespace {

constexpr std::string_view WHERE = "preferences"; /* the Error's `where` */

bool has_control(std::string_view text)
{
    return std::ranges::any_of(text, [](char c) { return static_cast<unsigned char>(c) < ' ' || c == 0x7f; });
}

/* why `path` cannot be the next recent model after `before`, or nothing */
std::optional<std::string> recent_problem(std::span<const std::string> before, std::string_view path)
{
    if (path.empty()) return "the path is empty";
    if (path.size() > MAX_PATH_BYTES) return xpp::format("the path is longer than {} bytes", MAX_PATH_BYTES);
    if (has_control(path)) return "the path has a control character";
    if (!files::is_absolute(path)) return xpp::format("\"{}\" is not an absolute path", path);
    if (std::ranges::find(before, path) != before.end()) return xpp::format("\"{}\" is listed twice", path);
    return std::nullopt;
}

using CountProblem = std::optional<std::string> (*)(long long);

using Problem = std::optional<std::string> (*)(std::span<const std::string>, std::string_view);

/* the reader of a description: each key it is told of is read into its
   member when the file names it, and left as it is when not */
class Take {
public:
    explicit Take(setx::Block &block) : block_(block) {}
    void paths(std::string_view key, std::vector<std::string> &out, std::size_t max, Problem problem)
    {
        std::vector<std::string> got;
        if (!block_.texts(key, got, max)) return;
        const setx::Member *m = block_.member(key);
        for (std::size_t i = 0; i < got.size(); i++)
            if (const auto why = problem(std::span(got).first(i), got[i])) block_.fail(m->value.items[i], *why);
        out = std::move(got);
    }

    /* `automatic` (0 in out) or a whole number problem accepts */
    void count(std::string_view key, unsigned &out, std::string_view automatic, CountProblem problem)
    {
        std::string text;
        if (!block_.text(key, text)) return;
        if (text == automatic) {
            out = 0;
            return;
        }
        const setx::Member *m = block_.member(key);
        int n = 0;
        if (!xpp::parse_int(text, n)) block_.fail(m->value, xpp::format("`{}`: `{}` is not \"{}\" or a whole number", key, text, automatic));
        if (const auto why = problem(n)) block_.fail(m->value, *why);
        out = static_cast<unsigned>(n);
    }

private:
    setx::Block &block_;
};

/* the writer of the same description */
class Put {
public:
    void paths(std::string_view key, const std::vector<std::string> &list, std::size_t max, Problem problem)
    {
        if (list.size() > max && !bad_) bad_ = xpp::format("`{}` has {} entries: at most {}", key, list.size(), max);
        for (std::size_t i = 0; i < list.size(); i++)
            if (const auto why = problem(std::span(list).first(i), list[i]); why && !bad_) bad_ = *why;
        out.texts(key, list);
    }
    void count(std::string_view key, const unsigned &value, std::string_view automatic, CountProblem problem)
    {
        if (value == 0) {
            out.word(key, automatic);
            return;
        }
        if (const auto why = problem(value); why && !bad_) bad_ = *why;
        out.whole(key, value);
    }
    setx::Out out;
    std::optional<std::string> bad_;
};

/* every group of the file, once, for the reader and the writer: a group is
   a member of Preferences and a line here */
template <class IO, class P> void describe(IO &io, P &p)
{
    io.paths("recent_models", p.recent_models, MAX_RECENT_MODELS, recent_problem);
    io.count("auto_threads", p.auto_threads, AUTO_THREADS_WORD, auto_threads_problem);
}

} // namespace

std::optional<std::string> auto_threads_problem(long long n)
{
    const unsigned most = cpu::logical_processors();
    if (n >= 1 && n <= static_cast<long long>(most)) return std::nullopt;
    return xpp::format("AUTO's threads must be \"{}\" or a whole number from 1 to {} (this computer's logical processors)", AUTO_THREADS_WORD, most);
}

Result<Preferences> parse_prefs(std::string_view text, std::string file)
{
    if (text.size() > MAX_FILE_BYTES)
        return fail_reading(std::string(WHERE), xpp::format("the file is longer than {} bytes", MAX_FILE_BYTES), std::move(file));
    return setx::read_block(std::string(WHERE), std::move(file), text, false, [](setx::Block &block) {
        Preferences p;
        Take take(block);
        describe(take, p);
        return p;
    });
}

Result<std::string> serialize_prefs(const Preferences &p)
{
    Put put;
    describe(put, p);
    if (put.bad_) return fail(std::string(WHERE), "cannot be saved: " + *put.bad_, Place{prefs_path()});
    return put.out.result(std::string(WHERE));
}

std::string prefs_path() { return files::config_path(FILE_NAME); }

Result<Preferences> load_prefs()
{
    return load_config_file<Preferences>(WHERE, FILE_NAME, MAX_FILE_BYTES, parse_prefs);
}

Result<> save_prefs(const Preferences &p)
{
    const Result<std::string> text = serialize_prefs(p);
    if (!text) return std::unexpected(text.error());
    return write_config_file(WHERE, FILE_NAME, *text);
}

Result<> update_prefs(const std::function<void(Preferences &)> &change)
{
    Result<Preferences> p = load_prefs();
    if (!p) return std::unexpected(p.error());
    change(*p);
    return save_prefs(*p);
}

} // namespace xpp::prefs
