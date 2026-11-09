/* preferences.odex: see xpp_prefs.h. */
#include "xpp_prefs.h"

#include <algorithm>
#include <optional>
#include <span>
#include <utility>

#include "model_options.h" /* choice_index */
#include "odex.h"
#include "odex_write.h"
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

/* why `path` cannot be a path the file keeps, or nothing */
std::optional<std::string> path_problem(std::string_view path)
{
    if (path.empty()) return "the path is empty";
    if (path.size() > MAX_PATH_BYTES) return xpp::format("the path is longer than {} bytes", MAX_PATH_BYTES);
    if (has_control(path)) return "the path has a control character";
    if (!files::is_absolute(path)) return xpp::format("\"{}\" is not an absolute path", path);
    return std::nullopt;
}

/* why `path` cannot be the next recent model after `before`, or nothing */
std::optional<std::string> recent_problem(std::span<const std::string> before, std::string_view path)
{
    if (const auto why = path_problem(path)) return why;
    if (std::ranges::find(before, path) != before.end()) return xpp::format("\"{}\" is listed twice", path);
    return std::nullopt;
}

using CountProblem = std::optional<std::string> (*)(long long);

/* why `a` cannot be the next approval after `before`, or nothing; which: "model" or "path" */
std::optional<std::string> approval_problem(std::span<const OutsideApproval> before, const OutsideApproval &a,
                                            std::string_view &which)
{
    which = "model";
    if (const auto why = path_problem(a.model)) return why;
    which = "path";
    if (const auto why = path_problem(a.path)) return why;
    if (std::ranges::find(before, a) != before.end()) return xpp::format("\"{}\" for \"{}\" is listed twice", a.path, a.model);
    return std::nullopt;
}

using Problem = std::optional<std::string> (*)(std::span<const std::string>, std::string_view);

/* the reader of a description: each key it is told of is read into its
   member when the file names it, and left as it is when not */
class Take {
public:
    explicit Take(odex::Block &block) : block_(block) {}
    void paths(std::string_view key, std::vector<std::string> &out, std::size_t max, Problem problem)
    {
        std::vector<std::string> got;
        if (!block_.texts(key, got, max)) return;
        const odex::Member *m = block_.member(key);
        for (std::size_t i = 0; i < got.size(); i++)
            if (const auto why = problem(std::span(got).first(i), got[i])) block_.fail(m->value.items[i], *why);
        out = std::move(got);
    }
    void approvals(std::string_view key, std::vector<OutsideApproval> &out, std::size_t max)
    {
        std::vector<odex::Block> groups;
        if (!block_.groups(key, groups, max)) return;
        std::vector<OutsideApproval> got;
        for (odex::Block &g : groups) {
            OutsideApproval a;
            if (!g.text("model", a.model) || !g.text("path", a.path)) g.fail("an entry needs a `model` and a `path`");
            std::string_view which;
            if (const auto why = approval_problem(got, a, which)) g.fail(*g.member(which), *why);
            g.done();
            got.push_back(std::move(a));
        }
        out = std::move(got);
    }

    /* one of the log level's words */
    void level(std::string_view key, XppLogLevel &out)
    {
        std::string text;
        if (!block_.word(key, text)) return;
        const int i = choice_index(log_level_names, text);
        if (i < 0) block_.fail(block_.member(key)->value, xpp::format("`{}`: `{}` is none of error, warn, info or debug", key, text));
        out = static_cast<XppLogLevel>(i);
    }
    /* a colour, when the file names one */
    void color(std::string_view key, Color &out) { block_.color(key, out); }
    /* an absolute path, when the file names one */
    void path(std::string_view key, std::string &out)
    {
        std::string got;
        if (!block_.text(key, got)) return;
        if (const auto why = path_problem(got)) block_.fail(block_.member(key)->value, *why);
        out = std::move(got);
    }

