/* xpp_install.h */
#include "xpp_install.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <new>
#include <vector>

#include "xpp_about.h"
#include "xpp_files.h"
#include "xpp_io.h"
#include "xpp_log.h"
#include "xpp_mem.h"
#include "xpp_process.h"
#include "xpp_sha256.h"
#include "xpp_zip.h"

namespace xpp::update {

namespace {

constexpr std::string_view WHERE = "update install";
constexpr std::string_view RECORD_NAME = "pending";
constexpr std::string_view KEPT_FOLDER_NAME = "update";
/* the record's four lines, in this order: `key value` */
constexpr std::array<std::string_view, 4> RECORD_KEYS = {"version", "name", "size", "sha256"};
constexpr std::size_t RECORD_BYTES = 1024; /* a record is four short lines */
/* what a program file starts with: ELF (Linux), MZ (Windows) */
constexpr std::string_view ELF_MAGIC = "\x7f" "ELF", EXE_MAGIC = "MZ";
constexpr std::string_view LINUX_INSTALLER_FOLDER = "/usr/bin"; /* where the .deb puts the program (tools/package_release.sh) */
constexpr std::string_view LINUX_OPENER = "xdg-open";            /* the system's own handler of a .deb */
constexpr std::string_view WINDOWS_INSTALLER_FOLDER_ENV = "ProgramFiles", WINDOWS_INSTALLER_FOLDER_NAME = "xppautX", SYSTEM_ROOT_ENV = "SystemRoot";
constexpr std::string_view MSIEXEC = "\\System32\\msiexec.exe";

constexpr Os RUNNING_OS =
#if defined(_WIN32)
    Os::windows_x64;
#elif defined(__APPLE__)
    Os::macos;
#elif defined(__linux__) && defined(__x86_64__)
    Os::linux_x64;
#else
    Os::other;
#endif
constexpr bool ON_WINDOWS = RUNNING_OS == Os::windows_x64;

bool same_folder(std::string_view a, std::string_view b, bool ignore_case)
{
    return a.size() == b.size() && std::ranges::equal(a, b, [&](char x, char y) {
               const auto fold = [&](char c) { return ignore_case && c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
               return fold(x) == fold(y);
           });
}

/* the end of a git describe: -<commits>-g<hex> */
bool describe_suffix(std::string_view version)
{
    const std::size_t g = version.rfind("-g");
    if (g == std::string_view::npos || g + 2 >= version.size()) return false;
    const std::string_view hash = version.substr(g + 2);
    const std::size_t dash = version.rfind('-', g - 1);
    if (g == 0 || dash == std::string_view::npos) return false;
    const std::string_view commits = version.substr(dash + 1, g - dash - 1);
    const auto digits = [](char c) { return c >= '0' && c <= '9'; };
    return !commits.empty() && std::ranges::all_of(commits, digits) && std::ranges::all_of(hash, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

std::string kept_path(std::string_view name) { return files::join(kept_folder(), name); }

std::string record_text(const ReleaseFile &file, std::string_view version)
{
    return xpp::format("{} {}\n{} {}\n{} {}\n{} {}\n", RECORD_KEYS[0], version, RECORD_KEYS[1], file.name, RECORD_KEYS[2], file.size, RECORD_KEYS[3],
                       digest_value(file.digest));
}

/* the file's bytes into `bytes`, and why they are not what the release list named (exactly its size and sha256), or nothing */
std::optional<std::string> read_verified(const std::string &path, unsigned long long size, std::string_view sha256, std::string &bytes)
{
    try {
        if (!read_bytes(path, bytes, static_cast<std::size_t>(size)) || bytes.size() != size) return xpp::format("it cannot be read or is not {} bytes", size);
    } catch (const std::bad_alloc &) {
        xpp::out_of_memory("reading the downloaded file");
    }
    Sha256 hash;
    hash.update(bytes.data(), bytes.size());
    if (const std::string have = hash.hex(); have != sha256) return xpp::format("its sha256 is {}, not {}", have, sha256);
    return std::nullopt;
}

/* the program in an archive: <one folder>/xppautX[.exe] (.exe for Windows' release), the one member of that name */
const zip::Entry *program_entry(const std::vector<zip::Entry> &entries, bool windows)
{
    const std::string wanted = std::string(PROGRAM_NAME) + (windows ? ".exe" : "");
    const zip::Entry *found = nullptr;
    for (const zip::Entry &e : entries) {
        const std::size_t slash = e.name.find('/');
        if (slash == std::string::npos || e.name.find('/', slash + 1) != std::string::npos || e.name.substr(slash + 1) != wanted) continue;
        if (found) return nullptr; /* two of them: which is the program is not for us to guess */
        found = &e;
    }
    return found;
}

}

/* the checked archive's program swapped in for `running` */
Result<> swap_program(const Plan &plan, const ReleaseFile &file, std::string_view staged, const std::string &running)
{
    const Place place{std::string(staged)};
    /* the archive is what the release list named, again: the temp folder is checked at the moment of use */
    std::string bytes;
    if (const auto why = read_verified(std::string(staged), file.size, digest_value(file.digest), bytes))
        return fail(std::string(WHERE), "the downloaded file changed after it was checked: " + *why, place);
    std::vector<zip::Entry> entries;
    if (plan.asset_suffix.ends_with(".zip")) {
        auto read = zip::read_zip(bytes);
        if (!read) return fail(std::string(WHERE), "the archive is not a zip file, or it is damaged or over the limits", place);
        entries = std::move(*read);
        for (const zip::Entry &e : entries)
            if (!zip::entry_name_ok(e.name)) return fail(std::string(WHERE), xpp::format("the archive's member name \"{}\" is not a plain relative path", e.name), place);
    } else {
        auto read = zip::read_tar_gz(bytes, staged);
        if (!read) return std::unexpected(read.error());
        entries = std::move(*read);
    }
    /* what the archive holds is the plan's system's program; how it replaces the running one is this system's */
    const bool windows_release = plan.os == Os::windows_x64;
    const zip::Entry *program = program_entry(entries, windows_release);
    if (!program) return fail(std::string(WHERE), xpp::format("the archive holds no single <folder>/{}", PROGRAM_NAME), place);
    if (!program->bytes.starts_with(windows_release ? EXE_MAGIC : ELF_MAGIC))
        return fail(std::string(WHERE), xpp::format("\"{}\" is not a program of this system", program->name), place);
    if (running.empty()) return fail(std::string(WHERE), "the running program's path is unknown", place);
    const std::string fresh = running + std::string(NEW_SUFFIX), old = running + std::string(REPLACED_SUFFIX);
    Writer out = Writer::binary(fresh);
    if (!out || !out.write(program->bytes)) return fail(std::string(WHERE), "the new program cannot be written beside the running one", Place{fresh});
    if (const Result<> done = out.commit(); !done) return done;
    const auto undo = [&](std::string what) {
        files::remove(fresh);
        return fail(std::string(WHERE), std::move(what), Place{running});
    };
    if (!files::make_executable(fresh)) return undo("the new program cannot be made executable");
    /* Windows cannot replace a running program but can rename it: the old one goes aside and is removed at the next start */
    if (ON_WINDOWS && files::replace_file(running, old) != 0) return undo("the running program cannot be renamed aside (a copy from an earlier update may still be in use: restart first)");
    if (files::replace_file(fresh, running) != 0) {
        if (ON_WINDOWS) files::replace_file(old, running);
        return undo("the new program cannot be put in place of the running one");
    }
    return {};
}

namespace {

/* the record's file: all or nothing (every line, every value), and what it names exists as it says */
Result<std::optional<Pending>> read_pending(const Plan &plan)
{
    const std::string record = kept_path(RECORD_NAME);
    if (kept_folder().empty() || !files::exists(record)) return std::optional<Pending>();
    std::string text;
    if (!read_bytes(record, text, RECORD_BYTES) || text.size() >= RECORD_BYTES) return fail(std::string(WHERE), "the record of the kept update cannot be read", Place{record});
    std::array<std::string, RECORD_KEYS.size()> values;
    std::size_t at = 0;
    for (std::size_t line = 0; line < RECORD_KEYS.size(); line++) {
        const std::size_t end = text.find('\n', at);
        const std::string_view row = std::string_view(text).substr(at, end == std::string::npos ? std::string::npos : end - at);
        const Place place{record, static_cast<int>(line + 1), 0, std::string(row)};
        const std::string key = std::string(RECORD_KEYS[line]) + " ";
        if (end == std::string::npos || !row.starts_with(key) || row.size() == key.size())
            return fail(std::string(WHERE), xpp::format("line {} is not \"{} <value>\"", line + 1, RECORD_KEYS[line]), place);
        values[line] = std::string(row.substr(key.size()));
        at = end + 1;
    }
    if (at != text.size()) return fail(std::string(WHERE), "the record has more than its four lines", Place{record, static_cast<int>(RECORD_KEYS.size() + 1)});
    unsigned long long size = 0;
    const std::string &digits = values[2];
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), size);
    if (parsed.ec != std::errc() || parsed.ptr != digits.data() + digits.size())
        return fail(std::string(WHERE), xpp::format("\"{}\" is not a size", digits), Place{record, 3, 0, "size " + digits});
    const std::string digest = std::string(DIGEST_PREFIX) + values[3];
    if (!version_ok(values[0])) return fail(std::string(WHERE), xpp::format("\"{}\" is not a version", values[0]), Place{record, 1, 0, "version " + values[0]});
    if (const auto why = content_problem(values[1], size, digest)) return fail(std::string(WHERE), *why, Place{record, 2});
    if (!fits(plan, values[1])) return fail(std::string(WHERE), xpp::format("\"{}\" is not a file for this installation", values[1]), Place{record, 2, 0, "name " + values[1]});
    const std::string path = kept_path(values[1]);
    std::string bytes;
    if (const auto why = read_verified(path, size, values[3], bytes)) return fail(std::string(WHERE), "the kept file is not the one recorded: " + *why, Place{path});
    return std::optional<Pending>(Pending{values[0], values[1], path});
}

} // namespace

std::string_view install_name(Install install)
{
    switch (install) {
    case Install::portable: return "portable";
    case Install::msi: return "msi";
    case Install::deb: return "deb";
    case Install::release_page: return "release_page";
    }
    return "release_page";
}

std::string_view ready_name(Ready ready) { return ready == Ready::restart ? "restart" : "install"; }

bool is_release_version(std::string_view version) { return version.size() > 1 && version.front() == 'v' && !describe_suffix(version); }

bool version_ok(std::string_view version)
{
    return !version.empty() && version.size() <= VERSION_BYTES &&
           std::ranges::all_of(version, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'; });
}

Plan plan_for(const Where &where)
{
    const auto page = [](std::string why) { return Plan{Install::release_page, std::move(why), {}}; };
    if (!is_release_version(where.version))
        return page(xpp::format("this program was built from a source checkout (version {}): an update is never installed over it", where.version));
    if (where.os == Os::macos) return page("macOS updates are installed from the release page");
    if (where.os == Os::other) return page("there is no release file for this system");
    const bool win = where.os == Os::windows_x64;
    const std::string platform = win ? "-windows-x64" : "-linux-x64";
    if (where.program_folder.empty()) return page("the folder xppautX runs from cannot be told");
    if (!where.installer_folder.empty() && same_folder(where.program_folder, where.installer_folder, win))
        return Plan{win ? Install::msi : Install::deb, {}, platform + (win ? ".msi" : ".deb"), where.os};
    if (!where.writable) return page(xpp::format("the folder {} cannot be written", where.program_folder));
    return Plan{Install::portable, {}, platform + (win ? ".zip" : ".tar.gz"), where.os};
}

Plan running_plan()
{
    Where where;
    where.version = xpp_version_string();
    where.os = RUNNING_OS;
    where.program_folder = files::split_path(files::program_file()).first;
    if (RUNNING_OS == Os::linux_x64) where.installer_folder = LINUX_INSTALLER_FOLDER;
    if (RUNNING_OS == Os::windows_x64)
        if (const char *programs = std::getenv(std::string(WINDOWS_INSTALLER_FOLDER_ENV).c_str()); programs && programs[0])
            where.installer_folder = files::join(programs, WINDOWS_INSTALLER_FOLDER_NAME);
    /* the installer's folder is the system's, never probed; a portable copy's must be writable */
    where.writable = !where.program_folder.empty() && where.program_folder != where.installer_folder && files::dir_writable(where.program_folder);
    return plan_for(where);
}

bool fits(const Plan &plan, std::string_view asset_name)
{
    return !plan.asset_suffix.empty() && asset_name.starts_with(PROGRAM_NAME) && asset_name.size() > PROGRAM_NAME.size() + plan.asset_suffix.size() &&
           asset_name.substr(PROGRAM_NAME.size()).starts_with('-') && asset_name.ends_with(plan.asset_suffix);
}

std::string kept_folder()
{
    const std::string config = files::config_dir();
    return config.empty() ? std::string() : files::join(config, KEPT_FOLDER_NAME);
}

void discard_pending()
{
    std::vector<files::DirEntry> entries;
    const std::string folder = kept_folder();
    if (folder.empty() || !files::list_dir(folder, entries)) return;
    for (const files::DirEntry &e : entries)
        if (!e.folder && files::name_ok(e.name, true)) files::remove(files::join(folder, e.name));
}

Result<Ready> prepare(const Plan &plan, const ReleaseFile &file, std::string_view version, std::string_view staged)
{
    const Place place{file.url};
    if (plan.install == Install::release_page) return fail(std::string(WHERE), "this installation is updated from the release page: " + plan.why, place);
    if (!fits(plan, file.name)) return fail(std::string(WHERE), xpp::format("\"{}\" is not the release file for this installation (it ends with {})", file.name, plan.asset_suffix), place);
    if (!version_ok(version)) return fail(std::string(WHERE), xpp::format("\"{}\" is not a version", version), place);
    if (plan.install == Install::portable) {
        if (const Result<> done = swap_program(plan, file, staged, files::program_file()); !done) return std::unexpected(done.error());
        return Ready::restart;
    }
    const std::string folder = kept_folder();
    if (folder.empty()) return fail(std::string(WHERE), "there is no folder of the preferences to keep the update in", place);
    if (!files::make_dirs(folder, files::config_base())) return fail(std::string(WHERE), xpp::format("cannot make the folder {}", folder), place);
    discard_pending();
    const std::string kept = kept_path(file.name);
    files::move(staged, kept);
    if (!files::exists(kept)) return fail(std::string(WHERE), "the checked file cannot be moved to the folder it is kept in", Place{kept});
    Writer record = Writer::binary(kept_path(RECORD_NAME));
    if (!record || !record.write(record_text(file, version))) return fail(std::string(WHERE), "the record of the kept update cannot be written", Place{kept_path(RECORD_NAME)});
    if (const Result<> done = record.commit(); !done) return std::unexpected(done.error());
    return Ready::install;
}

Result<std::optional<Pending>> pending(const Plan &plan)
{
    Result<std::optional<Pending>> found = read_pending(plan);
    if (!found) discard_pending();
    return found;
}

Result<> start_installer(const Plan &plan)
{
    if (plan.install != Install::msi && plan.install != Install::deb) return fail(std::string(WHERE), "this installation has no installer to start", Place{std::string(WHERE)});
    const Result<std::optional<Pending>> kept = pending(plan);
    if (!kept) return std::unexpected(kept.error());
    if (!*kept) return fail(std::string(WHERE), "no update is kept", Place{kept_folder()});
    std::vector<std::string> argv;
    if (plan.install == Install::msi) {
        const char *root = std::getenv(std::string(SYSTEM_ROOT_ENV).c_str()); /* the full path: no search for a program of that name */
        if (!root || !root[0]) return fail(std::string(WHERE), "the Windows folder is not known", Place{std::string(SYSTEM_ROOT_ENV)});
        argv = {std::string(root) + std::string(MSIEXEC), "/i", (*kept)->path};
    } else {
        argv = {std::string(LINUX_OPENER), (*kept)->path};
    }
    return process::spawn(argv, process::Wait::not_at_all);
}

Result<> start_new_program(std::string_view reopen, bool browser)
{
    std::vector<std::string> argv = {files::program_file()};
    if (browser) argv.push_back("--browser");
    if (!reopen.empty()) argv.emplace_back(reopen);
    return process::spawn(argv, process::Wait::not_at_all);
}

void remove_replaced_program()
{
    const std::string program = files::program_file();
    if (ON_WINDOWS && !program.empty()) files::remove(program + std::string(REPLACED_SUFFIX));
}

} // namespace xpp::update