    /* `automatic` (0 in out) or a whole number problem accepts */
    void count(std::string_view key, unsigned &out, std::string_view automatic, CountProblem problem)
    {
        std::string text;
        if (!block_.word(key, text)) return;
        if (text == automatic) {
            out = 0;
            return;
        }
        const odex::Member *m = block_.member(key);
        int n = 0;
        if (!xpp::parse_int(text, n)) block_.fail(m->value, xpp::format("`{}`: `{}` is not \"{}\" or a whole number", key, text, automatic));
        if (const auto why = problem(n)) block_.fail(m->value, *why);
        out = static_cast<unsigned>(n);
    }

private:
    odex::Block &block_;
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
    void level(std::string_view key, const XppLogLevel &value) { out.word(key, log_level_names[static_cast<std::size_t>(value)]); }
    void color(std::string_view key, const Color &value) { out.color(key, value); }
    void path(std::string_view key, const std::string &value)
    {
        if (value.empty()) return;
        if (const auto why = path_problem(value); why && !bad_) bad_ = *why;
        out.text(key, value);
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
    void approvals(std::string_view key, const std::vector<OutsideApproval> &list, std::size_t max)
    {
        if (list.size() > max && !bad_) bad_ = xpp::format("`{}` has {} entries: at most {}", key, list.size(), max);
        out.begin_list(key);
        for (std::size_t i = 0; i < list.size(); i++) {
            std::string_view which;
            if (const auto why = approval_problem(std::span(list).first(i), list[i], which); why && !bad_) bad_ = *why;
            out.begin_item();
            out.text("model", list[i].model);
            out.text("path", list[i].path);
            out.end_item();
        }
        out.end_list();
    }
    odex::Out out;
    std::optional<std::string> bad_;
};

/* every group of the file, once, for the reader and the writer: a group is
   a member of Preferences and a line here */
template <class IO, class P> void describe(IO &io, P &p)
{
    io.paths("recent_models", p.recent_models, MAX_RECENT_MODELS, recent_problem);
    io.count("threads", p.threads, THREADS_AUTO_WORD, threads_problem);
    io.level("log_level", p.log_level);
    io.path("log_file", p.log_file);
    for (const ColorKey &c : COLOR_KEYS) io.color(c.key, p.colors.*c.member);
    io.approvals("outside_paths", p.outside_paths, MAX_OUTSIDE_APPROVALS);
}

} // namespace

std::optional<std::string> threads_problem(long long n)
{
    const unsigned most = cpu::logical_processors();
    if (n >= 1 && n <= static_cast<long long>(most)) return std::nullopt;
    return xpp::format("the thread count must be \"{}\" or a whole number from 1 to {} (this computer's logical processors)", THREADS_AUTO_WORD, most);
}

Result<Preferences> parse_prefs(std::string_view text, std::string file)
{
    if (text.size() > MAX_FILE_BYTES)
        return fail_reading(std::string(WHERE), xpp::format("the file is longer than {} bytes", MAX_FILE_BYTES), std::move(file));
    Result<odex::StateFile> parsed = odex::read_state_file(odex::FileKind::preferences, std::string(WHERE), std::move(file), std::string(text));
    if (!parsed) return std::unexpected(parsed.error());
    try {
        odex::Block block = parsed->block(false, "the preferences");
        Preferences p;
        Take take(block);
        describe(take, p);
        block.done();
        return p;
    } catch (ReadFailed &e) {
        return std::unexpected(std::move(e.error));
    }
}

Result<std::string> serialize_prefs(const Preferences &p)
{
    Put put;
    put.out.begin_statement("preferences");
    describe(put, p);
    put.out.end_statement();
    if (put.bad_) return fail(std::string(WHERE), "cannot be saved: " + *put.bad_, Place{prefs_path()});
    Result<std::string> body = put.out.result(std::string(WHERE));
    if (!body) return std::unexpected(body.error());
    return odex::version_statement() + "\n" + *body;
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

Result<bool> outside_path_approved(std::string_view model, std::string_view path)
{
    const Result<Preferences> p = load_prefs();
    if (!p) return std::unexpected(p.error());
    return std::ranges::find(p->outside_paths, OutsideApproval{std::string(model), std::string(path)}) != p->outside_paths.end();
}

Result<> approve_outside_path(std::string_view model, std::string_view path)
{
    return update_prefs([&](Preferences &p) { remember_newest(p.outside_paths, OutsideApproval{std::string(model), std::string(path)}, MAX_OUTSIDE_APPROVALS); });
}

Result<> update_prefs(const std::function<void(Preferences &)> &change)
{
    Result<Preferences> p = load_prefs();
    if (!p) return std::unexpected(p.error());
    change(*p);
    return save_prefs(*p);
}

} // namespace xpp::prefs
